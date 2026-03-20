## Easy Trigger 2.7.0

### Playback and trigger fixes

- Fixed large forward timecode jumps so starting playback from the middle no longer fires every cue between the old and new position.
- Improved row highlight ownership so live Resolume feedback overrides temporary Test highlighting correctly.
- Tightened popup handling so Get Clips, Set, and End Action editors do not open duplicate windows.

### Resolume and OSC improvements

- Reworked Get Clips diagnostics with clearer `port busy`, `no reply`, and send/listen conflict reporting.
- Added safer Resolume listener startup with bind fallbacks and explicit protection against using the same send and listen port.
- Improved live Resolume feedback handling and reduced UI stalls after clip import by batching table refresh work.
- Expanded monitoring for multiple Trigger Out targets with compact status-bar summaries and per-target details in Status Monitor.
- Reworked Trigger Status Monitor into a two-column diagnostics view with the OSC Console preserved at the bottom.

### Packaging and installer

- Bundled app-local MSVC runtime files and the VC++ redistributable in the Windows installer.
- Added Windows Firewall rule setup/removal to make OSC communication more reliable on fresh systems.
- Updated the setup icon pipeline so release packaging uses the current application icon assets.
