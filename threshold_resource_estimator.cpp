#include "threshold_resource_estimator.hpp"

#include <stout/os.hpp>

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
    ThresholdResourceEstimatorProcess(
        std::function<Future<ResourceUsage>()> const &, std::function<Try<os::Load>()> const &, Resources const & fixed,
        Option<double> const &, Option<double> const &, Option<double> const &
    );
    Future<Resources> oversubscribable();
private:
    Future<Resources> calcUnusedResources(ResourceUsage const & usage);
    bool loadExceedsThresholds();

    std::function<Future<ResourceUsage>()> const usage;
    std::function<Try<os::Load>()> const load;
    Resources const fixed;
    Option<double> const loadThreshold1Min;
    Option<double> const loadThreshold5Min;
    Option<double> const loadThreshold15Min;
};


ThresholdResourceEstimatorProcess::ThresholdResourceEstimatorProcess(
    std::function<Future<ResourceUsage>()> const & usage,
    std::function<Try<os::Load>()> const & load,
    Resources const & fixed,
    Option<double> const & loadThreshold1Min,
    Option<double> const & loadThreshold5Min,
    Option<double> const & loadThreshold15Min
) :
    usage{usage},
    load{load},
    fixed{fixed},
    loadThreshold1Min{loadThreshold1Min},
    loadThreshold5Min{loadThreshold5Min},
    loadThreshold15Min{loadThreshold15Min}
{};

Future<Resources> ThresholdResourceEstimatorProcess::oversubscribable()
{
    if(loadExceedsThresholds()) {
        return Resources();
    }

    return usage().then(process::defer(self(), &Self::calcUnusedResources, std::placeholders::_1));
}

Future<Resources> ThresholdResourceEstimatorProcess::calcUnusedResources(ResourceUsage const & usage) {
    Resources allocatedRevocable;
    for(auto & executor: usage.executors()) {
        allocatedRevocable += Resources(executor.allocated()).revocable();
    }

    return fixed - allocatedRevocable;
}

bool ThresholdResourceEstimatorProcess::loadExceedsThresholds() {
    Try<os::Load> load = this->load();

    if (load.isError()) {
      LOG(ERROR) << "Failed to fetch system load: " + load.error();
      return false;
    }

    bool overloaded = false;

    if (loadThreshold1Min.isSome()) {
      if (load.get().one >= loadThreshold1Min.get()) {
        LOG(INFO) << "System 1 minute load average " << load.get().one
                  << " reached threshold " << loadThreshold1Min.get();
        overloaded = true;
      }
    }

    if (loadThreshold5Min.isSome()) {
      if (load.get().five >= loadThreshold5Min.get()) {
        LOG(INFO) << "System 5 minutes load average " << load.get().five
                  << " reached threshold " << loadThreshold5Min.get();
        overloaded = true;
      }
    }

    if (loadThreshold15Min.isSome()) {
      if (load.get().fifteen >= loadThreshold15Min.get()) {
        LOG(INFO) << "System 15 minutes load average " << load.get().fifteen
                  << " reached threshold " << loadThreshold15Min.get();
        overloaded = true;
      }
    }

    return overloaded;
}

Resources makeRevocable(Resources const & any) {
    Resources revocable;
    for(auto resource: any) {
        resource.mutable_revocable();
        revocable += resource;
    }
    return revocable;
}

ThresholdResourceEstimator::ThresholdResourceEstimator(
    std::function<Try<os::Load>()> const & load,
    Resources const & fixed,
    Option<double> const & loadThreshold1Min,
    Option<double> const & loadThreshold5Min,
    Option<double> const & loadThreshold15Min
) :
    load{load},
    fixed{makeRevocable(fixed)},
    loadThreshold1Min{loadThreshold1Min},
    loadThreshold5Min{loadThreshold5Min},
    loadThreshold15Min{loadThreshold15Min}
{
};

Try<Nothing> ThresholdResourceEstimator::initialize(std::function<Future<ResourceUsage>()> const & usage) {
    if (process.get() != nullptr) {
        return Error("Threshold resource estimator has already been initialized");
    }

    process.reset(new ThresholdResourceEstimatorProcess(
        usage, load, fixed,
        loadThreshold1Min, loadThreshold5Min, loadThreshold15Min
    ));
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
    Option<Resources> resources;
    Option<double> loadThreshold1Min = None();
    Option<double> loadThreshold5Min = None();
    Option<double> loadThreshold15Min = None();

    for(auto const & parameter: parameters.parameter()) {
        // Parse the resource to offer for oversubscription
        if (parameter.key() == "resources") {
            Try<Resources> parsed = Resources::parse(parameter.value());
            if (parsed.isError()) {
                LOG(ERROR) << "Failed to parse resources to offer for oversubscription: " << parsed.error();
                return nullptr;
            }
            resources = parsed.get();
        }

        // Parse any thresholds
        if (parameter.key() == "load_threshold_1min") {
          Try<double> thresholdParam = numify<double>(parameter.value());
          if (thresholdParam.isError()) {
            LOG(ERROR) << "Failed to parse 1 min load threshold: " << thresholdParam.error();
            return nullptr;
          }
          loadThreshold1Min = thresholdParam.get();

        } else if (parameter.key() == "load_threshold_5min") {
          Try<double> thresholdParam = numify<double>(parameter.value());
          if (thresholdParam.isError()) {
            LOG(ERROR) << "Failed to parse 5 min load threshold: " << thresholdParam.error();
            return nullptr;
          }
          loadThreshold5Min = thresholdParam.get();

        } else if (parameter.key() == "load_threshold_15min") {
          // Try to parse the load 15min value.
          Try<double> thresholdParam = numify<double>(parameter.value());
          if (thresholdParam.isError()) {
            LOG(ERROR) << "Failed to parse 15 min load threshold: " << thresholdParam.error();
            return nullptr;
          }
          loadThreshold15Min = thresholdParam.get();

        }
    }

    if (resources.isNone()) {
        LOG(ERROR) << "No resources specified for ThresholdResourceEstimator";
        return nullptr;
    }

    return new ThresholdResourceEstimator(
        os::loadavg,
        resources.get(),
        loadThreshold1Min,
        loadThreshold5Min,
        loadThreshold15Min
    );
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
