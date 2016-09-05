#include "threshold_qos_controller.hpp"

#include <list>
#include <limits>

#include <stout/os.hpp>

#include <glog/logging.h>

#include <process/defer.hpp>
#include <process/dispatch.hpp>
#include <process/id.hpp>
#include <process/process.hpp>

#include "os.hpp"
#include "threshold.hpp"

using std::list;

using process::dispatch;
using process::Failure;
using process::Future;
using process::Process;
using process::Owned;

using mesos::Resources;
using mesos::ResourceUsage;
using mesos::slave::QoSController;
using mesos::slave::QoSCorrection;

using com::blue_yonder::ThresholdQoSController;
using com::blue_yonder::ThresholdQoSControllerProcess;

using ::os::Load;


class ThresholdQoSControllerProcess : public Process<ThresholdQoSControllerProcess>
{
public:
    ThresholdQoSControllerProcess(
        std::function<Future<ResourceUsage>()> const &,
        std::function<Try<Load>()> const &,
        std::function<Try<os::MemInfo>()> const &,
        Load const &,
        Bytes const &
    );
    process::Future<list<QoSCorrection>> corrections();
private:
    process::Future<list<QoSCorrection>> _corrections(ResourceUsage const & usage);  // process::defer can't handle const methods

    std::function<Future<ResourceUsage>()> const usage;
    std::function<Try<Load>()> const load;
    std::function<Try<os::MemInfo>()> const memory;
    Load const loadThreshold;
    Bytes const memThreshold;
};


ThresholdQoSControllerProcess::ThresholdQoSControllerProcess(
    std::function<Future<ResourceUsage>()> const & usage,
    std::function<Try<Load>()> const & load,
    std::function<Try<os::MemInfo>()> const & memory,
    Load const & loadThreshold,
    Bytes const & memThreshold
) : ProcessBase(process::ID::generate("threshold-qos-controller")),
    usage{usage},
    load{load},
    memory{memory},
    loadThreshold{loadThreshold},
    memThreshold{memThreshold}
{}

process::Future<list<QoSCorrection>> ThresholdQoSControllerProcess::corrections()
{
    return usage().then(process::defer(self(), &Self::_corrections, std::placeholders::_1));
}

process::Future<list<QoSCorrection>> ThresholdQoSControllerProcess::_corrections(ResourceUsage const & usage) {

    bool cpu_overload = threshold::loadExceedsThresholds(load, loadThreshold);
    bool mem_overload = threshold::memExceedsThreshold(memory, memThreshold);

    if (mem_overload) {
        return list<QoSCorrection>();
    if (cpu_overload)
        return list<QoSCorrection>();
    }
    return list<QoSCorrection>();
}


ThresholdQoSController::ThresholdQoSController(
    std::function<Try<Load>()> const & load,
    std::function<Try<os::MemInfo>()> const & memory,
    Load const & loadThreshold,
    Bytes const & memThreshold
) :
    load{load},
    memory{memory},
    loadThreshold{loadThreshold},
    memThreshold{memThreshold}
{};

Try<Nothing> ThresholdQoSController::initialize(std::function<Future<ResourceUsage>()> const & usage) {
    if (process.get() != nullptr) {
        return Error("ThresholdQoSController has already been initialized");
    }

    LOG(INFO) << "Initializing ThresholdQoSController. Load thresholds: "
              << loadThreshold.one << " " << loadThreshold.five << " " << loadThreshold.fifteen << " "
              << "Memory threshold: " << memThreshold.megabytes() << " MB.";

    process.reset(new ThresholdQoSControllerProcess(
        usage,
        load,
        memory,
        loadThreshold,
        memThreshold
    ));
    spawn(process.get());

    return Nothing();
}

process::Future<list<QoSCorrection>> ThresholdQoSController::corrections() {
    if (process.get() == nullptr) {
        return Failure("ThresholdQoSController is not initialized");
    }
    return dispatch(process.get(), &ThresholdQoSControllerProcess::corrections);
}

ThresholdQoSController::~ThresholdQoSController()
{
    if (process.get() != nullptr) {
        terminate(process.get());
        wait(process.get());
    }
}

