#pragma once

#include <stout/bytes.hpp>

namespace com {
namespace blue_yonder {
namespace os {

struct MemInfo
{
  Bytes total;
  Bytes free;
  Bytes cached;
};

Try<MemInfo> meminfo();

} // os {
} // blue_yonder {
} // com {
