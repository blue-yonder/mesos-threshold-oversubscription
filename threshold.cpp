#include "threshold.hpp"

#include <stout/os.hpp>

#include "os.hpp"

#include <glog/logging.h>


namespace com { namespace blue_yonder { namespace threshold {


bool memExceedsThreshold(
        std::function<Try<os::MemInfo>()> const & memory,
        Option<Bytes> const & memThreshold)
{
    if (memThreshold.isSome()) {
        auto const memoryInfo = memory();

        if (memoryInfo.isError()) {
            LOG(ERROR) << "Failed to fetch memory information: " << memoryInfo.error()
                       << ". Assuming memory threshold to be exceeded";
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

bool loadExceedsThresholds(
        std::function<Try<::os::Load>()> const & load,
        Option<double> const & loadThreshold1Min,
        Option<double> const & loadThreshold5Min,
        Option<double> const & loadThreshold15Min)
{
    if (loadThreshold1Min.isSome() or loadThreshold5Min.isSome() or loadThreshold15Min.isSome()) {
        Try<::os::Load> const loadInfo = load();

        if (loadInfo.isError()) {
            LOG(ERROR) << "Failed to fetch system loadInfo: " + loadInfo.error()
                       << ". Assuming load thresholds to be exceeded.";
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

} } }
