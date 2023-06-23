#include "cache.h"
#include "config.h"
#include "repository.h"
#include "midx.h"

#define UPDATE_DEFAULT_BOOL(s,v) do { if (s == -1) { s = v; } } while(0)

void prepare_repo_settings(struct repository *r)
{
	int value;
	char *strval;

	if (r->settings.initialized)
		return;

	/* Defaults */
	memset(&r->settings, -1, sizeof(r->settings));

	if (!repo_config_get_bool(r, "core.commitgraph", &value))
		r->settings.core_commit_graph = value;
	if (!repo_config_get_bool(r, "commitgraph.readchangedpaths", &value))
		r->settings.commit_graph_read_changed_paths = value;
	if (!repo_config_get_bool(r, "gc.writecommitgraph", &value))
		r->settings.gc_write_commit_graph = value;
	UPDATE_DEFAULT_BOOL(r->settings.core_commit_graph, 1);
	UPDATE_DEFAULT_BOOL(r->settings.commit_graph_read_changed_paths, 1);
	UPDATE_DEFAULT_BOOL(r->settings.gc_write_commit_graph, 1);

	if (!repo_config_get_int(r, "index.version", &value))
		r->settings.index_version = value;
	if (!repo_config_get_maybe_bool(r, "core.untrackedcache", &value)) {
		if (value == 0)
			r->settings.core_untracked_cache = UNTRACKED_CACHE_REMOVE;
		else
			r->settings.core_untracked_cache = UNTRACKED_CACHE_WRITE;
	} else if (!repo_config_get_string(r, "core.untrackedcache", &strval)) {
		if (!strcasecmp(strval, "keep"))
			r->settings.core_untracked_cache = UNTRACKED_CACHE_KEEP;

		free(strval);
	}

	if (!repo_config_get_string(r, "fetch.negotiationalgorithm", &strval)) {
		if (!strcasecmp(strval, "skipping"))
			r->settings.fetch_negotiation_algorithm = FETCH_NEGOTIATION_SKIPPING;
		else if (!strcasecmp(strval, "noop"))
			r->settings.fetch_negotiation_algorithm = FETCH_NEGOTIATION_NOOP;
		else
			r->settings.fetch_negotiation_algorithm = FETCH_NEGOTIATION_DEFAULT;
	}

	if (!repo_config_get_bool(r, "pack.usesparse", &value))
		r->settings.pack_use_sparse = value;
	UPDATE_DEFAULT_BOOL(r->settings.pack_use_sparse, 1);

	value = git_env_bool(GIT_TEST_MULTI_PACK_INDEX, 0);
	if (value || !repo_config_get_bool(r, "core.multipackindex", &value))
		r->settings.core_multi_pack_index = value;
	UPDATE_DEFAULT_BOOL(r->settings.core_multi_pack_index, 1);

	if (!repo_config_get_bool(r, "feature.manyfiles", &value) && value) {
		UPDATE_DEFAULT_BOOL(r->settings.index_version, 4);
		UPDATE_DEFAULT_BOOL(r->settings.core_untracked_cache, UNTRACKED_CACHE_WRITE);
	}

	if (!repo_config_get_bool(r, "fetch.writecommitgraph", &value))
		r->settings.fetch_write_commit_graph = value;
	UPDATE_DEFAULT_BOOL(r->settings.fetch_write_commit_graph, 0);

	if (!repo_config_get_bool(r, "feature.experimental", &value) && value)
		UPDATE_DEFAULT_BOOL(r->settings.fetch_negotiation_algorithm, FETCH_NEGOTIATION_SKIPPING);

	/* Hack for test programs like test-dump-untracked-cache */
	if (ignore_untracked_cache_config)
		r->settings.core_untracked_cache = UNTRACKED_CACHE_KEEP;
	else
		UPDATE_DEFAULT_BOOL(r->settings.core_untracked_cache, UNTRACKED_CACHE_KEEP);

	UPDATE_DEFAULT_BOOL(r->settings.fetch_negotiation_algorithm, FETCH_NEGOTIATION_DEFAULT);
}
