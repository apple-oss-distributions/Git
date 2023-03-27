#!/bin/sh
#
# Copyright (c) 2007 Johannes Sixt
#

test_description='git checkout-index on filesystem w/o symlinks test.

This tests that git checkout-index creates a symbolic link as a plain
file if core.symlinks is false.'

. ./test-lib.sh

test_expect_success \
'preparation' '
git config core.symlinks false &&
l=$(printf file | git hash-object -t blob -w --stdin) &&
echo "120000 $l	symlink" | git update-index --index-info'

test_expect_success \
'the checked-out symlink must be a file' '
git checkout-index symlink &&
test -f symlink'

test_expect_success \
'the file must be the blob we added during the setup' '
test "$(git hash-object -t blob symlink)" = $l'

for mode in 'case' 'utf-8'
do
	case "$mode" in
	case)	dir='A' symlink='a' mode_prereq='CASE_INSENSITIVE_FS' ;;
	utf-8)
		dir=$(printf "\141\314\210") symlink=$(printf "\303\244")
		mode_prereq='UTF8_NFD_TO_NFC' ;;
	esac

	test_expect_success SYMLINKS,$mode_prereq \
	"checkout-index with $mode-collision don't write to the wrong place" '
		git init $mode-collision &&
		(
			cd $mode-collision &&
			mkdir target-dir &&

			empty_obj_hex=$(git hash-object -w --stdin </dev/null) &&
			symlink_hex=$(printf "%s" "$PWD/target-dir" | git hash-object -w --stdin) &&

			cat >objs <<-EOF &&
			100644 blob ${empty_obj_hex}	${dir}/x
			100644 blob ${empty_obj_hex}	${dir}/y
			100644 blob ${empty_obj_hex}	${dir}/z
			120000 blob ${symlink_hex}	${symlink}
			EOF

			git update-index --index-info <objs &&

			# Note: the order is important here to exercise the
			# case where the file at ${dir} has its type changed by
			# the time Git tries to check out ${dir}/z.
			#
			# Also, we use core.precomposeUnicode=false because we
			# want Git to treat the UTF-8 paths transparently on
			# Mac OS, matching what is in the index.
			#
			git -c core.precomposeUnicode=false checkout-index -f \
				${dir}/x ${dir}/y ${symlink} ${dir}/z &&

			# Should not create ${dir}/z at ${symlink}/z
			test_path_is_missing target-dir/z

		)
	'
done

test_done
