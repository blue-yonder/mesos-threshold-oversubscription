#include "threshold_resource_estimator.hpp"

#include <limits>

#include <stout/os.hpp>

#include <process/defer.hpp>
#include <process/dispatch.hpp>
#include <process/id.hpp>
#include <process/process.hpp>

#include "os.hpp"
#include "threshold.hpp"

using process::dispatch;
using process::Failure;
using process::Future;
using process::Process;
using process::Owned;

using mesos::Resources;
using mesos::ResourceUsage;

using com::blue_yonder::ThresholdResourceEstimator;
using com::blue_yonder::ThresholdResourceEstimatorProcess;

using ::os::Load;

class ThresholdResourceEstimatorProcess : public Process<ThresholdResourceEstimatorProcess>
{
public:
    ThresholdResourceEstimatorProcess(
        std::function<Future<ResourceUsage>()> const &,
        std::function<Try<Load>()> const &,
        std::function<Try<os::MemInfo>()> const &,
        Resources const &,
        Load const &,
        Bytes const &
    );
    Future<Resources> oversubscribable();
private:
    Future<Resources> calcUnusedResources(ResourceUsage const & usage);  // process::defer can't handle const methods

    std::function<Future<ResourceUsage>()> const usage;
    std::function<Try<Load>()> const load;
    std::function<Try<os::MemInfo>()> const memory;
    Resources const totalRevocable;
    Load const loadThreshold;
    Bytes const memThreshold;
};


ThresholdResourceEstimatorProcess::ThresholdResourceEstimatorProcess(
    std::function<Future<ResourceUsage>()> const & usage,
    std::function<Try<Load>()> const & load,
    std::function<Try<os::MemInfo>()> const & memory,
    Resources const & totalRevocable,
    Load const & loadThreshold,
    Bytes const & memThreshold
) : ProcessBase(process::ID::generate("threshold-resource-estimator")),
    usage{usage},
    load{load},
    memory{memory},
    totalRevocable{totalRevocable},
    loadThreshold{loadThreshold},
    memThreshold{memThreshold}
{}

Future<Resources> ThresholdResourceEstimatorProcess::oversubscribable()
{
    bool cpu_overload = threshold::loadExceedsThresholds(load, loadThreshold);
    bool mem_overload = threshold::memExceedsThreshold(memory, memThreshold);

    if (cpu_overload or mem_overload) {
        return Resources();
    }

    return usage().then(process::defer(self(), &Self::calcUnusedResources, std::placeholders::_1));
}

Future<Resources> ThresholdResourceEstimatorProcess::calcUnusedResources(ResourceUsage const & usage) {
    Resources allocatedRevocable;
    for (auto const & executor: usage.executors()) {
        allocatedRevocable += Resources(executor.allocated()).revocable();
    }

    return totalRevocable - allocatedRevocable;
}


namespace {

Resources makeRevocable(Resources const & any) {
    Resources revocable;
    for (auto resource: any) {
        resource.mutable_revocable();
        revocable += resource;
    }
    return revocable;
}

}

ThresholdResourceEstimator::ThresholdResourceEstimator(
    std::function<Try<Load>()> const & load,
    std::function<Try<os::MemInfo>()> const & memory,
    Resources const & totalRevocable,
    Load const & loadThreshold,
    Bytes const & memThreshold
) :
    load{load},
    memory{memory},
    totalRevocable{makeRevocable(totalRevocable)},
    loadThreshold{loadThreshold},
    memThreshold{memThreshold}
{};

Try<Nothing> ThresholdResourceEstimator::initialize(std::function<Future<ResourceUsage>()> const & usage) {
    if (process.get() != nullptr) {
        return Error("Threshold resource estimator has already been initialized");
    }

    process.reset(new ThresholdResourceEstimatorProcess(
        usage,
        load,
        memory,
        totalRevocable,
        loadThreshold,
        memThreshold
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

struct ParsingError {
    std::string message;

    ParsingError(std::string const & parameter_description, std::string const & error_message)
        : message("Failed to parse " + parameter_description + ": " + error_message)
    {}
};

double parse_double_parameter(std::string const & value, std::string const & parameter_description) {
    auto thresholdParam = numify<double>(value);
    if (thresholdParam.isError()) {
        throw ParsingError{parameter_description, thresholdParam.error()};
    }
    return thresholdParam.get();
}

static mesos::slave::ResourceEstimator* create(mesos::Parameters const & parameters) {
    Option<Resources> resources;
    Load loadThreshold = {
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max()};
    Bytes memThreshold = std::numeric_limits<uint64_t>::max();

    try {
        for (auto const & parameter: parameters.parameter()) {
            // Parse the resource to offer for oversubscription
            if (parameter.key() == "resources") {
                Try<Resources> parsed = Resources::parse(parameter.value());
                if (parsed.isError()) {
                    throw ParsingError("resources", parsed.error());
                }
                resources = parsed.get();
            }

            // Parse any thresholds
            if (parameter.key() == "load_threshold_1min") {
                loadThreshold.one = parse_double_parameter(parameter.value(), "1 min load threshold");
            } else if (parameter.key() == "load_threshold_5min") {
                loadThreshold.five = parse_double_parameter(parameter.value(), "5 min load threshold");
            } else if (parameter.key() == "load_threshold_15min") {
                loadThreshold.fifteen = parse_double_parameter(parameter.value(), "15 min load threshold");
            } else if (parameter.key() == "mem_threshold") {
                auto thresholdParam = Bytes::parse(parameter.value() + "MB");
                if (thresholdParam.isError()) {
                    throw ParsingError("memory threshold", thresholdParam.error());
                }
                memThreshold = thresholdParam.get();
            }
        }
    } catch(ParsingError e) {
        LOG(ERROR) << e.message;
        return nullptr;
    }

    if (resources.isNone()) {
        LOG(ERROR) << "No resources specified for ThresholdResourceEstimator";
        return nullptr;
    }

    return new ThresholdResourceEstimator(
        os::loadavg,
        com::blue_yonder::os::meminfo,
        resources.get(),
        loadThreshold,
        memThreshold
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
