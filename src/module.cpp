#include <limits>

#include <stout/os.hpp>

#include "threshold_qos_controller.hpp"
#include "threshold_resource_estimator.hpp"

#include "os.hpp"
#include "threshold.hpp"

using mesos::Resources;
using ::os::Load;
using com::blue_yonder::ThresholdResourceEstimator;
using com::blue_yonder::ThresholdQoSController;


namespace {

struct ParsingError
{
  std::string message;

  ParsingError(std::string const& description, std::string const& error)
    : message("Failed to parse " + description + ": " + error)
  {}
};

double parseDouble(std::string const& value, std::string const& description) {
  auto thresholdParam = numify<double>(value);
  if (thresholdParam.isError()) {
    throw ParsingError{description, thresholdParam.error()};
  }
  return thresholdParam.get();
}

template <typename Interface, typename ThresholdActor>
static Interface* create(mesos::Parameters const& parameters) {
  Resources resources;
  Load loadThreshold = {
    std::numeric_limits<double>::max(),
    std::numeric_limits<double>::max(),
    std::numeric_limits<double>::max()};
  Bytes memThreshold = std::numeric_limits<uint64_t>::max();

  try {
    for (auto const& parameter : parameters.parameter()) {
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
        loadThreshold.one = parseDouble(parameter.value(), "1 min load threshold");
      } else if (parameter.key() == "load_threshold_5min") {
        loadThreshold.five = parseDouble(parameter.value(), "5 min load threshold");
      } else if (parameter.key() == "load_threshold_15min") {
        loadThreshold.fifteen = parseDouble(parameter.value(), "15 min load threshold");
      } else if (parameter.key() == "mem_threshold") {
        auto thresholdParam = Bytes::parse(parameter.value() + "MB");
        if (thresholdParam.isError()) {
          throw ParsingError("memory threshold", thresholdParam.error());
        }
        memThreshold = thresholdParam.get();
      }
    }
  } catch (ParsingError e) {
    LOG(ERROR) << e.message;
    return nullptr;
  }

  return new ThresholdActor(
    os::loadavg, com::blue_yonder::os::meminfo, resources, loadThreshold, memThreshold);
}

static mesos::slave::ResourceEstimator* createEstimator(mesos::Parameters const& parameters) {
  return create<mesos::slave::ResourceEstimator, ThresholdResourceEstimator>(parameters);
}

static mesos::slave::QoSController* createController(mesos::Parameters const& parameters) {
  return create<mesos::slave::QoSController, ThresholdQoSController>(parameters);
}

static bool compatible() {
  return true; // TODO this might be slightly overoptimistic
}

} // namespace {

mesos::modules::Module<mesos::slave::ResourceEstimator> com_blue_yonder_ThresholdResourceEstimator(
  MESOS_MODULE_API_VERSION,
  MESOS_VERSION,
  "Matthias Bach",
  "matthias.bach@blue-yonder.com",
  "Threshold Resource Estimator Module.",
  compatible,
  createEstimator);


mesos::modules::Module<mesos::slave::QoSController> com_blue_yonder_ThresholdQoSController(
  MESOS_MODULE_API_VERSION,
  MESOS_VERSION,
  "Stephan Erb",
  "stephan.erb@blue-yonder.com",
  "Threshold QoS Controller Module.",
  compatible,
  createController);
