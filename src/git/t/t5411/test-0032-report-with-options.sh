test_expect_success "setup proc-receive hook (option without matching ok, $PROTOCOL)" '
	write_script "$upstream/hooks/proc-receive" <<-EOF
	printf >&2 "# proc-receive hook\n"
	test-tool proc-receive -v \
		-r "option refname refs/pull/123/head" \
		-r "option old-oid $B"
	EOF
'

# Refs of upstream : main(A)
# Refs of workbench: main(A)  tags/v123
# git push         :                       refs/for/next/topic(A)  refs/for/a/b/c/topic(A)  refs/for/main/topic(A)
test_expect_success "proc-receive: report option without matching ok ($PROTOCOL)" '
	test_must_fail git -C workbench push origin \
		HEAD:refs/for/main/topic \
		>out-$test_count 2>&1 &&
	make_user_friendly_and_stable_output <out-$test_count >actual &&
	cat >expect <<-EOF &&
	remote: # pre-receive hook
	remote: pre-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	remote: # proc-receive hook
	remote: proc-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	remote: proc-receive> option refname refs/pull/123/head
	remote: proc-receive> option old-oid <COMMIT-B>
	remote: error: proc-receive reported "option" without a matching "ok/ng" directive
	To <URL/of/upstream.git>
	 ! [remote rejected] HEAD -> refs/for/main/topic (proc-receive failed to report status)
	EOF
	test_cmp expect actual
'

test_expect_success "setup proc-receive hook (option refname, $PROTOCOL)" '
	write_script "$upstream/hooks/proc-receive" <<-EOF
	printf >&2 "# proc-receive hook\n"
	test-tool proc-receive -v \
		-r "ok refs/for/main/topic" \
		-r "option refname refs/pull/123/head"
	EOF
'

# Refs of upstream : main(A)
# Refs of workbench: main(A)  tags/v123
# git push         :                       refs/for/next/topic(A)  refs/for/a/b/c/topic(A)  refs/for/main/topic(A)
test_expect_success "proc-receive: report option refname ($PROTOCOL)" '
	git -C workbench push origin \
		HEAD:refs/for/main/topic \
		>out 2>&1 &&
	make_user_friendly_and_stable_output <out >actual &&
	cat >expect <<-EOF &&
	remote: # pre-receive hook
	remote: pre-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	remote: # proc-receive hook
	remote: proc-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	remote: proc-receive> ok refs/for/main/topic
	remote: proc-receive> option refname refs/pull/123/head
	remote: # post-receive hook
	remote: post-receive< <ZERO-OID> <COMMIT-A> refs/pull/123/head
	To <URL/of/upstream.git>
	 * [new reference] HEAD -> refs/pull/123/head
	EOF
	test_cmp expect actual
'

test_expect_success "setup proc-receive hook (option refname and forced-update, $PROTOCOL)" '
	write_script "$upstream/hooks/proc-receive" <<-EOF
	printf >&2 "# proc-receive hook\n"
	test-tool proc-receive -v \
		-r "ok refs/for/main/topic" \
		-r "option refname refs/pull/123/head" \
		-r "option forced-update"
	EOF
'
# Refs of upstream : main(A)
# Refs of workbench: main(A)  tags/v123
# git push         :                       refs/for/next/topic(A)  refs/for/a/b/c/topic(A)  refs/for/main/topic(A)
test_expect_success "proc-receive: report option refname and forced-update ($PROTOCOL)" '
	git -C workbench push origin \
		HEAD:refs/for/main/topic \
		>out 2>&1 &&
	make_user_friendly_and_stable_output <out >actual &&
	cat >expect <<-EOF &&
	remote: # pre-receive hook
	remote: pre-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	remote: # proc-receive hook
	remote: proc-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	remote: proc-receive> ok refs/for/main/topic
	remote: proc-receive> option refname refs/pull/123/head
	remote: proc-receive> option forced-update
	remote: # post-receive hook
	remote: post-receive< <ZERO-OID> <COMMIT-A> refs/pull/123/head
	To <URL/of/upstream.git>
	 * [new reference] HEAD -> refs/pull/123/head
	EOF
	test_cmp expect actual
'

test_expect_success "setup proc-receive hook (option refname and old-oid, $PROTOCOL)" '
	write_script "$upstream/hooks/proc-receive" <<-EOF
	printf >&2 "# proc-receive hook\n"
	test-tool proc-receive -v \
		-r "ok refs/for/main/topic" \
		-r "option refname refs/pull/123/head" \
		-r "option old-oid $B"
	EOF
'

# Refs of upstream : main(A)
# Refs of workbench: main(A)  tags/v123
# git push         :                       refs/for/next/topic(A)  refs/for/a/b/c/topic(A)  refs/for/main/topic(A)
test_expect_success "proc-receive: report option refname and old-oid ($PROTOCOL)" '
	git -C workbench push origin \
		HEAD:refs/for/main/topic \
		>out 2>&1 &&
	make_user_friendly_and_stable_output <out >actual &&
	cat >expect <<-EOF &&
	remote: # pre-receive hook
	remote: pre-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	remote: # proc-receive hook
	remote: proc-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	remote: proc-receive> ok refs/for/main/topic
	remote: proc-receive> option refname refs/pull/123/head
	remote: proc-receive> option old-oid <COMMIT-B>
	remote: # post-receive hook
	remote: post-receive< <COMMIT-B> <COMMIT-A> refs/pull/123/head
	To <URL/of/upstream.git>
	 <OID-B>..<OID-A> HEAD -> refs/pull/123/head
	EOF
	test_cmp expect actual
'

test_expect_success "setup proc-receive hook (option old-oid, $PROTOCOL)" '
	write_script "$upstream/hooks/proc-receive" <<-EOF
	printf >&2 "# proc-receive hook\n"
	test-tool proc-receive -v \
		-r "ok refs/for/main/topic" \
		-r "option old-oid $B"
	EOF
'

# Refs of upstream : main(A)
# Refs of workbench: main(A)  tags/v123
# git push         :                       refs/for/next/topic(A)  refs/for/a/b/c/topic(A)  refs/for/main/topic(A)
test_expect_success "proc-receive: report option old-oid ($PROTOCOL)" '
	git -C workbench push origin \
		HEAD:refs/for/main/topic \
		>out 2>&1 &&
	make_user_friendly_and_stable_output <out >actual &&
	cat >expect <<-EOF &&
	remote: # pre-receive hook
	remote: pre-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	remote: # proc-receive hook
	remote: proc-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	remote: proc-receive> ok refs/for/main/topic
	remote: proc-receive> option old-oid <COMMIT-B>
	remote: # post-receive hook
	remote: post-receive< <COMMIT-B> <COMMIT-A> refs/for/main/topic
	To <URL/of/upstream.git>
	 <OID-B>..<OID-A> HEAD -> refs/for/main/topic
	EOF
	test_cmp expect actual
'

test_expect_success "setup proc-receive hook (option old-oid and new-oid, $PROTOCOL)" '
	write_script "$upstream/hooks/proc-receive" <<-EOF
	printf >&2 "# proc-receive hook\n"
	test-tool proc-receive -v \
		-r "ok refs/for/main/topic" \
		-r "option old-oid $A" \
		-r "option new-oid $B"
	EOF
'

# Refs of upstream : main(A)
# Refs of workbench: main(A)  tags/v123
# git push         :                       refs/for/next/topic(A)  refs/for/a/b/c/topic(A)  refs/for/main/topic(A)
test_expect_success "proc-receive: report option old-oid and new-oid ($PROTOCOL)" '
	git -C workbench push origin \
		HEAD:refs/for/main/topic \
		>out 2>&1 &&
	make_user_friendly_and_stable_output <out >actual &&
	cat >expect <<-EOF &&
	remote: # pre-receive hook
	remote: pre-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	remote: # proc-receive hook
	remote: proc-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	remote: proc-receive> ok refs/for/main/topic
	remote: proc-receive> option old-oid <COMMIT-A>
	remote: proc-receive> option new-oid <COMMIT-B>
	remote: # post-receive hook
	remote: post-receive< <COMMIT-A> <COMMIT-B> refs/for/main/topic
	To <URL/of/upstream.git>
	 <OID-A>..<OID-B> HEAD -> refs/for/main/topic
	EOF
	test_cmp expect actual
'

test_expect_success "setup proc-receive hook (report with multiple rewrites, $PROTOCOL)" '
	write_script "$upstream/hooks/proc-receive" <<-EOF
	printf >&2 "# proc-receive hook\n"
	test-tool proc-receive -v \
		-r "ok refs/for/a/b/c/topic" \
		-r "ok refs/for/next/topic" \
		-r "option refname refs/pull/123/head" \
		-r "ok refs/for/main/topic" \
		-r "option refname refs/pull/124/head" \
		-r "option old-oid $B" \
		-r "option forced-update" \
		-r "option new-oid $A"
	EOF
'

# Refs of upstream : main(A)
# Refs of workbench: main(A)  tags/v123
# git push         :                       refs/for/next/topic(A)  refs/for/a/b/c/topic(A)  refs/for/main/topic(A)
test_expect_success "proc-receive: report with multiple rewrites ($PROTOCOL)" '
	git -C workbench push origin \
		HEAD:refs/for/next/topic \
		HEAD:refs/for/a/b/c/topic \
		HEAD:refs/for/main/topic \
		>out 2>&1 &&
	make_user_friendly_and_stable_output <out >actual &&
	cat >expect <<-EOF &&
	remote: # pre-receive hook
	remote: pre-receive< <ZERO-OID> <COMMIT-A> refs/for/next/topic
	remote: pre-receive< <ZERO-OID> <COMMIT-A> refs/for/a/b/c/topic
	remote: pre-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	remote: # proc-receive hook
	remote: proc-receive< <ZERO-OID> <COMMIT-A> refs/for/next/topic
	remote: proc-receive< <ZERO-OID> <COMMIT-A> refs/for/a/b/c/topic
	remote: proc-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	remote: proc-receive> ok refs/for/a/b/c/topic
	remote: proc-receive> ok refs/for/next/topic
	remote: proc-receive> option refname refs/pull/123/head
	remote: proc-receive> ok refs/for/main/topic
	remote: proc-receive> option refname refs/pull/124/head
	remote: proc-receive> option old-oid <COMMIT-B>
	remote: proc-receive> option forced-update
	remote: proc-receive> option new-oid <COMMIT-A>
	remote: # post-receive hook
	remote: post-receive< <ZERO-OID> <COMMIT-A> refs/pull/123/head
	remote: post-receive< <ZERO-OID> <COMMIT-A> refs/for/a/b/c/topic
	remote: post-receive< <COMMIT-B> <COMMIT-A> refs/pull/124/head
	To <URL/of/upstream.git>
	 * [new reference] HEAD -> refs/pull/123/head
	 * [new reference] HEAD -> refs/for/a/b/c/topic
	 + <OID-B>...<OID-A> HEAD -> refs/pull/124/head (forced update)
	EOF
	test_cmp expect actual &&

	test_cmp_refs -C "$upstream" <<-EOF
	<COMMIT-A> refs/heads/main
	EOF
'
