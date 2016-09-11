#include "threshold_qos_controller.hpp"

#include "testutils.hpp"

#include <gtest/gtest.h>


using mesos::Resources;

using com::blue_yonder::ThresholdQoSController;


namespace {

struct ThresholdQoSControllerTests : public ::testing::Test {
  ResourceUsageFake usage;
  LoadFake load;
  MemInfoFake memory;
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
      Resources::parse("").get(),
      loadThreshold,
      memThreshold}
  {
    controller.initialize(usage);
  }
};

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
