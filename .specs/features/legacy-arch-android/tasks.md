# legacy-arch-android Tasks

## Execution Protocol (MANDATORY -- do not skip)

Implement these tasks with the `tlc-spec-driven` skill: **activate it by name and follow its Execute flow and Critical Rules.** Do not search for skill files by filesystem path. The skill is the source of truth for the full flow (per-task cycle, sub-agent delegation, adequacy review, Verifier, discrimination sensor).

**If the skill cannot be activated, STOP and tell the user — do not proceed without it.**

---

**Design**: `.specs/features/legacy-arch-android/design.md`
**Status**: Approved (2026-11-15) — executing via 2 batch sub-agents

Baseline before T1 (2026-11-15, HEAD `7090ee0`): `yarn test` 60 passed; `yarn test:native` 46 passed.

---

## Test Coverage Matrix

> Generated from codebase, project guidelines, and spec — confirm before Execute. Guidelines found: `CLAUDE.md` (rules 9, 12; commands), `package.json` `jest`, `.github/workflows/ci.yml` (lint, typecheck, jest `--coverage`, test-native, prepare, build-android, build-ios); samples `src/__tests__/superResolve.test.ts`, `cpp/tests/*_test.cpp`, `.specs/features/example-camera-burst/evidence/`.

| Code Layer | Required Test Type | Coverage Expectation | Location Pattern | Run Command |
| ---------- | ------------------ | -------------------- | ---------------- | ----------- |
| JS wrapper / registry (`src/*.ts`) | unit (Jest) | 1:1 to LEG-05, LEG-11; existing tests untouched | `src/__tests__/*.test.ts` | `yarn test` |
| Shared JSI-free C++ (`cpp/sr/`, `android/legacy/TempDirLegacy.cpp`) | unit (host GoogleTest) | All branches; every current `toScale` case pinned; TMPDIR → cacheDir → `E_WRITE_FAILED` order | `cpp/tests/*_test.cpp` | `yarn test:native` |
| JNI adapter + Java module (`android/legacy/LegacyJni.cpp`, `android/src/main/java/**`) | e2e (emulator UAT in `example-legacy/`) | LEG-02..07, LEG-15: happy 5 & 15 frames, missing-frame error with `message` + `code`, concurrent, reload | `.specs/features/legacy-arch-android/evidence/` | manual UAT + Legacy build gate |
| Gradle / CMake / autolinking config | build | Legacy and New Arch Android builds succeed; NDK < r30 fails with AD-005 message | — | Android gates below |
| New Arch regression (example 0.88) | e2e (emulator UAT) | LEG-08, LEG-10: burst + home flow still work; module is C++ TurboModule | `.specs/features/legacy-arch-android/evidence/` | manual UAT + New Arch build gate |
| Docs / package metadata | none | build gate only | — | Build gate |

## Gate Check Commands

> Generated from codebase — confirm before Execute. Windows host: use `gradlew.bat` and the AVD workarounds in project memory.

| Gate Level | When to Use | Command |
| ---------- | ----------- | ------- |
| Quick JS | JS-only tasks | `yarn test` |
| Quick native | Host C++ tasks | `yarn test:native` |
| Build | Phase end, config/docs tasks | `yarn lint && yarn typecheck && yarn test && yarn test:native && yarn prepare` |
| Android New Arch | Gradle/CMake/autolinking tasks | `cd example/android && ./gradlew assembleDebug` |
| Android Legacy | Legacy native/Gradle/example tasks | `node scripts/pack-legacy-example.mjs && cd example-legacy/android && ./gradlew assembleDebug` |

---

## Execution Plan

### Phase 1: Shared groundwork (no behavior change)

```
T1 → T2 → T3
```

### Phase 2: Legacy Android path (ends in go/no-go)

```
T4 → T5 → T6 → T7 → T8
```

**Go/no-go checkpoint after T6** (first time the legacy `.so` is built with AGP 7.4.2 + NDK r30 and loaded): if build or `System.loadLibrary` fails and cannot be fixed inside the design, STOP with BLOCKED / TRIED / NEED (AD-005 re-open).

### Phase 3: Regression, CI, docs

```
T9 → T10 → T11
```

---

## Task Breakdown

### T1: Extract `toScale` into `cpp/sr/Scale.h`

**What**: Move `toScale` from `cpp/MeanNSuperResolutionImpl.cpp:17-22` into header-only `sr::toScale(std::optional<double>)`; Impl calls it; add host test.
**Where**: `cpp/sr/Scale.h` (new), `cpp/MeanNSuperResolutionImpl.cpp`, `cpp/tests/scale_test.cpp` (new), `cpp/tests/CMakeLists.txt`
**Depends on**: None
**Reuses**: existing function body verbatim
**Requirement**: LEG-03, LEG-05 (parity of scale handling across archs)

**Tools**: MCP: NONE · Skill: NONE

**Done when**:

- [ ] Tests assert: `nullopt → 2`; `1,2,3,4 → same`; `2.5 → 0`; `NaN, ±inf → 0`; `±1e7 → 0`; `-1 → -1`
- [ ] Impl has no local `toScale`
- [ ] Gate: `yarn test:native` passes, 46 + new tests (no deletions)

**Tests**: unit · **Gate**: quick native
**Commit**: `refactor(cpp): share scale conversion in sr/Scale.h`

---

### T2: Extract NDK r30 check into `cmake/RequireNdk.cmake`

**What**: Move the fail-fast block of `android/CMakeLists.txt:4-19` into `cmake/RequireNdk.cmake` (function/macro, same message) and include it from `android/CMakeLists.txt`.
**Where**: `cmake/RequireNdk.cmake` (new), `android/CMakeLists.txt`
**Depends on**: T1
**Reuses**: existing block verbatim
**Requirement**: LEG-14

**Tools**: MCP: NONE · Skill: NONE

**Done when**:

- [ ] `android/CMakeLists.txt` has no inline NDK check
- [ ] Android New Arch gate passes (NDK r30)
- [ ] With `ndkVersion` set to `29.0.14206865` in a throwaway local edit, configure fails with the unchanged message (output captured in task notes, edit reverted)

**Tests**: build · **Gate**: Android New Arch
**Commit**: `refactor(android): move NDK floor check to cmake/RequireNdk.cmake`

---

### T3: Nullable registry + not-linked rejection

**What**: `TurboModuleRegistry.get<Spec>`; `superResolve` rejects `E_INTERNAL: native module MeanNSuperResolution is not linked; rebuild the app (iOS requires the New Architecture)` when module is null (after `validateArgs`).
**Where**: `src/NativeMeanNSuperResolution.ts`, `src/superResolve.native.ts`, `src/__tests__/notLinked.test.ts` (new)
**Depends on**: T2
**Reuses**: `src/errors.ts` codes, existing mock pattern in `superResolve.test.ts`
**Requirement**: LEG-11, LEG-05

**Tools**: MCP: `context7` (RN TurboModuleRegistry `get`) · Skill: NONE

**Done when**:

- [ ] Test: module mocked as `null` → importing package does not throw; valid call rejects with exact message above
- [ ] Test: module `null` + invalid args → still `E_INVALID_ARGUMENT` (validation first)
- [ ] Existing 60 tests unchanged and passing; Build gate passes (codegen accepts `get`, `yarn prepare` ok)

**Tests**: unit · **Gate**: Build
**Commit**: `feat: reject with E_INTERNAL when native module is not linked`

---

### T4: Scaffold `example-legacy/` + pack script

**What**: Standalone RN 0.72.17 app (Java template), `newArchEnabled=false`, `ndkVersion "30.0.14904198"`, `react-native-file-access` 3.x; `scripts/pack-legacy-example.mjs` builds + packs lib and installs tarball into it; root lint/tsc/jest/workspaces/`files` ignore `example-legacy/`.
**Where**: `example-legacy/**` (new), `scripts/pack-legacy-example.mjs` (new), root `.eslintignore`/eslint config, `tsconfig.json`, `package.json` (jest ignore), `.gitignore`
**Depends on**: T3
**Reuses**: `example/` Windows gradle workaround if needed
**Requirement**: LEG-01 (harness)

**Tools**: MCP: `context7` (RN 0.72 template) · Skill: `react-native-expert`

**Done when**:

- [ ] Android Legacy gate passes (app builds with lib installed; lib not yet native-linked)
- [ ] Build gate passes at root (root tools ignore `example-legacy/`)

**Tests**: build · **Gate**: Android Legacy + Build
**Commit**: `chore(example-legacy): scaffold RN 0.72 old-arch test app`

---

### T5: Legacy native library (`android/legacy/`)

**What**: `CMakeLists.txt` (SHARED `meannsuperresolution`, `c++_static`, C++20, `-fexceptions -frtti`, OpenCV + `log`, `RequireNdk`, exported `Java_*`/`JNI_*` only), `TempDirLegacy.cpp` (TMPDIR → per-call cacheDir → `E_WRITE_FAILED`, thread-local), `LegacyJni.cpp` (signature per design).
**Where**: `android/legacy/{CMakeLists.txt,TempDirLegacy.cpp,TempDirLegacy.h,LegacyJni.cpp}`, `cpp/tests/temp_dir_legacy_test.cpp` (new), `cpp/tests/CMakeLists.txt`
**Depends on**: T4
**Reuses**: `sr::run`, `sr::toRejectMessage`, `sr::toScale`, `cmake/OpenCVMobile.cmake`, `cmake/RequireNdk.cmake`
**Requirement**: LEG-02, LEG-04, LEG-14

**Tools**: MCP: NONE · Skill: NONE

**Done when**:

- [ ] Host tests (separate test binary or source-level, not linking JNI): writable TMPDIR wins; unwritable TMPDIR → cacheDir; neither → `SrError` `E_WRITE_FAILED` with exact message; value not reused by a later call without cacheDir
- [ ] Gate: `yarn test:native` passes (46 + T1 + new)

**Tests**: unit · **Gate**: quick native
**Commit**: `feat(android): add old-arch JNI native library`

---

### T6: Gradle module + Java module/package + autolinking  ⛳ go/no-go

**What**: `android/build.gradle` (dual-arch per design), `AndroidManifest.xml`, Java `MeanNSuperResolutionModule`, `MeanNSuperResolutionPackage`, `MeanNSuperResolutionNative`, `SuperResolveResult`, `SuperResolveException`; `react-native.config.js` explicit `packageImportPath`/`packageInstance`; `package.json` `files` excludes `android/.cxx`.
**Where**: `android/build.gradle`, `android/src/main/**`, `react-native.config.js`, `package.json`
**Depends on**: T5
**Reuses**: design component specs
**Requirement**: LEG-01, LEG-04, LEG-06, LEG-07, LEG-15

**Tools**: MCP: `context7` (RN 0.72 `ReactContextBaseJavaModule`, `Promise`) · Skill: `react-native-expert`

**Done when**:

- [ ] Android Legacy gate passes (`libmeannsuperresolution.so` present in APK for all ABIs in `reactNativeArchitectures`)
- [ ] `NativeModules.MeanNSuperResolution` is non-null in `example-legacy` on emulator (logged)
- [ ] If either fails beyond design scope → BLOCKED / TRIED / NEED

**Tests**: e2e (smoke) · **Gate**: Android Legacy
**Commit**: `feat(android): add old-architecture module via Gradle`

---

### T7: `example-legacy` test screen

**What**: One screen: copy bundled `frames/frame0..4.png` (same files as `example/assets/frames`) to cache; buttons: run 5 frames, run 15 frames (frames repeated as in `example`), missing frame, 2 concurrent; show `{width,height,framesUsed}`, `error.message`, `error.code`, and output image.
**Where**: `example-legacy/App.tsx`, `example-legacy/android/app/src/main/res/drawable/` (frames)
**Depends on**: T6
**Reuses**: `example/src/screens/HomeScreen.tsx` copy flow
**Requirement**: LEG-02, LEG-04, LEG-07

**Tools**: MCP: NONE · Skill: `react-native-expert`

**Done when**:

- [ ] Android Legacy gate passes; screen renders on emulator

**Tests**: e2e (exercised in T8) · **Gate**: Android Legacy
**Commit**: `feat(example-legacy): add super-resolve test screen`

---

### T8: Legacy UAT on emulator

**What**: Run UAT and write `evidence/uat-legacy-android.md` with screenshots/logcat.
**Where**: `.specs/features/legacy-arch-android/evidence/`
**Depends on**: T7
**Requirement**: LEG-02, LEG-04, LEG-06, LEG-07, LEG-15

**Done when** (each with evidence):

- [ ] 5 frames, default scale → resolves; `width/height` = 2× frame size; output decodes
- [ ] 15 frames → resolves, `framesUsed` recorded
- [ ] Missing frame → `message` starts `E_DECODE_FAILED: ` naming index + path; `code === 'E_DECODE_FAILED'`
- [ ] 2 concurrent → both resolve, distinct paths
- [ ] UI responsive during job (spinner animates / button press handled)
- [ ] Reload (`r` in Metro) mid-job → no crash
- [ ] Recorded `{width,height,framesUsed}` for 5 and 15 frames for LEG-03 comparison in T9

**Tests**: e2e · **Gate**: UAT evidence
**Commit**: `test(example-legacy): record old-arch Android UAT evidence`

---

### T9: New Arch regression

**What**: Build and UAT `example/` (RN 0.88) Android with the new Gradle module; confirm C++ TurboModule serves `MeanNSuperResolution`; run same 5/15-frame inputs and compare with T8.
**Where**: `.specs/features/legacy-arch-android/evidence/uat-newarch-regression.md`
**Depends on**: T8
**Requirement**: LEG-03, LEG-08, LEG-09, LEG-10

**Done when**:

- [ ] Android New Arch gate passes; no duplicate-symbol / double-CMake errors; APK has no `libmeannsuperresolution.so`
- [ ] Home flow 5/15 frames resolves; `{width,height,framesUsed}` equal to T8 values
- [ ] Camera burst still works (one run)
- [ ] Build gate passes at root; iOS build delegated to CI `build-ios` (Windows host) — noted, not claimed

**Tests**: e2e · **Gate**: Android New Arch + Build
**Commit**: `test: record new-arch regression evidence`

---

### T10: CI job `build-android-legacy`

**What**: New job: JDK 17, NDK r30, `node scripts/pack-legacy-example.mjs`, `example-legacy` `assembleDebug`. Existing jobs untouched.
**Where**: `.github/workflows/ci.yml`
**Depends on**: T9
**Reuses**: `build-android` job steps (NDK install)
**Requirement**: LEG-01, LEG-09

**Done when**:

- [ ] YAML valid (actionlint if available, else `yarn` parse check); job steps mirror local Legacy gate
- [ ] CI result reported after push (not claimed before)

**Tests**: build · **Gate**: Build
**Commit**: `ci: build old-architecture example on Android`

---

### T11: README + peerDependencies

**What**: Support matrix (Android Old Arch from RN 0.71, tested 0.72; New Arch Android + iOS; iOS Old Arch unsupported; NDK r30+ both Android modes) and `peerDependencies.react-native` = `>=0.71.0`.
**Where**: `README.md`, `package.json`
**Depends on**: T10
**Requirement**: LEG-12, LEG-13

**Done when**:

- [ ] README section contains each item above; Build gate passes

**Tests**: none · **Gate**: Build
**Commit**: `docs: document old-architecture Android support`

---

## Phase Execution Map

```
Phase 1 → Phase 2 → Phase 3

Phase 1:  T1 ──→ T2 ──→ T3
Phase 2:  T4 ──→ T5 ──→ T6 ⛳ ──→ T7 ──→ T8
Phase 3:  T9 ──→ T10 ──→ T11
```

Batches: B1 = Phase 1 + Phase 2 (8 tasks), B2 = Phase 3 (3 tasks). Then Verifier.

---

## Task Granularity Check

| Task | Scope | Status |
| ---- | ----- | ------ |
| T1 | 1 function moved + test | ✅ |
| T2 | 1 CMake block moved | ✅ |
| T3 | registry + 1 rejection branch | ✅ |
| T4 | test-app scaffold + its install script | ⚠️ cohesive (harness) |
| T5 | 1 native target (3 small files) | ⚠️ cohesive (one `.so`) |
| T6 | Gradle module + its Java classes | ⚠️ cohesive (cannot build separately) |
| T7 | 1 screen | ✅ |
| T8 | 1 evidence doc | ✅ |
| T9 | 1 evidence doc | ✅ |
| T10 | 1 CI job | ✅ |
| T11 | 1 doc section + 1 field | ✅ |

## Diagram-Definition Cross-Check

| Task | Depends On (body) | Diagram | Status |
| ---- | ----------------- | ------- | ------ |
| T1 | None | — | ✅ |
| T2 | T1 | T1→T2 | ✅ |
| T3 | T2 | T2→T3 | ✅ |
| T4 | T3 | Phase1→T4 | ✅ |
| T5 | T4 | T4→T5 | ✅ |
| T6 | T5 | T5→T6 | ✅ |
| T7 | T6 | T6→T7 | ✅ |
| T8 | T7 | T7→T8 | ✅ |
| T9 | T8 | Phase2→T9 | ✅ |
| T10 | T9 | T9→T10 | ✅ |
| T11 | T10 | T10→T11 | ✅ |

## Test Co-location Validation

| Task | Layer | Matrix Requires | Task Says | Status |
| ---- | ----- | --------------- | --------- | ------ |
| T1 | Shared C++ | unit | unit | ✅ |
| T2 | CMake config | build | build | ✅ |
| T3 | JS wrapper | unit | unit | ✅ |
| T4 | Config / test app | build | build | ✅ |
| T5 | Shared C++ (TempDirLegacy) + JNI | unit + e2e | unit (+ e2e in T6/T8 via merge-forward: JNI only runnable once Gradle module exists) | ✅ |
| T6 | Java module + Gradle | e2e + build | e2e smoke | ✅ |
| T7 | Test app | e2e | e2e (T8) | ✅ test app is the harness; exercised in T8 |
| T8 | e2e evidence | e2e | e2e | ✅ |
| T9 | New Arch regression | e2e | e2e | ✅ |
| T10 | CI config | build | build | ✅ |
| T11 | Docs | none | none | ✅ |
