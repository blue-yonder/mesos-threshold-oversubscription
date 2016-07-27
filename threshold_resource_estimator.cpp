#include "threshold_resource_estimator.hpp"

#include <functional>

#include <process/defer.hpp>
#include <process/dispatch.hpp>
#include <process/process.hpp>

using process::dispatch;
using process::Failure;
using process::Future;
using process::Process;
using process::Owned;

using mesos::Resources;
using mesos::ResourceUsage;

using com::blue_yonder::ThresholdResourceEstimator;
using com::blue_yonder::ThresholdResourceEstimatorProcess;

class ThresholdResourceEstimatorProcess : public Process<ThresholdResourceEstimatorProcess>
{
public:
    ThresholdResourceEstimatorProcess(std::function<Future<ResourceUsage>()> const &, Resources const & fixed);
    Future<Resources> oversubscribable();
private:
    Future<Resources> calcUnusedResources(ResourceUsage const & usage);

    std::function<Future<ResourceUsage>()> const usage;
    Resources const fixed;
};


ThresholdResourceEstimatorProcess::ThresholdResourceEstimatorProcess(
    std::function<Future<ResourceUsage>()> const & usage,
    Resources const & fixed
)
    : usage(usage), fixed(fixed)
{};

Future<Resources> ThresholdResourceEstimatorProcess::oversubscribable()
{
    return usage().then(process::defer(self(), &Self::calcUnusedResources, std::placeholders::_1));
}

Future<Resources> ThresholdResourceEstimatorProcess::calcUnusedResources(ResourceUsage const & usage) {
    Resources allocatedRevocable;
    for(auto & executor: usage.executors()) {
        allocatedRevocable += Resources(executor.allocated()).revocable();
    }

    return fixed - allocatedRevocable;
}

Resources make_revocable(Resources const & any) {
    Resources revocable;
    for(auto resource: any) {
        resource.mutable_revocable();
        revocable += resource;
    }
    return revocable;
}

ThresholdResourceEstimator::ThresholdResourceEstimator(Resources const & fixed)
    : fixed{make_revocable(fixed)}
{
};

Try<Nothing> ThresholdResourceEstimator::initialize(std::function<Future<ResourceUsage>()> const & usage) {
    if (process.get() != nullptr) {
        return Error("Threshold resource estimator has already been initialized");
    }

    process.reset(new ThresholdResourceEstimatorProcess(usage, fixed));
    spawn(process.get());

    return Nothing();
}

Future<Resources> ThresholdResourceEstimator::oversubscribable() {
    if (process.get() == nullptr) {
        return Failure("Threshold resource estimator is not initialized");
    }

    return dispatch(process.get(), &ThresholdResourceEstimatorProcess::oversubscribable);
}

ThresholdResourceEstimator::~ThresholdResourceEstimator()
{
    if (process.get() != nullptr) {
        terminate(process.get());
        wait(process.get());
    }
}

namespace {

static mesos::slave::ResourceEstimator* create(const mesos::Parameters& parameters) {
    // Obtain the *fixed* resources from parameters.
    Option<Resources> resources;
    for(auto const & parameter: parameters.parameter()) {
        if (parameter.key() == "resources") {
            Try<Resources> parsed = Resources::parse(parameter.value());
            if (parsed.isError()) {
                return nullptr;
            }
            resources = parsed.get();
        }
    }

    if (resources.isNone()) {
        return nullptr;
    }
    return new ThresholdResourceEstimator(resources.get());
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
