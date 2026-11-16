#include "../platform/TempDir.h"

#include <filesystem>

namespace sr::platform {

std::string defaultOutputDir() {
  return std::filesystem::temp_directory_path().string();
}

}  // namespace sr::platform
