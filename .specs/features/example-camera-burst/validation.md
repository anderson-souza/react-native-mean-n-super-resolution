# example-camera-burst Validation

**Date**: 2026-11-13
**Spec**: `.specs/features/example-camera-burst/spec.md` (CAM-07 and CAM-09 wording tightened in cdc85a6)
**Diff range**: `5fe938c^..HEAD` (5fe938c spec commit through cdc85a6) on `feat/example-camera-burst`
**Verifier**: independent sub-agent (author ≠ verifier)
**Iteration**: 2 of max 3 (re-verification after fix iteration 1: 4c2ef45, defe18f, cdc85a6)

**Verdict**: ✅ PASS. Both surviving mutants from iteration 1 (M5, M6) are now killed. CAM-12's factory-error path now shows "No camera available". CAM-07 and CAM-09 now give exact text, and the code matches it. All gates are green with 60 tests.

---

## Iteration history

| Iteration | Verdict | Gaps |
| --------- | ------- | ---- |
| 1 (code at 80d6f83) | ❌ FAIL | M5 survived (elapsedMs scope, CAM-06); M6 survived (progress after frame, CAM-04); CAM-12 factory-error path showed only the raw error; spec-precision gaps CAM-07, CAM-09 |
| 2 (code at cdc85a6) | ✅ PASS | none blocking (see Residual notes) |

Fix commits checked:
- `4c2ef45`, test-only. The fake clock now advances in `takeFrame` (+100) and `deleteFiles` (+10) as well as `superResolve` (+250). It adds two tests: an ordered frame/progress log, and no progress for a failed frame. `burstRunner.ts` is unchanged since 80d6f83 (`git diff --quiet 80d6f83 HEAD -- burstRunner.ts`).
- `defe18f`. `CameraScreen.tsx:158-164`: the factory-error branch shows "No camera available" with the raw error below it.
- `cdc85a6`, spec plus artifacts. CAM-07 and CAM-09 reworded. Also commits the iteration-1 validation.md and the lessons files.

---

## Task Completion

| Task | Status | Notes |
| ---- | ------ | ----- |
| T1 deps + Android build | ✅ Done | Deps pinned exactly in `example/package.json`. Build: evidence.md CAM-15 row. |
| T2 manifest | ✅ Done | `android.permission.CAMERA` and `screenOrientation="portrait"` (diff checked). |
| T3 burstRunner + tests | ✅ Done | 17 tests. Sensor 6/6 killed (iteration 2). |
| T4 HomeScreen split | ✅ Done | Verbatim move (diff checked in iteration 1). |
| T5 CameraScreen | ✅ Done | Plus fix defe18f (CAM-12). |
| T6 PreviewScreen | ✅ Done | — |
| T7 UAT evidence | ✅ Done | AVD virtual scene. CAM-12 by code review only (disclosed). |
| Fix iteration 1 | ✅ Done | 4c2ef45, defe18f, cdc85a6 |

---

## Spec-Anchored Acceptance Criteria

`T` = `example/src/burst/__tests__/burstRunner.test.ts` (line numbers at HEAD). `E` = `evidence/evidence.md`. Screens are UAT-only per the Test Coverage Matrix. In iteration 1 the Verifier looked at downscaled copies of PNGs 04, 10b, 12 and 13, and they match E.

| AC | Spec-defined outcome | Evidence (`file:line` + assertion / UAT row + code) | Result |
| -- | -------------------- | --------------------------------------------------- | ------ |
| CAM-01 | Existing buttons unchanged + "Capture burst with camera" | `HomeScreen.tsx:149-153` button. Old-App diff shows nothing else changed. E row CAM-01 (`01-home.png`, L1 1280×960). | ✅ PASS (UAT) |
| CAM-02 | Android: camera screen with live back-camera preview | `App.tsx:35-43`. `CameraScreen.tsx:144` `getCameraDevice(factory, 'back')`, `<Camera isActive>` at `:174`. E row CAM-02. | ✅ PASS (UAT) |
| CAM-03 | 5/15/30, mutually exclusive, 5 default | `T:62` `expect([...FRAME_COUNTS]).toEqual([5, 15, 30])`, `T:63` `expect(DEFAULT_FRAME_COUNT).toBe(5)`. UI uses a single `count` state (`CameraScreen.tsx:69`, buttons `:185-192`). E row CAM-03. | ✅ PASS |
| CAM-04 | Exactly N snapshots, separate files, "i/N" updated after each frame | `T:75` takeFrame indices `0..N-1`. `T:78` progress `{captured:i+1,total:n}` for N∈{5,15,30}. **New** `T:107` `expect(log).toEqual(['frame0','progress1',…,'frame4','progress5','processing'])` (frame logged after an awaited tick, so the order is strict). **New** `T:134` failed frame k=2 → events exactly `[progress 1/5, progress 2/5]`. Files `CameraScreen.tsx:50`. Text `:212`. E row CAM-04. | ✅ PASS (M6 killed) |
| CAM-05 | `superResolve(paths)` once, capture order, default options, processing indicator | `T:86` `toHaveBeenCalledTimes(1)`. `T:87` `toEqual(pathsFor(0, n))`. `T:149` `['processing','superResolve']`. `CameraScreen.tsx:55` no options arg. `:214` "Processing…". E L2–L4. | ✅ PASS |
| CAM-06 | Preview with image, W×H, framesUsed, requested N, ms | `T:152-163` `toEqual({ok:true, run:{result, paths, requested:15, elapsedMs:250}})`. With the clock advancing in capture (+100/frame) and cleanup (+10), 250 holds only if the timer wraps `superResolve` alone. `PreviewScreen.tsx:26-32`. E row CAM-06 (`12-preview30.png`: 1992×2656, 30/30, 26120 ms). | ✅ PASS (M5 killed) |
| CAM-07 | **(reworded)** "Enhanced" / "Reference (frame 1)" toggle that switches between the enhanced result and `paths[0]`, at the same size | `PreviewScreen.tsx:34` `'Enhanced'` / `'[Enhanced]'`, `:39` `'Reference (frame 1)'` (bracketed when selected). `:17` `showReference ? file://${paths[0]} : result.path`. Same size: one `<Image>` with `aspectRatio: result.width / result.height` and `resizeMode="contain"` (`:47-48`) for both. E row CAM-07 (07/08 PNGs). | ✅ PASS (UAT). The spec-precision gap is closed. |
| CAM-08 | Preview back → camera ready; camera back while idle → home | `App.tsx:21-23`, `:48`, `:25-28` (guarded by `cameraBusy`), `:38`. E row CAM-08. | ✅ PASS (UAT) |
| CAM-09 | **(reworded)** Request if not asked; denied → text "Camera permission denied" + "Open settings", no capture controls | `CameraScreen.tsx:85-91` one request. `:152` `<Text>Camera permission denied</Text>`, `:153` `title="Open settings"`. The branch renders no Camera/count/capture. E row CAM-09 (`04-permission-denied.png` shows exactly that text). | ✅ PASS (UAT). The spec-precision gap is closed. |
| CAM-10 | Rejection message incl. `E_*` shown; no preview; new burst allowed | `T:173-177` `toEqual({ok:false, stage:'process', message:'E_ALIGNMENT_FAILED: …'})`. `T:228` new start not null. UI sets error = message and does not call `onResult`. E row CAM-10 (`13-rejection-on-camera.png`). | ✅ PASS |
| CAM-11 | Snapshot failure stops burst, no `superResolve`, error shown, new burst allowed | `T:195-203` `stage:'capture'`, `toHaveBeenCalledTimes(4)`, `superResolve).not.toHaveBeenCalled()`, idle. `T:134` no event for the failed frame. UI "Capture failed: …" (`CameraScreen.tsx:135`). E row CAM-11. | ✅ PASS |
| CAM-12 | No camera → "No camera available" instead of the preview | `CameraScreen.tsx:168` `!device` → "No camera available". **New** `:158-164`: factory error → `<Text>No camera available</Text>` + raw error below. Code review only (no device without a camera; disclosed in E). | ✅ PASS (code review only; the iteration-1 deviation is fixed) |
| CAM-13 | While busy: capture, counts, back disabled; one burst | `T:211` `isRunning()).toBe(true)`, `T:213` second start `toBeNull()`, `T:215` 5 frames. UI `CameraScreen.tsx:190`, `:197`, `:206` `disabled`. `App.tsx:27`. E row CAM-13. | ✅ PASS |
| CAM-14 | New burst deletes previous burst's files first | `T:238` first burst `toEqual([])`. `T:247-248` delete(prev) then take 0. `T:268` partial paths. `T:279` cleanup failure does not block. Module-scope runner (`CameraScreen.tsx:41-57`). E row CAM-14. | ✅ PASS |
| CAM-15 | Android compiles with VisionCamera; manifest CAMERA | Manifest diff. Build: E row CAM-15 (not re-run; iteration 2 changed only a test, one Text branch and docs). | ✅ PASS |
| CAM-16 | iOS plist key | Deferred (owner) | ⏭️ Deferred |
| CAM-17 | lint, typecheck, test pass | Verifier re-run: exit 0/0/0, 60 tests. | ✅ PASS |
| CAM-18 | Android stays portrait | Manifest `screenOrientation="portrait"`. E row CAM-18. | ✅ PASS |
| CAM-19 | iOS notice | Deferred (owner) | ⏭️ Deferred |

**Edge cases**
- [x] Next burst uses the newly selected N: `T:290` takeFrame called 35 times, `T:291` second call `toHaveLength(30)`.
- [x] Rotation mid-burst: CAM-18.
- [x] `framesUsed` < N shown: `PreviewScreen.tsx:29`. Not exercised on a device (the virtual scene always gives N).
- [x] Backgrounded mid-burst: E row CAM-11.

**Status**: ✅ All 17 in-scope ACs covered and matched to the spec outcome. No open spec-precision gaps.

---

## Discrimination Sensor (iteration 2)

Same procedure as iteration 1: each mutation was applied alone to `example/src/burst/burstRunner.ts`, `yarn test example/src/burst` was run, and the file was restored with `git checkout --`. A final `cmp` against a scratch copy matched, and `git status --short` was clean.

| # | File:line | Mutation | Result | Killing test(s) |
| - | --------- | -------- | ------ | --------------- |
| M5 | `burstRunner.ts:36` | `const start = deps.now()` moved to the top of `run` (times cleanup + capture) | ✅ Killed (1 failed) | CAM-06 outcome test `T:152` |
| M6 | `burstRunner.ts:32` | Progress event moved before `await deps.takeFrame(i)` | ✅ Killed (2 failed) | `T:92` order test, `T:122` failed-frame test |
| M1 | `burstRunner.ts:60` | Sync running guard removed | ✅ Killed (1 failed) | CAM-13 |
| M2 | `burstRunner.ts:19` | `deleteFiles(previousPaths)` → `deleteFiles([])` | ✅ Killed (2 failed) | CAM-14 |
| M3 | `burstRunner.ts:30` | Partial paths dropped on capture failure | ✅ Killed (1 failed) | CAM-14 partial |
| M4 | `burstRunner.ts:38` | Paths reversed before `superResolve` | ✅ Killed (3 failed) | CAM-04/05 |

Also tried and left out of the count: moving `start = deps.now()` one line up, before the synchronous `onEvent({type:'processing'})`. It survives, but it is an **equivalent mutant**: `onEvent` is synchronous and never touches the clock, so the change has no observable effect. It is not a test gap.

Iteration-1 kills M7 (running reset), M8 (off-by-one `captured`) and M9 (cleanup rethrow) were not re-run. Their tests are unchanged, apart from the clock offsets, which do not affect those assertions.

**Sensor depth**: lightweight+ (example app, not a P0 path)
**Result**: 6/6 killed (cumulative 9/9 distinct non-equivalent mutants killed across both iterations). ✅ PASS

---

## Code Quality

| Principle | Status |
| --------- | ------ |
| Minimum code | ✅ |
| Surgical changes | ✅ Fix commits touch only the test file, one JSX branch and the spec |
| No scope creep | ✅ |
| Matches patterns | ✅ |
| Spec-anchored outcome check | ✅ |
| Per-layer Coverage Expectation | ✅ Burst logic: every branch, plus ordering and timing semantics pinned down |
| Every test maps to a spec requirement | ✅ All 17 burst tests name a CAM id or the edge case |
| Documented guidelines followed | ✅ `CLAUDE.md`, `CONTRIBUTING.md` |

---

## Gate Check

- **Gate command**: `yarn lint && yarn typecheck && yarn test` (UI gate). The Android build was not re-run (instructed; nothing in the iteration-2 commits affects native config or deps).
- **Result**: lint exit 0, typecheck exit 0, test exit 0. 4 suites, **60 passed**, 0 failed, 0 skipped.
- **Test count before feature**: 43
- **Test count after feature**: 60 (iteration 1: 58, +2 from 4c2ef45)
- **Delta**: +17, all in `burstRunner.test.ts`
- **Skipped tests**: none
- **Failures**: none

---

## Residual notes (non-blocking)

1. CAM-12 is still verified by code review only, because no device without a camera was available. The evidence file discloses this.
2. `App.tsx:13-17`: the `cameraBusy` ref is updated in an effect after the busy render, so there is a tiny window in which hardware back could slip through. The UAT double-tap held.
3. The UAT used the AVD virtual scene, so framesUsed was always N. A real handheld burst was not exercised.

---

## Requirement Traceability Update

| Requirement | Previous Status | New Status |
| ----------- | --------------- | ---------- |
| CAM-01..11, 13, 14, 15, 17, 18 | Implemented (UAT PASS) / Needs Fix (CAM-04, CAM-06 in iteration 1) | ✅ Verified |
| CAM-12 | Implemented (code review only) | ✅ Verified (code review only) |
| CAM-16, CAM-19 | Deferred | Deferred |

---

## Summary

**Overall**: ✅ Ready

**Spec-anchored check**: 17/17 in-scope ACs matched the spec outcome. 0 open spec-precision gaps (CAM-07 and CAM-09 tightened in cdc85a6).
**Sensor**: 6/6 killed in iteration 2, including both iteration-1 survivors (M5, M6).
**Gate**: lint 0, typecheck 0, Jest 60/60.

**What works**: the full Android flow, with ordering and timing now protected by tests. Error, permission and no-camera paths match the spec text.

**Lessons**: L-007, L-008 and L-009 (recorded in iteration 1) stay as candidates. `lessons.md` has no resolve operation, and promotion to confirmed is automatic only when the same lesson recurs in another feature. This PASS adds no new signal, so nothing new is recorded.

**Next steps**: none required for this feature.
