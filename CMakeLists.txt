cmake_minimum_required(VERSION 3.0.2)  # CMake version provided by Debian Jessie, older versions untested
project(threshold_oversubscription CXX C)  # Language C is required by GTest
set(PROJECT_MAJOR_VERSION 0)
set(PROJECT_MINOR_VERSION 3)
set(PROJECT_PATCH_VERSION 0)
set(PROJECT_VERSION "${PROJECT_MAJOR_VERSION}.${PROJECT_MINOR_VERSION}.${PROJECT_PATCH_VERSION}")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()


#
# Find all the dependencies
#

find_package(Boost REQUIRED)
find_package(Protobuf REQUIRED)

find_path(GLOG_INCLUDE_DIR "glog/logging.h" DOC "Google logging include directory")
find_path(STOUT_INCLUDE_DIR "stout/version.hpp" DOC "Stout include directory")
find_path(LIBPROCESS_INCLUDE_DIR "process/run.hpp" DOC "libprocess include directory")
find_path(MESOS_INCLUDE_DIR "mesos/mesos.hpp" DOC "Mesos include directory")
find_library(MESOS_LIBRARIES "mesos" DOC "The Mesos library")

include_directories(
    SYSTEM
    ${GLOG_INCLUDE_DIR}
    ${STOUT_INCLUDE_DIR}
    ${LIBPROCESS_INCLUDE_DIR}
    ${MESOS_INCLUDE_DIR}
    ${PROTOBUF_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)


#
# Configure compilers
#
set(CMAKE_C_FLAGS "-Wall -Werror ${CMAKE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Werror ${CMAKE_CXX_FLAGS}")


#
# Bootstrap GTest
#
find_package(Threads REQUIRED)
include(ExternalProject)
ExternalProject_Add(
    GTest
    PREFIX "3rdparty/gtest"
    SOURCE_DIR "/usr/src/gtest"
    CMAKE_ARGS
        "-Dgtest_force_shared_crt=ON"
        "-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DCMAKE_CXX_FLAGS=${CMAKE_CXX_FLAGS}"
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_C_FLAGS=${CMAKE_C_FLAGS}"
    INSTALL_COMMAND ""  # GTEst does not provide an installation command
)
ExternalProject_Get_Property(GTest binary_dir)
set(GTEST_LIBRARIES "${binary_dir}/libgtest.a" "${CMAKE_THREAD_LIBS_INIT}")
set(GTEST_MAIN_LIBRARIES "${binary_dir}/libgtest_main.a")
set(GTEST_BOTH_LIBRARIES "${GTEST_MAIN_LIBRARIES}" "${GTEST_LIBRARIES}")

add_subdirectory (src)

enable_testing ()
add_subdirectory (tests)

