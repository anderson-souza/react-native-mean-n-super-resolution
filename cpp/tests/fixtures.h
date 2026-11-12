#pragma once

#include <opencv2/core.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace fixtures {

// invariant: same (w, h, seed) always yields the same pixels (seeded cv::RNG).
cv::Mat texturedImage(int width, int height, std::uint64_t seed = 42);

// invariant: returns a fresh, empty directory unique to the running test.
std::filesystem::path freshDir();

std::string writePng(const std::filesystem::path& dir, const std::string& name, const cv::Mat& img);

std::vector<unsigned char> readBytes(const std::filesystem::path& file);

}  // namespace fixtures
