#pragma once

#include <MeanNSuperResolutionSpecJSI.h>
#include <react/bridging/Promise.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace facebook::react {

using SuperResolveOptions =
  NativeMeanNSuperResolutionSuperResolveOptions<std::optional<double>, std::optional<std::string>>;

template <>
struct Bridging<SuperResolveOptions>
  : NativeMeanNSuperResolutionSuperResolveOptionsBridging<SuperResolveOptions> {};

using SuperResolveResult =
  NativeMeanNSuperResolutionSuperResolveResult<std::string, double, double, double>;

template <>
struct Bridging<SuperResolveResult>
  : NativeMeanNSuperResolutionSuperResolveResultBridging<SuperResolveResult> {};

class MeanNSuperResolutionImpl
  : public NativeMeanNSuperResolutionCxxSpec<MeanNSuperResolutionImpl> {
public:
  MeanNSuperResolutionImpl(std::shared_ptr<CallInvoker> jsInvoker);

  AsyncPromise<SuperResolveResult> superResolve(
    jsi::Runtime& rt,
    std::vector<std::string> paths,
    SuperResolveOptions options
  );
};

}
