#!/usr/bin/env python3
"""Set the HasCustomIcon bit (0x0400) in com.apple.FinderInfo on a DMG volume."""
import struct, subprocess, sys

path = sys.argv[1]
r = subprocess.run(['xattr', '-px', 'com.apple.FinderInfo', path],
                   capture_output=True, text=True)
fi = bytearray.fromhex(r.stdout.strip().replace(' ', '').replace('\n', '')) \
     if r.returncode == 0 and r.stdout.strip() else bytearray(32)
fi = fi + bytearray(max(0, 32 - len(fi)))
flags = struct.unpack('>H', bytes(fi[8:10]))[0] | 0x0400
struct.pack_into('>H', fi, 8, flags)
subprocess.run(['xattr', '-wx', 'com.apple.FinderInfo',
                ' '.join(f'{b:02x}' for b in fi), path], check=True)
