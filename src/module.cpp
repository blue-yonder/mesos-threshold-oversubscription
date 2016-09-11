#include <limits>

#include <stout/os.hpp>

#include "threshold_resource_estimator.hpp"
#include "threshold_qos_controller.hpp"


#include "os.hpp"
#include "threshold.hpp"

using mesos::Resources;
using ::os::Load;

namespace {

struct ParsingError {
    std::string message;

    ParsingError(std::string const & parameter_description, std::string const & error_message)
        : message("Failed to parse " + parameter_description + ": " + error_message)
    {}
};

double parse_double_parameter(std::string const & value, std::string const & parameter_description) {
    auto thresholdParam = numify<double>(value);
    if (thresholdParam.isError()) {
        throw ParsingError{parameter_description, thresholdParam.error()};
    }
    return thresholdParam.get();
}

template <typename Interface, typename ThresholdActor>
static Interface* create(mesos::Parameters const & parameters) {
    Resources resources;
    Load loadThreshold = {
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max()};
    Bytes memThreshold = std::numeric_limits<uint64_t>::max();

    try {
        for (auto const & parameter: parameters.parameter()) {
            // Parse the resource to offer for oversubscription
            if (parameter.key() == "resources") {
                Try<Resources> parsed = Resources::parse(parameter.value());
                if (parsed.isError()) {
                    throw ParsingError("resources", parsed.error());
                }
                resources = parsed.get();
            }

            // Parse any thresholds
            if (parameter.key() == "load_threshold_1min") {
                loadThreshold.one = parse_double_parameter(parameter.value(), "1 min load threshold");
            } else if (parameter.key() == "load_threshold_5min") {
                loadThreshold.five = parse_double_parameter(parameter.value(), "5 min load threshold");
            } else if (parameter.key() == "load_threshold_15min") {
                loadThreshold.fifteen = parse_double_parameter(parameter.value(), "15 min load threshold");
            } else if (parameter.key() == "mem_threshold") {
                auto thresholdParam = Bytes::parse(parameter.value() + "MB");
                if (thresholdParam.isError()) {
                    throw ParsingError("memory threshold", thresholdParam.error());
                }
                memThreshold = thresholdParam.get();
            }
        }
    } catch(ParsingError e) {
        LOG(ERROR) << e.message;
        return nullptr;
    }

    return new ThresholdActor(
        os::loadavg,
        com::blue_yonder::os::meminfo,
        resources,
        loadThreshold,
        memThreshold
    );
}

static mesos::slave::ResourceEstimator* create_estimator(mesos::Parameters const & parameters) {
    return create<mesos::slave::ResourceEstimator, com::blue_yonder::ThresholdResourceEstimator>(parameters);
}

static mesos::slave::QoSController* create_controller(mesos::Parameters const & parameters) {
    return create<mesos::slave::QoSController, com::blue_yonder::ThresholdQoSController>(parameters);
}

static bool compatible() {
    return true;  // TODO this might be slightly overoptimistic
}

}

mesos::modules::Module<mesos::slave::ResourceEstimator> com_blue_yonder_ThresholdResourceEstimator(
    MESOS_MODULE_API_VERSION,
    MESOS_VERSION,
    "Matthias Bach",
    "matthias.bach@blue-yonder.com",
    "Threshold Resource Estimator Module.",
    compatible,
    create_estimator
);


mesos::modules::Module<mesos::slave::QoSController> com_blue_yonder_ThresholdQoSController(
    MESOS_MODULE_API_VERSION,
    MESOS_VERSION,
    "Stephan Erb",
    "stephan.erb@blue-yonder.com",
    "Threshold QoS Controller Module.",
    compatible,
    create_controller
);
