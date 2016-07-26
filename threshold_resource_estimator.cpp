#include <mesos/module/resource_estimator.hpp>

using process::Future;

using mesos::Resources;
using mesos::ResourceUsage;

namespace {

class ThresholdResourceEstimator : public mesos::slave::ResourceEstimator
{
    virtual Try<Nothing> initialize(const std::function<Future<ResourceUsage>()>&) final {
        return Nothing();
    }

    virtual Future<Resources> oversubscribable() final {
        return Resources();
    }

};

static mesos::slave::ResourceEstimator* create(const mesos::Parameters& parameters) {
    return new ThresholdResourceEstimator();
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
