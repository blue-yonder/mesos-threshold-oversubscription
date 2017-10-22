#include "threshold_resource_estimator.hpp"

#include <limits>

#include <stout/os.hpp>

#include <glog/logging.h>

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

using mesos::Resource;
using mesos::Resources;
using mesos::ResourceUsage;

using com::blue_yonder::ThresholdResourceEstimator;
using com::blue_yonder::ThresholdResourceEstimatorProcess;

using ::os::Load;


namespace {

Resources unallocated(const Resources& resources) {
  Resources result = resources;
  result.unallocate();
  return result;
}

Resources makeRevocable(Resources const& any) {
  Resources revocable;
  for (mesos::Resource resource : any) {
    resource.mutable_revocable();
    revocable += resource;
  }
  return revocable;
}

} // namespace {


class ThresholdResourceEstimatorProcess : public Process<ThresholdResourceEstimatorProcess>
{
public:
  ThresholdResourceEstimatorProcess(
    std::function<Future<ResourceUsage>()> const&,
    std::function<Try<Load>()> const&,
    std::function<Try<os::MemInfo>()> const&,
    Resources const&,
    Load const&,
    Bytes const&);
  Future<Resources> oversubscribable();

private:
  Future<Resources> calcUnusedResources(ResourceUsage const& usage);

  std::function<Future<ResourceUsage>()> const usage;
  std::function<Try<Load>()> const load;
  std::function<Try<os::MemInfo>()> const memory;
  Resources const totalRevocable;
  Load const loadThreshold;
  Bytes const memThreshold;
};


ThresholdResourceEstimatorProcess::ThresholdResourceEstimatorProcess(
  std::function<Future<ResourceUsage>()> const& usage,
  std::function<Try<Load>()> const& load,
  std::function<Try<os::MemInfo>()> const& memory,
  Resources const& totalRevocable,
  Load const& loadThreshold,
  Bytes const& memThreshold)
  : ProcessBase(process::ID::generate("threshold-resource-estimator")),
    usage{usage},
    load{load},
    memory{memory},
    totalRevocable{totalRevocable},
    loadThreshold(loadThreshold),
    memThreshold(memThreshold)
{}

Future<Resources> ThresholdResourceEstimatorProcess::oversubscribable() {
  bool cpuOverload = threshold::loadExceedsThreshold(load, loadThreshold);
  bool memOverload = threshold::memExceedsThreshold(memory, memThreshold);

  if (cpuOverload or memOverload) {
    return Resources();
  }

  return usage().then(process::defer(self(), &Self::calcUnusedResources, std::placeholders::_1));
}

Future<Resources> ThresholdResourceEstimatorProcess::calcUnusedResources(ResourceUsage const& usage) {
  Resources allocatedRevocable;
  for (auto const& executor : usage.executors()) {
    allocatedRevocable += Resources(executor.allocated()).revocable();
  }
  return totalRevocable - unallocated(allocatedRevocable);
}


ThresholdResourceEstimator::ThresholdResourceEstimator(
  std::function<Try<Load>()> const& load,
  std::function<Try<os::MemInfo>()> const& memory,
  Resources const& totalRevocable,
  Load const& loadThreshold,
  Bytes const& memThreshold)
  : load{load},
    memory{memory},
    totalRevocable{makeRevocable(totalRevocable)},
    loadThreshold(loadThreshold),
    memThreshold(memThreshold)
{}

Try<Nothing> ThresholdResourceEstimator::initialize(
    std::function<Future<ResourceUsage>()> const& usage)
{
  if (process.get() != nullptr) {
    return Error("ThresholdResourceEstimator has already been initialized");
  }

  LOG(INFO) << "Initializing ThresholdResourceEstimator. Load thresholds: " << loadThreshold.one
            << " " << loadThreshold.five << " " << loadThreshold.fifteen << " "
            << "Memory threshold: " << memThreshold.megabytes() << " MB";

  process.reset(new ThresholdResourceEstimatorProcess(
    usage,
    load,
    memory,
    totalRevocable,
    loadThreshold,
    memThreshold));
  spawn(process.get());

  return Nothing();
}

Future<Resources> ThresholdResourceEstimator::oversubscribable() {
  if (process.get() == nullptr) {
    return Failure("ThresholdResourceEstimator is not initialized");
  }
  return dispatch(process.get(), &ThresholdResourceEstimatorProcess::oversubscribable);
}

ThresholdResourceEstimator::~ThresholdResourceEstimator() {
  if (process.get() != nullptr) {
    terminate(process.get());
    wait(process.get());
  }
}
