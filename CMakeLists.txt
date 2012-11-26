# PhysicsFS; a portable, flexible file i/o abstraction.
#
# Please see the file LICENSE.txt in the source's root directory.

cmake_minimum_required(VERSION 2.8.4)

project(PhysicsFS)
set(PHYSFS_VERSION 2.1.0)

# Increment this if/when we break backwards compatibility.
set(PHYSFS_SOVERSION 1)

# I hate that they define "WIN32" ... we're about to move to Win64...I hope!
if(WIN32 AND NOT WINDOWS)
    set(WINDOWS TRUE)
endif()

# Bleh, let's do it for "APPLE" too.
if(APPLE AND NOT MACOSX)
    set(MACOSX TRUE)
endif()

# For now, Haiku and BeOS are the same, as far as the build system cares.
if(HAIKU AND NOT BEOS)
    set(BEOS TRUE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    set(SOLARIS TRUE)
endif()

include(CheckIncludeFile)
include(CheckLibraryExists)
include(CheckCSourceCompiles)

include_directories(./src)

if(MACOSX)
    # Fallback to older OS X on PowerPC to support wider range of systems...
    if(CMAKE_OSX_ARCHITECTURES MATCHES ppc)
        add_definitions(-DMAC_OS_X_VERSION_MIN_REQUIRED=1020)
        set(OTHER_LDFLAGS ${OTHER_LDFLAGS} " -mmacosx-version-min=10.2")
    endif()

    # Need these everywhere...
    add_definitions(-fno-common)
    set(OTHER_LDFLAGS ${OTHER_LDFLAGS} "-framework Carbon -framework IOKit")
endif()

# Add some gcc-specific command lines.
if(CMAKE_COMPILER_IS_GNUCC)
    # Always build with debug symbols...you can strip it later.
    add_definitions(-g -pipe -Werror -fsigned-char)

    # Stupid BeOS generates warnings in the system headers.
    if(NOT BEOS)
        add_definitions(-Wall)
    endif()

    check_c_source_compiles("
        #if ((defined(__GNUC__)) && (__GNUC__ >= 4))
        int main(int argc, char **argv) { int is_gcc4 = 1; return 0; }
        #else
        #error This is not gcc4.
        #endif
    " PHYSFS_IS_GCC4)

    if(PHYSFS_IS_GCC4)
        # Not supported on several operating systems at this time.
        if(NOT SOLARIS AND NOT WINDOWS)
            add_definitions(-fvisibility=hidden)
        endif()
    endif()

    # Don't use -rpath.
    set(CMAKE_SKIP_RPATH ON CACHE BOOL "Skip RPATH" FORCE)
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "SunPro")
    add_definitions(-erroff=E_EMPTY_TRANSLATION_UNIT)
    add_definitions(-xldscope=hidden)
endif()

if(MSVC)
    # VS.NET 8.0 got really really anal about strcpy, etc, which even if we
    #  cleaned up our code, zlib, etc still use...so disable the warning.
    add_definitions(-D_CRT_SECURE_NO_WARNINGS=1)
endif()

# Basic chunks of source code ...

set(LZMA_SRCS
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

if(BEOS)
    # We add this explicitly, since we don't want CMake to think this
    #  is a C++ project unless we're on BeOS.
    set(PHYSFS_BEOS_SRCS src/platform_beos.cpp)
    find_library(BE_LIBRARY be)
    find_library(ROOT_LIBRARY root)
    set(OPTIONAL_LIBRARY_LIBS ${OPTIONAL_LIBRARY_LIBS} ${BE_LIBRARY} ${ROOT_LIBRARY})
endif()

# Almost everything is "compiled" here, but things that don't apply to the
#  build are #ifdef'd out. This is to make it easy to embed PhysicsFS into
#  another project or bring up a new build system: just compile all the source
#  code and #define the things you want.
set(PHYSFS_SRCS
    src/physfs.c
    src/physfs_byteorder.c
    src/physfs_unicode.c
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
    src/archiver_slb.c
    src/archiver_iso9660.c
    ${PHYSFS_BEOS_SRCS}
)


# platform layers ...

if(UNIX)
    if(BEOS)
        set(PHYSFS_HAVE_CDROM_SUPPORT TRUE)
        set(PHYSFS_HAVE_THREAD_SUPPORT TRUE)
        set(HAVE_PTHREAD_H TRUE)
    else()
        check_include_file(sys/ucred.h HAVE_UCRED_H)
        if(HAVE_UCRED_H)
            add_definitions(-DPHYSFS_HAVE_SYS_UCRED_H=1)
            set(PHYSFS_HAVE_CDROM_SUPPORT TRUE)
        endif()

        check_include_file(mntent.h HAVE_MNTENT_H)
        if(HAVE_MNTENT_H)
            add_definitions(-DPHYSFS_HAVE_MNTENT_H=1)
            set(PHYSFS_HAVE_CDROM_SUPPORT TRUE)
        endif()

        # !!! FIXME: Solaris fails this, because mnttab.h implicitly
        # !!! FIXME:  depends on other system headers.  :(
        #check_include_file(sys/mnttab.h HAVE_SYS_MNTTAB_H)
        check_c_source_compiles("
            #include <stdio.h>
            #include <sys/mnttab.h>
            int main(int argc, char **argv) { return 0; }
        " HAVE_SYS_MNTTAB_H)

        if(HAVE_SYS_MNTTAB_H)
            add_definitions(-DPHYSFS_HAVE_SYS_MNTTAB_H=1)
            set(PHYSFS_HAVE_CDROM_SUPPORT TRUE)
        endif()

        check_include_file(pthread.h HAVE_PTHREAD_H)
        if(HAVE_PTHREAD_H)
            set(PHYSFS_HAVE_THREAD_SUPPORT TRUE)
            find_library(PTHREAD_LIBRARY pthread)
            if(PTHREAD_LIBRARY)
                set(OPTIONAL_LIBRARY_LIBS ${OPTIONAL_LIBRARY_LIBS} ${PTHREAD_LIBRARY})
            endif()
        endif()
    endif()
endif()

if(WINDOWS)
    set(PHYSFS_HAVE_CDROM_SUPPORT TRUE)
    set(PHYSFS_HAVE_THREAD_SUPPORT TRUE)
endif()

if(NOT PHYSFS_HAVE_CDROM_SUPPORT)
    add_definitions(-DPHYSFS_NO_CDROM_SUPPORT=1)
    message(WARNING " ***")
    message(WARNING " *** There is no CD-ROM support in this build!")
    message(WARNING " *** PhysicsFS will just pretend there are no discs.")
    message(WARNING " *** This may be fine, depending on how PhysicsFS is used,")
    message(WARNING " ***   but is this what you REALLY wanted?")
    message(WARNING " *** (Maybe fix CMakeLists.txt, or write a platform driver?)")
    message(WARNING " ***")
endif()

if(PHYSFS_HAVE_THREAD_SUPPORT)
    add_definitions(-D_REENTRANT -D_THREAD_SAFE)
else()
    add_definitions(-DPHYSFS_NO_THREAD_SUPPORT=1)
    message(WARNING " ***")
    message(WARNING " *** There is no thread support in this build!")
    message(WARNING " *** PhysicsFS will NOT be reentrant!")
    message(WARNING " *** This may be fine, depending on how PhysicsFS is used,")
    message(WARNING " ***   but is this what you REALLY wanted?")
    message(WARNING " *** (Maybe fix CMakeLists.txt, or write a platform driver?)")
    message(WARNING " ***")
endif()


# Archivers ...

option(PHYSFS_ARCHIVE_ZIP "Enable ZIP support" TRUE)
if(PHYSFS_ARCHIVE_ZIP)
    add_definitions(-DPHYSFS_SUPPORTS_ZIP=1)
endif()

option(PHYSFS_ARCHIVE_7Z "Enable 7zip support" TRUE)
if(PHYSFS_ARCHIVE_7Z)
    add_definitions(-DPHYSFS_SUPPORTS_7Z=1)
    # !!! FIXME: rename to 7z.c?
    set(PHYSFS_SRCS ${PHYSFS_SRCS} ${LZMA_SRCS})
endif()

option(PHYSFS_ARCHIVE_GRP "Enable Build Engine GRP support" TRUE)
if(PHYSFS_ARCHIVE_GRP)
    add_definitions(-DPHYSFS_SUPPORTS_GRP=1)
endif()

option(PHYSFS_ARCHIVE_WAD "Enable Doom WAD support" TRUE)
if(PHYSFS_ARCHIVE_WAD)
    add_definitions(-DPHYSFS_SUPPORTS_WAD=1)
endif()

option(PHYSFS_ARCHIVE_HOG "Enable Descent I/II HOG support" TRUE)
if(PHYSFS_ARCHIVE_HOG)
    add_definitions(-DPHYSFS_SUPPORTS_HOG=1)
endif()

option(PHYSFS_ARCHIVE_MVL "Enable Descent I/II MVL support" TRUE)
if(PHYSFS_ARCHIVE_MVL)
    add_definitions(-DPHYSFS_SUPPORTS_MVL=1)
endif()

option(PHYSFS_ARCHIVE_QPAK "Enable Quake I/II QPAK support" TRUE)
if(PHYSFS_ARCHIVE_QPAK)
    add_definitions(-DPHYSFS_SUPPORTS_QPAK=1)
endif()

option(PHYSFS_ARCHIVE_SLB "Enable I-War / Independence War SLB support" TRUE)
if(PHYSFS_ARCHIVE_SLB)
    add_definitions(-DPHYSFS_SUPPORTS_SLB=1)
endif()

option(PHYSFS_ARCHIVE_ISO9660 "Enable ISO9660 support" TRUE)
if(PHYSFS_ARCHIVE_ISO9660)
    add_definitions(-DPHYSFS_SUPPORTS_ISO9660=1)
endif()


option(PHYSFS_BUILD_STATIC "Build static library" TRUE)
if(PHYSFS_BUILD_STATIC)
    add_library(physfs-static STATIC ${PHYSFS_SRCS})
    set_target_properties(physfs-static PROPERTIES OUTPUT_NAME "physfs")
    set(PHYSFS_LIB_TARGET physfs-static)
    set(PHYSFS_INSTALL_TARGETS ${PHYSFS_INSTALL_TARGETS} ";physfs-static")
endif()

option(PHYSFS_BUILD_SHARED "Build shared library" TRUE)
if(PHYSFS_BUILD_SHARED)
    add_library(physfs SHARED ${PHYSFS_SRCS})
    set_target_properties(physfs PROPERTIES VERSION ${PHYSFS_VERSION})
    set_target_properties(physfs PROPERTIES SOVERSION ${PHYSFS_SOVERSION})
    target_link_libraries(physfs ${OPTIONAL_LIBRARY_LIBS} ${OTHER_LDFLAGS})
    set(PHYSFS_LIB_TARGET physfs)
    set(PHYSFS_INSTALL_TARGETS ${PHYSFS_INSTALL_TARGETS} ";physfs")
endif()

if(NOT PHYSFS_BUILD_SHARED AND NOT PHYSFS_BUILD_STATIC)
    message(FATAL "Both shared and static libraries are disabled!")
endif()

# CMake FAQ says I need this...
if(PHYSFS_BUILD_SHARED AND PHYSFS_BUILD_STATIC)
    set_target_properties(physfs PROPERTIES CLEAN_DIRECT_OUTPUT 1)
    set_target_properties(physfs-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)
endif()

option(PHYSFS_BUILD_TEST "Build stdio test program." TRUE)
mark_as_advanced(PHYSFS_BUILD_TEST)
if(PHYSFS_BUILD_TEST)
    find_path(READLINE_H readline/readline.h)
    find_path(HISTORY_H readline/history.h)
    if(READLINE_H AND HISTORY_H)
        find_library(CURSES_LIBRARY NAMES curses ncurses)
        set(CMAKE_REQUIRED_LIBRARIES ${CURSES_LIBRARY})
        find_library(READLINE_LIBRARY readline)
        if(READLINE_LIBRARY)
            set(HAVE_SYSTEM_READLINE TRUE)
            set(TEST_PHYSFS_LIBS ${TEST_PHYSFS_LIBS} ${READLINE_LIBRARY} ${CURSES_LIBRARY})
            include_directories(${READLINE_H} ${HISTORY_H})
            add_definitions(-DPHYSFS_HAVE_READLINE=1)
        endif()
    endif()
    add_executable(test_physfs test/test_physfs.c)
    target_link_libraries(test_physfs ${PHYSFS_LIB_TARGET} ${TEST_PHYSFS_LIBS} ${OTHER_LDFLAGS})
    set(PHYSFS_INSTALL_TARGETS ${PHYSFS_INSTALL_TARGETS} ";test_physfs")
endif()

install(TARGETS ${PHYSFS_INSTALL_TARGETS}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib${LIB_SUFFIX}
        ARCHIVE DESTINATION lib${LIB_SUFFIX})
install(FILES src/physfs.h DESTINATION include)

find_package(Doxygen)
if(DOXYGEN_FOUND)
    set(PHYSFS_OUTPUT_DOXYFILE "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile")
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile"
        "${PHYSFS_OUTPUT_DOXYFILE}"
        COPYONLY
    )
    file(APPEND "${PHYSFS_OUTPUT_DOXYFILE}" "\n\n# Below auto-generated by cmake...\n\n")
    file(APPEND "${PHYSFS_OUTPUT_DOXYFILE}" "PROJECT_NUMBER = \"${PHYSFS_VERSION}\"\n")
    file(APPEND "${PHYSFS_OUTPUT_DOXYFILE}" "OUTPUT_DIRECTORY = \"${CMAKE_CURRENT_BINARY_DIR}/docs\"\n")
    file(APPEND "${PHYSFS_OUTPUT_DOXYFILE}" "\n# End auto-generated section.\n\n")

    add_custom_target(
        docs
        ${DOXYGEN_EXECUTABLE} "${PHYSFS_OUTPUT_DOXYFILE}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building documentation in 'docs' directory..."
    )
else()
    message(STATUS "Doxygen not found. You won't be able to build documentation.")
endif()

if(UNIX)
    set(PHYSFS_TARBALL "${CMAKE_CURRENT_SOURCE_DIR}/../physfs-${PHYSFS_VERSION}.tar.bz2")
    add_custom_target(
        dist
        hg archive -t tbz2 "${PHYSFS_TARBALL}"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Building source tarball '${PHYSFS_TARBALL}'..."
    )
    add_custom_target(
        uninstall
        "${CMAKE_CURRENT_SOURCE_DIR}/extras/uninstall.sh"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Uninstall the project..."
    )
endif()

macro(message_bool_option _NAME _VALUE)
    if(${_VALUE})
        message(STATUS "  ${_NAME}: enabled")
    else()
        message(STATUS "  ${_NAME}: disabled")
    endif()
endmacro()

message(STATUS "PhysicsFS will build with the following options:")
message_bool_option("ZIP support" PHYSFS_ARCHIVE_ZIP)
message_bool_option("7zip support" PHYSFS_ARCHIVE_7Z)
message_bool_option("GRP support" PHYSFS_ARCHIVE_GRP)
message_bool_option("WAD support" PHYSFS_ARCHIVE_WAD)
message_bool_option("HOG support" PHYSFS_ARCHIVE_HOG)
message_bool_option("MVL support" PHYSFS_ARCHIVE_MVL)
message_bool_option("QPAK support" PHYSFS_ARCHIVE_QPAK)
message_bool_option("SLB support" PHYSFS_ARCHIVE_SLB)
message_bool_option("CD-ROM drive support" PHYSFS_HAVE_CDROM_SUPPORT)
message_bool_option("Thread safety" PHYSFS_HAVE_THREAD_SUPPORT)
message_bool_option("Build static library" PHYSFS_BUILD_STATIC)
message_bool_option("Build shared library" PHYSFS_BUILD_SHARED)
message_bool_option("Build stdio test program" PHYSFS_BUILD_TEST)
if(PHYSFS_BUILD_TEST)
    message_bool_option("  Use readline in test program" HAVE_SYSTEM_READLINE)
endif()

# end of CMakeLists.txt ...

