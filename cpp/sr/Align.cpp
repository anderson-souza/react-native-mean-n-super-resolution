#include "Align.h"

#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>

// hazard: SPEC_DEVIATION from design step 2 (phaseCorrelate init + ECC Euclidean refine).
// opencv-mobile is built without C++ exceptions, so a non-converging findTransformECC
// (CV_Error StsNoConv) aborts the whole process instead of throwing. Alignment is
// therefore translation-only via phaseCorrelate, the fallback the design names for a
// missing video module, and a frame is accepted when the normalized cross-correlation
// at the estimated shift is >= 0.85. Rotation between frames is not compensated.

namespace sr {
namespace {

constexpr int kMaxAlignSide = 1024;
constexpr double kMinCorrelation = 0.85;
constexpr double kMinOverlapFraction = 0.25;

cv::Mat toAlignLevel(const cv::Mat& gray8, double d) {
  cv::Mat f;
  if (d < 1.0) {
    cv::Mat small;
    cv::resize(gray8, small, cv::Size(), d, d, cv::INTER_AREA);
    small.convertTo(f, CV_32F);
  } else {
    gray8.convertTo(f, CV_32F);
  }
  return f;
}

// invariant: returns NCC in [-1, 1] of ref(x) vs cur(x + shift) over their overlap,
// or -1 when the overlap is too small or either side is flat.
double correlationAtShift(const cv::Mat& ref, const cv::Mat& cur, cv::Point2d shift) {
  const cv::Mat W = (cv::Mat_<double>(2, 3) << 1, 0, shift.x, 0, 1, shift.y);
  cv::Mat warped;
  cv::warpAffine(cur, warped, W, ref.size(), cv::INTER_LINEAR | cv::WARP_INVERSE_MAP,
                 cv::BORDER_REPLICATE);

  const int x0 = std::max(0, static_cast<int>(std::ceil(-shift.x)) + 1);
  const int y0 = std::max(0, static_cast<int>(std::ceil(-shift.y)) + 1);
  const int x1 = std::min(ref.cols, ref.cols - static_cast<int>(std::ceil(shift.x)) - 1);
  const int y1 = std::min(ref.rows, ref.rows - static_cast<int>(std::ceil(shift.y)) - 1);
  if (x1 - x0 <= 0 || y1 - y0 <= 0) return -1.0;
  const cv::Rect overlap(x0, y0, x1 - x0, y1 - y0);
  if (overlap.area() < kMinOverlapFraction * ref.size().area()) return -1.0;

  cv::Scalar mr, sdr, mc, sdc;
  cv::meanStdDev(ref(overlap), mr, sdr);
  cv::meanStdDev(warped(overlap), mc, sdc);
  if (sdr[0] < 1e-6 || sdc[0] < 1e-6) return -1.0;

  cv::Mat zr, zc;
  cv::subtract(ref(overlap), mr, zr);
  cv::subtract(warped(overlap), mc, zc);
  return zr.dot(zc) / (static_cast<double>(overlap.area()) * sdr[0] * sdc[0]);
}

}  // namespace

cv::Mat estimateTransform(const cv::Mat& refGray, const cv::Mat& gray, bool& ok) {
  ok = false;
  const cv::Mat identity = cv::Mat::eye(2, 3, CV_64F);

  // why: identical frames (including flat ones, where NCC is undefined) align trivially.
  if (cv::norm(refGray, gray, cv::NORM_INF) == 0) {
    ok = true;
    return identity;
  }

  const int longSide = std::max(refGray.cols, refGray.rows);
  const double d = longSide > kMaxAlignSide ? static_cast<double>(kMaxAlignSide) / longSide : 1.0;
  const cv::Mat ref = toAlignLevel(refGray, d);
  const cv::Mat cur = toAlignLevel(gray, d);

  cv::Mat window;
  cv::createHanningWindow(window, ref.size(), CV_32F);
  // hazard: phaseCorrelate multiplies its inputs by the window in place when they are
  // already DFT-optimal size, so it must get copies.
  const cv::Point2d shift = cv::phaseCorrelate(ref.clone(), cur.clone(), window);
  if (!std::isfinite(shift.x) || !std::isfinite(shift.y)) return identity;

  if (!(correlationAtShift(ref, cur, shift) >= kMinCorrelation)) return identity;

  // invariant: cur(x + shift) ~ ref(x), so frame->reference is a translation by -shift.
  ok = true;
  return (cv::Mat_<double>(2, 3) << 1, 0, -shift.x / d, 0, 1, -shift.y / d);
}

}  // namespace sr
