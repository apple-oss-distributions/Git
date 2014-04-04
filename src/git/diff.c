#include "string-list.h"
static int diff_rename_limit_default = 400;
static int diff_use_color_default = -1;
static int diff_context_default = 3;
static int diff_stat_graph_width;
static int diff_dirstat_permille_default = 30;
static long diff_algorithm;
static int parse_dirstat_params(struct diff_options *options, const char *params_string,
				struct strbuf *errmsg)
{
	char *params_copy = xstrdup(params_string);
	struct string_list params = STRING_LIST_INIT_NODUP;
	int ret = 0;
	int i;

	if (*params_copy)
		string_list_split_in_place(&params, params_copy, ',', -1);
	for (i = 0; i < params.nr; i++) {
		const char *p = params.items[i].string;
		if (!strcmp(p, "changes")) {
			DIFF_OPT_CLR(options, DIRSTAT_BY_LINE);
			DIFF_OPT_CLR(options, DIRSTAT_BY_FILE);
		} else if (!strcmp(p, "lines")) {
			DIFF_OPT_SET(options, DIRSTAT_BY_LINE);
			DIFF_OPT_CLR(options, DIRSTAT_BY_FILE);
		} else if (!strcmp(p, "files")) {
			DIFF_OPT_CLR(options, DIRSTAT_BY_LINE);
			DIFF_OPT_SET(options, DIRSTAT_BY_FILE);
		} else if (!strcmp(p, "noncumulative")) {
			DIFF_OPT_CLR(options, DIRSTAT_CUMULATIVE);
		} else if (!strcmp(p, "cumulative")) {
			DIFF_OPT_SET(options, DIRSTAT_CUMULATIVE);
		} else if (isdigit(*p)) {
			char *end;
			int permille = strtoul(p, &end, 10) * 10;
			if (*end == '.' && isdigit(*++end)) {
				/* only use first digit */
				permille += *end - '0';
				/* .. and ignore any further digits */
				while (isdigit(*++end))
					; /* nothing */
			}
			if (!*end)
				options->dirstat_permille = permille;
			else {
				strbuf_addf(errmsg, _("  Failed to parse dirstat cut-off percentage '%s'\n"),
					    p);
				ret++;
			}
		} else {
			strbuf_addf(errmsg, _("  Unknown dirstat parameter '%s'\n"), p);
			ret++;
		}

	}
	string_list_clear(&params, 0);
	free(params_copy);
	return ret;
}

static int parse_submodule_params(struct diff_options *options, const char *value)
{
	if (!strcmp(value, "log"))
		DIFF_OPT_SET(options, SUBMODULE_LOG);
	else if (!strcmp(value, "short"))
		DIFF_OPT_CLR(options, SUBMODULE_LOG);
	else
		return -1;
	return 0;
}

long parse_algorithm_value(const char *value)
{
	if (!value)
		return -1;
	else if (!strcasecmp(value, "myers") || !strcasecmp(value, "default"))
		return 0;
	else if (!strcasecmp(value, "minimal"))
		return XDF_NEED_MINIMAL;
	else if (!strcasecmp(value, "patience"))
		return XDF_PATIENCE_DIFF;
	else if (!strcasecmp(value, "histogram"))
		return XDF_HISTOGRAM_DIFF;
	return -1;
}

		diff_use_color_default = git_config_colorbool(var, value);
		return 0;
	}
	if (!strcmp(var, "diff.context")) {
		diff_context_default = git_config_int(var, value);
		if (diff_context_default < 0)
			return -1;
	if (!strcmp(var, "diff.statgraphwidth")) {
		diff_stat_graph_width = git_config_int(var, value);
		return 0;
	}
	if (!strcmp(var, "diff.submodule")) {
		if (parse_submodule_params(&default_diff_options, value))
			warning(_("Unknown value for 'diff.submodule' config variable: '%s'"),
				value);
		return 0;
	}

	if (!strcmp(var, "diff.algorithm")) {
		diff_algorithm = parse_algorithm_value(value);
		if (diff_algorithm < 0)
			return -1;
		return 0;
	}

	if (git_color_config(var, value, cb) < 0)
		return -1;

	if (userdiff_config(var, value) < 0)
		return -1;
	if (!strcmp(var, "diff.dirstat")) {
		struct strbuf errmsg = STRBUF_INIT;
		default_diff_options.dirstat_permille = diff_dirstat_permille_default;
		if (parse_dirstat_params(&default_diff_options, value, &errmsg))
			warning(_("Found errors in 'diff.dirstat' config variable:\n%s"),
				errmsg.buf);
		strbuf_release(&errmsg);
		diff_dirstat_permille_default = default_diff_options.dirstat_permille;
		return 0;
	}

	return git_default_config(var, value, cb);
	fputs(diff_line_prefix(o), file);
		putc('\n', ecb->opt->file);
	const char *metainfo = diff_get_color(o->use_color, DIFF_METAINFO);
	const char *fraginfo = diff_get_color(o->use_color, DIFF_FRAGINFO);
	const char *reset = diff_get_color(o->use_color, DIFF_RESET);
	const char *line_prefix = diff_line_prefix(o);
	ecbdata.color_diff = want_color(o->use_color);
	if (!o->irreversible_delete)
		print_line_count(o->file, lc_a);
	else
		fprintf(o->file, "?,?");
	if (lc_a && !o->irreversible_delete)
static struct diff_words_style diff_words_styles[] = {
	const char *line_prefix;
	line_prefix = diff_line_prefix(opt);
	const char *line_prefix;
	line_prefix = diff_line_prefix(opt);
static void diff_filespec_load_driver(struct diff_filespec *one)
{
	/* Use already-loaded driver */
	if (one->driver)
		return;

	if (S_ISREG(one->mode))
		one->driver = userdiff_find_by_path(one->path);

	/* Fallback to default settings */
	if (!one->driver)
		one->driver = userdiff_find_by_name("default");
}

static const char *userdiff_word_regex(struct diff_filespec *one)
{
	diff_filespec_load_driver(one);
	return one->driver->word_regex;
}

static void init_diff_words_data(struct emit_callback *ecbdata,
				 struct diff_options *orig_opts,
				 struct diff_filespec *one,
				 struct diff_filespec *two)
{
	int i;
	struct diff_options *o = xmalloc(sizeof(struct diff_options));
	memcpy(o, orig_opts, sizeof(struct diff_options));

	ecbdata->diff_words =
		xcalloc(1, sizeof(struct diff_words_data));
	ecbdata->diff_words->type = o->word_diff;
	ecbdata->diff_words->opt = o;
	if (!o->word_regex)
		o->word_regex = userdiff_word_regex(one);
	if (!o->word_regex)
		o->word_regex = userdiff_word_regex(two);
	if (!o->word_regex)
		o->word_regex = diff_word_regex_cfg;
	if (o->word_regex) {
		ecbdata->diff_words->word_regex = (regex_t *)
			xmalloc(sizeof(regex_t));
		if (regcomp(ecbdata->diff_words->word_regex,
			    o->word_regex,
			    REG_EXTENDED | REG_NEWLINE))
			die ("Invalid regular expression: %s",
			     o->word_regex);
	}
	for (i = 0; i < ARRAY_SIZE(diff_words_styles); i++) {
		if (o->word_diff == diff_words_styles[i].type) {
			ecbdata->diff_words->style =
				&diff_words_styles[i];
			break;
		}
	}
	if (want_color(o->use_color)) {
		struct diff_words_style *st = ecbdata->diff_words->style;
		st->old.color = diff_get_color_opt(o, DIFF_FILE_OLD);
		st->new.color = diff_get_color_opt(o, DIFF_FILE_NEW);
		st->ctx.color = diff_get_color_opt(o, DIFF_PLAIN);
	}
}

		free (ecbdata->diff_words->opt);
	if (want_color(diff_use_color))
const char *diff_line_prefix(struct diff_options *opt)
{
	struct strbuf *msgbuf;
	if (!opt->output_prefix)
		return "";

	msgbuf = opt->output_prefix(opt, opt->output_prefix_data);
	return msgbuf->buf;
}

	const char *line_prefix = diff_line_prefix(o);
		} else if (!prefixcmp(line, "\\ ")) {
			/*
			 * Eat the "no newline at eof" marker as if we
			 * saw a "+" or "-" line with nothing on it,
			 * and return without diff_words_flush() to
			 * defer processing. If this is the end of
			 * preimage, more "+" lines may come after it.
			 */
			return;
			/*
			 * Skip the prefix character, if any.  With
			 * diff_suppress_blank_empty, there may be
			 * none.
			 */
			if (line[0] != '\n') {
			      line++;
			      len--;
			}
			emit_line(ecbdata->opt, plain, reset, line, len);
	int pfx_adjust_for_slash;
	/*
	 * If there is a common prefix, it must end in a slash.  In
	 * that case we let this loop run 1 into the prefix to see the
	 * same slash.
	 *
	 * If there is no common prefix, we cannot do this as it would
	 * underrun the input strings.
	 */
	pfx_adjust_for_slash = (pfx_length ? 1 : 0);
	while (a + pfx_length - pfx_adjust_for_slash <= old &&
	       b + pfx_length - pfx_adjust_for_slash <= new &&
	       *old == *new) {
		unsigned is_interesting:1;
	if (!it)
		return 0;
	 * make sure that at least one '-' or '+' is printed if
	 * there is any change to this path. The easiest way is to
	 * scale linearly as if the alloted width is one column shorter
	 * than it is, and then add 1 to the result.
	return 1 + (it * (width - 1) / max_change);
int print_stat_summary(FILE *fp, int files, int insertions, int deletions)
{
	struct strbuf sb = STRBUF_INIT;
	int ret;

	if (!files) {
		assert(insertions == 0 && deletions == 0);
		return fprintf(fp, "%s\n", " 0 files changed");
	}

	strbuf_addf(&sb,
		    (files == 1) ? " %d file changed" : " %d files changed",
		    files);

	/*
	 * For binary diff, the caller may want to print "x files
	 * changed" with insertions == 0 && deletions == 0.
	 *
	 * Not omitting "0 insertions(+), 0 deletions(-)" in this case
	 * is probably less confusing (i.e skip over "2 files changed
	 * but nothing about added/removed lines? Is this a bug in Git?").
	 */
	if (insertions || deletions == 0) {
		/*
		 * TRANSLATORS: "+" in (+) is a line addition marker;
		 * do not translate it.
		 */
		strbuf_addf(&sb,
			    (insertions == 1) ? ", %d insertion(+)" : ", %d insertions(+)",
			    insertions);
	}

	if (deletions || insertions == 0) {
		/*
		 * TRANSLATORS: "-" in (-) is a line removal marker;
		 * do not translate it.
		 */
		strbuf_addf(&sb,
			    (deletions == 1) ? ", %d deletion(-)" : ", %d deletions(-)",
			    deletions);
	}
	strbuf_addch(&sb, '\n');
	ret = fputs(sb.buf, fp);
	strbuf_release(&sb);
	return ret;
}

	int total_files = data->nr, count;
	int width, name_width, graph_width, number_width = 0, bin_width = 0;
	const char *reset, *add_c, *del_c;
	int extra_shown = 0;
	line_prefix = diff_line_prefix(options);
	count = options->stat_count ? options->stat_count : data->nr;
	/*
	 * Find the longest filename and max number of changes
	 */
	for (i = 0; (i < count) && (i < data->nr); i++) {

		if (!file->is_interesting && (change == 0)) {
			count++; /* not shown == room for one more */
			continue;
		}
		if (file->is_unmerged) {
			/* "Unmerged" is 8 characters */
			bin_width = bin_width < 8 ? 8 : bin_width;
			continue;
		}
		if (file->is_binary) {
			/* "Bin XXX -> YYY bytes" */
			int w = 14 + decimal_width(file->added)
				+ decimal_width(file->deleted);
			bin_width = bin_width < w ? w : bin_width;
			/* Display change counts aligned with "Bin" */
			number_width = 3;
		}

	count = i; /* where we can stop scanning in data->files[] */
	/*
	 * We have width = stat_width or term_columns() columns total.
	 * We want a maximum of min(max_len, stat_name_width) for the name part.
	 * We want a maximum of min(max_change, stat_graph_width) for the +- part.
	 * We also need 1 for " " and 4 + decimal_width(max_change)
	 * for " | NNNN " and one the empty column at the end, altogether
	 * 6 + decimal_width(max_change).
	 *
	 * If there's not enough space, we will use the smaller of
	 * stat_name_width (if set) and 5/8*width for the filename,
	 * and the rest for constant elements + graph part, but no more
	 * than stat_graph_width for the graph part.
	 * (5/8 gives 50 for filename and 30 for the constant parts + graph
	 * for the standard terminal size).
	 *
	 * In other words: stat_width limits the maximum width, and
	 * stat_name_width fixes the maximum width of the filename,
	 * and is also used to divide available columns if there
	 * aren't enough.
	 * Binary files are displayed with "Bin XXX -> YYY bytes"
	 * instead of the change count and graph. This part is treated
	 * similarly to the graph part, except that it is not
	 * "scaled". If total width is too small to accommodate the
	 * guaranteed minimum width of the filename part and the
	 * separators and this message, this message will "overflow"
	 * making the line longer than the maximum width.

	if (options->stat_width == -1)
		width = term_columns() - options->output_prefix_length;
		width = options->stat_width ? options->stat_width : 80;
	number_width = decimal_width(max_change) > number_width ?
		decimal_width(max_change) : number_width;
	if (options->stat_graph_width == -1)
		options->stat_graph_width = diff_stat_graph_width;

	/*
	 * Guarantee 3/8*16==6 for the graph part
	 * and 5/8*16==10 for the filename part
	 */
	if (width < 16 + 6 + number_width)
		width = 16 + 6 + number_width;

	/*
	 * First assign sizes that are wanted, ignoring available width.
	 * strlen("Bin XXX -> YYY bytes") == bin_width, and the part
	 * starting from "XXX" should fit in graph_width.
	 */
	graph_width = max_change + 4 > bin_width ? max_change : bin_width - 4;
	if (options->stat_graph_width &&
	    options->stat_graph_width < graph_width)
		graph_width = options->stat_graph_width;

	name_width = (options->stat_name_width > 0 &&
		      options->stat_name_width < max_len) ?
		options->stat_name_width : max_len;

	/*
	 * Adjust adjustable widths not to exceed maximum width
	 */
	if (name_width + number_width + 6 + graph_width > width) {
		if (graph_width > width * 3/8 - number_width - 6) {
			graph_width = width * 3/8 - number_width - 6;
			if (graph_width < 6)
				graph_width = 6;
		}

		if (options->stat_graph_width &&
		    graph_width > options->stat_graph_width)
			graph_width = options->stat_graph_width;
		if (name_width > width - number_width - 6 - graph_width)
			name_width = width - number_width - 6 - graph_width;
		else
			graph_width = width - number_width - 6 - name_width;
	}

	/*
	 * From here name_width is the width of the name area,
	 * and graph_width is the width of the graph area.
	 * max_change is used to scale graph properly.
	 */
	for (i = 0; i < count; i++) {
		struct diffstat_file *file = data->files[i];
		char *name = file->print_name;
		uintmax_t added = file->added;
		uintmax_t deleted = file->deleted;
		if (!file->is_interesting && (added + deleted == 0))
			continue;

		if (file->is_binary) {
			fprintf(options->file, " %*s", number_width, "Bin");
			if (!added && !deleted) {
				putc('\n', options->file);
				continue;
			}
			fprintf(options->file, " %s%"PRIuMAX"%s",
		else if (file->is_unmerged) {
			fprintf(options->file, " Unmerged\n");
		if (graph_width <= max_change) {
			int total = add + del;

			total = scale_linear(add + del, graph_width, max_change);
			if (total < 2 && add && del)
				/* width >= 2 due to the sanity check */
				total = 2;
			if (add < del) {
				add = scale_linear(add, graph_width, max_change);
				del = total - add;
			} else {
				del = scale_linear(del, graph_width, max_change);
				add = total - del;
			}
		fprintf(options->file, " %*"PRIuMAX"%s",
			number_width, added + deleted,
			added + deleted ? " " : "");

	for (i = 0; i < data->nr; i++) {
		struct diffstat_file *file = data->files[i];
		uintmax_t added = file->added;
		uintmax_t deleted = file->deleted;

		if (file->is_unmerged ||
		    (!file->is_interesting && (added + deleted == 0))) {
			total_files--;
			continue;
		}

		if (!file->is_binary) {
			adds += added;
			dels += deleted;
		}
		if (i < count)
			continue;
		if (!extra_shown)
			fprintf(options->file, "%s ...\n", line_prefix);
		extra_shown = 1;
	}
	print_stat_summary(options->file, total_files, adds, dels);
		int added = data->files[i]->added;
		int deleted= data->files[i]->deleted;

		if (data->files[i]->is_unmerged ||
		    (!data->files[i]->is_interesting && (added + deleted == 0))) {
			total_files--;
		} else if (!data->files[i]->is_binary) { /* don't count bytes */
			adds += added;
			dels += deleted;
	fprintf(options->file, "%s", diff_line_prefix(options));
	print_stat_summary(options->file, total_files, adds, dels);
		fprintf(options->file, "%s", diff_line_prefix(options));
	int alloc, nr, permille, cumulative;
	const char *line_prefix = diff_line_prefix(opt);
		if (this_dir) {
			int permille = this_dir * 1000 / changed;
			if (permille >= dir->permille) {
					permille / 10, permille % 10, baselen, base);
	dir.permille = options->dirstat_permille;
		int content_changed;
		name = p->two->path ? p->two->path : p->one->path;

		if (p->one->sha1_valid && p->two->sha1_valid)
			content_changed = hashcmp(p->one->sha1, p->two->sha1);
		else
			content_changed = 1;

		if (!content_changed) {
			/*
			 * The SHA1 has not changed, so pre-/post-content is
			 * identical. We can therefore skip looking at the
			 * file contents altogether.
			 */
			damage = 0;
			goto found_damage;
		}

		if (DIFF_OPT_TST(options, DIRSTAT_BY_FILE)) {
			/*
			 * In --dirstat-by-file mode, we don't really need to
			 * look at the actual file contents at all.
			 * The fact that the SHA1 changed is enough for us to
			 * add this file to the list of results
			 * (with each file contributing equal damage).
			 */
			damage = 1;
			goto found_damage;
		}
		 * made to the preimage.
		 * If the resulting damage is zero, we know that
		 * diffcore_count_changes() considers the two entries to
		 * be identical, but since content_changed is true, we
		 * know that there must have been _some_ kind of change,
		 * so we force all entries to have damage > 0.
		if (!damage)
found_damage:
static void show_dirstat_by_line(struct diffstat_t *data, struct diff_options *options)
{
	int i;
	unsigned long changed;
	struct dirstat_dir dir;

	if (data->nr == 0)
		return;

	dir.files = NULL;
	dir.alloc = 0;
	dir.nr = 0;
	dir.permille = options->dirstat_permille;
	dir.cumulative = DIFF_OPT_TST(options, DIRSTAT_CUMULATIVE);

	changed = 0;
	for (i = 0; i < data->nr; i++) {
		struct diffstat_file *file = data->files[i];
		unsigned long damage = file->added + file->deleted;
		if (file->is_binary)
			/*
			 * binary files counts bytes, not lines. Must find some
			 * way to normalize binary bytes vs. textual lines.
			 * The following heuristic assumes that there are 64
			 * bytes per "line".
			 * This is stupid and ugly, but very cheap...
			 */
			damage = (damage + 63) / 64;
		ALLOC_GROW(dir.files, dir.nr + 1, dir.alloc);
		dir.files[dir.nr].name = file->name;
		dir.files[dir.nr].changed = damage;
		changed += damage;
		dir.nr++;
	}

	/* This can happen even with many files, if everything was renames */
	if (!changed)
		return;

	/* Show all directories with more than x% of the changes */
	qsort(dir.files, dir.nr, sizeof(dir.files[0]), dirstat_compare);
	gather_dirstat(options, &dir, changed, "", 0);
}

	const char *ws = diff_get_color(data->o->use_color, DIFF_WHITESPACE);
	const char *reset = diff_get_color(data->o->use_color, DIFF_RESET);
	const char *set = diff_get_color(data->o->use_color, DIFF_FILE_NEW);
	const char *line_prefix;
	line_prefix = diff_line_prefix(data->o);
	git_zstream stream;
	git_deflate_init(&stream, zlib_compression_level);
	bound = git_deflate_bound(&stream, size);
	while (git_deflate(&stream, Z_FINISH) == Z_OK)
	git_deflate_end(&stream);
static void emit_binary_diff_body(FILE *file, mmfile_t *one, mmfile_t *two,
				  const char *prefix)
static void emit_binary_diff(FILE *file, mmfile_t *one, mmfile_t *two,
			     const char *prefix)
	return userdiff_get_textconv(one->driver);
	const char *meta = diff_get_color_opt(o, DIFF_METAINFO);
	const char *line_prefix = diff_line_prefix(o);
				line_prefix,
				meta, del, add, reset);
	strbuf_addf(&header, "%s%sdiff --git %s %s%s\n", line_prefix, meta, a_one, b_two, reset);
		strbuf_addf(&header, "%s%snew file mode %06o%s\n", line_prefix, meta, two->mode, reset);
		strbuf_addf(&header, "%s%sdeleted file mode %06o%s\n", line_prefix, meta, one->mode, reset);
			strbuf_addf(&header, "%s%sold mode %06o%s\n", line_prefix, meta, one->mode, reset);
			strbuf_addf(&header, "%s%snew mode %06o%s\n", line_prefix, meta, two->mode, reset);
	if (o->irreversible_delete && lbl[1][0] == '/') {
		fprintf(o->file, "%s", header.buf);
		strbuf_reset(&header);
		goto free_ab_and_return;
	} else if (!DIFF_OPT_TST(o, TEXT) &&
	} else {
		if (must_show_header) {
		ecbdata.color_diff = want_color(o->use_color);
		if (DIFF_OPT_TST(o, FUNCCONTEXT))
			xecfg.flags |= XDL_EMIT_FUNCCONTEXT;
		if (o->word_diff)
			init_diff_words_data(&ecbdata, o, one, two);
			     struct diff_filepair *p)
	int same_contents;
	int complete_rewrite = 0;

	if (!DIFF_PAIR_UNMERGED(p)) {
		if (p->status == DIFF_STATUS_MODIFIED && p->score)
			complete_rewrite = 1;
	}
	data->is_interesting = p->status != DIFF_STATUS_UNKNOWN;
	same_contents = !hashcmp(one->sha1, two->sha1);

		if (same_contents) {
			data->added = 0;
			data->deleted = 0;
		} else {
			data->added = diff_filespec_size(two);
			data->deleted = diff_filespec_size(one);
		}
	else if (!same_contents) {
		xecfg.ctxlen = o->context;
		xecfg.interhunkctxlen = o->interhunkcontext;
		   int sha1_valid, unsigned short mode)
		spec->sha1_valid = sha1_valid;
	/*
	 * demote FAIL to WARN to allow inspecting the situation
	 * instead of refusing.
	 */
	enum safe_crlf crlf_warn = (safe_crlf == SAFE_CRLF_FAIL
				    ? SAFE_CRLF_WARN
				    : safe_crlf);

		if (convert_to_git(s->path, s->data, s->size, &buf, crlf_warn)) {
	const char *line_prefix = diff_line_prefix(o);

	if (DIFF_OPT_TST(o, ALLOW_EXTERNAL)) {
			      want_color(o->use_color) && !pgm);
			if (one->is_stdin) {
	if (!DIFF_OPT_TST(o, ALLOW_EXTERNAL))
		pgm = NULL;

		builtin_diffstat(p->one->path, NULL, NULL, NULL, diffstat, o, p);
	builtin_diffstat(name, other, p->one, p->two, diffstat, o, p);
	options->dirstat_permille = diff_dirstat_permille_default;
	options->context = diff_context_default;
	DIFF_OPT_SET(options, RENAME_EMPTY);
	options->use_color = diff_use_color_default;
	options->xdl_opts |= diff_algorithm;
void diff_setup_done(struct diff_options *options)
	int graph_width = options->stat_graph_width;
	int count = options->stat_count;
		} else if (!prefixcmp(arg, "-graph-width")) {
			arg += strlen("-graph-width");
			if (*arg == '=')
				graph_width = strtoul(arg + 1, &end, 10);
			else if (!*arg && !av[1])
				die("Option '--stat-graph-width' requires a value");
			else if (!*arg) {
				graph_width = strtoul(av[1], &end, 10);
				argcount = 2;
			}
		} else if (!prefixcmp(arg, "-count")) {
			arg += strlen("-count");
			if (*arg == '=')
				count = strtoul(arg + 1, &end, 10);
			else if (!*arg && !av[1])
				die("Option '--stat-count' requires a value");
			else if (!*arg) {
				count = strtoul(av[1], &end, 10);
				argcount = 2;
			}
		if (*end == ',')
			count = strtoul(end+1, &end, 10);
	options->stat_graph_width = graph_width;
	options->stat_count = count;
static int parse_dirstat_opt(struct diff_options *options, const char *params)
{
	struct strbuf errmsg = STRBUF_INIT;
	if (parse_dirstat_params(options, params, &errmsg))
		die(_("Failed to parse --dirstat/-X option parameter:\n%s"),
		    errmsg.buf);
	strbuf_release(&errmsg);
	/*
	 * The caller knows a dirstat-related option is given from the command
	 * line; allow it to say "return this_function();"
	 */
	options->output_format |= DIFF_FORMAT_DIRSTAT;
	return 1;
}

static int parse_submodule_opt(struct diff_options *options, const char *value)
{
	if (parse_submodule_params(options, value))
		die(_("Failed to parse --submodule option parameter: '%s'"),
			value);
	return 1;
}

	else if (!strcmp(arg, "-X") || !strcmp(arg, "--dirstat"))
		return parse_dirstat_opt(options, "");
	else if (!prefixcmp(arg, "-X"))
		return parse_dirstat_opt(options, arg + 2);
	else if (!prefixcmp(arg, "--dirstat="))
		return parse_dirstat_opt(options, arg + 10);
	else if (!strcmp(arg, "--cumulative"))
		return parse_dirstat_opt(options, "cumulative");
	else if (!strcmp(arg, "--dirstat-by-file"))
		return parse_dirstat_opt(options, "files");
	else if (!prefixcmp(arg, "--dirstat-by-file=")) {
		parse_dirstat_opt(options, "files");
		return parse_dirstat_opt(options, arg + 18);
		/* --stat, --stat-width, --stat-name-width, or --stat-count */
	else if (!strcmp(arg, "-D") || !strcmp(arg, "--irreversible-delete")) {
		options->irreversible_delete = 1;
	}
	else if (!strcmp(arg, "--rename-empty"))
		DIFF_OPT_SET(options, RENAME_EMPTY);
	else if (!strcmp(arg, "--no-rename-empty"))
		DIFF_OPT_CLR(options, RENAME_EMPTY);
	else if (!strcmp(arg, "--minimal"))
		DIFF_XDL_SET(options, NEED_MINIMAL);
	else if (!strcmp(arg, "--no-minimal"))
		DIFF_XDL_CLR(options, NEED_MINIMAL);
		options->xdl_opts = DIFF_WITH_ALG(options, PATIENCE_DIFF);
	else if (!strcmp(arg, "--histogram"))
		options->xdl_opts = DIFF_WITH_ALG(options, HISTOGRAM_DIFF);
	else if ((argcount = parse_long_opt("diff-algorithm", av, &optarg))) {
		long value = parse_algorithm_value(optarg);
		if (value < 0)
			return error("option diff-algorithm accepts \"myers\", "
				     "\"minimal\", \"patience\" and \"histogram\"");
		/* clear out previous settings */
		DIFF_XDL_CLR(options, NEED_MINIMAL);
		options->xdl_opts &= ~XDF_DIFF_ALGORITHM_MASK;
		options->xdl_opts |= value;
		return argcount;
	}
	else if (!strcmp(arg, "--no-follow"))
		DIFF_OPT_CLR(options, FOLLOW_RENAMES);
		options->use_color = 1;
		int value = git_config_colorbool(NULL, arg+8);
		if (value < 0)
		options->use_color = value;
		options->use_color = 0;
		options->use_color = 1;
		options->use_color = 1;
			options->use_color = 1;
	else if (!prefixcmp(arg, "--submodule="))
		return parse_submodule_opt(options, arg + 12);
	else if (!strcmp(arg, "-W"))
		DIFF_OPT_SET(options, FUNCCONTEXT);
	else if (!strcmp(arg, "--function-context"))
		DIFF_OPT_SET(options, FUNCCONTEXT);
	else if (!strcmp(arg, "--no-function-context"))
		DIFF_OPT_CLR(options, FUNCCONTEXT);
	fprintf(opt->file, "%s", diff_line_prefix(opt));
	const char *line_prefix = diff_line_prefix(opt);
static const char rename_limit_warning[] =
"inexact rename detection was skipped due to too many files.";

static const char degrade_cc_to_c_warning[] =
"only found copies from modified paths due to too many files.";

static const char rename_limit_advice[] =
"you may want to set your %s variable to at least "
"%d and retry the command.";

void diff_warn_rename_limit(const char *varname, int needed, int degraded_cc)
{
	if (degraded_cc)
		warning(degrade_cc_to_c_warning);
	else if (needed)
		warning(rename_limit_warning);
	else
		return;
	if (0 < needed && needed < 32767)
		warning(rename_limit_advice, varname, needed);
}

	int dirstat_by_line = 0;
	if (output_format & DIFF_FORMAT_DIRSTAT && DIFF_OPT_TST(options, DIRSTAT_BY_LINE))
		dirstat_by_line = 1;

	if (output_format & (DIFF_FORMAT_DIFFSTAT|DIFF_FORMAT_SHORTSTAT|DIFF_FORMAT_NUMSTAT) ||
	    dirstat_by_line) {
		if (output_format & DIFF_FORMAT_DIRSTAT)
			show_dirstat_by_line(&diffstat, options);
	if ((output_format & DIFF_FORMAT_DIRSTAT) && !dirstat_by_line)
			fprintf(options->file, "%s%c",
				diff_line_prefix(options),
				options->line_termination);

	diff_warn_rename_limit("diff.renameLimit",
			       opt->needed_rename_limit,
			       opt->degraded_cc_to_c);
int diff_can_quit_early(struct diff_options *opt)
{
	return (DIFF_OPT_TST(opt, QUICK) &&
		!opt->filter &&
		DIFF_OPT_TST(opt, HAS_CHANGES));
}

		    int sha1_valid,
		fill_filespec(one, sha1, sha1_valid, mode);
		fill_filespec(two, sha1, sha1_valid, mode);
		 int old_sha1_valid, int new_sha1_valid,
		tmp = old_sha1_valid; old_sha1_valid = new_sha1_valid;
			new_sha1_valid = tmp;
	fill_filespec(one, old_sha1, old_sha1_valid, old_mode);
	fill_filespec(two, new_sha1, new_sha1_valid, new_mode);
struct diff_filepair *diff_unmerge(struct diff_options *options, const char *path)
	struct diff_filepair *pair;
		return NULL;
	pair = diff_queue(&diff_queued_diff, one, two);
	pair->is_unmerged = 1;
	return pair;

void setup_diff_pager(struct diff_options *opt)
{
	/*
	 * If the user asked for our exit code, then either they want --quiet
	 * or --exit-code. We should definitely not bother with a pager in the
	 * former case, as we will generate no output. Since we still properly
	 * report our exit code even when a pager is run, we _could_ run a
	 * pager with --exit-code. But since we have not done so historically,
	 * and because it is easy to find people oneline advising "git diff
	 * --exit-code" in hooks and other scripts, we do not do so.
	 */
	if (!DIFF_OPT_TST(opt, EXIT_WITH_STATUS) &&
	    check_pager_config("diff") != 0)
		setup_pager();
}