# Better Markers (OBS Plugin)

Cross-platform OBS plugin project scaffolded from the official OBS plugin template.

## Target Platforms

- macOS (host dev machine)
- Linux (Ubuntu)
- Windows (x64)

## Build Prerequisites

From the official template requirements:

- CMake 3.28+
- macOS: Xcode 16+
- Linux: `ninja-build`, `pkg-config`, `build-essential`
- Windows: Visual Studio 2022 + recent Windows SDK

## Quick Start (macOS)

```bash
./scripts/dev/configure.sh
./scripts/dev/build-install.sh
```

That builds the plugin and installs it into:

`~/Library/Application Support/obs-studio/plugins`

Then restart OBS.

## Fast Dev Loop (No Manual Install)

The OBS template build copies outputs into `build_macos/rundir/RelWithDebInfo`. You can run OBS against that folder directly:

```bash
./scripts/dev/run-obs-with-rundir.sh
```

This avoids manual plugin copying while iterating and launches OBS as a normal macOS app.

## Auto Rebuild + Auto Install

Use a watcher that triggers build+install whenever source files change:

```bash
./scripts/dev/watch-install.sh
```

Notes:
- Prefers `fswatch` if installed (`brew install fswatch`)
- Falls back to a portable polling mode if `fswatch` is unavailable

## Cross-Platform Build Commands

macOS:

```bash
cmake --preset macos
cmake --build --preset macos
```

Linux:

```bash
cmake --preset ubuntu-x86_64
cmake --build --preset ubuntu-x86_64
```

Windows (Developer PowerShell):

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
```

## Upstream References

- Template repository: https://github.com/obsproject/obs-plugintemplate
- Template wiki: https://github.com/obsproject/obs-plugintemplate/wiki
- Build requirements: https://github.com/obsproject/obs-plugintemplate/wiki/Build-System-Requirements
- Quick start: https://github.com/obsproject/obs-plugintemplate/wiki/Quick-Start-Guide
- Debugging plugin with `rundir`: https://github.com/obsproject/obs-plugintemplate/wiki/How-To-Debug-Your-Plugin
