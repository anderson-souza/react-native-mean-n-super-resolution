# legacy-arch-android Context

**Gathered:** 2026-11-15
**Spec:** `.specs/features/legacy-arch-android/spec.md`
**Status:** Ready for design

---

## Feature Boundary

`superResolve` works on Android in RN 0.71–0.72 apps with the Old Architecture, from the same npm package, with API and error parity, without regressing New Architecture users.

---

## Implementation Decisions

### Target

- Consumer app: RN 0.71–0.72, Android only, `newArchEnabled=false`.

### AD-001 conflict

- Amend AD-001 (do not replace): shared C++ pipeline stays the single implementation; a thin Old-Arch Android adapter (JVM module + JNI) is allowed.

### Distribution

- Single dual-arch package; no separate legacy package or branch.

### Evidence

- New repo test app `example-legacy/` with RN 0.72.x and `newArchEnabled=false`; Android emulator UAT.

### Agent's Discretion

- Kotlin vs Java for the adapter; exact 0.72 patch version in `example-legacy/`.

### Declined / Undiscussed Gray Areas → Assumptions

- Error shape on legacy (message parity + `error.code`), minimum RN 0.71.0, NDK r30 kept, `example-legacy/` on 0.72.x — all logged in spec Assumptions and confirmed by owner 2026-11-15.

---

## Specific References

- Existing New Arch error format `"E_CODE: detail"` (`src/errors.ts`, README).
- Windows AVD UAT workarounds from project memory apply to `example-legacy/`.

---

## Deferred Ideas

- iOS Old Architecture support.
- RN < 0.71 support.
- Checking the integration directly inside the target app.
