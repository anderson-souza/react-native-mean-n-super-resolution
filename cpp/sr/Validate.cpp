#include "Errors.h"
#include "Paths.h"
#include "Pipeline.h"

#include <string>

namespace sr {

void validate(const Request& request) {
  const auto n = request.paths.size();
  if (n != 5 && n != 15 && n != 30) {
    throw SrError(code::InvalidArgument,
                  "paths must contain exactly 5, 15 or 30 entries, got " + std::to_string(n));
  }
  for (size_t i = 0; i < n; ++i) {
    if (request.paths[i].empty()) {
      throw SrError(code::InvalidArgument, "paths[" + std::to_string(i) + "] is empty");
    }
  }
  if (request.scale < 1 || request.scale > 4) {
    throw SrError(code::InvalidArgument,
                  "scale must be 1, 2, 3 or 4, got " + std::to_string(request.scale));
  }
  if (request.outputPath) {
    const auto ext = lowerExt(*request.outputPath);
    if (request.outputPath->empty() || (ext != ".jpg" && ext != ".jpeg" && ext != ".png")) {
      throw SrError(code::InvalidArgument,
                    "outputPath must end in .jpg, .jpeg or .png: '" + *request.outputPath + "'");
    }
  }
}

}  // namespace sr
