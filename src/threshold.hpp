#pragma once

#include <stout/bytes.hpp>
#include <stout/os.hpp>

namespace com {
namespace blue_yonder {

namespace os {
struct MemInfo;
}

namespace threshold {

bool memExceedsThreshold(std::function<Try<os::MemInfo>()> const&, Bytes const&);

bool loadExceedsThreshold(std::function<Try<::os::Load>()> const&, ::os::Load const&);

} // namespace threshold {
} // namespace blue_yonder {
} // namespace com {
