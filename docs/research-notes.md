# Better Markers Research Notes

Date: 2026-02-26

## OBS API Findings (31.1.1)

Key implementation details verified against vendored OBS source and headers:

- Recording lifecycle events available through `obs_frontend_event`:
  - `OBS_FRONTEND_EVENT_RECORDING_STARTED`
  - `OBS_FRONTEND_EVENT_RECORDING_STOPPED`
  - `OBS_FRONTEND_EVENT_RECORDING_PAUSED`
  - `OBS_FRONTEND_EVENT_RECORDING_UNPAUSED`
- Recording output is available via `obs_frontend_get_recording_output()`.
- Packet timing can be tracked with `obs_output_add_packet_callback(...)`.
  - Video packets expose `encoder_packet::dts_usec`.
- Split recording transition can be observed through output signal:
  - `file_changed(string next_file)`
- Current recording path can be obtained via:
  - `obs_frontend_get_last_recording()` (preferred)
  - `obs_frontend_get_current_record_output_path()` (fallback)

## Marker Timing

Final frame selection strategy:

1. Prefer packet timestamp:
   - `frame = floor(dts_usec * fps_num / (fps_den * 1_000_000))`
2. Fallback to monotonic active-recording time (pause-adjusted).

Trigger-time snapshot is captured before any marker dialog is shown.

## XMP Strategy

- Sidecar writing is authoritative during recording.
- A single Premiere marker track is maintained and rewritten atomically per update.
- Marker `type` is locked to `Cue`.

## MP4/MOV Embed Strategy

- Native atom-level rewrite modifies `moov/udta/XMP_`.
- Rewrite flow:
  1. Read and patch `moov` atom.
  2. Write full temp file in same directory.
  3. Validate embedded `XMP_` atom in temp output.
  4. Replace original via backup+rename rollback flow.
- For unsupported atom ordering (`moov` before trailing `mdat`), embed is skipped and deferred.

## Recovery

- Failed embed attempts are persisted in `pending-embed.json`.
- Queue is retried on plugin load.
- Sidecar remains present regardless of embed outcome.
