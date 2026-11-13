# example-camera-burst Specification

## Problem Statement

The example app only exercises `superResolve` with 5 synthetic bundled PNGs, so nobody can see what the library does to real camera input or try 15 and 30 frames. Adding a camera burst flow lets a developer evaluating the library capture a real scene on a device, run the pipeline and see the result next to the original frame.

## Goals

- [ ] From the example app, a user can capture a 5, 15 or 30 frame burst and see the enhanced result on screen with no manual file handling.
- [ ] The existing bundled-asset demo and the two device checks (missing file, concurrent) keep working unchanged.

## Out of Scope

| Feature | Reason |
| --- | --- |
| Library API, C++ pipeline or JS wrapper changes | Example-only feature; `superResolve` is used as-is. |
| Save result to gallery / share | Needs another lib + write permission (deferred). |
| Zoom/pan on preview | Android needs extra solution (deferred). |
| Scale selector | Fixed `scale: 2` (owner decision). |
| Full-resolution `takePhoto` burst | Owner chose preview snapshots for speed (deferred). |
| React Navigation / navigation library | Owner chose a state switch in `App.tsx`. |
| iOS runtime verification | No Mac available; iOS gets config only. |
| Camera burst on iOS | VisionCamera 5.2.4 `takeSnapshot()` throws `"takeSnapshot() is not available on iOS!"`; owner chose Android-only camera flow (2026-11-12). iOS shows a notice (CAM-19). |
| Video, flash, focus/exposure controls, front/back toggle | Not needed to demo the library. |

---

## Assumptions & Open Questions

| Assumption / decision | Chosen default | Rationale | Confirmed? |
| --- | --- | --- | --- |
| Capture method | Back-to-back VisionCamera preview snapshots (`takeSnapshot`), each saved as a local file | Owner choice (speed over resolution) | y |
| Navigation | `App.tsx` state switch `home → camera → preview` | Owner choice | y |
| Scale | Default `scale: 2` (option omitted) | Owner choice | y |
| UAT platform | Android device only; iOS config-only | Owner choice; no Mac | y |
| VisionCamera version | `react-native-vision-camera@5.2.4` + `react-native-nitro-modules` + `react-native-nitro-image` (see design.md) | Latest release; snapshot API checked in its published source | y |
| iOS camera flow | Android-only; iOS shows a notice instead of the camera | `takeSnapshot()` is Android-only in v5 | y |
| Camera device | Back camera | Standard for scene capture | y |
| Default frame count on camera screen | 5 selected | Fastest; matches existing demo | n |
| Permission denied | Camera screen shows a message and a button that opens system settings; no crash, no capture | Standard RN permission UX | n |
| Orientation | Example app locked to portrait on Android and iOS (CAM-18); any residual `E_SIZE_MISMATCH` is shown per CAM-10 | Rotation mid-burst would change snapshot size | y |
| Leaving camera mid-burst | Back button is disabled while capturing or processing | Avoids orphaned async work and state updates on an unmounted screen | y |
| Temp files | Snapshot files live in the app cache dir; files from the previous camera burst are deleted when a new burst starts | Bounded disk use without extra UI | y |
| Processing location | `superResolve` runs on the camera screen after the last frame; the preview opens only on success | Errors stay next to the retry action | y |

**Open questions:** none — all resolved or logged above. Remaining `n` rows: VisionCamera version (Design), default frame count 5 and permission-denied UX (agent defaults).

---

## User Stories

### P1: Burst capture and enhanced preview ⭐ MVP

**User Story**: As a developer evaluating the library, I want to capture a burst of 5, 15 or 30 frames with the phone camera and see the super-resolved result, so that I can judge the library on real input.

**Why P1**: This is the whole feature; without it the example stays synthetic.

**Acceptance Criteria**:

1. WHEN the home screen renders THEN it SHALL show the existing bundled-asset button and check buttons unchanged, plus a new button labelled "Capture burst with camera". (CAM-01)
2. WHEN the user taps "Capture burst with camera" on Android THEN the app SHALL show the camera screen with a live back-camera preview. (CAM-02)
3. WHEN the camera screen renders THEN it SHALL show three mutually exclusive frame-count options 5, 15 and 30, with 5 selected by default. (CAM-03)
4. WHEN the user taps capture with N selected THEN the app SHALL take exactly N snapshots back-to-back, each written as a separate local file, and SHALL show progress as "`i`/N" updated after each frame. (CAM-04)
5. WHEN all N frames are captured THEN the app SHALL call `superResolve(paths)` with those N paths in capture order (first captured = `paths[0]`) and default options, and SHALL show a processing indicator until it settles. (CAM-05)
6. WHEN `superResolve` resolves THEN the app SHALL show the preview screen with the enhanced image, output width×height, `framesUsed`, the requested N, and processing time in ms. (CAM-06)
7. WHEN the preview screen renders THEN it SHALL let the user compare the reference frame (`paths[0]`) with the enhanced image. (CAM-07)
8. WHEN the user taps back on the preview screen THEN the app SHALL return to the camera screen ready for a new burst; WHEN the user taps back on the camera screen while idle THEN the app SHALL return to the home screen. (CAM-08)

**Independent Test**: On an Android device: home → "Capture burst with camera" → pick 15 → capture → progress counts to 15/15 → preview shows image with output = 2 × frame size, `framesUsed` ≤ 15, and the reference frame for comparison.

---

### P1: Permissions and failure handling ⭐ MVP

**User Story**: As a developer, I want clear on-screen messages when the camera or the pipeline fails, so that the demo never crashes or hangs.

**Why P1**: Camera permission and pipeline errors are guaranteed to happen on first run and with handheld bursts.

**Acceptance Criteria**:

1. WHEN the camera screen opens and permission has not been asked THEN the app SHALL request camera permission; WHEN it is denied THEN the screen SHALL show a message and an "Open settings" button and SHALL NOT render capture controls. (CAM-09)
2. WHEN `superResolve` rejects THEN the camera screen SHALL show the rejection message (including its `E_*` code), SHALL NOT open the preview, and SHALL allow a new burst. (CAM-10)
3. WHEN a snapshot fails mid-burst THEN the app SHALL stop the burst, SHALL NOT call `superResolve`, SHALL show the error, and SHALL allow a new burst. (CAM-11)
4. WHEN no camera device is available THEN the camera screen SHALL show "No camera available" instead of the preview. (CAM-12)
5. WHILE a burst is capturing or processing THEN the capture button, frame-count options and back button SHALL be disabled, so a second burst or navigation cannot start. (CAM-13)
6. WHEN a new burst starts THEN the app SHALL delete the snapshot files of the previous camera burst before capturing. (CAM-14)

**Independent Test**: Deny camera permission → message + settings button shown, no crash. Grant it → capture while double-tapping capture → only one burst runs. Force a rejection (capture a featureless scene or shake the phone for `E_ALIGNMENT_FAILED`) → error shown, preview not opened, next burst works.

---

### P1: Platform setup

**User Story**: As a developer cloning the repo, I want the example to build with the camera dependency on Android and to carry the iOS config, so that the flow runs without manual setup.

**Why P1**: The feature is unusable if the example stops building.

**Acceptance Criteria**:

1. WHEN the Android example is built (`yarn turbo run build:android` / `yarn example android`) THEN it SHALL compile with `react-native-vision-camera` and the manifest SHALL declare `android.permission.CAMERA`. (CAM-15)
2. WHEN the iOS project is inspected THEN `Info.plist` SHALL contain a non-empty `NSCameraUsageDescription`. (CAM-16)
3. WHEN `yarn lint`, `yarn typecheck` and `yarn test` run at the repo root THEN they SHALL pass with the new example code included. (CAM-17)
4. WHEN the example app runs on Android or iOS THEN it SHALL stay in portrait orientation when the device rotates. (CAM-18)
5. WHEN the user taps "Capture burst with camera" on iOS THEN the app SHALL show a screen with the text "Burst capture is Android-only in this example" and a back button, and SHALL NOT mount the camera. (CAM-19)

**Independent Test**: CI-style Android build succeeds; grep shows the permission and plist key; lint/typecheck/test green.

---

## Edge Cases

- WHEN the device rotates mid-burst THEN the app SHALL stay portrait (CAM-18), so snapshot size does not change; any `E_SIZE_MISMATCH` still shown per CAM-10.
- WHEN the scene is featureless or the phone moves a lot THEN `superResolve` may reject with `E_ALIGNMENT_FAILED`, shown per CAM-10; when some frames are skipped, `framesUsed` < N is shown on the preview (CAM-06).
- WHEN the user switches frame count between bursts THEN the next burst SHALL use the newly selected N.
- WHEN the app is backgrounded mid-burst THEN a resulting snapshot failure SHALL be handled per CAM-11 (no crash).

---

## Requirement Traceability

| Requirement ID | Story | Phase | Status |
| --- | --- | --- | --- |
| CAM-01 | P1: Burst capture | Design | Pending |
| CAM-02 | P1: Burst capture | Design | Pending |
| CAM-03 | P1: Burst capture | Design | Pending |
| CAM-04 | P1: Burst capture | Design | Pending |
| CAM-05 | P1: Burst capture | Design | Pending |
| CAM-06 | P1: Burst capture | Design | Pending |
| CAM-07 | P1: Burst capture | Design | Pending |
| CAM-08 | P1: Burst capture | Design | Pending |
| CAM-09 | P1: Permissions & failures | Design | Pending |
| CAM-10 | P1: Permissions & failures | Design | Pending |
| CAM-11 | P1: Permissions & failures | Design | Pending |
| CAM-12 | P1: Permissions & failures | Design | Pending |
| CAM-13 | P1: Permissions & failures | Design | Pending |
| CAM-14 | P1: Permissions & failures | Design | Pending |
| CAM-15 | P1: Platform setup | Design | Pending |
| CAM-16 | P1: Platform setup | Design | Pending |
| CAM-17 | P1: Platform setup | Design | Pending |
| CAM-18 | P1: Platform setup | Design | Pending |
| CAM-19 | P1: Platform setup | Design | Pending |

**Coverage:** 19 total, 0 mapped to tasks, 19 unmapped (pre-Tasks).

---

## Success Criteria

- [ ] On an Android device, 5-, 15- and 30-frame bursts each reach the preview screen with output = 2 × snapshot size (device evidence: screenshots + log line per N).
- [ ] Permission-denied and one forced `superResolve` rejection are shown on screen with no crash (device evidence).
- [ ] Existing bundled-asset demo still produces 1280×960 output from the 5 bundled frames.
- [ ] Lint, typecheck, Jest and the Android example build pass.
