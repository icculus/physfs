if BUILD_TEST_PHYSFS

bin_PROGRAMS = test_physfs

INCLUDES = -I$(top_srcdir)

if BUILD_READLINE
test_physfs_LDFLAGS="-lreadline -lcurses"
endif

test_physfs_LDADD = ../libphysfs.la
test_physfs_SOURCES = test_physfs.c

endif
