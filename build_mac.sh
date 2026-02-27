#!/usr/bin/env bash
set -euo pipefail

cmake --preset macos-universal
cmake --build --preset macos-universal-release

