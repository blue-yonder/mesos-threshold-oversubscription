#include "threshold_resource_estimator.hpp"

#include "testutils.hpp"

#include <gtest/gtest.h>

using mesos::Resources;

using com::blue_yonder::ThresholdResourceEstimator;

namespace {

struct ThresholdResourceEstimatorTests : public ::testing::Test
{
  ResourceUsageFake usage;
  LoadFake load;
  MemInfoFake memory;
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

struct NoResourcesTests : public ThresholdResourceEstimatorTests {
  NoResourcesTests() : ThresholdResourceEstimatorTests {
    "", os::Load{4, 3, 2}, Bytes::parse("384MB").get()
  } {
  }
};

TEST_F(NoResourcesTests, noop) {
  auto const availableResources = estimator.oversubscribable().get();
  EXPECT_TRUE(availableResources.empty());
}

struct EstimatorTests : public ThresholdResourceEstimatorTests {
  EstimatorTests() : ThresholdResourceEstimatorTests {
    "cpus(*):2;mem(*):512", os::Load{4, 3, 2}, Bytes::parse("384MB").get()
  } {
    usage.set("cpus(*):1.0;mem(*):64", "cpus(*):1.0;mem(*):128");
    load.set(3.9, 2.9, 1.9);
    memory.set("512MB", "64MB", "256MB");
  }
};

TEST_F(EstimatorTests, load_not_exceeded) {
  auto availableResources = estimator.oversubscribable().get();
  EXPECT_FALSE(availableResources.empty());
  EXPECT_EQ(1.0, availableResources.revocable().cpus().get());
  EXPECT_EQ(448, availableResources.revocable().mem().get().megabytes());

  load.set(1.0, 10.0, 1.9);
  availableResources = estimator.oversubscribable().get();
  EXPECT_FALSE(availableResources.empty());
  EXPECT_EQ(1.0, availableResources.revocable().cpus().get());
  EXPECT_EQ(448, availableResources.revocable().mem().get().megabytes());

  load.set(1.0, 1.0, 10.0);
  availableResources = estimator.oversubscribable().get();
  EXPECT_EQ(1.0, availableResources.revocable().cpus().get());
  EXPECT_EQ(448, availableResources.revocable().mem().get().megabytes());
}

TEST_F(EstimatorTests, load_exceeded) {
  load.set(10.0, 2.9, 1.9);
  auto availableResources = estimator.oversubscribable().get();
  EXPECT_TRUE(availableResources.empty());

  load.set(3.9, 10.0, 1.9);
  availableResources = estimator.oversubscribable().get();
  EXPECT_TRUE(availableResources.empty());

  load.set(3.9, 2.9, 10.0);
  availableResources = estimator.oversubscribable().get();
  EXPECT_TRUE(availableResources.empty());
}

TEST_F(EstimatorTests, load_not_available) {
  load.set_error();
  auto const availableResources = estimator.oversubscribable().get();
  EXPECT_TRUE(availableResources.empty());
}

TEST_F(EstimatorTests, mem_not_exceeded) {
  auto const availableResources = estimator.oversubscribable().get();
  EXPECT_FALSE(availableResources.empty());
  EXPECT_EQ(1.0, availableResources.revocable().cpus().get());
  EXPECT_EQ(448, availableResources.revocable().mem().get().megabytes());
}

TEST_F(EstimatorTests, mem_exceeded) {
  memory.set("512MB", "0MB", "0MB");
  auto const availableResources = estimator.oversubscribable().get();
  EXPECT_TRUE(availableResources.empty());
}

TEST_F(EstimatorTests, mem_not_available) {
  memory.set_error();
  auto const availableResources = estimator.oversubscribable().get();
  EXPECT_TRUE(availableResources.empty());
}

} // namespace {
