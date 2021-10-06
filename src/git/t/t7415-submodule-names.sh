#!/bin/sh

test_description='check handling of .. in submodule names

Exercise the name-checking function on a variety of names, and then give a
real-world setup that confirms we catch this in practice.
'
. ./test-lib.sh

test_expect_success 'check names' '
	cat >expect <<-\EOF &&
	valid
	valid/with/paths
	EOF

	git submodule--helper check-name >actual <<-\EOF &&
	valid
	valid/with/paths

	../foo
	/../foo
	..\foo
	\..\foo
	foo/..
	foo/../
	foo\..
	foo\..\
	foo/../bar
	EOF

	test_cmp expect actual
'

test_expect_success 'create innocent subrepo' '
	git init innocent &&
	git -C innocent commit --allow-empty -m foo
'

test_expect_success 'submodule add refuses invalid names' '
	test_must_fail \
		git submodule add --name ../../modules/evil "$PWD/innocent" evil
'

test_expect_success 'add evil submodule' '
	git submodule add "$PWD/innocent" evil &&

	mkdir modules &&
	cp -r .git/modules/evil modules &&
	write_script modules/evil/hooks/post-checkout <<-\EOF &&
	echo >&2 "RUNNING POST CHECKOUT"
	EOF

	git config -f .gitmodules submodule.evil.update checkout &&
	git config -f .gitmodules --rename-section \
		submodule.evil submodule.../../modules/evil &&
	git add modules &&
	git commit -am evil
'

# This step seems like it shouldn't be necessary, since the payload is
# contained entirely in the evil submodule. But due to the vagaries of the
# submodule code, checking out the evil module will fail unless ".git/modules"
# exists. Adding another submodule (with a name that sorts before "evil") is an
# easy way to make sure this is the case in the victim clone.
test_expect_success 'add other submodule' '
	git submodule add "$PWD/innocent" another-module &&
	git add another-module &&
	git commit -am another
'

test_expect_success 'clone evil superproject' '
	git clone --recurse-submodules . victim >output 2>&1 &&
	! grep "RUNNING POST CHECKOUT" output
'

test_done
