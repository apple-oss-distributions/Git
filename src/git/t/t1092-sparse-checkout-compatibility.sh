#!/bin/sh

test_description='compare full workdir to sparse workdir'

GIT_TEST_SPLIT_INDEX=0
GIT_TEST_SPARSE_INDEX=

. ./test-lib.sh

test_expect_success 'setup' '
	git init initial-repo &&
	(
		GIT_TEST_SPARSE_INDEX=0 &&
		cd initial-repo &&
		echo a >a &&
		echo "after deep" >e &&
		echo "after folder1" >g &&
		echo "after x" >z &&
		mkdir folder1 folder2 deep x &&
		mkdir deep/deeper1 deep/deeper2 &&
		mkdir deep/deeper1/deepest &&
		echo "after deeper1" >deep/e &&
		echo "after deepest" >deep/deeper1/e &&
		cp a folder1 &&
		cp a folder2 &&
		cp a x &&
		cp a deep &&
		cp a deep/deeper1 &&
		cp a deep/deeper2 &&
		cp a deep/deeper1/deepest &&
		cp -r deep/deeper1/deepest deep/deeper2 &&
		git add . &&
		git commit -m "initial commit" &&
		git checkout -b base &&
		for dir in folder1 folder2 deep
		do
			git checkout -b update-$dir &&
			echo "updated $dir" >$dir/a &&
			git commit -a -m "update $dir" || return 1
		done &&

		git checkout -b rename-base base &&
		echo >folder1/larger-content <<-\EOF &&
		matching
		lines
		help
		inexact
		renames
		EOF
		cp folder1/larger-content folder2/ &&
		cp folder1/larger-content deep/deeper1/ &&
		git add . &&
		git commit -m "add interesting rename content" &&

		git checkout -b rename-out-to-out rename-base &&
		mv folder1/a folder2/b &&
		mv folder1/larger-content folder2/edited-content &&
		echo >>folder2/edited-content &&
		git add . &&
		git commit -m "rename folder1/... to folder2/..." &&

		git checkout -b rename-out-to-in rename-base &&
		mv folder1/a deep/deeper1/b &&
		mv folder1/larger-content deep/deeper1/edited-content &&
		echo >>deep/deeper1/edited-content &&
		git add . &&
		git commit -m "rename folder1/... to deep/deeper1/..." &&

		git checkout -b rename-in-to-out rename-base &&
		mv deep/deeper1/a folder1/b &&
		mv deep/deeper1/larger-content folder1/edited-content &&
		echo >>folder1/edited-content &&
		git add . &&
		git commit -m "rename deep/deeper1/... to folder1/..." &&

		git checkout -b deepest base &&
		echo "updated deepest" >deep/deeper1/deepest/a &&
		git commit -a -m "update deepest" &&

		git checkout -f base &&
		git reset --hard
	)
'

init_repos () {
	rm -rf full-checkout sparse-checkout sparse-index &&

	# create repos in initial state
	cp -r initial-repo full-checkout &&
	git -C full-checkout reset --hard &&

	cp -r initial-repo sparse-checkout &&
	git -C sparse-checkout reset --hard &&

	cp -r initial-repo sparse-index &&
	git -C sparse-index reset --hard &&

	# initialize sparse-checkout definitions
	git -C sparse-checkout sparse-checkout init --cone &&
	git -C sparse-checkout sparse-checkout set deep &&
	git -C sparse-index sparse-checkout init --cone --sparse-index &&
	test_cmp_config -C sparse-index true index.sparse &&
	git -C sparse-index sparse-checkout set deep
}

run_on_sparse () {
	(
		cd sparse-checkout &&
		GIT_PROGRESS_DELAY=100000 "$@" >../sparse-checkout-out 2>../sparse-checkout-err
	) &&
	(
		cd sparse-index &&
		GIT_PROGRESS_DELAY=100000 "$@" >../sparse-index-out 2>../sparse-index-err
	)
}

run_on_all () {
	(
		cd full-checkout &&
		GIT_PROGRESS_DELAY=100000 "$@" >../full-checkout-out 2>../full-checkout-err
	) &&
	run_on_sparse "$@"
}

test_all_match () {
	run_on_all "$@" &&
	test_cmp full-checkout-out sparse-checkout-out &&
	test_cmp full-checkout-out sparse-index-out &&
	test_cmp full-checkout-err sparse-checkout-err &&
	test_cmp full-checkout-err sparse-index-err
}

test_sparse_match () {
	run_on_sparse "$@" &&
	test_cmp sparse-checkout-out sparse-index-out &&
	test_cmp sparse-checkout-err sparse-index-err
}

test_expect_success 'sparse-index contents' '
	init_repos &&

	test-tool -C sparse-index read-cache --table >cache &&
	for dir in folder1 folder2 x
	do
		TREE=$(git -C sparse-index rev-parse HEAD:$dir) &&
		grep "040000 tree $TREE	$dir/" cache \
			|| return 1
	done &&

	git -C sparse-index sparse-checkout set folder1 &&

	test-tool -C sparse-index read-cache --table >cache &&
	for dir in deep folder2 x
	do
		TREE=$(git -C sparse-index rev-parse HEAD:$dir) &&
		grep "040000 tree $TREE	$dir/" cache \
			|| return 1
	done &&

	git -C sparse-index sparse-checkout set deep/deeper1 &&

	test-tool -C sparse-index read-cache --table >cache &&
	for dir in deep/deeper2 folder1 folder2 x
	do
		TREE=$(git -C sparse-index rev-parse HEAD:$dir) &&
		grep "040000 tree $TREE	$dir/" cache \
			|| return 1
	done &&

	# Disabling the sparse-index removes tree entries with full ones
	git -C sparse-index sparse-checkout init --no-sparse-index &&

	test-tool -C sparse-index read-cache --table >cache &&
	! grep "040000 tree" cache &&
	test_sparse_match test-tool read-cache --table
'

test_expect_success 'expanded in-memory index matches full index' '
	init_repos &&
	test_sparse_match test-tool read-cache --expand --table
'

test_expect_success 'status with options' '
	init_repos &&
	test_sparse_match ls &&
	test_all_match git status --porcelain=v2 &&
	test_all_match git status --porcelain=v2 -z -u &&
	test_all_match git status --porcelain=v2 -uno &&
	run_on_all touch README.md &&
	test_all_match git status --porcelain=v2 &&
	test_all_match git status --porcelain=v2 -z -u &&
	test_all_match git status --porcelain=v2 -uno &&
	test_all_match git add README.md &&
	test_all_match git status --porcelain=v2 &&
	test_all_match git status --porcelain=v2 -z -u &&
	test_all_match git status --porcelain=v2 -uno
'

test_expect_success 'add, commit, checkout' '
	init_repos &&

	write_script edit-contents <<-\EOF &&
	echo text >>$1
	EOF
	run_on_all ../edit-contents README.md &&

	test_all_match git add README.md &&
	test_all_match git status --porcelain=v2 &&
	test_all_match git commit -m "Add README.md" &&

	test_all_match git checkout HEAD~1 &&
	test_all_match git checkout - &&

	run_on_all ../edit-contents README.md &&

	test_all_match git add -A &&
	test_all_match git status --porcelain=v2 &&
	test_all_match git commit -m "Extend README.md" &&

	test_all_match git checkout HEAD~1 &&
	test_all_match git checkout - &&

	run_on_all ../edit-contents deep/newfile &&

	test_all_match git status --porcelain=v2 -uno &&
	test_all_match git status --porcelain=v2 &&
	test_all_match git add . &&
	test_all_match git status --porcelain=v2 &&
	test_all_match git commit -m "add deep/newfile" &&

	test_all_match git checkout HEAD~1 &&
	test_all_match git checkout -
'

test_expect_success 'checkout and reset --hard' '
	init_repos &&

	test_all_match git checkout update-folder1 &&
	test_all_match git status --porcelain=v2 &&

	test_all_match git checkout update-deep &&
	test_all_match git status --porcelain=v2 &&

	test_all_match git checkout -b reset-test &&
	test_all_match git reset --hard deepest &&
	test_all_match git reset --hard update-folder1 &&
	test_all_match git reset --hard update-folder2
'

test_expect_success 'diff --staged' '
	init_repos &&

	write_script edit-contents <<-\EOF &&
	echo text >>README.md
	EOF
	run_on_all ../edit-contents &&

	test_all_match git diff &&
	test_all_match git diff --staged &&
	test_all_match git add README.md &&
	test_all_match git diff &&
	test_all_match git diff --staged
'

test_expect_success 'diff with renames' '
	init_repos &&

	for branch in rename-out-to-out rename-out-to-in rename-in-to-out
	do
		test_all_match git checkout rename-base &&
		test_all_match git checkout $branch -- .&&
		test_all_match git diff --staged --no-renames &&
		test_all_match git diff --staged --find-renames || return 1
	done
'

test_expect_success 'log with pathspec outside sparse definition' '
	init_repos &&

	test_all_match git log -- a &&
	test_all_match git log -- folder1/a &&
	test_all_match git log -- folder2/a &&
	test_all_match git log -- deep/a &&
	test_all_match git log -- deep/deeper1/a &&
	test_all_match git log -- deep/deeper1/deepest/a &&

	test_all_match git checkout update-folder1 &&
	test_all_match git log -- folder1/a
'

test_expect_success 'blame with pathspec inside sparse definition' '
	init_repos &&

	test_all_match git blame a &&
	test_all_match git blame deep/a &&
	test_all_match git blame deep/deeper1/a &&
	test_all_match git blame deep/deeper1/deepest/a
'

# TODO: blame currently does not support blaming files outside of the
# sparse definition. It complains that the file doesn't exist locally.
test_expect_failure 'blame with pathspec outside sparse definition' '
	init_repos &&

	test_all_match git blame folder1/a &&
	test_all_match git blame folder2/a &&
	test_all_match git blame deep/deeper2/a &&
	test_all_match git blame deep/deeper2/deepest/a
'

# TODO: reset currently does not behave as expected when in a
# sparse-checkout.
test_expect_failure 'checkout and reset (mixed)' '
	init_repos &&

	test_all_match git checkout -b reset-test update-deep &&
	test_all_match git reset deepest &&
	test_all_match git reset update-folder1 &&
	test_all_match git reset update-folder2
'

# Ensure that sparse-index behaves identically to
# sparse-checkout with a full index.
test_expect_success 'checkout and reset (mixed) [sparse]' '
	init_repos &&

	test_sparse_match git checkout -b reset-test update-deep &&
	test_sparse_match git reset deepest &&
	test_sparse_match git reset update-folder1 &&
	test_sparse_match git reset update-folder2
'

test_expect_success 'merge' '
	init_repos &&

	test_all_match git checkout -b merge update-deep &&
	test_all_match git merge -m "folder1" update-folder1 &&
	test_all_match git rev-parse HEAD^{tree} &&
	test_all_match git merge -m "folder2" update-folder2 &&
	test_all_match git rev-parse HEAD^{tree}
'

test_expect_success 'merge with outside renames' '
	init_repos &&

	for type in out-to-out out-to-in in-to-out
	do
		test_all_match git reset --hard &&
		test_all_match git checkout -f -b merge-$type update-deep &&
		test_all_match git merge -m "$type" rename-$type &&
		test_all_match git rev-parse HEAD^{tree} || return 1
	done
'

test_expect_success 'clean' '
	init_repos &&

	echo bogus >>.gitignore &&
	run_on_all cp ../.gitignore . &&
	test_all_match git add .gitignore &&
	test_all_match git commit -m "ignore bogus files" &&

	run_on_sparse mkdir folder1 &&
	run_on_all touch folder1/bogus &&

	test_all_match git status --porcelain=v2 &&
	test_all_match git clean -f &&
	test_all_match git status --porcelain=v2 &&
	test_sparse_match ls &&
	test_sparse_match ls folder1 &&

	test_all_match git clean -xf &&
	test_all_match git status --porcelain=v2 &&
	test_sparse_match ls &&
	test_sparse_match ls folder1 &&

	test_all_match git clean -xdf &&
	test_all_match git status --porcelain=v2 &&
	test_sparse_match ls &&
	test_sparse_match ls folder1 &&

	test_sparse_match test_path_is_dir folder1
'

test_expect_success 'submodule handling' '
	init_repos &&

	test_all_match mkdir modules &&
	test_all_match touch modules/a &&
	test_all_match git add modules &&
	test_all_match git commit -m "add modules directory" &&

	run_on_all git submodule add "$(pwd)/initial-repo" modules/sub &&
	test_all_match git commit -m "add submodule" &&

	# having a submodule prevents "modules" from collapse
	test-tool -C sparse-index read-cache --table >cache &&
	grep "100644 blob .*	modules/a" cache &&
	grep "160000 commit $(git -C initial-repo rev-parse HEAD)	modules/sub" cache
'

test_expect_success 'sparse-index is expanded and converted back' '
	init_repos &&

	GIT_TRACE2_EVENT="$(pwd)/trace2.txt" GIT_TRACE2_EVENT_NESTING=10 \
		git -C sparse-index -c core.fsmonitor="" reset --hard &&
	test_region index convert_to_sparse trace2.txt &&
	test_region index ensure_full_index trace2.txt &&

	rm trace2.txt &&
	GIT_TRACE2_EVENT="$(pwd)/trace2.txt" GIT_TRACE2_EVENT_NESTING=10 \
		git -C sparse-index -c core.fsmonitor="" status -uno &&
	test_region index ensure_full_index trace2.txt
'

test_done
