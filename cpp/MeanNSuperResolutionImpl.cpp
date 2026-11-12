#include "MeanNSuperResolutionImpl.h"

namespace facebook::react {

MeanNSuperResolutionImpl::MeanNSuperResolutionImpl(
  std::shared_ptr<CallInvoker> jsInvoker
)
  : NativeMeanNSuperResolutionCxxSpec(std::move(jsInvoker)) {}

double MeanNSuperResolutionImpl::multiply(
  jsi::Runtime& rt,
  double a,
  double b
) {
  return a * b;
}

}
