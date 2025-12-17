cmake_minimum_required(VERSION 3.0...4.0)
project(test_physfs C)

# Override CMAKE_FIND_ROOT_PATH_MODE to allow search for SDL3_mixer outside of sysroot
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE NEVER)

option(TEST_STATIC "Test static PhysFS library" ON)
if(TEST_STATIC)
    find_package(PhysFS CONFIG REQUIRED COMPONENTS PhysFS-static)
    add_executable(a-static ../../test/test_physfs.c)
    target_link_libraries(a-static PRIVATE PhysFS::PhysFS-static)
endif()

option(TEST_SHARED "Test shared PhysFS library" ON)
if(TEST_SHARED)
    find_package(PhysFS CONFIG REQUIRED COMPONENTS PhysFS-shared)
    add_executable(a-shared ../../test/test_physfs.c)
    target_link_libraries(a-shared PRIVATE PhysFS::PhysFS-shared)
endif()

find_package(PhysFS CONFIG REQUIRED COMPONENTS PhysFS)
add_executable(a ../../test/test_physfs.c)
target_link_libraries(a PRIVATE PhysFS::PhysFS)
