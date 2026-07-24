#!/usr/bin/env bash
# Assemble a flashable Magisk/KSU Zygisk module zip.
#   usage: scripts/package.sh <path-to-libvcam.so>
# NOTE: if install fails on your KSU device, drop in your proven zygisk-scb scripts/package.sh
# instead (same idea) — the installer stub below is the standard Magisk/KSU-compatible one.
set -euo pipefail

SO="${1:?usage: package.sh <path-to-libvcam.so>}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/dist"
STAGE="$OUT/stage"

rm -rf "$STAGE"; mkdir -p "$STAGE/zygisk" "$STAGE/META-INF/com/google/android"

cp "$ROOT/module/module.prop" "$STAGE/module.prop"
[ -f "$ROOT/module/customize.sh" ] && cp "$ROOT/module/customize.sh" "$STAGE/customize.sh"
cp "$SO" "$STAGE/zygisk/arm64-v8a.so"

# Magisk/KSU module installer stubs
printf '#MAGISK\n' > "$STAGE/META-INF/com/google/android/updater-script"
cat > "$STAGE/META-INF/com/google/android/update-binary" <<'EOF'
#!/sbin/sh
umask 022
OUTFD=$2
ZIPFILE=$3
mount /data 2>/dev/null
# Load util_functions from whichever root is present (Magisk / KernelSU / APatch)
for U in /data/adb/magisk/util_functions.sh \
         /data/adb/ksu/bin/util_functions.sh \
         /data/adb/ap/bin/util_functions.sh; do
  [ -f "$U" ] && { . "$U"; break; }
done
install_module
exit 0
EOF

VER="$(sed -n 's/^version=//p' "$ROOT/module/module.prop" | head -1)"
ZIP="$OUT/vcam-zygisk-${VER:-v0.1}.zip"
rm -f "$ZIP"
( cd "$STAGE" && zip -r9 "$ZIP" . -x '.*' >/dev/null )
echo "packaged: $ZIP"
ls -l "$ZIP"
