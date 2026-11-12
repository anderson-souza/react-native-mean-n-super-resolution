#include <gtest/gtest.h>

#include "../sr/Align.h"
#include "../sr/Errors.h"
#include "../sr/Paths.h"
#include "../sr/Pipeline.h"
#include "fixtures.h"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace {

constexpr int kW = 200;
constexpr int kH = 160;
constexpr int kMargin = 16;

// invariant: |dx| <= 5% of kW (10 px), |dy| <= 5% of kH (8 px).
const std::vector<cv::Point> kShifts = {{0, 0}, {3, 2}, {-5, 4}, {8, -6}, {-9, -7}};

cv::Mat scene(int w, int h, int margin) {
  return fixtures::texturedImage(w + 2 * margin, h + 2 * margin, 7);
}

// invariant: frame(x) == ref(x - shift), i.e. content moved by +shift.
cv::Mat shiftedFrame(const cv::Mat& sceneImg, int w, int h, int margin, cv::Point shift) {
  return sceneImg(cv::Rect(margin - shift.x, margin - shift.y, w, h)).clone();
}

cv::Mat unrelatedNoise(int w, int h, std::uint64_t seed) {
  cv::RNG rng(seed);
  cv::Mat m(h, w, CV_8UC3);
  rng.fill(m, cv::RNG::UNIFORM, cv::Scalar::all(0), cv::Scalar::all(256));
  return m;
}

cv::Mat gray(const cv::Mat& bgr) {
  cv::Mat g;
  cv::cvtColor(bgr, g, cv::COLOR_BGR2GRAY);
  return g;
}

cv::Mat upscale(const cv::Mat& img, int s) {
  const cv::Mat S = (cv::Mat_<double>(2, 3) << s, 0, 0, 0, s, 0);
  cv::Mat out;
  cv::warpAffine(img, out, S, cv::Size(s * img.cols, s * img.rows), cv::INTER_CUBIC,
                 cv::BORDER_REPLICATE);
  return out;
}

// invariant: region (in output pixels) covered by every shifted frame, shrunk by
// the cubic kernel footprint so border replication never enters the metric.
cv::Rect commonRegion(int s) {
  int x0 = 0, y0 = 0, x1 = kW, y1 = kH;
  for (const auto& d : kShifts) {
    x0 = std::max(x0, -d.x);
    y0 = std::max(y0, -d.y);
    x1 = std::min(x1, kW - d.x);
    y1 = std::min(y1, kH - d.y);
  }
  const int pad = 2 * s;
  return {s * x0 + pad, s * y0 + pad, s * (x1 - x0) - 2 * pad, s * (y1 - y0) - 2 * pad};
}

std::vector<std::string> writeFrames(const std::vector<cv::Mat>& frames) {
  const auto dir = fixtures::freshDir();
  std::vector<std::string> paths;
  for (size_t i = 0; i < frames.size(); ++i) {
    paths.push_back(fixtures::writePng(dir, "f" + std::to_string(i) + ".png", frames[i]));
  }
  return paths;
}

sr::Request requestFor(const std::vector<std::string>& paths, const std::string& out) {
  sr::Request r;
  r.paths = paths;
  r.outputPath = out;
  r.defaultOutputDir = std::filesystem::path(out).parent_path().string();
  return r;
}

}  // namespace

TEST(Align, ShiftedFramesBeatIdentityAndReach30dB) {
  const cv::Mat sc = scene(kW, kH, kMargin);
  std::vector<cv::Mat> frames;
  for (const auto& d : kShifts) frames.push_back(shiftedFrame(sc, kW, kH, kMargin, d));
  const auto paths = writeFrames(frames);
  const std::string out = (std::filesystem::path(paths[0]).parent_path() / "out.png").string();

  const sr::Result res = sr::run(requestFor(paths, out));
  ASSERT_EQ(res.framesUsed, 5);

  const int s = 2;
  const cv::Rect roi = commonRegion(s);
  ASSERT_GT(roi.width, 0);
  ASSERT_GT(roi.height, 0);

  const cv::Mat reference = upscale(frames[0], s);
  const cv::Mat aligned = cv::imread(out, cv::IMREAD_COLOR);

  cv::Mat acc = cv::Mat::zeros(reference.size(), CV_32FC3);
  for (const auto& f : frames) cv::accumulate(upscale(f, s), acc);
  cv::Mat unaligned;
  acc.convertTo(unaligned, CV_8UC3, 1.0 / frames.size());

  const double psnrAligned = cv::PSNR(aligned(roi), reference(roi));
  const double psnrIdentity = cv::PSNR(unaligned(roi), reference(roi));
  EXPECT_GE(psnrAligned, 30.0);
  EXPECT_GT(psnrAligned, psnrIdentity);
}

TEST(Align, EstimatesIntegerTranslationWithinHalfPixel) {
  const cv::Mat sc = scene(kW, kH, kMargin);
  const cv::Mat ref = gray(shiftedFrame(sc, kW, kH, kMargin, {0, 0}));
  for (size_t i = 1; i < kShifts.size(); ++i) {
    const cv::Point d = kShifts[i];
    bool ok = false;
    const cv::Mat A = sr::estimateTransform(ref, gray(shiftedFrame(sc, kW, kH, kMargin, d)), ok);
    ASSERT_TRUE(ok) << d;
    EXPECT_NEAR(A.at<double>(0, 2), -d.x, 0.5) << d;
    EXPECT_NEAR(A.at<double>(1, 2), -d.y, 0.5) << d;
  }
}

TEST(Align, EstimatesTranslationOnFramesLargerThan1024px) {
  constexpr int w = 1600, h = 1200, margin = 64;
  const cv::Point d(52, -41);
  const cv::Mat sc = fixtures::texturedImage(w + 2 * margin, h + 2 * margin, 11);
  bool ok = false;
  const cv::Mat A = sr::estimateTransform(gray(shiftedFrame(sc, w, h, margin, {0, 0})),
                                          gray(shiftedFrame(sc, w, h, margin, d)), ok);
  ASSERT_TRUE(ok);
  EXPECT_NEAR(A.at<double>(0, 2), -d.x, 0.5);
  EXPECT_NEAR(A.at<double>(1, 2), -d.y, 0.5);
}

TEST(Align, SkipsOneUnrelatedFrame) {
  const cv::Mat sc = scene(kW, kH, kMargin);
  std::vector<cv::Mat> frames;
  for (const auto& d : kShifts) frames.push_back(shiftedFrame(sc, kW, kH, kMargin, d));
  frames[2] = unrelatedNoise(kW, kH, 99);
  const auto paths = writeFrames(frames);
  const std::string out = (std::filesystem::path(paths[0]).parent_path() / "out.png").string();

  const sr::Result res = sr::run(requestFor(paths, out));
  EXPECT_EQ(res.framesUsed, 4);
  EXPECT_TRUE(std::filesystem::exists(out));
}

TEST(Align, FailsWhenFewerThanTwoFramesAlign) {
  const cv::Mat sc = scene(kW, kH, kMargin);
  std::vector<cv::Mat> frames = {shiftedFrame(sc, kW, kH, kMargin, {0, 0})};
  for (int i = 1; i < 5; ++i) frames.push_back(unrelatedNoise(kW, kH, 100 + i));
  const auto paths = writeFrames(frames);
  const std::string out = (std::filesystem::path(paths[0]).parent_path() / "out.png").string();

  std::string code = "<none>";
  try {
    sr::run(requestFor(paths, out));
  } catch (const sr::SrError& e) {
    code = e.code;
  }
  EXPECT_EQ(code, "E_ALIGNMENT_FAILED");
}
