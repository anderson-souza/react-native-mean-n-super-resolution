#pragma once

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

// invariant: throws SrError(E_INVALID_ARGUMENT) iff the request breaks the JS API rules.
void validate(const Request& request);

// invariant: reports every failure as SrError; codes live in Errors.h.
Result run(const Request& request);

}  // namespace sr
