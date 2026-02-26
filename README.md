# Better Markers for OBS

Better Markers lets you drop edit markers while recording in OBS and export them for Premiere Pro, DaVinci Resolve, and Final Cut Pro.

## What You Get

- `Add Marker` in OBS while recording
- Marker templates (title, description, color)
- Optional template fields editable at marker time
- Hotkeys for quick markers
- Global/Profile/Scene Collection template scopes
- Multi-output export targets:
  - Premiere Pro (XMP sidecar + embed for MP4/MOV)
  - DaVinci Resolve (FCPXML timeline markers)
  - Final Cut Pro on macOS (FCPXML clip markers)

## Quick Start

1. Open OBS and go to `Tools -> Better Markers Settings`.
2. In `Export Targets`, enable the hosts you use.
3. Create marker templates.
4. Optionally set hotkeys in `Settings -> Hotkeys`.
5. Start recording and add markers from the dock or hotkeys.

## Generated Files

For recording file `<name>.mp4` or `<name>.mov`, Better Markers creates:

- Premiere:
  - `<name>.xmp`
  - Marker embed is applied automatically to MP4/MOV when recording closes
- Final Cut Pro (macOS):
  - `<name>.better-markers.fcp.fcpxml`
- DaVinci Resolve:
  - `<name>.better-markers.resolve.fcpxml`

All files are written in the same folder as the recording.

## Import In Your Editor

- Premiere Pro:
  - Import/open the recording normally. Markers are already embedded (MP4/MOV) or available from the `.xmp` sidecar.
- DaVinci Resolve:
  - Import the recording.
  - Import `<name>.better-markers.resolve.fcpxml` to bring in timeline markers.
- Final Cut Pro (macOS):
  - Import the recording.
  - Import `<name>.better-markers.fcp.fcpxml` to bring in clip markers.

## Important Notes

- Markers can be added only while recording is active (not paused).
- Export writes happen immediately after each new marker.
- Multi-output runs in parallel: one target failing does not block the others.
- Final Cut export is available only on macOS.
- Resolve export uses timeline markers in v1.
- Only MP4/MOV recordings are supported for marker export artifacts.
