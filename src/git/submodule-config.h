#ifndef SUBMODULE_CONFIG_CACHE_H
#define SUBMODULE_CONFIG_CACHE_H

#include "hashmap.h"
#include "submodule.h"
#include "strbuf.h"

/*
 * Submodule entry containing the information about a certain submodule
 * in a certain revision.
 */
struct submodule {
	const char *path;
	const char *name;
	const char *url;
	int fetch_recurse;
	const char *ignore;
	const char *branch;
	struct submodule_update_strategy update_strategy;
	/* the sha1 blob id of the responsible .gitmodules file */
	unsigned char gitmodules_sha1[20];
	int recommend_shallow;
};

struct submodule_cache;
struct repository;

extern void submodule_cache_free(struct submodule_cache *cache);

extern int parse_submodule_fetchjobs(const char *var, const char *value);
extern int parse_fetch_recurse_submodules_arg(const char *opt, const char *arg);
struct option;
extern int option_fetch_parse_recurse_submodules(const struct option *opt,
						 const char *arg, int unset);
extern int parse_update_recurse_submodules_arg(const char *opt, const char *arg);
extern int parse_push_recurse_submodules_arg(const char *opt, const char *arg);
extern void repo_read_gitmodules(struct repository *repo);
extern void gitmodules_config_oid(const struct object_id *commit_oid);
extern const struct submodule *submodule_from_name(
		const struct object_id *commit_or_tree, const char *name);
extern const struct submodule *submodule_from_path(
		const struct object_id *commit_or_tree, const char *path);
extern const struct submodule *submodule_from_cache(struct repository *repo,
						    const struct object_id *treeish_name,
						    const char *key);
extern void submodule_free(void);

/*
 * Returns 0 if the name is syntactically acceptable as a submodule "name"
 * (e.g., that may be found in the subsection of a .gitmodules file) and -1
 * otherwise.
 */
int check_submodule_name(const char *name);

#endif /* SUBMODULE_CONFIG_H */
