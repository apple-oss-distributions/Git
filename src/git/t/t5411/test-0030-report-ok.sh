test_expect_success "setup proc-receive hook (ok, $PROTOCOL)" '
	write_script "$upstream/hooks/proc-receive" <<-EOF
	printf >&2 "# proc-receive hook\n"
	test-tool proc-receive -v \
		-r "ok refs/for/main/topic"
	EOF
'

# Refs of upstream : main(A)
# Refs of workbench: main(A)  tags/v123
# git push         :                       refs/for/main/topic
test_expect_success "proc-receive: ok ($PROTOCOL)" '
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
	remote: # post-receive hook
	remote: post-receive< <ZERO-OID> <COMMIT-A> refs/for/main/topic
	To <URL/of/upstream.git>
	 * [new reference] HEAD -> refs/for/main/topic
	EOF
	test_cmp expect actual &&

	test_cmp_refs -C "$upstream" <<-EOF
	<COMMIT-A> refs/heads/main
	EOF
'
