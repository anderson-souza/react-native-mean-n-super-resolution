# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

`react-native-mean-n-super-resolution`: a React Native library exposing `superResolve(paths, options?)`, which aligns 5, 15 or 30 same-size local image files to `paths[0]`, upscales them (scale 1–4, default 2) and averages them with OpenCV, writing the result to a file. Scaffolded with `create-react-native-library` (type `turbo-module`, language `cpp`). README documents the public API and `E_*` error codes.

## Commands

Monorepo using Yarn 4 workspaces (`.yarn/releases/yarn-4.18.1.cjs`, `nodeLinker: node-modules`); root is the library, `example/` is the test app. Node version in `.nvmrc`.

```sh
yarn                      # install all workspaces
yarn typecheck            # tsc
yarn lint                 # eslint (add --fix for prettier formatting)
yarn test                 # jest (preset @react-native/jest-preset)
yarn test path/to/file    # single test file; add -t "name" for a single test
yarn test:native          # host C++ GoogleTest suite (scripts/test-native.mjs: finds cmake on PATH or ANDROID_HOME/cmake, Ninja, MSVC via vswhere on Windows)
yarn test:native -R Align # extra args go to ctest; run build/native-tests/sr_tests(.exe) directly to see test stdout
yarn prepare              # bob build -> lib/ (ESM module + .d.ts)
yarn clean                # remove lib/ and native build dirs
yarn example start        # Metro for example app
yarn example android      # build + run example on Android
yarn example ios          # build + run example on iOS (run `bundle exec pod install` in example/ios first)
yarn turbo run build:android   # CI-style cached example build (same for build:ios)
yarn release              # release-it: bump, changelog, tag, npm publish, GitHub release
```

Lefthook pre-commit runs eslint on staged files and `tsc`; commit-msg runs commitlint (conventional commits: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`). CI (`.github/workflows/ci.yml`) runs lint, typecheck, Jest with coverage, `test-native`, `yarn prepare`, and example builds for Android (NDK r30) and iOS.

## Architecture

This is a **pure C++ TurboModule** (New Architecture only, no Kotlin/Obj-C module code). One method flows through these layers, and adding or changing an API means touching all of them:

1. **Spec** — `src/NativeMeanNSuperResolution.ts`. The `Spec` interface is the codegen source (`codegenConfig` in `package.json`, name `MeanNSuperResolutionSpec`, `jsSrcsDir: src`). Codegen generates `MeanNSuperResolutionSpecJSI.h` containing the base class `NativeMeanNSuperResolutionCxxSpec<T>`.
2. **C++ TurboModule adapter** — `cpp/MeanNSuperResolutionImpl.{h,cpp}`. Subclasses the generated spec; each spec method takes `jsi::Runtime&` first and C++ types matching the TS signature (codegen errors surface as compile errors here, not in TS). `superResolve` returns `AsyncPromise<…>` and runs the work on a detached worker thread (must not capture `this`); errors map to `"E_CODE: message"` via `cpp/sr/ErrorMapping`.
   - **Pipeline** — `cpp/sr/` (Validate, Paths, Align, Pipeline, Errors) is JSI-free and covered by host tests in `cpp/tests/` (AD-003). Platform hooks sit behind `cpp/platform/TempDir.h` (impls: `ios/TempDir.mm`, `android/src/TempDir.cpp`, `cpp/tests/TempDirHost.cpp`).
3. **Platform registration** (shared C++ is compiled by both platforms):
   - iOS: `ios/OnLoad.mm` registers the module in `+load` via `registerCxxModuleToGlobalModuleMap`. `MeanNSuperResolution.podspec` compiles `ios/**` and `cpp/**`.
   - Android: `react-native.config.js` (`cxxModuleCMakeListsPath`, `cxxModuleHeaderName`) tells RN autolinking to build `android/CMakeLists.txt` and instantiate `MeanNSuperResolutionImpl`. CMakeLists links `jsi`, `reactnative`, `react_codegen_MeanNSuperResolutionSpec`. New `.cpp` files must be added to `add_library` here (the podspec picks them up via glob).
4. **JS wrapper** — `src/superResolve.native.ts` validates args (`src/validate.ts`, rejects `E_INVALID_ARGUMENT` before calling native) and calls the TurboModule; `src/superResolve.ts` is the web fallback that rejects. `src/index.tsx` re-exports the public API. Validation rules are duplicated in C++ (`cpp/sr/Validate.cpp`) — change both.

### OpenCV

- opencv-mobile prebuilt packages, version + per-platform URL + SHA-256 pinned in root `opencv.json` (AD-002). Downloaded at build time into gitignored `.opencv/<version>/<platform>/` by `cmake/OpenCVMobile.cmake` (Android CMake, host tests) and by Ruby in the podspec (iOS, runs at spec evaluation). Env `OPENCV_MOBILE_DIR` skips download/hash checks.
- Built with `-fno-exceptions`: OpenCV errors **abort the process**; `catch (cv::Exception)` never fires. Guard every input before calling OpenCV and keep the 1 GiB memory pre-flight (AD-004). This is also why alignment is translation-only `phaseCorrelate`, not ECC.
- Image I/O comes from `<opencv2/highgui.hpp>` (no imgcodecs). Encode via `imencode` with a lower-cased extension (opencv-mobile rejects mixed case); `phaseCorrelate` can overwrite its inputs, so pass copies.
- Android needs NDK r30+ (AD-005); `android/CMakeLists.txt` fails fast below it. The example works around an RN 0.88 Windows bug (backslash paths in `Android-autolinking.cmake`) in `example/android/app/build.gradle`.

### Specs

Feature work follows spec-driven flow in `.specs/`: `STATE.md` (decisions log `AD-NNN` + handoff snapshot), `features/<name>/` (spec/design/tasks/validation), `LESSONS.md`/`lessons.json` (machine-owned via the tlc-spec-driven `lessons.py` script — do not hand-edit). Read `STATE.md` Decisions before designing a feature.

### Packaging / resolution

- `package.json` `exports` has a custom condition `react-native-mean-n-super-resolution-source` pointing at `src/index.tsx`. The example app's Metro config and Jest's `customExportConditions` use it, so the example and tests consume library **source** directly — JS changes need no rebuild; native changes require rebuilding the example app.
- Published output is `lib/` (built by `react-native-builder-bob`, typings via `tsconfig.build.json`). Jest ignores `lib/` and `example/node_modules`.
