#include "threshold.hpp"

#include <stout/os.hpp>

#include <glog/logging.h>

#include "os.hpp"


namespace com {
namespace blue_yonder {
namespace threshold {

/*
 * Returns true if the current memory usage (not including buffers and caches)
 * exceeds the given threshold.
 */
bool memExceedsThreshold(
    std::function<Try<os::MemInfo>()> const& memory,
    Bytes const& memThreshold)
{
  auto const memoryInfo = memory();

  if (memoryInfo.isError()) {
    LOG(ERROR) << "Failed to fetch memory information: " << memoryInfo.error()
               << ". Assuming memory threshold to be exceeded";
    return true;
  }

  auto const usedMemory = memoryInfo.get().total - memoryInfo.get().free - memoryInfo.get().cached;

  if (usedMemory >= memThreshold) {
    LOG(INFO) << "Total memory used " << usedMemory
              << "reached threshold " << memThreshold;
    return true;
  }
  return false;
}

/*
 * Returns true if the current load has reached one of the given thresholds.
 *
 * We only consider the 15m load threshold to be reached if also the respective
 * shorter load intervals have reached the same threshold. This ensures that we
 * don't make a decision based on overload of the 15m threshold, even though
 * the situation has already improved in the more recent past (1m or 5m) but not
 * yet sufficently affected the longer interval. The same holds for the 5m load
 * threshold.
 */
bool loadExceedsThreshold(
    std::function<Try<::os::Load>()> const& load,
    ::os::Load const& threshold)
{
  Try<::os::Load> const currentLoad = load();

  if (currentLoad.isError()) {
    LOG(ERROR) << "Failed to fetch system load: " + currentLoad.error()
               << ". Assuming load thresholds to be exceeded";
    return true;
  }

  if (currentLoad.get().one >= threshold.one) {
    LOG(INFO) << "System 1 minute load average " << currentLoad.get().one
              << " reached threshold " << threshold.one;
    return true;
  }

  if (currentLoad.get().five >= threshold.five &&
      currentLoad.get().one >= threshold.five) {
    LOG(INFO) << "System 5 minutes load average " << currentLoad.get().five
              << " reached threshold " << threshold.five;
    return true;
  }

  if (currentLoad.get().fifteen >= threshold.fifteen &&
      currentLoad.get().five >= threshold.fifteen &&
      currentLoad.get().one >= threshold.fifteen) {
    LOG(INFO) << "System 15 minutes load average " << currentLoad.get().fifteen
              << " reached threshold " << threshold.fifteen;
    return true;
  }
  return false;
}

} // namespace threshold {
} // namespace blue_yonder {
} // namespace com {
