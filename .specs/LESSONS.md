# LESSONS — auto-maintained by scripts/lessons.py

> Machine-owned. Do NOT hand-edit. Changes are overwritten on the next `lessons.py` write.
> Canonical state lives in `.specs/lessons.json`. Edit lessons only via the script.
> promote_threshold=2 distinct features · window_days=45 · quarantine_threshold=2

## Confirmed (load these at Specify/Design)

Corroborated across multiple features. Safe to apply as guidance.

_none_

## Candidates (under observation — do NOT load as guidance yet)

Seen once or not yet corroborated. Tracked, not trusted.

### L-001 — When frames can be skipped, assert output pixel values on the skip path, not only the used-frame count
- signal: `surviving_mutant` · recurrence: 1 feature(s) · scope: `native-pipeline` · harmful: 0
- features: opencv-super-resolution
- evidence: validation.md M8 cpp/sr/Pipeline.cpp:130 (native-pipeline)
- last seen: 2026-11-13T02:08:31Z

### L-002 — Assert encoder parameters the spec names, such as JPEG quality, not only the output file format
- signal: `surviving_mutant` · recurrence: 1 feature(s) · scope: `native-pipeline` · harmful: 0
- features: opencv-super-resolution
- evidence: validation.md M9 cpp/sr/Pipeline.cpp:43 (native-pipeline)
- last seen: 2026-11-13T02:08:32Z

### L-003 — Give every degraded path that changes the output (skips, fallbacks) an acceptance criterion with an exact expected value
- signal: `spec_precision_gap` · recurrence: 1 feature(s) · scope: `specs` · harmful: 0
- features: opencv-super-resolution
- evidence: spec.md Goals / SR-ERR-06 (specs)
- last seen: 2026-11-13T02:08:32Z

### L-004 — Name the harness that proves the TurboModule rejection path, since host tests do not compile the JSI adapter
- signal: `spec_precision_gap` · recurrence: 1 feature(s) · scope: `turbomodule` · harmful: 0
- features: opencv-super-resolution
- evidence: SR-ERR-08 cpp/MeanNSuperResolutionImpl.cpp:57 (turbomodule)
- last seen: 2026-11-13T02:08:33Z

### L-005 — Record platform runtime ACs as open evidence gaps until a run on that platform is captured
- signal: `ac_gap` · recurrence: 1 feature(s) · scope: `platforms` · harmful: 0
- features: opencv-super-resolution
- evidence: SR-CORE-06 (platforms)
- last seen: 2026-11-13T02:08:34Z

### L-006 — Confirm a native dependency's exception and abort behavior in the spike before designing on APIs that signal failure by throwing
- signal: `spec_deviation` · recurrence: 1 feature(s) · scope: `native-deps` · harmful: 0
- features: opencv-super-resolution
- evidence: cpp/sr/Align.cpp:8 (T6 SPEC_DEVIATION) (native-deps)
- last seen: 2026-11-13T02:08:34Z

## Quarantined (failed when applied — ignore)

A confirmed lesson that recurred alongside failure. Kept for the maintainer to review.

_none_
