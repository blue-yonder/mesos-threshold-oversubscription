#include "threshold_qos_controller.hpp"

#include <algorithm>
#include <limits>
#include <list>

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
    std::function<Future<ResourceUsage>()> const&,
    std::function<Try<Load>()> const&,
    std::function<Try<os::MemInfo>()> const&,
    Load const&,
    Bytes const&);
  Future<list<QoSCorrection>> corrections();

private:
  Future<list<QoSCorrection>> _corrections(ResourceUsage const& usage);

  std::function<Future<ResourceUsage>()> const usage;
  std::function<Try<Load>()> const load;
  std::function<Try<os::MemInfo>()> const memory;
  Load const loadThreshold;
  Bytes const memThreshold;
};


ThresholdQoSControllerProcess::ThresholdQoSControllerProcess(
  std::function<Future<ResourceUsage>()> const& usage,
  std::function<Try<Load>()> const& load,
  std::function<Try<os::MemInfo>()> const& memory,
  Load const& loadThreshold,
  Bytes const& memThreshold)
  : ProcessBase(process::ID::generate("threshold-qos-controller")),
    usage{usage},
    load{load},
    memory{memory},
    loadThreshold{loadThreshold},
    memThreshold{memThreshold}
{}

Future<list<QoSCorrection>> ThresholdQoSControllerProcess::corrections() {
  return usage().then(process::defer(self(), &Self::_corrections, std::placeholders::_1));
}

namespace {

QoSCorrection killCorrection(ResourceUsage::Executor const& executor) {
  QoSCorrection correction;
  correction.set_type(mesos::slave::QoSCorrection_Type_KILL);
  correction.mutable_kill()->mutable_framework_id()->CopyFrom(
    executor.executor_info().framework_id());
  correction.mutable_kill()->mutable_executor_id()->CopyFrom(
    executor.executor_info().executor_id());
  return correction;
}

bool mostGreedyRevocable(ResourceUsage::Executor const& a, ResourceUsage::Executor const& b) {
  auto const memA =
    Resources(a.allocated()).revocable().empty() ? 0 : a.statistics().mem_total_bytes();
  auto const memB =
    Resources(b.allocated()).revocable().empty() ? 0 : b.statistics().mem_total_bytes();
  return (memA < memB);
}

} // namespace {

Future<list<QoSCorrection>> ThresholdQoSControllerProcess::_corrections(ResourceUsage const& usage) {
  // We assume all tasks are run in cgroups so that a single task cannot
  // overload the entire host. The host memory may only be exceeded due to the
  // existence of revocable tasks.
  //
  // By killing a single revocable task per correction interval, we prevent
  // runs of the Linux OOM due to host memory pressure. The latter has the
  // disadvantage that it does not differentiate between revocable and non-
  // revocalbe tasks, therefore leading to potential SLA violations at our
  // end. (This could be changed if Mesos adopts the oom.victim cgroup)
  //
  // If there are revocable tasks, we kill the one that has the largest memory
  // footprint.
  if (threshold::memExceedsThreshold(memory, memThreshold)) {
    auto const most_greedy =
      std::max_element(usage.executors().begin(), usage.executors().end(), mostGreedyRevocable);

    if (usage.executors().end() != most_greedy &&
        !Resources(most_greedy->allocated()).revocable().empty()) {
      return list<QoSCorrection>{killCorrection(*most_greedy)};
    }
  }

  // We assume all tasks are run in cgroups with appropriate shares and quota.
  // This ensures that a  single cgroup cannot overload the entire host and
  // starve other tasks (even though there is a potetential risk of slightly
  // increased tail latency).
  //
  // This basic protection enables us to react to CPU overload situations in a
  // rather calm and defered fashion, i.e. kill a random task per correction
  // interval if any load threshold is exceeded.
  //
  // Killing a random tasks rather than the one that is using the most cpu time
  // is a simplificiation. Otherwise we would have to make this QoSController
  // stateful in order to measure which revocable task is using the most CPU time.
  if (threshold::loadExceedsThreshold(load, loadThreshold)) {
    foreach (ResourceUsage::Executor const& executor, usage.executors()) {
      if (!Resources(executor.allocated()).revocable().empty()) {
        return list<QoSCorrection>{killCorrection(executor)};
      }
    }
  }
  return list<QoSCorrection>();
}


ThresholdQoSController::ThresholdQoSController(
  std::function<Try<Load>()> const& load,
  std::function<Try<os::MemInfo>()> const& memory,
  mesos::Resources const& totalRevocable,
  Load const& loadThreshold,
  Bytes const& memThreshold)
  : load{load},
    memory{memory},
    loadThreshold{loadThreshold},
    memThreshold{memThreshold}
{}

Try<Nothing> ThresholdQoSController::initialize(std::function<Future<ResourceUsage>()> const& usage) {
  if (process.get() != nullptr) {
    return Error("ThresholdQoSController has already been initialized");
  }

  LOG(INFO) << "Initializing ThresholdQoSController. Load thresholds: "
            << loadThreshold.one << " " << loadThreshold.five << " " << loadThreshold.fifteen << " "
            << "Memory threshold: " << memThreshold.megabytes() << " MB";

  process.reset(new ThresholdQoSControllerProcess(
    usage,
    load,
    memory,
    loadThreshold,
    memThreshold));
  spawn(process.get());

  return Nothing();
}

process::Future<list<QoSCorrection>> ThresholdQoSController::corrections() {
  if (process.get() == nullptr) {
    return Failure("ThresholdQoSController is not initialized");
  }
  return dispatch(process.get(), &ThresholdQoSControllerProcess::corrections);
}

ThresholdQoSController::~ThresholdQoSController() {
  if (process.get() != nullptr) {
    terminate(process.get());
    wait(process.get());
  }
}
