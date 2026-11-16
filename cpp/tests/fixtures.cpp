#include "fixtures.h"

#include <gtest/gtest.h>

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <fstream>
#include <iterator>
#include <stdexcept>

namespace fixtures {

cv::Mat texturedImage(int width, int height, std::uint64_t seed) {
  cv::RNG rng(seed);
  cv::Mat noise(height, width, CV_8UC3);
  rng.fill(noise, cv::RNG::UNIFORM, cv::Scalar::all(0), cv::Scalar::all(256));
  // why: smooth texture with strong edges gives phase correlation / ECC a usable signal.
  cv::Mat img;
  cv::GaussianBlur(noise, img, cv::Size(0, 0), 2.0);
  for (int i = 0; i < 12; ++i) {
    const cv::Point c(rng.uniform(0, width), rng.uniform(0, height));
    const int r = rng.uniform(2, std::max(3, std::min(width, height) / 6));
    const cv::Scalar color(rng.uniform(0, 256), rng.uniform(0, 256), rng.uniform(0, 256));
    cv::circle(img, c, r, color, cv::FILLED, cv::LINE_AA);
  }
  return img;
}

std::filesystem::path freshDir() {
  const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
  const auto dir = std::filesystem::temp_directory_path() / "sr-native-tests" /
                   (std::string(info->test_suite_name()) + "." + info->name());
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  return dir;
}

std::string writePng(const std::filesystem::path& dir, const std::string& name, const cv::Mat& img) {
  const auto p = (dir / name).string();
  if (!cv::imwrite(p, img)) throw std::runtime_error("fixture imwrite failed: " + p);
  return p;
}

std::vector<unsigned char> readBytes(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

}  // namespace fixtures
