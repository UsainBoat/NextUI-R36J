#!/bin/sh
# MinUI.pak launcher for r36j (Allwinner A33)

export PLATFORM="r36j"
SCRIPT_DIR="$(dirname "$0")"

. "$SCRIPT_DIR/../../bin/r36j-init.sh"
r36j_mount_sdcard

export SDCARD_PATH="$COMPAT_SDCARD_PATH"
export BIOS_PATH="$SDCARD_PATH/bios"
export ROMS_PATH="$SDCARD_PATH/roms"
export SAVES_PATH="$SDCARD_PATH/saves"
export CHEATS_PATH="$SDCARD_PATH/cheats"
export SYSTEM_PATH="$SDCARD_PATH/.system/$PLATFORM"
export CORES_PATH="$SYSTEM_PATH/cores"
export USERDATA_PATH="$SDCARD_PATH/.userdata/$PLATFORM"
export SHARED_USERDATA_PATH="$SDCARD_PATH/.userdata/shared"
export LOGS_PATH="$USERDATA_PATH/logs"
export HOOKS_PATH="$USERDATA_PATH/.hooks"
export HOME="$USERDATA_PATH"
LAUNCH_LOG="$LOGS_PATH/launch.txt"

export PATH="$SYSTEM_PATH/bin:/usr/bin:/bin:/usr/sbin:/sbin:$PATH"
export LD_LIBRARY_PATH="$SYSTEM_PATH/lib:/usr/lib:/usr/lib/arm-linux-gnueabihf:/lib/arm-linux-gnueabihf:$LD_LIBRARY_PATH"
r36j_setup_runtime

if [ -f "/tmp/poweroff" ]; then
	poweroff
	exit 0
fi
if [ -f "/tmp/reboot" ]; then
	reboot
	exit 0
fi

mkdir -p "$BIOS_PATH"
mkdir -p "$ROMS_PATH"
mkdir -p "$SAVES_PATH"
mkdir -p "$CHEATS_PATH"
mkdir -p "$USERDATA_PATH"
mkdir -p "$LOGS_PATH"
mkdir -p "$HOOKS_PATH"
mkdir -p "$SHARED_USERDATA_PATH/.minui"

echo "launch: starting $(date)" > "$LAUNCH_LOG"

sh "$SYSTEM_PATH/bin/governor.sh" "auto" 2>/dev/null || true

keymon.elf > "$LOGS_PATH/keymon.txt" 2>&1 &
KEYMON_PID=$!

batmon.elf > "$LOGS_PATH/batmon.txt" 2>&1 &
BATMON_PID=$!

cd "$SCRIPT_DIR"

killall -9 show2.elf > /dev/null 2>&1 || true

EXEC_PATH="/tmp/nextui_exec"
NEXT_PATH="/tmp/next"
touch "$EXEC_PATH" && sync

FAIL_COUNT=0
while [ -f "$EXEC_PATH" ]; do
	echo "launch: starting nextui.elf $(date)" >> "$LAUNCH_LOG"
	nextui.elf > "$LOGS_PATH/nextui.txt" 2>&1
	EXIT_CODE=$?
	echo "launch: nextui.elf exited $EXIT_CODE $(date)" >> "$LAUNCH_LOG"

	if [ -f "$NEXT_PATH" ]; then
		FAIL_COUNT=0
		CMD="$(cat "$NEXT_PATH")"
		eval "$CMD"
		rm -f "$NEXT_PATH"
	elif [ "$EXIT_CODE" -ne 0 ]; then
		FAIL_COUNT=$((FAIL_COUNT + 1))
		echo "launch: nextui.elf failed ($FAIL_COUNT/5), sleeping..." >> "$LAUNCH_LOG"
		sleep 2
		if [ "$FAIL_COUNT" -ge 5 ]; then
			echo "launch: nextui.elf failed 5 times, breaking loop." >> "$LAUNCH_LOG"
			break
		fi
	else
		FAIL_COUNT=0
	fi

	if [ -f "/tmp/poweroff" ]; then
		kill -9 $KEYMON_PID $BATMON_PID >/dev/null 2>&1 || true
		poweroff
		exit 0
	fi
	if [ -f "/tmp/reboot" ]; then
		kill -9 $KEYMON_PID $BATMON_PID >/dev/null 2>&1 || true
		reboot
		exit 0
	fi
done

kill -9 $KEYMON_PID $BATMON_PID >/dev/null 2>&1 || true
