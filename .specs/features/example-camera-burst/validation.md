# example-camera-burst Validation

**Date**: 2026-11-13
**Spec**: `.specs/features/example-camera-burst/spec.md`
**Diff range**: `5fe938c^..HEAD` (5fe938c spec commit through 80d6f83) on `feat/example-camera-burst`
**Verifier**: independent sub-agent (author ≠ verifier)

**Verdict**: ❌ FAIL. 2 discrimination-sensor mutants survived in `burstRunner.ts`, the unit-tested layer. Every in-scope AC is implemented and has evidence, and every gate is green. The FAIL comes only from the weak tests. Two small fix tasks cover it (see Fix Plans).

---

## Task Completion

| Task | Status | Notes |
| ---- | ------ | ----- |
| T1 deps + Android build | ✅ Done | `example/package.json`: deps pinned exactly (`5.2.4`, `0.37.2`, `0.15.2`, no `^`/`~`). Build log: evidence.md CAM-15 row (not re-run by Verifier). |
| T2 manifest | ✅ Done | `AndroidManifest.xml`: `android.permission.CAMERA` and `android:screenOrientation="portrait"` on MainActivity (diff checked). |
| T3 burstRunner + tests | ⚠️ Done, weak tests | 15 tests. 2 mutants survive (see Sensor). |
| T4 HomeScreen split | ✅ Done | `diff` of old `App.tsx` against `screens/HomeScreen.tsx` shows only the asset `require` path depth, the component name/prop and the new button. Nothing else changed. |
| T5 CameraScreen | ✅ Done | — |
| T6 PreviewScreen | ✅ Done | — |
| T7 UAT evidence | ✅ Done | AVD virtual scene. CAM-12 by code review only (disclosed). |

---

## Spec-Anchored Acceptance Criteria

Layer per the Test Coverage Matrix: burst logic = unit (Jest), screens = UAT-only, native config = build + static. Test file: `example/src/burst/__tests__/burstRunner.test.ts` (abbreviated `T` below). Evidence file: `evidence/evidence.md` (abbreviated `E`). The Verifier also looked at downscaled copies of PNGs 04, 10b, 12 and 13, and they match E.

| AC | Spec-defined outcome | Evidence (`file:line` + assertion / UAT row + code) | Result |
| -- | -------------------- | --------------------------------------------------- | ------ |
| CAM-01 | Existing buttons unchanged + "Capture burst with camera" | `HomeScreen.tsx:149-153` `<Button title="Capture burst with camera" …>`. Diff against old App shows nothing else changed. E row CAM-01 (`01-home.png`; L1 bundled demo 1280×960). | ✅ PASS (UAT) |
| CAM-02 | Android: camera screen with live back-camera preview | `App.tsx:35-43` camera branch. `CameraScreen.tsx:144-146` `getCameraDevice(factory,'back')`, `<Camera isActive>` at `:169-177`. E row CAM-02 (`05-camera-idle.png`). | ✅ PASS (UAT) |
| CAM-03 | Options 5/15/30, mutually exclusive, 5 default | `T:58` `expect([...FRAME_COUNTS]).toEqual([5, 15, 30])`, `T:59` `expect(DEFAULT_FRAME_COUNT).toBe(5)`. UI: `CameraScreen.tsx:69` `useState(DEFAULT_FRAME_COUNT)`, `:180-187` single `count` state. E row CAM-03. | ✅ PASS |
| CAM-04 | Exactly N snapshots, separate files, progress "i/N" after each frame | `T:71-73` takeFrame indices `0..N-1`. `T:74-81` events `{progress, captured:i+1, total:n}` for N∈{5,15,30}. Files: `CameraScreen.tsx:50` `burst-${burstId}-${i}.jpg`. Text: `CameraScreen.tsx:206-207` `${captured}/${total}`. E row CAM-04 (`10-burst15-progress-*`, 4/15 seen). | ⚠️ PASS with weak test. The "after each frame" ordering is not asserted (mutant M6 survived). |
| CAM-05 | `superResolve(paths)` once, capture order, default options, processing indicator | `T:82` `toHaveBeenCalledTimes(1)`. `T:83` `superResolve.mock.calls[0]?.[0]).toEqual(pathsFor(0, n))`. `T:97` `order.slice(-2)).toEqual(['processing','superResolve'])`. Default options: `CameraScreen.tsx:55` `superResolve(paths)` with no options arg. Indicator: `:208-209` "Processing…". E L2–L4. | ✅ PASS |
| CAM-06 | Preview with image, W×H, framesUsed, requested N, ms | Outcome data: `T:103-111` `toEqual({ok:true, run:{result, paths, requested:15, elapsedMs:250}})`. UI: `PreviewScreen.tsx:26-32` (Output W×H, `framesUsed: k / N requested`, `Processing: X ms`). E row CAM-06 (`12-preview30.png` checked: 1992×2656, 30/30, 26120 ms). | ⚠️ PASS with weak test. "elapsedMs = superResolve only" (design / T3 Done-when) is not discriminated (mutant M5 survived). |
| CAM-07 | User can compare reference (`paths[0]`) with enhanced | `PreviewScreen.tsx:17` `showReference ? file://${paths[0]} : result.path`, toggle at `:33-43`. E row CAM-07 (07/08 PNGs). | ✅ PASS (UAT). ⚠️ Spec-precision gap: "let the user compare" names no mechanism. |
| CAM-08 | Preview back → camera ready; camera back while idle → home | `App.tsx:21-23` and `:48` preview→camera. `:25-28` hardware back to home only if `!cameraBusy.current`. `:38` on-screen Back → home. E row CAM-08 (14/15 PNGs). | ✅ PASS (UAT) |
| CAM-09 | Request if not asked; denied → message + "Open settings", no capture controls | `CameraScreen.tsx:85-91` requests once. `:149-155` denied branch renders text + `Open settings` only, with no Camera/count/capture. E row CAM-09 (`04-permission-denied.png` checked). | ✅ PASS (UAT). ⚠️ Spec-precision gap: message text not specified (implemented as "Camera permission denied"). |
| CAM-10 | Rejection message incl. `E_*` shown; no preview; new burst allowed | `T:121-125` `toEqual({ok:false, stage:'process', message:'E_ALIGNMENT_FAILED: only 1 of 5 frames aligned'})`. `T:174-176` idle again, `start` not null. UI: `CameraScreen.tsx:128-137` sets error text = message and does not call `onResult`. E row CAM-10 (`13-rejection-on-camera.png` checked: `E_DECODE_FAILED: index=15 …`). | ✅ PASS |
| CAM-11 | Snapshot failure stops burst, no `superResolve`, error shown, new burst allowed | `T:143-151` `stage:'capture'`, `toHaveBeenCalledTimes(4)` (k=3), `superResolve).not.toHaveBeenCalled()`, no `processing`, `isRunning()` false. UI: `CameraScreen.tsx:134-135` "Capture failed: …". E row CAM-11 (`17-background-mid-burst.png`, L6). | ✅ PASS |
| CAM-12 | No camera → "No camera available" instead of preview | `CameraScreen.tsx:162-163` `!device` → `No camera available`. Code review only (E row CAM-12 discloses this). Note: if `createDeviceFactory()` rejects, `:158-159` shows the raw error text, not "No camera available". | ⚠️ PASS by code review only. Minor deviation on the factory-error path. |
| CAM-13 | While busy: capture, counts, back disabled; no second burst/navigation | `T:158-163` `isRunning()` true, second `start` `toBeNull()`, takeFrame called 5 times. UI: `CameraScreen.tsx:185` counts `disabled={busy}`, `:192` capture `disabled={!ready \|\| busy}`, `:201` Back `disabled={busy}`. `App.tsx:27` hardware back guarded. E row CAM-13 (`10-burst15-progress-b.png` checked: all greyed out; one outcome line for a double tap). | ✅ PASS |
| CAM-14 | New burst deletes previous burst's files before capturing | `T:186` first burst `toEqual([])`. `T:195-196` `calls[0]` = delete(prev 5 paths), `calls[1]` = take 0. `T:216` partial paths `['/cache/p0.jpg','/cache/p1.jpg']`. `T:227-228` cleanup failure does not block. Across mounts: runner at module scope, `CameraScreen.tsx:41-57`. E row CAM-14 (one `uniq -c` group left). | ✅ PASS |
| CAM-15 | Android compiles with VisionCamera; manifest has CAMERA | Manifest diff: `<uses-permission android:name="android.permission.CAMERA" />`. Build: E row CAM-15 (not re-run by Verifier; nothing in the diff raises doubt). | ✅ PASS |
| CAM-16 | iOS plist key | Deferred (owner, iOS out of scope) | ⏭️ Deferred |
| CAM-17 | lint, typecheck, test pass | Verifier re-ran all three: exit 0 / 0 / 0, 58 tests (see Gate Check). | ✅ PASS |
| CAM-18 | Android stays portrait | Manifest diff: `android:screenOrientation="portrait"`. E row CAM-18 (`dumpsys` ROTATION_0 with landscape forced). | ✅ PASS |
| CAM-19 | iOS notice | Deferred (owner, iOS out of scope) | ⏭️ Deferred |

**Edge cases**
- [x] Next burst uses the newly selected N: `T:238-239` takeFrame called 35 times in total, `superResolve.mock.calls[1]` length 30.
- [x] Rotation mid-burst: covered by CAM-18 (portrait lock).
- [x] `framesUsed` < N shown: `PreviewScreen.tsx:29` renders `framesUsed / requested`. Not exercised on a device, because the virtual scene always gave framesUsed = N (E Observations).
- [x] Backgrounded mid-burst → capture failure handled: E row CAM-11.

**Status**: all 17 in-scope ACs have evidence. 2 ACs (CAM-04, CAM-06) have unit assertions that do not discriminate a required behavior. 2 minor spec-precision gaps (CAM-07, CAM-09 message text). CAM-12 is verified by code review only.

---

## Discrimination Sensor

Target: `example/src/burst/burstRunner.ts`. Each mutation was applied alone to the working file, `yarn test example/src/burst` was run, and the file was restored with `git checkout -- example/src/burst/burstRunner.ts`. Afterwards `cmp` against a scratch copy of the original matched.

| # | File:line | Mutation | Result |
| - | --------- | -------- | ------ |
| M1 | `burstRunner.ts:60` | Removed the sync guard `if (running) return null;` | ✅ Killed (1 failed) |
| M2 | `burstRunner.ts:19` | `deleteFiles(previousPaths)` → `deleteFiles([])` | ✅ Killed (2 failed) |
| M3 | `burstRunner.ts:30` | Partial paths dropped on capture failure (`paths.length = 0` before return) | ✅ Killed (1 failed) |
| M4 | `burstRunner.ts:38` | `superResolve([...paths].reverse())` | ✅ Killed (3 failed) |
| M5 | `burstRunner.ts:36` | `const start = deps.now()` moved to the start of `run`, so elapsedMs includes cleanup + capture | ❌ **Survived** (15/15 pass) |
| M6 | `burstRunner.ts:32` | Progress event moved before `await deps.takeFrame(i)` (emitted before the frame resolves) | ❌ **Survived** (15/15 pass) |
| M7 | `burstRunner.ts:53` | Removed `running = false` in `finally` | ✅ Killed (6 failed) |
| M8 | `burstRunner.ts:32` | `captured: i + 1` → `captured: i` | ✅ Killed (3 failed) |
| M9 | `burstRunner.ts:22` | Cleanup error rethrown instead of logged | ✅ Killed (1 failed) |

**Why the survivors survive**
- M5: in `setup()` the fake clock moves only inside `superResolve` (`T:28`, `clock += 250`). `takeFrame` and `deleteFiles` never move it, so `elapsedMs` is 250 wherever `start` is sampled.
- M6: tests check the event sequence (`T:74-81`) and the takeFrame calls (`T:71`) separately and never check how the two interleave. In the CAM-11 test (`T:150`) only the absence of `processing` is checked, so an extra `progress` for the failed frame also goes unnoticed.

**Sensor depth**: lightweight+ (9 mutations; the example app is not a P0 path)
**Result**: 7/9 killed. ❌ FAIL

---

## Code Quality

| Principle | Status |
| --------- | ------ |
| Minimum code | ✅ |
| Surgical changes | ✅ HomeScreen move is verbatim (diff checked); only the manifest's two lines changed |
| No scope creep | ✅ No library/C++/wrapper changes in the diff |
| Matches patterns | ✅ Test style follows `src/__tests__/superResolve.test.ts` (Jest, fake deps, exact message asserts) |
| Spec-anchored outcome check | ⚠️ CAM-04 ("after each frame") and CAM-06 elapsedMs scope not discriminated |
| Per-layer Coverage Expectation | ⚠️ Burst logic "all branches": branches are covered, but two ordering/timing semantics are not pinned down |
| Every test maps to a spec requirement | ✅ All 15 tests carry a CAM id or the edge case in their name |
| Documented guidelines followed | ✅ `CLAUDE.md`, `CONTRIBUTING.md` |

Observations (not failures):
- `App.tsx:13-17`: `cameraBusy` is a ref updated in a `useEffect` after the busy render, so a hardware back press in the same frame as the Capture tap could slip through. The window is very small, and the UAT double-tap test held.
- Frame count resets to 5 after returning from the preview (remount). The spec only requires "ready for a new burst".

---

## Gate Check

- **Gate command**: `yarn lint && yarn typecheck && yarn test` (UI gate). The Build-gate Android build (`yarn turbo run build:android --force`) was not re-run by the Verifier, as instructed. It relies on E row CAM-15 (green at T1/T2/T6), and nothing in the diff raises doubt.
- **Result**: lint exit 0, typecheck exit 0, test exit 0. 4 suites, **58 passed**, 0 failed, 0 skipped.
- **Test count before feature**: 43
- **Test count after feature**: 58
- **Delta**: +15 (all in `example/src/burst/__tests__/burstRunner.test.ts`; count matches the file's 1 + 3 + 4 + 2 + 4 + 1 cases)
- **Skipped tests**: none
- **Failures**: none

---

## Fix Plans

### Fix 1: elapsedMs scope not discriminated (M5)
- **Root cause**: the fake clock in `setup()` advances only inside `superResolve`.
- **Fix task**: in `burstRunner.test.ts`, advance the fake clock inside `takeFrame` (and/or `deleteFiles`), for example `clock += 10` per frame, and keep asserting `elapsedMs: 250` in the CAM-06 test.
- **Verify**: re-apply M5. The CAM-06 test must fail.
- **Priority**: Major (the asserted outcome is design-specified)

### Fix 2: progress-after-frame ordering not discriminated (M6)
- **Root cause**: events and takeFrame calls are recorded separately, so their interleaving is never checked.
- **Fix task**: record `take i` (on resolve) and `progress` into a single ordered log and assert a strict alternation `take0, progress1, take1, progress2, …, processing`. Also assert in the CAM-11 test that the events are exactly `progress` 1..k with no event for the failed frame.
- **Verify**: re-apply M6. At least one test must fail.
- **Priority**: Major (CAM-04 says "updated after each frame")

### Fix 3 (optional, Minor): CAM-12 factory-error path
- `CameraScreen.tsx:158-159` shows the raw error when `createDeviceFactory()` rejects. Either show "No camera available" there too, or record in the spec that a factory error is shown verbatim.

---

## Requirement Traceability Update

| Requirement | Previous Status | New Status |
| ----------- | --------------- | ---------- |
| CAM-01, 02, 03, 05, 07, 08, 09, 10, 11, 13, 14, 15, 17, 18 | Implemented (UAT PASS) | ✅ Verified |
| CAM-04 | Implemented (UAT PASS) | ❌ Needs Fix (test strength, Fix 2) |
| CAM-06 | Implemented (UAT PASS) | ❌ Needs Fix (test strength, Fix 1) |
| CAM-12 | Implemented (code review only) | ⚠️ Verified by code review only |
| CAM-16, CAM-19 | Deferred | Deferred |

---

## Summary

**Overall**: ❌ Not Ready. The behavior is correct; the problem is the strength of two tests.

**Spec-anchored check**: 17/17 in-scope ACs have evidence. 2 assertions are not discriminating (CAM-04, CAM-06). 2 minor spec-precision gaps (CAM-07, CAM-09 message text).
**Sensor**: 7/9 mutations killed (M5, M6 survived).
**Gate**: lint 0, typecheck 0, Jest 58/58.

**What works**: the full Android flow (home → camera → 5/15/30 burst → preview → back). Permission denial, rejection and capture-failure handling. The single-run guard. Cleanup across mounts. Portrait lock. The bundled demo is unchanged.

**Issues found**: M5 and M6 surviving mutants (Fixes 1–2). CAM-12 factory-error text (Fix 3, optional).

**Next steps**: route Fixes 1–2 to an implementer (test-only changes in `burstRunner.test.ts`), then re-verify by re-running M5/M6.
