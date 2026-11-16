#include "MeanNSuperResolutionImpl.h"

#include "platform/TempDir.h"
#include "sr/ErrorMapping.h"
#include "sr/Pipeline.h"
#include "sr/Scale.h"

#include <exception>
#include <thread>
#include <utility>

namespace facebook::react {

MeanNSuperResolutionImpl::MeanNSuperResolutionImpl(
  std::shared_ptr<CallInvoker> jsInvoker
)
  : NativeMeanNSuperResolutionCxxSpec(std::move(jsInvoker)) {}

AsyncPromise<SuperResolveResult> MeanNSuperResolutionImpl::superResolve(
  jsi::Runtime& rt,
  std::vector<std::string> paths,
  SuperResolveOptions options
) {
  AsyncPromise<SuperResolveResult> promise(rt, jsInvoker_);

  sr::Request request;
  request.paths = std::move(paths);
  request.scale = sr::toScale(options.scale);
  request.outputPath = std::move(options.outputPath);

  // invariant: the worker owns copies of the request and promise and never
  // captures `this`; the module may be destroyed before the worker finishes.
  auto work = [promise, request = std::move(request)]() mutable {
    try {
      // why: resolved on the worker so a missing temp dir rejects instead of
      // throwing into JS; Android may need JNI here (see android/src/TempDir.cpp).
      if (!request.outputPath) request.defaultOutputDir = sr::platform::defaultOutputDir();
      const sr::Result r = sr::run(request);
      promise.resolve(SuperResolveResult{
        r.path,
        static_cast<double>(r.width),
        static_cast<double>(r.height),
        static_cast<double>(r.framesUsed)});
    } catch (...) {
      promise.reject(Error(sr::toRejectMessage(std::current_exception())));
    }
  };

  try {
    std::thread(std::move(work)).detach();
  } catch (...) {
    promise.reject(Error(sr::toRejectMessage(std::current_exception())));
  }
  return promise;
}

}
