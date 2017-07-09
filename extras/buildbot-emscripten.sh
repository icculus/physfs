#!/bin/bash

if [ -z "$SDKDIR" ]; then
    SDKDIR="/emsdk_portable"
fi

ENVSCRIPT="$SDKDIR/emsdk_env.sh"
if [ ! -f "$ENVSCRIPT" ]; then
   echo "ERROR: This script expects the Emscripten SDK to be in '$SDKDIR'." 1>&2
   echo "ERROR: Set the \$SDKDIR environment variable to override this." 1>&2
   exit 1
fi

TARBALL="$1"
if [ -z $1 ]; then
    TARBALL=physfs-emscripten.tar.xz
fi

cd `dirname "$0"`
cd ..
PHYSFSBASE=`pwd`

if [ -z "$MAKE" ]; then
    OSTYPE=`uname -s`
    if [ "$OSTYPE" == "Linux" ]; then
        NCPU=`cat /proc/cpuinfo |grep vendor_id |wc -l`
        let NCPU=$NCPU+1
    elif [ "$OSTYPE" = "Darwin" ]; then
        NCPU=`sysctl -n hw.ncpu`
    elif [ "$OSTYPE" = "SunOS" ]; then
        NCPU=`/usr/sbin/psrinfo |wc -l |sed -e 's/^ *//g;s/ *$//g'`
    else
        NCPU=1
    fi

    if [ -z "$NCPU" ]; then
        NCPU=1
    elif [ "$NCPU" = "0" ]; then
        NCPU=1
    fi

    MAKE="make -j$NCPU"
fi

echo "\$MAKE is '$MAKE'"
MAKECMD="$MAKE"
unset MAKE  # prevent warnings about jobserver mode.

echo "Setting up Emscripten SDK environment..."
source "$ENVSCRIPT"

echo "Setting up..."
cd "$PHYSFSBASE"
rm -rf buildbot
mkdir buildbot
cd buildbot

echo "Configuring..."
emcmake cmake -G "Unix Makefiles" -DPHYSFS_BUILD_SHARED=False -DCMAKE_BUILD_TYPE=MinSizeRel .. || exit $?

echo "Building..."
emmake $MAKECMD || exit $?

set -e
rm -rf "$TARBALL" physfs-emscripten
mkdir -p physfs-emscripten
echo "Archiving to '$TARBALL' ..."
cp ../src/physfs.h libphysfs.a physfs-emscripten
chmod -R a+r physfs-emscripten
chmod a+x physfs-emscripten
chmod -R go-w physfs-emscripten
tar -cJvvf "$TARBALL" physfs-emscripten
echo "Done."

exit 0

# end of emscripten-buildbot.sh ...

