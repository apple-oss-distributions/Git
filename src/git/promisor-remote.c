#include "cache.h"
#include "object-store.h"
#include "promisor-remote.h"
#include "config.h"
#include "transport.h"
#include "strvec.h"

static char *repository_format_partial_clone;

void set_repository_format_partial_clone(char *partial_clone)
{
	repository_format_partial_clone = xstrdup_or_null(partial_clone);
}

static int fetch_objects(const char *remote_name,
			 const struct object_id *oids,
			 int oid_nr)
{
	struct child_process child = CHILD_PROCESS_INIT;
	int i;
	FILE *child_in;

	child.git_cmd = 1;
	child.in = -1;
	strvec_pushl(&child.args, "-c", "fetch.negotiationAlgorithm=noop",
		     "fetch", remote_name, "--no-tags",
		     "--no-write-fetch-head", "--recurse-submodules=no",
		     "--filter=blob:none", "--stdin", NULL);
	if (start_command(&child))
		die(_("promisor-remote: unable to fork off fetch subprocess"));
	child_in = xfdopen(child.in, "w");

	for (i = 0; i < oid_nr; i++) {
		if (fputs(oid_to_hex(&oids[i]), child_in) < 0)
			die_errno(_("promisor-remote: could not write to fetch subprocess"));
		if (fputc('\n', child_in) < 0)
			die_errno(_("promisor-remote: could not write to fetch subprocess"));
	}

	if (fclose(child_in) < 0)
		die_errno(_("promisor-remote: could not close stdin to fetch subprocess"));
	return finish_command(&child) ? -1 : 0;
}

static struct promisor_remote *promisors;
static struct promisor_remote **promisors_tail = &promisors;

static struct promisor_remote *promisor_remote_new(const char *remote_name)
{
	struct promisor_remote *r;

	if (*remote_name == '/') {
		warning(_("promisor remote name cannot begin with '/': %s"),
			remote_name);
		return NULL;
	}

	FLEX_ALLOC_STR(r, name, remote_name);

	*promisors_tail = r;
	promisors_tail = &r->next;

	return r;
}

static struct promisor_remote *promisor_remote_lookup(const char *remote_name,
						      struct promisor_remote **previous)
{
	struct promisor_remote *r, *p;

	for (p = NULL, r = promisors; r; p = r, r = r->next)
		if (!strcmp(r->name, remote_name)) {
			if (previous)
				*previous = p;
			return r;
		}

	return NULL;
}

static void promisor_remote_move_to_tail(struct promisor_remote *r,
					 struct promisor_remote *previous)
{
	if (r->next == NULL)
		return;

	if (previous)
		previous->next = r->next;
	else
		promisors = r->next ? r->next : r;
	r->next = NULL;
	*promisors_tail = r;
	promisors_tail = &r->next;
}

static int promisor_remote_config(const char *var, const char *value, void *data)
{
	const char *name;
	size_t namelen;
	const char *subkey;

	if (parse_config_key(var, "remote", &name, &namelen, &subkey) < 0)
		return 0;

	if (!strcmp(subkey, "promisor")) {
		char *remote_name;

		if (!git_config_bool(var, value))
			return 0;

		remote_name = xmemdupz(name, namelen);

		if (!promisor_remote_lookup(remote_name, NULL))
			promisor_remote_new(remote_name);

		free(remote_name);
		return 0;
	}
	if (!strcmp(subkey, "partialclonefilter")) {
		struct promisor_remote *r;
		char *remote_name = xmemdupz(name, namelen);

		r = promisor_remote_lookup(remote_name, NULL);
		if (!r)
			r = promisor_remote_new(remote_name);

		free(remote_name);

		if (!r)
			return 0;

		return git_config_string(&r->partial_clone_filter, var, value);
	}

	return 0;
}

static int initialized;

static void promisor_remote_init(void)
{
	if (initialized)
		return;
	initialized = 1;

	git_config(promisor_remote_config, NULL);

	if (repository_format_partial_clone) {
		struct promisor_remote *o, *previous;

		o = promisor_remote_lookup(repository_format_partial_clone,
					   &previous);
		if (o)
			promisor_remote_move_to_tail(o, previous);
		else
			promisor_remote_new(repository_format_partial_clone);
	}
}

static void promisor_remote_clear(void)
{
	while (promisors) {
		struct promisor_remote *r = promisors;
		promisors = promisors->next;
		free(r);
	}

	promisors_tail = &promisors;
}

void promisor_remote_reinit(void)
{
	initialized = 0;
	promisor_remote_clear();
	promisor_remote_init();
}

struct promisor_remote *promisor_remote_find(const char *remote_name)
{
	promisor_remote_init();

	if (!remote_name)
		return promisors;

	return promisor_remote_lookup(remote_name, NULL);
}

int has_promisor_remote(void)
{
	return !!promisor_remote_find(NULL);
}

static int remove_fetched_oids(struct repository *repo,
			       struct object_id **oids,
			       int oid_nr, int to_free)
{
	int i, remaining_nr = 0;
	int *remaining = xcalloc(oid_nr, sizeof(*remaining));
	struct object_id *old_oids = *oids;
	struct object_id *new_oids;

	for (i = 0; i < oid_nr; i++)
		if (oid_object_info_extended(repo, &old_oids[i], NULL,
					     OBJECT_INFO_SKIP_FETCH_OBJECT)) {
			remaining[i] = 1;
			remaining_nr++;
		}

	if (remaining_nr) {
		int j = 0;
		new_oids = xcalloc(remaining_nr, sizeof(*new_oids));
		for (i = 0; i < oid_nr; i++)
			if (remaining[i])
				oidcpy(&new_oids[j++], &old_oids[i]);
		*oids = new_oids;
		if (to_free)
			free(old_oids);
	}

	free(remaining);

	return remaining_nr;
}

int promisor_remote_get_direct(struct repository *repo,
			       const struct object_id *oids,
			       int oid_nr)
{
	struct promisor_remote *r;
	struct object_id *remaining_oids = (struct object_id *)oids;
	int remaining_nr = oid_nr;
	int to_free = 0;
	int res = -1;

	if (oid_nr == 0)
		return 0;

	promisor_remote_init();

	for (r = promisors; r; r = r->next) {
		if (fetch_objects(r->name, remaining_oids, remaining_nr) < 0) {
			if (remaining_nr == 1)
				continue;
			remaining_nr = remove_fetched_oids(repo, &remaining_oids,
							 remaining_nr, to_free);
			if (remaining_nr) {
				to_free = 1;
				continue;
			}
		}
		res = 0;
		break;
	}

	if (to_free)
		free(remaining_oids);

	return res;
}
