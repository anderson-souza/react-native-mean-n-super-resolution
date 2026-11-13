#include "ErrorMapping.h"

#include "Errors.h"

#include <new>

namespace sr {

std::string toRejectMessage(std::exception_ptr error) {
  const std::string internal = std::string(code::Internal) + ": ";
  if (!error) return internal + "unknown error";
  try {
    std::rethrow_exception(error);
  } catch (const SrError& e) {
    return e.what();
  } catch (const std::bad_alloc&) {
    return internal + "out of memory";
  } catch (const std::exception& e) {
    return internal + e.what();
  } catch (...) {
    return internal + "unknown error";
  }
}

}  // namespace sr
