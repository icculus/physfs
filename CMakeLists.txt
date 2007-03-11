# PhysicsFS; a portable, flexible file i/o abstraction.
# Copyright (C) 2007  Ryan C. Gordon.
#
# Please see the file LICENSE in the source's root directory.

PROJECT(PhysicsFS)
SET(PHYSFS_VERSION 1.1.2)

# I hate that they define "WIN32" ... we're about to move to Win64...I hope!
IF(WIN32 AND NOT WINDOWS)
    SET(WINDOWS TRUE)
ENDIF(WIN32 AND NOT WINDOWS)

# Bleh, let's do it for "APPLE" too.
IF(APPLE AND NOT MACOSX)
    SET(MACOSX TRUE)
ENDIF(APPLE AND NOT MACOSX)

INCLUDE(CheckIncludeFile)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckCSourceCompiles)

INCLUDE_DIRECTORIES(.)
#INCLUDE_DIRECTORIES(platform)
#INCLUDE_DIRECTORIES(archivers)

IF(MACOSX)
    # Fallback to older OS X on PowerPC to support wider range of systems...
    IF(CMAKE_OSX_ARCHITECTURES MATCHES ppc)
        ADD_DEFINITIONS(-DMAC_OS_X_VERSION_MIN_REQUIRED=1020)
        SET(OTHER_LDFLAGS ${OTHER_LDFLAGS} " -mmacosx-version-min=10.2")
    ENDIF(CMAKE_OSX_ARCHITECTURES MATCHES ppc)

    # Need these everywhere...
    ADD_DEFINITIONS(-fno-common)
    SET(OTHER_LDFLAGS ${OTHER_LDFLAGS} " -framework Carbon -framework IOKit")
ENDIF(MACOSX)

# Add some gcc-specific command lines.
IF(CMAKE_COMPILER_IS_GNUCC)
    # Always build with debug symbols...you can strip it later.
    ADD_DEFINITIONS(-g -pipe -Wall -Werror -fsigned-char)

    CHECK_C_SOURCE_COMPILES("
        #if ((defined(__GNUC__)) && (__GNUC__ >= 4))
        int main(int argc, char **argv) { int is_gcc4 = 1; return 0; }
        #else
        #error This is not gcc4.
        #endif
    " PHYSFS_IS_GCC4)

    IF(PHYSFS_IS_GCC4)
        ADD_DEFINITIONS(-fvisibility=hidden)
    ENDIF(PHYSFS_IS_GCC4)
ENDIF(CMAKE_COMPILER_IS_GNUCC)


# Basic chunks of source code ...

SET(ZLIB_SRCS
    zlib123/adler32.c
    zlib123/compress.c
    zlib123/crc32.c
    zlib123/deflate.c
    zlib123/gzio.c
    zlib123/infback.c
    zlib123/inffast.c
    zlib123/inflate.c
    zlib123/inftrees.c
    zlib123/trees.c
    zlib123/uncompr.c
    zlib123/zutil.c
)

SET(LZMA_SRCS
    lzma/7zBuffer.c
    lzma/7zCrc.c
    lzma/7zDecode.c
    lzma/7zExtract.c
    lzma/7zHeader.c
    lzma/7zIn.c
    lzma/7zItem.c
    lzma/7zMethodID.c
    lzma/LzmaDecode.c
    lzma/LzmaStateDecode.c
)

SET(PHYSFS_SRCS
    physfs.c
    physfs_byteorder.c
    physfs_unicode.c
    archivers/dir.c
)


# platform layers ...

IF(UNIX)
    SET(PHYSFS_SRCS ${PHYSFS_SRCS} platform/posix.c)
    IF(BEOS)
        SET(PHYSFS_SRCS ${PHYSFS_SRCS} platform/beos.cpp)
        SET(PHYSFS_HAVE_CDROM_SUPPORT TRUE)
    ELSE(BEOS)
        SET(PHYSFS_SRCS ${PHYSFS_SRCS} platform/unix.c)
        # !!! FIXME
        #  AC_DEFINE([PHYSFS_HAVE_LLSEEK], 1, [define if we have llseek])
        CHECK_INCLUDE_FILE(sys/ucred.h HAVE_UCRED_H)
        IF(HAVE_UCRED_H)
            ADD_DEFINITIONS(-DPHYSFS_HAVE_SYS_UCRED_H=1)
            SET(PHYSFS_HAVE_CDROM_SUPPORT TRUE)
        ENDIF(HAVE_UCRED_H)

        CHECK_INCLUDE_FILE(mntent.h HAVE_MNTENT_H)
        IF(HAVE_MNTENT_H)
            ADD_DEFINITIONS(-DPHYSFS_HAVE_MNTENT_H=1)
            SET(PHYSFS_HAVE_CDROM_SUPPORT TRUE)
        ENDIF(HAVE_MNTENT_H)
    ENDIF(BEOS)

    CHECK_INCLUDE_FILE(pthread.h HAVE_PTHREAD_H)
    IF(HAVE_PTHREAD_H)
        SET(PHYSFS_HAVE_THREAD_SUPPORT TRUE)
    ELSE(HAVE_PTHREAD_H)
        ADD_DEFINITIONS(-DPHYSFS_NO_PTHREADS_SUPPORT=1)
    ENDIF(HAVE_PTHREAD_H)
ENDIF(UNIX)

IF(WINDOWS)
    SET(PHYSFS_SRCS ${PHYSFS_SRCS} platform/windows.c)
    SET(PHYSFS_HAVE_CDROM_SUPPORT TRUE)
    SET(PHYSFS_HAVE_THREAD_SUPPORT TRUE)
    # !!! FIXME: platform/pocketpc.c ... ?
ENDIF(WINDOWS)

IF(NOT PHYSFS_HAVE_CDROM_SUPPORT)
    ADD_DEFINITIONS(-DPHYSFS_NO_CDROM_SUPPORT=1)
    MESSAGE(WARNING " ***")
    MESSAGE(WARNING " *** There is no CD-ROM support in this build!")
    MESSAGE(WARNING " *** PhysicsFS will just pretend there are no discs.")
    MESSAGE(WARNING " *** This may be fine, depending on how PhysicsFS is used,")
    MESSAGE(WARNING " ***   but is this what you REALLY wanted?")
    MESSAGE(WARNING " *** (Maybe fix CMakeLists.txt, or write a platform driver?)")
    MESSAGE(WARNING " ***")
ENDIF(NOT PHYSFS_HAVE_CDROM_SUPPORT)

IF(PHYSFS_HAVE_THREAD_SUPPORT)
    ADD_DEFINITIONS(-D_REENTRANT -D_THREAD_SAFE)
ELSE(PHYSFS_HAVE_THREAD_SUPPORT)
    MESSAGE(WARNING " ***")
    MESSAGE(WARNING " *** There is no thread support in this build!")
    MESSAGE(WARNING " *** PhysicsFS will NOT be reentrant!")
    MESSAGE(WARNING " *** This may be fine, depending on how PhysicsFS is used,")
    MESSAGE(WARNING " ***   but is this what you REALLY wanted?")
    MESSAGE(WARNING " *** (Maybe fix CMakeLists.txt, or write a platform driver?)")
    MESSAGE(WARNING " ***")
ENDIF(PHYSFS_HAVE_THREAD_SUPPORT)

# Archivers ...

OPTION(PHYSFS_ARCHIVE_ZIP "Enable ZIP support" TRUE)
IF(PHYSFS_ARCHIVE_ZIP)
    ADD_DEFINITIONS(-DPHYSFS_SUPPORTS_ZIP=1)
    SET(PHYSFS_SRCS ${PHYSFS_SRCS} archivers/zip.c)
    SET(PHYSFS_NEED_ZLIB TRUE)
ENDIF(PHYSFS_ARCHIVE_ZIP)

OPTION(PHYSFS_ARCHIVE_7Z "Enable 7zip support" TRUE)
IF(PHYSFS_ARCHIVE_7Z)
    ADD_DEFINITIONS(-DPHYSFS_SUPPORTS_7Z=1)
    # !!! FIXME: rename to 7z.c?
    SET(PHYSFS_SRCS ${PHYSFS_SRCS} ${LZMA_SRCS} archivers/lzma.c)
    INCLUDE_DIRECTORIES(lzma)
    ADD_DEFINITIONS(-D_LZMA_IN_CB=1)
    ADD_DEFINITIONS(-D_LZMA_PROB32=1)
    ADD_DEFINITIONS(-D_LZMA_SYSTEM_SIZE_T=1)
    ADD_DEFINITIONS(-D_SZ_ONE_DIRECTORY=1)
ENDIF(PHYSFS_ARCHIVE_7Z)

OPTION(PHYSFS_ARCHIVE_GRP "Enable Build Engine GRP support" TRUE)
IF(PHYSFS_ARCHIVE_GRP)
    ADD_DEFINITIONS(-DPHYSFS_SUPPORTS_GRP=1)
    SET(PHYSFS_SRCS ${PHYSFS_SRCS} archivers/grp.c)
ENDIF(PHYSFS_ARCHIVE_GRP)

OPTION(PHYSFS_ARCHIVE_WAD "Enable Doom WAD support" TRUE)
IF(PHYSFS_ARCHIVE_WAD)
    ADD_DEFINITIONS(-DPHYSFS_SUPPORTS_WAD=1)
    SET(PHYSFS_SRCS ${PHYSFS_SRCS} archivers/wad.c)
ENDIF(PHYSFS_ARCHIVE_WAD)

OPTION(PHYSFS_ARCHIVE_HOG "Enable Descent I/II HOG support" TRUE)
IF(PHYSFS_ARCHIVE_HOG)
    ADD_DEFINITIONS(-DPHYSFS_SUPPORTS_HOG=1)
    SET(PHYSFS_SRCS ${PHYSFS_SRCS} archivers/hog.c)
ENDIF(PHYSFS_ARCHIVE_HOG)

OPTION(PHYSFS_ARCHIVE_MVL "Enable Descent I/II MVL support" TRUE)
IF(PHYSFS_ARCHIVE_MVL)
    ADD_DEFINITIONS(-DPHYSFS_SUPPORTS_MVL=1)
    SET(PHYSFS_SRCS ${PHYSFS_SRCS} archivers/mvl.c)
ENDIF(PHYSFS_ARCHIVE_MVL)

OPTION(PHYSFS_ARCHIVE_QPAK "Enable Quake I/II QPAK support" TRUE)
IF(PHYSFS_ARCHIVE_QPAK)
    ADD_DEFINITIONS(-DPHYSFS_SUPPORTS_QPAK=1)
    SET(PHYSFS_SRCS ${PHYSFS_SRCS} archivers/qpak.c)
ENDIF(PHYSFS_ARCHIVE_QPAK)


# See if some archiver required zlib, and see about using system version.

IF(PHYSFS_NEED_ZLIB)
    CHECK_INCLUDE_FILE(zlib.h HAVE_ZLIB_H)
    IF(HAVE_ZLIB_H)
        CHECK_LIBRARY_EXISTS("z" "inflate" "" HAVE_LIBZ)
        IF(HAVE_LIBZ)
            SET(HAVE_SYSTEM_ZLIB TRUE)
        ENDIF(HAVE_LIBZ)
    ENDIF(HAVE_ZLIB_H)

    IF(HAVE_SYSTEM_ZLIB)
        OPTION(PHYSFS_INTERNAL_ZLIB "Link own zlib instead of system library" FALSE)
    ELSE(HAVE_SYSTEM_ZLIB)
        SET(PHYSFS_INTERNAL_ZLIB TRUE)
    ENDIF(HAVE_SYSTEM_ZLIB)

    IF(PHYSFS_INTERNAL_ZLIB)
        INCLUDE_DIRECTORIES(zlib123)
        ADD_DEFINITIONS(-DZ_PREFIX=1)
        SET(PHYSFS_SRCS ${PHYSFS_SRCS} ${ZLIB_SRCS})
    ELSE(PHYSFS_INTERNAL_ZLIB)
        SET(OPTIONAL_LIBRARY_LIBS ${OPTIONAL_LIBRARY_LIBS} z)
    ENDIF(PHYSFS_INTERNAL_ZLIB)
ENDIF(PHYSFS_NEED_ZLIB)

OPTION(PHYSFS_BUILD_STATIC "Build static library" TRUE)
IF(PHYSFS_BUILD_STATIC)
    ADD_LIBRARY(physfs-static STATIC ${PHYSFS_SRCS})
    SET_TARGET_PROPERTIES(physfs-static PROPERTIES OUTPUT_NAME "physfs")
    SET(PHYSFS_LIB_TARGET physfs-static)
ENDIF(PHYSFS_BUILD_STATIC)

OPTION(PHYSFS_BUILD_SHARED "Build shared library" TRUE)
IF(PHYSFS_BUILD_SHARED)
    ADD_LIBRARY(physfs SHARED ${PHYSFS_SRCS})
    TARGET_LINK_LIBRARIES(physfs ${OPTIONAL_LIBRARY_LIBS} ${OTHER_LDFLAGS})
    SET(PHYSFS_LIB_TARGET physfs)
ENDIF(PHYSFS_BUILD_SHARED)

IF(NOT PHYSFS_BUILD_SHARED AND NOT PHYSFS_BUILD_STATIC)
    MESSAGE(FATAL "Both shared and static libraries are disabled!")
ENDIF(NOT PHYSFS_BUILD_SHARED AND NOT PHYSFS_BUILD_STATIC)

# CMake FAQ says I need this...
IF(PHYSFS_BUILD_SHARED AND PHYSFS_BUILD_STATIC)
    SET_TARGET_PROPERTIES(physfs PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    SET_TARGET_PROPERTIES(physfs-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
ENDIF(PHYSFS_BUILD_SHARED AND PHYSFS_BUILD_STATIC)

OPTION(PHYSFS_BUILD_TEST "Build test program." TRUE)
MARK_AS_ADVANCED(PHYSFS_BUILD_TEST)
IF(PHYSFS_BUILD_TEST)
    CHECK_INCLUDE_FILE(readline/readline.h HAVE_READLINE_H)
    CHECK_INCLUDE_FILE(readline/history.h HAVE_HISTORY_H)
    IF(HAVE_READLINE_H AND HAVE_HISTORY_H)
        SET(CMAKE_REQUIRED_LIBRARIES curses)
        CHECK_LIBRARY_EXISTS("readline" "readline" "" HAVE_LIBREADLINE)
        CHECK_LIBRARY_EXISTS("readline" "history" "" HAVE_LIBHISTORY)
        IF(HAVE_LIBREADLINE AND HAVE_LIBHISTORY)
            SET(HAVE_SYSTEM_READLINE TRUE)
            SET(TEST_PHYSFS_LIBS ${TEST_PHYSFS_LIBS} " " readline curses)
            ADD_DEFINITIONS(-DPHYSFS_HAVE_READLINE=1)
        ENDIF(HAVE_LIBREADLINE AND HAVE_LIBHISTORY)
    ENDIF(HAVE_READLINE_H AND HAVE_HISTORY_H)
    ADD_EXECUTABLE(test_physfs test/test_physfs.c)
    TARGET_LINK_LIBRARIES(test_physfs ${PHYSFS_LIB_TARGET} ${TEST_PHYSFS_LIBS} ${OTHER_LDFLAGS})
ENDIF(PHYSFS_BUILD_TEST)

FIND_PACKAGE(Doxygen)
IF(DOXYGEN_FOUND)
    ADD_CUSTOM_TARGET(docs ${DOXYGEN_EXECUTABLE} COMMENT "Building documentation")
ELSE(DOXYGEN_FOUND)
    MESSAGE(STATUS "Doxygen not found. You won't be able to build documentation.")
ENDIF(DOXYGEN_FOUND)


MACRO(MESSAGE_BOOL_OPTION _NAME _VALUE)
    IF(${_VALUE})
        MESSAGE(STATUS "  ${_NAME}: enabled")
    ELSE(${_VALUE})
        MESSAGE(STATUS "  ${_NAME}: disabled")
    ENDIF(${_VALUE})
ENDMACRO(MESSAGE_BOOL_OPTION)

MESSAGE(STATUS "PhysicsFS will build with the following options:")
MESSAGE_BOOL_OPTION("ZIP support" PHYSFS_ARCHIVE_ZIP)
MESSAGE_BOOL_OPTION("7zip support" PHYSFS_ARCHIVE_7Z)
MESSAGE_BOOL_OPTION("GRP support" PHYSFS_ARCHIVE_GRP)
MESSAGE_BOOL_OPTION("WAD support" PHYSFS_ARCHIVE_WAD)
MESSAGE_BOOL_OPTION("HOG support" PHYSFS_ARCHIVE_HOG)
MESSAGE_BOOL_OPTION("MVL support" PHYSFS_ARCHIVE_MVL)
MESSAGE_BOOL_OPTION("QPAK support" PHYSFS_ARCHIVE_QPAK)
MESSAGE_BOOL_OPTION("CD-ROM drive support" PHYSFS_HAVE_CDROM_SUPPORT)
MESSAGE_BOOL_OPTION("Thread safety" PHYSFS_HAVE_THREAD_SUPPORT)
MESSAGE_BOOL_OPTION("Build own zlib" PHYSFS_INTERNAL_ZLIB)
MESSAGE_BOOL_OPTION("Build static library" PHYSFS_BUILD_STATIC)
MESSAGE_BOOL_OPTION("Build shared library" PHYSFS_BUILD_SHARED)
MESSAGE_BOOL_OPTION("Build test program" PHYSFS_BUILD_TEST)
IF(PHYSFS_BUILD_TEST)
    MESSAGE_BOOL_OPTION("  Use readline in test program" HAVE_SYSTEM_READLINE)
ENDIF(PHYSFS_BUILD_TEST)

# end of CMakeLists.txt ...

