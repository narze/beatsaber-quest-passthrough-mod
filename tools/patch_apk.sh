#!/usr/bin/env bash
# Add com.oculus.feature.PASSTHROUGH to an MBF-patched Beat Saber APK and
# re-sign it with the MBF debug key, so `adb install -r` can update in place
# (no uninstall, app data/OBB/DLC stay).
#
# Usage: tools/patch_apk.sh IN.apk OUT.apk MBF_DEBUG_CERT_PEM
set -euo pipefail

IN=$1 OUT=$2 PEM=$3
BT=${BUILD_TOOLS:-$HOME/Library/Android/sdk/build-tools/36.1.0}
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

unzip -q "$IN" AndroidManifest.xml -d "$WORK"
uv run -q --no-project --with androguard --with pyaxml python -I \
  "$HERE/add_manifest_feature.py" "$WORK/AndroidManifest.xml" "$WORK/new.xml" \
  com.oculus.feature.PASSTHROUGH 2>/dev/null

uv run -q --no-project python -I - "$IN" "$WORK/new.xml" "$WORK/unsigned.apk" <<'PY'
import sys, zipfile
src, manifest, out = sys.argv[1:]
new = open(manifest, "rb").read()
with zipfile.ZipFile(src) as a, zipfile.ZipFile(out, "w") as b:
    for i in a.infolist():
        if i.filename.startswith("META-INF/"):
            continue  # old signature files, apksigner writes new ones
        data = new if i.filename == "AndroidManifest.xml" else a.read(i.filename)
        zi = zipfile.ZipInfo(i.filename, max(i.date_time, (1980, 1, 1, 0, 0, 0)))
        zi.compress_type, zi.external_attr = i.compress_type, i.external_attr
        b.writestr(zi, data)
PY

"$BT/zipalign" -f -P 16 4 "$WORK/unsigned.apk" "$WORK/aligned.apk"
openssl pkcs8 -topk8 -nocrypt -in "$PEM" -outform DER -out "$WORK/key.pk8"
openssl x509 -in "$PEM" -out "$WORK/cert.pem"
mise exec java@temurin-21 -- "$BT/apksigner" sign \
  --v1-signing-enabled false --v2-signing-enabled true --v3-signing-enabled false \
  --key "$WORK/key.pk8" --cert "$WORK/cert.pem" --out "$OUT" "$WORK/aligned.apk"
mise exec java@temurin-21 -- "$BT/apksigner" verify --print-certs "$OUT" | grep 'certificate SHA-256'
"$BT/aapt2" dump xmltree --file AndroidManifest.xml "$OUT" | grep -A2 PASSTHROUGH
