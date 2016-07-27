#pragma once

#include <mesos/module/resource_estimator.hpp>

namespace com { namespace blue_yonder {

class ThresholdResourceEstimatorProcess;

class ThresholdResourceEstimator : public mesos::slave::ResourceEstimator
{
public:
    ThresholdResourceEstimator(mesos::Resources const & fixed);
    virtual Try<Nothing> initialize(const std::function<process::Future<mesos::ResourceUsage>()>&) final;
    virtual process::Future<mesos::Resources> oversubscribable() final;
    virtual ~ThresholdResourceEstimator();

private:
    process::Owned<ThresholdResourceEstimatorProcess> process;
    mesos::Resources const fixed;
};

} }
