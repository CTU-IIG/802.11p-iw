#!/bin/sh

VERSION="0.9.9"
OUT="version.h"

if head=`git rev-parse --verify HEAD 2>/dev/null`; then
	git update-index --refresh --unmerged > /dev/null
	descr=$(git describe)

	# on git builds check that the version number above
	# is correct...
	[ "${descr%%-*}" = "v$VERSION" ] || exit 2
	
	echo -n '#define IW_VERSION "' > "$OUT"
	echo -n "${descr#v}" >> "$OUT"
	if git diff-index --name-only HEAD | read dummy ; then
		echo -n "-dirty" >> "$OUT"
	fi
	echo '"' >> "$OUT"
else
echo "#define IW_VERSION \"$VERSION-nogit\"" > "$OUT"
fi
