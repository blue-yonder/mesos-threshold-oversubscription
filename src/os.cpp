#include "os.hpp"

#include <fstream>

#include <glog/logging.h>

using com::blue_yonder::os::meminfo;

Try<com::blue_yonder::os::MemInfo> com::blue_yonder::os::meminfo() {
  std::ifstream proc{"/proc/meminfo"};

  std::string identifier;
  std::string bytes;
  std::string unit;

  Option<Bytes> total = None();
  Option<Bytes> memAvailable = None();

  while (proc >> identifier >> bytes >> unit) {
    if (identifier == "MemTotal:") {
      auto const parsed = Bytes::parse(bytes + unit);
      if (parsed.isError()) {
        return Error("Failed to parse MemTotal from /proc/meminfo: " + parsed.error());
      }
      total = parsed.get();
    } else if (identifier == "MemAvailable:") {
      auto const parsed = Bytes::parse(bytes + unit);
      if (parsed.isError()) {
        return Error("Failed to parse MemAvailable from /proc/meminfo: " + parsed.error());
      }
      memAvailable = parsed.get();
    }
  }

  if (not proc.eof() and proc.fail()) {
    return Error("Failed to read /proc/meminfo");
  }
  if (not total.isSome()) {
    return Error("Could not find MemTotal in /proc/meminfo");
  }
  if (not memAvailable.isSome()) {
    return Error("Could not find MemAvailable in /proc/meminfo");
  }

  return MemInfo{total.get(), memAvailable.get()};
}
