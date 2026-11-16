# legacy-arch-android Specification

**Scope:** Complex (new domain: Old Architecture / Bridge native module on Android, RN 0.71–0.72 toolchain, dual-arch packaging; conflicts with AD-001).

## Problem Statement

The library only works on the New Architecture (AD-001: pure C++ TurboModule). An important consumer app runs React Native 0.71–0.72 on Android with `newArchEnabled=false`; there, `TurboModuleRegistry.getEnforcing('MeanNSuperResolution')` throws and nothing native is linked. We need the same `superResolve` to work in that app without forking the package or the C++ pipeline.

## Goals

- [ ] `superResolve(paths, options?)` works on Android in an RN 0.71–0.72 app with `newArchEnabled=false`, with the same results and error codes as on the New Architecture.
- [ ] One npm package serves both architectures; New Architecture apps (example on RN 0.88) keep building and passing with no consumer-side change.
- [ ] Image processing stays in shared JSI-free C++ (`cpp/sr/`, AD-003); the legacy layer is a thin adapter only.

## Out of Scope

| Feature | Reason |
| ------- | ------ |
| iOS on the Old Architecture | Target app is Android-only (owner, 2026-11-15) |
| React Native < 0.71 | No prefab AARs; target app is 0.71–0.72 (owner, 2026-11-15) |
| Separate legacy npm package or branch | Owner chose single dual-arch package |
| New API surface (progress, cancel, new options) | Parity only; same API as New Architecture |
| Lowering the NDK requirement (AD-005) | Same opencv-mobile binaries; changing that is a separate decision |
| Expo Go / Expo managed without prebuild | Native code always requires a dev/custom build |
| Validating inside the target app | Owner chose a repo test app (`example-legacy/`) as evidence |

---

## Assumptions & Open Questions

| Assumption / decision | Chosen default | Rationale | Confirmed? |
| --------------------- | -------------- | --------- | ---------- |
| AD-001 conflict | Amend AD-001: pipeline remains shared C++; add a thin Old-Arch Android adapter (Java/Kotlin + JNI) as the only non-C++ module code | Owner choice | y |
| Distribution | Same package; architecture chosen automatically (build via `newArchEnabled`, JS via TurboModule-or-`NativeModules`) | Owner choice | y |
| Evidence | New `example-legacy/` app in repo, RN 0.71–0.72, `newArchEnabled=false`, Android UAT on emulator | Owner choice; RN 0.88 example cannot disable New Arch | y |
| RN version of `example-legacy/` | 0.72.x (latest patch) | Owner choice 2026-11-15; 0.71 floor is documented but not exercised by the test app | y |
| Minimum supported RN for legacy mode | 0.71.0 (documented; only 0.72.x tested) | Matches target range and prefab availability | y |
| NDK on legacy apps | Still r30+ (AD-005); library fails fast below r30 with same message | Same OpenCV binaries; feasibility of r30 with RN 0.72 toolchain is a Design research item and may block the feature | y |
| Error shape on legacy | Promise rejects with `Error` whose `message` is byte-identical to the New Arch message (`"E_CODE: detail"`); `error.code` equals `E_CODE` | Callers parse the message prefix today (README contract); `code` is free on the Bridge | y |
| Kotlin vs Java for the adapter | Agent's discretion at Design | Implementation detail; no user-visible effect | — |

**Open questions:** none — all resolved or logged above.

---

## User Stories

### P1: superResolve on Android Old Architecture ⭐ MVP

**User Story**: As a developer of an RN 0.71–0.72 Android app with `newArchEnabled=false`, I want to install the library and call `superResolve` so that I get the same super-resolved image as New Architecture users.

**Why P1**: This is the whole reason for the feature (target app).

**Acceptance Criteria**:

1. WHEN the package is added to an RN 0.72.x Android app with `newArchEnabled=false` and NDK r30+ THEN `./gradlew assembleDebug` SHALL succeed with no manual `MainApplication`/`settings.gradle` edits beyond what RN autolinking does (LEG-01).
2. WHEN `superResolve(paths, options)` is called there with 5, 15 or 30 valid same-size frames THEN it SHALL resolve with `{ path, width, height, framesUsed }` where `width`/`height` equal reference size × `scale` (default 2) and the file at `path` exists and decodes to that size (LEG-02).
3. WHEN the same frames and options are given to the New Arch example and the legacy app THEN `width`, `height` and `framesUsed` SHALL be equal (LEG-03).
4. WHEN native rejects (e.g. missing frame file) THEN the promise SHALL reject with an `Error` whose `message` starts with the same `E_*` code and detail format as on the New Architecture (e.g. `E_DECODE_FAILED: ...` naming the frame index and path), and `error.code` SHALL equal that code (LEG-04).
5. WHEN arguments are invalid (wrong frame count, scale outside 1–4, non-string path) THEN the promise SHALL reject with `E_INVALID_ARGUMENT` before any native call, identical to New Arch (LEG-05).
6. WHEN `superResolve` runs THEN the JS thread and UI thread SHALL stay responsive (work runs off both threads) and the promise SHALL settle exactly once (LEG-06).
7. WHEN two `superResolve` calls run concurrently THEN both SHALL resolve with distinct output paths (LEG-07).

**Independent Test**: In `example-legacy/` on the Android emulator, run a 5-frame and a 15-frame job, see resolved size and image; delete one frame and see `E_DECODE_FAILED`.

---

### P1: New Architecture unchanged ⭐ MVP

**User Story**: As an existing New Architecture user, I want upgrades to keep working without config changes so that legacy support costs me nothing.

**Why P1**: Regressing current users is unacceptable.

**Acceptance Criteria**:

1. WHEN the RN 0.88 `example/` is built for Android and iOS THEN builds SHALL succeed and the module SHALL be the C++ TurboModule (no Bridge module registered as `MeanNSuperResolution`) (LEG-08).
2. WHEN `yarn test`, `yarn test:native`, `yarn typecheck`, `yarn lint` run THEN all SHALL pass, with existing tests unchanged (LEG-09).
3. WHEN a New Arch Android build links the library THEN the shared C++ sources SHALL be compiled into exactly one native library (no duplicate symbol / double CMake inclusion) (LEG-10).

**Independent Test**: CI green + example burst still works on emulator.

---

### P2: Clear failure when native is not linked

**User Story**: As a developer, I want a readable error when the native module is missing so that I don't chase a cryptic `undefined` crash.

**Why P2**: Affects install experience, not core function.

**Acceptance Criteria**:

1. WHEN neither a TurboModule nor a Bridge module named `MeanNSuperResolution` is available (e.g. iOS on Old Architecture, or app not rebuilt) THEN importing the package SHALL NOT throw, and calling `superResolve` SHALL reject with an `Error` whose message starts with `E_INTERNAL: ` and states the native module is not linked and that iOS requires the New Architecture (LEG-11).

**Independent Test**: Jest with no native module mocked → assert rejection message.

---

### P2: Documentation

**User Story**: As a consumer, I want the README to state supported architectures and versions so that I know if my app qualifies.

**Acceptance Criteria**:

1. WHEN reading the README THEN it SHALL list: Android Old Arch from RN 0.71, New Arch on Android and iOS, iOS Old Arch unsupported, NDK r30+ for both Android modes (LEG-12).
2. WHEN `package.json` is inspected THEN `peerDependencies["react-native"]` SHALL be `>=0.71.0` (LEG-13).

---

## Edge Cases

- WHEN legacy Android build uses NDK < r30 THEN configure SHALL fail with the existing AD-005 message (LEG-14).
- WHEN the app is backgrounded mid-job on legacy THEN the job SHALL still settle (resolve or reject), never leave the promise pending forever (covered by LEG-06).
- WHEN `options` is omitted on legacy THEN defaults (scale 2, temp-dir output) SHALL apply, same as New Arch (covered by LEG-02).
- WHEN the React instance is reloaded while a job runs THEN the app SHALL NOT crash; result of the stale job is discarded (LEG-15).

### Implicit-dimension sweep

| Dimension | Resolution |
| --------- | ---------- |
| Input validation & bounds | LEG-05 (JS) + shared C++ validation unchanged |
| Failure / partial-failure | LEG-04, LEG-11, LEG-14, LEG-15 |
| Idempotency / retry | N/A because call is a pure function writing a uniquely named file (existing behavior) |
| Auth / rate limits | N/A because local on-device library, no remote calls |
| Concurrency / ordering | LEG-06, LEG-07 |
| Data lifecycle / expiry | N/A because output file lifecycle is unchanged (caller-owned, temp dir) |
| Observability | N/A because library has no logging; errors surface via codes (LEG-04) |
| External-dependency failure | LEG-14; OpenCV download failure behaves as today (AD-002) |
| State-transition integrity | Promise settles exactly once (LEG-06), reload safety (LEG-15) |

---

## Requirement Traceability

| Requirement ID | Story | Phase | Status |
| -------------- | ----- | ----- | ------ |
| LEG-01 | P1: Legacy superResolve | Design | Pending |
| LEG-02 | P1: Legacy superResolve | Design | Pending |
| LEG-03 | P1: Legacy superResolve | Design | Pending |
| LEG-04 | P1: Legacy superResolve | Design | Pending |
| LEG-05 | P1: Legacy superResolve | Design | Pending |
| LEG-06 | P1: Legacy superResolve | Design | Pending |
| LEG-07 | P1: Legacy superResolve | Design | Pending |
| LEG-08 | P1: New Arch unchanged | Design | Pending |
| LEG-09 | P1: New Arch unchanged | Design | Pending |
| LEG-10 | P1: New Arch unchanged | Design | Pending |
| LEG-11 | P2: Not-linked error | Design | Pending |
| LEG-12 | P2: Docs | Design | Pending |
| LEG-13 | P2: Docs | Design | Pending |
| LEG-14 | Edge: NDK | Design | Pending |
| LEG-15 | Edge: reload | Design | Pending |

**Coverage:** 15 total, 0 mapped to tasks, 15 unmapped (pre-Tasks)

---

## Success Criteria

- [ ] `example-legacy/` (RN 0.72.x, `newArchEnabled=false`) builds and completes 5- and 15-frame jobs on the Android emulator, with evidence captured.
- [ ] CI green on all existing jobs plus a legacy Android build job.
- [ ] Zero changes to `cpp/sr/` behavior (host tests unchanged and passing).
