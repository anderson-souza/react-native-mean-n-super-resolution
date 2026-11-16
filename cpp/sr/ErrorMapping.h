#pragma once

#include <exception>
#include <string>

namespace sr {

// invariant: always returns "<E_CODE>: <detail>"; SrError keeps its own code,
// everything else (bad_alloc, std::exception, non-std throws) is E_INTERNAL.
// cv::Exception is deliberately absent: opencv-mobile never throws it (AD-004).
std::string toRejectMessage(std::exception_ptr error);

}  // namespace sr
