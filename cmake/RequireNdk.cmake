# invariant: opencv-mobile v38 static libs reference libc++/OpenMP symbols that
# only NDK r30+ provides (r27/r29 fail at link with undefined __hash_memory);
# fail at configure with an actionable message instead.
function(sr_require_android_ndk)
  set(SR_MIN_NDK_MAJOR 30)
  set(_sr_ndk_dir "${CMAKE_ANDROID_NDK}")
  if(NOT _sr_ndk_dir)
    set(_sr_ndk_dir "${ANDROID_NDK}")
  endif()
  file(STRINGS "${_sr_ndk_dir}/source.properties" _sr_ndk_rev REGEX "^Pkg[.]Revision")
  string(REGEX MATCH "= *([0-9]+)" _sr_ndk_match "${_sr_ndk_rev}")
  if(NOT CMAKE_MATCH_1 OR CMAKE_MATCH_1 LESS SR_MIN_NDK_MAJOR)
    message(FATAL_ERROR
      "react-native-mean-n-super-resolution requires Android NDK r${SR_MIN_NDK_MAJOR}+ "
      "(found '${_sr_ndk_rev}' in ${_sr_ndk_dir}). Set ndkVersion = \"30.0.14904198\" "
      "(or newer) in your app's android/build.gradle ext block.")
  endif()
endfunction()
