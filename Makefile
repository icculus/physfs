#-----------------------------------------------------------------------------#
# PhysicsFS -- A filesystem abstraction.
#
#  Please see the file LICENSE in the source's root directory.
#   This file written by Ryan C. Gordon.
#-----------------------------------------------------------------------------#


#-----------------------------------------------------------------------------#
# Makefile for building PhysicsFS on Unix-like systems. Follow the
#  instructions for editing this file, then run "make" on the command line.
#-----------------------------------------------------------------------------#


#-----------------------------------------------------------------------------#
# Set to your liking.
#-----------------------------------------------------------------------------#
CC = gcc
LINKER = gcc


#-----------------------------------------------------------------------------#
# If this makefile fails to detect Cygwin correctly, or you want to force
#  the build process's behaviour, set it to "true" or "false" (w/o quotes).
#-----------------------------------------------------------------------------#
#cygwin := true
#cygwin := false
cygwin := autodetect

#-----------------------------------------------------------------------------#
# Set this to true if you want to create a debug build.
#-----------------------------------------------------------------------------#
#debugging := false
debugging := true

#-----------------------------------------------------------------------------#
# Set the archive types you'd like to support.
#  Note that various archives may need external libraries.
#-----------------------------------------------------------------------------#
use_archive_zip := true
use_archive_grp := true

#-----------------------------------------------------------------------------#
# Set to "true" if you'd like to build a DLL. Set to "false" otherwise.
#-----------------------------------------------------------------------------#
#build_dll := false
build_dll := true

#-----------------------------------------------------------------------------#
# Set one of the below. Currently, none of these are used.
#-----------------------------------------------------------------------------#
#use_asm = -DUSE_I386_ASM
use_asm = -DUSE_PORTABLE_C


#-----------------------------------------------------------------------------#
# Set this to where you want PhysicsFS installed. It will put the
#  files in $(install_prefix)/bin, $(install_prefix)/lib, and
#  $(install_prefix)/include  ...
#-----------------------------------------------------------------------------#
install_prefix := /usr/local


#-----------------------------------------------------------------------------#
#-----------------------------------------------------------------------------#
#-----------------------------------------------------------------------------#
#-----------------------------------------------------------------------------#
# Everything below this line is probably okay.
#-----------------------------------------------------------------------------#
#-----------------------------------------------------------------------------#
#-----------------------------------------------------------------------------#
#-----------------------------------------------------------------------------#

#-----------------------------------------------------------------------------#
# CygWin autodetect.
#-----------------------------------------------------------------------------#

ifeq ($(strip $(cygwin)),autodetect)
  ifneq ($(strip $(shell gcc -v 2>&1 |grep "cygwin")),)
    cygwin := true
  else
    cygwin := false
  endif
endif


#-----------------------------------------------------------------------------#
# Platform-specific binary stuff.
#-----------------------------------------------------------------------------#

ifeq ($(strip $(cygwin)),true)
  # !!! FIXME
  build_dll := false
  # !!! FIXME

  ASM = nasmw
  EXE_EXT = .exe
  DLL_EXT = .dll
  STATICLIB_EXT = .a
  ASMOBJFMT = win32
  ASMDEFS = -dC_IDENTIFIERS_UNDERSCORED
  CFLAGS += -DC_IDENTIFIERS_UNDERSCORED
else
  ASM = nasm
  EXE_EXT =
  DLL_EXT = .so
  STATICLIB_EXT = .a
  ASMOBJFMT = elf
endif

ifeq ($(strip $(build_dll)),true)
LIB_EXT := $(DLL_EXT)
SHAREDFLAGS += -shared
else
LIB_EXT := $(STATICLIB_EXT)
endif

#-----------------------------------------------------------------------------#
# Version crapola.
#-----------------------------------------------------------------------------#
VERMAJOR := $(shell grep "define PHYSFS_VER_MAJOR" physfs.h | sed "s/\#define PHYSFS_VER_MAJOR //")
VERMINOR := $(shell grep "define PHYSFS_VER_MINOR" physfs.h | sed "s/\#define PHYSFS_VER_MINOR //")
VERPATCH := $(shell grep "define PHYSFS_VER_PATCH" physfs.h | sed "s/\#define PHYSFS_VER_PATCH //")

VERMAJOR := $(strip $(VERMAJOR))
VERMINOR := $(strip $(VERMINOR))
VERPATCH := $(strip $(VERPATCH))

VERFULL := $(VERMAJOR).$(VERMINOR).$(VERPATCH)

#-----------------------------------------------------------------------------#
# General compiler, assembler, and linker flags.
#-----------------------------------------------------------------------------#

BINDIR := bin
SRCDIR := .

CFLAGS += $(use_asm) -I$(SRCDIR) -I/usr/include/readline -D_REENTRANT -fsigned-char -DPLATFORM_UNIX
CFLAGS += -Wall -Werror -fno-exceptions -fno-rtti -ansi -pedantic

LDFLAGS += -lm

ifeq ($(strip $(debugging)),true)
  CFLAGS += -DDEBUG -g -fno-omit-frame-pointer
  LDFLAGS += -g -fno-omit-frame-pointer
else
  CFLAGS += -DNDEBUG -O2 -fomit-frame-pointer
  LDFLAGS += -O2 -fomit-frame-pointer
endif

ASMFLAGS := -f $(ASMOBJFMT) $(ASMDEFS)

TESTLDFLAGS := -lreadline

#-----------------------------------------------------------------------------#
# Source and target names.
#-----------------------------------------------------------------------------#

BASELIBNAME := physfs
ifneq ($(strip $(cygwin)),true)
BASELIBNAME := lib$(strip $(BASELIBNAME))
endif

MAINLIB := $(BINDIR)/$(strip $(BASELIBNAME))$(strip $(LIB_EXT))

TESTSRCS := test/test_physfs.c

MAINSRCS := physfs.c archivers/dir.c

ifeq ($(strip $(use_archive_zip)),true)
MAINSRCS += archivers/zip.c archivers/unzip.c
CFLAGS += -DPHYSFS_SUPPORTS_ZIP
LDFLAGS += -lz
endif

ifeq ($(strip $(use_archive_grp)),true)
MAINSRCS += archivers/grp.c
CFLAGS += -DPHYSFS_SUPPORTS_GRP
endif

ifeq ($(strip $(cygwin)),true)
MAINSRCS += platform/win32.c
else
MAINSRCS += platform/unix.c
endif

TESTEXE := $(BINDIR)/test_physfs$(EXE_EXT)

# Rule for getting list of objects from source
MAINOBJS1 := $(MAINSRCS:.c=.o)
MAINOBJS2 := $(MAINOBJS1:.cpp=.o)
MAINOBJS3 := $(MAINOBJS2:.asm=.o)
MAINOBJS := $(foreach f,$(MAINOBJS3),$(BINDIR)/$(f))
MAINSRCS := $(foreach f,$(MAINSRCS),$(SRCDIR)/$(f))

TESTOBJS1 := $(TESTSRCS:.c=.o)
TESTOBJS2 := $(TESTOBJS1:.cpp=.o)
TESTOBJS3 := $(TESTOBJS2:.asm=.o)
TESTOBJS := $(foreach f,$(TESTOBJS3),$(BINDIR)/$(f))
TESTSRCS := $(foreach f,$(TESTSRCS),$(SRCDIR)/$(f))

CLEANUP = $(wildcard *.exe) $(wildcard *.obj) \
          $(wildcard $(BINDIR)/*.exe) $(wildcard $(BINDIR)/*.obj) \
          $(wildcard *~) $(wildcard *.err) \
          $(wildcard .\#*) core


#-----------------------------------------------------------------------------#
# Rules.
#-----------------------------------------------------------------------------#

# Rules for turning source files into .o files
$(BINDIR)/%.o: $(SRCDIR)/%.cpp
	$(CC) -c -o $@ $< $(CFLAGS)

$(BINDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(BINDIR)/%.o: $(SRCDIR)/%.asm
	$(ASM) $(ASMFLAGS) -o $@ $<

.PHONY: all clean distclean listobjs install

all: $(BINDIR) $(MAINLIB) $(TESTEXE)

$(MAINLIB) : $(BINDIR) $(MAINOBJS)
	$(LINKER) -o $(MAINLIB) $(LDFLAGS) $(SHAREDFLAGS) $(MAINOBJS)

$(TESTEXE) : $(MAINLIB) $(TESTOBJS)
	$(LINKER) -o $(TESTEXE) $(LDFLAGS) $(TESTLDFLAGS) $(TESTOBJS) $(MAINLIB)


install: all
	mkdir -p $(install_prefix)/bin
	mkdir -p $(install_prefix)/lib
	mkdir -p $(install_prefix)/include
	cp $(SRCDIR)/physfs.h $(install_prefix)/include
	cp $(TESTEXE) $(install_prefix)/bin
ifeq ($(strip $(cygwin)),true)
	cp $(MAINLIB) $(install_prefix)/lib/$(strip $(BASELIBNAME))$(strip $(LIB_EXT))
else
	cp $(MAINLIB) $(install_prefix)/lib/$(strip $(BASELIBNAME))$(strip $(LIB_EXT)).$(strip $(VERFULL))
	ln -sf $(strip $(BASELIBNAME))$(strip $(LIB_EXT)).$(strip $(VERFULL)) $(install_prefix)/lib/$(strip $(BASELIBNAME))$(strip $(LIB_EXT))
	ln -sf $(strip $(BASELIBNAME))$(strip $(LIB_EXT)).$(strip $(VERFULL)) $(install_prefix)/lib/$(strip $(BASELIBNAME))$(strip $(LIB_EXT)).$(strip $(VERMAJOR))
	chmod 0755 $(install_prefix)/lib/$(strip $(BASELIBNAME))$(strip $(LIB_EXT)).$(strip $(VERFULL))
	chmod 0644 $(install_prefix)/include/physfs.h
endif

$(BINDIR):
	mkdir -p $(BINDIR)
	mkdir -p $(BINDIR)/archivers
	mkdir -p $(BINDIR)/platform
	mkdir -p $(BINDIR)/test

distclean: clean

clean:
	rm -f $(CLEANUP)
	rm -rf $(BINDIR)

listobjs:
	@echo SOURCES:
	@echo $(MAINSRCS)
	@echo
	@echo OBJECTS:
	@echo $(MAINOBJS)
	@echo
	@echo BINARIES:
	@echo $(MAINLIB)

showcfg:
	@echo "Using CygWin   : $(cygwin)"
	@echo "Debugging      : $(debugging)"
	@echo "ASM flag       : $(use_asm)"
	@echo "Building DLLs  : $(build_dll)"
	@echo "Install prefix : $(install_prefix)"
	@echo "PhysFS version : $(VERFULL)"
	@echo "Supports .GRP  : $(use_archive_grp)"
	@echo "Supports .ZIP  : $(use_archive_zip)"

#-----------------------------------------------------------------------------#
# This section is pretty much just for Ryan's use to make distributions.
#  You Probably Should Not Touch.
#-----------------------------------------------------------------------------#

# These are the files needed in a binary distribution, regardless of what
#  platform is being used.
BINSCOMMON := LICENSE.TXT physfs.h

.PHONY: package msbins win32bins nocygwin
package: clean
	cd .. ; mv physfs physfs-$(VERFULL) ; tar -cyvvf ./physfs-$(VERFULL).tar.bz2 --exclude="*CVS*" physfs-$(VERFULL) ; mv physfs-$(VERFULL) physfs


ifeq ($(strip $(cygwin)),true)
msbins: win32bins

win32bins: clean all
	echo -e "\r\n\r\n\r\nHEY YOU.\r\n\r\n\r\nTake a look at README-win32bins.txt FIRST.\r\n\r\n\r\n--ryan. (icculus@clutteredmind.org)\r\n\r\n" |zip -9rz ../physfs-win32bins-$(shell date +%m%d%Y).zip $(MAINLIB) $(EXTRAPACKAGELIBS) README-win32bins.txt

else

msbins: nocygwin
win32bins: nocygwin

nocygwin:
	@echo This must be done on a Windows box in the Cygwin environment.

endif

#-----------------------------------------------------------------------------#
# That's all, folks.
#-----------------------------------------------------------------------------#

# end of Makefile ...
