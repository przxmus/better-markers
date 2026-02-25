# OBS Cross-Platform Plugin Notes

Date: 2026-02-25

## Recommended Base

Use the official OBS plugin template (`obsproject/obs-plugintemplate`) for cross-platform support.
It includes CMake presets and CI workflows for macOS, Linux, and Windows.

## Platform Build Targets

- macOS: `macos` preset (Universal `arm64;x86_64`)
- Linux: `ubuntu-x86_64` preset (Ninja)
- Windows: `windows-x64` preset (Visual Studio 2022)

## Local Dev Testing Strategy

For fast iteration on macOS:

1. Build with template preset.
2. Use template-generated `build_macos/rundir/RelWithDebInfo` for runtime plugin assets.
3. Launch OBS with `OBS_PLUGINS_PATH` and `OBS_PLUGINS_DATA_PATH` set to that `rundir`.

This avoids repetitive manual copying into global plugin directories.

## Local Install Path Defaults

Template defaults:

- macOS: `~/Library/Application Support/obs-studio/plugins`
- Windows: `%ALLUSERSPROFILE%/obs-studio/plugins`

## Source Links

- https://github.com/obsproject/obs-plugintemplate
- https://github.com/obsproject/obs-plugintemplate/wiki/Quick-Start-Guide
- https://github.com/obsproject/obs-plugintemplate/wiki/Build-System-Requirements
- https://github.com/obsproject/obs-plugintemplate/wiki/How-To-Debug-Your-Plugin
