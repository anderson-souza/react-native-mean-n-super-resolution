# example-camera-burst Tasks

## Execution Protocol (MANDATORY -- do not skip)

Implement these tasks with the `tlc-spec-driven` skill: **activate it by name and follow its Execute flow and Critical Rules.** Do not search for skill files by filesystem path. The skill is the source of truth for the full flow (per-task cycle, sub-agent delegation, adequacy review, Verifier, discrimination sensor).

**If the skill cannot be activated, STOP and tell the user — do not proceed without it.**

---

**Design**: `.specs/features/example-camera-burst/design.md`
**Status**: Execute done (2026-11-13) — T1 b05bca8, T2 bdf7574, T3 8dac7a3, T4 a56a90a, T5 6af7d8e, T6 02abb96, T7 UAT fixes d3d30e2 + 859da98 + evidence; awaiting Verifier. Approved (2026-11-12) — Android-only (iOS deferred by owner); UAT on AVD virtual-scene camera; tools: context7, skill `react-native-expert`, adb

---

## Test Coverage Matrix

> Generated from codebase, project guidelines, and spec — confirm before Execute. Guidelines found: `CLAUDE.md` (Jest preset, source export condition), `CONTRIBUTING.md` ("add tests for your change if possible"), `package.json` `jest` (no coverage threshold; root config picks up `example/src/**/__tests__`), `.github/workflows/ci.yml` (`yarn test --maxWorkers=2 --coverage`). Existing samples: `src/__tests__/superResolve.test.ts` (Jest, mocked native, asserts `E_*` codes) is the style floor. No React component test library in repo (design: screens are UAT-only).

| Code Layer | Required Test Type | Coverage Expectation | Location Pattern | Run Command |
| ---------- | ------------------ | -------------------- | ---------------- | ----------- |
| Burst logic (`example/src/burst/**`) | unit (Jest, fake deps) | All branches; 1:1 to CAM-03, 04, 05, 06 (outcome data), 10, 11, 13, 14; edge case "next burst uses newly selected N" | `example/src/burst/__tests__/*.test.ts` | `yarn test` |
| Screens + App switch (`example/src/App.tsx`, `example/src/screens/**`) | manual UAT (Android device) | CAM-01, 02, 06, 07, 08, 09, 10, 11, 12 (code review), 13; lint + typecheck | — | `yarn example android` |
| Native config (`example/android/**`, `example/ios/**`, `example/package.json`) | build + static check | Android build passes; manifest keys present (CAM-15, 18) | — | `yarn turbo run build:android`; `grep` |

## Gate Check Commands

> Generated from codebase — confirm before Execute.

| Gate Level | When to Use | Command |
| ---------- | ----------- | ------- |
| Quick | Tasks with unit tests only | `yarn test` |
| UI | Tasks touching screens / App | `yarn lint && yarn typecheck && yarn test` |
| Build | Native config / deps tasks, end of each phase | `yarn lint && yarn typecheck && yarn test && yarn turbo run build:android --force` |

---

## Execution Plan

### Phase 1: Foundation — deps, native config, burst logic

```
T1 → T2 → T3
```

### Phase 2: Screens

```
T4 → T5 → T6
```

### Phase 3: Device verification

```
T7
```

---

## Task Breakdown

### T1: Add VisionCamera + Nitro deps and get the Android build green

**What**: Add exact-pinned `react-native-vision-camera@5.2.4`, `react-native-nitro-modules@0.37.2` and `react-native-nitro-image@0.15.2` to `example/package.json`, run `yarn`, and make `build:android` pass. Fix Kotlin (`kotlinVersion`) or the Windows autolinking path workaround only if the build demands it, and record any such fix in the commit body.
**Where**: `example/package.json`, `yarn.lock`, `example/android/build.gradle` / `example/android/app/build.gradle` (only if needed)
**Depends on**: None
**Reuses**: Existing Windows autolinking workaround in `example/android/app/build.gradle`
**Requirement**: CAM-15 (build part), CAM-17

**Tools**: MCP: context7 (VisionCamera / Nitro install docs) · Skill: `react-native-expert`

**Done when**:

- [ ] The three deps are pinned exactly (no `^`/`~`) in `example/package.json`
- [ ] `yarn turbo run build:android --force` exits 0 (log tail cited in commit/validation)
- [ ] `yarn lint && yarn typecheck && yarn test` pass; test count unchanged from baseline (record the number)

**Tests**: build
**Gate**: build
**Commit**: `chore(example): add react-native-vision-camera and nitro deps`

---

### T2: Camera permission and portrait lock (Android)

**What**: Android manifest: `android.permission.CAMERA` + `android:screenOrientation="portrait"` on `MainActivity`.
**Where**: `example/android/app/src/main/AndroidManifest.xml`
**Depends on**: T1
**Reuses**: —
**Requirement**: CAM-15 (permission), CAM-18

**Tools**: MCP: NONE · Skill: NONE

**Done when**:

- [ ] `grep` shows `android.permission.CAMERA` and `screenOrientation="portrait"` in the manifest
- [ ] `yarn turbo run build:android --force` exits 0

**Tests**: build + static check
**Gate**: build
**Commit**: `chore(example): declare camera permission and lock portrait on android`

---

### T3: burstRunner + frameCounts + types with unit tests

**What**: Implement `example/src/burst/frameCounts.ts`, `types.ts` and `burstRunner.ts` exactly per design (sync guard, cleanup of previous paths with swallowed errors, sequential capture with progress events, capture-failure short-circuit, `superResolve` in capture order, timed outcome, message pass-through).
**Where**: `example/src/burst/{frameCounts,types,burstRunner}.ts`, `example/src/burst/__tests__/burstRunner.test.ts`
**Depends on**: T2 (phase order only; no code dependency)
**Reuses**: `SuperResolveResult` type from `react-native-mean-n-super-resolution`; test style from `src/__tests__/superResolve.test.ts`
**Requirement**: CAM-03, CAM-04, CAM-05, CAM-06, CAM-10, CAM-11, CAM-13, CAM-14

**Tools**: MCP: NONE · Skill: NONE

**Done when** (each bullet = at least one test asserting the spec outcome):

- [ ] `FRAME_COUNTS` deep-equals `[5, 15, 30]`; `DEFAULT_FRAME_COUNT === 5` (CAM-03)
- [ ] For N ∈ {5, 15, 30}: `takeFrame` called exactly N times with indices `0..N-1`; progress events `captured` = `1..N`, `total` = N, in order (CAM-04)
- [ ] `superResolve` called once with the paths in capture order, after a `processing` event (CAM-05)
- [ ] Success outcome: `result` is what `superResolve` resolved, `paths` = captured paths, `requested` = N, `elapsedMs` = `now()` delta around `superResolve` only (CAM-06)
- [ ] `superResolve` rejects with `Error('E_ALIGNMENT_FAILED: …')` → `{ok:false, stage:'process', message}` with the message unchanged (CAM-10)
- [ ] `takeFrame` throws at index k → `{ok:false, stage:'capture'}`, `superResolve` not called, no further `takeFrame` calls (CAM-11)
- [ ] Second `start()` while running returns `null` synchronously; `takeFrame` total calls stay N (CAM-13); `isRunning()` true during and false after (success and failure)
- [ ] Next `start()` calls `deleteFiles` with the previous burst's paths (including partial paths after a capture failure) before its first `takeFrame`; first-ever burst deletes nothing / empty list; `deleteFiles` rejection does not stop the burst (CAM-14)
- [ ] Next burst with a different N captures the new N (edge case)
- [ ] `yarn test` passes; test count = baseline + new tests (record both)

**Tests**: unit
**Gate**: quick
**Commit**: `feat(example): add burst runner for camera capture`

---

### T4: Split HomeScreen out of App and add the screen switch

**What**: Move the current `App.tsx` demo into `example/src/screens/HomeScreen.tsx` verbatim plus the "Capture burst with camera" button (`onOpenCamera` prop, disabled while busy). `App.tsx` becomes the `home | camera | preview` switch; camera renders a temporary placeholder with Back until T5.
**Where**: `example/src/App.tsx`, `example/src/screens/HomeScreen.tsx`
**Depends on**: T3
**Reuses**: Entire current `App.tsx` body
**Requirement**: CAM-01, CAM-08 (home↔camera)

**Tools**: MCP: NONE · Skill: `react-native-expert`

**Done when**:

- [ ] Diff of the moved demo code vs. old `App.tsx` shows only the added button/prop and import path changes
- [ ] `yarn lint && yarn typecheck && yarn test` pass (test count unchanged from T3)

**Tests**: manual UAT (T7)
**Gate**: UI
**Commit**: `refactor(example): split home screen and add screen switch`

---

### T5: CameraScreen (Android)

**What**: Implement `CameraScreen` per design (iOS notice branch dropped — iOS out of scope): Android permission gate (request on mount, denied → message + "Open settings"), "No camera available" branch, fixed-size `<Camera>` with ref and `onPreviewStarted`, 5/15/30 picker (default 5), Capture wired to `createBurstRunner` with `takeFrame` = `takeSnapshot` + `saveToFileAsync(path,'jpg',95)` into `Dirs.CacheDir/burst-<id>-<i>.jpg`, `deleteFiles` via `FileSystem.unlink`, status `i/N` / "Processing…", error text, controls + Back disabled while busy, `onBusyChange`, `onResult`, and a `[camera-burst]` console log line on success/failure. Replace the T4 placeholder; App's `BackHandler` honors busy.
**Where**: `example/src/screens/CameraScreen.tsx`, `example/src/App.tsx`
**Depends on**: T4
**Reuses**: `burstRunner` (T3), `react-native-file-access` cleanup pattern from `HomeScreen`
**Requirement**: CAM-02, 03, 04, 05, 09, 10, 11, 12, 13, 14

**Tools**: MCP: context7 (VisionCamera Camera/permission API) · Skill: `react-native-expert`

**Done when**:

- [ ] `yarn lint && yarn typecheck && yarn test` pass (test count unchanged from T3)

**Tests**: manual UAT (T7)
**Gate**: UI
**Commit**: `feat(example): add camera burst screen`

---

### T6: PreviewScreen and preview navigation

**What**: Implement `PreviewScreen` (metrics `Output: W×H`, `framesUsed: k / N requested`, `Processing: X ms`; Enhanced/Reference toggle; image at full width with output aspect ratio, `resizeMode="contain"`) and wire `App` camera→preview (`onResult`) and preview→camera (Back + hardware back).
**Where**: `example/src/screens/PreviewScreen.tsx`, `example/src/App.tsx`
**Depends on**: T5
**Reuses**: Result block pattern from `HomeScreen`
**Requirement**: CAM-06, CAM-07, CAM-08

**Tools**: MCP: NONE · Skill: `react-native-expert`

**Done when**:

- [ ] `yarn lint && yarn typecheck && yarn test` pass (test count unchanged from T3)
- [ ] `yarn turbo run build:android --force` exits 0 (end of phase 2 build gate)

**Tests**: manual UAT (T7)
**Gate**: build
**Commit**: `feat(example): add enhanced image preview screen`

---

### T7: Android device UAT + evidence

**What**: Install the example on an Android AVD with the virtual-scene back camera (owner choice) and run every scenario below, saving screenshots and `adb logcat` lines under `.specs/features/example-camera-burst/evidence/`.
**Where**: `.specs/features/example-camera-burst/evidence/**`
**Depends on**: T6
**Reuses**: Evidence approach from `opencv-super-resolution` validation
**Requirement**: CAM-01, 02, 06, 07, 08, 09, 10, 13, 18 (runtime); CAM-12 (code review entry)

**Tools**: MCP: NONE · Skill: NONE (adb)

**Done when** (each = screenshot and/or log line):

- [ ] Home shows existing buttons + "Capture burst with camera"; 5-frame bundled demo still reports 1280×960 (CAM-01)
- [ ] Bursts of 5, 15 and 30 each reach the preview; `[camera-burst]` log shows N, W×H = 2 × snapshot size, framesUsed ≤ N, ms (CAM-02, 04, 05, 06)
- [ ] Toggle shows reference and enhanced (CAM-07); back preview→camera→home works (CAM-08)
- [ ] Permission denied (`adb shell pm revoke … android.permission.CAMERA` / deny dialog) → message + "Open settings" (CAM-09)
- [ ] A rejection shown on the camera screen with its `E_*` code and preview not opened (CAM-10); method recorded (e.g. lens covered → featureless)
- [ ] Rapid double tap on Capture → one burst (single progress sequence in log) (CAM-13)
- [ ] Device rotated during/after burst → UI stays portrait (CAM-18)
- [ ] Gap logged: CAM-12 (no-camera branch) verified by code review only; AVD virtual scene (not a real sensor) stated

**Tests**: manual UAT
**Gate**: build
**Commit**: `test(example): record camera burst device evidence`

---

## Phase Execution Map

```
Phase 1 → Phase 2 → Phase 3

Phase 1:  T1 ──→ T2 ──→ T3
Phase 2:  T4 ──→ T5 ──→ T6
Phase 3:  T7
```

7 tasks → single batch → executed inline (no sub-agents). Verifier sub-agent runs automatically after T7.

---

## Task Granularity Check

| Task | Scope | Status |
| --- | --- | --- |
| T1: deps + Android build | 1 concern (dependency install), build fixes only if forced | ✅ Granular |
| T2: manifest + plist keys | 2 config files, one concern (platform declarations) | ⚠️ OK — cohesive |
| T3: burstRunner (+ constants, types) | 1 function module + its constants/types | ⚠️ OK — cohesive |
| T4: HomeScreen split + switch | 1 component move + switch shell | ⚠️ OK — cohesive |
| T5: CameraScreen | 1 component (+ BackHandler busy hook-up in App) | ✅ Granular |
| T6: PreviewScreen | 1 component (+ App wiring) | ✅ Granular |
| T7: UAT evidence | 1 verification deliverable | ✅ Granular |

## Diagram-Definition Cross-Check

| Task | Depends On (task body) | Diagram Shows | Status |
| --- | --- | --- | --- |
| T1 | None | start of Phase 1 | ✅ Match |
| T2 | T1 | T1 → T2 | ✅ Match |
| T3 | T2 | T2 → T3 | ✅ Match |
| T4 | T3 | Phase 1 → T4 | ✅ Match |
| T5 | T4 | T4 → T5 | ✅ Match |
| T6 | T5 | T5 → T6 | ✅ Match |
| T7 | T6 | Phase 2 → T7 | ✅ Match |

## Test Co-location Validation

| Task | Code Layer Created/Modified | Matrix Requires | Task Says | Status |
| --- | --- | --- | --- | --- |
| T1 | Native config / deps | build + static | build | ✅ OK |
| T2 | Native config | build + static | build + static check | ✅ OK |
| T3 | Burst logic | unit | unit | ✅ OK |
| T4 | Screens + App | manual UAT + lint/typecheck | manual UAT (T7) + UI gate | ✅ OK |
| T5 | Screens + App | manual UAT + lint/typecheck | manual UAT (T7) + UI gate | ✅ OK |
| T6 | Screens + App | manual UAT + lint/typecheck | manual UAT (T7) + build gate | ✅ OK |
| T7 | Evidence | manual UAT | manual UAT | ✅ OK |

Note: T4–T6 point to T7 for UAT because the matrix's required type for screens is manual UAT, which needs the whole flow assembled; their own gate (lint/typecheck/Jest) runs in-task.
