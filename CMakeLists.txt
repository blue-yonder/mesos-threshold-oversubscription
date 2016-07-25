cmake_minimum_required(VERSION 2.8.9)  # CMake version provided by Debian Wheezy, older versions untested
project(threshold_resource_estimator CXX)

find_package(Boost REQUIRED)
find_package(Protobuf REQUIRED)

find_path(GLOG_INCLUDE_DIR "glog/logging.h" DOC "Google logging include directory")
find_path(STOUT_INCLUDE_DIR "stout/version.hpp" DOC "Stout include directory")
find_path(LIBPROCESS_INCLUDE_DIR "process/run.hpp" DOC "libprocess include directory")
find_path(MESOS_INCLUDE_DIR "mesos/mesos.hpp" DOC "Mesos include directory")
find_library(MESOS_LIBRARIES "mesos" DOC "The Mesos library")


include_directories(
    ${GLOG_INCLUDE_DIR}
    ${STOUT_INCLUDE_DIR}
    ${LIBPROCESS_INCLUDE_DIR}
    ${MESOS_INCLUDE_DIR}
    ${PROTOBUF_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)

add_definitions(-std=c++11)


add_library("${CMAKE_PROJECT_NAME}" SHARED threshold_resource_estimator.cpp)
install(
	TARGETS "${CMAKE_PROJECT_NAME}"
	LIBRARY DESTINATION "lib/x86_64-linux-gnu/mesos/modules/"
)
