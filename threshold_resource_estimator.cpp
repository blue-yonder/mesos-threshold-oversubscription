#include <process/dispatch.hpp>
#include <process/process.hpp>

#include <mesos/module/resource_estimator.hpp>

using process::dispatch;
using process::Failure;
using process::Future;
using process::Process;
using process::Owned;

using mesos::Resources;
using mesos::ResourceUsage;

namespace {

class ThresholdResourceEstimatorProcess : public Process<ThresholdResourceEstimatorProcess>
{
public:
    Future<Resources> oversubscribable();
};

class ThresholdResourceEstimator : public mesos::slave::ResourceEstimator
{
public:
    virtual Try<Nothing> initialize(const std::function<Future<ResourceUsage>()>&) final;

    virtual Future<Resources> oversubscribable() final;

    virtual ~ThresholdResourceEstimator();

protected:
    Owned<ThresholdResourceEstimatorProcess> process;
};


Future<Resources> ThresholdResourceEstimatorProcess::oversubscribable()
{
    return Resources();
}


Try<Nothing> ThresholdResourceEstimator::initialize(const std::function<Future<ResourceUsage>()>&) {
    if (process.get() != nullptr) {
        return Error("Threshold resource estimator has already been initialized");
    }

    process.reset(new ThresholdResourceEstimatorProcess());
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
