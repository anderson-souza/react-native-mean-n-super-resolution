#include "Pipeline.h"

#include "Align.h"
#include "Errors.h"
#include "Paths.h"

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <cmath>
#include <fstream>
#include <string>
#include <vector>

namespace sr {
namespace {

std::string sizeText(const cv::Size& s) {
  return std::to_string(s.width) + "x" + std::to_string(s.height);
}

cv::Mat decodeFrame(const std::string& pathOrUri, size_t index) {
  const std::string fsPath = toFsPath(pathOrUri);
  cv::Mat img = cv::imread(fsPath, cv::IMREAD_COLOR);
  if (img.empty() || img.cols <= 0 || img.rows <= 0) {
    throw SrError(code::DecodeFailed,
                  "index=" + std::to_string(index) + " could not decode image: " + fsPath);
  }
  // invariant: every later OpenCV call assumes 8-bit BGR (AD-004: asserts abort).
  if (img.type() != CV_8UC3) {
    throw SrError(code::Internal, "index=" + std::to_string(index) +
                                      " decoded to unexpected type " + std::to_string(img.type()));
  }
  return img;
}

void encodeTo(const std::string& fsPath, const cv::Mat& img) {
  // why: opencv-mobile's imwrite matches extensions case-sensitively (.Jpg fails),
  // so encode with the lower-cased extension and write the bytes ourselves.
  const std::string ext = lowerExt(fsPath);
  std::vector<int> params;
  if (ext == ".jpg" || ext == ".jpeg") params = {cv::IMWRITE_JPEG_QUALITY, 95};

  std::vector<uchar> bytes;
  if (!cv::imencode(ext, img, bytes, params)) {
    throw SrError(code::WriteFailed, "could not encode " + ext);
  }

  std::ofstream out(fsPath, std::ios::binary | std::ios::trunc);
  if (!out) throw SrError(code::WriteFailed, "could not open for writing: " + fsPath);
  out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  out.close();
  if (!out) throw SrError(code::WriteFailed, "could not write: " + fsPath);
}

bool isFinite(const cv::Mat& m) {
  for (int r = 0; r < m.rows; ++r) {
    for (int c = 0; c < m.cols; ++c) {
      if (!std::isfinite(m.at<double>(r, c))) return false;
    }
  }
  return true;
}

}  // namespace

std::uint64_t estimatePeakBytes(int width, int height, int scale) {
  const std::uint64_t px = static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
  const std::uint64_t s2 = static_cast<std::uint64_t>(scale) * static_cast<std::uint64_t>(scale);
  return s2 * px * (3 * 4 + 3) + 3 * px + 2 * px;
}

Result run(const Request& request) {
  validate(request);

  const std::string outFs =
      request.outputPath ? toFsPath(*request.outputPath) : uniqueOutputPath(request.defaultOutputDir);
  const double s = request.scale;

  cv::Size refSize;
  cv::Size outSize;
  cv::Mat refGray;
  cv::Mat acc;
  int framesUsed = 0;

  // invariant: one decoded frame alive at a time; only refGray and the accumulator persist.
  for (size_t i = 0; i < request.paths.size(); ++i) {
    const cv::Mat frame = decodeFrame(request.paths[i], i);
    if (i == 0) {
      const std::uint64_t peak = estimatePeakBytes(frame.cols, frame.rows, request.scale);
      if (peak > kMemoryBudgetBytes) {
        throw SrError(code::Internal, "estimated memory " + std::to_string(peak >> 20) +
                                          " MiB exceeds budget of " +
                                          std::to_string(kMemoryBudgetBytes >> 20) + " MiB");
      }
      refSize = frame.size();
      outSize = cv::Size(request.scale * refSize.width, request.scale * refSize.height);
      acc = cv::Mat::zeros(outSize, CV_32FC3);
      cv::cvtColor(frame, refGray, cv::COLOR_BGR2GRAY);
    } else if (frame.size() != refSize) {
      throw SrError(code::SizeMismatch, "index=" + std::to_string(i) + " is " +
                                            sizeText(frame.size()) + ", expected " +
                                            sizeText(refSize));
    }

    // invariant: the reference is never estimated or skipped; it always uses A = I.
    cv::Mat A = cv::Mat::eye(2, 3, CV_64F);
    if (i > 0) {
      cv::Mat gray;
      cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
      bool aligned = false;
      A = estimateTransform(refGray, gray, aligned);
      if (!aligned || !isFinite(A)) continue;
    }
    const cv::Mat M = s * A;

    cv::Mat warped;
    cv::warpAffine(frame, warped, M, outSize, cv::INTER_CUBIC, cv::BORDER_REPLICATE);
    cv::accumulate(warped, acc);
    ++framesUsed;
  }

  if (framesUsed < 2) {
    throw SrError(code::AlignmentFailed,
                  "only " + std::to_string(framesUsed) + " frame(s) usable, need at least 2");
  }

  cv::Mat mean;
  acc.convertTo(mean, CV_8UC3, 1.0 / framesUsed);
  acc.release();

  encodeTo(outFs, mean);
  return Result{toFileUri(outFs), outSize.width, outSize.height, framesUsed};
}

}  // namespace sr
