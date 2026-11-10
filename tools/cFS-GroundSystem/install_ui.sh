#!/usr/bin/env bash
set -euo pipefail
SOURCE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
TARGET="${1:-$HOME/adcs_cfs/tools/cFS-GroundSystem}"
if [[ ! -f "$TARGET/GroundSystem.py" || ! -f "$TARGET/RoutingService.py" ]]; then
  echo "LOI: Khong tim thay NASA cFS-GroundSystem tai: $TARGET" >&2
  echo "Vi du: bash install_ui.sh \"\$HOME/adcs_cfs/tools/cFS-GroundSystem\"" >&2
  exit 1
fi
mkdir -p "$TARGET/.unified-ui-backup"
for name in GroundControl.py unified_logic.py test_unified_logic.py; do
  if [[ -f "$TARGET/$name" ]]; then
    cp -p "$TARGET/$name" "$TARGET/.unified-ui-backup/$name.$(date +%Y%m%d-%H%M%S)"
  fi
  cp "$SOURCE_DIR/$name" "$TARGET/$name"
done
chmod +x "$TARGET/GroundControl.py"
echo "DA CAI UI MOI vao: $TARGET"
echo "Khong thay doi GroundSystem.py hay RoutingService.py cua NASA."
echo "Chay: cd '$TARGET' && python3 GroundControl.py"
