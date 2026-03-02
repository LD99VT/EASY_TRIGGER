#!/usr/bin/env bash
set -euo pipefail

# ── Build ─────────────────────────────────────────────────────────────────────
cmake --preset macos-universal
cmake --build --preset macos-universal-release

# ── Locate built .app ─────────────────────────────────────────────────────────
APP_PATH=$(find build/macos-universal -type d -name "Easy Trigger.app" | head -n 1)
if [ -z "$APP_PATH" ]; then
  echo "ERROR: Easy Trigger.app not found in build/macos-universal" >&2
  exit 1
fi

# ── Version from CMakeLists.txt ───────────────────────────────────────────────
VERSION=$(grep -m1 'project(.*VERSION' CMakeLists.txt | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')
if [ -z "$VERSION" ]; then
  echo "ERROR: could not parse version from CMakeLists.txt" >&2
  exit 1
fi

# ── Create DMG with volume icon ───────────────────────────────────────────────
VOLNAME="Easy Trigger ${VERSION}"
STAGING=$(mktemp -d)
cp -R "$APP_PATH"   "$STAGING/Easy Trigger.app"
ln -sf /Applications "$STAGING/Applications"

mkdir -p Installer
DMG_RW="Installer/EasyTrigger-${VERSION}-rw.dmg"
DMG_PATH="Installer/EasyTrigger-${VERSION}.dmg"

# Step 1 — read-write DMG (without .VolumeIcon.icns — hdiutil skips hidden files)
hdiutil create \
  -volname "$VOLNAME" \
  -srcfolder "$STAGING" \
  -ov -format UDRW \
  "$DMG_RW"
rm -rf "$STAGING"

# Step 2 — mount; copy icon into the volume, then set flags
hdiutil attach "$DMG_RW" -readwrite -nobrowse > /dev/null
MOUNT="/Volumes/${VOLNAME}"
cp "Icon/Icon Trigger.icns" "$MOUNT/.VolumeIcon.icns"
xcrun SetFile -a V "$MOUNT/.VolumeIcon.icns" 2>/dev/null || true  # make icon file invisible
# Set "Has Custom Icon" bit (0x0400) in com.apple.FinderInfo via xattr
python3 - "$MOUNT" <<'PYEOF'
import struct, subprocess, sys
path = sys.argv[1]
r = subprocess.run(['xattr', '-px', 'com.apple.FinderInfo', path],
                   capture_output=True, text=True)
fi = bytearray.fromhex(r.stdout.strip().replace(' ', '').replace('\n', '')) \
     if r.returncode == 0 and r.stdout.strip() else bytearray(32)
while len(fi) < 32:
    fi.append(0)
flags = struct.unpack('>H', fi[8:10])[0] | 0x0400
struct.pack_into('>H', fi, 8, flags)
hex_val = ' '.join(f'{b:02x}' for b in fi)
subprocess.run(['xattr', '-wx', 'com.apple.FinderInfo', hex_val, path], check=True)
print(f"Set HasCustomIcon on {path}")
PYEOF
hdiutil eject "$MOUNT" -quiet

# Step 3 — compress to final read-only DMG
hdiutil convert "$DMG_RW" -ov -format UDZO -o "$DMG_PATH"
rm -f "$DMG_RW"

echo "Created: $DMG_PATH"
