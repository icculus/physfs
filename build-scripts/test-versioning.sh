#!/bin/sh
# Copyright 2022 Collabora Ltd.
# SPDX-License-Identifier: Zlib

set -eu

cd `dirname $0`/..

# Needed so sed doesn't report illegal byte sequences on macOS
export LC_CTYPE=C

header=src/physfs.h
ref_major=$(sed -ne 's/^#define PHYSFS_VER_MAJOR  *//p' $header)
ref_minor=$(sed -ne 's/^#define PHYSFS_VER_MINOR  *//p' $header)
ref_micro=$(sed -ne 's/^#define PHYSFS_VER_PATCH  *//p' $header)
ref_version="${ref_major}.${ref_minor}.${ref_micro}"

tests=0
failed=0

ok () {
    tests=$(( tests + 1 ))
    echo "ok - $*"
}

not_ok () {
    tests=$(( tests + 1 ))
    echo "not ok - $*"
    failed=1
}

version=$(sed -ne 's/^set(PHYSFS_VERSION \([0-9.]*\))$/\1/p' CMakeLists.txt)

if [ "$ref_version" = "$version" ]; then
    ok "CMakeLists.txt $version"
else
    not_ok "CMakeLists.txt $version disagrees with physfs.h $ref_version"
fi

version=$(sed -ne 's/^VERSION = \([0-9.]*\)$/\1/p' src/Makefile.os2)

if [ "$ref_version" = "$version" ]; then
    ok "src/Makefile.os $version"
else
    not_ok "src/Makefile.os $version disagrees with physfs.h $ref_version"
fi

for rcfile in src/physfs_version.rc; do
    tuple=$(sed -ne 's/^ *FILEVERSION *//p' "$rcfile" | tr -d '\r')
    ref_tuple="${ref_major},${ref_minor},${ref_micro},0"

    if [ "$ref_tuple" = "$tuple" ]; then
        ok "$rcfile FILEVERSION $tuple"
    else
        not_ok "$rcfile FILEVERSION $tuple disagrees with physfs.h $ref_tuple"
    fi

    tuple=$(sed -ne 's/^ *PRODUCTVERSION *//p' "$rcfile" | tr -d '\r')

    if [ "$ref_tuple" = "$tuple" ]; then
        ok "$rcfile PRODUCTVERSION $tuple"
    else
        not_ok "$rcfile PRODUCTVERSION $tuple disagrees with physfs.h $ref_tuple"
    fi

    tuple=$(sed -Ene 's/^ *VALUE "FileVersion", "([0-9, ]*)\\0"\r?$/\1/p' "$rcfile" | tr -d '\r')
    ref_tuple="${ref_major}, ${ref_minor}, ${ref_micro}, 0"

    if [ "$ref_tuple" = "$tuple" ]; then
        ok "$rcfile FileVersion $tuple"
    else
        not_ok "$rcfile FileVersion $tuple disagrees with physfs.h $ref_tuple"
    fi

    tuple=$(sed -Ene 's/^ *VALUE "ProductVersion", "([0-9, ]*)\\0"\r?$/\1/p' "$rcfile" | tr -d '\r')

    if [ "$ref_tuple" = "$tuple" ]; then
        ok "$rcfile ProductVersion $tuple"
    else
        not_ok "$rcfile ProductVersion $tuple disagrees with physfs.h $ref_tuple"
    fi
done

echo "1..$tests"
exit "$failed"
