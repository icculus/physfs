# PhysicsFS; a portable, flexible file i/o abstraction.
# Copyright (C) 2007  Ryan C. Gordon.
#
# Please see the file LICENSE.txt in the source's root directory.

CMAKE_MINIMUM_REQUIRED(VERSION 2.4)

PROJECT(PhysicsFS)
SET(PHYSFS_VERSION 2.0.0)

# Increment this if/when we break backwards compatibility.
SET(PHYSFS_SOVERSION 1)

# I hate that they define "WIN32" ... we're about to move to Win64...I hope!
IF(WIN32 AND NOT WINDOWS)
    SET(WINDOWS TRUE)
ENDIF(WIN32 AND NOT WINDOWS)

# Bleh, let's do it for "APPLE" too.
IF(APPLE AND NOT MACOSX)
    SET(MACOSX TRUE)
ENDIF(APPLE AND NOT MACOSX)

# And this might be wrong...
IF (CMAKE_SYSTEM MATCHES OS2)
    SET(OS2 TRUE)
ENDIF (CMAKE_SYSTEM MATCHES OS2)

IF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    SET(SOLARIS TRUE)
ENDIF(CMAKE_SYSTEM_NAME STREQUAL "SunOS")

INCLUDE(CheckIncludeFile)
INCLUDE(CheckLibraryExists)
INCLUDE(CheckCSourceCompiles)

INCLUDE_DIRECTORIES(./src)

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
    ADD_DEFINITIONS(-g -pipe -Werror -fsigned-char)

    # Stupid BeOS generates warnings in the system headers.
    IF(NOT BEOS)
        ADD_DEFINITIONS(-Wall)
    ENDIF(NOT BEOS)

    CHECK_C_SOURCE_COMPILES("
        #if ((defined(__GNUC__)) && (__GNUC__ >= 4))
        int main(int argc, char **argv) { int is_gcc4 = 1; return 0; }
        #else
        #error This is not gcc4.
        #endif
    " PHYSFS_IS_GCC4)

    IF(PHYSFS_IS_GCC4)
        # Not supported on several operating systems at this time.
        IF(NOT OS2 AND NOT SOLARIS AND NOT WINDOWS) 
            ADD_DEFINITIONS(-fvisibility=hidden)
        ENDIF(NOT OS2 AND NOT SOLARIS AND NOT WINDOWS)
    ENDIF(PHYSFS_IS_GCC4)

    # Don't use -rpath.
    SET(CMAKE_SKIP_RPATH ON CACHE BOOL "Skip RPATH" FORCE)
ENDIF(CMAKE_COMPILER_IS_GNUCC)

IF(CMAKE_C_COMPILER_ID STREQUAL "SunPro")
    ADD_DEFINITIONS(-erroff=E_EMPTY_TRANSLATION_UNIT)
    ADD_DEFINITIONS(-xldscope=hidden)
ENDIF(CMAKE_C_COMPILER_ID STREQUAL "SunPro")

IF(MSVC)
    # VS.NET 8.0 got really really anal about strcpy, etc, which even if we
    #  cleaned up our code, zlib, etc still use...so disable the warning.
    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS=1)
ENDIF(MSVC)

# Basic chunks of source code ...

SET(ZLIB_SRCS
    src/zlib123/adler32.c
    src/zlib123/compress.c
    src/zlib123/crc32.c
    src/zlib123/deflate.c
    src/zlib123/gzio.c
    src/zlib123/infback.c
    src/zlib123/inffast.c
    src/zlib123/inflate.c
    src/zlib123/inftrees.c
    src/zlib123/trees.c
    src/zlib123/uncompr.c
    src/zlib123/zutil.c
)

SET(LZMA_SRCS
    src/lzma/C/7zCrc.c
    src/lzma/C/Archive/7z/7zBuffer.c
    src/lzma/C/Archive/7z/7zDecode.c
    src/lzma/C/Archive/7z/7zExtract.c
    src/lzma/C/Archive/7z/7zHeader.c
    src/lzma/C/Archive/7z/7zIn.c
    src/lzma/C/Archive/7z/7zItem.c
    src/lzma/C/Archive/7z/7zMethodID.c
    src/lzma/C/Compress/Branch/BranchX86.c
    src/lzma/C/Compress/Branch/BranchX86_2.c
    src/lzma/C/Compress/Lzma/LzmaDecode.c
)

IF(BEOS)
    # We add this explicitly, since we don't want CMake to think this
    #  is a C++ project unless we're on BeOS.
    SET(PHYSFS_BEOS_SRCS src/platform_beos.cpp)
    FIND_LIBRARY(BE_LIBRARY be)
    FIND_LIBRARY(ROOT_LIBRARY root)
    SET(OPTIONAL_LIBRARY_LIBS ${OPTIONAL_LIBRARY_LIBS} ${BE_LIBRARY} ${ROOT_LIBRARY})
ENDIF(BEOS)

# Almost everything is "compiled" here, but things that don't apply to the
#  build are #ifdef'd out. This is to make it easy to embed PhysicsFS into
#  another project or bring up a new build system: just compile all the source
#  code and #define the things you want.
SET(PHYSFS_SRCS
    src/physfs.c
    src/physfs_byteorder.c
    src/physfs_unicode.c
    src/platform_os2.c
    src/platform_pocketpc.c
    src/platform_posix.c
    src/platform_unix.c
    src/platform_macosx.c
    src/platform_windows.c
    src/archiver_dir.c
    src/archiver_unpacked.c
    src/archiver_grp.c
    src/archiver_hog.c
    src/archiver_lzma.c
    src/archiver_mvl.c
    src/archiver_qpak.c
    src/archiver_wad.c
    src/archiver_zip.c
    src/archiver_iso9660.c
    ${PHYSFS_BEOS_SRCS}
)


# platform layers ...

IF(UNIX)
    IF(BEOS)
        SET(PHYSFS_HAVE_CDROM_SUPPORT TRUE)
        SET(PHYSFS_HAVE_THREAD_SUPPORT TRUE)
        SET(HAVE_PTHREAD_H TRUE)
    ELSE(BEOS)
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

        # !!! FIXME: Solaris fails this, because mnttab.h implicitly
        # !!! FIXME:  depends on other system headers.  :(
        #CHECK_INCLUDE_FILE(sys/mnttab.h HAVE_SYS_MNTTAB_H)
        CHECK_C_SOURCE_COMPILES("
            #include <stdio.h>
            #include <sys/mnttab.h>
            int main(int argc, char **argv) { return 0; }
        " HAVE_SYS_MNTTAB_H)

        IF(HAVE_SYS_MNTTAB_H)
            ADD_DEFINITIONS(-DPHYSFS_HAVE_SYS_MNTTAB_H=1)
            SET(PHYSFS_HAVE_CDROM_SUPPORT TRUE)
        ENDIF(HAVE_SYS_MNTTAB_H)

        CHECK_INCLUDE_FILE(pthread.h HAVE_PTHREAD_H)
        IF(HAVE_PTHREAD_H)
            SET(PHYSFS_HAVE_THREAD_SUPPORT TRUE)
        ENDIF(HAVE_PTHREAD_H)
    ENDIF(BEOS)
ENDIF(UNIX)

IF(WINDOWS OR OS2)
    SET(PHYSFS_HAVE_CDROM_SUPPORT TRUE)
    SET(PHYSFS_HAVE_THREAD_SUPPORT TRUE)
ENDIF(WINDOWS OR OS2)

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
    ADD_DEFINITIONS(-DPHYSFS_NO_THREAD_SUPPORT=1)
    MESSAGE(WARNING " ***")
    MESSAGE(WARNING " *** There is no thread support in this build!")
    MESSAGE(WARNING " *** PhysicsFS will NOT be reentrant!")
    MESSAGE(WARNING " *** This may be fine, depending on how PhysicsFS is used,")
    MESSAGE(WARNING " ***   but is this what you REALLY wanted?")
    MESSAGE(WARNING " *** (Maybe fix CMakeLists.txt, or write a platform driver?)")
    MESSAGE(WARNING " ***")
ENDIF(PHYSFS_HAVE_THREAD_SUPPORT)

CHECK_INCLUDE_FILE(assert.h HAVE_ASSERT_H)
IF(HAVE_ASSERT_H)
    ADD_DEFINITIONS(-DHAVE_ASSERT_H=1)
ENDIF(HAVE_ASSERT_H)



# Archivers ...

OPTION(PHYSFS_ARCHIVE_ZIP "Enable ZIP support" TRUE)
IF(PHYSFS_ARCHIVE_ZIP)
    ADD_DEFINITIONS(-DPHYSFS_SUPPORTS_ZIP=1)
    SET(PHYSFS_NEED_ZLIB TRUE)
ENDIF(PHYSFS_ARCHIVE_ZIP)

OPTION(PHYSFS_ARCHIVE_7Z "Enable 7zip support" TRUE)
IF(PHYSFS_ARCHIVE_7Z)
    ADD_DEFINITIONS(-DPHYSFS_SUPPORTS_7Z=1)
    # !!! FIXME: rename to 7z.c?
    SET(PHYSFS_SRCS ${PHYSFS_SRCS} ${LZMA_SRCS})
ENDIF(PHYSFS_ARCHIVE_7Z)

OPTION(PHYSFS_ARCHIVE_GRP "Enable Build Engine GRP support" TRUE)
IF(PHYSFS_ARCHIVE_GRP)
    ADD_DEFINITIONS(-DPHYSFS_SUPPORTS_GRP=1)
ENDIF(PHYSFS_ARCHIVE_GRP)

OPTION(PHYSFS_ARCHIVE_WAD "Enable Doom WAD support" TRUE)
IF(PHYSFS_ARCHIVE_WAD)
    ADD_DEFINITIONS(-DPHYSFS_SUPPORTS_WAD=1)
ENDIF(PHYSFS_ARCHIVE_WAD)

OPTION(PHYSFS_ARCHIVE_HOG "Enable Descent I/II HOG support" TRUE)
IF(PHYSFS_ARCHIVE_HOG)
    ADD_DEFINITIONS(-DPHYSFS_SUPPORTS_HOG=1)
ENDIF(PHYSFS_ARCHIVE_HOG)

OPTION(PHYSFS_ARCHIVE_MVL "Enable Descent I/II MVL support" TRUE)
IF(PHYSFS_ARCHIVE_MVL)
    ADD_DEFINITIONS(-DPHYSFS_SUPPORTS_MVL=1)
ENDIF(PHYSFS_ARCHIVE_MVL)

OPTION(PHYSFS_ARCHIVE_QPAK "Enable Quake I/II QPAK support" TRUE)
IF(PHYSFS_ARCHIVE_QPAK)
    ADD_DEFINITIONS(-DPHYSFS_SUPPORTS_QPAK=1)
ENDIF(PHYSFS_ARCHIVE_QPAK)

OPTION(PHYSFS_ARCHIVE_ISO9660 "Enable ISO9660 support" TRUE)
IF(PHYSFS_ARCHIVE_ISO9660)
    ADD_DEFINITIONS(-DPHYSFS_SUPPORTS_ISO9660=1)
ENDIF(PHYSFS_ARCHIVE_ISO9660)


# See if some archiver required zlib, and see about using system version.

IF(PHYSFS_NEED_ZLIB)
    FIND_PACKAGE(ZLIB)

    IF(ZLIB_FOUND)
        OPTION(PHYSFS_INTERNAL_ZLIB "Link own zlib instead of system library" FALSE)
    ELSE(HAVE_SYSTEM_ZLIB)
        SET(PHYSFS_INTERNAL_ZLIB TRUE)
    ENDIF(ZLIB_FOUND)

    IF(PHYSFS_INTERNAL_ZLIB)
        INCLUDE_DIRECTORIES(./src/zlib123)
        ADD_DEFINITIONS(-DZ_PREFIX=1)
        SET(PHYSFS_SRCS ${PHYSFS_SRCS} ${ZLIB_SRCS})
    ELSE(PHYSFS_INTERNAL_ZLIB)
        SET(OPTIONAL_LIBRARY_LIBS ${OPTIONAL_LIBRARY_LIBS} ${ZLIB_LIBRARY})
        INCLUDE_DIRECTORIES(${ZLIB_INCLUDE_DIR})
    ENDIF(PHYSFS_INTERNAL_ZLIB)
ENDIF(PHYSFS_NEED_ZLIB)

OPTION(PHYSFS_BUILD_STATIC "Build static library" TRUE)
IF(PHYSFS_BUILD_STATIC)
    ADD_LIBRARY(physfs-static STATIC ${PHYSFS_SRCS})
    SET_TARGET_PROPERTIES(physfs-static PROPERTIES OUTPUT_NAME "physfs")
    SET(PHYSFS_LIB_TARGET physfs-static)
    SET(PHYSFS_INSTALL_TARGETS ${PHYSFS_INSTALL_TARGETS} ";physfs-static")
ENDIF(PHYSFS_BUILD_STATIC)

OPTION(PHYSFS_BUILD_SHARED "Build shared library" TRUE)
IF(PHYSFS_BUILD_SHARED)
    ADD_LIBRARY(physfs SHARED ${PHYSFS_SRCS})
    SET_TARGET_PROPERTIES(physfs PROPERTIES VERSION ${PHYSFS_VERSION})
    SET_TARGET_PROPERTIES(physfs PROPERTIES SOVERSION ${PHYSFS_SOVERSION})
    TARGET_LINK_LIBRARIES(physfs ${OPTIONAL_LIBRARY_LIBS} ${OTHER_LDFLAGS})
    SET(PHYSFS_LIB_TARGET physfs)
    SET(PHYSFS_INSTALL_TARGETS ${PHYSFS_INSTALL_TARGETS} ";physfs")
ENDIF(PHYSFS_BUILD_SHARED)

IF(NOT PHYSFS_BUILD_SHARED AND NOT PHYSFS_BUILD_STATIC)
    MESSAGE(FATAL "Both shared and static libraries are disabled!")
ENDIF(NOT PHYSFS_BUILD_SHARED AND NOT PHYSFS_BUILD_STATIC)

# CMake FAQ says I need this...
IF(PHYSFS_BUILD_SHARED AND PHYSFS_BUILD_STATIC)
    SET_TARGET_PROPERTIES(physfs PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    SET_TARGET_PROPERTIES(physfs-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
ENDIF(PHYSFS_BUILD_SHARED AND PHYSFS_BUILD_STATIC)

OPTION(PHYSFS_BUILD_TEST "Build stdio test program." TRUE)
MARK_AS_ADVANCED(PHYSFS_BUILD_TEST)
IF(PHYSFS_BUILD_TEST)
    FIND_PATH(READLINE_H readline/readline.h)
    FIND_PATH(HISTORY_H readline/history.h)
    IF(READLINE_H AND HISTORY_H)
        FIND_LIBRARY(CURSES_LIBRARY NAMES curses ncurses)
        SET(CMAKE_REQUIRED_LIBRARIES ${CURSES_LIBRARY})
        FIND_LIBRARY(READLINE_LIBRARY readline)
        FIND_LIBRARY(HISTORY_LIBRARY history)
        IF(READLINE_LIBRARY AND HISTORY_LIBRARY)
            SET(HAVE_SYSTEM_READLINE TRUE)
            SET(TEST_PHYSFS_LIBS ${TEST_PHYSFS_LIBS} ${READLINE_LIBRARY} ${CURSES_LIBRARY})
            INCLUDE_DIRECTORIES(${READLINE_H} ${HISTORY_H})
            ADD_DEFINITIONS(-DPHYSFS_HAVE_READLINE=1)
        ENDIF(READLINE_LIBRARY AND HISTORY_LIBRARY)
    ENDIF(READLINE_H AND HISTORY_H)
    ADD_EXECUTABLE(test_physfs test/test_physfs.c)
    TARGET_LINK_LIBRARIES(test_physfs ${PHYSFS_LIB_TARGET} ${TEST_PHYSFS_LIBS} ${OTHER_LDFLAGS})
    SET(PHYSFS_INSTALL_TARGETS ${PHYSFS_INSTALL_TARGETS} ";test_physfs")
ENDIF(PHYSFS_BUILD_TEST)

OPTION(PHYSFS_BUILD_WX_TEST "Build wxWidgets test program." TRUE)
MARK_AS_ADVANCED(PHYSFS_BUILD_WX_TEST)
IF(PHYSFS_BUILD_WX_TEST)
    SET(wxWidgets_USE_LIBS base core adv)
    SET(wxWidgets_INCLUDE_DIRS_NO_SYSTEM 1)
    FIND_PACKAGE(wxWidgets)
    IF(wxWidgets_FOUND)
        INCLUDE(${wxWidgets_USE_FILE})
        ADD_EXECUTABLE(wxtest_physfs test/wxtest_physfs.cpp)
        SET_SOURCE_FILES_PROPERTIES(test/wxtest_physfs.cpp PROPERTIES COMPILE_FLAGS "${wxWidgets_CXX_FLAGS}")
        TARGET_LINK_LIBRARIES(wxtest_physfs ${PHYSFS_LIB_TARGET} ${wxWidgets_LIBRARIES} ${OTHER_LDFLAGS})
        SET(PHYSFS_INSTALL_TARGETS ${PHYSFS_INSTALL_TARGETS} ";wxtest_physfs")
    ELSE(wxWidgets_FOUND)
        MESSAGE(STATUS "wxWidgets not found. Disabling wx test app.")
        SET(PHYSFS_BUILD_WX_TEST FALSE)
    ENDIF(wxWidgets_FOUND)
ENDIF(PHYSFS_BUILD_WX_TEST)


# Scripting language bindings...

#CMake's SWIG support is basically useless.
#FIND_PACKAGE(SWIG)

FIND_PROGRAM(SWIG swig DOC "Path to swig command line app: http://swig.org/")
IF(NOT SWIG)
    MESSAGE(STATUS "SWIG not found. You won't be able to build scripting language bindings.")
ELSE(NOT SWIG)
    MARK_AS_ADVANCED(SWIG)
    IF(DEFINED CMAKE_BUILD_TYPE)
        IF((NOT CMAKE_BUILD_TYPE STREQUAL "") AND (NOT CMAKE_BUILD_TYPE STREQUAL "Debug"))
            IF(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
                SET(SWIG_OPT_CFLAGS "-small")
            ELSE(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
                SET(SWIG_OPT_CFLAGS "-O")
            ENDIF(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
        ENDIF((NOT CMAKE_BUILD_TYPE STREQUAL "") AND (NOT CMAKE_BUILD_TYPE STREQUAL "Debug"))
    ENDIF(DEFINED CMAKE_BUILD_TYPE)

    SET(SWIG_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/physfs-swig-bindings")

    MACRO(CONFIGURE_SWIG_BINDING _LANG _INSTALLPATH _EXTRAOUTPUTS _EXTRACFLAGS _EXTRALDFLAGS)
        STRING(TOUPPER "${_LANG}" _UPPERLANG)
        STRING(TOLOWER "${_LANG}" _LOWERLANG)
        SET(_TARGET "physfs-${_LOWERLANG}")
        SET(_TARGETDIR "${SWIG_OUTPUT_DIR}/${_LOWERLANG}")

        IF(NOT EXISTS "${_TARGETDIR}")
            FILE(MAKE_DIRECTORY "${_TARGETDIR}")
        ENDIF(NOT EXISTS "${_TARGETDIR}")

        IF(PHYSFS_BUILD_${_UPPERLANG})
            ADD_CUSTOM_COMMAND(
                OUTPUT "${_TARGETDIR}/${_TARGET}.c" ${_EXTRAOUTPUTS}
                MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/extras/physfs-swig.i"
                COMMAND "${SWIG}"
                ARGS ${SWIG_OPT_CFLAGS} -${_LOWERLANG} -outdir "${_TARGETDIR}" -o "${_TARGETDIR}/${_TARGET}.c" "${CMAKE_CURRENT_SOURCE_DIR}/extras/physfs-swig.i"
                COMMENT "Generating ${_LANG} bindings..."
            )

            ADD_LIBRARY(${_TARGET} SHARED "${_TARGETDIR}/${_TARGET}.c")
            TARGET_LINK_LIBRARIES(${_TARGET} ${PHYSFS_LIB_TARGET})
            SET_TARGET_PROPERTIES(${_TARGET} PROPERTIES
                COMPILE_FLAGS "${_EXTRACFLAGS}"
                LINK_FLAGS "${_EXTRALDFLAGS}"
                LIBRARY_OUTPUT_NAME "physfs"
                LIBRARY_OUTPUT_DIRECTORY "${_TARGETDIR}"
                CLEAN_DIRECT_OUTPUT 1
            )
            INSTALL(TARGETS ${_TARGET} LIBRARY DESTINATION "${_INSTALLPATH}")
            MESSAGE(STATUS "${_LANG} bindings configured!")
        ELSE(PHYSFS_BUILD_${_UPPERLANG})
            MESSAGE(STATUS "Couldn't figure out ${_LANG} configuration. Skipping ${_LANG} bindings.")
        ENDIF(PHYSFS_BUILD_${_UPPERLANG})
    ENDMACRO(CONFIGURE_SWIG_BINDING)

    MACRO(ADD_SCRIPT_BINDING_OPTION _VAR _LANG _DEFVAL)
        OPTION(${_VAR} "Build ${_LANG} bindings." ${_DEFVAL})
        MARK_AS_ADVANCED(${_VAR})
    ENDMACRO(ADD_SCRIPT_BINDING_OPTION)

    ADD_SCRIPT_BINDING_OPTION(PHYSFS_BUILD_PERL "Perl" TRUE)
    ADD_SCRIPT_BINDING_OPTION(PHYSFS_BUILD_RUBY "Ruby" TRUE)
ENDIF(NOT SWIG)

IF(PHYSFS_BUILD_PERL)
    MESSAGE(STATUS "Configuring Perl bindings...")
    FIND_PROGRAM(PERL perl DOC "Path to perl command line app: http://perl.org/")
    IF(NOT PERL)
        MESSAGE(STATUS "Perl not found. You won't be able to build perl bindings.")
        SET(PHYSFS_BUILD_PERL FALSE)
    ENDIF(NOT PERL)
    MARK_AS_ADVANCED(PERL)

    MACRO(GET_PERL_CONFIG _KEY _VALUE)
        IF(PHYSFS_BUILD_PERL)
            MESSAGE(STATUS "Figuring out perl config value '${_KEY}' ...")
            EXECUTE_PROCESS(
                COMMAND ${PERL} -w -e "use Config; print \$Config{${_KEY}};"
                RESULT_VARIABLE GET_PERL_CONFIG_RC
                OUTPUT_VARIABLE ${_VALUE}
            )
            IF(NOT GET_PERL_CONFIG_RC EQUAL 0)
                MESSAGE(STATUS "Perl executable ('${PERL}') reported failure: ${GET_PERL_CONFIG_RC}")
                SET(PHYSFS_BUILD_PERL FALSE)
            ENDIF(NOT GET_PERL_CONFIG_RC EQUAL 0)
            IF(NOT ${_VALUE})
                MESSAGE(STATUS "Perl executable ('${PERL}') didn't have a value for '${_KEY}'")
                SET(PHYSFS_BUILD_PERL FALSE)
            ENDIF(NOT ${_VALUE})

            IF(PHYSFS_BUILD_PERL)
                MESSAGE(STATUS "Perl says: '${${_VALUE}}'.")
            ENDIF(PHYSFS_BUILD_PERL)
        ENDIF(PHYSFS_BUILD_PERL)
    ENDMACRO(GET_PERL_CONFIG)

    # !!! FIXME: installsitearch might be the wrong location.
    GET_PERL_CONFIG("archlibexp" PERL_INCLUDE_PATH)
    GET_PERL_CONFIG("ccflags" PERL_CCFLAGS)
    GET_PERL_CONFIG("ldflags" PERL_LDFLAGS)
    GET_PERL_CONFIG("installsitearch" PERL_INSTALL_PATH)

    # !!! FIXME: this test for Mac OS X is wrong.
    IF(MACOSX)
        GET_PERL_CONFIG("libperl" PERL_LIBPERL)
        SET(TMPLIBPERL "${PERL_LIBPERL}")
        STRING(REGEX REPLACE "^lib" "" TMPLIBPERL "${TMPLIBPERL}")
        STRING(REGEX REPLACE "\\.so$" "" TMPLIBPERL "${TMPLIBPERL}")
        STRING(REGEX REPLACE "\\.dylib$" "" TMPLIBPERL "${TMPLIBPERL}")
        STRING(REGEX REPLACE "\\.dll$" "" TMPLIBPERL "${TMPLIBPERL}")
        IF(NOT "${TMPLIBPERL}" STREQUAL "${PERL_LIBPERL}")
            MESSAGE(STATUS "Stripped '${PERL_LIBPERL}' down to '${TMPLIBPERL}'.")
            SET(PERL_LIBPERL "${TMPLIBPERL}")
        ENDIF(NOT "${TMPLIBPERL}" STREQUAL "${PERL_LIBPERL}")
        SET(PERL_LIBPERL "-l${PERL_LIBPERL}")
    ENDIF(MACOSX)

    CONFIGURE_SWIG_BINDING(Perl "${PERL_INSTALL_PATH}" "${SWIG_OUTPUT_DIR}/perl/physfs.pm" "\"-I${PERL_INCLUDE_PATH}/CORE\" ${PERL_CCFLAGS} -w" "\"-L${PERL_INCLUDE_PATH}/CORE\" ${PERL_LIBPERL} ${PERL_LDFLAGS}")
    INSTALL(FILES "${SWIG_OUTPUT_DIR}/perl/physfs.pm" DESTINATION "${PERL_INSTALL_PATH}")
    INSTALL(
        FILES test/test_physfs.pl
        DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
ENDIF(PHYSFS_BUILD_PERL)

# !!! FIXME: lots of cut-and-paste from perl bindings.
IF(PHYSFS_BUILD_RUBY)
    MESSAGE(STATUS "Configuring Ruby bindings...")
    FIND_PROGRAM(RUBY ruby DOC "Path to ruby command line app: http://ruby-lang.org/")
    IF(NOT RUBY)
        MESSAGE(STATUS "Ruby not found. You won't be able to build ruby bindings.")
        SET(PHYSFS_BUILD_RUBY FALSE)
    ENDIF(NOT RUBY)
    MARK_AS_ADVANCED(RUBY)

    MACRO(GET_RUBY_CONFIG _KEY _VALUE)
        IF(PHYSFS_BUILD_RUBY)
            MESSAGE(STATUS "Figuring out ruby config value '${_KEY}' ...")
            EXECUTE_PROCESS(
                COMMAND ${RUBY} -e "require 'rbconfig'; puts RbConfig::CONFIG['${_KEY}'];"
                RESULT_VARIABLE GET_RUBY_CONFIG_RC
                OUTPUT_VARIABLE ${_VALUE}
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            IF(NOT GET_RUBY_CONFIG_RC EQUAL 0)
                MESSAGE(STATUS "Ruby executable ('${RUBY}') reported failure: ${GET_RUBY_CONFIG_RC}")
                SET(PHYSFS_BUILD_RUBY FALSE)
            ENDIF(NOT GET_RUBY_CONFIG_RC EQUAL 0)
            IF(NOT ${_VALUE})
                MESSAGE(STATUS "Ruby executable ('${RUBY}') didn't have a value for '${_KEY}'")
                SET(PHYSFS_BUILD_RUBY FALSE)
            ENDIF(NOT ${_VALUE})

            IF(PHYSFS_BUILD_RUBY)
                MESSAGE(STATUS "Ruby says: '${${_VALUE}}'.")
            ENDIF(PHYSFS_BUILD_RUBY)
        ENDIF(PHYSFS_BUILD_RUBY)
    ENDMACRO(GET_RUBY_CONFIG)

    GET_RUBY_CONFIG("archdir" RUBY_INCLUDE_PATH)
    GET_RUBY_CONFIG("CFLAGS" RUBY_CCFLAGS)
    GET_RUBY_CONFIG("LDFLAGS" RUBY_LDFLAGS)
    GET_RUBY_CONFIG("sitearchdir" RUBY_INSTALL_PATH)
    GET_RUBY_CONFIG("LIBRUBYARG_SHARED" RUBY_LIBRUBY)
    GET_RUBY_CONFIG("libdir" RUBY_LIBDIR)

    CONFIGURE_SWIG_BINDING(Ruby "${RUBY_INSTALL_PATH}" "" "\"-I${RUBY_INCLUDE_PATH}\" ${RUBY_CCFLAGS} -w" "\"-L${RUBY_LIBDIR}\" ${RUBY_LIBRUBY} ${RUBY_LDFLAGS}")
    SET_TARGET_PROPERTIES(physfs-ruby PROPERTIES PREFIX "")
    INSTALL(
        FILES test/test_physfs.rb
        DESTINATION bin
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                    GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )
ENDIF(PHYSFS_BUILD_RUBY)


INSTALL(TARGETS ${PHYSFS_INSTALL_TARGETS}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
INSTALL(FILES src/physfs.h DESTINATION include)

FIND_PACKAGE(Doxygen)
IF(DOXYGEN_FOUND)
    ADD_CUSTOM_TARGET(docs ${DOXYGEN_EXECUTABLE} COMMENT "Building documentation")
ELSE(DOXYGEN_FOUND)
    MESSAGE(STATUS "Doxygen not found. You won't be able to build documentation.")
ENDIF(DOXYGEN_FOUND)

IF(UNIX)
    SET(PHYSFS_TARBALL "${CMAKE_CURRENT_SOURCE_DIR}/../physfs-${PHYSFS_VERSION}.tar.gz")
    ADD_CUSTOM_TARGET(
        dist
        hg archive -t tgz "${PHYSFS_TARBALL}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building source tarball '${PHYSFS_TARBALL}'..."
    )
    ADD_CUSTOM_TARGET(
        uninstall
        "${CMAKE_CURRENT_SOURCE_DIR}/extras/uninstall.sh"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Uninstall the project..."
    )
ENDIF(UNIX)

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
MESSAGE_BOOL_OPTION("Build Perl bindings" PHYSFS_BUILD_PERL)
MESSAGE_BOOL_OPTION("Build Ruby bindings" PHYSFS_BUILD_RUBY)
MESSAGE_BOOL_OPTION("Build wxWidgets test program" PHYSFS_BUILD_WX_TEST)
MESSAGE_BOOL_OPTION("Build stdio test program" PHYSFS_BUILD_TEST)
IF(PHYSFS_BUILD_TEST)
    MESSAGE_BOOL_OPTION("  Use readline in test program" HAVE_SYSTEM_READLINE)
ENDIF(PHYSFS_BUILD_TEST)

# end of CMakeLists.txt ...

