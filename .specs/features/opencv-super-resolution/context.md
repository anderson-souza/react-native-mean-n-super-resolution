# opencv-super-resolution Context

**Gathered:** 2026-11-11
**Spec:** `.specs/features/opencv-super-resolution/spec.md`
**Status:** Ready for design

---

## Feature Boundary

A single async library function that takes N local image files of the same scene, runs an OpenCV multi-frame pipeline (align → upscale → mean) in shared C++ on iOS and Android, writes the result to a file, and resolves with its path and metadata. Replaces the template `multiply` API.

---

## Implementation Decisions

### Input

- JS passes an array of local file paths / `file://` URIs. No base64.
- Native side decodes the files itself (OpenCV decoding).

### Output

- Native writes the result image to a file and resolves with its path (+ width, height, frames used).
- Caller may supply the output path; otherwise a file in the app's temp/cache directory is generated.

### Algorithm

- Pipeline: choose a reference frame, register every other frame to it, upscale, average pixelwise.
- Not "no alignment" and not "denoise only" — upscaling is part of the output.

### Threading

- Function returns a Promise. OpenCV work runs off the JS thread; JS/UI stays responsive during processing.

### Agent's Discretion

- Registration method (e.g. ECC vs feature-based), interpolation method, how OpenCV is distributed for each platform — Design decides.

### Declined / Undiscussed Gray Areas → Assumptions

Logged in spec.md "Assumptions & Open Questions": default scale factor and bounds, frame-count bounds, reference-frame choice, per-frame alignment failure policy, output encoding default, error format, web behavior, removal of `multiply`, verification strategy for native behavior.

---

## Specific References

No specific references — standard multi-frame "shift-and-add" style super resolution by averaging aligned, upscaled frames.

---

## Deferred Ideas

None — discussion stayed within feature scope.
