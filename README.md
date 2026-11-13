# react-native-mean-n-super-resolution

Multi-frame super resolution for React Native. It takes a burst of 5, 15 or 30 photos of the same scene and returns one higher-resolution, lower-noise image. Under the hood it uses OpenCV ([opencv-mobile](https://github.com/nihui/opencv-mobile)) to align every frame to the first one, upscale it, and average the results.

- Runs off the JS thread and returns a Promise.
- Takes local files in and writes a file out (JPEG or PNG).
- Uses one shared C++ pipeline on iOS and Android, built as a pure C++ TurboModule.

## Requirements

- React Native **New Architecture** only. The Old Architecture (bridge) is not supported.
- **Android:** NDK **r30** (`30.0.14904198`) or newer. The prebuilt opencv-mobile static libraries are built with a recent NDK and do not link with RN 0.88's default NDK 27. Set it in `android/build.gradle`:

  ```groovy
  buildscript {
    ext {
      ndkVersion = "30.0.14904198"
    }
  }
  ```

- **iOS:** CocoaPods with `curl` and `unzip` on the machine that runs `pod install`.
- The first native build needs network access to download OpenCV (see below).

## Installation

```sh
npm install react-native-mean-n-super-resolution
# or
yarn add react-native-mean-n-super-resolution
```

iOS:

```sh
cd ios && bundle exec pod install
```

### OpenCV is downloaded at build time

OpenCV is not in the npm package. The library pins one opencv-mobile release in `opencv.json` (version, URL and SHA-256 for each platform). The package is downloaded on first build, its checksum is verified, and it is cached in `node_modules/react-native-mean-n-super-resolution/.opencv/`:

- **Android:** downloaded by the library's CMake while Gradle configures the native build. All ABIs share one download.
- **iOS:** downloaded while CocoaPods evaluates the podspec (`pod install`), then vendored as `opencv2.xcframework`.

A checksum mismatch fails the build.

**Offline or air-gapped Android builds:** set `OPENCV_MOBILE_DIR` to an already-extracted opencv-mobile Android package and the download is skipped:

```sh
export OPENCV_MOBILE_DIR=/path/to/extracted/opencv-mobile-android   # the folder containing opencv-mobile-4.14.0-android/
```

## Usage

```ts
import {
  superResolve,
  ErrorCodes,
  type SuperResolveResult,
} from 'react-native-mean-n-super-resolution';

const paths = [
  'file:///data/user/0/com.app/cache/burst-0.jpg', // reference frame
  'file:///data/user/0/com.app/cache/burst-1.jpg',
  'file:///data/user/0/com.app/cache/burst-2.jpg',
  'file:///data/user/0/com.app/cache/burst-3.jpg',
  'file:///data/user/0/com.app/cache/burst-4.jpg',
];

try {
  const result: SuperResolveResult = await superResolve(paths, { scale: 2 });
  console.log(result.path, result.width, result.height, result.framesUsed);
} catch (e) {
  const message = e instanceof Error ? e.message : String(e);
  if (message.startsWith(ErrorCodes.ALIGNMENT_FAILED)) {
    // frames are too different from the reference
  }
}
```

### `superResolve(paths, options?)`

`superResolve(paths: string[], options?: SuperResolveOptions): Promise<SuperResolveResult>`

#### `paths`

- **Frame count:** the array must contain exactly **5, 15 or 30** paths. Any other length rejects with `E_INVALID_ARGUMENT`.
- **Path form:** each entry is an absolute filesystem path or a `file://` URI. Percent-encoding such as `%20` is decoded. `content://`, `ph://` and `http(s)://` URIs are rejected, so copy those images to a local file first.
- **Reference frame:** `paths[0]` is the reference. The output is aligned to it and sized from it.
- **Frame sizes:** every frame must decode to the same width × height as the reference.
- **Formats:** frames can be JPEG, PNG, BMP or PNM. Grayscale and alpha inputs are converted to 3-channel color, and EXIF orientation is applied to all frames.
- **Duplicates:** the same path may appear more than once. Each entry counts as a separate frame.

#### Options

| Option | Type | Default | Description |
| --- | --- | --- | --- |
| `scale` | `1 \| 2 \| 3 \| 4` | `2` | Integer upscale factor. Output size is `scale ×` the reference frame's size. `1` aligns and averages without upscaling, which only reduces noise. |
| `outputPath` | `string` | unique file in the platform temp dir | Absolute path or `file://` URI where the result is written. The extension (case-insensitive) picks the format: `.jpg`/`.jpeg` writes JPEG at quality 95, `.png` writes PNG. An existing file is overwritten, and the directory must already exist. |

#### Result

```ts
type SuperResolveResult = {
  path: string;       // file:// URI of the written image
  width: number;      // scale × reference width
  height: number;     // scale × reference height
  framesUsed: number; // frames averaged, reference included (2..N)
};
```

#### How the image is built

Each frame is aligned to the reference, upscaled with bicubic interpolation, and averaged with the others.

- **Translation only:** alignment estimates a translation (shift) for each frame with phase correlation. Rotation between frames is not compensated.
- **Skipped frames:** a frame that does not correlate well with the reference after alignment is left out of the average, and `framesUsed` reports how many frames were used. If fewer than 2 frames remain, including the reference, the call rejects with `E_ALIGNMENT_FAILED`.
- **Determinism:** the same inputs and options give byte-identical output on the same device and build.

### Default output location and lifecycle

When `outputPath` is omitted, the result is written as JPEG to a unique `superres-<…>.jpg` file in the platform temp/cache directory:

- **Android:** `TMPDIR` when it is set and writable, otherwise the app's cache dir (`Context.getCacheDir()`).
- **iOS:** `NSTemporaryDirectory()`.

Concurrent calls never write to the same file.

The OS can delete files in these folders at any time, so move or copy results you want to keep. The library never deletes output files, so you must clean up old ones yourself.

### Errors

The Promise rejects with an `Error` whose `message` starts with one of these codes, followed by `: ` and a detail. The codes are exported as `ErrorCodes`.

| Code | When |
| --- | --- |
| `E_INVALID_ARGUMENT` | `paths` is not an array of 5, 15 or 30 non-empty strings, `scale` is not 1–4, `outputPath` has an extension other than `.jpg`/`.jpeg`/`.png`, or a path uses an unsupported URI scheme. Array, count, `scale` and extension checks run in JS before the native call; the native side re-checks them and rejects non-`file://` URI schemes (`content://`, `ph://`, `http(s)://`) before decoding anything. |
| `E_DECODE_FAILED` | A frame is missing or cannot be decoded. The message includes `index=<i>`. |
| `E_SIZE_MISMATCH` | A frame's size differs from `paths[0]`. The message includes the first mismatching `index=<i>`. |
| `E_ALIGNMENT_FAILED` | Fewer than 2 frames, including the reference, could be aligned. |
| `E_WRITE_FAILED` | The output file cannot be written, for example because the directory is missing or not writable, or no temp directory is available. |
| `E_INTERNAL` | An unexpected C++ exception was thrown, or the estimated peak memory exceeds the 1 GiB budget. |

#### Memory limit and residual crash risk

- **Memory budget:** after decoding the reference frame and before allocating the output buffers, the pipeline estimates its peak memory, roughly `scale² × width × height × 15` bytes. If that exceeds **1 GiB**, the call rejects with `E_INTERNAL` instead of running out of memory. For example, a 12 MP burst works at `scale: 2` but not at `scale: 4`.
- **Residual crash risk:** opencv-mobile is built without C++ exceptions, so an internal OpenCV assertion terminates the process instead of throwing. Every input is validated before it reaches OpenCV to prevent this. A crash is still possible in situations those checks did not anticipate.

### Web

Not supported. On web, `superResolve` rejects with `'react-native-mean-n-super-resolution' is only supported on native platforms.`

## Example app

`example/` super-resolves 5 bundled frames (generated by `example/scripts/generate-frames.mjs`) and shows the result, its dimensions, `framesUsed`, and a JS heartbeat counter that keeps ticking while the native work runs.

```sh
yarn
yarn example android   # or: yarn example ios
```

## Contributing

- [Development workflow](CONTRIBUTING.md#development-workflow)
- [Sending a pull request](CONTRIBUTING.md#sending-a-pull-request)
- [Code of conduct](CODE_OF_CONDUCT.md)

The native pipeline has a host GoogleTest suite: `yarn test:native` (needs CMake and a C++20 compiler).

## License

MIT

---

Made with [create-react-native-library](https://github.com/callstack/react-native-builder-bob)
