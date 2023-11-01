# Library of functions shared by all CI scripts

skip_branch_tip_with_tag () {
	# Sometimes, a branch is pushed at the same time the tag that points
	# at the same commit as the tip of the branch is pushed, and building
	# both at the same time is a waste.
	#
	# When the build is triggered by a push to a tag, $CI_BRANCH will
	# have that tagname, e.g. v2.14.0.  Let's see if $CI_BRANCH is
	# exactly at a tag, and if so, if it is different from $CI_BRANCH.
	# That way, we can tell if we are building the tip of a branch that
	# is tagged and we can skip the build because we won't be skipping a
	# build of a tag.

	if TAG=$(git describe --exact-match "$CI_BRANCH" 2>/dev/null) &&
		test "$TAG" != "$CI_BRANCH"
	then
		echo "$(tput setaf 2)Tip of $CI_BRANCH is exactly at $TAG$(tput sgr0)"
		exit 0
	fi
}

# Save some info about the current commit's tree, so we can skip the build
# job if we encounter the same tree again and can provide a useful info
# message.
save_good_tree () {
	echo "$(git rev-parse $CI_COMMIT^{tree}) $CI_COMMIT $CI_JOB_NUMBER $CI_JOB_ID" >>"$good_trees_file"
	# limit the file size
	tail -1000 "$good_trees_file" >"$good_trees_file".tmp
	mv "$good_trees_file".tmp "$good_trees_file"
}

# Skip the build job if the same tree has already been built and tested
# successfully before (e.g. because the branch got rebased, changing only
# the commit messages).
skip_good_tree () {
	if test "$TRAVIS_DEBUG_MODE" = true || test true = "$GITHUB_ACTIONS"
	then
		return
	fi

	if ! good_tree_info="$(grep "^$(git rev-parse $CI_COMMIT^{tree}) " "$good_trees_file")"
	then
		# Haven't seen this tree yet, or no cached good trees file yet.
		# Continue the build job.
		return
	fi

	echo "$good_tree_info" | {
		read tree prev_good_commit prev_good_job_number prev_good_job_id

		if test "$CI_JOB_ID" = "$prev_good_job_id"
		then
			cat <<-EOF
			$(tput setaf 2)Skipping build job for commit $CI_COMMIT.$(tput sgr0)
			This commit has already been built and tested successfully by this build job.
			To force a re-build delete the branch's cache and then hit 'Restart job'.
			EOF
		else
			cat <<-EOF
			$(tput setaf 2)Skipping build job for commit $CI_COMMIT.$(tput sgr0)
			This commit's tree has already been built and tested successfully in build job $prev_good_job_number for commit $prev_good_commit.
			The log of that build job is available at $(url_for_job_id $prev_good_job_id)
			To force a re-build delete the branch's cache and then hit 'Restart job'.
			EOF
		fi
	}

	exit 0
}

check_unignored_build_artifacts ()
{
	! git ls-files --other --exclude-standard --error-unmatch \
		-- ':/*' 2>/dev/null ||
	{
		echo "$(tput setaf 1)error: found unignored build artifacts$(tput sgr0)"
		false
	}
}

# GitHub Action doesn't set TERM, which is required by tput
export TERM=${TERM:-dumb}

# Clear MAKEFLAGS that may come from the outside world.
export MAKEFLAGS=

# Set 'exit on error' for all CI scripts to let the caller know that
# something went wrong.
# Set tracing executed commands, primarily setting environment variables
# and installing dependencies.
set -ex

if test true = "$TRAVIS"
then
	CI_TYPE=travis
	# When building a PR, TRAVIS_BRANCH refers to the *target* branch. Not
	# what we want here. We want the source branch instead.
	CI_BRANCH="${TRAVIS_PULL_REQUEST_BRANCH:-$TRAVIS_BRANCH}"
	CI_COMMIT="$TRAVIS_COMMIT"
	CI_JOB_ID="$TRAVIS_JOB_ID"
	CI_JOB_NUMBER="$TRAVIS_JOB_NUMBER"
	CI_OS_NAME="$TRAVIS_OS_NAME"
	CI_REPO_SLUG="$TRAVIS_REPO_SLUG"

	cache_dir="$HOME/travis-cache"

	url_for_job_id () {
		echo "https://travis-ci.org/$CI_REPO_SLUG/jobs/$1"
	}

	BREW_INSTALL_PACKAGES="git-lfs gettext"
	export GIT_PROVE_OPTS="--timer --jobs 3 --state=failed,slow,save"
	export GIT_TEST_OPTS="--verbose-log -x --immediate"
	MAKEFLAGS="$MAKEFLAGS --jobs=2"
elif test -n "$SYSTEM_COLLECTIONURI" || test -n "$SYSTEM_TASKDEFINITIONSURI"
then
	CI_TYPE=azure-pipelines
	# We are running in Azure Pipelines
	CI_BRANCH="$BUILD_SOURCEBRANCH"
	CI_COMMIT="$BUILD_SOURCEVERSION"
	CI_JOB_ID="$BUILD_BUILDID"
	CI_JOB_NUMBER="$BUILD_BUILDNUMBER"
	CI_OS_NAME="$(echo "$AGENT_OS" | tr A-Z a-z)"
	test darwin != "$CI_OS_NAME" || CI_OS_NAME=osx
	CI_REPO_SLUG="$(expr "$BUILD_REPOSITORY_URI" : '.*/\([^/]*/[^/]*\)$')"
	CC="${CC:-gcc}"

	# use a subdirectory of the cache dir (because the file share is shared
	# among *all* phases)
	cache_dir="$HOME/test-cache/$SYSTEM_PHASENAME"

	url_for_job_id () {
		echo "$SYSTEM_TASKDEFINITIONSURI$SYSTEM_TEAMPROJECT/_build/results?buildId=$1"
	}

	export GIT_PROVE_OPTS="--timer --jobs 10 --state=failed,slow,save"
	export GIT_TEST_OPTS="--verbose-log -x --write-junit-xml"
	MAKEFLAGS="$MAKEFLAGS --jobs=10"
	test windows_nt != "$CI_OS_NAME" ||
	GIT_TEST_OPTS="--no-chain-lint --no-bin-wrappers $GIT_TEST_OPTS"
elif test true = "$GITHUB_ACTIONS"
then
	CI_TYPE=github-actions
	CI_BRANCH="$GITHUB_REF"
	CI_COMMIT="$GITHUB_SHA"
	CI_OS_NAME="$(echo "$RUNNER_OS" | tr A-Z a-z)"
	test macos != "$CI_OS_NAME" || CI_OS_NAME=osx
	CI_REPO_SLUG="$GITHUB_REPOSITORY"
	CI_JOB_ID="$GITHUB_RUN_ID"
	CC="${CC:-gcc}"
	DONT_SKIP_TAGS=t

	cache_dir="$HOME/none"

	export GIT_PROVE_OPTS="--timer --jobs 10"
	export GIT_TEST_OPTS="--verbose-log -x"
	MAKEFLAGS="$MAKEFLAGS --jobs=10"
	test windows != "$CI_OS_NAME" ||
	GIT_TEST_OPTS="--no-chain-lint --no-bin-wrappers $GIT_TEST_OPTS"
else
	echo "Could not identify CI type" >&2
	env >&2
	exit 1
fi

good_trees_file="$cache_dir/good-trees"

mkdir -p "$cache_dir"

test -n "${DONT_SKIP_TAGS-}" ||
skip_branch_tip_with_tag
skip_good_tree

if test -z "$jobname"
then
	jobname="$CI_OS_NAME-$CC"
fi

export DEVELOPER=1
export DEFAULT_TEST_TARGET=prove
export GIT_TEST_CLONE_2GB=true
export SKIP_DASHED_BUILT_INS=YesPlease

case "$jobname" in
linux-clang|linux-gcc)
	if [ "$jobname" = linux-gcc ]
	then
		export CC=gcc-8
		MAKEFLAGS="$MAKEFLAGS PYTHON_PATH=/usr/bin/python3"
	else
		MAKEFLAGS="$MAKEFLAGS PYTHON_PATH=/usr/bin/python2"
	fi

	export GIT_TEST_HTTPD=true

	# The Linux build installs the defined dependency versions below.
	# The OS X build installs much more recent versions, whichever
	# were recorded in the Homebrew database upon creating the OS X
	# image.
	# Keep that in mind when you encounter a broken OS X build!
	export LINUX_P4_VERSION="16.2"
	export LINUX_GIT_LFS_VERSION="1.5.2"

	P4_PATH="$HOME/custom/p4"
	GIT_LFS_PATH="$HOME/custom/git-lfs"
	export PATH="$GIT_LFS_PATH:$P4_PATH:$PATH"
	;;
osx-clang|osx-gcc)
	if [ "$jobname" = osx-gcc ]
	then
		export CC=gcc-9
		MAKEFLAGS="$MAKEFLAGS PYTHON_PATH=$(which python3)"
	else
		MAKEFLAGS="$MAKEFLAGS PYTHON_PATH=$(which python2)"
	fi

	# t9810 occasionally fails on Travis CI OS X
	# t9816 occasionally fails with "TAP out of sequence errors" on
	# Travis CI OS X
	export GIT_SKIP_TESTS="t9810 t9816"
	;;
linux-gcc-default)
	;;
Linux32)
	CC=gcc
	;;
linux-musl)
	CC=gcc
	MAKEFLAGS="$MAKEFLAGS PYTHON_PATH=/usr/bin/python3 USE_LIBPCRE2=Yes"
	MAKEFLAGS="$MAKEFLAGS NO_REGEX=Yes ICONV_OMITS_BOM=Yes"
	;;
esac

MAKEFLAGS="$MAKEFLAGS CC=${CC:-cc}"
