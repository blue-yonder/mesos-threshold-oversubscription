#pragma once

#include <mesos/module/resource_estimator.hpp>

namespace os {
    struct Load;
    struct Memory;
}

namespace com { namespace blue_yonder {

class ThresholdResourceEstimatorProcess;

class ThresholdResourceEstimator : public mesos::slave::ResourceEstimator
{
public:
    ThresholdResourceEstimator(
        std::function<Try<os::Load>()> const & load,
        std::function<Try<os::Memory>()> const & memory,
        mesos::Resources const & fixed,
        Option<double> const & loadThreshold1Min,
        Option<double> const & loadThreshold5Min,
        Option<double> const & loadThreshold15Min,
        Option<Bytes> const & memThreshold
    );
    virtual Try<Nothing> initialize(const std::function<process::Future<mesos::ResourceUsage>()>&) final;
    virtual process::Future<mesos::Resources> oversubscribable() final;
    virtual ~ThresholdResourceEstimator();

private:
    process::Owned<ThresholdResourceEstimatorProcess> process;
    std::function<Try<os::Load>()> const load;
    std::function<Try<os::Memory>()> const memory;
    mesos::Resources const fixed;
    Option<double> const loadThreshold1Min;
    Option<double> const loadThreshold5Min;
    Option<double> const loadThreshold15Min;
    Option<Bytes> const memThreshold;
};

} }
