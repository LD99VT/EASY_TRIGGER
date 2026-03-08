# Easy Trigger

**Easy Trigger** is a cross-platform desktop application for receiving timecode and firing Resolume clip triggers with frame-accurate offsets.

It shares the same LTC/MTC/ArtNet/OSC input core as Easy Bridge, but is focused on Resolume workflows: layer/clip discovery, trigger timing, global offset, and LTC output monitoring.

![Windows](https://img.shields.io/badge/platform-Windows-blue)
![macOS](https://img.shields.io/badge/platform-macOS-lightgrey)
![Linux](https://img.shields.io/badge/platform-Linux-yellow)
![C++17](https://img.shields.io/badge/language-C%2B%2B17-orange)
![JUCE 8](https://img.shields.io/badge/framework-JUCE%208-green)
[![License](https://img.shields.io/badge/license-MIT-brightgreen)](LICENSE)

---
(screenshots/Screenshot 2026-03-08 at 21.40.25.png)
---
## Supported timecode input

| Input | LTC | MTC | ArtNet TC | OSC |
|-------|:---:|:---:|:---------:|:---:|
| Easy Trigger | yes | yes | yes | yes |

- **LTC** - Linear Timecode over audio (ASIO / WDM / CoreAudio)
- **MTC** - MIDI Timecode
- **ArtNet TC** - Timecode over Ethernet via ArtNet
- **OSC** - Open Sound Control timecode input

Additional app features:
- Resolume layer/clip fetch and trigger table
- Global trigger offset with `+` / `-` time shift
- LTC output monitoring and routing
- Windows installer and macOS DMG packaging in CI

---

## Download

Pre-built installers are attached to each [GitHub Release](../../releases/latest):

| Platform | Asset |
|----------|-------|
| Windows  | `EasyTrigger_Setup_<version>.exe` |
| macOS    | `EasyTrigger-<version>.dmg` |

---

## Build from source

### Prerequisites

**Windows**

- Visual Studio 2022 (workload: *Desktop development with C++*)
- CMake 3.22+
- Git

```powershell
winget install Kitware.CMake Git.Git
```

- (Optional) [Steinberg ASIO SDK](https://www.steinberg.net/asiosdk/) for ASIO/ReaRoute support.
  Place it in `ASIOSDK/` inside the repo or set the `ASIO_SDK_DIR` environment variable.

**macOS**

- Xcode + Command Line Tools
- CMake 3.22+
- Git

```bash
xcode-select --install
brew install cmake git
```

### Build

**Windows:**
```powershell
./build_win.ps1
```

**macOS:**
```bash
chmod +x build_mac.sh
./build_mac.sh
```

---

## Release workflow

GitHub Actions release workflow:
- file: `.github/workflows/release.yml`
- triggers:
  - push tag `v*`
  - manual `workflow_dispatch`

Tag release example:

```bash
git tag v2.6.0
git push origin v2.6.0
```

Produced assets:
- `EasyTrigger_Setup_<version>.exe`
- `EasyTrigger-<version>.dmg`
