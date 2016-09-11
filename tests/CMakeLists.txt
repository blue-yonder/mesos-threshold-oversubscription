include_directories (${CMAKE_SOURCE_DIR}/src)



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

add_executable(threshold_qos_controller_test threshold_qos_controller_test.cpp)
add_dependencies(threshold_qos_controller_test GTest)
target_link_libraries(threshold_qos_controller_test ${GTEST_BOTH_LIBRARIES} "${CMAKE_PROJECT_NAME}")
add_test("QoSControllerTests" threshold_qos_controller_test)
