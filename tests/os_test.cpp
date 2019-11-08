#include "os.hpp"

#include <gtest/gtest.h>

using com::blue_yonder::os::meminfo;

TEST(MemoryTests, smoketest) {
  auto const memInfo = meminfo().get();

  EXPECT_NE(0, memInfo.total.bytes());
  EXPECT_NE(0, memInfo.memAvailable.bytes());

  EXPECT_LT(memInfo.memAvailable, memInfo.total);
}
