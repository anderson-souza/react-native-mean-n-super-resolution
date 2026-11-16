#pragma once

#include <string>

namespace sr {

std::string toFsPath(const std::string& pathOrUri);
std::string toFileUri(const std::string& fsPath);
std::string lowerExt(const std::string& path);
std::string uniqueOutputPath(const std::string& dir);

}  // namespace sr
