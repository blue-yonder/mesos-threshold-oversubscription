#include <mesos/module/resource_estimator.hpp>

namespace {

static mesos::slave::ResourceEstimator* create(const mesos::Parameters& parameters) {
    return nullptr;  // FIXME implementation needed
}

static bool compatible() {
    return true;  // TODO this might be slightly overoptimistic
}

}

mesos::modules::Module<mesos::slave::ResourceEstimator> com_blue_yonder_ThresholdResourceEstimator(
    MESOS_MODULE_API_VERSION,
    MESOS_VERSION,
    "Matthias Bach",
    "matthias.bach@blue-yonder.com",
    "Threshold Resource Estimator Module.",
    compatible,
    create
);
