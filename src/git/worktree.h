#ifndef WORKTREE_H
#define WORKTREE_H

#include "cache.h"
#include "refs.h"

struct strbuf;

struct worktree {
	char *path;
	char *id;
	char *head_ref;		/* NULL if HEAD is broken or detached */
	char *lock_reason;	/* private - use worktree_lock_reason */
	struct object_id head_oid;
	int is_detached;
	int is_bare;
	int is_current;
	int lock_reason_valid; /* private */
};

/*
 * Get the worktrees.  The primary worktree will always be the first returned,
 * and linked worktrees will follow in no particular order.
 *
 * The caller is responsible for freeing the memory from the returned
 * worktrees by calling free_worktrees().
 */
struct worktree **get_worktrees(void);

/*
 * Returns 1 if linked worktrees exist, 0 otherwise.
 */
int submodule_uses_worktrees(const char *path);

/*
 * Return git dir of the worktree. Note that the path may be relative.
 * If wt is NULL, git dir of current worktree is returned.
 */
const char *get_worktree_git_dir(const struct worktree *wt);

/*
 * Search for the worktree identified unambiguously by `arg` -- typically
 * supplied by the user via the command-line -- which may be a pathname or some
 * shorthand uniquely identifying a worktree, thus making it convenient for the
 * user to specify a worktree with minimal typing. For instance, if the last
 * component (say, "foo") of a worktree's pathname is unique among worktrees
 * (say, "work/foo" and "work/bar"), it can be used to identify the worktree
 * unambiguously.
 *
 * `prefix` should be the `prefix` handed to top-level Git commands along with
 * `argc` and `argv`.
 *
 * Return the worktree identified by `arg`, or NULL if not found.
 */
struct worktree *find_worktree(struct worktree **list,
			       const char *prefix,
			       const char *arg);

/*
 * Return the worktree corresponding to `path`, or NULL if no such worktree
 * exists.
 */
struct worktree *find_worktree_by_path(struct worktree **, const char *path);

/*
 * Return true if the given worktree is the main one.
 */
int is_main_worktree(const struct worktree *wt);

/*
 * Return the reason string if the given worktree is locked or NULL
 * otherwise.
 */
const char *worktree_lock_reason(struct worktree *wt);

#define WT_VALIDATE_WORKTREE_MISSING_OK (1 << 0)

/*
 * Return zero if the worktree is in good condition. Error message is
 * returned if "errmsg" is not NULL.
 */
int validate_worktree(const struct worktree *wt,
		      struct strbuf *errmsg,
		      unsigned flags);

/*
 * Update worktrees/xxx/gitdir with the new path.
 */
void update_worktree_location(struct worktree *wt,
			      const char *path_);

typedef void (* worktree_repair_fn)(int iserr, const char *path,
				    const char *msg, void *cb_data);

/*
 * Visit each registered linked worktree and repair corruptions. For each
 * repair made or error encountered while attempting a repair, the callback
 * function, if non-NULL, is called with the path of the worktree and a
 * description of the repair or error, along with the callback user-data.
 */
void repair_worktrees(worktree_repair_fn, void *cb_data);

/*
 * Repair administrative files corresponding to the worktree at the given path.
 * The worktree's .git file pointing at the repository must be intact for the
 * repair to succeed. Useful for re-associating an orphaned worktree with the
 * repository if the worktree has been moved manually (without using "git
 * worktree move"). For each repair made or error encountered while attempting
 * a repair, the callback function, if non-NULL, is called with the path of the
 * worktree and a description of the repair or error, along with the callback
 * user-data.
 */
void repair_worktree_at_path(const char *, worktree_repair_fn, void *cb_data);

/*
 * Free up the memory for worktree(s)
 */
void free_worktrees(struct worktree **);

/*
 * Check if a per-worktree symref points to a ref in the main worktree
 * or any linked worktree, and return the worktree that holds the ref,
 * or NULL otherwise. The result may be destroyed by the next call.
 */
const struct worktree *find_shared_symref(const char *symref,
					  const char *target);

/*
 * Similar to head_ref() for all HEADs _except_ one from the current
 * worktree, which is covered by head_ref().
 */
int other_head_refs(each_ref_fn fn, void *cb_data);

int is_worktree_being_rebased(const struct worktree *wt, const char *target);
int is_worktree_being_bisected(const struct worktree *wt, const char *target);

/*
 * Similar to git_path() but can produce paths for a specified
 * worktree instead of current one
 */
const char *worktree_git_path(const struct worktree *wt,
			      const char *fmt, ...)
	__attribute__((format (printf, 2, 3)));

/*
 * Parse a worktree ref (i.e. with prefix main-worktree/ or
 * worktrees/) and return the position of the worktree's name and
 * length (or NULL and zero if it's main worktree), and ref.
 *
 * All name, name_length and ref arguments could be NULL.
 */
int parse_worktree_ref(const char *worktree_ref, const char **name,
		       int *name_length, const char **ref);

/*
 * Return a refname suitable for access from the current ref store.
 */
void strbuf_worktree_ref(const struct worktree *wt,
			 struct strbuf *sb,
			 const char *refname);

#endif
