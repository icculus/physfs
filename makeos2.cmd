@echo off
rem this is a simple batch file to build PhysicsFS on OS/2. You need to have
rem  the EMX development tools installed for this to work.
rem
rem This script (and, indeed, our OS/2 support) could use some tweaking.
rem  Patches go to icculus@clutteredmind.org ...

set CFLAGS=-Wall -Werror -g -Zomf -Zmt -Zmtd -I. -Izlib114 -c -DDEBUG -DOS2 -DPHYSFS_SUPPORTS_ZIP -DPHYSFS_SUPPORTS_GRP

@echo on
mkdir bin

@echo ;don't edit this directly! It is rewritten by makeos2.cmd! > bin\physfs.def
@echo NAME PHYSFS WINDOWCOMPAT >> bin\physfs.def
@echo DESCRIPTION 'PhysicsFS: http://icculus.org/physfs/' >> bin\physfs.def
@echo STACKSIZE 20000 >> bin\physfs.def
@echo BASE=0x10000 >> bin\physfs.def
@echo PROTMODE >> bin\physfs.def


gcc %CFLAGS% -o bin/physfs.obj physfs.c
gcc %CFLAGS% -o bin/physfs_byteorder.obj physfs_byteorder.c
gcc %CFLAGS% -o bin/os2.obj platform/os2.c
gcc %CFLAGS% -o bin/dir.obj archivers/dir.c
gcc %CFLAGS% -o bin/grp.obj archivers/grp.c
gcc %CFLAGS% -o bin/zip.obj archivers/zip.c

gcc %CFLAGS% -o bin/adler32.obj zlib114/adler32.c
gcc %CFLAGS% -o bin/compress.obj zlib114/compress.c
gcc %CFLAGS% -o bin/crc32.obj zlib114/crc32.c
gcc %CFLAGS% -o bin/deflate.obj zlib114/deflate.c
gcc %CFLAGS% -o bin/infblock.obj zlib114/infblock.c
gcc %CFLAGS% -o bin/infcodes.obj zlib114/infcodes.c
gcc %CFLAGS% -o bin/inffast.obj zlib114/inffast.c
gcc %CFLAGS% -o bin/inflate.obj zlib114/inflate.c
gcc %CFLAGS% -o bin/inftrees.obj zlib114/inftrees.c
gcc %CFLAGS% -o bin/infutil.obj zlib114/infutil.c
gcc %CFLAGS% -o bin/trees.obj zlib114/trees.c
gcc %CFLAGS% -o bin/uncompr.obj zlib114/uncompr.c 
gcc %CFLAGS% -o bin/zutil.obj zlib114/zutil.c

gcc %CFLAGS% -o bin/test_physfs.obj test/test_physfs.c

gcc -Zomf -Zmt -Zmtd -o bin/test_physfs.exe bin/*.obj bin/physfs.def

