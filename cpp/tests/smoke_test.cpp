#include <gtest/gtest.h>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

#include <vector>

TEST(Smoke, EncodesMatAsPng) {
  cv::Mat img(8, 8, CV_8UC3, cv::Scalar(10, 20, 30));
  std::vector<uchar> buf;

  ASSERT_TRUE(cv::imencode(".png", img, buf));
  ASSERT_GE(buf.size(), 8u);
  const std::vector<uchar> pngMagic = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  EXPECT_EQ(std::vector<uchar>(buf.begin(), buf.begin() + 8), pngMagic);
}
