#!/bin/sh

# Generic script to create a package with Project Builder in mind
# There should only be one version of this script for all projects!

FRAMEWORK="$1"
VARIANT="$2"

if test "$VARIANT" = "devel" ; then
  PACKAGE="$FRAMEWORK-devel"
  PACKAGE_RESOURCES="pkg-support/devel-resources"	
else
  PACKAGE="$FRAMEWORK"
  PACKAGE_RESOURCES="pkg-support/resources"
fi

echo "Building package for $FRAMEWORK.framework"
echo "Will fetch resources from $PACKAGE_RESOURCES"
echo "Will create the package $PACKAGE.pkg"

# create a copy of the framework
mkdir -p build/pkg-tmp
/Developer/Tools/CpMac -r "build/$FRAMEWORK.framework" build/pkg-tmp/


if test "$VARIANT" = "standard" ; then
  rm -rf "build/pkg-tmp/$FRAMEWORK.framework/Headers"
  rm -rf "build/pkg-tmp/$FRAMEWORK.framework/Versions/Current/Headers"
fi

rm -rf build/pkg-tmp/$FRAMEWORK.framework/Resources/pbdevelopment.plist
rm -rf $PACKAGE_RESOURCES/.DS_Store

# create the .pkg
package build/pkg-tmp "pkg-support/$PACKAGE.info" -d  build -r "$PACKAGE_RESOURCES"

if test "$VARIANT" = "devel" ; then
  # create install scripts
  DIR="build/$PACKAGE.pkg"
  cp "$DIR/install.sh" "$DIR/$PACKAGE.post_install"
  mv "$DIR/install.sh" "$DIR/$PACKAGE.post_upgrade"

  # add execute flag to scripts
  chmod 755 "$DIR/$PACKAGE.post_install" "$DIR/$PACKAGE.post_upgrade"
fi

# remove temporary files
rm -rf build/pkg-tmp

# compress
(cd build; tar -zcvf "$PACKAGE.pkg.tar.gz" "$PACKAGE.pkg")
