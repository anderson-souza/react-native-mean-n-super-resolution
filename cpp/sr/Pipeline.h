#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sr {

struct Request {
  std::vector<std::string> paths;
  int scale = 2;
  std::optional<std::string> outputPath;
  std::string defaultOutputDir;
};

struct Result {
  std::string path;
  int width = 0;
  int height = 0;
  int framesUsed = 0;
};

// invariant: run() rejects with E_INTERNAL before allocating output-sized buffers
// when estimatePeakBytes() of the decoded reference exceeds this budget.
inline constexpr std::uint64_t kMemoryBudgetBytes = std::uint64_t{1} << 30;

// invariant: s^2*w*h*(3*4 + 3) (CV_32FC3 accumulator + one CV_8UC3 output-sized
// buffer) + 3*w*h (decoded frame) + 2*w*h (reference and current gray).
std::uint64_t estimatePeakBytes(int width, int height, int scale);

// invariant: throws SrError(E_INVALID_ARGUMENT) iff the request breaks the JS API rules.
void validate(const Request& request);

// invariant: reports every failure as SrError; codes live in Errors.h.
Result run(const Request& request);

}  // namespace sr
