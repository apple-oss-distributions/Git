test_expect_success "setup git config for remote-tracking of special refs" '
	(
		cd workbench &&
		if ! git config --get-all remote.origin.fetch | grep refs/for/
		then
			git config --add remote.origin.fetch \
				"+refs/for/*:refs/t/for/*" &&
			git config --add remote.origin.fetch \
				"+refs/pull/*:refs/t/pull/*" &&
			git config --add remote.origin.fetch \
				"+refs/changes/*:refs/t/changes/*"
		fi
	)
'

test_expect_success "setup proc-receive hook (multiple rewrites for one ref, no refname for the 1st rewrite, $PROTOCOL)" '
	write_script "$upstream/hooks/proc-receive" <<-EOF
	printf >&2 "# proc-receive hook\n"
	test-tool proc-receive -v \
		-r "ok refs/for/main/topic" \
		-r "option old-oid $A" \
		-r "option new-oid $B" \
		-r "ok refs/for/main/topic" \
		-r "option refname refs/changes/24/124/1" \
		-r "option old-oid $ZERO_OID" \
		-r "option new-oid $A" \
		-r "ok refs/for/main/topic" \
		-r "option refname refs/changes/25/125/1" \
		-r "option old-oid $A" \
		-r "option new-oid $B"
	EOF
'

# Refs of upstream : main(A)
# Refs of workbench: main(A)  tags/v123
# git push         :                       refs/for/main/topic(A)
test_expect_success "proc-receive: multiple rewrite for one ref, no refname for the 1st rewrite ($PROTOCOL)" '
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
	remote: proc-receive> ok refs/for/main/topic
	remote: proc-receive> option refname refs/changes/24/124/1
	remote: proc-receive> option old-oid <ZERO-OID>
	remote: proc-receive> option new-oid <COMMIT-A>
	remote: proc-receive> ok refs/for/main/topic
	remote: proc-receive> option refname refs/changes/25/125/1
	remote: proc-receive> option old-oid <COMMIT-A>
	remote: proc-receive> option new-oid <COMMIT-B>
	remote: # post-receive hook
	remote: post-receive< <COMMIT-A> <COMMIT-B> refs/for/main/topic
	remote: post-receive< <ZERO-OID> <COMMIT-A> refs/changes/24/124/1
	remote: post-receive< <COMMIT-A> <COMMIT-B> refs/changes/25/125/1
	To <URL/of/upstream.git>
	 <OID-A>..<OID-B> HEAD -> refs/for/main/topic
	 * [new reference] HEAD -> refs/changes/24/124/1
	 <OID-A>..<OID-B> HEAD -> refs/changes/25/125/1
	EOF
	test_cmp expect actual &&
	git -C "$upstream" show-ref >out &&
	make_user_friendly_and_stable_output <out >actual &&
	cat >expect <<-EOF &&
	<COMMIT-A> refs/heads/main
	EOF
	test_cmp expect actual
'

test_expect_success "proc-receive: check remote-tracking #1 ($PROTOCOL)" '
	git -C workbench show-ref |
		grep -v -e refs/remotes -e refs/heads -e refs/tags >out &&
	make_user_friendly_and_stable_output <out >actual &&
	cat >expect <<-EOF &&
	<COMMIT-A> refs/t/changes/24/124/1
	<COMMIT-B> refs/t/changes/25/125/1
	<COMMIT-B> refs/t/for/main/topic
	EOF
	test_cmp expect actual &&
	git -C workbench update-ref -d refs/t/for/main/topic &&
	git -C workbench update-ref -d refs/t/changes/24/124/1 &&
	git -C workbench update-ref -d refs/t/changes/25/125/1
'

test_expect_success "setup proc-receive hook (multiple rewrites for one ref, no refname for the 2nd rewrite, $PROTOCOL)" '
	write_script "$upstream/hooks/proc-receive" <<-EOF
	printf >&2 "# proc-receive hook\n"
	test-tool proc-receive -v \
		-r "ok refs/for/main/topic" \
		-r "option refname refs/changes/24/124/1" \
		-r "option old-oid $ZERO_OID" \
		-r "option new-oid $A" \
		-r "ok refs/for/main/topic" \
		-r "option old-oid $A" \
		-r "option new-oid $B" \
		-r "ok refs/for/main/topic" \
		-r "option refname refs/changes/25/125/1" \
		-r "option old-oid $B" \
		-r "option new-oid $A" \
		-r "option forced-update"
	EOF
'

# Refs of upstream : main(A)
# Refs of workbench: main(A)  tags/v123
# git push         :                       refs/for/main/topic(A)
test_expect_success "proc-receive: multiple rewrites for one ref, no refname for the 2nd rewrite ($PROTOCOL)" '
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
	remote: proc-receive> option refname refs/changes/24/124/1
	remote: proc-receive> option old-oid <ZERO-OID>
	remote: proc-receive> option new-oid <COMMIT-A>
	remote: proc-receive> ok refs/for/main/topic
	remote: proc-receive> option old-oid <COMMIT-A>
	remote: proc-receive> option new-oid <COMMIT-B>
	remote: proc-receive> ok refs/for/main/topic
	remote: proc-receive> option refname refs/changes/25/125/1
	remote: proc-receive> option old-oid <COMMIT-B>
	remote: proc-receive> option new-oid <COMMIT-A>
	remote: proc-receive> option forced-update
	remote: # post-receive hook
	remote: post-receive< <ZERO-OID> <COMMIT-A> refs/changes/24/124/1
	remote: post-receive< <COMMIT-A> <COMMIT-B> refs/for/main/topic
	remote: post-receive< <COMMIT-B> <COMMIT-A> refs/changes/25/125/1
	To <URL/of/upstream.git>
	 * [new reference] HEAD -> refs/changes/24/124/1
	 <OID-A>..<OID-B> HEAD -> refs/for/main/topic
	 + <OID-B>...<OID-A> HEAD -> refs/changes/25/125/1 (forced update)
	EOF
	test_cmp expect actual &&
	git -C "$upstream" show-ref >out &&
	make_user_friendly_and_stable_output <out >actual &&
	cat >expect <<-EOF &&
	<COMMIT-A> refs/heads/main
	EOF
	test_cmp expect actual
'

test_expect_success "proc-receive: check remote-tracking #2 ($PROTOCOL)" '
	git -C workbench show-ref |
		grep -v -e refs/remotes -e refs/heads -e refs/tags >out &&
	make_user_friendly_and_stable_output <out >actual &&
	cat >expect <<-EOF &&
	<COMMIT-A> refs/t/changes/24/124/1
	<COMMIT-A> refs/t/changes/25/125/1
	<COMMIT-B> refs/t/for/main/topic
	EOF
	test_cmp expect actual &&
	git -C workbench update-ref -d refs/t/for/main/topic &&
	git -C workbench update-ref -d refs/t/changes/24/124/1 &&
	git -C workbench update-ref -d refs/t/changes/25/125/1
'

test_expect_success "setup proc-receive hook (multiple rewrites for one ref, $PROTOCOL)" '
	write_script "$upstream/hooks/proc-receive" <<-EOF
	printf >&2 "# proc-receive hook\n"
	test-tool proc-receive -v \
		-r "ok refs/for/main/topic" \
		-r "option refname refs/changes/23/123/1" \
		-r "ok refs/for/main/topic" \
		-r "option refname refs/changes/24/124/2" \
		-r "option old-oid $A" \
		-r "option new-oid $B"
	EOF
'

# Refs of upstream : main(A)
# Refs of workbench: main(A)  tags/v123
# git push         :                       refs/for/main/topic(A)
test_expect_success "proc-receive: multiple rewrites for one ref ($PROTOCOL)" '
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
	remote: proc-receive> option refname refs/changes/23/123/1
	remote: proc-receive> ok refs/for/main/topic
	remote: proc-receive> option refname refs/changes/24/124/2
	remote: proc-receive> option old-oid <COMMIT-A>
	remote: proc-receive> option new-oid <COMMIT-B>
	remote: # post-receive hook
	remote: post-receive< <ZERO-OID> <COMMIT-A> refs/changes/23/123/1
	remote: post-receive< <COMMIT-A> <COMMIT-B> refs/changes/24/124/2
	To <URL/of/upstream.git>
	 * [new reference] HEAD -> refs/changes/23/123/1
	 <OID-A>..<OID-B> HEAD -> refs/changes/24/124/2
	EOF
	test_cmp expect actual &&
	git -C "$upstream" show-ref >out &&
	make_user_friendly_and_stable_output <out >actual &&
	cat >expect <<-EOF &&
	<COMMIT-A> refs/heads/main
	EOF
	test_cmp expect actual
'

test_expect_success "proc-receive: check remote-tracking #3 ($PROTOCOL)" '
	git -C workbench show-ref |
		grep -v -e refs/remotes -e refs/heads -e refs/tags >out &&
	make_user_friendly_and_stable_output <out >actual &&
	cat >expect <<-EOF &&
	<COMMIT-A> refs/t/changes/23/123/1
	<COMMIT-B> refs/t/changes/24/124/2
	EOF
	test_cmp expect actual &&
	git -C workbench update-ref -d refs/t/changes/24/124/1 &&
	git -C workbench update-ref -d refs/t/changes/25/125/2
'
