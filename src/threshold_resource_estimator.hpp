#pragma once

#include <mesos/module/resource_estimator.hpp>

#include <functional>

#include <stout/bytes.hpp>
#include <stout/os.hpp>


namespace com { namespace blue_yonder {

namespace os {
    struct MemInfo;
}

class ThresholdResourceEstimatorProcess;

class ThresholdResourceEstimator : public mesos::slave::ResourceEstimator
{
public:
    ThresholdResourceEstimator(
        std::function<Try<::os::Load>()> const & load,
        std::function<Try<os::MemInfo>()> const & memory,
        mesos::Resources const & totalRevocable,
        ::os::Load const & loadThreshold,
        Bytes const & memThreshold
    );
    virtual Try<Nothing> initialize(const std::function<process::Future<mesos::ResourceUsage>()>&) final;
    virtual process::Future<mesos::Resources> oversubscribable() final;
    virtual ~ThresholdResourceEstimator();

private:
    process::Owned<ThresholdResourceEstimatorProcess> process;
    std::function<Try<::os::Load>()> const load;
    std::function<Try<os::MemInfo>()> const memory;
    mesos::Resources const totalRevocable;
    ::os::Load const loadThreshold;
    Bytes const memThreshold;
};

} }
