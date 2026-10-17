#!/usr/bin/env bash
set -euo pipefail
# Module-only builds can mix incompatible bridges; always build the suite.
exec bash "$(dirname "$0")/../../build-zig.sh" "$@"
