#include <stout/bytes.hpp>
#include <stout/dynamiclibrary.hpp>
#include <stout/os.hpp>
#include <stout/path.hpp>
#include <stout/version.hpp>

#include <process/owned.hpp>

#include <mesos/version.hpp>
#include <mesos/module.hpp>
#include <mesos/resources.hpp>
#include <mesos/module/resource_estimator.hpp>

#include <gtest/gtest.h>

using std::string;

using process::Future;
using process::Owned;

using mesos::Resources;
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

mesos::Parameters make_parameters(std::string fixed) {
    mesos::Parameters parameters{};
    auto parameter = parameters.add_parameter();
    parameter->set_key("resources");
    parameter->set_value(fixed);
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
    if(this->moduleBase == nullptr) {
        auto const path = "./" + os::libraries::expandName(libraryName);
        Try<Nothing> result = this->dynamicLibrary->open(path);
        if(!result.isSome()) {
            return Error("Error opening library of module: '" + moduleName + "': " + result.error());
        }

        Try<void*> symbol = this->dynamicLibrary->loadSymbol(moduleName);
        if(symbol.isError()) {
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

class UsageMock {
public:
    UsageMock()
        : value{std::make_shared<ResourceUsage>()}
    {};

    void set(std::string const revocable_allocated) {
        value->Clear();
        auto * revocable_executor = value->add_executors();
        auto resources = Resources::parse(revocable_allocated);
        for(auto const & parsed_resource: resources.get()) {
            auto * mutable_resource = revocable_executor->add_allocated();
            mutable_resource->CopyFrom(parsed_resource);
            mutable_resource->mutable_revocable();  // mark as revocable
        }
    }
    Future<ResourceUsage> operator()() const {
        return *value;
    }
private:
    std::shared_ptr<ResourceUsage> value;
};

TEST(UsageMockTest, test_set) {
    UsageMock mock;
    mock.set("cpus:3;mem:62");

    ResourceUsage usage = mock().get();
    Resources allocatedRevocable;
    for(auto & executor: usage.executors()) {
        allocatedRevocable += Resources(executor.allocated()).revocable();
    }

    EXPECT_EQ(1, usage.executors_size());
    EXPECT_EQ(3, allocatedRevocable.cpus().get());
    EXPECT_EQ(Bytes::parse("62MB").get(), allocatedRevocable.mem().get());

    // must also work for copy
    UsageMock copy = mock;
    mock.set("cpus:5;mem:127");
    usage = copy().get();

    Resources copiedRevocable;
    for(auto & executor: usage.executors()) {
        copiedRevocable += Resources(executor.allocated()).revocable();
    }
    EXPECT_EQ(5, copiedRevocable.cpus().get());
    EXPECT_EQ(Bytes::parse("127MB").get(), copiedRevocable.mem().get());
}

TEST_F(ThresholdResourceEstimatorTest, test_load_library) {
    auto load_result = loadModule();
    ASSERT_FALSE(load_result.isError()) << load_result.error();
    ModuleBase* moduleBase = load_result.get();
    verifyModule(moduleName, moduleBase);
    Owned<ResourceEstimator> estimator{createEstimator(make_parameters(""))};
    ASSERT_NE(nullptr, estimator.get());
}

TEST_F(ThresholdResourceEstimatorTest, test_noop) {
    UsageMock usage;
    Owned<ResourceEstimator> estimator{createEstimator(make_parameters(""))};
    estimator->initialize(usage);
    auto available_resources = estimator->oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

TEST_F(ThresholdResourceEstimatorTest, test_underutilized) {
    UsageMock usage;  // no usage at all
    auto const parameters = make_parameters("cpus(*):2;mem(*):512");
    Owned<ResourceEstimator> estimator{createEstimator(parameters)};
    estimator->initialize(usage);
    auto available_resources = estimator->oversubscribable().get();
    EXPECT_FALSE(available_resources.empty());
    EXPECT_EQ(2.0, available_resources.revocable().cpus().get());
    EXPECT_EQ(Bytes::parse("512MB").get(), available_resources.revocable().mem().get());

    usage.set("cpus(*):1.5;mem(*):128");
    available_resources = estimator->oversubscribable().get();
    EXPECT_EQ(0.5, available_resources.revocable().cpus().get());
    EXPECT_EQ(Bytes::parse("384MB").get(), available_resources.revocable().mem().get());

    // TODO also test with actual usage at threshold
}

}
