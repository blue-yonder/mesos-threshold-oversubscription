Change Log
==========

All notable changes to this project are noted in this file. This project adheres to [Semantic
Versioning](http://semver.org/).


UNRELEASED
----------

### Changed

* All memory usage in the system is taken into account, not only that by Mesos executors.
* Only report first load threshold detected to be exceeded, not all.


0.1.0 (2016-07-28)
------------------

### Added

* Offer a fixed amount of resources for oversubscription.
* Stop offering resources for oversubscription if load threshold is exceeded.
* Stop offering resources for oversubscription if memory threshold is exceeded.
* Compatibility with Mesos 0.28
