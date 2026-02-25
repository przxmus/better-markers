# Better Markers (OBS Plugin)

Better Markers is an OBS frontend plugin that adds Premiere Pro-compatible clip markers during recording.

## What It Does

- Adds a **Better Markers** dock with an **Add Marker** button on the main OBS screen.
- Lets you create marker templates in **Tools -> Better Markers Settings** with:
  - template name
  - title
  - description
  - Premiere marker color
  - per-field editable flags for add-marker dialogs
  - scope (`Global`, `Profile`, `Scene Collection`)
- Registers hotkeys in OBS **Settings -> Hotkeys** for:
  - `Better Markers: Quick Marker`
  - `Better Markers: Quick Custom Marker`
  - every active template (scope-aware)
- Captures marker time **at trigger press**, not at dialog submit.
- Allows marker creation **only while recording and not paused**.
- Writes XMP sidecar markers immediately for the active recording file.
- On split/stop, tries native MP4/MOV embed into `moov/udta/XMP_` with safe temp rewrite and rollback.
- Keeps sidecar files even after successful embed.
- Queues failed embed jobs and retries them on plugin load.

## Scope Model

Active templates are the union of:

- `Global`
- current `Profile`
- current `Scene Collection`

Each template keeps its own scoped hotkey binding data.

## Premiere XMP Mapping

Each marker is written as:

- `xmpDM:startTime` (frame index, 0-based)
- `xmpDM:duration` (`0`)
- `xmpDM:name`
- `xmpDM:comment`
- `xmpDM:type` (`Cue`)
- `xmpDM:guid` (UUIDv4)
- `xmpDM:cuePointParams` with:
  - `marker_guid`
  - `color` (`0..8`, Premiere color IDs)

Track metadata:

- `trackName = Adobe Premiere Pro Clip Marker`
- `trackType = Clip`
- `frameRate = <num>f<den>s`

## Build (macOS)

```bash
cmake --preset macos
cmake --build --preset macos --config RelWithDebInfo
```

## Notes

- UI is English-only.
- If MP4/MOV embedding fails, original recording is preserved and sidecar remains authoritative.
