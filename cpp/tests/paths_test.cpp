#include <gtest/gtest.h>

#include "../sr/Errors.h"
#include "../sr/Paths.h"

#include <filesystem>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string codeOf(void (*fn)()) {
  try {
    fn();
  } catch (const sr::SrError& e) {
    return e.code;
  }
  return "<no SrError thrown>";
}

}  // namespace

TEST(ToFsPath, StripsFileScheme) {
  EXPECT_EQ(sr::toFsPath("file:///a/b.jpg"), "/a/b.jpg");
}

TEST(ToFsPath, PercentDecodesFileUri) {
  EXPECT_EQ(sr::toFsPath("file:///a/my%20img.jpg"), "/a/my img.jpg");
}

TEST(ToFsPath, PlainAbsolutePathUnchanged) {
  EXPECT_EQ(sr::toFsPath("/a/b.jpg"), "/a/b.jpg");
}

TEST(ToFsPath, RejectsContentScheme) {
  EXPECT_EQ(codeOf([] { sr::toFsPath("content://x"); }), "E_INVALID_ARGUMENT");
}

TEST(ToFsPath, RejectsPhotosScheme) {
  EXPECT_EQ(codeOf([] { sr::toFsPath("ph://x"); }), "E_INVALID_ARGUMENT");
}

TEST(ToFsPath, RejectsHttpScheme) {
  EXPECT_EQ(codeOf([] { sr::toFsPath("http://x"); }), "E_INVALID_ARGUMENT");
}

TEST(LowerExt, LowercasesExtension) {
  EXPECT_EQ(sr::lowerExt("X.JPEG"), ".jpeg");
}

TEST(ToFileUri, RoundTripsWithToFsPath) {
  for (const std::string p : {"/a/b.jpg", "/a/my img.jpg", "/x/100%/y.png"}) {
    const std::string uri = sr::toFileUri(p);
    EXPECT_EQ(uri.rfind("file://", 0), 0u) << uri;
    EXPECT_EQ(sr::toFsPath(uri), p) << uri;
  }
}

TEST(UniqueOutputPath, DistinctAcrossThreads) {
  const std::string dir = (std::filesystem::temp_directory_path() / "sr-unique").string();
  constexpr int kThreads = 8;
  constexpr int kPerThread = 125;

  std::mutex mu;
  std::vector<std::string> all;
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; ++t) {
    threads.emplace_back([&] {
      std::vector<std::string> local;
      for (int i = 0; i < kPerThread; ++i) local.push_back(sr::uniqueOutputPath(dir));
      std::lock_guard<std::mutex> lock(mu);
      all.insert(all.end(), local.begin(), local.end());
    });
  }
  for (auto& th : threads) th.join();

  ASSERT_EQ(all.size(), 1000u);
  EXPECT_EQ(std::set<std::string>(all.begin(), all.end()).size(), 1000u);
  for (const auto& p : all) {
    const std::filesystem::path fp(p);
    EXPECT_EQ(fp.extension().string(), ".jpg") << p;
    EXPECT_EQ(fp.parent_path(), std::filesystem::path(dir)) << p;
  }
}
