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

using process::Owned;

using mesos::modules::ModuleBase;
using mesos::modules::Module;
using mesos::slave::ResourceEstimator;

namespace {

string const libraryName{"threshold_resource_estimator"};
string const moduleName{"com_blue_yonder_ThresholdResourceEstimator"};

class ThresholdResourceEstimatorTest : public ::testing::Test {
public:
    ThresholdResourceEstimatorTest()
        : dynamicLibrary(new DynamicLibrary())
    { }

    virtual void TearDown()
    {
        dynamicLibrary->close();

        ::testing::Test::TearDown();
    }

    Try<ModuleBase*> loadModule();

private:
    Owned<DynamicLibrary> dynamicLibrary;
};

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
    auto const path = "./" + os::libraries::expandName(libraryName);
    Try<Nothing> result = this->dynamicLibrary->open(path);
    if(!result.isSome()) {
        return Error("Error opening library of module: '" + moduleName + "': " + result.error());
    }

    Try<void*> symbol = dynamicLibrary->loadSymbol(moduleName);
    if(symbol.isError()) {
        return Error("Error loading module '" + moduleName + "': " + symbol.error());
    }

    return reinterpret_cast<ModuleBase*>(symbol.get());
}

TEST_F(ThresholdResourceEstimatorTest, test_load_library) {
    auto load_result = loadModule();
    ASSERT_FALSE(load_result.isError()) << load_result.error();
    ModuleBase* moduleBase = load_result.get();
    verifyModule(moduleName, moduleBase);
    auto estimatorModule = reinterpret_cast<Module<ResourceEstimator>*>(moduleBase);
    auto estimator = estimatorModule->create(mesos::Parameters());
    ASSERT_NE(nullptr, estimator);
    delete estimator;
}

}
