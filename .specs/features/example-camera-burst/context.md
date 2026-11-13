# example-camera-burst Context

**Gathered:** 2026-11-12
**Spec:** `.specs/features/example-camera-burst/spec.md`
**Status:** Ready for design (spec confirmed 2026-11-12)

---

## Feature Boundary

Example app only (`example/`). Keeps the existing bundled-asset demo and adds a camera flow: home → camera screen (pick 5/15/30, burst capture with `react-native-vision-camera`) → run `superResolve` → preview screen with the enhanced image. No change to the library API, C++ pipeline or JS wrapper.

---

## Implementation Decisions

### Navigation

- Simple state switch in `App.tsx` (`'home' | 'camera' | 'preview'`) — no React Navigation, no extra native navigation deps.

### Burst capture

- Use VisionCamera **preview snapshot** (`takeSnapshot`), taken back-to-back, not full `takePhoto`/`capturePhotoToFile`. Owner prefers speed over per-frame resolution.
- Consequence accepted: frames are preview-resolution, so output = 2 × preview size.

### Preview screen

- Shows the enhanced image + metrics (output width×height, `framesUsed`, frame count requested, processing time).
- Before/after comparison: reference frame (`paths[0]`) vs result.
- No zoom/pan, no save-to-gallery.

### Scale / validation platform

- `scale` fixed at default 2 (no selector).
- Runtime UAT on an Android device only; iOS gets config (Info.plist camera usage string, Podfile) but no runtime evidence (no Mac available).

### Agent's Discretion

- VisionCamera major version (v4 vs v5/Nitro) and how a snapshot becomes a local file — decided in Design after checking RN 0.88 / New Architecture compatibility.
- Before/after layout (toggle vs side-by-side).

### Declined / Undiscussed Gray Areas → Assumptions

Logged in spec.md "Assumptions & Open Questions". Owner confirmed 2026-11-12: back camera, back disabled while busy, processing on camera screen, cache-dir cleanup, and **portrait lock** (changed from "not locked"). Still agent defaults: frame count 5 preselected, permission-denied UX.

---

## Specific References

No specific references — standard approaches.

---

## Deferred Ideas

- Save enhanced image to gallery.
- Zoom/pan on the preview.
- Scale selector (1–4) with resolution-aware budget handling.
- Full-resolution `takePhoto` burst mode.
