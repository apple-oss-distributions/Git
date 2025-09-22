#!/bin/sh

set -e

if test $# -ne 3
then
	echo >&2 "USAGE: $0 <GIT_BUILD_OPTIONS> <INPUT> <OUTPUT>"
	exit 1
fi

GIT_BUILD_OPTIONS="$1"
INPUT="$2"
OUTPUT="$3"

. "$GIT_BUILD_OPTIONS"

sed -e "1s|#!.*python|#!$PYTHON_PATH|" \
    -e 's|\(os\.getenv("GITPYTHONLIB"\)[^)]*)|\1,"@@INSTLIBDIR@@")|' \
    -e 's|"@@INSTLIBDIR@@"|os.path.realpath(os.path.dirname(sys.argv[0])) + "/../../share/git-core/python"|g' \
    "$INPUT" >"$OUTPUT+"
chmod a+x "$OUTPUT+"
mv "$OUTPUT+" "$OUTPUT"
