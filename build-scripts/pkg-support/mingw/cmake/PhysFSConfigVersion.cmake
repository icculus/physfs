# PhysFS CMake version configuration file:
# This file is meant to be placed in a cmake subfolder of physfs-devel-3.x.y-mingw

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(physfs_config_path "${CMAKE_CURRENT_LIST_DIR}/../i686-w64-mingw32/lib/cmake/PhysFS/PhysFSConfigVersion.cmake")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(physfs_config_path "${CMAKE_CURRENT_LIST_DIR}/../x86_64-w64-mingw32/lib/cmake/PhysFS/PhysFSConfigVersion.cmake")
else()
    set(PACKAGE_VERSION_UNSUITABLE TRUE)
    return()
endif()

if(NOT EXISTS "${physfs_config_path}")
    message(WARNING "${physfs_config_path} does not exist: MinGW development package is corrupted")
    set(PACKAGE_VERSION_UNSUITABLE TRUE)
    return()
endif()

include("${physfs_config_path}")
