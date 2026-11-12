# opencv-super-resolution Design

**Spec**: `.specs/features/opencv-super-resolution/spec.md`
**Context**: `.specs/features/opencv-super-resolution/context.md`
**Status**: Approved
**Conforms to**: AD-001 (pure C++ TurboModule). Adds AD-002, AD-003.

---

## Architecture Overview

Three layers. JS validates arguments and calls the TurboModule. The TurboModule creates a Promise, hands the work to a worker thread and settles the Promise through the `CallInvoker`. The pipeline is plain C++ plus OpenCV and has no JSI, so it can be tested on a desktop machine.

```mermaid
graph TD
    App[App JS] -->|superResolve paths, options| W[src/superResolve.native.ts<br/>validate → E_INVALID_ARGUMENT]
    W -->|TurboModule call| TM[cpp/MeanNSuperResolutionImpl<br/>AsyncPromise + worker thread]
    TM -->|Request| P[cpp/sr/Pipeline<br/>decode → align → warp/upscale → mean → encode]
    P --> PA[cpp/sr/Paths<br/>file:// decode, ext, unique name]
    PA --> TD[cpp/platform/TempDir<br/>iOS: NSTemporaryDirectory<br/>Android: TMPDIR / JNI cacheDir<br/>host: std::filesystem]
    P --> CV[(opencv-mobile<br/>core, imgproc, video, highgui)]
    P -->|Result or SrError| TM
    TM -->|jsInvoker: resolve / reject 'E_CODE: msg'| W
    Web[src/superResolve.ts web] -->|reject 'only supported on native platforms'| App
    HT[cpp/tests host gtest] --> P
```

### Pipeline algorithm (SR-CORE-02..04, SR-CFG-01)

For N frames, reference `R = frame[0]`, scale `s`:

1. **Decode** each path with `cv::imread(p, IMREAD_COLOR)`, which always gives 8-bit BGR (grayscale and alpha are converted, EXIF orientation is applied). If the result is empty → `E_DECODE_FAILED index=i`. If its size differs from R → `E_SIZE_MISMATCH index=i`.
2. **Estimate motion** of frame i to R on grayscale, downscaled so the long side is at most 1024 px (factor `d`):
   - Initial guess: `cv::phaseCorrelate` for the translation.
   - Refine with `cv::findTransformECC(MOTION_EUCLIDEAN, criteria COUNT|EPS 50 / 1e-4, gaussFiltSize 5)`.
   - Then scale the translation back by `1/d`.
   - Failure: if ECC throws or reports correlation < `0.85` → frame skipped (SR-ERR-06). R is never skipped.
3. **Warp + upscale in one interpolation**: `cv::warpAffine(frame_i, out, S·A_i, (s·w, s·h), INTER_CUBIC, BORDER_REPLICATE)`, with `S = diag(s, s)` and `A_i` the 2×3 transform from frame i to R. R uses `S·I`, so identical inputs go through exactly the same operation, which makes SR-CORE-03 hold by construction.
4. **Mean**: accumulate into `CV_32FC3` in index order, divide by `framesUsed`, convert to `CV_8UC3` with rounding. If `framesUsed < 2` → `E_ALIGNMENT_FAILED`.
5. **Encode** with `cv::imwrite`:
   - `.jpg`/`.jpeg` → `IMWRITE_JPEG_QUALITY 95`; `.png` → default.
   - If `imwrite` returns false or throws → `E_WRITE_FAILED`.

Pixels the warp leaves uncovered are filled with replicated borders, so the output stays defined. SR-CORE-04's PSNR is measured only in the region every frame covers (spec).

---

## Code Reuse Analysis

### Existing Components to Leverage

| Component | Location | How to Use |
| --------- | -------- | ---------- |
| Codegen spec pattern | `src/NativeMeanNSuperResolution.ts` | Replace `multiply` with `superResolve`; keep `TurboModuleRegistry.getEnforcing` |
| `.native.tsx` / `.tsx` split | `src/multiply.native.tsx`, `src/multiply.tsx` | Same pattern for `superResolve` (native impl / web reject); delete multiply files |
| C++ spec subclass | `cpp/MeanNSuperResolutionImpl.{h,cpp}` | Keep class + registration; swap method |
| iOS registration | `ios/OnLoad.mm` | Unchanged |
| Android autolinking | `react-native.config.js`, `android/CMakeLists.txt` | Extend CMake: new sources, OpenCV, fbjni |
| `AsyncPromise<T>` | `node_modules/react-native/ReactCommon/react/bridging/Promise.h` | Thread-safe resolve/reject that dispatches via `CallInvoker` (verified in source: mutex-guarded `AsyncCallback`) |
| Jest config | `package.json` `jest` | Already resolves `src` via source export condition; add tests in `src/__tests__/` |
| Turbo cache inputs | `turbo.json` | Add `opencv.json` to build inputs |

### Integration Points

| System | Integration Method |
| ------ | ------------------ |
| React Native codegen | Typed object aliases in Spec → generated `NativeMeanNSuperResolution…` structs + `Bridging<>` in `MeanNSuperResolutionSpecJSI.h` (exact generated names confirmed in T-task after first codegen run) |
| Android app build | Library CMake is pulled into app `appmodules` by autolinking. OpenCV is fetched and found inside that CMake run, so the consumer adds no Gradle deps |
| CocoaPods | Podspec downloads the opencv-mobile Apple package at spec evaluation and vendors `opencv2.xcframework` |
| CI | `ci.yml`: add `test-native` job (host gtest); existing build-android/ios exercise real linking |

---

## Components

### JS API — `src/superResolve.native.ts`, `src/superResolve.ts`, `src/types.ts`, `src/errors.ts`

- **Purpose**: Public typed API; argument validation; web fallback.
- **Interfaces**:
  - `superResolve(paths: string[], options?: SuperResolveOptions): Promise<SuperResolveResult>`
  - `ErrorCodes` const map (`E_INVALID_ARGUMENT`, …) exported for callers to match `error.message.startsWith(code)`.
  - `validateArgs(paths, options): string | null`: pure function that returns an error message or null. Used by the native wrapper and tested directly.
- **Rules**: `paths` is an array of non-empty strings with `length ∈ {5,15,30}`; `scale` is undefined or one of `{1,2,3,4}`; `outputPath` is undefined or a non-empty string whose lowercase extension is one of `.jpg`/`.jpeg`/`.png`. Any violation returns `Promise.reject(new Error('E_INVALID_ARGUMENT: …'))` and the native code is not called (SR-ERR-01..03).
- **Web**: `superResolve` returns `Promise.reject(new Error("'react-native-mean-n-super-resolution' is only supported on native platforms."))` (SR-ERR-09).
- **Reuses**: existing split-file pattern.

### TurboModule Spec — `src/NativeMeanNSuperResolution.ts`

```ts
export type SuperResolveOptions = { scale?: number; outputPath?: string };
export type SuperResolveResult = { path: string; width: number; height: number; framesUsed: number };
export interface Spec extends TurboModule {
  superResolve(paths: string[], options: SuperResolveOptions): Promise<SuperResolveResult>;
}
```

`options` is required at the native boundary; the JS wrapper passes `{}` when the caller gives none. This avoids an optional-object codegen edge case.

### TurboModule Impl — `cpp/MeanNSuperResolutionImpl.{h,cpp}`

- **Purpose**: Bridge JSI ↔ pipeline; async execution; error mapping.
- **Interface**: `AsyncPromise<SuperResolveResultStruct> superResolve(jsi::Runtime&, std::vector<std::string> paths, OptionsStruct options)` (types = codegen-generated).
- **Behavior**:
  1. Build `sr::Request` on the JS thread (cheap copy).
  2. Create `AsyncPromise` with `jsInvoker_`.
  3. Start a detached `std::thread` that captures the request and promise by value, never `this`.
  4. The worker runs `sr::run(request)` inside `try`:
     - `sr::SrError e` → `reject(Error(e.code + ": " + e.message))`
     - `cv::Exception`, `std::bad_alloc`, `std::exception`, or `...` → `E_INTERNAL: …` (SR-ERR-08)
     - otherwise `resolve(result)`.
- **Concurrency**: each call gets its own thread and state, with no shared mutable state (SR-CORE-07). Detached threads are acceptable for v1 (no cancellation; out of scope).

### Pipeline — `cpp/sr/Pipeline.{h,cpp}`, `cpp/sr/Errors.h`

- **Purpose**: Platform-agnostic super-resolution (algorithm above). No JSI or RN headers (AD-003).
- **Interfaces**:
  - `struct Request { std::vector<std::string> paths; int scale = 2; std::optional<std::string> outputPath; std::string defaultOutputDir; }`
  - `struct Result { std::string path; int width; int height; int framesUsed; }` (`path` is a `file://` URI)
  - `Result run(const Request&)`, which throws `SrError{ std::string code; std::string message; }`
  - `void validate(const Request&)` re-checks JS rules (frame count, scale, extension) → `E_INVALID_ARGUMENT`.
  - Internal, exposed in header for tests: `cv::Mat estimateTransform(const cv::Mat& refGray, const cv::Mat& gray, bool& ok)`.
- **Dependencies**: OpenCV `core`, `imgproc` (phaseCorrelate, warpAffine, cvtColor, resize), `video` (findTransformECC), and image read/write (`highgui` in opencv-mobile; `imgcodecs` in stock OpenCV).

### Paths — `cpp/sr/Paths.{h,cpp}`

- `std::string toFsPath(const std::string& pathOrUri)`: strips `file://`, percent-decodes, rejects other schemes → `E_INVALID_ARGUMENT`.
- `std::string toFileUri(const std::string& fsPath)`: builds the URI.
- `std::string lowerExt(const std::string&)`.
- `std::string uniqueOutputPath(const std::string& dir)`: `dir/superres-<epoch_ms>-<atomic counter>-<pid-or-random>.jpg`, unique across concurrent calls in one process (SR-CORE-07).

### TempDir — `cpp/platform/TempDir.h` + per-platform impl

- `std::string defaultOutputDir()`, resolved per call and not cached, so tests can override it.
  - **iOS** `ios/TempDir.mm`: `NSTemporaryDirectory()`.
  - **Android** `android/src/TempDir.cpp`: `getenv("TMPDIR")` if it is set and `access(W_OK)` passes. Otherwise call `ActivityThread.currentApplication().getCacheDir().getAbsolutePath()` through fbjni. If both fail → `E_WRITE_FAILED: no writable temp dir; pass options.outputPath`.
  - **Host tests** `cpp/tests/TempDirHost.cpp`: `std::filesystem::temp_directory_path()`.

### OpenCV distribution — `opencv.json`, `android/CMakeLists.txt`, `MeanNSuperResolution.podspec`

- **`opencv.json`** (repo root, shipped in npm `files`) is the single source for the pinned opencv-mobile version, plus `{url, sha256}` for the `android`, `apple` and host (`linux`, `windows`, `macos`) packages.
- **Android CMake**:
  - Read `opencv.json` with `string(JSON)` (CMake ≥ 3.19; RN's NDK CMake is newer).
  - Under `file(LOCK)`, because ABIs configure in parallel, use `file(DOWNLOAD … EXPECTED_HASH SHA256=…)` into `${CMAKE_CURRENT_LIST_DIR}/../.opencv/<version>/android` and extract.
  - `find_package(OpenCV REQUIRED PATHS <sdk>/native/jni NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)`, then link `${OpenCV_LIBS}` plus `fbjni` and `log`.
  - Env override `OPENCV_MOBILE_DIR` skips the download (offline/CI cache).
- **Podspec**:
  - At evaluation, Ruby reads `opencv.json`. If `.opencv/<version>/apple/opencv2.xcframework` is missing, it downloads the archive, verifies SHA-256 and extracts it. This runs at spec evaluation because `prepare_command` does not run for `:path` development pods.
  - Then `s.vendored_frameworks = ".opencv/<version>/apple/opencv2.xcframework"` and `s.frameworks = "Accelerate"`.
  - Source files add `cpp/sr/**`, `cpp/platform/**`, `ios/TempDir.mm`.
- **Git/npm**: `.opencv/` goes in `.gitignore` and is excluded from npm `files`, so consumers download at build time.

### Native host tests — `cpp/tests/`

- `CMakeLists.txt` builds a GoogleTest suite (via FetchContent) that links the pipeline sources and the host opencv-mobile package from `opencv.json`, using the same download logic, shared as `cmake/OpenCVMobile.cmake`.
- Fixtures are generated in code (seeded textured image, `cv::RNG(42)`) and written to a temp dir, so there are no binary fixtures in git.
- Commands:
  - `yarn test:native`: configure + build + `ctest` (`cmake -S cpp/tests -B build/native-tests`).
  - Single test: `ctest --test-dir build/native-tests -R <name>`.

### Example app — `example/src/App.tsx`

- Ships 5 sample frames: one photo plus 4 shifted crops, committed under `example/assets/frames/`. These are needed because the device has no other source of same-scene frames.
- Copies the frames to a local file path with an example-only filesystem dependency. The package is chosen in Tasks after checking RN 0.88 compatibility; the library itself gets no new JS dependency.
- Calls `superResolve` and shows the result in `<Image>`, along with `width`, `height`, `framesUsed` and elapsed time.
- Includes a "JS heartbeat" counter driven by `setInterval`, which visibly keeps ticking during processing (SR-CORE-05 manual/UAT evidence).

---

## Data Models

```ts
export type SuperResolveOptions = {
  scale?: 1 | 2 | 3 | 4;          // default 2 (public type narrower than spec type)
  outputPath?: string;            // abs path or file:// URI; .jpg/.jpeg/.png
};
export type SuperResolveResult = {
  path: string;                   // file:// URI
  width: number;                  // = scale * input width
  height: number;                 // = scale * input height
  framesUsed: number;             // 2..N (reference always included)
};
export const ErrorCodes = {
  INVALID_ARGUMENT: 'E_INVALID_ARGUMENT', DECODE_FAILED: 'E_DECODE_FAILED',
  SIZE_MISMATCH: 'E_SIZE_MISMATCH', ALIGNMENT_FAILED: 'E_ALIGNMENT_FAILED',
  WRITE_FAILED: 'E_WRITE_FAILED', INTERNAL: 'E_INTERNAL',
} as const;
```

Error message format: `"<CODE>: <detail>"`. Detail includes `index=<i>` for decode/size errors (SR-ERR-04/05).

---

## Error Handling Strategy

| Error Scenario | Handling | User Impact |
| -------------- | -------- | ----------- |
| Bad `paths`/`scale`/`outputPath` | JS `validateArgs` rejects; C++ `validate` re-checks | `E_INVALID_ARGUMENT: …`, no native work |
| Non-`file://` URI scheme | `toFsPath` throws | `E_INVALID_ARGUMENT: unsupported URI scheme` |
| Missing / undecodable file | `imread` empty → SrError | `E_DECODE_FAILED: index=i …` |
| Size mismatch | compare to reference | `E_SIZE_MISMATCH: index=i …` |
| Frame won't align | skipped; `framesUsed` lower | resolves; caller sees `framesUsed < N` |
| < 2 frames usable | SrError | `E_ALIGNMENT_FAILED` |
| `imwrite` false / throws, no temp dir | SrError | `E_WRITE_FAILED: …` |
| `cv::Exception`, `bad_alloc`, anything else on worker | catch-all on worker thread | `E_INTERNAL: …`, app keeps running |
| Web | wrapper rejects | existing native-only message |

---

## Risks & Concerns

| Concern | Location | Impact | Mitigation |
| ------- | -------- | ------ | ---------- |
| opencv-mobile module set unverified: needs `video` (ECC) and JPEG/PNG write via its highgui; EXIF-orientation behavior of its stb-based `imread` unknown | `opencv.json`, `cpp/sr/Pipeline.cpp` | Alignment or encoding unavailable; orientation inconsistency | **T1 spike**: download the pinned package, confirm `findTransformECC`, `phaseCorrelate`, `imread`/`imwrite` symbols and headers. If `video` is missing, fall back to phaseCorrelate-only translation (spec still satisfied: ACs use translations) and record a `SPEC_DEVIATION` note in design. If EXIF isn't applied, document "orientation applied consistently = not applied" (all frames treated alike) and update the spec edge case |
| Android `find_package` inside app `appmodules` CMake: root-path mode ONLY can hide OpenCV config (reported on OpenCV forum) | `android/CMakeLists.txt` | Android build fails | `NO_CMAKE_FIND_ROOT_PATH` + explicit `PATHS`; fallback to importing `.a` libs manually as `IMPORTED` targets; proven by CI `build-android` |
| Android temp dir from pure C++: stock TMPDIR behavior not confirmed; `ActivityThread.currentApplication` is a hidden (greylisted) API | `android/src/TempDir.cpp` | Default output path fails on some devices | Try TMPDIR first, JNI second, clear `E_WRITE_FAILED` hint third; device-verify in example on API 24+ and latest; README recommends passing `outputPath` |
| iOS runtime can't be verified on owner's Windows machine | `ios/`, podspec | SR-CORE-06 iOS evidence limited to CI build | CI `build-ios` proves compile/link; iOS runtime UAT needs a Mac/device. Flag as open evidence gap in validation.md if unavailable |
| Download at build/pod-install time: network dependency, supply chain | `cmake/OpenCVMobile.cmake`, podspec | Offline builds fail; tampered archive | Pin version + SHA-256 in `opencv.json`; fail hard on hash mismatch; `OPENCV_MOBILE_DIR` override for offline; CI caches `.opencv/` |
| Determinism: OpenCV may parallelize reductions (ECC) → float-order differences | `Pipeline.cpp` | Byte-identical-output assumption broken | Host test runs pipeline twice and asserts byte equality. If it is flaky, do not use `cv::setNumThreads(1)` (process-global, slows the host app); instead relax the assumption to ±1 per channel and log a `SPEC_DEVIATION` |
| Memory: 30 × 12 MP × scale 4 | `Pipeline.cpp` | OOM → crash | Streamed processing: decode → align → warp → accumulate one frame at a time (only R gray, accumulator, current frame live); `bad_alloc` → `E_INTERNAL` |
| Detached worker threads outlive RN reload | `MeanNSuperResolutionImpl.cpp` | Resolve on dead runtime | `AsyncPromise` holds `LongLivedObject` + `CallInvoker` weak handling (RN-owned); worker never touches module `this`. Accept for v1 |
| Existing tests are only `it.todo` | `src/__tests__/index.test.tsx` | No regression net | Replaced by real Jest suite + native gtest suite |
| `packageManager` removed + `pnpm-lock.yaml` in working tree while repo is Yarn 4 | `package.json` | Wrong installs / CI drift | Out of feature scope; leave untouched, flagged to owner |

---

## Tech Decisions

| Decision | Choice | Rationale |
| -------- | ------ | --------- |
| OpenCV distribution | opencv-mobile prebuilt, pinned in `opencv.json`, downloaded at build | User choice; small; same version on all platforms + host (AD-002) |
| Testable core | JSI-free `cpp/sr/` + host gtest | Only way to get automated evidence for ACs on native pipeline (AD-003) |
| Alignment | phaseCorrelate init + ECC Euclidean on ≤1024-px pyramid level | Robust to the ≤5 % shifts in spec, cheap on large frames |
| Upscale | Single `warpAffine(S·A)` with INTER_CUBIC | One interpolation per frame; reference uses the same op → identity AC holds exactly |
| Async | `AsyncPromise` + detached `std::thread` per call | RN-provided thread-safe settlement; no pool needed for v1 |
| Optional options | JS passes `{}`; native arg required | Avoids optional-object codegen edge case |
| Validation | Duplicated in JS (Jest-tested) and C++ (gtest) | JS gives fast tests; C++ guards direct callers |

---

## Spike Results

T1 spike, 2026-11-11. Evidence was collected from the downloaded archives (headers, `llvm-nm` from NDK 30.0.14904198) and from opencv-mobile's `highgui/src/highgui.cpp` at tag `v38`.

### Version pinned

- Release **v38** (published 2026-11-10), the latest. OpenCV **4.14.0**, which is the newest 4.x the release ships (it also ships 5.x; not used).
- `opencv.json` packages: `android`, `apple`, `windows` (`windows-vs2022`), `linux` (`ubuntu-2404`, matching CI `ubuntu-latest`), `macos`.
- Each `sha256` came from `sha256sum` over the downloaded file, and each one equals the GitHub asset `digest` field (`gh api repos/nihui/opencv-mobile/releases/tags/v38`):

| key | sha256 (computed) |
| --- | --- |
| android | `2169b7cf49dcd8c314ea99efaa8106ce5de282ab01835cf34df08ed133cbbc9f` |
| apple | `921d6dcff419f55ff0073c7fd092aa567877ff96715d7145772f2efb8100ab25` |
| windows | `607c71d64f7b65a4a14465e02429f7fecbf57a331c053c4d46184e0852e12c6c` |
| linux | `62409b7834840e3c6f447f25abf8ca9e05eae121e71abe458b1200f22ff97bd5` |
| macos | `df7b762956f94362f2747413ae29641151f41b850e5d7965fabbacb6fb4e2d59` |

### Modules

- `opencv2/opencv_modules.hpp` defines: `HAVE_OPENCV_CORE`, `FEATURES2D`, `HIGHGUI`, `IMGPROC`, `PHOTO`, `VIDEO`.
- **`video` is present**, so the design runs as written: phaseCorrelate init followed by ECC. No phaseCorrelate-only fallback is needed.
- There is **no `imgcodecs` module**. `imread`/`imwrite`/`imencode`/`imdecode` are declared in `opencv2/highgui/highgui.hpp`, so include `<opencv2/highgui.hpp>`.

| API | Header evidence | Library symbol (`llvm-nm -C --defined-only`) |
| --- | --- | --- |
| `findTransformECC` | `video/tracking.hpp:365` | `T cv::findTransformECC(..., cv::TermCriteria, cv::_InputArray const&, int)` in `libopencv_video.a` (ubuntu-2404 and android arm64-v8a) |
| `phaseCorrelate` | `imgproc.hpp:3053` | `T cv::phaseCorrelate(...)` in `libopencv_imgproc.a` |
| `warpAffine` | `imgproc.hpp:2481` | `T cv::warpAffine(...)` in `libopencv_imgproc.a` |
| `imread` / `imwrite` / `imencode` / `imdecode` | `highgui/highgui.hpp:62-68` | `T cv::imread`, `T cv::imwrite`, `T cv::imencode`, `T cv::imdecode` in `libopencv_highgui.a` |

### Image codecs (highgui.cpp @ v38)

- stb_image is built with `STBI_ONLY_JPEG`, `STBI_ONLY_PNG`, `STBI_ONLY_BMP` and `STBI_ONLY_PNM` (lines 34-37). Decode therefore supports JPEG, PNG, BMP and PNM.
- `imread(..., IMREAD_COLOR)` asks stb for 3 channels (lines 133-135), so grayscale and alpha inputs come back as 8-bit BGR.
- `imwrite` and `imencode` support JPEG (`stbi_write_jpg`; the `IMWRITE_JPEG_QUALITY` param is honored and defaults to 95; lines 537-549) and PNG (`stbi_write_png`, lines 550-553).
- **Hazard:** extension matching is an exact string compare against `.jpg`, `.jpeg`, `.JPG`, `.JPEG`, `.png` and `.PNG`. A mixed-case extension such as `.Jpg` makes `imwrite` return false.
  - **Decision for the pipeline:** encode with `cv::imencode(lowerExt(outputPath), ...)` and write the bytes with `std::ofstream`. Any open/write failure maps to `E_WRITE_FAILED`. This keeps the spec's case-insensitive extension rule (SR-ERR-03, SR-CFG-03).
- `imread` opens files with narrow `fopen` and prints to `stderr` on failure. UTF-8 paths work on Android and iOS.

### EXIF orientation

- **Applied.** After decoding, `imread` parses EXIF and calls `rotate_by_orientation` for orientation 1-8 (`highgui.cpp` lines 284-296; `imdecode` does the same at lines 707-718).
- The spec edge case ("orientation applied consistently to all frames") holds as written, so no spec amendment is needed.

### Archive layout

- **Android:** `opencv-mobile-4.14.0-android/sdk/native/jni/OpenCVConfig.cmake`. It dispatches by ABI to `jni/abi-<ABI>/OpenCVConfig.cmake` via `ANDROID_NDK_ABI_NAME`. Static libs live in `sdk/native/staticlibs/<ABI>/` for arm64-v8a, armeabi-v7a, x86 and x86_64; arm64 also gets `3rdparty/libs/arm64-v8a/libkleidicv*.a`.
- **Apple:** `opencv2.xcframework/` at the archive root, with slices `ios-arm64` and `ios-arm64_x86_64-simulator` (plus macOS, catalyst, tvOS, watchOS and visionOS). Headers are at `opencv2.framework/Versions/A/Headers/` (`video/tracking.hpp` is present).
- **Windows (vs2022):** `opencv-mobile-4.14.0-windows-vs2022/x64/OpenCVConfig.cmake`. It dispatches to `x64/vc17/staticlib/`, with static libs named `opencv_<module>4140.lib`.
- **Linux (ubuntu-2404):** `opencv-mobile-4.14.0-ubuntu-2404/lib/cmake/opencv4/OpenCVConfig.cmake`, with static `lib/libopencv_*.a`.
- **macOS:** `opencv2.framework/` at the archive root.

### Addendum (T6): C++ exceptions are disabled in opencv-mobile

- **Finding:** opencv-mobile builds OpenCV with C++ exceptions and RTTI disabled. Evidence:
  - README v38: "C++ RTTI and exceptions are disabled for minimal build on mobile platforms".
  - `.github/workflows/release.yml:371`: `-DANDROID_CPP_FEATURES="no-rtti no-exceptions"`.
  - Lines 575-576 and others: `-fno-rtti -fno-exceptions`.
  - Reproduced on the Windows host package: a `findTransformECC` call on an unrelated-noise frame (`CV_Error StsNoConv`) ended the test process with exit code 3 (`abort`). It did not throw `cv::Exception`.
- **Consequence:** any `CV_Error`/`CV_Assert` inside OpenCV **terminates the process**. This includes allocation failure in `cv::Mat::create`, which goes through `CV_Error(OutOfMemory)`. `try/catch (cv::Exception&)` can never fire.
- **Decision (T6), SPEC_DEVIATION from step 2:** alignment is **translation-only**:
  1. `phaseCorrelate` with a Hanning window, on the ≤1024-px level, with the shift scaled by `1/d`.
  2. The frame is accepted when the normalized cross-correlation of `ref(x)` vs `frame(x + shift)` over the overlap is ≥ 0.85. The overlap must be ≥ 25 % of the frame, and neither side may be flat. Otherwise the frame is skipped (SR-ERR-06).
  3. Bit-identical frames are accepted with `A = I` directly.
  - This is the fallback the risk table names for a missing `video` module. All SR-CORE-04 ACs use translations, so the spec is still met. Rotation between frames is not compensated.
- **Hazard:** `cv::phaseCorrelate(src1, src2, window)` multiplies its inputs by the window **in place** when they already have DFT-optimal sizes. Callers must pass copies; without them the follow-up correlation (and, earlier, ECC) saw windowed images.
- **Impact on later tasks (T8, SR-ERR-08, large-frame edge case):** `E_INTERNAL` can only come from exceptions our own code throws (`std::bad_alloc` from std containers, `SrError`, `std::exception`). OOM or invalid arguments inside OpenCV abort instead. Validating inputs before calling OpenCV is the only way to avoid aborts; a pre-flight memory budget check would be needed to honor "reject with `E_INTERNAL` on OOM". That needs an owner decision.
