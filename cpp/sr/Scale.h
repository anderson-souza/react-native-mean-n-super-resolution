#pragma once

#include <cmath>
#include <optional>

namespace sr {

// invariant: a non-integral or non-finite scale maps to 0, which sr::validate
// rejects with E_INVALID_ARGUMENT like any other out-of-range value.
inline int toScale(const std::optional<double>& scale) {
  if (!scale) return 2;
  const double v = *scale;
  if (!std::isfinite(v) || v != std::trunc(v) || v < -1e6 || v > 1e6) return 0;
  return static_cast<int>(v);
}

}  // namespace sr
