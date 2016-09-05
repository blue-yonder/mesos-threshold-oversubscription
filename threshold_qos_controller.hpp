#pragma once

#include <mesos/module/qos_controller.hpp>

#include <functional>

#include <stout/bytes.hpp>
#include <stout/os.hpp>


namespace com { namespace blue_yonder {

namespace os {
    struct MemInfo;
}

class ThresholdQoSControllerProcess;

class ThresholdQoSController : public mesos::slave::QoSController
{
public:
    ThresholdQoSController(
        std::function<Try<::os::Load>()> const & load,
        std::function<Try<os::MemInfo>()> const & memory,
        ::os::Load const & loadThreshold,
        Bytes const & memThreshold
    );
    virtual Try<Nothing> initialize(const std::function<process::Future<mesos::ResourceUsage>()>&) final;
    virtual process::Future<std::list<mesos::slave::QoSCorrection>> corrections() final;
    virtual ~ThresholdQoSController();

private:
    process::Owned<ThresholdQoSControllerProcess> process;
    std::function<Try<::os::Load>()> const load;
    std::function<Try<os::MemInfo>()> const memory;
    ::os::Load const loadThreshold;
    Bytes const memThreshold;
};

} }
