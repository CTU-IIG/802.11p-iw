#!/bin/sh

VERSION="1.0"

(
echo "#define VERSION \"$VERSION\""
if head=`git rev-parse --verify HEAD 2>/dev/null`; then
	git update-index --refresh --unmerged > /dev/null
	printf "#define IW_GIT_VERSION \"-g%.8s" "$head"
	if git diff-index --name-only HEAD | read dummy ; then
		printf -- "-dirty"
	fi
	echo '"'
else
	echo '#define IW_GIT_VERSION ""'
fi
) > version.h
