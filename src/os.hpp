#pragma once

#include <stout/bytes.hpp>

namespace com {
namespace blue_yonder {
namespace os {

struct MemInfo
{
  Bytes total;
  Bytes memAvailable;
};

Try<MemInfo> meminfo();

} // os {
} // blue_yonder {
} // com {
