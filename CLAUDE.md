# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

`react-native-mean-n-super-resolution`: a React Native library meant to perform super resolution over a set of images using OpenCV (multi-frame / mean-of-N approach). It was scaffolded with `create-react-native-library` (type `turbo-module`, language `cpp`) and currently only contains the template `multiply` method — OpenCV is not yet integrated into either native build.

## Commands

Monorepo using Yarn 4 workspaces (`.yarn/releases/yarn-4.18.1.cjs`, `nodeLinker: node-modules`); root is the library, `example/` is the test app. Node version in `.nvmrc`.

```sh
yarn                      # install all workspaces
yarn typecheck            # tsc
yarn lint                 # eslint (add --fix for prettier formatting)
yarn test                 # jest (preset @react-native/jest-preset)
yarn test path/to/file    # single test file; add -t "name" for a single test
yarn prepare              # bob build -> lib/ (ESM module + .d.ts)
yarn clean                # remove lib/ and native build dirs
yarn example start        # Metro for example app
yarn example android      # build + run example on Android
yarn example ios          # build + run example on iOS (run `bundle exec pod install` in example/ios first)
yarn turbo run build:android   # CI-style cached example build (same for build:ios)
yarn release              # release-it: bump, changelog, tag, npm publish, GitHub release
```

Lefthook pre-commit runs eslint on staged files and `tsc`; commit-msg runs commitlint (conventional commits: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`). CI (`.github/workflows/ci.yml`) runs lint, typecheck, tests with coverage, `yarn prepare`, and example builds for Android and iOS.

## Architecture

This is a **pure C++ TurboModule** (New Architecture only, no Kotlin/Obj-C module code). One method flows through these layers, and adding or changing an API means touching all of them:

1. **Spec** — `src/NativeMeanNSuperResolution.ts`. The `Spec` interface is the codegen source (`codegenConfig` in `package.json`, name `MeanNSuperResolutionSpec`, `jsSrcsDir: src`). Codegen generates `MeanNSuperResolutionSpecJSI.h` containing the base class `NativeMeanNSuperResolutionCxxSpec<T>`.
2. **C++ implementation** — `cpp/MeanNSuperResolutionImpl.{h,cpp}`. Subclasses the generated spec; each spec method must be implemented with a `jsi::Runtime&` first parameter and C++ types matching the TS signature (codegen errors surface as compile errors here, not in TS).
3. **Platform registration** (shared C++ is compiled by both platforms):
   - iOS: `ios/OnLoad.mm` registers the module in `+load` via `registerCxxModuleToGlobalModuleMap`. `MeanNSuperResolution.podspec` compiles `ios/**` and `cpp/**`.
   - Android: `react-native.config.js` (`cxxModuleCMakeListsPath`, `cxxModuleHeaderName`) tells RN autolinking to build `android/CMakeLists.txt` and instantiate `MeanNSuperResolutionImpl`. CMakeLists links `jsi`, `reactnative`, `react_codegen_MeanNSuperResolutionSpec`. New `.cpp` files must be added to `add_library` here (the podspec picks them up via glob).
4. **JS wrapper** — `src/<name>.native.tsx` calls the TurboModule; `src/<name>.tsx` is the non-native (web) fallback that throws. `src/index.tsx` re-exports the public API.

Integrating OpenCV will require adding it to both native builds: a pod dependency / framework in the podspec for iOS, and `find_package`/linking in `android/CMakeLists.txt` for Android.

### Specs

Feature work follows spec-driven flow in `.specs/`: `STATE.md` (decisions log `AD-NNN` + handoff snapshot), `features/<name>/` (spec/design/tasks/validation), `LESSONS.md`/`lessons.json` (machine-owned via the tlc-spec-driven `lessons.py` script — do not hand-edit). Read `STATE.md` Decisions before designing a feature.

### Packaging / resolution

- `package.json` `exports` has a custom condition `react-native-mean-n-super-resolution-source` pointing at `src/index.tsx`. The example app's Metro config and Jest's `customExportConditions` use it, so the example and tests consume library **source** directly — JS changes need no rebuild; native changes require rebuilding the example app.
- Published output is `lib/` (built by `react-native-builder-bob`, typings via `tsconfig.build.json`). Jest ignores `lib/` and `example/node_modules`.
