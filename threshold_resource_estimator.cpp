#include "threshold_resource_estimator.hpp"

#include <stout/os.hpp>

#include <process/defer.hpp>
#include <process/dispatch.hpp>
#include <process/process.hpp>

#include "os.hpp"

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
        std::function<Future<ResourceUsage>()> const &,
        std::function<Try<::os::Load>()> const &,
        std::function<Try<os::MemInfo>()> const &,
        Resources const &,
        Option<double> const &, Option<double> const &, Option<double> const &,
        Option<Bytes> const &
    );
    Future<Resources> oversubscribable();
private:
    Future<Resources> calcUnusedResources(ResourceUsage const & usage);  // process::defer can't handle const methods
    bool loadExceedsThresholds() const;
    bool memExceedsThreshold() const;

    std::function<Future<ResourceUsage>()> const usage;
    std::function<Try<::os::Load>()> const load;
    std::function<Try<os::MemInfo>()> const memory;
    Resources const totalRevocable;
    Option<double> const loadThreshold1Min;
    Option<double> const loadThreshold5Min;
    Option<double> const loadThreshold15Min;
    Option<Bytes> const memThreshold;
};


ThresholdResourceEstimatorProcess::ThresholdResourceEstimatorProcess(
    std::function<Future<ResourceUsage>()> const & usage,
    std::function<Try<::os::Load>()> const & load,
    std::function<Try<os::MemInfo>()> const & memory,
    Resources const & totalRevocable,
    Option<double> const & loadThreshold1Min,
    Option<double> const & loadThreshold5Min,
    Option<double> const & loadThreshold15Min,
    Option<Bytes> const & memThreshold
) :
    usage{usage},
    load{load},
    memory{memory},
    totalRevocable{totalRevocable},
    loadThreshold1Min{loadThreshold1Min},
    loadThreshold5Min{loadThreshold5Min},
    loadThreshold15Min{loadThreshold15Min},
    memThreshold{memThreshold}
{}

Future<Resources> ThresholdResourceEstimatorProcess::oversubscribable()
{
    if (loadExceedsThresholds() or memExceedsThreshold()) {
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

bool ThresholdResourceEstimatorProcess::loadExceedsThresholds() const {
    if (loadThreshold1Min.isSome() or loadThreshold5Min.isSome() or loadThreshold15Min.isSome()) {
        Try<::os::Load> const loadInfo = load();

        if (loadInfo.isError()) {
            LOG(ERROR) << "Failed to fetch system loadInfo: " + loadInfo.error();
            LOG(INFO)  << "Assuming load thresholds to be exceeded.";
            return true;
        }

        if (loadThreshold1Min.isSome() and loadInfo.get().one >= loadThreshold1Min.get()) {
            LOG(INFO) << "System 1 minute load average " << loadInfo.get().one
                      << " reached threshold " << loadThreshold1Min.get() << ".";
            return true;
        }

        if (loadThreshold5Min.isSome() and loadInfo.get().five >= loadThreshold5Min.get()) {
            LOG(INFO) << "System 5 minutes load average " << loadInfo.get().five
                      << " reached threshold " << loadThreshold5Min.get() << ".";
            return true;
        }

        if (loadThreshold15Min.isSome() and loadInfo.get().fifteen >= loadThreshold15Min.get()) {
            LOG(INFO) << "System 15 minutes load average " << loadInfo.get().fifteen
                      << " reached threshold " << loadThreshold15Min.get() << ".";
            return true;
        }
    }
    return false;
}


bool ThresholdResourceEstimatorProcess::memExceedsThreshold() const {
    if (memThreshold.isSome()) {
        auto const memoryInfo = memory();

        if (memoryInfo.isError()) {
            LOG(ERROR) << "Failed to fetch memory information: " << memoryInfo.error();
            LOG(INFO)  << "Assuming memory threshold to be exceeded";
            return true;
        }

        auto usedMemory = memoryInfo.get().total - memoryInfo.get().free - memoryInfo.get().cached;
        if (usedMemory >= memThreshold.get()) {
            LOG(INFO) << "Total memory used " << usedMemory.megabytes() << " MB "
                      << "reached threshold " << memThreshold.get().megabytes() << " MB.";
            return true;
        }
    }
    return false;
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
    std::function<Try<::os::Load>()> const & load,
    std::function<Try<os::MemInfo>()> const & memory,
    Resources const & totalRevocable,
    Option<double> const & loadThreshold1Min,
    Option<double> const & loadThreshold5Min,
    Option<double> const & loadThreshold15Min,
    Option<Bytes> const & memThreshold
) :
    load{load},
    memory{memory},
    totalRevocable{makeRevocable(totalRevocable)},
    loadThreshold1Min{loadThreshold1Min},
    loadThreshold5Min{loadThreshold5Min},
    loadThreshold15Min{loadThreshold15Min},
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
        loadThreshold1Min,
        loadThreshold5Min,
        loadThreshold15Min,
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
    Option<double> loadThreshold1Min;
    Option<double> loadThreshold5Min;
    Option<double> loadThreshold15Min;
    Option<Bytes> memThreshold;

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
                loadThreshold1Min = parse_double_parameter(parameter.value(), "1 min load threshold");
            } else if (parameter.key() == "load_threshold_5min") {
                loadThreshold5Min = parse_double_parameter(parameter.value(), "5 min load threshold");
            } else if (parameter.key() == "load_threshold_15min") {
                loadThreshold15Min = parse_double_parameter(parameter.value(), "15 min load threshold");
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
        loadThreshold1Min,
        loadThreshold5Min,
        loadThreshold15Min,
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
