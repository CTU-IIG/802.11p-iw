#!/bin/bash

VERSION="0.9.1"

(
if head=`git rev-parse --verify HEAD 2>/dev/null`; then
	git update-index --refresh --unmerged > /dev/null
	descr=$(git describe)

	# on git builds check that the version number above
	# is correct...
	[ "${descr/-*/}" == "v$VERSION" ] || exit 2
	
	echo -n '#define IW_VERSION "'
	echo -n "${descr:1}"
	if git diff-index --name-only HEAD | read dummy ; then
		echo -n "-dirty"
	fi
	echo '"'
else
echo "#define IW_VERSION \"$VERSION\""
fi
) > version.h
