#!/bin/sh
# Recover Allwinner android_usb adbd after USB unplug/replug or process exit

USB_ONLINE_SYS="/sys/class/power_supply/usb/online"
LOCK_DIR="/tmp/adbd_recover.lock"
STATE_FILE="/tmp/adbd_usb_online"
STAMP_FILE="/tmp/adbd_recover.stamp"

restart_adbd_debounced() {
	now=$(date +%s 2>/dev/null || echo 0)
	last=0
	[ -f "$STAMP_FILE" ] && last=$(cat "$STAMP_FILE" 2>/dev/null || echo 0)
	if [ "$now" -gt 0 ] && [ "$last" -gt 0 ] && [ $((now - last)) -lt 4 ]; then
		return 0
	fi
	mkdir "$LOCK_DIR" 2>/dev/null || return 0
	echo "$now" > "$STAMP_FILE"
	(
		sleep 1
		online=$(cat "$USB_ONLINE_SYS" 2>/dev/null || echo 0)
		if [ "$online" = "1" ]; then
			/etc/init.d/adbd restart >/dev/null 2>&1 || true
		fi
		rmdir "$LOCK_DIR" 2>/dev/null || true
	) </dev/null >/dev/null 2>&1 &
}

check_transition() {
	[ -r "$USB_ONLINE_SYS" ] || return 0
	cur=$(cat "$USB_ONLINE_SYS" 2>/dev/null || echo 0)
	prev="$cur"
	[ -f "$STATE_FILE" ] && prev=$(cat "$STATE_FILE" 2>/dev/null || echo "$cur")

	if [ "$cur" != "$prev" ]; then
		echo "$cur" > "$STATE_FILE"
		if [ "$cur" = "1" ] && [ "$prev" = "0" ]; then
			restart_adbd_debounced
			return 0
		fi
	fi

	if [ "$cur" = "1" ] && ! pidof adbd >/dev/null 2>&1; then
		restart_adbd_debounced
	fi
}

case "$1" in
	init)
		cat "$USB_ONLINE_SYS" 2>/dev/null > "$STATE_FILE" || echo 0 > "$STATE_FILE"
		;;
	udev)
		check_transition
		;;
	restart)
		rm -f "$STAMP_FILE"
		restart_adbd_debounced
		;;
	watch)
		PID_FILE="/tmp/adbd_recover.pid"
		if [ -f "$PID_FILE" ] && kill -0 "$(cat "$PID_FILE" 2>/dev/null)" 2>/dev/null; then
			exit 0
		fi
		echo $$ > "$PID_FILE"
		cat "$USB_ONLINE_SYS" 2>/dev/null > "$STATE_FILE" || echo 0 > "$STATE_FILE"
		while true; do
			sleep 2
			check_transition
		done
		;;
	*)
		check_transition
		;;
esac
