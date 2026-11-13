#include <gtest/gtest.h>

#include "../sr/ErrorMapping.h"
#include "../sr/Errors.h"

#include <exception>
#include <new>
#include <stdexcept>
#include <string>

namespace {

template <typename E>
std::string mapped(E e) {
  return sr::toRejectMessage(std::make_exception_ptr(e));
}

}  // namespace

TEST(ErrorMapping, SrErrorKeepsItsCodeAndMessage) {
  EXPECT_EQ(mapped(sr::SrError(sr::code::DecodeFailed, "index=2 could not decode image: /a")),
            "E_DECODE_FAILED: index=2 could not decode image: /a");
  EXPECT_EQ(mapped(sr::SrError(sr::code::Internal, "estimated memory 2048 MiB exceeds budget")),
            "E_INTERNAL: estimated memory 2048 MiB exceeds budget");
}

TEST(ErrorMapping, BadAllocBecomesInternal) {
  EXPECT_EQ(mapped(std::bad_alloc()), "E_INTERNAL: out of memory");
}

TEST(ErrorMapping, StdExceptionBecomesInternalWithWhat) {
  EXPECT_EQ(mapped(std::runtime_error("boom")), "E_INTERNAL: boom");
  EXPECT_EQ(mapped(std::out_of_range("vector index")), "E_INTERNAL: vector index");
}

TEST(ErrorMapping, NonStdExceptionBecomesInternalUnknown) {
  EXPECT_EQ(mapped(42), "E_INTERNAL: unknown error");
}

TEST(ErrorMapping, NullExceptionPtrBecomesInternalUnknown) {
  EXPECT_EQ(sr::toRejectMessage(nullptr), "E_INTERNAL: unknown error");
}
