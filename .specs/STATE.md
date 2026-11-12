# STATE

## Decisions

### AD-001
- **Decision**: All native logic (including future OpenCV super-resolution code) lives in shared C++ under `cpp/`, exposed as a pure C++ TurboModule; no Kotlin/Swift/Obj-C module code.
- **Reason**: Project scaffolded as `turbo-module` + `cpp` (create-react-native-library 0.64.0); one implementation compiles for both iOS (podspec + `ios/OnLoad.mm`) and Android (`android/CMakeLists.txt` via `react-native.config.js`).
- **Trade-off**: New Architecture only; platform-specific APIs (e.g. image decoding from Photos/content URIs) need C++ bridging or must be handled in JS before calling the module.
- **Scope**: All features; `cpp/`, `ios/`, `android/`, `src/Native*.ts`.
- **Date**: 2026-11-11
- **Status**: active

### AD-002
- **Decision**: OpenCV comes from opencv-mobile prebuilt packages, version + per-platform URL + SHA-256 pinned in root `opencv.json`, downloaded at build time (Android CMake, podspec evaluation, host tests) into gitignored `.opencv/`; never committed or shipped in npm tarball.
- **Reason**: Smallest binaries, one OpenCV version across Android, iOS and host tests; consumer adds no Gradle/Pod deps.
- **Trade-off**: Build needs network (or `OPENCV_MOBILE_DIR` override); module set limited to what opencv-mobile ships.
- **Scope**: All native features using OpenCV.
- **Date**: 2026-11-11
- **Status**: active

### AD-003
- **Decision**: Image-processing logic lives in JSI-free C++ under `cpp/sr/` and is covered by host GoogleTest suite in `cpp/tests/` (`yarn test:native`); TurboModule impl is a thin async adapter. Platform specifics go behind `cpp/platform/` interfaces.
- **Reason**: Only way to get automated, CI-runnable evidence for native behavior; RN layer stays thin.
- **Trade-off**: Extra CMake project and CI job; platform glue (temp dir, threading) still needs device verification.
- **Scope**: All native features.
- **Date**: 2026-11-11
- **Status**: active

### AD-004
- **Decision**: Native code must never rely on catching `cv::Exception`: opencv-mobile is built with `-fno-exceptions`, so OpenCV assertion failures abort the process. Every OpenCV call is preceded by input guards (non-empty, expected type, positive sizes, finite matrices) and allocation-heavy steps by a memory pre-flight budget.
- **Reason**: Owner chose to keep opencv-mobile (AD-002) rather than build/ship OpenCV with exceptions (2026-11-11).
- **Trade-off**: No ECC (`findTransformECC` asserts internally) — alignment is translation-only; residual crash risk for unforeseen OpenCV asserts.
- **Scope**: All native code calling OpenCV.
- **Date**: 2026-11-11
- **Status**: active

## Handoff

- **Feature**: `.specs/features/opencv-super-resolution`
- **Phase / Task**: Execute — Batch B (T7–T13 incl. T7b)
- **Completed**: Specify, Design (approved), Tasks (approved, T0–T13)
- **In-progress** (file:line): Batch A done (13477a1..80b1a4c, 35 native tests); Batch B dispatched
- **Next step**: On Batch B summary → owner decides on GitHub remote/push for CI evidence (T10/T11, iOS) → Verifier.
- **Blockers**: none
- **Uncommitted files**: none (.tlc/, .codegraph/ are untracked tooling)
- **Branch**: feat/opencv-super-resolution
