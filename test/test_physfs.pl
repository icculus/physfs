#!/usr/bin/perl -w

use strict;
use warnings;
use physfs;

print "testing PhysicsFS Perl bindings...\n";

print "init...\n";
physfs::init("$0") or die("physfs::init('$0'): ".physfs::getLastError()."\n");

print "user dir: ", physfs::getUserDir(), "\n";
print "base dir: ", physfs::getBaseDir(), "\n";
print "pref dir: ", physfs::getPrefDir("icculus.org", "test_physfs"), "\n";

print "deinit...\n";
physfs::deinit();

print "done!\n";
exit 0;

