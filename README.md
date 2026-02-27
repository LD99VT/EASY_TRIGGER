# MTC Bridge JUCE Rewrite

Cross-platform C++ rewrite target for Easy Bridge v2 (Windows + macOS) with shared LTC core direction.

## Current status

- JUCE app skeleton is created.
- Core modules ported from Python:
  - `Timecode` (including 29.97 DF math)
  - `ClockState` (thread-safe monotonic sync model)
  - `ConfigStore` (JSON load/save)
- UI is a placeholder frame preserving Bridge layout zones (IN / OUT / STATUS), no final styling yet.

## Prerequisites

### Windows

- Visual Studio 2022 with workload:
  - Desktop development with C++
- Git
- CMake 3.22+

Quick install (PowerShell as Administrator):

```powershell
winget install Kitware.CMake
winget install Git.Git
```

### macOS

- Xcode + Command Line Tools
- CMake 3.22+
- Git

Quick install:

```bash
xcode-select --install
brew install cmake git
```

## Build

Windows:

```powershell
./build_win.ps1
```

macOS:

```bash
chmod +x build_mac.sh
./build_mac.sh
```

