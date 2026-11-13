#import <Foundation/Foundation.h>

#include "../cpp/platform/TempDir.h"
#include "../cpp/sr/Errors.h"

#include <string>

namespace sr::platform {

std::string defaultOutputDir() {
  @autoreleasepool {
    NSString* dir = NSTemporaryDirectory();
    if (dir.length == 0) {
      throw SrError(code::WriteFailed, "no writable temp dir; pass options.outputPath");
    }
    return std::string(dir.fileSystemRepresentation);
  }
}

}  // namespace sr::platform
