cmake_minimum_required(VERSION 2.8.9)  # CMake version provided by Debian Wheezy, older versions untested
project(threshold_oversubscription CXX C)  # Language C is required by GTest
set(PROJECT_MAJOR_VERSION 0)
set(PROJECT_MINOR_VERSION 2)
set(PROJECT_PATCH_VERSION 1)
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
# Define the estimator library
#

add_library("${CMAKE_PROJECT_NAME}" SHARED module.cpp threshold_resource_estimator.cpp threshold_qos_controller.cpp os.cpp threshold.cpp)
target_link_libraries("${CMAKE_PROJECT_NAME}" ${MESOS_LIBRARIES})
set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES VERSION "${PROJECT_VERSION}")
install(
	TARGETS "${CMAKE_PROJECT_NAME}"
	LIBRARY DESTINATION "lib/x86_64-linux-gnu/mesos/modules/"
)


#
# Bootstrap GTest
#

enable_testing()
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

#
# Define our tests
#

add_executable(module_test module_test.cpp)
add_dependencies(module_test GTest)
target_link_libraries(module_test ${GTEST_BOTH_LIBRARIES} ${MESOS_LIBRARIES})
add_test("ModuleTests" module_test)

add_executable(os_test os_test.cpp)
add_dependencies(os_test GTest)
target_link_libraries(os_test ${GTEST_BOTH_LIBRARIES} "${CMAKE_PROJECT_NAME}")
add_test("OSHelperTests" os_test)

add_executable(testutils_test testutils_test.cpp)
add_dependencies(testutils_test GTest)
target_link_libraries(testutils_test ${GTEST_BOTH_LIBRARIES} "${CMAKE_PROJECT_NAME}")
add_test("TestUtilsTests" testutils_test)

add_executable(threshold_resource_estimator_test threshold_resource_estimator_test.cpp)
add_dependencies(threshold_resource_estimator_test GTest)
target_link_libraries(threshold_resource_estimator_test ${GTEST_BOTH_LIBRARIES} "${CMAKE_PROJECT_NAME}")
add_test("EstimatorTests" threshold_resource_estimator_test)

add_executable(threshold_qos_controller threshold_qos_controller_test.cpp)
add_dependencies(threshold_qos_controller GTest)
target_link_libraries(threshold_qos_controller ${GTEST_BOTH_LIBRARIES} "${CMAKE_PROJECT_NAME}")
add_test("QoSControllerTests" threshold_qos_controller)
