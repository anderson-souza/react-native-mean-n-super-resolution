# opencv-super-resolution Tasks

## Execution Protocol (MANDATORY -- do not skip)

Implement these tasks with the `tlc-spec-driven` skill: **activate it by name and follow its Execute flow and Critical Rules.** Do not search for skill files by filesystem path. The skill is the source of truth for the full flow (per-task cycle, sub-agent delegation, adequacy review, Verifier, discrimination sensor).

**If the skill cannot be activated, STOP and tell the user — do not proceed without it.**

---

**Design**: `.specs/features/opencv-super-resolution/design.md`
**Status**: Approved (2026-11-11) — tools: context7, WebSearch/fetch, skills `react-native-expert`, `codenavi`; execution: sub-agent per batch

---

## Test Coverage Matrix

> Generated from codebase, project guidelines, and spec — confirm before Execute. Guidelines found: `CONTRIBUTING.md` ("add tests for your change if possible", `yarn test`), `CLAUDE.md`, `package.json` `jest` (no coverage threshold), `.github/workflows/ci.yml` (`yarn test --maxWorkers=2 --coverage`). Existing tests: only `src/__tests__/index.test.tsx` (`it.todo`) — no floor; strong defaults applied. Test types fixed by approved design (AD-003).

| Code Layer | Required Test Type | Coverage Expectation | Location Pattern | Run Command |
| ---------- | ------------------ | -------------------- | ---------------- | ----------- |
| C++ pipeline / paths / validation / error mapping (`cpp/sr/**`) | unit (host GoogleTest) | All branches; 1:1 to spec ACs SR-CORE-02..04, 07, SR-ERR-01..08, SR-CFG-01..03; every spec edge case | `cpp/tests/*_test.cpp` | `yarn test:native` |
| JS API (`src/**`) | unit (Jest, native module mocked) | 1:1 to SR-ERR-01..03, 09, SR-CORE-01 (pass-through + defaults), SR-DOC-01 exports | `src/__tests__/*.test.ts` | `yarn test` |
| TurboModule adapter (`cpp/MeanNSuperResolutionImpl.*`) | build + device UAT | Compiles/links on Android (local or CI) and iOS (CI); async + no-crash verified in example app | — | `yarn turbo run build:android`; CI `build-ios` |
| Platform glue (`android/**`, `ios/**`, podspec, CMake) | build + device UAT | Builds; default temp dir works on device | — | same as above |
| Example app (`example/**`) | manual UAT | SR-CORE-05/06 evidence (heartbeat ticks, result shown, dims printed) | — | `yarn example android` |
| Config / docs (`opencv.json`, `turbo.json`, CI yml, README) | none | build gate only | — | build gate |

## Gate Check Commands

> Generated from codebase — confirmed. Package manager: **Yarn 4** (owner decision 2026-11-11; restored in T0).

| Gate Level | When to Use | Command |
| ---------- | ----------- | ------- |
| Quick (JS) | Tasks touching `src/**` only | `yarn test` |
| Quick (native) | Tasks touching `cpp/sr/**`, `cpp/tests/**` | `yarn test:native` |
| Full | Tasks touching TurboModule / platform glue | `yarn test && yarn test:native && yarn turbo run build:android` (iOS: CI `build-ios`) |
| Build | End of each phase | `yarn lint && yarn typecheck && yarn test && yarn test:native && yarn prepare` |

---

## Execution Plan

### Phase 1: Foundation — tooling, OpenCV + native test harness

```
T0 → T1 → T2
```

### Phase 2: Core C++ pipeline (host-tested)

```
T3 → T4 → T5 → T6
```

### Phase 3: React Native integration

```
T7 → T7b → T8 → T9 → T10 → T11
```

### Phase 4: Example + docs

```
T12 → T13
```

---

## Task Breakdown

### T0: Restore Yarn 4 tooling

**What**: Restore `"packageManager": "yarn@4.18.1"` in `package.json`, delete untracked `pnpm-lock.yaml`, remove pnpm-created `node_modules`, run `yarn install` (creates `yarn.lock`).
**Where**: `package.json`, `pnpm-lock.yaml` (delete), `yarn.lock` (new)
**Depends on**: None
**Requirement**: tooling prerequisite (owner decision)

**Done when**:
- [ ] `yarn --version` = 4.18.1; `yarn install` succeeds; `yarn.lock` committed
- [ ] `yarn lint`, `yarn typecheck`, `yarn test` run (pass or todo-only) — output cited

**Tests**: none (tooling) · **Gate**: build (lint/typecheck/test)
**Commit**: `chore: restore yarn 4 package manager`
**Status**: ✅ Done (13477a1)

---

### T1: opencv-mobile spike + `opencv.json`

**What**: Choose latest opencv-mobile **4.x** release (C++20 compatible, not 5.0 unless 4.x lacks a required module), write `opencv.json` with `version` and `{url, sha256}` for `android`, `apple`, `windows`, `linux`, `macos`; download each archive once and verify module availability.
**Where**: `opencv.json` (new); findings appended to `design.md` → `## Spike Results`
**Depends on**: T0
**Reuses**: —
**Requirement**: enables SR-CORE-04, SR-CFG-03; resolves design risk #1

**Tools**: MCP: NONE (WebSearch / release page fetch) · Skill: NONE

**Done when**:
- [ ] `opencv.json` lists real release URLs; every `sha256` computed from the downloaded file (command output cited)
- [ ] Spike Results state, with header/library evidence (grep of headers / `nm` or lib listing): presence of `findTransformECC` (video), `phaseCorrelate`, `warpAffine`, `imread`/`imwrite` + JPEG and PNG support, and archive layout (Android `sdk/native/jni/OpenCVConfig.cmake` path; Apple `opencv2.xcframework` path)
- [ ] EXIF orientation behavior of `imread` documented (applied / not applied); if not applied → spec edge case amended
- [ ] If `video` missing → design fallback (phaseCorrelate-only) recorded as decision

**Tests**: none (config) · **Gate**: evidence in Spike Results
**Commit**: `chore(opencv): pin opencv-mobile version and checksums`
**Status**: ✅ Done (83dd1a6)

---

### T2: OpenCV fetch helper + host GoogleTest harness

**What**: `cmake/OpenCVMobile.cmake` (reads `opencv.json`, `file(LOCK)`, `file(DOWNLOAD … EXPECTED_HASH)`, extract to `.opencv/<version>/<platform>`, honors `OPENCV_MOBILE_DIR`, exposes `find_package(OpenCV)`); `cpp/tests/CMakeLists.txt` with GoogleTest via FetchContent; one smoke test creating `cv::Mat` + `cv::imencode` to PNG; `test:native` script in `package.json`; `.gitignore` adds `.opencv/`, `build/`.
**Where**: `cmake/OpenCVMobile.cmake`, `cpp/tests/CMakeLists.txt`, `cpp/tests/smoke_test.cpp`, `package.json`, `.gitignore`
**Depends on**: T1
**Reuses**: `opencv.json`
**Requirement**: AD-003 harness

**Tools**: MCP: `context7` (CMake FetchContent/GoogleTest) · Skill: NONE

**Done when**:
- [ ] `yarn test:native` on clean checkout downloads OpenCV, builds, runs: 1 test passes
- [ ] Second run uses cache (no re-download, log shows skip)
- [ ] Tampered sha256 → configure fails with hash error (shown once, then reverted)
- [ ] `git status` shows no `.opencv/` or `build/` files

**Tests**: unit (smoke) · **Gate**: quick (native)
**Commit**: `build(native): add opencv-mobile fetch helper and host test harness`
**Status**: ✅ Done (ba7dddf)

---

### T3: Error codes + path utilities

**What**: `cpp/sr/Errors.h` (`SrError{code,message}`, code constants); `cpp/sr/Paths.{h,cpp}`: `toFsPath`, `toFileUri`, `lowerExt`, `uniqueOutputPath`.
**Where**: `cpp/sr/Errors.h`, `cpp/sr/Paths.h`, `cpp/sr/Paths.cpp`, `cpp/tests/paths_test.cpp`
**Depends on**: T2
**Requirement**: SR-ERR-01 (scheme), SR-CORE-07 (uniqueness), edge case `%20`

**Done when**:
- [ ] Tests: `file:///a/b.jpg` → `/a/b.jpg`; `file:///a/my%20img.jpg` → `/a/my img.jpg`; plain abs path unchanged; `content://x`, `ph://x`, `http://x` → `E_INVALID_ARGUMENT`; `lowerExt("X.JPEG")` = `.jpeg`; `toFileUri` round-trips with `toFsPath`; 1000 `uniqueOutputPath` calls across 8 threads → 1000 distinct paths all ending `.jpg` in given dir
- [ ] `yarn test:native` passes; test count = previous + new (no deletions)

**Tests**: unit · **Gate**: quick (native)
**Commit**: `feat(native): add error codes and path utilities`
**Status**: ✅ Done (304567e)

---

### T4: Request validation + host TempDir

**What**: `cpp/sr/Pipeline.h` with `Request`/`Result` structs and `validate(const Request&)`; `cpp/platform/TempDir.h` interface + `cpp/tests/TempDirHost.cpp`.
**Where**: `cpp/sr/Pipeline.h`, `cpp/sr/Validate.cpp`, `cpp/platform/TempDir.h`, `cpp/tests/TempDirHost.cpp`, `cpp/tests/validate_test.cpp`
**Depends on**: T3
**Requirement**: SR-ERR-01, SR-ERR-02, SR-ERR-03 (native re-check)

**Done when**:
- [ ] Tests: counts 5, 15, 30 accepted; 0, 1, 4, 6, 14, 16, 29, 31 → `E_INVALID_ARGUMENT`; empty-string path → `E_INVALID_ARGUMENT`; scale 1–4 accepted, 0, 5, -1 → reject; outputPath `.jpg/.JPG/.jpeg/.png` accepted, `.bmp`, no extension, empty → reject
- [ ] `yarn test:native` passes; count = previous + new

**Tests**: unit · **Gate**: quick (native)
**Commit**: `feat(native): validate super-resolution requests`
**Status**: ✅ Done (b79d2c7)

---

### T5: Pipeline run — decode, warp/upscale, mean, encode (identity alignment)

**What**: `sr::run(Request)` implementing design steps 1, 3, 4, 5 with `A_i = I` for all frames (alignment added in T6), streamed one frame at a time; result `file://` URI; default output via `uniqueOutputPath(defaultOutputDir)`.
**Where**: `cpp/sr/Pipeline.cpp`, `cpp/tests/pipeline_test.cpp`, `cpp/tests/fixtures.{h,cpp}` (seeded synthetic images)
**Depends on**: T4
**Requirement**: SR-CORE-01, 02, 03, 07; SR-CFG-01, 02, 03; SR-ERR-04, 05, 07; edge cases (duplicate paths, gray/alpha, scale 1); determinism assumption

**Done when**:
- [ ] Tests: 5 identical frames, default scale → file exists, dims = 2×, `framesUsed` = 5, pixels within ±1 of `warpAffine(ref, S, INTER_CUBIC)`; scale 1/3/4 dims; `.png` output decodes as PNG (magic bytes), `.jpg` as JPEG; existing outputPath overwritten; missing file at index 2 → `E_DECODE_FAILED` with `index=2`; text file → `E_DECODE_FAILED`; one frame of different size at index 3 → `E_SIZE_MISMATCH` `index=3`; output dir missing → `E_WRITE_FAILED`; grayscale + RGBA inputs → 3-channel output; same path listed 5× valid; two runs → byte-identical files; two concurrent runs without outputPath → different paths, both exist
- [ ] `yarn test:native` passes; count = previous + new

**Tests**: unit · **Gate**: quick (native)
**Commit**: `feat(native): implement mean-of-N upscale pipeline`
**Status**: ✅ Done (64f11db)

---

### T6: Frame alignment + skip policy

**What**: `estimateTransform` (≤1024-px level, phaseCorrelate init, ECC Euclidean refine, correlation ≥ 0.85) wired into `run`; failing frames skipped; `framesUsed < 2` → `E_ALIGNMENT_FAILED`.
**Where**: `cpp/sr/Align.{h,cpp}`, `cpp/sr/Pipeline.cpp`, `cpp/tests/align_test.cpp`
**Depends on**: T5
**Requirement**: SR-CORE-04, SR-ERR-06

**Done when**:
- [ ] Tests: 5 frames = reference shifted by known integer offsets (≤5 % of size) → PSNR in common region vs upscaled reference ≥ 30 dB AND strictly greater than identity-alignment PSNR on same frames; estimated translation within ±0.5 px of ground truth; 1 of 5 frames replaced by unrelated noise → resolves, `framesUsed` = 4; 4 of 5 frames unrelated noise → `E_ALIGNMENT_FAILED`; T5 identity test still passes
- [ ] `yarn test:native` passes; count = previous + new

**Tests**: unit · **Gate**: quick (native) + Build gate (end of Phase 2)
**Commit**: `feat(native): align frames before averaging`
**Status**: ✅ Done (39026ac) — SPEC_DEVIATION: translation-only phaseCorrelate + NCC gate (ECC aborts: opencv-mobile has no C++ exceptions; see design.md Spike Results addendum)

---

### T7: JS API + spec, remove `multiply` from JS

**What**: Spec `superResolve(paths, options): Promise<Result>`; `src/types.ts`, `src/errors.ts` (`ErrorCodes`), `src/validate.ts` (`validateArgs`), `src/superResolve.native.ts`, `src/superResolve.ts` (web), `src/index.tsx` exports; delete `src/multiply*.tsx`, replace `src/__tests__/index.test.tsx`.
**Where**: `src/**`
**Depends on**: T6
**Requirement**: SR-ERR-01, 02, 03, 09; SR-CORE-01 (JS pass-through); SR-DOC-01

**Done when**:
- [ ] Jest (native module mocked): invalid paths / counts (4, 6, 0, non-array, non-string, empty string), bad scale (0, 5, 2.5, '2'), bad extension → reject `E_INVALID_ARGUMENT` and mock **not called**; valid call with no options → mock called with `(paths, {})`; with options → passed through; resolves with mock's result unchanged; native rejection propagates unchanged; web module rejects with native-only message; `index` exports `superResolve`, `ErrorCodes`, and no `multiply`
- [ ] `yarn test`, `yarn typecheck`, `yarn lint` pass; `yarn prepare` emits `lib/typescript/src/index.d.ts` with `superResolve`, `SuperResolveOptions`, `SuperResolveResult`

**Tests**: unit · **Gate**: quick (JS) + typecheck/lint
**Commit**: `feat(api): add superResolve JS API and remove multiply`
**Status**: ✅ Done (cc82493)

---

### T7b: Pre-flight guards (no-exceptions OpenCV)

**What**: In `sr::run`, before any OpenCV call that can assert: guard every input (non-empty Mat, type `CV_8UC3`, sizes > 0, scale in range, warp matrix finite) and, after decoding the reference frame, compute peak-memory estimate `s²·w·h·(3·4 + 3) + frame + gray working set`; > 1 GiB (`kMemoryBudgetBytes`) → `E_INTERNAL: estimated memory <N> MiB exceeds budget`. Remove dead `catch(cv::Exception)` in Pipeline.cpp.
**Where**: `cpp/sr/Pipeline.cpp`, `cpp/sr/Pipeline.h` (budget constant + `estimatePeakBytes` exposed for tests), `cpp/tests/pipeline_test.cpp`
**Depends on**: T7
**Requirement**: SR-ERR-08 (amended), large-frame edge case

**Done when**:
- [ ] Tests: `estimatePeakBytes` exact value for a known w/h/s; request whose reference decodes to a size above budget at scale 4 (generate e.g. 4096×4096 PNG; s=4) → `E_INTERNAL` with `exceeds budget`, no output file written; same image at s=1 → resolves
- [ ] `yarn test:native` passes; count = previous + new

**Tests**: unit · **Gate**: quick (native)
**Commit**: `feat(native): add pre-flight memory and input guards`
**Status**: ✅ Done (21cfa38)

---

### T8: Error mapping + TurboModule async adapter

**What**: `cpp/sr/ErrorMapping.{h,cpp}` — `std::string toRejectMessage(std::exception_ptr)` (SrError → `CODE: msg`; `cv::Exception`/`bad_alloc`/`std::exception`/unknown → `E_INTERNAL: …`); `MeanNSuperResolutionImpl::superResolve` using codegen structs + `AsyncPromise` + detached worker; remove `multiply`.
**Where**: `cpp/sr/ErrorMapping.*`, `cpp/tests/error_mapping_test.cpp`, `cpp/MeanNSuperResolutionImpl.{h,cpp}`
**Depends on**: T7b
**Requirement**: SR-ERR-08, SR-CORE-05 (async), SR-CORE-01

**Done when**:
- [ ] Host tests for every exception category → exact prefix
- [ ] Generated codegen struct names confirmed from `MeanNSuperResolutionSpecJSI.h` (cite path) and used
- [ ] Worker lambda captures no `this` (code review point)
- [ ] `yarn test:native` passes; C++ compiles in T9 Android build (combined evidence)

**Tests**: unit (mapping) + build · **Gate**: quick (native); full after T9
**Commit**: `feat(native): async superResolve TurboModule method`
**Status**: ✅ Done (c9993a9) — host mapping tests 5/5; codegen structs `NativeMeanNSuperResolutionSuperResolveOptions`/`…SuperResolveResult` (+`…Bridging`) from `example/android/app/build/generated/source/codegen/pureCxx/MeanNSuperResolutionSpec/jni/react/renderer/components/MeanNSuperResolutionSpec/MeanNSuperResolutionSpecJSI.h`; no `cv::Exception` branch (AD-004); Android compile evidence in T9

---

### T9: Android integration

**What**: `android/CMakeLists.txt` adds `cpp/sr/*.cpp`, `android/src/TempDir.cpp`, includes `cmake/OpenCVMobile.cmake`, links `${OpenCV_LIBS}`, `fbjni`, `log`; TempDir: TMPDIR (writable) → JNI `ActivityThread.currentApplication().getCacheDir()` → `E_WRITE_FAILED` hint.
**Where**: `android/CMakeLists.txt`, `android/src/TempDir.cpp`
**Depends on**: T8
**Requirement**: SR-CORE-06 (Android), SR-CORE-07 (default dir)

**Done when**:
- [ ] `yarn turbo run build:android` succeeds locally (if Android SDK present) or CI `build-android` green — log cited
- [ ] All 4 ABIs configure without concurrent-download corruption (single download in log)

**Tests**: build · **Gate**: full
**Commit**: `build(android): link opencv-mobile and add temp dir provider`
**Status**: ✅ Done (266f18a) — `yarn turbo run build:android` BUILD SUCCESSFUL; clean 4-ABI `assembleDebug` from empty `.opencv/`: arm64-v8a configure logs `downloading … extracted`, armeabi-v7a/x86/x86_64 log `cache hit (download skipped)`. DEVIATIONS: opencv-mobile v38 Android libs need NDK r30 (`__hash_memory`/`__kmpc_dispatch_deinit` undefined under r27/r29) → example `ndkVersion` 30.0.14904198, README requirement; RN 0.88 gradle-plugin writes backslash paths into `Android-autolinking.cmake` on Windows → example-only path normalization in `example/android/app/build.gradle`

---

### T10: iOS integration + packaging

**What**: Podspec reads `opencv.json`, downloads/verifies/extracts Apple package at evaluation if missing, vendors `opencv2.xcframework`, adds `Accelerate`, new source globs; `ios/TempDir.mm`; `package.json` `files` adds `opencv.json`, `cmake`, excludes `.opencv`; `turbo.json` inputs add `opencv.json`, `cmake`.
**Where**: `MeanNSuperResolution.podspec`, `ios/TempDir.mm`, `package.json`, `turbo.json`
**Depends on**: T9
**Requirement**: SR-CORE-06 (iOS)

**Done when**:
- [ ] CI `build-ios` green (log cited); `npm pack --dry-run` lists `opencv.json`, `cmake/OpenCVMobile.cmake`, no `.opencv/`

**Tests**: build · **Gate**: full (CI)
**Commit**: `build(ios): vendor opencv-mobile xcframework via podspec`
**Status**: ⚠️ Partial — CI pending (no remote). Local: `npm pack --dry-run` lists `opencv.json`, `cmake/OpenCVMobile.cmake`, `ios/TempDir.mm`, no `.opencv/`; podspec Ruby syntax NOT checked (no `ruby` on host); iOS build/runtime unverified (no Mac)

---

### T11: CI native test job + OpenCV cache

**What**: `ci.yml` adds `test-native` job (ubuntu, `yarn test:native`); cache `.opencv/` keyed on `hashFiles('opencv.json')` in native, android, ios jobs.
**Where**: `.github/workflows/ci.yml`
**Depends on**: T10
**Requirement**: AD-003

**Done when**:
- [ ] CI run on branch: all jobs green incl. `test-native` (run URL cited); second run shows cache hit
- [ ] Build gate passes locally (end of Phase 3)

**Tests**: none (CI config) · **Gate**: build
**Commit**: `ci: run native tests and cache opencv-mobile`

---

### T12: Example app demo

**What**: 5 sample frames in `example/assets/frames/` (one photo + 4 shifted crops, generated by a committed script or tool output); example-only FS dependency (RN 0.88-compatible, chosen after checking) to copy assets to cache; `App.tsx`: run button, heartbeat counter (`setInterval` 100 ms), result `<Image>`, width/height/framesUsed/elapsed, error text.
**Where**: `example/src/App.tsx`, `example/assets/frames/*`, `example/package.json`
**Depends on**: T11
**Requirement**: SR-CORE-05, SR-CORE-06 evidence, P1 Independent Test

**Done when**:
- [ ] Android device/emulator: result shows 2× dims, `framesUsed` 5, heartbeat increments during processing (screenshot/recording or log cited)
- [ ] Default output path lives in app cache dir (logged path)
- [ ] `yarn lint`, `yarn typecheck` pass

**Tests**: manual UAT · **Gate**: build + UAT
**Commit**: `feat(example): demo superResolve with bundled frames`

---

### T13: README

**What**: Install (incl. build-time OpenCV download, `OPENCV_MOBILE_DIR`, New Arch only), usage, options table, result shape, error codes, temp-dir lifecycle note, frame count rule (5/15/30).
**Where**: `README.md`
**Depends on**: T12
**Requirement**: SR-DOC-02

**Done when**:
- [ ] Every item in SR-DOC-02 present; code sample matches exported types (typecheck a copy in a scratch file)

**Tests**: none (docs) · **Gate**: build
**Commit**: `docs: document superResolve API`

---

## Phase Execution Map

```
Phase 1 → Phase 2 → Phase 3 → Phase 4

Phase 1:  T0 ──→ T1 ──→ T2
Phase 2:  T3 ──→ T4 ──→ T5 ──→ T6
Phase 3:  T7 ──→ T7b ──→ T8 ──→ T9 ──→ T10 ──→ T11
Phase 4:  T12 ──→ T13
```

Batches (~7 tasks, whole phases): **Batch A** = Phase 1 + 2 (T0–T6, 7 tasks); **Batch B** = Phase 3 + 4 (T7–T13 incl. T7b, 8 tasks).

---

## Task Granularity Check

| Task | Scope | Status |
| ---- | ----- | ------ |
| T0 | package manager restore | ✅ |
| T1 | 1 config file + findings | ✅ |
| T2 | fetch helper + harness (helper untestable alone) | ⚠️ cohesive |
| T3 | errors header + one utility module | ⚠️ cohesive |
| T4 | validate fn + TempDir interface | ⚠️ cohesive |
| T5 | one function (`run`) | ✅ |
| T6 | one function (`estimateTransform`) + wiring | ✅ |
| T7 | JS API module set (split files are one API) | ⚠️ cohesive |
| T8 | mapping fn + adapter method | ⚠️ cohesive |
| T9 | Android build file + provider | ⚠️ cohesive |
| T10 | iOS podspec + provider + packaging lists | ⚠️ cohesive |
| T11 | one CI file | ✅ |
| T12 | example app | ✅ |
| T13 | one doc | ✅ |

## Diagram-Definition Cross-Check

| Task | Depends On (body) | Diagram Shows | Status |
| ---- | ----------------- | ------------- | ------ |
| T0 | None | start P1 | ✅ |
| T1 | T0 | T0→T1 | ✅ |
| T2 | T1 | T1→T2 | ✅ |
| T3 | T2 | P1→T3 | ✅ |
| T4 | T3 | T3→T4 | ✅ |
| T5 | T4 | T4→T5 | ✅ |
| T6 | T5 | T5→T6 | ✅ |
| T7 | T6 | P2→T7 | ✅ |
| T7b | T7 | T7→T7b | ✅ |
| T8 | T7b | T7b→T8 | ✅ |
| T9 | T8 | T8→T9 | ✅ |
| T10 | T9 | T9→T10 | ✅ |
| T11 | T10 | T10→T11 | ✅ |
| T12 | T11 | P3→T12 | ✅ |
| T13 | T12 | T12→T13 | ✅ |

## Test Co-location Validation

| Task | Layer | Matrix Requires | Task Says | Status |
| ---- | ----- | --------------- | --------- | ------ |
| T0 | tooling | none | none | ✅ |
| T1 | config | none | none | ✅ |
| T2 | harness (cpp/tests) | unit | unit (smoke) | ✅ |
| T3 | cpp/sr | unit | unit | ✅ |
| T4 | cpp/sr + platform iface | unit | unit | ✅ |
| T5 | cpp/sr | unit | unit | ✅ |
| T6 | cpp/sr | unit | unit | ✅ |
| T7 | src | unit | unit | ✅ |
| T7b | cpp/sr | unit | unit | ✅ |
| T8 | cpp/sr + TM adapter | unit + build | unit + build | ✅ |
| T9 | platform glue | build + UAT | build (UAT in T12) | ✅ |
| T10 | platform glue | build + UAT | build (UAT: iOS gap, see design risk) | ✅ |
| T11 | config | none | none | ✅ |
| T12 | example | manual UAT | manual UAT | ✅ |
| T13 | docs | none | none | ✅ |
