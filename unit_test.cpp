#include "threshold_resource_estimator.hpp"

#include <stout/os.hpp>

#include <gtest/gtest.h>


using process::Future;

using mesos::Resources;
using mesos::ResourceUsage;

using com::blue_yonder::ThresholdResourceEstimator;

namespace {

class UsageMock {
public:
    UsageMock()
        : value{std::make_shared<ResourceUsage>()}
    {};

    void set(
        std::string const & revocable_allocated, std::string const & non_revocable_allocated
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

    Try<os::Load> operator()() {
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

#define EXPECT_LOAD(expect_one, expect_five, expect_fifteen, load) \
    do { \
        EXPECT_EQ(expect_one, load.one); \
        EXPECT_EQ(expect_five, load.five); \
        EXPECT_EQ(expect_fifteen, load.fifteen); \
    } while(false); \

class MemoryMock {
public:
    MemoryMock() : value{std::make_shared<Try<os::Memory>>(os::Memory{0, 0, 0, 0})} {};

    Try<os::Memory> operator()() {
        return *value;
    }

    void set(std::string const & total, std::string const & free) {
        *value = os::Memory{
            Bytes::parse(total).get(),
            Bytes::parse(free).get(),
            0,
            0,
        };
    }

    void set_error() {
        *value = Error("Injected by Test");
    }

private:
    std::shared_ptr<Try<os::Memory>> value;
};


TEST(UsageMockTest, test_set) {
    UsageMock mock;
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
    EXPECT_EQ(Bytes::parse("62MB").get(), allocatedRevocable.mem().get());
    EXPECT_EQ(1, allocatedNonRevocable.cpus().get());
    EXPECT_EQ(Bytes::parse("48MB").get(), allocatedNonRevocable.mem().get());

    // must also work for copy
    UsageMock copy = mock;
    mock.set("cpus:5;mem:127", "");
    usage = copy().get();

    Resources copiedRevocable;
    for(auto & executor: usage.executors()) {
        copiedRevocable += Resources(executor.allocated()).revocable();
    }
    EXPECT_EQ(5, copiedRevocable.cpus().get());
    EXPECT_EQ(Bytes::parse("127MB").get(), copiedRevocable.mem().get());
}

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

TEST(MemoryMockTest, test_default) {
    MemoryMock mock;
    auto const mem = mock().get();
    EXPECT_EQ(0, mem.total.bytes());
    EXPECT_EQ(0, mem.free.bytes());
}

TEST(MemoryMockTest, test_set) {
    MemoryMock mock;
    auto copy = mock;
    mock.set("1GB", "256MB");
    auto const mem = copy().get();
    EXPECT_EQ(1, mem.total.gigabytes());
    EXPECT_EQ(256, mem.free.megabytes());
}

TEST(MemoryMockTest, test_set_error) {
    MemoryMock mock;
    auto copy = mock;
    mock.set_error();
    EXPECT_TRUE(copy().isError());
}


struct ThresholdResourceEstimatorTests : public ::testing::Test {
    UsageMock usage;
    LoadMock load;
    MemoryMock memory;
    ThresholdResourceEstimator estimator;

    ThresholdResourceEstimatorTests(
        std::string const & resources,
        Option<double> const & loadThreshold1Min,
        Option<double> const & loadThreshold5Min,
        Option<double> const & loadThreshold15Min,
        Option<Bytes> const & memThreshold
    ) : usage{}, load{}, memory{}, estimator{
        load,
        memory,
        Resources::parse(resources).get(),
        loadThreshold1Min,
        loadThreshold5Min,
        loadThreshold15Min,
        memThreshold
    } {
        estimator.initialize(usage);
    }
};


struct NoResourcesTests : public ThresholdResourceEstimatorTests {
    NoResourcesTests() : ThresholdResourceEstimatorTests {
        "", None(), None(), None(), None()
    } {
    }
};

TEST_F(NoResourcesTests, noop) {
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}


struct NoThresholdTests : public ThresholdResourceEstimatorTests {
    NoThresholdTests() : ThresholdResourceEstimatorTests {
        "cpus(*):2;mem(*):512", None(), None(), None(), None()
    } {
    }
};

TEST_F(NoThresholdTests, no_usage) {
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_FALSE(available_resources.empty());
    EXPECT_EQ(2.0, available_resources.revocable().cpus().get());
    EXPECT_EQ(Bytes::parse("512MB").get(), available_resources.revocable().mem().get());
}

TEST_F(NoThresholdTests, revocable_usage_reduces_offers) {
    usage.set("cpus(*):1.5;mem(*):128", "");
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_EQ(0.5, available_resources.revocable().cpus().get());
    EXPECT_EQ(Bytes::parse("384MB").get(), available_resources.revocable().mem().get());
}

TEST_F(NoThresholdTests, non_revocable_usage_is_ignored) {
    usage.set("", "cpus(*):1.5;mem(*):128");
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_EQ(2.0, available_resources.revocable().cpus().get());
    EXPECT_EQ(Bytes::parse("512MB").get(), available_resources.revocable().mem().get());
}


struct LoadThresholdTests : public ThresholdResourceEstimatorTests {
    LoadThresholdTests() : ThresholdResourceEstimatorTests {
        "cpus(*):2;mem(*):512", 4, 3, 2, None()
    } {
        load.set(3.9, 2.9, 1.9);
    }
};

TEST_F(LoadThresholdTests, not_exceeded) {
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_FALSE(available_resources.empty());
    EXPECT_EQ(2.0, available_resources.revocable().cpus().get());
    EXPECT_EQ(Bytes::parse("512MB").get(), available_resources.revocable().mem().get());
}

TEST_F(LoadThresholdTests, load1_exceeded) {
    load.set(4.0, 2.9, 1.9);
    auto available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
    load.set(10.0, 2.9, 1.9);
    available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

TEST_F(LoadThresholdTests, load5_exceeded) {
    load.set(3.9, 3.0, 1.9);
    auto available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
    load.set(3.9, 10.0, 1.9);
    available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

TEST_F(LoadThresholdTests, load15_exceeded) {
    load.set(3.9, 2.9, 2.0);
    auto available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
    load.set(3.9, 2.9, 10.0);
    available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

TEST_F(LoadThresholdTests, load_not_available) {
    load.set_error();
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_FALSE(available_resources.empty());
    EXPECT_EQ(2.0, available_resources.revocable().cpus().get());
    EXPECT_EQ(Bytes::parse("512MB").get(), available_resources.revocable().mem().get());
}


struct MemThresholdTests : public ThresholdResourceEstimatorTests {
    MemThresholdTests() : ThresholdResourceEstimatorTests {
        "cpus(*):2;mem(*):512", None(), None(), None(), Bytes::parse("384MB").get()
    } {
        usage.set("cpus(*):1.5;mem(*):128", "cpus(*):1.5;mem(*):128");
        load.set(3.9, 2.9, 1.9);
        memory.set("512MB", "256MB");
    }
};

TEST_F(MemThresholdTests, not_exceeded) {
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_FALSE(available_resources.empty());
}

TEST_F(MemThresholdTests, reached) {
    memory.set("512MB", "128MB");
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

TEST_F(MemThresholdTests, exceeded) {
    memory.set("512MB", "0MB");
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

TEST_F(MemThresholdTests, memory_statistics_not_available) {
    memory.set_error();
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

}
