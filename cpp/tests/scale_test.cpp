#include <gtest/gtest.h>

#include "../sr/Scale.h"

#include <limits>
#include <optional>

TEST(Scale, MissingScaleDefaultsToTwo) {
  EXPECT_EQ(sr::toScale(std::nullopt), 2);
}

TEST(Scale, IntegralValuesPassThrough) {
  EXPECT_EQ(sr::toScale(1.0), 1);
  EXPECT_EQ(sr::toScale(2.0), 2);
  EXPECT_EQ(sr::toScale(3.0), 3);
  EXPECT_EQ(sr::toScale(4.0), 4);
}

TEST(Scale, NonIntegralMapsToZero) {
  EXPECT_EQ(sr::toScale(2.5), 0);
}

TEST(Scale, NonFiniteMapsToZero) {
  EXPECT_EQ(sr::toScale(std::numeric_limits<double>::quiet_NaN()), 0);
  EXPECT_EQ(sr::toScale(std::numeric_limits<double>::infinity()), 0);
  EXPECT_EQ(sr::toScale(-std::numeric_limits<double>::infinity()), 0);
}

TEST(Scale, HugeMagnitudeMapsToZero) {
  EXPECT_EQ(sr::toScale(1e7), 0);
  EXPECT_EQ(sr::toScale(-1e7), 0);
}

TEST(Scale, SmallNegativeIntegralIsKeptForValidateToReject) {
  EXPECT_EQ(sr::toScale(-1.0), -1);
}
