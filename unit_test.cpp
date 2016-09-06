#include "threshold_resource_estimator.hpp"
#include "threshold_qos_controller.hpp"


#include <stout/os.hpp>

#include <gtest/gtest.h>

#include "os.hpp"


using process::Future;

using mesos::Resources;
using mesos::ResourceUsage;

using com::blue_yonder::ThresholdResourceEstimator;
using com::blue_yonder::ThresholdQoSController;
using com::blue_yonder::os::MemInfo;

namespace {

class ResourceUsageMock {
public:
    ResourceUsageMock()
        : value{std::make_shared<ResourceUsage>()}
    {};

    void set(
        std::string const & revocable_allocated,
        std::string const & non_revocable_allocated
    ) {
        value->Clear();
        auto * revocable_executor = value->add_executors();
        auto revocable_resources = Resources::parse(revocable_allocated);
        for(auto const & parsed_resource: revocable_resources.get()) {
            auto * mutable_resource = revocable_executor->add_allocated();
            mutable_resource->CopyFrom(parsed_resource);
            mutable_resource->mutable_revocable();  // mark as revocable
        }

        auto * non_revocable_executor = value->add_executors();
        auto non_revocable_resources = Resources::parse(non_revocable_allocated);
        for(auto const & parsed_resource: non_revocable_resources.get()) {
            auto * mutable_resource = non_revocable_executor->add_allocated();
            mutable_resource->CopyFrom(parsed_resource);
            mutable_resource->clear_revocable();  // mark as non-revocable
        }
    }
    Future<ResourceUsage> operator()() const {
        return *value;
    }
private:
    std::shared_ptr<ResourceUsage> value;
};

class LoadMock {
public:
    LoadMock() : value{std::make_shared<Try<os::Load>>(os::Load{0, 0, 0})} {};

    Try<os::Load> operator()() const {
        return *value;
    }

    void set(float one, float five, float fifteen) {
        *value = os::Load{one, five, fifteen};
    }

    void set_error() {
        *value = Error("Injected by Test");
    }

private:
    std::shared_ptr<Try<os::Load>> value;
};

class MemInfoMock {
public:
    MemInfoMock() : value{std::make_shared<Try<MemInfo>>(MemInfo{0, 0, 0})} {};

    Try<MemInfo> operator()() const {
        return *value;
    }

    void set(std::string const & total, std::string const & free, std::string const & cached) {
        *value = MemInfo{
            Bytes::parse(total).get(),
            Bytes::parse(free).get(),
            Bytes::parse(cached).get()
        };
    }

    void set_error() {
        *value = Error("Injected by Test");
    }

private:
    std::shared_ptr<Try<MemInfo>> value;
};


TEST(ResourceUsageMockTest, test_set) {
    ResourceUsageMock mock;
    mock.set("cpus:3;mem:62", "cpus:1;mem:48");

    ResourceUsage usage = mock().get();
    Resources allocatedRevocable;
    Resources allocatedNonRevocable;
    for(auto & executor: usage.executors()) {
        allocatedRevocable += Resources(executor.allocated()).revocable();
        allocatedNonRevocable += Resources(executor.allocated()).nonRevocable();
    }

    EXPECT_EQ(2, usage.executors_size());
    EXPECT_EQ(3, allocatedRevocable.cpus().get());
    EXPECT_EQ(62, allocatedRevocable.mem().get().megabytes());
    EXPECT_EQ(1, allocatedNonRevocable.cpus().get());
    EXPECT_EQ(48, allocatedNonRevocable.mem().get().megabytes());

    // must also work for copy
    ResourceUsageMock copy = mock;
    mock.set("cpus:5;mem:127", "");
    usage = copy().get();

    Resources copiedRevocable;
    for(auto & executor: usage.executors()) {
        copiedRevocable += Resources(executor.allocated()).revocable();
    }
    EXPECT_EQ(5, copiedRevocable.cpus().get());
    EXPECT_EQ(127, copiedRevocable.mem().get().megabytes());
}

#define EXPECT_LOAD(expect_one, expect_five, expect_fifteen, load) \
    do { \
        EXPECT_EQ(expect_one, load.one); \
        EXPECT_EQ(expect_five, load.five); \
        EXPECT_EQ(expect_fifteen, load.fifteen); \
    } while(false); \

TEST(LoadMockTest, test_set) {
    LoadMock mock;
    EXPECT_LOAD(0, 0, 0, mock().get());

    auto copy = mock;
    mock.set(1.5, 2.5, 3.5);
    EXPECT_LOAD(1.5, 2.5, 3.5, mock().get());
    EXPECT_LOAD(1.5, 2.5, 3.5, copy().get());
}

TEST(LoadMockTest, test_set_error) {
    LoadMock mock;

    auto copy = mock;
    mock.set_error();
    EXPECT_TRUE(copy().isError());
}

TEST(MemInfoMockTest, test_default) {
    MemInfoMock mock;
    auto const mem = mock().get();
    EXPECT_EQ(0, mem.total.bytes());
    EXPECT_EQ(0, mem.free.bytes());
}

TEST(MemInfoMockTest, test_set) {
    MemInfoMock mock;
    auto copy = mock;
    mock.set("1GB", "128MB", "256MB");
    auto const mem = copy().get();
    EXPECT_EQ(1, mem.total.gigabytes());
    EXPECT_EQ(128, mem.free.megabytes());
    EXPECT_EQ(256, mem.cached.megabytes());
}

TEST(MemInfoMockTest, test_set_error) {
    MemInfoMock mock;
    auto copy = mock;
    mock.set_error();
    EXPECT_TRUE(copy().isError());
}


struct ThresholdResourceEstimatorTests : public ::testing::Test {
    ResourceUsageMock usage;
    LoadMock load;
    MemInfoMock memory;
    ThresholdResourceEstimator estimator;

    ThresholdResourceEstimatorTests(
        std::string const & resources,
        os::Load const & loadThreshold,
        Bytes const & memThreshold
    ) :
        usage{},
        load{},
        memory{},
        estimator{
            load,
            memory,
            Resources::parse(resources).get(),
            loadThreshold,
            memThreshold}
    {
        estimator.initialize(usage);
    }
};


struct ThresholdQoSControllerTests : public ::testing::Test {
    ResourceUsageMock usage;
    LoadMock load;
    MemInfoMock memory;
    ThresholdQoSController controller;

    ThresholdQoSControllerTests(
        os::Load const & loadThreshold,
        Bytes const & memThreshold
    ) :
        usage{},
        load{},
        memory{},
        controller{
            load,
            memory,
            loadThreshold,
            memThreshold}
    {
        controller.initialize(usage);
    }
};

struct NoResourcesTests : public ThresholdResourceEstimatorTests {
    NoResourcesTests() : ThresholdResourceEstimatorTests {
        "", os::Load{4, 3, 2}, Bytes::parse("384MB").get()
    } {
    }
};

TEST_F(NoResourcesTests, noop) {
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}



struct EstimatorTests : public ThresholdResourceEstimatorTests {
    EstimatorTests() : ThresholdResourceEstimatorTests {
        "cpus(*):2;mem(*):512", os::Load{4, 3, 2}, Bytes::parse("384MB").get()
    } {
        usage.set("cpus(*):1.5;mem(*):128", "cpus(*):1.5;mem(*):128");
        load.set(3.9, 2.9, 1.9);
        memory.set("512MB", "64MB", "256MB");
    }
};

TEST_F(EstimatorTests, load_not_exceeded) {
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_FALSE(available_resources.empty());
}

TEST_F(EstimatorTests, load_exceeded) {
    load.set(10.0, 2.9, 1.9);
    auto available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());

    load.set(3.9, 10.0, 1.9);
    available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());

    load.set(3.9, 2.9, 10.0);
    available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

TEST_F(EstimatorTests, load_not_available) {
    load.set_error();
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

TEST_F(EstimatorTests, mem_not_exceeded) {
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_FALSE(available_resources.empty());
}

TEST_F(EstimatorTests, mem_exceeded) {
    memory.set("512MB", "0MB", "0MB");
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

TEST_F(EstimatorTests, mem_not_available) {
    memory.set_error();
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}


struct ControllerTests : public ThresholdQoSControllerTests {
    ControllerTests() : ThresholdQoSControllerTests {
        os::Load{4, 3, 2}, Bytes::parse("384MB").get()
    } {
        usage.set("cpus(*):1.5;mem(*):128", "cpus(*):1.5;mem(*):128");
        load.set(3.9, 2.9, 1.9);
        memory.set("512MB", "64MB", "256MB");
    }
};

TEST_F(ControllerTests, load_not_exceeded) {
    auto const corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.empty());
}

TEST_F(ControllerTests, load_exceeded) {
    load.set(10.0, 2.9, 1.9);
    auto corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.size() == 1);

    load.set(3.9, 10.0, 1.9);
    corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.size() == 1);

    load.set(3.9, 2.9, 10.0);
    corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.size() == 1);
}

TEST_F(ControllerTests, load_not_available) {
    load.set_error();
    auto const corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.size() == 1);
}

TEST_F(ControllerTests, mem_exceeded) {
    memory.set("512MB", "0MB", "0MB");
    auto const corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.size() == 1);
}

TEST_F(ControllerTests, mem_not_available) {
    memory.set_error();
    auto const corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.size() == 1);
}

TEST_F(ControllerTests, thresholds_exceed_but_no_revocable_tasks) {
    load.set(10.0, 2.9, 1.9);
    usage.set("", "cpus(*):1.5;mem(*):128");
    memory.set("512MB", "32MB", "32MB");

    auto corrections = controller.corrections().get();
    EXPECT_TRUE(corrections.empty());
}

}
