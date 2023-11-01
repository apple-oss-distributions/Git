/*
 * The Scalar command-line interface.
 */

#include "cache.h"
#include "gettext.h"
#include "parse-options.h"
#include "config.h"
#include "run-command.h"
#include "refs.h"
#include "dir.h"
#include "packfile.h"
#include "help.h"
#include "archive.h"
#include "object-store.h"

/*
 * Remove the deepest subdirectory in the provided path string. Path must not
 * include a trailing path separator. Returns 1 if parent directory found,
 * otherwise 0.
 */
static int strbuf_parent_directory(struct strbuf *buf)
{
	size_t len = buf->len;
	size_t offset = offset_1st_component(buf->buf);
	char *path_sep = find_last_dir_sep(buf->buf + offset);
	strbuf_setlen(buf, path_sep ? path_sep - buf->buf : offset);

	return buf->len < len;
}

static void setup_enlistment_directory(int argc, const char **argv,
				       const char * const *usagestr,
				       const struct option *options,
				       struct strbuf *enlistment_root)
{
	struct strbuf path = STRBUF_INIT;
	char *root;
	int enlistment_found = 0;

	if (startup_info->have_repository)
		BUG("gitdir already set up?!?");

	if (argc > 1)
		usage_with_options(usagestr, options);

	/* find the worktree, determine its corresponding root */
	if (argc == 1) {
		strbuf_add_absolute_path(&path, argv[0]);
		if (!is_directory(path.buf))
			die(_("'%s' does not exist"), path.buf);
	} else if (strbuf_getcwd(&path) < 0)
		die(_("need a working directory"));

	strbuf_trim_trailing_dir_sep(&path);
	do {
		const size_t len = path.len;

		/* check if currently in enlistment root with src/ workdir */
		strbuf_addstr(&path, "/src");
		if (is_nonbare_repository_dir(&path)) {
			if (enlistment_root)
				strbuf_add(enlistment_root, path.buf, len);

			enlistment_found = 1;
			break;
		}

		/* reset to original path */
		strbuf_setlen(&path, len);

		/* check if currently in workdir */
		if (is_nonbare_repository_dir(&path)) {
			if (enlistment_root) {
				/*
				 * If the worktree's directory's name is `src`, the enlistment is the
				 * parent directory, otherwise it is identical to the worktree.
				 */
				root = strip_path_suffix(path.buf, "src");
				strbuf_addstr(enlistment_root, root ? root : path.buf);
				free(root);
			}

			enlistment_found = 1;
			break;
		}
	} while (strbuf_parent_directory(&path));

	if (!enlistment_found)
		die(_("could not find enlistment root"));

	if (chdir(path.buf) < 0)
		die_errno(_("could not switch to '%s'"), path.buf);

	strbuf_release(&path);
	setup_git_directory();
}

static int run_git(const char *arg, ...)
{
	struct strvec argv = STRVEC_INIT;
	va_list args;
	const char *p;
	int res;

	va_start(args, arg);
	strvec_push(&argv, arg);
	while ((p = va_arg(args, const char *)))
		strvec_push(&argv, p);
	va_end(args);

	res = run_command_v_opt(argv.v, RUN_GIT_CMD);

	strvec_clear(&argv);
	return res;
}

static int set_recommended_config(int reconfigure)
{
	struct {
		const char *key;
		const char *value;
		int overwrite_on_reconfigure;
	} config[] = {
		/* Required */
		{ "am.keepCR", "true", 1 },
		{ "core.FSCache", "true", 1 },
		{ "core.multiPackIndex", "true", 1 },
		{ "core.preloadIndex", "true", 1 },
#ifndef WIN32
		{ "core.untrackedCache", "true", 1 },
#else
		/*
		 * Unfortunately, Scalar's Functional Tests demonstrated
		 * that the untracked cache feature is unreliable on Windows
		 * (which is a bummer because that platform would benefit the
		 * most from it). For some reason, freshly created files seem
		 * not to update the directory's `lastModified` time
		 * immediately, but the untracked cache would need to rely on
		 * that.
		 *
		 * Therefore, with a sad heart, we disable this very useful
		 * feature on Windows.
		 */
		{ "core.untrackedCache", "false", 1 },
#endif
		{ "core.logAllRefUpdates", "true", 1 },
		{ "credential.https://dev.azure.com.useHttpPath", "true", 1 },
		{ "credential.validate", "false", 1 }, /* GCM4W-only */
		{ "gc.auto", "0", 1 },
		{ "gui.GCWarning", "false", 1 },
		{ "index.threads", "true", 1 },
		{ "index.version", "4", 1 },
		{ "merge.stat", "false", 1 },
		{ "merge.renames", "true", 1 },
		{ "pack.useBitmaps", "false", 1 },
		{ "pack.useSparse", "true", 1 },
		{ "receive.autoGC", "false", 1 },
		{ "feature.manyFiles", "false", 1 },
		{ "feature.experimental", "false", 1 },
		{ "fetch.unpackLimit", "1", 1 },
		{ "fetch.writeCommitGraph", "false", 1 },
#ifdef WIN32
		{ "http.sslBackend", "schannel", 1 },
#endif
		/* Optional */
		{ "status.aheadBehind", "false" },
		{ "commitGraph.generationVersion", "1" },
		{ "core.autoCRLF", "false" },
		{ "core.safeCRLF", "false" },
		{ "fetch.showForcedUpdates", "false" },
		{ NULL, NULL },
	};
	int i;
	char *value;

	for (i = 0; config[i].key; i++) {
		if ((reconfigure && config[i].overwrite_on_reconfigure) ||
		    git_config_get_string(config[i].key, &value)) {
			trace2_data_string("scalar", the_repository, config[i].key, "created");
			if (git_config_set_gently(config[i].key,
						  config[i].value) < 0)
				return error(_("could not configure %s=%s"),
					     config[i].key, config[i].value);
		} else {
			trace2_data_string("scalar", the_repository, config[i].key, "exists");
			free(value);
		}
	}

	/*
	 * The `log.excludeDecoration` setting is special because it allows
	 * for multiple values.
	 */
	if (git_config_get_string("log.excludeDecoration", &value)) {
		trace2_data_string("scalar", the_repository,
				   "log.excludeDecoration", "created");
		if (git_config_set_multivar_gently("log.excludeDecoration",
						   "refs/prefetch/*",
						   CONFIG_REGEX_NONE, 0))
			return error(_("could not configure "
				       "log.excludeDecoration"));
	} else {
		trace2_data_string("scalar", the_repository,
				   "log.excludeDecoration", "exists");
		free(value);
	}

	return 0;
}

static int toggle_maintenance(int enable)
{
	return run_git("maintenance", enable ? "start" : "unregister", NULL);
}

static int add_or_remove_enlistment(int add)
{
	int res;

	if (!the_repository->worktree)
		die(_("Scalar enlistments require a worktree"));

	res = run_git("config", "--global", "--get", "--fixed-value",
		      "scalar.repo", the_repository->worktree, NULL);

	/*
	 * If we want to add and the setting is already there, then do nothing.
	 * If we want to remove and the setting is not there, then do nothing.
	 */
	if ((add && !res) || (!add && res))
		return 0;

	return run_git("config", "--global", add ? "--add" : "--unset",
		       add ? "--no-fixed-value" : "--fixed-value",
		       "scalar.repo", the_repository->worktree, NULL);
}

static int register_dir(void)
{
	int res = add_or_remove_enlistment(1);

	if (!res)
		res = set_recommended_config(0);

	if (!res)
		res = toggle_maintenance(1);

	return res;
}

static int unregister_dir(void)
{
	int res = 0;

	if (toggle_maintenance(0) < 0)
		res = -1;

	if (add_or_remove_enlistment(0) < 0)
		res = -1;

	return res;
}

static int add_directory_to_archiver(struct strvec *archiver_args,
					  const char *path, int recurse)
{
	int at_root = !*path;
	DIR *dir = opendir(at_root ? "." : path);
	struct dirent *e;
	struct strbuf buf = STRBUF_INIT;
	size_t len;
	int res = 0;

	if (!dir)
		return error_errno(_("could not open directory '%s'"), path);

	if (!at_root)
		strbuf_addf(&buf, "%s/", path);
	len = buf.len;
	strvec_pushf(archiver_args, "--prefix=%s", buf.buf);

	while (!res && (e = readdir(dir))) {
		if (!strcmp(".", e->d_name) || !strcmp("..", e->d_name))
			continue;

		strbuf_setlen(&buf, len);
		strbuf_addstr(&buf, e->d_name);

		if (e->d_type == DT_REG)
			strvec_pushf(archiver_args, "--add-file=%s", buf.buf);
		else if (e->d_type != DT_DIR)
			warning(_("skipping '%s', which is neither file nor "
				  "directory"), buf.buf);
		else if (recurse &&
			 add_directory_to_archiver(archiver_args,
						   buf.buf, recurse) < 0)
			res = -1;
	}

	closedir(dir);
	strbuf_release(&buf);
	return res;
}

#ifndef WIN32
#include <sys/statvfs.h>
#endif

static int get_disk_info(struct strbuf *out)
{
#ifdef WIN32
	struct strbuf buf = STRBUF_INIT;
	char volume_name[MAX_PATH], fs_name[MAX_PATH];
	DWORD serial_number, component_length, flags;
	ULARGE_INTEGER avail2caller, total, avail;

	strbuf_realpath(&buf, ".", 1);
	if (!GetDiskFreeSpaceExA(buf.buf, &avail2caller, &total, &avail)) {
		error(_("could not determine free disk size for '%s'"),
		      buf.buf);
		strbuf_release(&buf);
		return -1;
	}

	strbuf_setlen(&buf, offset_1st_component(buf.buf));
	if (!GetVolumeInformationA(buf.buf, volume_name, sizeof(volume_name),
				   &serial_number, &component_length, &flags,
				   fs_name, sizeof(fs_name))) {
		error(_("could not get info for '%s'"), buf.buf);
		strbuf_release(&buf);
		return -1;
	}
	strbuf_addf(out, "Available space on '%s': ", buf.buf);
	strbuf_humanise_bytes(out, avail2caller.QuadPart);
	strbuf_addch(out, '\n');
	strbuf_release(&buf);
#else
	struct strbuf buf = STRBUF_INIT;
	struct statvfs stat;

	strbuf_realpath(&buf, ".", 1);
	if (statvfs(buf.buf, &stat) < 0) {
		error_errno(_("could not determine free disk size for '%s'"),
			    buf.buf);
		strbuf_release(&buf);
		return -1;
	}

	strbuf_addf(out, "Available space on '%s': ", buf.buf);
	strbuf_humanise_bytes(out, st_mult(stat.f_bsize, stat.f_bavail));
	strbuf_addf(out, " (mount flags 0x%lx)\n", stat.f_flag);
	strbuf_release(&buf);
#endif
	return 0;
}

/* printf-style interface, expects `<key>=<value>` argument */
static int set_config(const char *fmt, ...)
{
	struct strbuf buf = STRBUF_INIT;
	char *value;
	int res;
	va_list args;

	va_start(args, fmt);
	strbuf_vaddf(&buf, fmt, args);
	va_end(args);

	value = strchr(buf.buf, '=');
	if (value)
		*(value++) = '\0';
	res = git_config_set_gently(buf.buf, value);
	strbuf_release(&buf);

	return res;
}

static char *remote_default_branch(const char *url)
{
	struct child_process cp = CHILD_PROCESS_INIT;
	struct strbuf out = STRBUF_INIT;

	cp.git_cmd = 1;
	strvec_pushl(&cp.args, "ls-remote", "--symref", url, "HEAD", NULL);
	if (!pipe_command(&cp, NULL, 0, &out, 0, NULL, 0)) {
		const char *line = out.buf;

		while (*line) {
			const char *eol = strchrnul(line, '\n'), *p;
			size_t len = eol - line;
			char *branch;

			if (!skip_prefix(line, "ref: ", &p) ||
			    !strip_suffix_mem(line, &len, "\tHEAD")) {
				line = eol + (*eol == '\n');
				continue;
			}

			eol = line + len;
			if (skip_prefix(p, "refs/heads/", &p)) {
				branch = xstrndup(p, eol - p);
				strbuf_release(&out);
				return branch;
			}

			error(_("remote HEAD is not a branch: '%.*s'"),
			      (int)(eol - p), p);
			strbuf_release(&out);
			return NULL;
		}
	}
	warning(_("failed to get default branch name from remote; "
		  "using local default"));
	strbuf_reset(&out);

	child_process_init(&cp);
	cp.git_cmd = 1;
	strvec_pushl(&cp.args, "symbolic-ref", "--short", "HEAD", NULL);
	if (!pipe_command(&cp, NULL, 0, &out, 0, NULL, 0)) {
		strbuf_trim(&out);
		return strbuf_detach(&out, NULL);
	}

	strbuf_release(&out);
	error(_("failed to get default branch name"));
	return NULL;
}

static int delete_enlistment(struct strbuf *enlistment)
{
#ifdef WIN32
	struct strbuf parent = STRBUF_INIT;
#endif

	if (unregister_dir())
		die(_("failed to unregister repository"));

#ifdef WIN32
	/*
	 * Change the current directory to one outside of the enlistment so
	 * that we may delete everything underneath it.
	 */
	strbuf_addbuf(&parent, enlistment);
	strbuf_parent_directory(&parent);
	if (chdir(parent.buf) < 0)
		die_errno(_("could not switch to '%s'"), parent.buf);
	strbuf_release(&parent);
#endif

	if (remove_dir_recursively(enlistment, 0))
		die(_("failed to delete enlistment directory"));

	return 0;
}

/*
 * Dummy implementation; Using `get_version_info()` would cause a link error
 * without this.
 */
void load_builtin_commands(const char *prefix, struct cmdnames *cmds)
{
	die("not implemented");
}

static int cmd_clone(int argc, const char **argv)
{
	const char *branch = NULL;
	int full_clone = 0, single_branch = 0;
	struct option clone_options[] = {
		OPT_STRING('b', "branch", &branch, N_("<branch>"),
			   N_("branch to checkout after clone")),
		OPT_BOOL(0, "full-clone", &full_clone,
			 N_("when cloning, create full working directory")),
		OPT_BOOL(0, "single-branch", &single_branch,
			 N_("only download metadata for the branch that will "
			    "be checked out")),
		OPT_END(),
	};
	const char * const clone_usage[] = {
		N_("scalar clone [<options>] [--] <repo> [<dir>]"),
		NULL
	};
	const char *url;
	char *enlistment = NULL, *dir = NULL;
	struct strbuf buf = STRBUF_INIT;
	int res;

	argc = parse_options(argc, argv, NULL, clone_options, clone_usage, 0);

	if (argc == 2) {
		url = argv[0];
		enlistment = xstrdup(argv[1]);
	} else if (argc == 1) {
		url = argv[0];

		strbuf_addstr(&buf, url);
		/* Strip trailing slashes, if any */
		while (buf.len > 0 && is_dir_sep(buf.buf[buf.len - 1]))
			strbuf_setlen(&buf, buf.len - 1);
		/* Strip suffix `.git`, if any */
		strbuf_strip_suffix(&buf, ".git");

		enlistment = find_last_dir_sep(buf.buf);
		if (!enlistment) {
			die(_("cannot deduce worktree name from '%s'"), url);
		}
		enlistment = xstrdup(enlistment + 1);
	} else {
		usage_msg_opt(_("You must specify a repository to clone."),
			      clone_usage, clone_options);
	}

	if (is_directory(enlistment))
		die(_("directory '%s' exists already"), enlistment);

	dir = xstrfmt("%s/src", enlistment);

	strbuf_reset(&buf);
	if (branch)
		strbuf_addf(&buf, "init.defaultBranch=%s", branch);
	else {
		char *b = repo_default_branch_name(the_repository, 1);
		strbuf_addf(&buf, "init.defaultBranch=%s", b);
		free(b);
	}

	if ((res = run_git("-c", buf.buf, "init", "--", dir, NULL)))
		goto cleanup;

	if (chdir(dir) < 0) {
		res = error_errno(_("could not switch to '%s'"), dir);
		goto cleanup;
	}

	setup_git_directory();

	/* common-main already logs `argv` */
	trace2_def_repo(the_repository);

	if (!branch && !(branch = remote_default_branch(url))) {
		res = error(_("failed to get default branch for '%s'"), url);
		goto cleanup;
	}

	if (set_config("remote.origin.url=%s", url) ||
	    set_config("remote.origin.fetch="
		       "+refs/heads/%s:refs/remotes/origin/%s",
		       single_branch ? branch : "*",
		       single_branch ? branch : "*") ||
	    set_config("remote.origin.promisor=true") ||
	    set_config("remote.origin.partialCloneFilter=blob:none")) {
		res = error(_("could not configure remote in '%s'"), dir);
		goto cleanup;
	}

	if (!full_clone &&
	    (res = run_git("sparse-checkout", "init", "--cone", NULL)))
		goto cleanup;

	if (set_recommended_config(0))
		return error(_("could not configure '%s'"), dir);

	if ((res = run_git("fetch", "--quiet", "origin", NULL))) {
		warning(_("partial clone failed; attempting full clone"));

		if (set_config("remote.origin.promisor") ||
		    set_config("remote.origin.partialCloneFilter")) {
			res = error(_("could not configure for full clone"));
			goto cleanup;
		}

		if ((res = run_git("fetch", "--quiet", "origin", NULL)))
			goto cleanup;
	}

	if ((res = set_config("branch.%s.remote=origin", branch)))
		goto cleanup;
	if ((res = set_config("branch.%s.merge=refs/heads/%s",
			      branch, branch)))
		goto cleanup;

	strbuf_reset(&buf);
	strbuf_addf(&buf, "origin/%s", branch);
	res = run_git("checkout", "-f", "-t", buf.buf, NULL);
	if (res)
		goto cleanup;

	res = register_dir();

cleanup:
	free(enlistment);
	free(dir);
	strbuf_release(&buf);
	return res;
}

static void dir_file_stats_objects(const char *full_path, size_t full_path_len,
				   const char *file_name, void *data)
{
	struct strbuf *buf = data;
	struct stat st;

	if (!stat(full_path, &st))
		strbuf_addf(buf, "%-70s %16" PRIuMAX "\n", file_name,
			    (uintmax_t)st.st_size);
}

static int dir_file_stats(struct object_directory *object_dir, void *data)
{
	struct strbuf *buf = data;

	strbuf_addf(buf, "Contents of %s:\n", object_dir->path);

	for_each_file_in_pack_dir(object_dir->path, dir_file_stats_objects,
				  data);

	return 0;
}

static int count_files(char *path)
{
	DIR *dir = opendir(path);
	struct dirent *e;
	int count = 0;

	if (!dir)
		return 0;

	while ((e = readdir(dir)) != NULL)
		if (!is_dot_or_dotdot(e->d_name) && e->d_type == DT_REG)
			count++;

	closedir(dir);
	return count;
}

static void loose_objs_stats(struct strbuf *buf, const char *path)
{
	DIR *dir = opendir(path);
	struct dirent *e;
	int count;
	int total = 0;
	unsigned char c;
	struct strbuf count_path = STRBUF_INIT;
	size_t base_path_len;

	if (!dir)
		return;

	strbuf_addstr(buf, "Object directory stats for ");
	strbuf_add_absolute_path(buf, path);
	strbuf_addstr(buf, ":\n");

	strbuf_add_absolute_path(&count_path, path);
	strbuf_addch(&count_path, '/');
	base_path_len = count_path.len;

	while ((e = readdir(dir)) != NULL)
		if (!is_dot_or_dotdot(e->d_name) &&
		    e->d_type == DT_DIR && strlen(e->d_name) == 2 &&
		    !hex_to_bytes(&c, e->d_name, 1)) {
			strbuf_setlen(&count_path, base_path_len);
			strbuf_addstr(&count_path, e->d_name);
			total += (count = count_files(count_path.buf));
			strbuf_addf(buf, "%s : %7d files\n", e->d_name, count);
		}

	strbuf_addf(buf, "Total: %d loose objects", total);

	strbuf_release(&count_path);
	closedir(dir);
}

static int cmd_diagnose(int argc, const char **argv)
{
	struct option options[] = {
		OPT_END(),
	};
	const char * const usage[] = {
		N_("scalar diagnose [<enlistment>]"),
		NULL
	};
	struct strbuf zip_path = STRBUF_INIT;
	struct strvec archiver_args = STRVEC_INIT;
	char **argv_copy = NULL;
	int stdout_fd = -1, archiver_fd = -1;
	time_t now = time(NULL);
	struct tm tm;
	struct strbuf path = STRBUF_INIT, buf = STRBUF_INIT;
	int res = 0;

	argc = parse_options(argc, argv, NULL, options,
			     usage, 0);

	setup_enlistment_directory(argc, argv, usage, options, &zip_path);

	strbuf_addstr(&zip_path, "/.scalarDiagnostics/scalar_");
	strbuf_addftime(&zip_path,
			"%Y%m%d_%H%M%S", localtime_r(&now, &tm), 0, 0);
	strbuf_addstr(&zip_path, ".zip");
	switch (safe_create_leading_directories(zip_path.buf)) {
	case SCLD_EXISTS:
	case SCLD_OK:
		break;
	default:
		error_errno(_("could not create directory for '%s'"),
			    zip_path.buf);
		goto diagnose_cleanup;
	}
	stdout_fd = dup(1);
	if (stdout_fd < 0) {
		res = error_errno(_("could not duplicate stdout"));
		goto diagnose_cleanup;
	}

	archiver_fd = xopen(zip_path.buf, O_CREAT | O_WRONLY | O_TRUNC, 0666);
	if (archiver_fd < 0 || dup2(archiver_fd, 1) < 0) {
		res = error_errno(_("could not redirect output"));
		goto diagnose_cleanup;
	}

	init_zip_archiver();
	strvec_pushl(&archiver_args, "scalar-diagnose", "--format=zip", NULL);

	strbuf_reset(&buf);
	strbuf_addstr(&buf, "Collecting diagnostic info\n\n");
	get_version_info(&buf, 1);

	strbuf_addf(&buf, "Enlistment root: %s\n", the_repository->worktree);
	get_disk_info(&buf);
	write_or_die(stdout_fd, buf.buf, buf.len);
	strvec_pushf(&archiver_args,
		     "--add-virtual-file=diagnostics.log:%.*s",
		     (int)buf.len, buf.buf);

	strbuf_reset(&buf);
	strbuf_addstr(&buf, "--add-virtual-file=packs-local.txt:");
	dir_file_stats(the_repository->objects->odb, &buf);
	foreach_alt_odb(dir_file_stats, &buf);
	strvec_push(&archiver_args, buf.buf);

	strbuf_reset(&buf);
	strbuf_addstr(&buf, "--add-virtual-file=objects-local.txt:");
	loose_objs_stats(&buf, ".git/objects");
	strvec_push(&archiver_args, buf.buf);

	if ((res = add_directory_to_archiver(&archiver_args, ".git", 0)) ||
	    (res = add_directory_to_archiver(&archiver_args, ".git/hooks", 0)) ||
	    (res = add_directory_to_archiver(&archiver_args, ".git/info", 0)) ||
	    (res = add_directory_to_archiver(&archiver_args, ".git/logs", 1)) ||
	    (res = add_directory_to_archiver(&archiver_args, ".git/objects/info", 0)))
		goto diagnose_cleanup;

	strvec_pushl(&archiver_args, "--prefix=",
		     oid_to_hex(the_hash_algo->empty_tree), "--", NULL);

	/* `write_archive()` modifies the `argv` passed to it. Let it. */
	argv_copy = xmemdupz(archiver_args.v,
			     sizeof(char *) * archiver_args.nr);
	res = write_archive(archiver_args.nr, (const char **)argv_copy, NULL,
			    the_repository, NULL, 0);
	if (res) {
		error(_("failed to write archive"));
		goto diagnose_cleanup;
	}

	if (!res)
		fprintf(stderr, "\n"
		       "Diagnostics complete.\n"
		       "All of the gathered info is captured in '%s'\n",
		       zip_path.buf);

diagnose_cleanup:
	if (archiver_fd >= 0) {
		close(1);
		dup2(stdout_fd, 1);
	}
	free(argv_copy);
	strvec_clear(&archiver_args);
	strbuf_release(&zip_path);
	strbuf_release(&path);
	strbuf_release(&buf);

	return res;
}

static int cmd_list(int argc, const char **argv)
{
	if (argc != 1)
		die(_("`scalar list` does not take arguments"));

	if (run_git("config", "--global", "--get-all", "scalar.repo", NULL) < 0)
		return -1;
	return 0;
}

static int cmd_register(int argc, const char **argv)
{
	struct option options[] = {
		OPT_END(),
	};
	const char * const usage[] = {
		N_("scalar register [<enlistment>]"),
		NULL
	};

	argc = parse_options(argc, argv, NULL, options,
			     usage, 0);

	setup_enlistment_directory(argc, argv, usage, options, NULL);

	return register_dir();
}

static int get_scalar_repos(const char *key, const char *value, void *data)
{
	struct string_list *list = data;

	if (!strcmp(key, "scalar.repo"))
		string_list_append(list, value);

	return 0;
}

static int cmd_reconfigure(int argc, const char **argv)
{
	int all = 0;
	struct option options[] = {
		OPT_BOOL('a', "all", &all,
			 N_("reconfigure all registered enlistments")),
		OPT_END(),
	};
	const char * const usage[] = {
		N_("scalar reconfigure [--all | <enlistment>]"),
		NULL
	};
	struct string_list scalar_repos = STRING_LIST_INIT_DUP;
	int i, res = 0;
	struct repository r = { NULL };
	struct strbuf commondir = STRBUF_INIT, gitdir = STRBUF_INIT;

	argc = parse_options(argc, argv, NULL, options,
			     usage, 0);

	if (!all) {
		setup_enlistment_directory(argc, argv, usage, options, NULL);

		return set_recommended_config(1);
	}

	if (argc > 0)
		usage_msg_opt(_("--all or <enlistment>, but not both"),
			      usage, options);

	git_config(get_scalar_repos, &scalar_repos);

	for (i = 0; i < scalar_repos.nr; i++) {
		const char *dir = scalar_repos.items[i].string;

		strbuf_reset(&commondir);
		strbuf_reset(&gitdir);

		if (chdir(dir) < 0) {
			warning_errno(_("could not switch to '%s'"), dir);
			res = -1;
		} else if (discover_git_directory(&commondir, &gitdir) < 0) {
			warning_errno(_("git repository gone in '%s'"), dir);
			res = -1;
		} else {
			git_config_clear();

			the_repository = &r;
			r.commondir = commondir.buf;
			r.gitdir = gitdir.buf;

			if (set_recommended_config(1) < 0)
				res = -1;
		}
	}

	string_list_clear(&scalar_repos, 1);
	strbuf_release(&commondir);
	strbuf_release(&gitdir);

	return res;
}

static int cmd_run(int argc, const char **argv)
{
	struct option options[] = {
		OPT_END(),
	};
	struct {
		const char *arg, *task;
	} tasks[] = {
		{ "config", NULL },
		{ "commit-graph", "commit-graph" },
		{ "fetch", "prefetch" },
		{ "loose-objects", "loose-objects" },
		{ "pack-files", "incremental-repack" },
		{ NULL, NULL }
	};
	struct strbuf buf = STRBUF_INIT;
	const char *usagestr[] = { NULL, NULL };
	int i;

	strbuf_addstr(&buf, N_("scalar run <task> [<enlistment>]\nTasks:\n"));
	for (i = 0; tasks[i].arg; i++)
		strbuf_addf(&buf, "\t%s\n", tasks[i].arg);
	usagestr[0] = buf.buf;

	argc = parse_options(argc, argv, NULL, options,
			     usagestr, 0);

	if (!argc)
		usage_with_options(usagestr, options);

	if (!strcmp("all", argv[0])) {
		i = -1;
	} else {
		for (i = 0; tasks[i].arg && strcmp(tasks[i].arg, argv[0]); i++)
			; /* keep looking for the task */

		if (i > 0 && !tasks[i].arg) {
			error(_("no such task: '%s'"), argv[0]);
			usage_with_options(usagestr, options);
		}
	}

	argc--;
	argv++;
	setup_enlistment_directory(argc, argv, usagestr, options, NULL);
	strbuf_release(&buf);

	if (i == 0)
		return register_dir();

	if (i > 0)
		return run_git("maintenance", "run",
			       "--task", tasks[i].task, NULL);

	if (register_dir())
		return -1;
	for (i = 1; tasks[i].arg; i++)
		if (run_git("maintenance", "run",
			    "--task", tasks[i].task, NULL))
			return -1;
	return 0;
}

static int remove_deleted_enlistment(struct strbuf *path)
{
	int res = 0;
	strbuf_realpath_forgiving(path, path->buf, 1);

	if (run_git("config", "--global",
		    "--unset", "--fixed-value",
		    "scalar.repo", path->buf, NULL) < 0)
		res = -1;

	if (run_git("config", "--global",
		    "--unset", "--fixed-value",
		    "maintenance.repo", path->buf, NULL) < 0)
		res = -1;

	return res;
}

static int cmd_unregister(int argc, const char **argv)
{
	struct option options[] = {
		OPT_END(),
	};
	const char * const usage[] = {
		N_("scalar unregister [<enlistment>]"),
		NULL
	};

	argc = parse_options(argc, argv, NULL, options,
			     usage, 0);

	/*
	 * Be forgiving when the enlistment or worktree does not even exist any
	 * longer; This can be the case if a user deleted the worktree by
	 * mistake and _still_ wants to unregister the thing.
	 */
	if (argc == 1) {
		struct strbuf src_path = STRBUF_INIT, workdir_path = STRBUF_INIT;

		strbuf_addf(&src_path, "%s/src/.git", argv[0]);
		strbuf_addf(&workdir_path, "%s/.git", argv[0]);
		if (!is_directory(src_path.buf) && !is_directory(workdir_path.buf)) {
			/* remove possible matching registrations */
			int res = -1;

			strbuf_strip_suffix(&src_path, "/.git");
			res = remove_deleted_enlistment(&src_path) && res;

			strbuf_strip_suffix(&workdir_path, "/.git");
			res = remove_deleted_enlistment(&workdir_path) && res;

			strbuf_release(&src_path);
			strbuf_release(&workdir_path);
			return res;
		}
		strbuf_release(&src_path);
		strbuf_release(&workdir_path);
	}

	setup_enlistment_directory(argc, argv, usage, options, NULL);

	return unregister_dir();
}

static int cmd_delete(int argc, const char **argv)
{
	char *cwd = xgetcwd();
	struct option options[] = {
		OPT_END(),
	};
	const char * const usage[] = {
		N_("scalar delete <enlistment>"),
		NULL
	};
	struct strbuf enlistment = STRBUF_INIT;
	int res = 0;

	argc = parse_options(argc, argv, NULL, options,
			     usage, 0);

	if (argc != 1)
		usage_with_options(usage, options);

	setup_enlistment_directory(argc, argv, usage, options, &enlistment);

	if (dir_inside_of(cwd, enlistment.buf) >= 0)
		res = error(_("refusing to delete current working directory"));
	else {
		close_object_store(the_repository->objects);
		res = delete_enlistment(&enlistment);
	}
	strbuf_release(&enlistment);
	free(cwd);

	return res;
}

static int cmd_version(int argc, const char **argv)
{
	int verbose = 0, build_options = 0;
	struct option options[] = {
		OPT__VERBOSE(&verbose, N_("include Git version")),
		OPT_BOOL(0, "build-options", &build_options,
			 N_("include Git's build options")),
		OPT_END(),
	};
	const char * const usage[] = {
		N_("scalar verbose [-v | --verbose] [--build-options]"),
		NULL
	};
	struct strbuf buf = STRBUF_INIT;

	argc = parse_options(argc, argv, NULL, options,
			     usage, 0);

	if (argc != 0)
		usage_with_options(usage, options);

	get_version_info(&buf, build_options);
	fprintf(stderr, "%s\n", buf.buf);
	strbuf_release(&buf);

	return 0;
}

static struct {
	const char *name;
	int (*fn)(int, const char **);
} builtins[] = {
	{ "clone", cmd_clone },
	{ "list", cmd_list },
	{ "register", cmd_register },
	{ "unregister", cmd_unregister },
	{ "run", cmd_run },
	{ "reconfigure", cmd_reconfigure },
	{ "delete", cmd_delete },
	{ "version", cmd_version },
	{ "diagnose", cmd_diagnose },
	{ NULL, NULL},
};

int cmd_main(int argc, const char **argv)
{
	struct strbuf scalar_usage = STRBUF_INIT;
	int i;

	while (argc > 1 && *argv[1] == '-') {
		if (!strcmp(argv[1], "-C")) {
			if (argc < 3)
				die(_("-C requires a <directory>"));
			if (chdir(argv[2]) < 0)
				die_errno(_("could not change to '%s'"),
					  argv[2]);
			argc -= 2;
			argv += 2;
		} else if (!strcmp(argv[1], "-c")) {
			if (argc < 3)
				die(_("-c requires a <key>=<value> argument"));
			git_config_push_parameter(argv[2]);
			argc -= 2;
			argv += 2;
		} else
			break;
	}

	if (argc > 1) {
		argv++;
		argc--;

		for (i = 0; builtins[i].name; i++)
			if (!strcmp(builtins[i].name, argv[0]))
				return !!builtins[i].fn(argc, argv);
	}

	strbuf_addstr(&scalar_usage,
		      N_("scalar [-C <directory>] [-c <key>=<value>] "
			 "<command> [<options>]\n\nCommands:\n"));
	for (i = 0; builtins[i].name; i++)
		strbuf_addf(&scalar_usage, "\t%s\n", builtins[i].name);

	usage(scalar_usage.buf);
}
