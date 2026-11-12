#pragma once

#include <MeanNSuperResolutionSpecJSI.h>

#include <memory>

namespace facebook::react {

class MeanNSuperResolutionImpl
  : public NativeMeanNSuperResolutionCxxSpec<MeanNSuperResolutionImpl> {
public:
  MeanNSuperResolutionImpl(std::shared_ptr<CallInvoker> jsInvoker);

  double multiply(jsi::Runtime& rt, double a, double b);
};

}
