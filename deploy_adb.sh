#!/bin/sh
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DIST="$SCRIPT_DIR/workspace/build/r36j/dist/nextui"
TOOLS="$SCRIPT_DIR/skeleton/EXTRAS/Tools/r36j"

echo "Waiting for ADB device..."
adb wait-for-device

echo "Pushing NextUI binaries, libraries, shaders, and paks..."
adb shell "mkdir -p /userdata/nextui/bin /userdata/nextui/lib /userdata/nextui/paks/Emus /userdata/nextui/shaders /userdata/nextui/res /userdata/Tools/r36j"
adb push "$DIST/bin/." /userdata/nextui/bin/
adb push "$DIST/lib/." /userdata/nextui/lib/
adb push "$DIST/paks/." /userdata/nextui/paks/
adb push "$DIST/shaders/." /userdata/nextui/shaders/
adb push "$DIST/res/." /userdata/nextui/res/
adb push "$DIST/start_nextui.sh" /userdata/nextui/start_nextui.sh
adb push "$DIST/version.txt" /userdata/nextui/version.txt
adb push "$TOOLS/." /userdata/Tools/r36j/

echo "Fixing permissions and running startup setup..."
adb shell "rm -f /userdata/nextui/lib/libcrypto.so /userdata/nextui/lib/libcrypto.so.* && chmod +x /userdata/nextui/bin/* /userdata/nextui/start_nextui.sh /userdata/nextui/paks/MinUI.pak/launch.sh /userdata/nextui/paks/Emus/*.pak/launch.sh /userdata/Tools/r36j/*.pak/launch.sh && sync"

echo "Deployment complete."
