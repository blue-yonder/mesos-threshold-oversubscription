#include "threshold.hpp"

#include <stout/os.hpp>

#include <glog/logging.h>

#include "os.hpp"



namespace com { namespace blue_yonder { namespace threshold {


bool memExceedsThreshold(
        std::function<Try<os::MemInfo>()> const & memory,
        Bytes const & memThreshold)
{
    auto const memoryInfo = memory();

    if (memoryInfo.isError()) {
        LOG(ERROR) << "Failed to fetch memory information: " << memoryInfo.error()
                   << ". Assuming memory threshold to be exceeded";
        return true;
    }

    auto usedMemory = memoryInfo.get().total - memoryInfo.get().free - memoryInfo.get().cached;
    if (usedMemory >= memThreshold) {
        LOG(INFO) << "Total memory used " << usedMemory.megabytes() << " MB "
                  << "reached threshold " << memThreshold.megabytes() << " MB.";
        return true;
    }
    return false;
}

bool loadExceedsThresholds(
        std::function<Try<::os::Load>()> const & load,
        ::os::Load const & threshold)
{
    Try<::os::Load> const currentLoad = load();

    if (currentLoad.isError()) {
        LOG(ERROR) << "Failed to fetch system load: " + currentLoad.error()
                   << ". Assuming load thresholds to be exceeded.";
        return true;
    }

    if (currentLoad.get().one >= threshold.one) {
        LOG(INFO) << "System 1 minute load average " << currentLoad.get().one
                  << " reached threshold " <<  threshold.one << ".";
        return true;
    }

    if (currentLoad.get().five >= threshold.five) {
        LOG(INFO) << "System 5 minutes load average " << currentLoad.get().five
                  << " reached threshold " << threshold.five << ".";
        return true;
    }

    if (currentLoad.get().fifteen >= threshold.fifteen) {
        LOG(INFO) << "System 15 minutes load average " << currentLoad.get().fifteen
                  << " reached threshold " << threshold.fifteen << ".";
        return true;
    }
    return false;
}

} } }
