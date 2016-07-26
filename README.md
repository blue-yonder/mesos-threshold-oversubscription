Threshold Resource Estimator for Mesos
======================================

This is a resource estimator for Mesos oversubscription. It behaves similar to the fixed resource estimator provided
by Mesos itself providing a configured amount of resources for usage by revocable tasks. However, once system
utilization reaches a defined treshold resources will be cut to the currently used amount, avoiding further scheduling
of revocable tasks to this node until system utilization has decreased again.

Motivation
----------

Using the fixed resource estimator for a larger portion of the machines resource could lead to situations where system
utilization increases far enough for a quality-of-service controller to start killing of tasks. However, the fixed
resource estimator would then report the just freed resources as available for oversubscription, allowing another
revocable task to take the slot and again being killed by the quality-of-service controller. Not offering revocable
resources in such high-utilization scenarios should improve the behaviour.

Installation
------------

This project uses CMake. Build requires mesos development headers and a compatible version of GCC. The Vagrant file in
this repository creates a proper build environment for Debian Wheezy. Build and installation follow the usual CMake
tripplet where on Debian Wheezy a compatible compiler must be selected:

	cmake <path to source> CXX=g++-5 CC=gcc-5
	make
	make install

Configuration
-------------

Add the following configuration to your `mesos-slave` invocation:

    --resource_estimator="com_blue_yonder_ThresholdResourceEstimator"
    --modules='{
      "libraries": {
        "file": "/<path>/<to>/libthreshold_resource_estimator.so",
        "modules": {
          "name": "com_blue_yonder_ThresholdResourceEstimator",
        }
      }
