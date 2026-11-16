#pragma once

#include <string>

namespace sr::platform {

// invariant: resolved on every call (never cached), so tests and hosts can redirect it.
std::string defaultOutputDir();

}  // namespace sr::platform
