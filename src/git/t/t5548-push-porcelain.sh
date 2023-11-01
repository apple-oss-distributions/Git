#!/bin/sh
#
# Copyright (c) 2020 Jiang Xin
#
test_description='Test git push porcelain output'

. ./test-lib.sh

# Create commits in <repo> and assign each commit's oid to shell variables
# given in the arguments (A, B, and C). E.g.:
#
#     create_commits_in <repo> A B C
#
# NOTE: Never calling this function from a subshell since variable
# assignments will disappear when subshell exits.
create_commits_in () {
	repo="$1" &&
	if ! parent=$(git -C "$repo" rev-parse HEAD^{} --)
	then
		parent=
	fi &&
	T=$(git -C "$repo" write-tree) &&
	shift &&
	while test $# -gt 0
	do
		name=$1 &&
		test_tick &&
		if test -z "$parent"
		then
			oid=$(echo $name | git -C "$repo" commit-tree $T)
		else
			oid=$(echo $name | git -C "$repo" commit-tree -p $parent $T)
		fi &&
		eval $name=$oid &&
		parent=$oid &&
		shift ||
		return 1
	done &&
	git -C "$repo" update-ref refs/heads/main $oid
}

# Format the output of git-push, git-show-ref and other commands to make a
# user-friendly and stable text.  We can easily prepare the expect text
# without having to worry about future changes of the commit ID and spaces
# of the output.
make_user_friendly_and_stable_output () {
	sed \
		-e "s/  *\$//" \
		-e "s/   */ /g" \
		-e "s/	/    /g" \
		-e "s/$A/<COMMIT-A>/g" \
		-e "s/$B/<COMMIT-B>/g" \
		-e "s/$ZERO_OID/<ZERO-OID>/g" \
		-e "s/$(echo $A | cut -c1-7)[0-9a-f]*/<OID-A>/g" \
		-e "s/$(echo $B | cut -c1-7)[0-9a-f]*/<OID-B>/g" \
		-e "s#To $URL_PREFIX/upstream.git#To <URL/of/upstream.git>#"
}

setup_upstream_and_workbench () {
	# Upstream  after setup : main(B)  foo(A)  bar(A)  baz(A)
	# Workbench after setup : main(A)
	test_expect_success "setup upstream repository and workbench" '
		rm -rf upstream.git workbench &&
		git init --bare upstream.git &&
		git init workbench &&
		create_commits_in workbench A B &&
		(
			cd workbench &&
			# Try to make a stable fixed width for abbreviated commit ID,
			# this fixed-width oid will be replaced with "<OID>".
			git config core.abbrev 7 &&
			git remote add origin ../upstream.git &&
			git update-ref refs/heads/main $A &&
			git push origin \
				$B:refs/heads/main \
				$A:refs/heads/foo \
				$A:refs/heads/bar \
				$A:refs/heads/baz
		) &&
		git -C "workbench" config advice.pushUpdateRejected false &&
		upstream=upstream.git
	'
}

run_git_push_porcelain_output_test() {
	case $1 in
	http)
		PROTOCOL="HTTP protocol"
		URL_PREFIX="http://.*"
		;;
	file)
		PROTOCOL="builtin protocol"
		URL_PREFIX="\.\."
		;;
	esac

	# Refs of upstream : main(B)  foo(A)  bar(A)  baz(A)
	# Refs of workbench: main(A)                  baz(A)  next(A)
	# git-push         : main(A)  NULL    (B)     baz(A)  next(A)
	test_expect_success "porcelain output of successful git-push ($PROTOCOL)" '
		(
			cd workbench &&
			git update-ref refs/heads/main $A &&
			git update-ref refs/heads/baz $A &&
			git update-ref refs/heads/next $A &&
			git push --porcelain --force origin \
				main \
				:refs/heads/foo \
				$B:bar \
				baz \
				next
		) >out &&
		make_user_friendly_and_stable_output <out >actual &&
		cat >expect <<-EOF &&
		To <URL/of/upstream.git>
		=    refs/heads/baz:refs/heads/baz    [up to date]
		     <COMMIT-B>:refs/heads/bar    <OID-A>..<OID-B>
		-    :refs/heads/foo    [deleted]
		+    refs/heads/main:refs/heads/main    <OID-B>...<OID-A> (forced update)
		*    refs/heads/next:refs/heads/next    [new branch]
		Done
		EOF
		test_cmp expect actual &&

		git -C "$upstream" show-ref >out &&
		make_user_friendly_and_stable_output <out >actual &&
		cat >expect <<-EOF &&
		<COMMIT-B> refs/heads/bar
		<COMMIT-A> refs/heads/baz
		<COMMIT-A> refs/heads/main
		<COMMIT-A> refs/heads/next
		EOF
		test_cmp expect actual
	'

	# Refs of upstream : main(A)  bar(B)  baz(A)  next(A)
	# Refs of workbench: main(B)  bar(A)  baz(A)  next(A)
	# git-push         : main(B)  bar(A)  NULL    next(A)
	test_expect_success "atomic push failed ($PROTOCOL)" '
		(
			cd workbench &&
			git update-ref refs/heads/main $B &&
			git update-ref refs/heads/bar $A &&
			test_must_fail git push --atomic --porcelain origin \
				main \
				bar \
				:baz \
				next
		) >out &&
		make_user_friendly_and_stable_output <out >actual &&
		cat >expect <<-EOF &&
		To <URL/of/upstream.git>
		=    refs/heads/next:refs/heads/next    [up to date]
		!    refs/heads/bar:refs/heads/bar    [rejected] (non-fast-forward)
		!    (delete):refs/heads/baz    [rejected] (atomic push failed)
		!    refs/heads/main:refs/heads/main    [rejected] (atomic push failed)
		Done
		EOF
		test_cmp expect actual &&

		git -C "$upstream" show-ref >out &&
		make_user_friendly_and_stable_output <out >actual &&
		cat >expect <<-EOF &&
		<COMMIT-B> refs/heads/bar
		<COMMIT-A> refs/heads/baz
		<COMMIT-A> refs/heads/main
		<COMMIT-A> refs/heads/next
		EOF
		test_cmp expect actual
	'
	test_expect_success "prepare pre-receive hook ($PROTOCOL)" '
		write_script "$upstream/hooks/pre-receive" <<-EOF
		exit 1
		EOF
	'

	# Refs of upstream : main(A)  bar(B)  baz(A)  next(A)
	# Refs of workbench: main(B)  bar(A)  baz(A)  next(A)
	# git-push         : main(B)  bar(A)  NULL    next(A)
	test_expect_success "pre-receive hook declined ($PROTOCOL)" '
		(
			cd workbench &&
			git update-ref refs/heads/main $B &&
			git update-ref refs/heads/bar $A &&
			test_must_fail git push --porcelain --force origin \
				main \
				bar \
				:baz \
				next
		) >out &&
		make_user_friendly_and_stable_output <out >actual &&
		cat >expect <<-EOF &&
		To <URL/of/upstream.git>
		=    refs/heads/next:refs/heads/next    [up to date]
		!    refs/heads/bar:refs/heads/bar    [remote rejected] (pre-receive hook declined)
		!    :refs/heads/baz    [remote rejected] (pre-receive hook declined)
		!    refs/heads/main:refs/heads/main    [remote rejected] (pre-receive hook declined)
		Done
		EOF
		test_cmp expect actual &&

		git -C "$upstream" show-ref >out &&
		make_user_friendly_and_stable_output <out >actual &&
		cat >expect <<-EOF &&
		<COMMIT-B> refs/heads/bar
		<COMMIT-A> refs/heads/baz
		<COMMIT-A> refs/heads/main
		<COMMIT-A> refs/heads/next
		EOF
		test_cmp expect actual
	'

	test_expect_success "remove pre-receive hook ($PROTOCOL)" '
		rm "$upstream/hooks/pre-receive"
	'

	# Refs of upstream : main(A)  bar(B)  baz(A)  next(A)
	# Refs of workbench: main(B)  bar(A)  baz(A)  next(A)
	# git-push         : main(B)  bar(A)  NULL    next(A)
	test_expect_success "non-fastforward push ($PROTOCOL)" '
		(
			cd workbench &&
			test_must_fail git push --porcelain origin \
				main \
				bar \
				:baz \
				next
		) >out &&
		make_user_friendly_and_stable_output <out >actual &&
		cat >expect <<-EOF &&
		To <URL/of/upstream.git>
		=    refs/heads/next:refs/heads/next    [up to date]
		-    :refs/heads/baz    [deleted]
		     refs/heads/main:refs/heads/main    <OID-A>..<OID-B>
		!    refs/heads/bar:refs/heads/bar    [rejected] (non-fast-forward)
		Done
		EOF
		test_cmp expect actual &&

		git -C "$upstream" show-ref >out &&
		make_user_friendly_and_stable_output <out >actual &&
		cat >expect <<-EOF &&
		<COMMIT-B> refs/heads/bar
		<COMMIT-B> refs/heads/main
		<COMMIT-A> refs/heads/next
		EOF
		test_cmp expect actual
	'
}

# Initialize the upstream repository and local workbench.
setup_upstream_and_workbench

# Run git-push porcelain test on builtin protocol
run_git_push_porcelain_output_test file

ROOT_PATH="$PWD"
. "$TEST_DIRECTORY"/lib-gpg.sh
. "$TEST_DIRECTORY"/lib-httpd.sh
. "$TEST_DIRECTORY"/lib-terminal.sh
start_httpd

# Re-initialize the upstream repository and local workbench.
setup_upstream_and_workbench

test_expect_success "setup for http" '
	git -C upstream.git config http.receivepack true &&
	upstream="$HTTPD_DOCUMENT_ROOT_PATH/upstream.git" &&
	mv upstream.git "$upstream" &&

	git -C workbench remote set-url origin $HTTPD_URL/smart/upstream.git
'

setup_askpass_helper

# Run git-push porcelain test on HTTP protocol
run_git_push_porcelain_output_test http

test_done
