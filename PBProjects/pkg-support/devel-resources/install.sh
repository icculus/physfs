#!/bin/sh
# finish up the installation
# this script should be executed using the sudo command
# this file is copied to physfs.post_install and physfs.post_upgrade
# inside the .pkg bundle
echo "Running post-install script"
umask 022

ROOT=/Developer/Documentation/SDL

echo "Moving physfs.framework to ~/Library/Frameworks"
# move SDL to its proper home, so the target stationary works
mkdir -p ~/Library/Frameworks
/Developer/Tools/CpMac -r $ROOT/physfs.framework ~/Library/Frameworks
rm -rf $ROOT/physfs.framework

echo "Precompiling Header"
# precompile header for speedier compiles
/usr/bin/cc -I $HOME/Library/Frameworks/SDL.framework/Headers -precomp ~/Library/Frameworks/physfs.framework/Headers/physfs.h -o ~/Library/Frameworks/physfs.framework/Headers/physfs.p
