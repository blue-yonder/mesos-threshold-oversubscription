#pragma once

#include <stout/bytes.hpp>

namespace os {
    struct Load;
}

namespace com { namespace blue_yonder {

namespace os {
    struct MemInfo;
}

namespace threshold {

bool memExceedsThreshold(std::function<Try<os::MemInfo>()> const &, Option<Bytes> const &);

bool loadExceedsThresholds(
    std::function<Try<::os::Load>()> const &,
    Option<double> const &,
    Option<double> const &,
    Option<double> const &);

} } }
