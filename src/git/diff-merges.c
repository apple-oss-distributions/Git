#include "diff-merges.h"

#include "revision.h"

typedef void (*diff_merges_setup_func_t)(struct rev_info *);
static void set_separate(struct rev_info *revs);

static diff_merges_setup_func_t set_to_default = set_separate;

static void suppress(struct rev_info *revs)
{
	revs->separate_merges = 0;
	revs->first_parent_merges = 0;
	revs->combine_merges = 0;
	revs->dense_combined_merges = 0;
	revs->combined_all_paths = 0;
	revs->combined_imply_patch = 0;
	revs->merges_need_diff = 0;
}

static void set_separate(struct rev_info *revs)
{
	suppress(revs);
	revs->separate_merges = 1;
}

static void set_first_parent(struct rev_info *revs)
{
	set_separate(revs);
	revs->first_parent_merges = 1;
}

static void set_m(struct rev_info *revs)
{
	/*
	 * To "diff-index", "-m" means "match missing", and to the "log"
	 * family of commands, it means "show default diff for merges". Set
	 * both fields appropriately.
	 */
	set_to_default(revs);
	revs->match_missing = 1;
}

static void set_combined(struct rev_info *revs)
{
	suppress(revs);
	revs->combine_merges = 1;
	revs->dense_combined_merges = 0;
}

static void set_dense_combined(struct rev_info *revs)
{
	suppress(revs);
	revs->combine_merges = 1;
	revs->dense_combined_merges = 1;
}

static diff_merges_setup_func_t func_by_opt(const char *optarg)
{
	if (!strcmp(optarg, "off") || !strcmp(optarg, "none"))
		return suppress;
	if (!strcmp(optarg, "1") || !strcmp(optarg, "first-parent"))
		return set_first_parent;
	else if (!strcmp(optarg, "separate"))
		return set_separate;
	else if (!strcmp(optarg, "c") || !strcmp(optarg, "combined"))
		return set_combined;
	else if (!strcmp(optarg, "cc") || !strcmp(optarg, "dense-combined"))
		return set_dense_combined;
	else if (!strcmp(optarg, "m") || !strcmp(optarg, "on"))
		return set_to_default;
	return NULL;
}

static void set_diff_merges(struct rev_info *revs, const char *optarg)
{
	diff_merges_setup_func_t func = func_by_opt(optarg);

	if (!func)
		die(_("unknown value for --diff-merges: %s"), optarg);

	func(revs);

	/* NOTE: the merges_need_diff flag is cleared by func() call */
	if (func != suppress)
		revs->merges_need_diff = 1;
}

/*
 * Public functions. They are in the order they are called.
 */

int diff_merges_config(const char *value)
{
	diff_merges_setup_func_t func = func_by_opt(value);

	if (!func)
		return -1;

	set_to_default = func;
	return 0;
}

int diff_merges_parse_opts(struct rev_info *revs, const char **argv)
{
	int argcount = 1;
	const char *optarg;
	const char *arg = argv[0];

	if (!strcmp(arg, "-m")) {
		set_m(revs);
	} else if (!strcmp(arg, "-c")) {
		set_combined(revs);
		revs->combined_imply_patch = 1;
	} else if (!strcmp(arg, "--cc")) {
		set_dense_combined(revs);
		revs->combined_imply_patch = 1;
	} else if (!strcmp(arg, "--no-diff-merges")) {
		suppress(revs);
	} else if (!strcmp(arg, "--combined-all-paths")) {
		revs->combined_all_paths = 1;
	} else if ((argcount = parse_long_opt("diff-merges", argv, &optarg))) {
		set_diff_merges(revs, optarg);
	} else
		return 0;

	revs->explicit_diff_merges = 1;
	return argcount;
}

void diff_merges_suppress(struct rev_info *revs)
{
	suppress(revs);
}

void diff_merges_default_to_first_parent(struct rev_info *revs)
{
	if (!revs->explicit_diff_merges)
		revs->separate_merges = 1;
	if (revs->separate_merges)
		revs->first_parent_merges = 1;
}

void diff_merges_default_to_dense_combined(struct rev_info *revs)
{
	if (!revs->explicit_diff_merges)
		set_dense_combined(revs);
}

void diff_merges_set_dense_combined_if_unset(struct rev_info *revs)
{
	if (!revs->combine_merges)
		set_dense_combined(revs);
}

void diff_merges_setup_revs(struct rev_info *revs)
{
	if (revs->combine_merges == 0)
		revs->dense_combined_merges = 0;
	if (revs->separate_merges == 0)
		revs->first_parent_merges = 0;
	if (revs->combined_all_paths && !revs->combine_merges)
		die("--combined-all-paths makes no sense without -c or --cc");
	if (revs->combined_imply_patch)
		revs->diff = 1;
	if (revs->combined_imply_patch || revs->merges_need_diff) {
		if (!revs->diffopt.output_format)
			revs->diffopt.output_format = DIFF_FORMAT_PATCH;
	}
}
