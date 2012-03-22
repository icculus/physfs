#!/usr/bin/ruby

require 'physfs'

puts "testing PhysicsFS Ruby bindings..."
puts "init..."

Physfs.init($0)

puts "user dir: " + Physfs.getUserDir()
puts "base dir: " + Physfs.getBaseDir()
puts "pref dir: " + Physfs.getPrefDir("icculus.org", "test_physfs")

puts "deinit..."
Physfs.deinit()

puts "done!"
exit(0)

