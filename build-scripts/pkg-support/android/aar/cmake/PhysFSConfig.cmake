# PhysicsFS CMake configuration file:
# This file is meant to be placed in lib/cmake/PhysFS subfolder of a reconstructed Android PhysFS SDK

cmake_minimum_required(VERSION 3.0...4.0)

include(FeatureSummary)
set_package_properties(PhysicsFS PROPERTIES
    URL "https://icculus.org/physfs/"
    DESCRIPTION "Library to provide abstract access to various archives"
)

# Copied from `configure_package_config_file`
macro(set_and_check _var _file)
    set(${_var} "${_file}")
    if(NOT EXISTS "${_file}")
        message(FATAL_ERROR "File or directory ${_file} referenced by variable ${_var} does not exist !")
    endif()
endmacro()

# Copied from `configure_package_config_file`
macro(check_required_components _NAME)
    foreach(comp ${${_NAME}_FIND_COMPONENTS})
        if(NOT ${_NAME}_${comp}_FOUND)
            if(${_NAME}_FIND_REQUIRED_${comp})
                set(${_NAME}_FOUND FALSE)
            endif()
        endif()
    endforeach()
endmacro()

set(PhysFS_FOUND TRUE)

if(SDL_CPU_X86)
    set(_sdl_arch_subdir "x86")
elseif(SDL_CPU_X64)
    set(_sdl_arch_subdir "x86_64")
elseif(SDL_CPU_ARM32)
    set(_sdl_arch_subdir "armeabi-v7a")
elseif(SDL_CPU_ARM64)
    set(_sdl_arch_subdir "arm64-v8a")
else()
    set(PhysFS_FOUND FALSE)
    return()
endif()

get_filename_component(_physfs_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set_and_check(_physfs_prefix          "${_physfs_prefix}")
set_and_check(_physfs_include_dirs    "${_physfs_prefix}/include")

set_and_check(_physfs_lib             "${_physfs_prefix}/lib/${_sdl_arch_subdir}/libphysfs.so")
unset(_sdl_arch_subdir)
unset(_physfs_prefix)

# All targets are created, even when some might not be requested though COMPONENTS.
# This is done for compatibility with CMake generated PhysFS-target.cmake files.

if(EXISTS "${_physfs_lib}")
    if(NOT TARGET PhysFS::PhysFS-shared)
        add_library(PhysFS::PhysFS-shared SHARED IMPORTED)
        set_target_properties(PhysFS::PhysFS-shared
            PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${_physfs_include_dirs}"
                IMPORTED_LOCATION "${_physfs_lib}"
    )
    endif()
    set(PhysFS_PhysFS-shared_FOUND TRUE)
else()
    set(PhysFS_PhysFS-shared_FOUND FALSE)
endif()
unset(_physfs_lib)
unset(_physfs_include_dirs)

set(PhysFS_PhysFS-static_FOUND FALSE)

if(PhysFS_PhysFS-shared_FOUND)
    set(PhysFS_PhysFS_FOUND TRUE)
endif()

function(_sdl_create_target_alias_compat NEW_TARGET TARGET)
    if(CMAKE_VERSION VERSION_LESS "3.18")
        # Aliasing local targets is not supported on CMake < 3.18, so make it global.
        add_library(${NEW_TARGET} INTERFACE IMPORTED)
        set_target_properties(${NEW_TARGET} PROPERTIES INTERFACE_LINK_LIBRARIES "${TARGET}")
    else()
        add_library(${NEW_TARGET} ALIAS ${TARGET})
    endif()
endfunction()

# Make sure PhysFS::PhysFS always exists
if(NOT TARGET PhysFS::PhysFS)
    if(TARGET PhysFS::PhysFS-shared)
        _sdl_create_target_alias_compat(PhysFS::PhysFS PhysFS::PhysFS-shared)
    endif()
endif()

check_required_components(PhysFS)
