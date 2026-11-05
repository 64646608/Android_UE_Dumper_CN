#!/system/bin/sh
# Stage 1 of APK SDK migration: inventory only (no game memory access).
# Usage: su -c 'sh /data/local/tmp/android_apk_inventory.sh [package_name]'
set -u
PKG="\${1:-com.tencent.tmgp.dfm}"
case "$PKG" in ''|*[!A-Za-z0-9._]*) echo "Invalid package: $PKG"; exit 2;; esac
STAMP="$(date +%Y%m%d_%H%M%S)"
OUT="/data/local/tmp/dfm_inventory_\${STAMP}.txt"
umask 077
{
  echo "DFM SDK version inventory / no live memory reading"
  echo "Time: $(date)"
  echo "Package: $PKG"
  echo "Android: $(getprop ro.build.version.release)"
  echo "ABI: $(getprop ro.product.cpu.abi)"
  echo "----- Version -----"
  dumpsys package "$PKG" 2>/dev/null | grep -m 4 -E 'versionCode=|versionName='
  echo "----- APK SHA256 -----"
  APKS="$(pm path "$PKG" 2>/dev/null | sed -n 's/^package://p')"
  if [ -z "$APKS" ]; then echo 'ERROR: no APK path found'; exit 4; fi
  printf '%s\n' "$APKS" | while IFS= read -r f; do
    [ -r "$f" ] || continue
    echo "APK: $f"
    sha256sum "$f"
  done
  echo "----- Installed UE4 SO SHA256 (if extracted) -----"
  BASEAPK="$(printf '%s\n' "$APKS" | grep '/base.apk$' | head -n 1)"
  if [ -n "$BASEAPK" ]; then
    LIB="$(dirname "$BASEAPK")/lib/arm64/libUE4.so"
    if [ -r "$LIB" ]; then echo "SO: $LIB"; sha256sum "$LIB"; else echo 'SO separate file not available'; fi
  fi
  echo "----- Existing SDK files -----"
  for d in /data/local/tmp/dfm_sdk_* /data/local/tmp/ue4qa; do
    [ -d "$d" ] || continue
    find "$d" -type f \( -name AIOHeader.hpp -o -name Offsets.hpp -o -name Logs.txt \) 2>/dev/null | head -n 20
  done
  echo "Stage 1 complete. A version-matched SDK or symbols are required for layout migration."
} > "$OUT" 2>&1
RC=$?
cat "$OUT"
if cp "$OUT" "/sdcard/Download/dfm_inventory_\${STAMP}.txt" 2>/dev/null; then
  chmod 0644 "/sdcard/Download/dfm_inventory_\${STAMP}.txt" 2>/dev/null || true
  echo "Saved /sdcard/Download/dfm_inventory_\${STAMP}.txt"
fi
exit "$RC"
