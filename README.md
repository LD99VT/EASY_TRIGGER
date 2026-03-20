# Easy Trigger

**Easy Trigger** is a cross-platform desktop application for receiving timecode and firing Resolume clip triggers with frame-accurate offsets.

It shares the same LTC/MTC/ArtNet/OSC input core as Easy Bridge, but is focused on Resolume workflows: layer/clip discovery, trigger timing, global offset, and LTC output monitoring.

![Windows](https://img.shields.io/badge/platform-Windows-blue)
![macOS](https://img.shields.io/badge/platform-macOS-lightgrey)
![Linux](https://img.shields.io/badge/platform-Linux-yellow)
![C++17](https://img.shields.io/badge/language-C%2B%2B17-orange)
![JUCE 8](https://img.shields.io/badge/framework-JUCE%208-green)
[![License](https://img.shields.io/badge/license-MIT-brightgreen)](LICENSE)
[![Donate](https://img.shields.io/badge/Donate-PayPal-00457C?logo=paypal&logoColor=white)](https://www.paypal.com/donate/?hosted_button_id=USMEZ72YAMXN6)

## Screenshot

![Easy Trigger UI](screenshots/NEW%202-7-0.png)

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

## Latest update (2.7.0)

### 2.7.0 - Resolume, monitoring, and packaging update
- Fixed large forward timecode jumps so starting playback from the middle no longer fires every cue between the old and new position.
- Reworked `Get Clips` diagnostics with clearer `port busy`, `no reply`, and send/listen conflict reporting.
- Added safer Resolume listener startup with bind fallbacks and protection against using the same send and listen port.
- Improved live Resolume feedback handling and reduced UI stalls after clip import by batching heavy table refresh work.
- Expanded multi-target `Trigger Out` monitoring with compact status-bar summaries and per-target details in `Status Monitor`.
- Reworked `Status Monitor` into a two-column diagnostics view while keeping the live OSC Console at the bottom.
- Prevented duplicate popups for `Get Clips`, `Set`, and `End Action`.
- Bundled runtime and firewall setup improvements into the Windows installer for more reliable first-run behaviour on clean systems.

### 2.7.0 — Table UI & trigger behaviour improvements
- Unified flat button style across the entire trigger table: no borders, consistent `hover` / `press` colours for all action buttons.
- `Custom Trigger` edit popup: **Reset** is now left-aligned; **Apply** and **Cancel** are grouped on the right — matching the Preferences layout.
- **End Action** `Set` button is now available directly in group (collapsible) rows and applies the selected action to every clip in the layer at once.
- Text labels next to `Set` buttons now use **Bold 14 px** font (same as regular cell text) for visual consistency.
- When a trigger row fires (orange highlight), the text label next to `Set` switches to dark amber so it stays readable against the bright background.
- The test-highlight border is suppressed while the row is in the fired state to avoid visual conflict with the orange glow.
- `Trigger Out` `+` / `−` buttons use the same background as the IP/Port input fields (`#242424`).
- Send target dropdown background matches the Test button colour (`#383838`).

### 2.6.9 — Windows LTC scan performance
- Reworked Windows audio device enumeration: channel counts are now resolved lazily for the selected device instead of opening every audio device during the full scan.
- LTC devices appear significantly faster at startup on Windows; the scan no longer blocks the UI.

### 2.6.8 — Trigger rewind handling
- Backward timecode jumps (rewind) are now treated as a re-arm event: range and one-shot state are cleared so triggers fire again when time moves forward through the markers.
- `currentTriggerKeys_`, `triggerRangeActive_`, and `pendingEndActions_` are all flushed on any backward frame jump.

### 2.6.7 — UI & Trigger Out overhaul
- Added the FPS strip under the main clock with `23.976 / 24 / 25 / 29.97 / 30` indicators.
- LTC input and output now use the real channel count from the selected audio device, including stereo pair options such as `1+2`, `3+4`, and so on.
- Added `Convert FPS` to `Out LTC` for explicit incoming-to-output frame-rate conversion.
- Split Trigger destinations into a dedicated `Trigger Out` section with up to 5 send targets.
- Each `Trigger Out` row has its own network adapter selection and sends via a socket bound to that adapter.
- Added adapter selection to `Resolume Settings` for the network side of the Resolume connection.
- Added `TC` / `C` badges in the clip table; improved timecode clip detection using Resolume `transporttype`.

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
git tag v2.7.0
git push origin v2.7.0
```

Produced assets:
- `EasyTrigger_Setup_<version>.exe`
- `EasyTrigger-<version>.dmg`
