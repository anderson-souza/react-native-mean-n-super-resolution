#import <Foundation/Foundation.h>
#import "MeanNSuperResolutionImpl.h"
#import <ReactCommon/CxxTurboModuleUtils.h>

@interface MeanNSuperResolutionOnLoad : NSObject
@end

@implementation MeanNSuperResolutionOnLoad

using namespace facebook::react;

+ (void)load
{
  registerCxxModuleToGlobalModuleMap(
    std::string(MeanNSuperResolutionImpl::kModuleName),
    [](std::shared_ptr<CallInvoker> jsInvoker) {
      return std::make_shared<MeanNSuperResolutionImpl>(jsInvoker);
    }
  );
}

@end
