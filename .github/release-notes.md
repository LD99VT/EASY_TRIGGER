## Easy Trigger 2.7.2

### Trigger behaviour

- Added `PRE / MID / POST` range modes to the `Range` column for both individual rows and layer/group header rows.
- Trigger windows can now be shaped before the trigger time, centered on it, or after it, making timecode landing behaviour much more flexible.
- Trigger matching now also stays active for the full `Duration` of the clip, so jumping into the middle of a playing clip still selects the correct trigger.
- While timecode runs steadily inside the same active window, the trigger does not repeatedly re-fire; it only re-arms after rewind, a seek/jump, signal loss, or leaving and re-entering the window.

### Save / load workflow

- Simplified saving to a single `Save` and `Save As...` flow.
- Both save actions now open one dark modal with `Settings` and `Triggers` checkboxes, both enabled by default.
- Removed the older split `Save Settings / Save Clips / Save All` menu structure.

### Documentation

- Updated the built-in help page to describe the new save dialog flow, the `PRE / MID / POST` range modes, and duration-aware trigger behaviour.
