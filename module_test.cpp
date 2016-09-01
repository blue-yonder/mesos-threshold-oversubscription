#include <stout/bytes.hpp>
#include <stout/dynamiclibrary.hpp>
#include <stout/os.hpp>
#include <stout/path.hpp>
#include <stout/version.hpp>

#include <process/owned.hpp>

#include <mesos/version.hpp>
#include <mesos/module.hpp>
#include <mesos/module/resource_estimator.hpp>

#include <gtest/gtest.h>

using std::string;

using process::Future;
using process::Owned;

using mesos::ResourceUsage;
using mesos::modules::ModuleBase;
using mesos::modules::Module;
using mesos::slave::ResourceEstimator;

namespace {

string const libraryName{"threshold_resource_estimator"};
string const moduleName{"com_blue_yonder_ThresholdResourceEstimator"};

class ThresholdResourceEstimatorTest : public ::testing::Test {
public:
    ThresholdResourceEstimatorTest()
        : dynamicLibrary(new DynamicLibrary()),
          moduleBase(nullptr)
    { }

    virtual void TearDown()
    {
        moduleBase = nullptr;
        dynamicLibrary->close();

        ::testing::Test::TearDown();
    }

    Try<ModuleBase*> loadModule();
    ResourceEstimator* createEstimator(mesos::Parameters const & params);

private:
    Owned<DynamicLibrary> dynamicLibrary;
    ModuleBase* moduleBase;
};

mesos::Parameters make_parameters(
    std::string fixed,
    Option<string> load_one, Option<string> load_five, Option<string> load_fifteen,
    Option<string> mem_threshold
) {
    mesos::Parameters parameters{};
    {
        auto * parameter = parameters.add_parameter();
        parameter->set_key("resources");
        parameter->set_value(fixed);
    }

    if (load_one.isSome()) {
        auto * parameter = parameters.add_parameter();
        parameter->set_key("load_threshold_1min");
        parameter->set_value(load_one.get());
    }

    if (load_five.isSome()) {
        auto * parameter = parameters.add_parameter();
        parameter->set_key("load_threshold_5min");
        parameter->set_value(load_five.get());
    }

    if (load_fifteen.isSome()) {
        auto * parameter = parameters.add_parameter();
        parameter->set_key("load_threshold_15min");
        parameter->set_value(load_fifteen.get());
    }

    if (mem_threshold.isSome()) {
        auto * parameter = parameters.add_parameter();
        parameter->set_key("mem_threshold");
        parameter->set_value(mem_threshold.get());
    }

    return parameters;
}

void verifyModule(const string& moduleName, const ModuleBase* moduleBase)
{
    ASSERT_TRUE(moduleBase != nullptr);
    ASSERT_TRUE(moduleBase->mesosVersion != NULL) << "Module " << moduleName << " is missing field 'mesosVersion'";
    ASSERT_TRUE(moduleBase->moduleApiVersion != NULL)
        << "Module " << moduleName <<" is missing field 'moduleApiVersion'";
    ASSERT_TRUE(moduleBase->authorName != NULL) << "Module " << moduleName << " is missing field 'authorName'";
    ASSERT_TRUE(moduleBase->authorEmail != NULL) << "Module " << moduleName << " is missing field 'authoEmail'";
    ASSERT_TRUE(moduleBase->description != NULL) << "Module " << moduleName << " is missing field 'description'";
    ASSERT_TRUE(moduleBase->kind != NULL) << "Module " << moduleName << " is missing field 'kind'";

    // Verify module API version.
    EXPECT_TRUE(stringify(moduleBase->moduleApiVersion) == MESOS_MODULE_API_VERSION)
        << "Module API version mismatch. Mesos has: " MESOS_MODULE_API_VERSION ", "
        << "library requires: " + stringify(moduleBase->moduleApiVersion);

    EXPECT_EQ("ResourceEstimator", stringify(moduleBase->kind));

    Try<Version> mesosVersion = Version::parse(MESOS_VERSION);

    Try<Version> moduleMesosVersion = Version::parse(moduleBase->mesosVersion);
    EXPECT_FALSE(moduleMesosVersion.isError()) << moduleMesosVersion.error();

    EXPECT_EQ(moduleMesosVersion.get(), mesosVersion.get())
        << "Module is not compiled for the current Mesos version.";

    EXPECT_TRUE(moduleBase->compatible()) << "Module " << moduleName << "has determined to be incompatible";
}

Try<ModuleBase*> ThresholdResourceEstimatorTest::loadModule() {
    if (this->moduleBase == nullptr) {
        auto const path = "./" + os::libraries::expandName(libraryName);
        Try<Nothing> result = this->dynamicLibrary->open(path);
        if (!result.isSome()) {
            return Error("Error opening library of module: '" + moduleName + "': " + result.error());
        }

        Try<void*> symbol = this->dynamicLibrary->loadSymbol(moduleName);
        if (symbol.isError()) {
            return Error("Error loading module '" + moduleName + "': " + symbol.error());
        }

        this->moduleBase = reinterpret_cast<ModuleBase*>(symbol.get());
    }
    return moduleBase;
}

ResourceEstimator* ThresholdResourceEstimatorTest::createEstimator(mesos::Parameters const & params) {
    auto load_result = this->loadModule();
    ModuleBase* moduleBase = load_result.get();
    auto estimatorModule = reinterpret_cast<Module<ResourceEstimator>*>(moduleBase);
    return estimatorModule->create(params);
}

ResourceUsage noUsage() {
    return ResourceUsage{};
}

TEST_F(ThresholdResourceEstimatorTest, test_load_library) {
    auto load_result = loadModule();
    ASSERT_FALSE(load_result.isError()) << load_result.error();
    ModuleBase* moduleBase = load_result.get();
    verifyModule(moduleName, moduleBase);
    Owned<ResourceEstimator> estimator{createEstimator(make_parameters("", None(), None(), None(), None()))};
    ASSERT_NE(nullptr, estimator.get());
}

TEST_F(ThresholdResourceEstimatorTest, test_no_thresholds) {
    Owned<ResourceEstimator> estimator{createEstimator(make_parameters(
        "cpus(*):2;mem(*):512", None(), None(), None(), None()  // no thresholds set
    ))};
    estimator->initialize(noUsage);
    auto const available_resources = estimator->oversubscribable().get();
    EXPECT_EQ(2.0, available_resources.revocable().cpus().get());
    EXPECT_EQ(512, available_resources.revocable().mem().get().megabytes());
}

TEST_F(ThresholdResourceEstimatorTest, test_below_thresholds) {
    Owned<ResourceEstimator> estimator{createEstimator(make_parameters(
        "cpus(*):2;mem(*):512", "1000.0", "1000.0", "1000.0", "500000"  // high threshloads that should never be hit
    ))};
    estimator->initialize(noUsage);
    auto const available_resources = estimator->oversubscribable().get();
    EXPECT_EQ(2.0, available_resources.revocable().cpus().get());
    EXPECT_EQ(512, available_resources.revocable().mem().get().megabytes());
}

TEST_F(ThresholdResourceEstimatorTest, test_above_load_threshold_1min) {
    Owned<ResourceEstimator> estimator{createEstimator(make_parameters(
        "cpus(*):2;mem(*):512", "0.0", "0.0", "0.0", "0"  // absurdly low load limit that will always be hit
    ))};
    estimator->initialize(noUsage);
    auto const available_resources = estimator->oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}


}
