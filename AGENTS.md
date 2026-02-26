# AGENTS: Better Markers

## What This Project Is

Better Markers is an OBS plugin that lets you drop markers during recording and automatically export them to:
- Adobe Premiere Pro (XMP sidecar + MP4/MOV embed),
- DaVinci Resolve (FCPXML with timeline markers),
- Final Cut Pro on macOS (FCPXML with clip markers).

The plugin works only while recording is active (not paused) and writes artifacts next to the recording file.

## How It Works (High Level)

1. OBS emits recording events (`started/stopped/paused/unpaused` and `file_changed` during split recording).
2. `RecordingSessionTracker` keeps current session state, fps, and active media path.
3. `MarkerController` creates a marker (from UI, hotkey, or template), computes frame timing, and dispatches data to export sinks.
4. Sinks run independently:
   - `PremiereXmpSink` writes sidecar data and attempts MP4/MOV embed,
   - `ResolveFcpxmlSink` generates `.better-markers.resolve.fcpxml`,
   - `FinalCutFcpxmlSink` (macOS) generates `.better-markers.fcp.fcpxml`.
5. If embed fails, the job is stored in `RecoveryQueue` and retried on plugin startup.

## Key Directories And Files

- `src/plugin-main.cpp`: plugin entry point, tools menu/dock wiring, OBS callbacks, controller/hotkey initialization.
- `src/bm-recording-session-tracker.*`: recording state, current media path, DTS-based timing with monotonic fallback.
- `src/bm-marker-controller.*`: core marker and export orchestration.
- `src/bm-settings-dialog.*` + `src/bm-template-editor-dialog.*`: settings and template UI.
- `src/bm-scope-store.*` + `src/bm-models.*`: config persistence and model serialization.
- `src/bm-*-sink.*`, `src/bm-fcpxml-writer.*`, `src/bm-xmp-sidecar-writer.*`, `src/bm-mp4-mov-embed-engine.*`: export and file generation internals.
- `tests/`: config and FCPXML serialization tests.
- `data/locale/*.ini`: UI localization files.

## Data And Artifacts

- Plugin config:
  - `.../obs-studio/plugin_config/better-markers/stores/global-store.json`
  - `.../obs-studio/plugin_config/better-markers/stores/profiles/<profile>/profile-store.json`
- Embed recovery queue:
  - `pending-embed.json` in the stores directory.
- For recording `<name>.mp4/.mov`:
  - `<name>.xmp`
  - `<name>.better-markers.resolve.fcpxml`
  - `<name>.better-markers.fcp.fcpxml` (macOS)

## Build, Run, Tests

- Fast macOS dev flow:
  - `./scripts/dev/configure.sh`
  - `./scripts/dev/build-install.sh`
- Auto rebuild/install:
  - `./scripts/dev/watch-install.sh`
- Tests:
  - `cmake --build build_macos --config RelWithDebInfo --target better-markers-tests`
  - `ctest --test-dir build_macos -C RelWithDebInfo --output-on-failure`

## Working Rules For This Repo

- Make small, frequent commits for logical changes.
- Do not mix refactors and new features in one commit.
- For export-related changes, verify all enabled targets (Premiere/Resolve/Final Cut).
- For hotkey/focus behavior changes, run the checklist in `docs/focus-qa-checklist.md`.

## Final Code Formatting Gate

- Run `clang-format-19` and `gersemi` only after the full implementation is complete.
- Do not run these checks on every partial change or on every commit.
- Treat this as a final gate before handing off completed work.
- If either tool reports issues, fix the code and rerun checks until both are clean.

## Localization Maintenance

- Keep `data/locale/en-US.ini` as the source key set.
- If any locale key is added, removed, or renamed in `en-US.ini`, apply the same key change and provide actual translations (not placeholders or default English strings) in all maintained locale files in the same change set, ensuring 100% translation coverage.
- Maintained locale files are:
  - `an-ES.ini`
  - `ar-SA.ini`
  - `az-AZ.ini`
  - `be-BY.ini`
  - `bg-BG.ini`
  - `bn-BD.ini`
  - `ca-ES.ini`
  - `cs-CZ.ini`
  - `da-DK.ini`
  - `de-DE.ini`
  - `el-GR.ini`
  - `en-GB.ini`
  - `en-US.ini`
  - `es-ES.ini`
  - `et-EE.ini`
  - `eu-ES.ini`
  - `fa-IR.ini`
  - `fi-FI.ini`
  - `fil-PH.ini`
  - `fr-FR.ini`
  - `gd-GB.ini`
  - `gl-ES.ini`
  - `he-IL.ini`
  - `hi-IN.ini`
  - `hr-HR.ini`
  - `hu-HU.ini`
  - `hy-AM.ini`
  - `id-ID.ini`
  - `it-IT.ini`
  - `ja-JP.ini`
  - `ka-GE.ini`
  - `kab-KAB.ini`
  - `kmr-TR.ini`
  - `ko-KR.ini`
  - `lo-LA.ini`
  - `ms-MY.ini`
  - `nb-NO.ini`
  - `nl-NL.ini`
  - `pl-PL.ini`
  - `pt-BR.ini`
  - `pt-PT.ini`
  - `ro-RO.ini`
  - `ru-RU.ini`
  - `si-LK.ini`
  - `sk-SK.ini`
  - `sl-SI.ini`
  - `sr-CS.ini`
  - `sr-SP.ini`
  - `sv-SE.ini`
  - `th-TH.ini`
  - `tl-PH.ini`
  - `tr-TR.ini`
  - `ug-CN.ini`
  - `uk-UA.ini`
  - `vi-VN.ini`
  - `zh-CN.ini`
  - `zh-TW.ini`
