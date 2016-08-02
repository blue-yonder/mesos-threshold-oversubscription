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
        std::string const & revocable_allocated, std::string const & non_revocable_allocated,
        Option<std::string> const & revocable_mem_used, Option<std::string> const & non_revocable_mem_used
    ) {
        value->Clear();
        auto * revocable_executor = value->add_executors();
        auto revocable_resources = Resources::parse(revocable_allocated);
        for(auto const & parsed_resource: revocable_resources.get()) {
            auto * mutable_resource = revocable_executor->add_allocated();
            mutable_resource->CopyFrom(parsed_resource);
            mutable_resource->mutable_revocable();  // mark as revocable
        }
        if(revocable_mem_used.isSome()) {
            auto revocable_stats = revocable_executor->mutable_statistics();
            revocable_stats->set_mem_total_bytes(Bytes::parse(revocable_mem_used.get()).get().bytes());
        }

        auto * non_revocable_executor = value->add_executors();
        auto non_revocable_resources = Resources::parse(non_revocable_allocated);
        for(auto const & parsed_resource: non_revocable_resources.get()) {
            auto * mutable_resource = non_revocable_executor->add_allocated();
            mutable_resource->CopyFrom(parsed_resource);
            mutable_resource->clear_revocable();  // mark as non-revocable
        }
        if(non_revocable_mem_used.isSome()) {
            auto non_revocable_stats = non_revocable_executor->mutable_statistics();
            non_revocable_stats->set_mem_total_bytes(Bytes::parse(non_revocable_mem_used.get()).get().bytes());
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
    LoadMock() : value{std::make_shared<os::Load>()} {};

    Try<os::Load> operator()() {
        return *value;
    }

    void set(float one, float five, float fifteen) {
        *value = os::Load{one, five, fifteen};
    }

private:
    std::shared_ptr<os::Load> value;
};

#define EXPECT_LOAD(expect_one, expect_five, expect_fifteen, load) \
    do { \
        EXPECT_EQ(expect_one, load.one); \
        EXPECT_EQ(expect_five, load.five); \
        EXPECT_EQ(expect_fifteen, load.fifteen); \
    } while(false); \

TEST(UsageMockTest, test_set) {
    UsageMock mock;
    mock.set("cpus:3;mem:62", "cpus:1;mem:48", "50MB", "32MB");

    ResourceUsage usage = mock().get();
    Resources allocatedRevocable;
    Resources allocatedNonRevocable;
    uint64_t memUsed{0};
    for(auto & executor: usage.executors()) {
        allocatedRevocable += Resources(executor.allocated()).revocable();
        allocatedNonRevocable += Resources(executor.allocated()).nonRevocable();
        memUsed += executor.statistics().mem_total_bytes();
    }

    EXPECT_EQ(2, usage.executors_size());
    EXPECT_EQ(3, allocatedRevocable.cpus().get());
    EXPECT_EQ(Bytes::parse("62MB").get(), allocatedRevocable.mem().get());
    EXPECT_EQ(1, allocatedNonRevocable.cpus().get());
    EXPECT_EQ(Bytes::parse("48MB").get(), allocatedNonRevocable.mem().get());
    EXPECT_EQ(Bytes::parse("82MB").get().bytes(), memUsed);

    // must also work for copy
    UsageMock copy = mock;
    mock.set("cpus:5;mem:127", "", "0B", "0B");
    usage = copy().get();

    Resources copiedRevocable;
    memUsed = 0;
    for(auto & executor: usage.executors()) {
        copiedRevocable += Resources(executor.allocated()).revocable();
        memUsed += executor.statistics().mem_total_bytes();
    }
    EXPECT_EQ(5, copiedRevocable.cpus().get());
    EXPECT_EQ(Bytes::parse("127MB").get(), copiedRevocable.mem().get());

    // can ommit usage values
    mock.set("cpus:5;mem:127", "", None(), None());
    usage = mock().get();
    for(auto & executor: usage.executors()) {
        EXPECT_FALSE(executor.has_statistics());
    }
}

TEST(LoadMockTest, test_set) {
    LoadMock mock;
    EXPECT_LOAD(0, 0, 0, mock().get());

    auto copy = mock;
    mock.set(1.5, 2.5, 3.5);
    EXPECT_LOAD(1.5, 2.5, 3.5, mock().get());
    EXPECT_LOAD(1.5, 2.5, 3.5, copy().get());
}


struct ThresholdResourceEstimatorTests : public ::testing::Test {
    UsageMock usage;
    LoadMock load;
    ThresholdResourceEstimator estimator;

    ThresholdResourceEstimatorTests(
        std::string const & resources,
        Option<double> const & loadThreshold1Min,
        Option<double> const & loadThreshold5Min,
        Option<double> const & loadThreshold15Min,
        Option<Bytes> const & memThreshold
    ) : usage{}, load{}, estimator{
        load,
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
    usage.set("cpus(*):1.5;mem(*):128", "", "0B", "0B");
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_EQ(0.5, available_resources.revocable().cpus().get());
    EXPECT_EQ(Bytes::parse("384MB").get(), available_resources.revocable().mem().get());
}

TEST_F(NoThresholdTests, non_revocable_usage_is_ignored) {
    usage.set("", "cpus(*):1.5;mem(*):128", "0B", "0B");
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


struct MemThresholdTests : public ThresholdResourceEstimatorTests {
    MemThresholdTests() : ThresholdResourceEstimatorTests {
        "cpus(*):2;mem(*):512", None(), None(), None(), Bytes::parse("384MB").get()
    } {
        usage.set("cpus(*):1.5;mem(*):128", "cpus(*):1.5;mem(*):128", "128MB", "128MB");
        load.set(3.9, 2.9, 1.9);
    }
};

TEST_F(MemThresholdTests, not_exceeded) {
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_FALSE(available_resources.empty());
}

TEST_F(MemThresholdTests, reached_by_revocable) {
    usage.set("cpus(*):1.5;mem(*):128", "cpus(*):1.5;mem(*):128", "384MB", "0B");
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

TEST_F(MemThresholdTests, reached_by_non_revocable) {
    usage.set("cpus(*):1.5;mem(*):128", "cpus(*):1.5;mem(*):128", "0B", "384MB");
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

TEST_F(MemThresholdTests, exceeded_by_combined_usage) {
    usage.set("cpus(*):1.5;mem(*):128", "cpus(*):1.5;mem(*):128", "256MB", "256MB");
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

TEST_F(MemThresholdTests, no_statistices_and_allocated_below_threshold) {
    usage.set("cpus(*):1.5;mem(*):128", "cpus(*):1.5;mem(*):128", None(), None());
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_FALSE(available_resources.empty());
}

TEST_F(MemThresholdTests, no_statistices_and_allocated_reaches_threshold) {
    usage.set("cpus(*):1.5;mem(*):128", "cpus(*):1.5;mem(*):256", None(), None());
    auto const available_resources = estimator.oversubscribable().get();
    EXPECT_TRUE(available_resources.empty());
}

}
