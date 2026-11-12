#pragma once

#include <opencv2/core.hpp>

namespace sr {

// invariant: returns a 2x3 CV_64F affine A mapping frame coordinates to reference
// coordinates (a frame whose content moved by +d yields A ~ [I | -d]). ok is false,
// and A is identity, when the frame's correlation with the reference at the estimated
// shift is below the acceptance threshold (0.85).
cv::Mat estimateTransform(const cv::Mat& refGray, const cv::Mat& gray, bool& ok);

}  // namespace sr
