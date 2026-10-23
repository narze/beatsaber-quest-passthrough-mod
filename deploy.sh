#!/usr/bin/env bash
# Build the mod, push it to the headset, restart Beat Saber and tail the log.
#
# Usage: ./deploy.sh [--no-launch] [--no-log]
#   ADB_SERIAL=<serial>  pick a device when more than one is connected
set -euo pipefail
cd "$(dirname "$0")"

PKG=com.beatgames.beatsaber
# Early mod: the OpenXR hooks must be in place before the game creates its
# OpenXR instance, and normal mods load after that.
MODS_DIR=/sdcard/ModData/$PKG/Modloader/early_mods
OLD_PATH=/sdcard/ModData/$PKG/Modloader/mods/libPassthrough.so
SO=build/libPassthrough.so
NDK_LINK=$HOME/.local/share/android-ndk-qpm   # cmake rejects NDK paths with spaces
LAUNCH=1 LOG=1
for arg in "$@"; do
  case $arg in
    --no-launch) LAUNCH=0 LOG=0 ;;
    --no-log) LOG=0 ;;
    *) echo "unknown option: $arg" >&2; exit 2 ;;
  esac
done

adb() { command adb ${ADB_SERIAL:+-s "$ADB_SERIAL"} "$@"; }
export PATH=$HOME/.local/share/qpm-bin:$PATH

# 1. Dependencies and NDK
[ -d extern/includes ] || qpm restore
[ -f ndkpath.txt ] || qpm ndk resolve -d
ln -sfn "$(cat ndkpath.txt)" "$NDK_LINK"

# 2. Build
mise exec -- cmake -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_ANDROID_NDK="$NDK_LINK" -B build >/dev/null
mise exec -- cmake --build build

# 3. Push
adb push "$SO" "$MODS_DIR/" | tail -1
adb shell rm -f "$OLD_PATH"   # our own file from the hello-world stage
[ $LAUNCH = 1 ] || exit 0

# 4. Restart the game. Keep it running when the headset is off
#    (proximity sensor override, resets on reboot).
adb shell am broadcast -a com.oculus.vrpowermanager.prox_close >/dev/null
adb shell am force-stop $PKG
adb shell am start -n $PKG/com.unity3d.player.UnityPlayerActivity >/dev/null
# Horizon OS sometimes shows a "Restore App" launch check first. Wait for the
# user to select "Open App" there (never "Restore").
pid=""
for i in $(seq 1 300); do
  pid=$(adb shell pidof $PKG | tr -d '\r') && [ -n "$pid" ] && break
  [ "$i" = 15 ] && echo "waiting for the game... if the headset shows 'Restore App', select 'Open App'"
  sleep 0.2
done
[ -n "$pid" ] || { echo "game did not start within 60 s" >&2; exit 1; }
echo "started pid=$pid"
[ $LOG = 1 ] || exit 0

# 5. Tail our log, OpenXR passthrough messages, and crashes (the crash
#    dump comes from another process, so do not filter those by pid)
adb logcat -T 1 | grep --line-buffered -E \
  " $pid .*(Passthrough|scotland2.*(libPassthrough|Failed)|XR_FB_passthrough)| F DEBUG |libc  *: Fatal"
