#include <gtest/gtest.h>

#include "../sr/Errors.h"
#include "../sr/Pipeline.h"

#include <optional>
#include <string>
#include <vector>

namespace {

constexpr const char* kAccepted = "<accepted>";

sr::Request validRequest(size_t frames = 5) {
  sr::Request r;
  r.paths.assign(frames, "/frames/a.jpg");
  r.defaultOutputDir = "/tmp";
  return r;
}

std::string outcome(const sr::Request& r) {
  try {
    sr::validate(r);
  } catch (const sr::SrError& e) {
    return e.code;
  }
  return kAccepted;
}

}  // namespace

TEST(Validate, AcceptsFrameCounts5_15_30) {
  for (size_t n : {5u, 15u, 30u}) EXPECT_EQ(outcome(validRequest(n)), kAccepted) << n;
}

TEST(Validate, RejectsOtherFrameCounts) {
  for (size_t n : {0u, 1u, 4u, 6u, 14u, 16u, 29u, 31u}) {
    EXPECT_EQ(outcome(validRequest(n)), "E_INVALID_ARGUMENT") << n;
  }
}

TEST(Validate, RejectsEmptyPathString) {
  auto r = validRequest();
  r.paths[3] = "";
  EXPECT_EQ(outcome(r), "E_INVALID_ARGUMENT");
}

TEST(Validate, AcceptsScale1To4) {
  for (int s : {1, 2, 3, 4}) {
    auto r = validRequest();
    r.scale = s;
    EXPECT_EQ(outcome(r), kAccepted) << s;
  }
}

TEST(Validate, RejectsScaleOutOfRange) {
  for (int s : {0, 5, -1}) {
    auto r = validRequest();
    r.scale = s;
    EXPECT_EQ(outcome(r), "E_INVALID_ARGUMENT") << s;
  }
}

TEST(Validate, AcceptsJpegAndPngOutputExtensions) {
  for (const char* p : {"/out/a.jpg", "/out/a.JPG", "/out/a.jpeg", "/out/a.png"}) {
    auto r = validRequest();
    r.outputPath = p;
    EXPECT_EQ(outcome(r), kAccepted) << p;
  }
}

TEST(Validate, RejectsOtherOutputExtensions) {
  for (const char* p : {"/out/a.bmp", "/out/a", ""}) {
    auto r = validRequest();
    r.outputPath = std::string(p);
    EXPECT_EQ(outcome(r), "E_INVALID_ARGUMENT") << '"' << p << '"';
  }
}
