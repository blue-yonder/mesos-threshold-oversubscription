Change Log
==========

All notable changes to this project are noted in this file. This project adheres to [Semantic
Versioning](http://semver.org/).


0.8.0 (UNRELEASED)
------------------

### Added

* Add compatibility with Mesos 1.5.0.


0.7.0 (2018-03-07)
------------------

### Added

* Debian 9 (Stretch) is now supported.

### Changed

* Use Mesos's bundled libraries by default. This avoids issues when the bundled libraries are incompatible with the ones provided by the Linux distribution.

### Removed

* Debian 7 (Wheezy) is no longer supported.


0.6.0 (2017-10-23)
------------------

### Added

* Add compatibility with Mesos 1.2.0 and the new multi-role support.

### Fixed

* Fixed a bug in the test helpers that caused the test suite to fail if debug symbols were enabled on GCC 4.9.


0.5.0 (2017-01-26)
------------------

### Added

* Fix compatibility with Mesos 1.1.0.


0.4.0 (2016-09-21)
------------------

### Added

* The source is now compatible with GCC 4.9.

0.3.0 (2016-09-13)
------------------

### Added

* QoSController implemenation that reacts on load and memory thresholds.

### Changed

* The library has ben renamed to `libthreshold_oversubscription.so`. It contains both the
  `ThresholdResourceEstimator` and the `ThresholdQoSController` module.
* Changed the semantics of a load threshold: A load interval (5m, 15m) will only be considered
  exceeded if the previous intervals have exceeded the same threshold. This ensures
  that we only act on a 5m or 15m threshold if there is no indication that the load will change
  automatically without our action.

0.2.1 (2016-08-03)
------------------

### Bugfix

* Ignore cached memory when evaluating the memory threshold.


0.2.0 (2016-08-03)
------------------

### Changed

* All memory usage in the system is taken into account, not only that by Mesos executors.
* If system load cannot be measured any configured load threshold is assumed to be exceeded.
* Only report first load threshold detected to be exceeded, not all.


0.1.0 (2016-07-28)
------------------

### Added

* Offer a fixed amount of resources for oversubscription.
* Stop offering resources for oversubscription if load threshold is exceeded.
* Stop offering resources for oversubscription if memory threshold is exceeded.
* Compatibility with Mesos 0.28.
