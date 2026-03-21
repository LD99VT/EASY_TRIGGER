## Easy Trigger 2.7.1

### Playback, Resolume and monitoring

- Fixed large forward timecode jumps so starting playback from the middle no longer fires every cue between the old and new position.
- Improved Resolume `Get Clips`, OSC diagnostics, and live feedback handling.
- Expanded multi-output monitoring in the status bar and Status Monitor with clearer per-target reporting.
- Improved highlight ownership so live Resolume feedback correctly overrides temporary test highlighting.

### UI and workflow

- Continued the UI refactor by extracting more monitoring and settings-panel logic from the main window.
- Added built-in update checking on startup and from the Help menu.
- Added an in-app update prompt and platform-aware update flow.
- Prevented duplicate popup windows for `Get Clips`, `Set`, and `End Action`.

### Packaging and release

- Installer now uses the icon from the shared `Icon` asset folder, matching the application icon.
- Bundled runtime and installer flow remain ready for clean Windows installs.
- Release workflow now checks out the repository before publishing so release notes are attached correctly.
