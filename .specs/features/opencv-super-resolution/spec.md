# opencv-super-resolution Specification

**Scope:** Complex (new domain: native OpenCV integration on two platforms + async C++ TurboModule).

## Problem Statement

The library promises multi-frame super resolution with OpenCV but ships only the template `multiply` method and has no OpenCV in either native build. App developers need one call that turns a burst of N photos of the same scene into a single higher-resolution, lower-noise image, without blocking the JS thread.

## Goals

- [ ] `superResolve(paths, options?)` available on iOS and Android, returning a Promise that resolves with a written image file.
- [ ] Output is `scale ×` the reference frame's dimensions and is the pixelwise mean of the aligned, upscaled frames.
- [ ] Invalid input is rejected with a stable, documented error code instead of a crash.

## Out of Scope

| Feature | Reason |
| ------- | ------ |
| Base64 / in-memory image input or output | Decided: file paths in, file path out (context.md) |
| Web support | Library is native-only; web keeps throwing "only supported on native platforms" |
| Learned/DNN super resolution (`dnn_superres`, ESPCN, etc.) | Feature is mean-of-N classical pipeline |
| Cancellation / progress callbacks | Not requested; separate feature if needed |
| Camera capture / burst acquisition / image picking | Caller's responsibility; example app may use hard-coded or bundled samples |
| Video input | Only still image files |
| Remote (`http(s)://`) or `content://` / `ph://` URIs | Only local filesystem paths; caller copies to a local file first |
| Old Architecture (bridge) support | AD-001: pure C++ TurboModule, New Architecture only |

---

## Assumptions & Open Questions

| Assumption / decision | Chosen default | Rationale | Confirmed? |
| --------------------- | -------------- | --------- | ---------- |
| Frame count | N must be exactly 5, 15 or 30 | User decision: three supported burst sizes | y |
| Scale factor | Integer, allowed `1, 2, 3, 4`; default `2` | Common SR factors; 1 = align+mean denoise only, still useful | y |
| Reference frame | First path in array (`paths[0]`) | Deterministic, caller controls it | y |
| Input dimensions | All frames must have identical width×height after decoding | Burst frames match; resizing mismatched frames hides caller bugs | y |
| Per-frame alignment failure | Frame is skipped (excluded from mean); call still succeeds if ≥2 frames (incl. reference) remain; `framesUsed` reports count | Hand-held bursts often have one bad frame; failing whole call is harsh | y |
| Output path default | `<platform temp/cache dir>/superres-<unique>.jpg` | Unique name so concurrent calls don't collide | y |
| Output encoding | By `outputPath` extension: `.jpg`/`.jpeg` → JPEG quality 95, `.png` → PNG; default JPEG | Common formats; extension is unambiguous | y |
| Output color | 8-bit, 3-channel (BGR in OpenCV, standard RGB file); alpha dropped; grayscale input → 3-channel output | Photos; keeps one code path | y |
| Path forms accepted | Absolute path or `file://` URI (prefix stripped, percent-decoding applied) | Common outputs of RN camera/picker libs | y |
| Error format | Promise rejects with `Error` whose `message` begins with a code: `E_INVALID_ARGUMENT`, `E_DECODE_FAILED`, `E_SIZE_MISMATCH`, `E_ALIGNMENT_FAILED`, `E_WRITE_FAILED`, `E_INTERNAL` | Stable, testable codes; TurboModule promise rejection carries a message | y |
| Argument validation location | Shape/bounds validated in JS wrapper (reject before calling native) and re-checked in C++ | JS checks are Jest-testable; C++ checks guard direct native callers | y |
| `multiply` | Removed from public API, spec, C++ and example | Template placeholder; README documents real API | y |
| Concurrent calls | Allowed; each call is independent and writes its own output file | No shared mutable state required | y |
| Determinism | Same inputs + options SHALL produce byte-identical output on the same device/build | Makes behavior testable and debuggable | y |
| Overwriting `outputPath` | If file exists it is overwritten | Caller explicitly chose the path | y |
| Native behavior verification | JS wrapper covered by Jest (native module mocked); native pipeline verified by a C++ host test target and/or the example app on device with bundled sample frames — exact harness chosen in Design | No native test harness exists today | y |
| OpenCV exceptions | opencv-mobile has no C++ exceptions: no ECC (phaseCorrelate translation-only alignment, rotation not compensated); OpenCV asserts abort the process, so every input is validated before OpenCV calls + 1 GiB memory pre-flight; residual crash risk documented in README | Owner decision 2026-11-11 (keep opencv-mobile + guards) | y |
| Library size | OpenCV adds significant binary size; acceptable, but only modules needed (core, imgproc, imgcodecs, video) are linked where the distribution allows | Unavoidable for OpenCV | y |

**Open questions:** none — all resolved or logged above (required before the spec is confirmed).

---

## User Stories

### P1: Super-resolve N frames into one file ⭐ MVP

**User Story**: As a React Native app developer, I want to call `superResolve(paths)` with a burst of photos of the same scene so that I get one higher-resolution, lower-noise image file without freezing my UI.

**Why P1**: This is the library's entire purpose.

**Acceptance Criteria**:

1. WHEN `superResolve(paths)` is called with 5, 15 or 30 valid same-size local image paths THEN system SHALL resolve with `{ path: string, width: number, height: number, framesUsed: number }` where `path` is a `file://` URI of an existing, decodable image file.
2. WHEN called without `options.scale` THEN the resolved `width`/`height` SHALL equal `2 × width` / `2 × height` of `paths[0]`, and the file on disk SHALL have those exact dimensions.
3. WHEN all N input files are pixel-identical copies of one image THEN the output SHALL equal that image upscaled by the same interpolation used in the pipeline, within ±1 per channel per pixel (mean of identical frames is identity), and `framesUsed` SHALL equal N.
4. WHEN frames are translated copies of one scene (known integer pixel shifts ≤ 5% of width/height) THEN the pipeline SHALL align them to `paths[0]` before averaging, such that output PSNR vs. the upscaled reference is ≥ 30 dB in the region covered by all frames; the same frames averaged *without* alignment SHALL score lower (proves alignment is applied).
5. WHEN `superResolve` is running THEN the JS thread SHALL NOT be blocked (a JS timer scheduled at call time fires before the Promise settles for an input that takes > 500 ms to process).
6. WHEN called on iOS and on Android with the same inputs THEN both SHALL resolve per criteria 1–3 (same C++ pipeline, AD-001).
7. WHEN no `outputPath` is given THEN the file SHALL be written in the platform temp/cache directory with a unique name, as JPEG; two concurrent calls SHALL resolve with different `path` values and both files SHALL exist.

**Independent Test**: In the example app, bundle 5 sample frames, call `superResolve` on them, display the resulting `path` in `<Image>` and print `width`/`height`/`framesUsed`; plus native pipeline test for AC 3–4.

---

### P1: Reject invalid input with stable error codes ⭐ MVP

**User Story**: As an app developer, I want predictable rejections for bad input so that I can show a meaningful message instead of crashing.

**Why P1**: Native image code without validation crashes the app; MVP must be safe.

**Acceptance Criteria**:

1. WHEN `paths` is not an array, has a length other than 5, 15 or 30, or contains a non-string / empty string THEN Promise SHALL reject with message starting `E_INVALID_ARGUMENT`, and native processing SHALL NOT start.
2. WHEN `options.scale` is provided and is not one of `1, 2, 3, 4` THEN Promise SHALL reject with `E_INVALID_ARGUMENT`.
3. WHEN `options.outputPath` is provided with an extension other than `.jpg`, `.jpeg`, `.png` (case-insensitive) THEN Promise SHALL reject with `E_INVALID_ARGUMENT`.
4. WHEN any path does not exist or cannot be decoded as an image THEN Promise SHALL reject with `E_DECODE_FAILED` and the message SHALL include the failing index.
5. WHEN decoded frames do not all share `paths[0]`'s width×height THEN Promise SHALL reject with `E_SIZE_MISMATCH` and the message SHALL include the first mismatching index.
6. WHEN alignment fails for so many frames that fewer than 2 frames (including reference) remain THEN Promise SHALL reject with `E_ALIGNMENT_FAILED`.
7. WHEN the output file cannot be written (e.g. directory does not exist or is not writable) THEN Promise SHALL reject with `E_WRITE_FAILED`.
8. WHEN a C++ exception is thrown on the worker (`SrError` excluded, e.g. `std::bad_alloc`, `std::exception`) or the pre-flight memory estimate (accumulator + output + working frames) exceeds the 1 GiB budget THEN Promise SHALL reject with `E_INTERNAL` and the app SHALL NOT crash. *(Amended 2026-11-11, owner decision: opencv-mobile is built without exceptions, so OpenCV-internal assertion failures cannot be caught — they are prevented by pre-flight validation, not handled.)*
9. WHEN called on web THEN `superResolve` SHALL reject (or throw) with the existing "only supported on native platforms" message.

**Independent Test**: Jest tests for AC 1–3, 9 with native module mocked; native/example tests for AC 4–8 using a missing path, a text file, two different-size images, an unwritable output path.

---

### P2: Configure scale and output location

**User Story**: As an app developer, I want to choose the upscale factor and where the result is saved so that I control quality/size trade-offs and file management.

**Why P2**: Defaults cover MVP; configuration is the next most useful control.

**Acceptance Criteria**:

1. WHEN `options.scale = s` with `s ∈ {1,2,3,4}` THEN resolved and on-disk dimensions SHALL equal `s ×` reference dimensions.
2. WHEN `options.outputPath` is given (absolute path or `file://` URI) THEN the result SHALL be written exactly there (overwriting an existing file) and `path` SHALL be its `file://` URI.
3. WHEN `outputPath` ends in `.png` THEN the file SHALL be a PNG; WHEN it ends in `.jpg`/`.jpeg` THEN a JPEG (quality 95).

**Independent Test**: Example app call with `{ scale: 3, outputPath: <cache>/out.png }`, then verify file exists, is PNG, and dimensions are 3×.

---

### P2: Documented public API

**User Story**: As an app developer, I want README and TypeScript types describing `superResolve` so that I can use it without reading native code.

**Why P2**: Needed for publishing but not for function to work.

**Acceptance Criteria**:

1. WHEN the package is built (`yarn prepare`) THEN `lib/typescript` SHALL export `superResolve`, `SuperResolveOptions`, `SuperResolveResult`, and SHALL NOT export `multiply`.
2. WHEN reading README THEN it SHALL show install steps (including any OpenCV setup required per platform), a usage example, options table, result shape, and the error codes list.

**Independent Test**: `yarn prepare && yarn typecheck`; inspect `lib/typescript/src/index.d.ts` and README.

---

## Edge Cases

- WHEN `paths` contains the same path twice THEN system SHALL treat them as separate frames (identical frames, valid input).
- WHEN input is grayscale or has alpha THEN system SHALL convert to 3-channel 8-bit before processing (output always 3-channel).
- WHEN `scale = 1` THEN output dimensions SHALL equal input dimensions (align + mean only).
- WHEN `file://` URI has percent-encoded characters (e.g. spaces `%20`) THEN system SHALL decode them before opening.
- WHEN EXIF orientation is present THEN system SHALL apply OpenCV's default decode behavior (orientation applied) consistently to all frames.
- WHEN frames are very large (e.g. 12 MP × 30 frames × scale 4) THEN system SHALL, after decoding the reference frame and before allocating the accumulator, estimate peak memory and reject with `E_INTERNAL` if it exceeds 1 GiB; otherwise complete — never hang indefinitely.

---

## Implicit-Requirement Dimensions Sweep

| Dimension | Resolution |
| --------- | ---------- |
| Input validation & bounds | SR-ERR-01..05, SR-CFG-01..03 |
| Failure / partial-failure states | Per-frame alignment skip (SR-ERR-06); no partial output file on failure is guaranteed — assumption: on rejection after write starts, partial file may remain (logged as accepted) |
| Idempotency / retry / duplicate handling | Determinism assumption; re-calling with same `outputPath` overwrites (SR-CFG-02) |
| Auth boundaries & rate limits | N/A because local on-device computation, no auth or network |
| Concurrency / ordering | Concurrent calls independent, unique default outputs (SR-CORE-07) |
| Data lifecycle / expiry | Default outputs live in temp/cache dir; OS may purge; caller moves files it wants to keep (documented in README) |
| Observability | N/A because library has no logging contract; errors surface via codes |
| External-dependency failure | OpenCV is linked statically/bundled — no runtime fetch; decode/write failures covered by error codes |
| State-transition integrity | N/A because function is stateless (no persisted state between calls) |

---

## Requirement Traceability

| Requirement ID | Story | Phase | Status |
| -------------- | ----- | ----- | ------ |
| SR-CORE-01 | P1 Super-resolve: result shape + file exists | Execute | Verified |
| SR-CORE-02 | P1 Super-resolve: default scale 2 dimensions | Execute | Verified |
| SR-CORE-03 | P1 Super-resolve: identical frames identity | Execute | Verified |
| SR-CORE-04 | P1 Super-resolve: alignment applied (PSNR) | Execute | Verified |
| SR-CORE-05 | P1 Super-resolve: non-blocking JS thread | Execute | Verified |
| SR-CORE-06 | P1 Super-resolve: iOS + Android parity | Execute | Verified (Android/host; iOS unverified) |
| SR-CORE-07 | P1 Super-resolve: default temp output, unique, concurrent | Execute | Verified |
| SR-ERR-01 | P1 Errors: paths validation | Execute | Verified |
| SR-ERR-02 | P1 Errors: scale validation | Execute | Verified |
| SR-ERR-03 | P1 Errors: outputPath extension validation | Execute | Verified |
| SR-ERR-04 | P1 Errors: decode failure + index | Execute | Verified |
| SR-ERR-05 | P1 Errors: size mismatch + index | Execute | Verified |
| SR-ERR-06 | P1 Errors: alignment failure / frame skip | Execute | Verified |
| SR-ERR-07 | P1 Errors: write failure | Execute | Verified |
| SR-ERR-08 | P1 Errors: internal exception, no crash | Execute | Verified |
| SR-ERR-09 | P1 Errors: web unsupported | Execute | Verified |
| SR-CFG-01 | P2 Configure: scale 1–4 | Execute | Verified |
| SR-CFG-02 | P2 Configure: outputPath honored/overwrite | Execute | Verified |
| SR-CFG-03 | P2 Configure: format by extension | Execute | Verified |
| SR-DOC-01 | P2 Docs: exported types, no multiply | Execute | Verified |
| SR-DOC-02 | P2 Docs: README | Execute | Verified |

**Coverage:** 21 total, 21 mapped to tasks (T1–T13), 0 unmapped

---

## Success Criteria

- [ ] Example app on Android and iOS super-resolves 5 bundled frames and displays result with correct ×2 dimensions.
- [ ] Every `E_*` error code reproducible by a test; zero app crashes across error-case tests.
- [ ] `yarn lint`, `yarn typecheck`, `yarn test`, `yarn prepare` and both CI example builds pass with OpenCV linked.
