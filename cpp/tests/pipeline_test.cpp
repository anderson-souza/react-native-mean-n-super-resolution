#include <gtest/gtest.h>

#include "../platform/TempDir.h"
#include "../sr/Errors.h"
#include "../sr/Paths.h"
#include "../sr/Pipeline.h"
#include "fixtures.h"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr int kW = 64;
constexpr int kH = 48;

sr::Request requestFor(const std::vector<std::string>& paths, const fs::path& outDir) {
  sr::Request r;
  r.paths = paths;
  r.defaultOutputDir = outDir.string();
  return r;
}

std::vector<std::string> identicalFrames(const fs::path& dir, int n = 5) {
  const cv::Mat img = fixtures::texturedImage(kW, kH);
  std::vector<std::string> paths;
  for (int i = 0; i < n; ++i) {
    paths.push_back(fixtures::writePng(dir, "f" + std::to_string(i) + ".png", img));
  }
  return paths;
}

sr::SrError runExpectingError(const sr::Request& r) {
  try {
    sr::run(r);
  } catch (const sr::SrError& e) {
    return e;
  }
  return sr::SrError("<none>", "run() did not throw");
}

bool startsWith(const std::vector<unsigned char>& bytes, const std::vector<unsigned char>& magic) {
  return bytes.size() >= magic.size() && std::equal(magic.begin(), magic.end(), bytes.begin());
}

const std::vector<unsigned char> kPngMagic = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
const std::vector<unsigned char> kJpegMagic = {0xFF, 0xD8, 0xFF};

}  // namespace

TEST(Pipeline, IdenticalFramesDefaultScaleEqualsUpscaledReference) {
  const auto dir = fixtures::freshDir();
  auto r = requestFor(identicalFrames(dir), dir);
  r.outputPath = (dir / "out.png").string();

  const sr::Result res = sr::run(r);

  EXPECT_EQ(res.path.rfind("file://", 0), 0u) << res.path;
  const std::string outFs = sr::toFsPath(res.path);
  ASSERT_TRUE(fs::exists(outFs)) << outFs;
  EXPECT_EQ(res.width, 2 * kW);
  EXPECT_EQ(res.height, 2 * kH);
  EXPECT_EQ(res.framesUsed, 5);

  const cv::Mat out = cv::imread(outFs, cv::IMREAD_COLOR);
  ASSERT_FALSE(out.empty());
  EXPECT_EQ(out.cols, 2 * kW);
  EXPECT_EQ(out.rows, 2 * kH);

  const cv::Mat ref = cv::imread(r.paths[0], cv::IMREAD_COLOR);
  const cv::Mat S = (cv::Mat_<double>(2, 3) << 2, 0, 0, 0, 2, 0);
  cv::Mat expected;
  cv::warpAffine(ref, expected, S, cv::Size(2 * kW, 2 * kH), cv::INTER_CUBIC, cv::BORDER_REPLICATE);

  cv::Mat diff;
  cv::absdiff(out, expected, diff);
  double maxDiff = 0;
  cv::minMaxLoc(diff.reshape(1), nullptr, &maxDiff);
  EXPECT_LE(maxDiff, 1.0);
}

TEST(Pipeline, ScaleControlsOutputDimensions) {
  const auto dir = fixtures::freshDir();
  const auto paths = identicalFrames(dir);
  for (int s : {1, 3, 4}) {
    auto r = requestFor(paths, dir);
    r.scale = s;
    r.outputPath = (dir / ("out" + std::to_string(s) + ".png")).string();
    const sr::Result res = sr::run(r);
    EXPECT_EQ(res.width, s * kW) << s;
    EXPECT_EQ(res.height, s * kH) << s;
    const cv::Mat out = cv::imread(sr::toFsPath(res.path), cv::IMREAD_COLOR);
    EXPECT_EQ(out.cols, s * kW) << s;
    EXPECT_EQ(out.rows, s * kH) << s;
  }
}

TEST(Pipeline, OutputFormatFollowsExtension) {
  const auto dir = fixtures::freshDir();
  const auto paths = identicalFrames(dir);
  const struct {
    const char* name;
    const std::vector<unsigned char>* magic;
  } cases[] = {{"o.png", &kPngMagic}, {"o.jpg", &kJpegMagic}, {"o.jpeg", &kJpegMagic},
               {"o.Jpg", &kJpegMagic}, {"o.PNG", &kPngMagic}};
  for (const auto& c : cases) {
    auto r = requestFor(paths, dir);
    r.outputPath = (dir / c.name).string();
    const sr::Result res = sr::run(r);
    EXPECT_EQ(res.path, sr::toFileUri(*r.outputPath)) << c.name;
    EXPECT_TRUE(startsWith(fixtures::readBytes(*r.outputPath), *c.magic)) << c.name;
  }
}

TEST(Pipeline, OutputPathAcceptsFileUri) {
  const auto dir = fixtures::freshDir();
  auto r = requestFor(identicalFrames(dir), dir);
  const std::string target = (dir / "uri out.png").string();
  r.outputPath = sr::toFileUri(target);
  const sr::Result res = sr::run(r);
  EXPECT_EQ(res.path, sr::toFileUri(target));
  EXPECT_TRUE(fs::exists(target));
}

TEST(Pipeline, OverwritesExistingOutput) {
  const auto dir = fixtures::freshDir();
  auto r = requestFor(identicalFrames(dir), dir);
  r.outputPath = (dir / "out.png").string();
  { std::ofstream(*r.outputPath) << "stale contents"; }

  sr::run(r);

  EXPECT_TRUE(startsWith(fixtures::readBytes(*r.outputPath), kPngMagic));
  const cv::Mat out = cv::imread(*r.outputPath, cv::IMREAD_COLOR);
  EXPECT_EQ(out.cols, 2 * kW);
  EXPECT_EQ(out.rows, 2 * kH);
}

TEST(Pipeline, MissingFileFailsDecodeWithIndex) {
  const auto dir = fixtures::freshDir();
  auto paths = identicalFrames(dir);
  paths[2] = (dir / "does-not-exist.png").string();
  const auto e = runExpectingError(requestFor(paths, dir));
  EXPECT_EQ(e.code, "E_DECODE_FAILED");
  EXPECT_NE(e.message.find("index=2"), std::string::npos) << e.message;
}

TEST(Pipeline, TextFileFailsDecodeWithIndex) {
  const auto dir = fixtures::freshDir();
  auto paths = identicalFrames(dir);
  const auto txt = dir / "notes.txt";
  { std::ofstream(txt) << "this is not an image\n"; }
  paths[1] = txt.string();
  const auto e = runExpectingError(requestFor(paths, dir));
  EXPECT_EQ(e.code, "E_DECODE_FAILED");
  EXPECT_NE(e.message.find("index=1"), std::string::npos) << e.message;
}

TEST(Pipeline, DifferentSizeFrameFailsWithIndex) {
  const auto dir = fixtures::freshDir();
  auto paths = identicalFrames(dir);
  paths[3] = fixtures::writePng(dir, "small.png", fixtures::texturedImage(kW - 8, kH));
  const auto e = runExpectingError(requestFor(paths, dir));
  EXPECT_EQ(e.code, "E_SIZE_MISMATCH");
  EXPECT_NE(e.message.find("index=3"), std::string::npos) << e.message;
}

TEST(Pipeline, MissingOutputDirectoryFailsWrite) {
  const auto dir = fixtures::freshDir();
  auto r = requestFor(identicalFrames(dir), dir);
  r.outputPath = (dir / "no-such-dir" / "out.jpg").string();
  const auto e = runExpectingError(r);
  EXPECT_EQ(e.code, "E_WRITE_FAILED");
}

TEST(Pipeline, GrayAndAlphaInputsYieldThreeChannelOutput) {
  const auto dir = fixtures::freshDir();
  const cv::Mat color = fixtures::texturedImage(kW, kH);
  cv::Mat gray, rgba;
  cv::cvtColor(color, gray, cv::COLOR_BGR2GRAY);
  cv::cvtColor(color, rgba, cv::COLOR_BGR2BGRA);

  for (const auto& [name, img] : {std::pair<std::string, cv::Mat>{"gray", gray}, {"rgba", rgba}}) {
    std::vector<std::string> paths;
    for (int i = 0; i < 5; ++i) {
      paths.push_back(fixtures::writePng(dir, name + std::to_string(i) + ".png", img));
    }
    auto r = requestFor(paths, dir);
    r.outputPath = (dir / (name + "-out.png")).string();
    sr::run(r);
    const cv::Mat out = cv::imread(*r.outputPath, cv::IMREAD_UNCHANGED);
    ASSERT_FALSE(out.empty()) << name;
    EXPECT_EQ(out.channels(), 3) << name;
    EXPECT_EQ(out.depth(), CV_8U) << name;
  }
}

TEST(Pipeline, SamePathRepeatedIsValid) {
  const auto dir = fixtures::freshDir();
  const auto one = fixtures::writePng(dir, "only.png", fixtures::texturedImage(kW, kH));
  auto r = requestFor(std::vector<std::string>(5, one), dir);
  r.outputPath = (dir / "out.png").string();
  EXPECT_EQ(sr::run(r).framesUsed, 5);
}

TEST(Pipeline, RepeatedRunsAreByteIdentical) {
  const auto dir = fixtures::freshDir();
  const auto paths = identicalFrames(dir);
  auto a = requestFor(paths, dir);
  a.outputPath = (dir / "a.jpg").string();
  auto b = requestFor(paths, dir);
  b.outputPath = (dir / "b.jpg").string();
  sr::run(a);
  sr::run(b);
  const auto ba = fixtures::readBytes(*a.outputPath);
  ASSERT_FALSE(ba.empty());
  EXPECT_EQ(ba, fixtures::readBytes(*b.outputPath));
}

TEST(Pipeline, ConcurrentDefaultOutputsAreDistinctJpegsInTempDir) {
  const auto dir = fixtures::freshDir();
  const auto paths = identicalFrames(dir);
  const std::string tempDir = sr::platform::defaultOutputDir();

  sr::Result r1, r2;
  std::thread t1([&] { r1 = sr::run(requestFor(paths, tempDir)); });
  std::thread t2([&] { r2 = sr::run(requestFor(paths, tempDir)); });
  t1.join();
  t2.join();

  EXPECT_NE(r1.path, r2.path);
  for (const auto& res : {r1, r2}) {
    const fs::path p = sr::toFsPath(res.path);
    EXPECT_TRUE(fs::exists(p)) << p;
    EXPECT_TRUE(fs::equivalent(p.parent_path(), tempDir)) << p;
    EXPECT_TRUE(startsWith(fixtures::readBytes(p), kJpegMagic)) << p;
    fs::remove(p);
  }
}
