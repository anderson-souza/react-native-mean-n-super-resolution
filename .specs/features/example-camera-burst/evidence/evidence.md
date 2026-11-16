# example-camera-burst — Android UAT evidence (T7)

- **Device**: Android emulator AVD `Pixel_6` (API 35, x86_64, 1080×2400), back camera = **virtual scene** (synthetic, not a real sensor). Owner choice.
- **Build**: `example/android` `gradlew app:installDebug -PreactNativeArchitectures=x86_64`, debug JS from Metro.
- **Dates**: 2026-11-13. Code under test: up to commit `859da98`, plus `d3d30e2` (permission fix) and `859da98` (runner lifetime + device memo fix), which were both found during this UAT.
- **Environment note**: `adb reverse` silently dropped connections (two adb versions on PATH restarting each other's server). The app's dev server host was set to `10.0.2.2:8081` through `shared_prefs/meannsuperresolution.example_preferences.xml` (`debug_http_host`). This is an emulator dev setting only; app code is unchanged.

## Results per requirement

| Req | Result | Evidence |
| --- | --- | --- |
| CAM-01 | PASS | `01-home.png`: existing buttons plus "CAPTURE BURST WITH CAMERA". `02-bundled-demo.png` and log line L1: the bundled 5-frame demo still gives 1280×960, framesUsed 5. |
| CAM-02 | PASS | `05-camera-idle.png`: live back-camera (virtual scene) preview. |
| CAM-03 | PASS | `05-camera-idle.png`: options 5 / 15 / 30, with `[5]` selected by default. `09-camera-15-selected.png`: 15 selected. |
| CAM-04 | PASS | `10-burst15-progress-{a,b,c}.png` show progress `0/15`, `4/15` and `9/15`. |
| CAM-05 | PASS | Log lines L2–L4: `superResolve` ran over N frames after capture, with framesUsed = N and the reference = frame `-0`. |
| CAM-06 | PASS | `07-preview5-enhanced.png`, `11-preview15.png` and `12-preview30.png`: each shows Output 1992×2656 (= 2 × the 996×1328 snapshot), `framesUsed: N / N requested` and the processing ms, for N = 5, 15 and 30. |
| CAM-07 | PASS | `07-preview5-enhanced.png` shows `[ENHANCED]` with the `superres-*.jpg` path; `08-preview5-reference.png` shows `[REFERENCE (FRAME 1)]` with the `burst-*-0.jpg` path. |
| CAM-08 | PASS | `14-back-to-camera.png`: on-screen Back goes from preview to camera, which is ready for a new burst. `15-back-to-home.png`: hardware Back goes from camera to home. |
| CAM-09 | PASS (after fix `d3d30e2`) | `04-permission-denied.png`: after "Don't allow", the screen shows "Camera permission denied" + "OPEN SETTINGS" and no capture controls. Before the fix, the screen stayed on "Requesting camera permission…" because Android re-reports `not-determined` after a first denial. |
| CAM-10 | PASS | `13-rejection-on-camera.png` + log line L5: `E_DECODE_FAILED: index=15 …` is shown on the camera screen and the preview does not open. Method (owner-approved): during a 30-frame burst, right after frame 29 was written, `run-as … rm cache/burst-<current id>-15.jpg` deleted one frame before decode. |
| CAM-11 | PASS | `17-background-mid-burst.png` + log line L6: HOME pressed 1.5 s into a 30-frame burst. The snapshot threw `Cannot take snapshot - PreviewView isn't ready yet!`, the burst stopped, `superResolve` was not called, "Capture failed: …" is shown and the controls are enabled again. No crash (0 `FATAL` lines). |
| CAM-12 | GAP (code review only) | Not reproducible on a device that has a camera. Branch `!device` → "No camera available" in `example/src/screens/CameraScreen.tsx`. Loading and no-camera are told apart by holding the device factory. |
| CAM-13 | PASS | `10-burst15-progress-*.png`: Back, the count options and Capture are all disabled while busy. Double tap (`input tap …; input tap …` in one shell call) on a 30-frame burst produced exactly one outcome line (L4). |
| CAM-14 | PASS (after fix `859da98`) | Burst 5 → preview → back → burst 15 left only the 15 files of the latest burst in `cache/` (`uniq -c` → `15 1794603444869`). Before the fix, files from every burst piled up, because each success unmounts the camera screen and the per-mount runner forgot `previousPaths`. |
| CAM-15 | PASS | `yarn turbo run build:android --force` was green at T1, T2 and T6. The manifest has `android.permission.CAMERA`. |
| CAM-17 | PASS | `yarn lint`, `yarn typecheck` and `yarn test` (58 tests) are green at every commit. |
| CAM-18 | PASS | `16-rotated-stays-portrait.png`: with `accelerometer_rotation=0` and `user_rotation=1` (landscape forced), `dumpsys` reports `ROTATION_0` / `port` and the UI stays portrait. Settings were restored afterwards (`1/0`). |
| CAM-16, CAM-19 | Deferred | iOS out of scope (owner, 2026-11-12). |

## Bugs found and fixed during UAT

1. `d3d30e2`: the permission screen got stuck after the first denial (CAM-09).
2. `859da98`: `getCameraDevice()` returns a new object on every render, so `<Camera>` reconfigured its session on every render. Logcat showed `CameraGraph-383` restarts, and preview readiness kept flipping, which disabled Capture. The fix memoizes the device; afterwards 0 `Camera graph updated` lines appeared over 6 s idle.
3. `859da98`: the runner lifetime was per-mount, so CAM-14 cleanup never happened across bursts.

## Log lines (`adb logcat -s ReactNativeJS`)

```
L1 [superResolve] {"result":{"path":"file:///data/user/0/meannsuperresolution.example/cache/superres-1794539193599-0-1245451988.jpg","width":1280,"height":960,"framesUsed":5},"elapsedMs":368,"ticksDuring":3}
L2 [camera-burst] ok N=5 out=1992x2656 framesUsed=5 ms=4612 ref=/data/user/0/meannsuperresolution.example/cache/burst-1794603426058-0.jpg
L3 [camera-burst] ok N=15 out=1992x2656 framesUsed=15 ms=8642 ref=/data/user/0/meannsuperresolution.example/cache/burst-1794603444869-0.jpg
L4 [camera-burst] ok N=30 out=1992x2656 framesUsed=30 ms=26120 ref=/data/user/0/meannsuperresolution.example/cache/burst-1794539717905-0.jpg
L5 [camera-burst] process failed: E_DECODE_FAILED: index=15 could not decode image: /data/user/0/meannsuperresolution.example/cache/burst-1794603789019-15.jpg
L6 [camera-burst] capture failed: java.lang.Error: Cannot take snapshot - PreviewView isn't ready yet!
```

L4 comes from the run before the runner-lifetime fix. That fix touched only where the runner lives, not the capture/process path. L2/L3 were re-run after the fix.

## Observations (not failures)

- The virtual scene is synthetic and nearly static, so framesUsed = N in every success. Real handheld bursts were not exercised.
- The frame count resets to 5 when coming back from the preview, because the camera screen remounts. The spec only requires "ready for a new burst".
- The "Capture failed" message includes the Java stack trace from VisionCamera (`17-background-mid-burst.png`). It is readable, but long.
