#include "testutils.hpp"

#include <stout/os.hpp>

#include <gtest/gtest.h>

#include "os.hpp"

using mesos::Resources;
using mesos::ResourceUsage;

namespace {

TEST(ResourceUsageFakeTest, test_set) {
  ResourceUsageFake fake;
  fake.set("cpus:3;mem:62", "cpus:1;mem:48");

  ResourceUsage usage = fake().get();
  Resources allocatedRevocable;
  Resources allocatedNonRevocable;
  for (auto& executor : usage.executors()) {
    allocatedRevocable += Resources(executor.allocated()).revocable();
    allocatedNonRevocable += Resources(executor.allocated()).nonRevocable();
  }

  EXPECT_EQ(2, usage.executors_size());
  EXPECT_EQ(3, allocatedRevocable.cpus().get());
  EXPECT_EQ(62 * 1024 * 1024, allocatedRevocable.mem().get().bytes());
  EXPECT_EQ(1, allocatedNonRevocable.cpus().get());
  EXPECT_EQ(48 * 1024 * 1024, allocatedNonRevocable.mem().get().bytes());

  // set not usage
  fake.set("", "");
  fake().get();

  // must also work for copy
  ResourceUsageFake copy = fake;
  fake.set("cpus:5;mem:127", "cpus:2;mem:32");
  usage = copy().get();

  Resources copiedRevocable;
  for (auto& executor : usage.executors()) {
    copiedRevocable += Resources(executor.allocated()).revocable();
  }
  EXPECT_EQ(5, copiedRevocable.cpus().get());
  EXPECT_EQ(127 * 1024 * 1024, copiedRevocable.mem().get().bytes());
}

#define EXPECT_LOAD(expect_one, expect_five, expect_fifteen, load) \
  do {                                                             \
    EXPECT_EQ(expect_one, load.one);                               \
    EXPECT_EQ(expect_five, load.five);                             \
    EXPECT_EQ(expect_fifteen, load.fifteen);                       \
  } while (false);

TEST(LoadFakeTest, test_set) {
  LoadFake fake;
  EXPECT_LOAD(0, 0, 0, fake().get());

  auto copy = fake;
  fake.set(1.5, 2.5, 3.5);
  EXPECT_LOAD(1.5, 2.5, 3.5, fake().get());
  EXPECT_LOAD(1.5, 2.5, 3.5, copy().get());
}

TEST(LoadFakeTest, test_set_error) {
  LoadFake fake;

  auto copy = fake;
  fake.set_error();
  EXPECT_TRUE(copy().isError());
}

TEST(MemInfoFakeTest, test_default) {
  MemInfoFake fake;
  auto const mem = fake().get();
  EXPECT_EQ(0, mem.total.bytes());
  EXPECT_EQ(0, mem.memAvailable.bytes());
}

TEST(MemInfoFakeTest, test_set) {
  MemInfoFake fake;
  auto copy = fake;
  fake.set("1GB", "374MB");
  auto const mem = copy().get();
  EXPECT_EQ(1 * 1024 * 1024 * 1024, mem.total.bytes());
  EXPECT_EQ(374 * 1024 * 1024, mem.memAvailable.bytes());
}

TEST(MemInfoFakeTest, test_set_error) {
  MemInfoFake fake;
  auto copy = fake;
  fake.set_error();
  EXPECT_TRUE(copy().isError());
}

} // namespace {
