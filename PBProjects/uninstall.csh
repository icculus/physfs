#!/bin/csh

###
## This script removes the Developer physfs package
###

setenv HOME_DIR ~

sudo -v -p "Enter administrator password to remove physfs: "

sudo rm -rf "$HOME_DIR/Library/Frameworks/physfs.framework"

# will only remove the Frameworks dir if empty (since we put it there)
sudo rmdir "$HOME_DIR/Library/Frameworks"

#sudo rm -r "$HOME_DIR/Readme physfs Developer.txt"
sudo rm -r "/Developer/Documentation/physfs"
sudo rm -r "/Developer/Documentation/ManPages/man1/physfs"*
#sudo rm -r "/Developer/ProjectBuilder Extras/Project Templates/Application/physfs Application"
#sudo rm -r "/Developer/ProjectBuilder Extras/Target Templates/physfs"
sudo rm -r "/Library/Receipts/physfs-devel.pkg"

# rebuild apropos database
sudo /usr/libexec/makewhatis

unsetenv HOME_DIR



