#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace sr {

namespace code {
inline constexpr std::string_view InvalidArgument = "E_INVALID_ARGUMENT";
inline constexpr std::string_view DecodeFailed = "E_DECODE_FAILED";
inline constexpr std::string_view SizeMismatch = "E_SIZE_MISMATCH";
inline constexpr std::string_view AlignmentFailed = "E_ALIGNMENT_FAILED";
inline constexpr std::string_view WriteFailed = "E_WRITE_FAILED";
inline constexpr std::string_view Internal = "E_INTERNAL";
}  // namespace code

// invariant: what() is always "<code>: <message>", the JS rejection format.
struct SrError : std::runtime_error {
  std::string code;
  std::string message;

  SrError(std::string_view c, std::string m)
      : std::runtime_error(std::string(c) + ": " + m), code(c), message(std::move(m)) {}
};

}  // namespace sr
