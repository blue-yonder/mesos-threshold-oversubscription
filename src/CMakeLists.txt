#
# Define the module library
#

add_library("${CMAKE_PROJECT_NAME}" SHARED module.cpp threshold_resource_estimator.cpp threshold_qos_controller.cpp os.cpp threshold.cpp)
target_link_libraries("${CMAKE_PROJECT_NAME}" ${MESOS_LIBRARIES})
set_target_properties("${CMAKE_PROJECT_NAME}" PROPERTIES VERSION "${PROJECT_VERSION}")
install(
    TARGETS "${CMAKE_PROJECT_NAME}"
    LIBRARY DESTINATION "lib/x86_64-linux-gnu/mesos/modules/"
)
