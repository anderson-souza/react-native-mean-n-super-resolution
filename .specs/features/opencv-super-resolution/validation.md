# opencv-super-resolution Validation

**Date**: 2026-11-12
**Spec**: `.specs/features/opencv-super-resolution/spec.md` (incl. amended SR-ERR-08 and large-frame edge case)
**Diff range**: `03f379c..962beac` (branch `feat/opencv-super-resolution`, 20 commits)
**Verifier**: independent sub-agent (author ≠ verifier), evidence-or-zero

## Verdict: FAIL ❌

The build gate is green and the main behavior is well covered, but **2 sensor mutants survived** (the mean divisor when a frame is skipped, and JPEG quality 95). Under the process, a surviving mutant is a fix task. Separately, iOS (SR-CORE-06) has no evidence at all. That gap is owner-accepted and stays open.

---

## Task Completion

| Task | Status | Notes |
| ---- | ------ | ----- |
| T0–T9, T12, T13 | ✅ Done | Commit hashes recorded in tasks.md |
| T6 | ✅ Done (SPEC_DEVIATION) | Alignment is translation-only (`phaseCorrelate` + NCC ≥ 0.85 gate) instead of ECC Euclidean. Rotation is not compensated. Source: `cpp/sr/Align.cpp:8-13`. Allowed by spec assumption "OpenCV exceptions" (owner decision 2026-11-11). |
| T10 | ⚠️ Partial | No Mac: iOS build/runtime not verified. Podspec Ruby syntax not checked. |
| T11 | ⚠️ Partial | No git remote: CI (`test-native`, `build-ios`, `.opencv` cache) has never run. |

---

## Gate Check (run by verifier)

| Step | Exit | Result |
| ---- | ---- | ------ |
| `yarn lint` | 0 | clean |
| `yarn typecheck` | 0 | clean |
| `yarn test` | 0 | 3 suites, **43 passed**, 0 failed, 0 skipped |
| `yarn test:native` | 0 | **44/44** ctest passed |
| `yarn prepare` | 0 | `lib/typescript/src/index.d.ts` exports `superResolve`, `ErrorCodes`, `SuperResolveOptions`, `SuperResolveResult`. `grep -rn multiply lib/` finds nothing. |

- Test count before the feature: 0 real tests (Jest had one `it.todo`, and there was no native harness). After: 43 Jest + 44 native. No tests were deleted. `multiply` and its todo test were removed as the spec requires.

---

## Spec-Anchored Acceptance Criteria

Test paths: Jest `src/__tests__/`, native `cpp/tests/`. "UAT" means the implementer's Android emulator evidence (Pixel_9a x86_64, debug). The verifier inspected it directly: `uat-2-result.png`, `uat-logcat.txt`, and `uat-output.jpg`, which `file` reports as JPEG 1280x960, 3 components.

### P1: Super-resolve

| AC | Spec outcome | Evidence (`file:line` — assertion) | Result |
| -- | ------------ | ---------------------------------- | ------ |
| SR-CORE-01 | Resolves `{path,width,height,framesUsed}`; `path` is a `file://` URI of an existing, decodable image | `pipeline_test.cpp:66` `EXPECT_EQ(res.path.rfind("file://",0),0u)`; `:68` `ASSERT_TRUE(fs::exists(outFs))`; `:69-71` width `2*kW`, height `2*kH`, framesUsed `5`; `:73-74` `imread` not empty. JS pass-through: `superResolve.test.ts:129` `resolves.toBe(nativeResult)`. UAT logcat: `{"path":"file:///data/user/0/…/cache/superres-….jpg","width":1280,"height":960,"framesUsed":5}` | ✅ All 4 payload fields asserted on value |
| SR-CORE-02 | Default scale gives 2× dims, both in the result and on disk | `pipeline_test.cpp:69-70` and `:75-76` (decoded `out.cols/rows == 2*kW/2*kH`); UAT 640×480 → 1280×960 | ✅ |
| SR-CORE-03 | Identical frames give `warpAffine` cubic upscale of the reference ±1, and `framesUsed == N` | `pipeline_test.cpp:79-87` `EXPECT_LE(maxDiff, 1.0)` against `warpAffine(ref,S,INTER_CUBIC,BORDER_REPLICATE)`; `:71` framesUsed 5 | ✅ |
| SR-CORE-04 | Shifts ≤ 5%: PSNR ≥ 30 dB in the common region, and higher than the unaligned average | `align_test.cpp:23` shifts within 10/8 px of 200×160; `:113` `EXPECT_GE(psnrAligned, 30.0)`; `:114` `EXPECT_GT(psnrAligned, psnrIdentity)`; `:57-67` common region | ✅ |
| SR-CORE-05 | JS timer fires before the Promise settles, for a run longer than 500 ms | UAT only (`example/src/App.tsx:57-84`): logcat `"elapsedMs":1129,"ticksDuring":9`, 100 ms interval. Android only, no automated test | ⚠️ Evidence: Android manual UAT only. iOS open. |
| SR-CORE-06 | iOS and Android both resolve per AC 1–3 | Android: UAT above. iOS: **no evidence** (no Mac, CI never ran) | ❌ Open evidence gap (owner-accepted) |
| SR-CORE-07 | No `outputPath` gives a JPEG with a unique name in the temp/cache dir; two concurrent calls give different paths and both files exist | `pipeline_test.cpp:239` `EXPECT_NE(r1.path, r2.path)`; `:242` exists; `:243` `fs::equivalent(parent, tempDir)`; `:244` JPEG magic; `paths_test.cpp:80-85` 1000 distinct `.jpg` names. Host `TempDirHost.cpp` stands in for the platform dir. UAT shows one default-path call into the app cache dir | ✅ (concurrency on device not exercised) |

### P1: Errors

| AC | Spec outcome | Evidence | Result |
| -- | ------------ | -------- | ------ |
| SR-ERR-01 | Non-array, wrong count, non-string or empty entry rejects `^E_INVALID_ARGUMENT` and native is not called | `superResolve.test.ts:34-35` `rejects.toThrow(/^E_INVALID_ARGUMENT/)` + `not.toHaveBeenCalled()`, used at `:39-70` (counts 0,1,4,6,14,16,29,31; string/null/undefined/object; `42`; `''`). C++ re-check: `validate_test.cpp:36-46` | ✅ |
| SR-ERR-02 | Scale outside {1,2,3,4} rejects `E_INVALID_ARGUMENT` | `superResolve.test.ts:74-77` (0, 5, 2.5, '2', -1); `validate_test.cpp:56-61`; `MeanNSuperResolutionImpl.cpp:18-23` maps non-integral to 0 | ✅ |
| SR-ERR-03 | Extension not in .jpg/.jpeg/.png (case-insensitive) rejects | `superResolve.test.ts:87-100` (.bmp, none, .jpg.txt, '', `/out.d/x`, non-string); accepts mixed case `:102-113`; `validate_test.cpp:72-77` | ✅ |
| SR-ERR-04 | Missing or undecodable file gives `E_DECODE_FAILED` with the index in the message | `pipeline_test.cpp:152-153` code + `find("index=2")`; `:163-164` text file `index=1` | ✅ |
| SR-ERR-05 | Size mismatch gives `E_SIZE_MISMATCH` with the first mismatching index | `pipeline_test.cpp:172-173` code + `index=3` | ✅ |
| SR-ERR-06 | Fewer than 2 frames usable gives `E_ALIGNMENT_FAILED`; skipped frames are excluded from the mean | `align_test.cpp:168` code; `:151` `framesUsed == 4` with one noise frame; `pipeline_test.cpp:298` | ✅ code and count. **Mutant M8 survived**: no test checks output pixels when a frame is skipped (see sensor) |
| SR-ERR-07 | Unwritable output gives `E_WRITE_FAILED` | `pipeline_test.cpp:181` (missing directory). The read-only directory case is not tested; the spec says "e.g." | ✅ |
| SR-ERR-08 (amended) | Non-`SrError` C++ exception, or pre-flight estimate > 1 GiB, gives `E_INTERNAL` and no crash | Mapping: `error_mapping_test.cpp:28` `"E_INTERNAL: out of memory"`, `:32-33`, `:37`, `:41`. Pre-flight: `pipeline_test.cpp:251-253` exact estimates, `:267-269` `E_INTERNAL` + `exceeds budget` + no output file. **Adapter wiring** (`MeanNSuperResolutionImpl.cpp:57-58`, worker `catch(...)` leading to `promise.reject`) is covered by no automated test and was not exercised on device | ⚠️ Partial: logic covered, TurboModule rejection path unevidenced |
| SR-ERR-09 | Web rejects with the "only supported on native platforms" message | `web.test.ts:166-171` exact message | ✅ |

### P2: Configure / Docs

| AC | Spec outcome | Evidence | Result |
| -- | ------------ | -------- | ------ |
| SR-CFG-01 | Scale s ∈ 1..4 gives s× dims, both in the result and on disk | `pipeline_test.cpp:93-103` (s=1,3,4; result + decoded) and `:69-76` (s=2) | ✅ |
| SR-CFG-02 | Written exactly at `outputPath` (absolute or `file://`), overwrites, `path` = its `file://` URI | `pipeline_test.cpp:118` `res.path == toFileUri(outputPath)`; `:127-130` `file://` with a space; `:137-144` stale file overwritten (PNG magic + dims) | ✅ |
| SR-CFG-03 | `.png` writes PNG; `.jpg`/`.jpeg` writes JPEG **quality 95** | `pipeline_test.cpp:112-119` magic bytes for png/jpg/jpeg/Jpg/PNG. **Quality 95 is not asserted; mutant M9 (95→50) survived** | ❌ Conjunct untested |
| SR-DOC-01 | `lib/typescript` exports `superResolve`, `SuperResolveOptions`, `SuperResolveResult`; no `multiply` | `index.test.ts:147-157` (source); verifier inspected `lib/typescript/src/index.d.ts` after `yarn prepare` (all three exported); `grep multiply lib/` finds nothing | ✅ |
| SR-DOC-02 | README has install (incl. OpenCV per platform), usage, options table, result shape, error codes | `README.md:25-51` install/OpenCV/`OPENCV_MOBILE_DIR`, `:72-73` usage, `:99-100` options, `:107-109` result, `:138-143` codes, `:145-148` memory/crash risk | ✅ (minor inaccuracy: `:138` says URI-scheme rejection runs in JS, but it happens natively in `Paths.cpp:64-67`) |

**Status**: 17/21 ACs fully matched. Not matched: SR-CORE-06 (iOS evidence gap), SR-CFG-03 (quality conjunct), SR-ERR-08 (adapter wiring), SR-CORE-05 (manual Android-only).

### Spec-precision gaps

1. **Mean with skipped frames.** The Goal says "pixelwise mean of the aligned, upscaled frames", but the only AC that pins output pixel values is SR-CORE-03, which uses identical frames and therefore never skips. No AC defines the expected output when `framesUsed < N`. That is why M8 could survive.
2. **SR-ERR-08 "app SHALL NOT crash"** cannot be checked on the host. The spec does not say which harness proves the TurboModule rejection path.
3. **EXIF edge case** ("OpenCV's default decode behavior") depends on the distribution. There is no test. The only evidence is the source-level spike note (design.md "EXIF orientation", opencv-mobile `highgui.cpp:284-296`).

---

## Edge Cases

- [x] Same path twice treated as separate frames: `pipeline_test.cpp:206-211` framesUsed 5
- [x] Grayscale/alpha produce 3-channel 8-bit output: `pipeline_test.cpp:199-202`
- [x] `scale = 1` keeps the input dims: `pipeline_test.cpp:93-103` (s=1)
- [x] `file://` percent-decoding: `paths_test.cpp:30-31`, `pipeline_test.cpp:123-131`; mutant M10 killed
- [ ] EXIF orientation applied consistently: **no test**; source-level evidence only (design.md spike). Unevidenced.
- [x] Very large frames: pre-flight after the reference decode, before the accumulator (`Pipeline.cpp:90-99`); `pipeline_test.cpp:256-286` rejects at s=4 and resolves at s=1; mutant M6 killed
- [x] Extra: degenerate 1-row frames reject instead of aborting (`pipeline_test.cpp:288-299`)

---

## Discrimination Sensor (P0-depth, 12 manual mutations)

Run in the scratch worktree `git worktree add <scratchpad>/verify-wt HEAD`, using `OPENCV_MOBILE_DIR=.opencv/4.14.0/windows`. Baseline before mutating: 44/44 passed. Each mutation was reverted with `git checkout` after its run. The worktree was then removed; the real tree was never modified.

| # | File:line | Mutation | Killed? (by) |
| - | --------- | -------- | ------------ |
| M1 | `src/validate.ts:25` | Drop the empty-string check (`p === ''`) | ✅ Jest (1 failed) |
| M2 | `src/validate.ts:3` | Accept 6 frames (`[5, 6, 15, 30]`) | ✅ Jest (1 failed) |
| M3 | `cpp/sr/Pipeline.cpp:124` | `framesUsed < 2` → `< 1` | ✅ `DegenerateOneRowFrames…`, `FailsWhenFewerThanTwoFramesAlign` |
| M4 | `cpp/sr/Align.cpp:19` | Skip policy off (`kMinCorrelation` → -2.0) | ✅ `SkipsOneUnrelatedFrame`, `FailsWhenFewerThanTwoFramesAlign` |
| M5 | `cpp/sr/Align.cpp:91` | Translation sign flipped | ✅ `ShiftedFramesBeatIdentity…`, `EstimatesIntegerTranslation…` |
| M6 | `cpp/sr/Pipeline.cpp:92` | Memory budget doubled | ✅ `OverBudgetReferenceRejectsInternal…` |
| M7 | `cpp/sr/ErrorMapping.cpp:17` | `bad_alloc` mapped to `E_WRITE_FAILED` | ✅ `BadAllocBecomesInternal` |
| M8 | `cpp/sr/Pipeline.cpp:130` | Mean divisor `1/framesUsed` → `1/paths.size()` (skipped frame darkens the output) | ❌ **Survived** (44/44 pass) |
| M9 | `cpp/sr/Pipeline.cpp:43` | JPEG quality 95 → 50 | ❌ **Survived** (44/44 pass) |
| M10 | `cpp/sr/Paths.cpp:62` | Percent-decoding removed | ✅ `PercentDecodesFileUri`, `RoundTrips…` |
| M11 | `cpp/sr/Pipeline.cpp:101` | Size-mismatch check removed | ✅ `DifferentSizeFrameFailsWithIndex` |
| M12 | `cpp/sr/Pipeline.cpp:114` | Never skip unaligned frames | ✅ `SkipsOneUnrelatedFrame`, `DegenerateOneRowFrames…` |

**Result**: 10/12 killed. **FAIL ❌**

Structural note: `cpp/MeanNSuperResolutionImpl.cpp` (worker thread, `toScale`, `catch(...)` → reject) is not compiled by any automated test target, so any mutation there would survive the gate. The only evidence for it is the Android UAT happy path.

---

## Code Quality

| Principle | Status |
| --------- | ------ |
| Minimum code / no scope creep | ✅ Modules match the design (Validate/Paths/Pipeline/Align/ErrorMapping) |
| Surgical changes | ✅ Example-only gradle path normalization is documented (T9 deviation) |
| Matches patterns | ✅ Comment-prefix rule (`why:`/`hazard:`/`invariant:`) followed |
| Spec-anchored asserted values | ⚠️ Two exceptions: SR-CFG-03 quality and the skipped-frame mean |
| Per-layer coverage | ⚠️ The TurboModule adapter layer has no automated coverage |
| Every test maps to a requirement | ✅ Extra tests map to T7b/AD-004 guards (degenerate frames, determinism assumption) |
| Documented guidelines | CLAUDE.md + tlc-spec-driven coding principles |

---

## Open Evidence Gaps (owner-accepted, not passed)

1. **No git remote, so CI never ran**: the `test-native` job (ubuntu), `build-ios`, `build-android` on CI, and the `.opencv` cache keyed on `opencv.json`.
2. **No Mac**: iOS pod/xcframework build and runtime are unverified (SR-CORE-06; SR-CORE-01..03 and 05 on iOS). Podspec Ruby syntax is unchecked.
3. **Android NDK floor**: `ndk29-floor.log` shows the intended fail-fast ("requires Android NDK r30+"). r30 build success is in `t9/turbo-build-android.log` (EXIT=0).
4. **Android device**: no evidence of an `E_*` rejection reaching JS through the TurboModule, and no concurrent default-path calls on device.

---

## Fix Plans

### Fix 1 (Major): assert the mean when a frame is skipped (M8)
- **Root cause**: `align_test.cpp:142-153` checks only `framesUsed` and that the file exists.
- **Fix task**: In `SkipsOneUnrelatedFrame` (or a new test with identical frames plus one noise frame), compare the output against the mean of the 4 used frames. With 4 identical frames plus noise, the output should equal the upscaled reference within ±1. This kills M8.
- **Priority**: Major (core output correctness on the skip path).

### Fix 2 (Minor): assert JPEG quality 95 (M9)
- **Fix task**: In `pipeline_test.cpp`, check that the encoded bytes equal `cv::imencode(".jpg", expectedMean, {IMWRITE_JPEG_QUALITY, 95})`, or compare file size / decode error against a q=95 reference encode. This kills M9.

### Fix 3 (Minor): README `:138` URI-scheme wording
- Say that the unsupported-scheme rejection is native (`E_INVALID_ARGUMENT` from `toFsPath`), not a JS pre-check.

### Fix 4 (Owner decision, optional): adapter-path evidence for SR-ERR-08
- Add an example-app UAT step (e.g. a missing file) that shows `[superResolve] error E_DECODE_FAILED…` in logcat without a crash.

---

## Requirement Traceability Update (proposed)

| Requirement | New status |
| ----------- | ---------- |
| SR-CORE-01..04, SR-CORE-07, SR-ERR-01..05, 07, 09, SR-CFG-01, 02, SR-DOC-01, 02 | ✅ Verified (Android/host) |
| SR-ERR-06 | ❌ Needs Fix (M8) |
| SR-CFG-03 | ❌ Needs Fix (M9) |
| SR-ERR-08 | ⚠️ Partial (adapter unevidenced) |
| SR-CORE-05 | ⚠️ Verified on Android UAT only |
| SR-CORE-06 | ⏸ Blocked: iOS unverifiable without a Mac |

## Summary

**Overall**: ❌ Not Ready. 2 surviving mutants; iOS evidence gap.
**Spec-anchored check**: 17/21 ACs matched; 3 spec-precision gaps.
**Sensor**: 10/12 killed.
**Gate**: lint/typecheck/prepare exit 0; Jest 43/43; native 44/44.
