#include "Paths.h"

#include "Errors.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <random>
#include <string_view>

namespace sr {
namespace {

constexpr std::string_view kFileScheme = "file://";

bool hasScheme(const std::string& s) {
  const auto pos = s.find("://");
  if (pos == std::string::npos || pos == 0) return false;
  if (!std::isalpha(static_cast<unsigned char>(s[0]))) return false;
  for (size_t i = 1; i < pos; ++i) {
    const auto c = static_cast<unsigned char>(s[i]);
    if (!std::isalnum(c) && c != '+' && c != '-' && c != '.') return false;
  }
  return true;
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

std::string percentDecode(std::string_view s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      const int hi = hexValue(s[i + 1]);
      const int lo = hexValue(s[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back(static_cast<char>(hi * 16 + lo));
        i += 2;
        continue;
      }
    }
    out.push_back(s[i]);
  }
  return out;
}

bool isUnreservedOrSlash(unsigned char c) {
  return std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~' || c == '/' || c == ':';
}

}  // namespace

std::string toFsPath(const std::string& pathOrUri) {
  if (pathOrUri.compare(0, kFileScheme.size(), kFileScheme) == 0) {
    return percentDecode(std::string_view(pathOrUri).substr(kFileScheme.size()));
  }
  if (hasScheme(pathOrUri)) {
    throw SrError(code::InvalidArgument,
                  "unsupported URI scheme (only absolute paths and file:// URIs): " + pathOrUri);
  }
  return pathOrUri;
}

std::string toFileUri(const std::string& fsPath) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string out(kFileScheme);
  for (const char ch : fsPath) {
    const auto c = static_cast<unsigned char>(ch);
    if (isUnreservedOrSlash(c)) {
      out.push_back(ch);
    } else {
      out.push_back('%');
      out.push_back(kHex[c >> 4]);
      out.push_back(kHex[c & 0x0F]);
    }
  }
  return out;
}

std::string lowerExt(const std::string& path) {
  std::string ext = std::filesystem::path(path).extension().string();
  for (auto& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return ext;
}

std::string uniqueOutputPath(const std::string& dir) {
  static std::atomic<std::uint64_t> counter{0};
  // why: the random tag keeps names unique across processes sharing one temp dir.
  static const std::uint32_t processTag = std::random_device{}();

  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
  const auto n = counter.fetch_add(1, std::memory_order_relaxed);
  const std::string name = "superres-" + std::to_string(ms) + "-" + std::to_string(n) + "-" +
                           std::to_string(processTag) + ".jpg";
  return (std::filesystem::path(dir) / name).string();
}

}  // namespace sr
