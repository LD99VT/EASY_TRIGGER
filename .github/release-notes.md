## What's new

- Added the new FPS strip below the main clock with `23.976 / 24 / 25 / 29.97 / 30`.
- LTC input and output now use the real channel count of the selected device, including stereo pairs like `1+2`, `3+4`, and more.
- Added `Convert FPS` in `Out LTC` for explicit incoming-to-output frame-rate conversion.
- Split outgoing trigger destinations into a dedicated `Trigger Out` section with up to 5 send rows.
- Each `Trigger Out` row now has its own adapter selector and sends through a socket bound to that local network interface.
- Added adapter selection to `Resolume Settings`.
- Added `TC` / `C` badges in the clip table and improved timecode clip detection using Resolume `transporttype`.
- Unified `+ / -` button styling, refreshed the table visuals, and updated the help documentation and screenshot.
