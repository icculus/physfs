#!/bin/csh

###
## This script creates "PBProjects.tar.gz" in the parent directory
###

# remove build products
rm -rf build

# remove Finder info files
find . -name ".DS_Store" -exec rm "{}" ";"

# remove user project prefs
find . -name "*.pbxuser" -exec rm "{}" ";"

# create the archive
(cd .. && gnutar -zcvf PBProjects.tar.gz PBProjects)



