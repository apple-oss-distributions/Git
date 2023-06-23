#include "builtin.h"
#include "cache.h"
#include "parse-options.h"
#include "bisect.h"
#include "refs.h"
#include "dir.h"
#include "strvec.h"
#include "run-command.h"
#include "prompt.h"
#include "quote.h"
#include "revision.h"

static GIT_PATH_FUNC(git_path_bisect_terms, "BISECT_TERMS")
static GIT_PATH_FUNC(git_path_bisect_expected_rev, "BISECT_EXPECTED_REV")
static GIT_PATH_FUNC(git_path_bisect_ancestors_ok, "BISECT_ANCESTORS_OK")
static GIT_PATH_FUNC(git_path_bisect_start, "BISECT_START")
static GIT_PATH_FUNC(git_path_bisect_log, "BISECT_LOG")
static GIT_PATH_FUNC(git_path_head_name, "head-name")
static GIT_PATH_FUNC(git_path_bisect_names, "BISECT_NAMES")
static GIT_PATH_FUNC(git_path_bisect_first_parent, "BISECT_FIRST_PARENT")

static const char * const git_bisect_helper_usage[] = {
	N_("git bisect--helper --bisect-reset [<commit>]"),
	N_("git bisect--helper --bisect-write [--no-log] <state> <revision> <good_term> <bad_term>"),
	N_("git bisect--helper --bisect-check-and-set-terms <command> <good_term> <bad_term>"),
	N_("git bisect--helper --bisect-next-check <good_term> <bad_term> [<term>]"),
	N_("git bisect--helper --bisect-terms [--term-good | --term-old | --term-bad | --term-new]"),
	N_("git bisect--helper --bisect-start [--term-{new,bad}=<term> --term-{old,good}=<term>]"
					    " [--no-checkout] [--first-parent] [<bad> [<good>...]] [--] [<paths>...]"),
	N_("git bisect--helper --bisect-next"),
	N_("git bisect--helper --bisect-auto-next"),
	N_("git bisect--helper --bisect-state (bad|new) [<rev>]"),
	N_("git bisect--helper --bisect-state (good|old) [<rev>...]"),
	NULL
};

struct add_bisect_ref_data {
	struct rev_info *revs;
	unsigned int object_flags;
};

struct bisect_terms {
	char *term_good;
	char *term_bad;
};

static void free_terms(struct bisect_terms *terms)
{
	FREE_AND_NULL(terms->term_good);
	FREE_AND_NULL(terms->term_bad);
}

static void set_terms(struct bisect_terms *terms, const char *bad,
		      const char *good)
{
	free((void *)terms->term_good);
	terms->term_good = xstrdup(good);
	free((void *)terms->term_bad);
	terms->term_bad = xstrdup(bad);
}

static const char vocab_bad[] = "bad|new";
static const char vocab_good[] = "good|old";

static int bisect_autostart(struct bisect_terms *terms);

/*
 * Check whether the string `term` belongs to the set of strings
 * included in the variable arguments.
 */
LAST_ARG_MUST_BE_NULL
static int one_of(const char *term, ...)
{
	int res = 0;
	va_list matches;
	const char *match;

	va_start(matches, term);
	while (!res && (match = va_arg(matches, const char *)))
		res = !strcmp(term, match);
	va_end(matches);

	return res;
}

/*
 * return code BISECT_INTERNAL_SUCCESS_MERGE_BASE
 * and BISECT_INTERNAL_SUCCESS_1ST_BAD_FOUND are codes
 * that indicate special success.
 */

static int is_bisect_success(enum bisect_error res)
{
	return !res ||
		res == BISECT_INTERNAL_SUCCESS_1ST_BAD_FOUND ||
		res == BISECT_INTERNAL_SUCCESS_MERGE_BASE;
}

static int write_in_file(const char *path, const char *mode, const char *format, va_list args)
{
	FILE *fp = NULL;
	int res = 0;

	if (strcmp(mode, "w") && strcmp(mode, "a"))
		BUG("write-in-file does not support '%s' mode", mode);
	fp = fopen(path, mode);
	if (!fp)
		return error_errno(_("cannot open file '%s' in mode '%s'"), path, mode);
	res = vfprintf(fp, format, args);

	if (res < 0) {
		int saved_errno = errno;
		fclose(fp);
		errno = saved_errno;
		return error_errno(_("could not write to file '%s'"), path);
	}

	return fclose(fp);
}

static int write_to_file(const char *path, const char *format, ...)
{
	int res;
	va_list args;

	va_start(args, format);
	res = write_in_file(path, "w", format, args);
	va_end(args);

	return res;
}

static int append_to_file(const char *path, const char *format, ...)
{
	int res;
	va_list args;

	va_start(args, format);
	res = write_in_file(path, "a", format, args);
	va_end(args);

	return res;
}

static int check_term_format(const char *term, const char *orig_term)
{
	int res;
	char *new_term = xstrfmt("refs/bisect/%s", term);

	res = check_refname_format(new_term, 0);
	free(new_term);

	if (res)
		return error(_("'%s' is not a valid term"), term);

	if (one_of(term, "help", "start", "skip", "next", "reset",
			"visualize", "view", "replay", "log", "run", "terms", NULL))
		return error(_("can't use the builtin command '%s' as a term"), term);

	/*
	 * In theory, nothing prevents swapping completely good and bad,
	 * but this situation could be confusing and hasn't been tested
	 * enough. Forbid it for now.
	 */

	if ((strcmp(orig_term, "bad") && one_of(term, "bad", "new", NULL)) ||
		 (strcmp(orig_term, "good") && one_of(term, "good", "old", NULL)))
		return error(_("can't change the meaning of the term '%s'"), term);

	return 0;
}

static int write_terms(const char *bad, const char *good)
{
	int res;

	if (!strcmp(bad, good))
		return error(_("please use two different terms"));

	if (check_term_format(bad, "bad") || check_term_format(good, "good"))
		return -1;

	res = write_to_file(git_path_bisect_terms(), "%s\n%s\n", bad, good);

	return res;
}

static int bisect_reset(const char *commit)
{
	struct strbuf branch = STRBUF_INIT;

	if (!commit) {
		if (strbuf_read_file(&branch, git_path_bisect_start(), 0) < 1) {
			printf(_("We are not bisecting.\n"));
			return 0;
		}
		strbuf_rtrim(&branch);
	} else {
		struct object_id oid;

		if (get_oid_commit(commit, &oid))
			return error(_("'%s' is not a valid commit"), commit);
		strbuf_addstr(&branch, commit);
	}

	if (!ref_exists("BISECT_HEAD")) {
		struct strvec argv = STRVEC_INIT;

		strvec_pushl(&argv, "checkout", branch.buf, "--", NULL);
		if (run_command_v_opt(argv.v, RUN_GIT_CMD)) {
			error(_("could not check out original"
				" HEAD '%s'. Try 'git bisect"
				" reset <commit>'."), branch.buf);
			strbuf_release(&branch);
			strvec_clear(&argv);
			return -1;
		}
		strvec_clear(&argv);
	}

	strbuf_release(&branch);
	return bisect_clean_state();
}

static void log_commit(FILE *fp, char *fmt, const char *state,
		       struct commit *commit)
{
	struct pretty_print_context pp = {0};
	struct strbuf commit_msg = STRBUF_INIT;
	char *label = xstrfmt(fmt, state);

	format_commit_message(commit, "%s", &commit_msg, &pp);

	fprintf(fp, "# %s: [%s] %s\n", label, oid_to_hex(&commit->object.oid),
		commit_msg.buf);

	strbuf_release(&commit_msg);
	free(label);
}

static int bisect_write(const char *state, const char *rev,
			const struct bisect_terms *terms, int nolog)
{
	struct strbuf tag = STRBUF_INIT;
	struct object_id oid;
	struct commit *commit;
	FILE *fp = NULL;
	int res = 0;

	if (!strcmp(state, terms->term_bad)) {
		strbuf_addf(&tag, "refs/bisect/%s", state);
	} else if (one_of(state, terms->term_good, "skip", NULL)) {
		strbuf_addf(&tag, "refs/bisect/%s-%s", state, rev);
	} else {
		res = error(_("Bad bisect_write argument: %s"), state);
		goto finish;
	}

	if (get_oid(rev, &oid)) {
		res = error(_("couldn't get the oid of the rev '%s'"), rev);
		goto finish;
	}

	if (update_ref(NULL, tag.buf, &oid, NULL, 0,
		       UPDATE_REFS_MSG_ON_ERR)) {
		res = -1;
		goto finish;
	}

	fp = fopen(git_path_bisect_log(), "a");
	if (!fp) {
		res = error_errno(_("couldn't open the file '%s'"), git_path_bisect_log());
		goto finish;
	}

	commit = lookup_commit_reference(the_repository, &oid);
	log_commit(fp, "%s", state, commit);

	if (!nolog)
		fprintf(fp, "git bisect %s %s\n", state, rev);

finish:
	if (fp)
		fclose(fp);
	strbuf_release(&tag);
	return res;
}

static int check_and_set_terms(struct bisect_terms *terms, const char *cmd)
{
	int has_term_file = !is_empty_or_missing_file(git_path_bisect_terms());

	if (one_of(cmd, "skip", "start", "terms", NULL))
		return 0;

	if (has_term_file && strcmp(cmd, terms->term_bad) &&
	    strcmp(cmd, terms->term_good))
		return error(_("Invalid command: you're currently in a "
				"%s/%s bisect"), terms->term_bad,
				terms->term_good);

	if (!has_term_file) {
		if (one_of(cmd, "bad", "good", NULL)) {
			set_terms(terms, "bad", "good");
			return write_terms(terms->term_bad, terms->term_good);
		}
		if (one_of(cmd, "new", "old", NULL)) {
			set_terms(terms, "new", "old");
			return write_terms(terms->term_bad, terms->term_good);
		}
	}

	return 0;
}

static int mark_good(const char *refname, const struct object_id *oid,
		     int flag, void *cb_data)
{
	int *m_good = (int *)cb_data;
	*m_good = 0;
	return 1;
}

static const char need_bad_and_good_revision_warning[] =
	N_("You need to give me at least one %s and %s revision.\n"
	   "You can use \"git bisect %s\" and \"git bisect %s\" for that.");

static const char need_bisect_start_warning[] =
	N_("You need to start by \"git bisect start\".\n"
	   "You then need to give me at least one %s and %s revision.\n"
	   "You can use \"git bisect %s\" and \"git bisect %s\" for that.");

static int decide_next(const struct bisect_terms *terms,
		       const char *current_term, int missing_good,
		       int missing_bad)
{
	if (!missing_good && !missing_bad)
		return 0;
	if (!current_term)
		return -1;

	if (missing_good && !missing_bad &&
	    !strcmp(current_term, terms->term_good)) {
		char *yesno;
		/*
		 * have bad (or new) but not good (or old). We could bisect
		 * although this is less optimum.
		 */
		warning(_("bisecting only with a %s commit"), terms->term_bad);
		if (!isatty(0))
			return 0;
		/*
		 * TRANSLATORS: Make sure to include [Y] and [n] in your
		 * translation. The program will only accept English input
		 * at this point.
		 */
		yesno = git_prompt(_("Are you sure [Y/n]? "), PROMPT_ECHO);
		if (starts_with(yesno, "N") || starts_with(yesno, "n"))
			return -1;
		return 0;
	}

	if (!is_empty_or_missing_file(git_path_bisect_start()))
		return error(_(need_bad_and_good_revision_warning),
			     vocab_bad, vocab_good, vocab_bad, vocab_good);
	else
		return error(_(need_bisect_start_warning),
			     vocab_good, vocab_bad, vocab_good, vocab_bad);
}

static int bisect_next_check(const struct bisect_terms *terms,
			     const char *current_term)
{
	int missing_good = 1, missing_bad = 1;
	char *bad_ref = xstrfmt("refs/bisect/%s", terms->term_bad);
	char *good_glob = xstrfmt("%s-*", terms->term_good);

	if (ref_exists(bad_ref))
		missing_bad = 0;

	for_each_glob_ref_in(mark_good, good_glob, "refs/bisect/",
			     (void *) &missing_good);

	free(good_glob);
	free(bad_ref);

	return decide_next(terms, current_term, missing_good, missing_bad);
}

static int get_terms(struct bisect_terms *terms)
{
	struct strbuf str = STRBUF_INIT;
	FILE *fp = NULL;
	int res = 0;

	fp = fopen(git_path_bisect_terms(), "r");
	if (!fp) {
		res = -1;
		goto finish;
	}

	free_terms(terms);
	strbuf_getline_lf(&str, fp);
	terms->term_bad = strbuf_detach(&str, NULL);
	strbuf_getline_lf(&str, fp);
	terms->term_good = strbuf_detach(&str, NULL);

finish:
	if (fp)
		fclose(fp);
	strbuf_release(&str);
	return res;
}

static int bisect_terms(struct bisect_terms *terms, const char *option)
{
	if (get_terms(terms))
		return error(_("no terms defined"));

	if (option == NULL) {
		printf(_("Your current terms are %s for the old state\n"
			 "and %s for the new state.\n"),
		       terms->term_good, terms->term_bad);
		return 0;
	}
	if (one_of(option, "--term-good", "--term-old", NULL))
		printf("%s\n", terms->term_good);
	else if (one_of(option, "--term-bad", "--term-new", NULL))
		printf("%s\n", terms->term_bad);
	else
		return error(_("invalid argument %s for 'git bisect terms'.\n"
			       "Supported options are: "
			       "--term-good|--term-old and "
			       "--term-bad|--term-new."), option);

	return 0;
}

static int bisect_append_log_quoted(const char **argv)
{
	int res = 0;
	FILE *fp = fopen(git_path_bisect_log(), "a");
	struct strbuf orig_args = STRBUF_INIT;

	if (!fp)
		return -1;

	if (fprintf(fp, "git bisect start") < 1) {
		res = -1;
		goto finish;
	}

	sq_quote_argv(&orig_args, argv);
	if (fprintf(fp, "%s\n", orig_args.buf) < 1)
		res = -1;

finish:
	fclose(fp);
	strbuf_release(&orig_args);
	return res;
}

static int add_bisect_ref(const char *refname, const struct object_id *oid,
			  int flags, void *cb)
{
	struct add_bisect_ref_data *data = cb;

	add_pending_oid(data->revs, refname, oid, data->object_flags);

	return 0;
}

static int prepare_revs(struct bisect_terms *terms, struct rev_info *revs)
{
	int res = 0;
	struct add_bisect_ref_data cb = { revs };
	char *good = xstrfmt("%s-*", terms->term_good);

	/*
	 * We cannot use terms->term_bad directly in
	 * for_each_glob_ref_in() and we have to append a '*' to it,
	 * otherwise for_each_glob_ref_in() will append '/' and '*'.
	 */
	char *bad = xstrfmt("%s*", terms->term_bad);

	/*
	 * It is important to reset the flags used by revision walks
	 * as the previous call to bisect_next_all() in turn
	 * sets up a revision walk.
	 */
	reset_revision_walk();
	init_revisions(revs, NULL);
	setup_revisions(0, NULL, revs, NULL);
	for_each_glob_ref_in(add_bisect_ref, bad, "refs/bisect/", &cb);
	cb.object_flags = UNINTERESTING;
	for_each_glob_ref_in(add_bisect_ref, good, "refs/bisect/", &cb);
	if (prepare_revision_walk(revs))
		res = error(_("revision walk setup failed\n"));

	free(good);
	free(bad);
	return res;
}

static int bisect_skipped_commits(struct bisect_terms *terms)
{
	int res;
	FILE *fp = NULL;
	struct rev_info revs;
	struct commit *commit;
	struct pretty_print_context pp = {0};
	struct strbuf commit_name = STRBUF_INIT;

	res = prepare_revs(terms, &revs);
	if (res)
		return res;

	fp = fopen(git_path_bisect_log(), "a");
	if (!fp)
		return error_errno(_("could not open '%s' for appending"),
				  git_path_bisect_log());

	if (fprintf(fp, "# only skipped commits left to test\n") < 0)
		return error_errno(_("failed to write to '%s'"), git_path_bisect_log());

	while ((commit = get_revision(&revs)) != NULL) {
		strbuf_reset(&commit_name);
		format_commit_message(commit, "%s",
				      &commit_name, &pp);
		fprintf(fp, "# possible first %s commit: [%s] %s\n",
			terms->term_bad, oid_to_hex(&commit->object.oid),
			commit_name.buf);
	}

	/*
	 * Reset the flags used by revision walks in case
	 * there is another revision walk after this one.
	 */
	reset_revision_walk();

	strbuf_release(&commit_name);
	fclose(fp);
	return 0;
}

static int bisect_successful(struct bisect_terms *terms)
{
	struct object_id oid;
	struct commit *commit;
	struct pretty_print_context pp = {0};
	struct strbuf commit_name = STRBUF_INIT;
	char *bad_ref = xstrfmt("refs/bisect/%s",terms->term_bad);
	int res;

	read_ref(bad_ref, &oid);
	commit = lookup_commit_reference_by_name(bad_ref);
	format_commit_message(commit, "%s", &commit_name, &pp);

	res = append_to_file(git_path_bisect_log(), "# first %s commit: [%s] %s\n",
			    terms->term_bad, oid_to_hex(&commit->object.oid),
			    commit_name.buf);

	strbuf_release(&commit_name);
	free(bad_ref);
	return res;
}

static enum bisect_error bisect_next(struct bisect_terms *terms, const char *prefix)
{
	enum bisect_error res;

	if (bisect_autostart(terms))
		return BISECT_FAILED;

	if (bisect_next_check(terms, terms->term_good))
		return BISECT_FAILED;

	/* Perform all bisection computation */
	res = bisect_next_all(the_repository, prefix);

	if (res == BISECT_INTERNAL_SUCCESS_1ST_BAD_FOUND) {
		res = bisect_successful(terms);
		return res ? res : BISECT_INTERNAL_SUCCESS_1ST_BAD_FOUND;
	} else if (res == BISECT_ONLY_SKIPPED_LEFT) {
		res = bisect_skipped_commits(terms);
		return res ? res : BISECT_ONLY_SKIPPED_LEFT;
	}
	return res;
}

static enum bisect_error bisect_auto_next(struct bisect_terms *terms, const char *prefix)
{
	if (bisect_next_check(terms, NULL))
		return BISECT_OK;

	return bisect_next(terms, prefix);
}

static enum bisect_error bisect_start(struct bisect_terms *terms, const char **argv, int argc)
{
	int no_checkout = 0;
	int first_parent_only = 0;
	int i, has_double_dash = 0, must_write_terms = 0, bad_seen = 0;
	int flags, pathspec_pos;
	enum bisect_error res = BISECT_OK;
	struct string_list revs = STRING_LIST_INIT_DUP;
	struct string_list states = STRING_LIST_INIT_DUP;
	struct strbuf start_head = STRBUF_INIT;
	struct strbuf bisect_names = STRBUF_INIT;
	struct object_id head_oid;
	struct object_id oid;
	const char *head;

	if (is_bare_repository())
		no_checkout = 1;

	/*
	 * Check for one bad and then some good revisions
	 */
	for (i = 0; i < argc; i++) {
		if (!strcmp(argv[i], "--")) {
			has_double_dash = 1;
			break;
		}
	}

	for (i = 0; i < argc; i++) {
		const char *arg = argv[i];
		if (!strcmp(argv[i], "--")) {
			break;
		} else if (!strcmp(arg, "--no-checkout")) {
			no_checkout = 1;
		} else if (!strcmp(arg, "--first-parent")) {
			first_parent_only = 1;
		} else if (!strcmp(arg, "--term-good") ||
			 !strcmp(arg, "--term-old")) {
			i++;
			if (argc <= i)
				return error(_("'' is not a valid term"));
			must_write_terms = 1;
			free((void *) terms->term_good);
			terms->term_good = xstrdup(argv[i]);
		} else if (skip_prefix(arg, "--term-good=", &arg) ||
			   skip_prefix(arg, "--term-old=", &arg)) {
			must_write_terms = 1;
			free((void *) terms->term_good);
			terms->term_good = xstrdup(arg);
		} else if (!strcmp(arg, "--term-bad") ||
			 !strcmp(arg, "--term-new")) {
			i++;
			if (argc <= i)
				return error(_("'' is not a valid term"));
			must_write_terms = 1;
			free((void *) terms->term_bad);
			terms->term_bad = xstrdup(argv[i]);
		} else if (skip_prefix(arg, "--term-bad=", &arg) ||
			   skip_prefix(arg, "--term-new=", &arg)) {
			must_write_terms = 1;
			free((void *) terms->term_bad);
			terms->term_bad = xstrdup(arg);
		} else if (starts_with(arg, "--")) {
			return error(_("unrecognized option: '%s'"), arg);
		} else if (!get_oidf(&oid, "%s^{commit}", arg)) {
			string_list_append(&revs, oid_to_hex(&oid));
		} else if (has_double_dash) {
			die(_("'%s' does not appear to be a valid "
			      "revision"), arg);
		} else {
			break;
		}
	}
	pathspec_pos = i;

	/*
	 * The user ran "git bisect start <sha1> <sha1>", hence did not
	 * explicitly specify the terms, but we are already starting to
	 * set references named with the default terms, and won't be able
	 * to change afterwards.
	 */
	if (revs.nr)
		must_write_terms = 1;
	for (i = 0; i < revs.nr; i++) {
		if (bad_seen) {
			string_list_append(&states, terms->term_good);
		} else {
			bad_seen = 1;
			string_list_append(&states, terms->term_bad);
		}
	}

	/*
	 * Verify HEAD
	 */
	head = resolve_ref_unsafe("HEAD", 0, &head_oid, &flags);
	if (!head)
		if (get_oid("HEAD", &head_oid))
			return error(_("bad HEAD - I need a HEAD"));

	/*
	 * Check if we are bisecting
	 */
	if (!is_empty_or_missing_file(git_path_bisect_start())) {
		/* Reset to the rev from where we started */
		strbuf_read_file(&start_head, git_path_bisect_start(), 0);
		strbuf_trim(&start_head);
		if (!no_checkout) {
			struct strvec argv = STRVEC_INIT;

			strvec_pushl(&argv, "checkout", start_head.buf,
				     "--", NULL);
			if (run_command_v_opt(argv.v, RUN_GIT_CMD)) {
				res = error(_("checking out '%s' failed."
						 " Try 'git bisect start "
						 "<valid-branch>'."),
					       start_head.buf);
				goto finish;
			}
		}
	} else {
		/* Get the rev from where we start. */
		if (!get_oid(head, &head_oid) &&
		    !starts_with(head, "refs/heads/")) {
			strbuf_reset(&start_head);
			strbuf_addstr(&start_head, oid_to_hex(&head_oid));
		} else if (!get_oid(head, &head_oid) &&
			   skip_prefix(head, "refs/heads/", &head)) {
			/*
			 * This error message should only be triggered by
			 * cogito usage, and cogito users should understand
			 * it relates to cg-seek.
			 */
			if (!is_empty_or_missing_file(git_path_head_name()))
				return error(_("won't bisect on cg-seek'ed tree"));
			strbuf_addstr(&start_head, head);
		} else {
			return error(_("bad HEAD - strange symbolic ref"));
		}
	}

	/*
	 * Get rid of any old bisect state.
	 */
	if (bisect_clean_state())
		return BISECT_FAILED;

	/*
	 * Write new start state
	 */
	write_file(git_path_bisect_start(), "%s\n", start_head.buf);

	if (first_parent_only)
		write_file(git_path_bisect_first_parent(), "\n");

	if (no_checkout) {
		if (get_oid(start_head.buf, &oid) < 0) {
			res = error(_("invalid ref: '%s'"), start_head.buf);
			goto finish;
		}
		if (update_ref(NULL, "BISECT_HEAD", &oid, NULL, 0,
			       UPDATE_REFS_MSG_ON_ERR)) {
			res = BISECT_FAILED;
			goto finish;
		}
	}

	if (pathspec_pos < argc - 1)
		sq_quote_argv(&bisect_names, argv + pathspec_pos);
	write_file(git_path_bisect_names(), "%s\n", bisect_names.buf);

	for (i = 0; i < states.nr; i++)
		if (bisect_write(states.items[i].string,
				 revs.items[i].string, terms, 1)) {
			res = BISECT_FAILED;
			goto finish;
		}

	if (must_write_terms && write_terms(terms->term_bad,
					    terms->term_good)) {
		res = BISECT_FAILED;
		goto finish;
	}

	res = bisect_append_log_quoted(argv);
	if (res)
		res = BISECT_FAILED;

finish:
	string_list_clear(&revs, 0);
	string_list_clear(&states, 0);
	strbuf_release(&start_head);
	strbuf_release(&bisect_names);
	if (res)
		return res;

	res = bisect_auto_next(terms, NULL);
	if (!is_bisect_success(res))
		bisect_clean_state();
	return res;
}

static inline int file_is_not_empty(const char *path)
{
	return !is_empty_or_missing_file(path);
}

static int bisect_autostart(struct bisect_terms *terms)
{
	int res;
	const char *yesno;

	if (file_is_not_empty(git_path_bisect_start()))
		return 0;

	fprintf_ln(stderr, _("You need to start by \"git bisect "
			  "start\"\n"));

	if (!isatty(STDIN_FILENO))
		return -1;

	/*
	 * TRANSLATORS: Make sure to include [Y] and [n] in your
	 * translation. The program will only accept English input
	 * at this point.
	 */
	yesno = git_prompt(_("Do you want me to do it for you "
			     "[Y/n]? "), PROMPT_ECHO);
	res = tolower(*yesno) == 'n' ?
		-1 : bisect_start(terms, empty_strvec, 0);

	return res;
}

static enum bisect_error bisect_state(struct bisect_terms *terms, const char **argv,
				      int argc)
{
	const char *state;
	int i, verify_expected = 1;
	struct object_id oid, expected;
	struct strbuf buf = STRBUF_INIT;
	struct oid_array revs = OID_ARRAY_INIT;

	if (!argc)
		return error(_("Please call `--bisect-state` with at least one argument"));

	if (bisect_autostart(terms))
		return BISECT_FAILED;

	state = argv[0];
	if (check_and_set_terms(terms, state) ||
	    !one_of(state, terms->term_good, terms->term_bad, "skip", NULL))
		return BISECT_FAILED;

	argv++;
	argc--;
	if (argc > 1 && !strcmp(state, terms->term_bad))
		return error(_("'git bisect %s' can take only one argument."), terms->term_bad);

	if (argc == 0) {
		const char *head = "BISECT_HEAD";
		enum get_oid_result res_head = get_oid(head, &oid);

		if (res_head == MISSING_OBJECT) {
			head = "HEAD";
			res_head = get_oid(head, &oid);
		}

		if (res_head)
			error(_("Bad rev input: %s"), head);
		oid_array_append(&revs, &oid);
	}

	/*
	 * All input revs must be checked before executing bisect_write()
	 * to discard junk revs.
	 */

	for (; argc; argc--, argv++) {
		if (get_oid(*argv, &oid)){
			error(_("Bad rev input: %s"), *argv);
			oid_array_clear(&revs);
			return BISECT_FAILED;
		}
		oid_array_append(&revs, &oid);
	}

	if (strbuf_read_file(&buf, git_path_bisect_expected_rev(), 0) < the_hash_algo->hexsz ||
	    get_oid_hex(buf.buf, &expected) < 0)
		verify_expected = 0; /* Ignore invalid file contents */
	strbuf_release(&buf);

	for (i = 0; i < revs.nr; i++) {
		if (bisect_write(state, oid_to_hex(&revs.oid[i]), terms, 0)) {
			oid_array_clear(&revs);
			return BISECT_FAILED;
		}
		if (verify_expected && !oideq(&revs.oid[i], &expected)) {
			unlink_or_warn(git_path_bisect_ancestors_ok());
			unlink_or_warn(git_path_bisect_expected_rev());
			verify_expected = 0;
		}
	}

	oid_array_clear(&revs);
	return bisect_auto_next(terms, NULL);
}

int cmd_bisect__helper(int argc, const char **argv, const char *prefix)
{
	enum {
		BISECT_RESET = 1,
		BISECT_WRITE,
		CHECK_AND_SET_TERMS,
		BISECT_NEXT_CHECK,
		BISECT_TERMS,
		BISECT_START,
		BISECT_AUTOSTART,
		BISECT_NEXT,
		BISECT_AUTO_NEXT,
		BISECT_STATE
	} cmdmode = 0;
	int res = 0, nolog = 0;
	struct option options[] = {
		OPT_CMDMODE(0, "bisect-reset", &cmdmode,
			 N_("reset the bisection state"), BISECT_RESET),
		OPT_CMDMODE(0, "bisect-write", &cmdmode,
			 N_("write out the bisection state in BISECT_LOG"), BISECT_WRITE),
		OPT_CMDMODE(0, "check-and-set-terms", &cmdmode,
			 N_("check and set terms in a bisection state"), CHECK_AND_SET_TERMS),
		OPT_CMDMODE(0, "bisect-next-check", &cmdmode,
			 N_("check whether bad or good terms exist"), BISECT_NEXT_CHECK),
		OPT_CMDMODE(0, "bisect-terms", &cmdmode,
			 N_("print out the bisect terms"), BISECT_TERMS),
		OPT_CMDMODE(0, "bisect-start", &cmdmode,
			 N_("start the bisect session"), BISECT_START),
		OPT_CMDMODE(0, "bisect-next", &cmdmode,
			 N_("find the next bisection commit"), BISECT_NEXT),
		OPT_CMDMODE(0, "bisect-auto-next", &cmdmode,
			 N_("verify the next bisection state then checkout the next bisection commit"), BISECT_AUTO_NEXT),
		OPT_CMDMODE(0, "bisect-state", &cmdmode,
			 N_("mark the state of ref (or refs)"), BISECT_STATE),
		OPT_BOOL(0, "no-log", &nolog,
			 N_("no log for BISECT_WRITE")),
		OPT_END()
	};
	struct bisect_terms terms = { .term_good = NULL, .term_bad = NULL };

	argc = parse_options(argc, argv, prefix, options,
			     git_bisect_helper_usage,
			     PARSE_OPT_KEEP_DASHDASH | PARSE_OPT_KEEP_UNKNOWN);

	if (!cmdmode)
		usage_with_options(git_bisect_helper_usage, options);

	switch (cmdmode) {
	case BISECT_RESET:
		if (argc > 1)
			return error(_("--bisect-reset requires either no argument or a commit"));
		return !!bisect_reset(argc ? argv[0] : NULL);
	case BISECT_WRITE:
		if (argc != 4 && argc != 5)
			return error(_("--bisect-write requires either 4 or 5 arguments"));
		set_terms(&terms, argv[3], argv[2]);
		res = bisect_write(argv[0], argv[1], &terms, nolog);
		break;
	case CHECK_AND_SET_TERMS:
		if (argc != 3)
			return error(_("--check-and-set-terms requires 3 arguments"));
		set_terms(&terms, argv[2], argv[1]);
		res = check_and_set_terms(&terms, argv[0]);
		break;
	case BISECT_NEXT_CHECK:
		if (argc != 2 && argc != 3)
			return error(_("--bisect-next-check requires 2 or 3 arguments"));
		set_terms(&terms, argv[1], argv[0]);
		res = bisect_next_check(&terms, argc == 3 ? argv[2] : NULL);
		break;
	case BISECT_TERMS:
		if (argc > 1)
			return error(_("--bisect-terms requires 0 or 1 argument"));
		res = bisect_terms(&terms, argc == 1 ? argv[0] : NULL);
		break;
	case BISECT_START:
		set_terms(&terms, "bad", "good");
		res = bisect_start(&terms, argv, argc);
		break;
	case BISECT_NEXT:
		if (argc)
			return error(_("--bisect-next requires 0 arguments"));
		get_terms(&terms);
		res = bisect_next(&terms, prefix);
		break;
	case BISECT_AUTO_NEXT:
		if (argc)
			return error(_("--bisect-auto-next requires 0 arguments"));
		get_terms(&terms);
		res = bisect_auto_next(&terms, prefix);
		break;
	case BISECT_STATE:
		set_terms(&terms, "bad", "good");
		get_terms(&terms);
		res = bisect_state(&terms, argv, argc);
		break;
	default:
		BUG("unknown subcommand %d", cmdmode);
	}
	free_terms(&terms);

	/*
	 * Handle early success
	 * From check_merge_bases > check_good_are_ancestors_of_bad > bisect_next_all
	 */
	if ((res == BISECT_INTERNAL_SUCCESS_MERGE_BASE) || (res == BISECT_INTERNAL_SUCCESS_1ST_BAD_FOUND))
		res = BISECT_OK;

	return -res;
}
