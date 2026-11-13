#include "platform/TempDir.h"

#include "sr/Errors.h"

#include <fbjni/fbjni.h>

#include <cstdlib>
#include <string>
#include <unistd.h>

namespace sr::platform {
namespace {

namespace jni = facebook::jni;

struct JFile : jni::JavaClass<JFile> {
  static constexpr auto kJavaDescriptor = "Ljava/io/File;";

  std::string absolutePath() const {
    static const auto method = javaClassStatic()->getMethod<jstring()>("getAbsolutePath");
    return method(self())->toStdString();
  }
};

struct JApplication : jni::JavaClass<JApplication> {
  static constexpr auto kJavaDescriptor = "Landroid/app/Application;";

  jni::local_ref<JFile> cacheDir() const {
    static const auto method = javaClassStatic()->getMethod<JFile::javaobject()>("getCacheDir");
    return method(self());
  }
};

// hazard: ActivityThread.currentApplication() is a hidden (greylisted) API; it is
// only the fallback after TMPDIR, and failures map to an E_WRITE_FAILED hint.
struct JActivityThread : jni::JavaClass<JActivityThread> {
  static constexpr auto kJavaDescriptor = "Landroid/app/ActivityThread;";

  static jni::local_ref<JApplication> currentApplication() {
    static const auto method =
        javaClassStatic()->getStaticMethod<JApplication::javaobject()>("currentApplication");
    return method(javaClassStatic());
  }
};

std::string cacheDirViaJni() {
  // why: called from the detached worker thread, which the JVM does not know yet.
  jni::ThreadScope scope;
  const auto app = JActivityThread::currentApplication();
  if (!app) return {};
  const auto dir = app->cacheDir();
  if (!dir) return {};
  return dir->absolutePath();
}

bool writableDir(const char* dir) {
  return dir != nullptr && *dir != '\0' && access(dir, W_OK) == 0;
}

}  // namespace

std::string defaultOutputDir() {
  if (const char* tmp = std::getenv("TMPDIR"); writableDir(tmp)) return tmp;
  std::string cache;
  try {
    cache = cacheDirViaJni();
  } catch (const std::exception&) {
    cache.clear();
  }
  if (!writableDir(cache.c_str())) {
    throw SrError(code::WriteFailed, "no writable temp dir; pass options.outputPath");
  }
  return cache;
}

}  // namespace sr::platform
