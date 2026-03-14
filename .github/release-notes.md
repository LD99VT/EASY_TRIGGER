## What's fixed

- Improved Windows audio device scanning so LTC devices appear much faster.
- Reworked scan flow to avoid opening every audio device during the full enumeration pass.
- Channel counts are now resolved lazily for the selected LTC input/output device instead of blocking the whole scan.
- Windows release build now generates the app icon correctly from the PNG asset when ImageMagick is available.
