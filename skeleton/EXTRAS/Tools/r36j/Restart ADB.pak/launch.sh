#!/bin/sh
/etc/init.d/adbd restart > "$(dirname "$0")/restart_adb.log" 2>&1
if ! pidof dropbear >/dev/null 2>&1; then
	for db in /userdata/nextui/bin/dropbear /userdata/.system/r36j/bin/dropbear; do
		if [ -x "$db" ] && [ -f /userdata/system/ssh/dropbear_ed25519_host_key ]; then
			"$db" -r /userdata/system/ssh/dropbear_ed25519_host_key -p 22 </dev/null >/dev/null 2>&1
			break
		fi
	done
fi
