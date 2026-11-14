#!/bin/sh
# Enable Wi-Fi in connmand when a wlan* interface appears and Wi-Fi is enabled in settings

CFG_MINUI="/userdata/.userdata/shared/minuisettings.txt"
if grep -q '^wifi=0$' "$CFG_MINUI" 2>/dev/null; then
	exit 0
fi
if ! grep -q '^wifi=1$' "$CFG_MINUI" 2>/dev/null && \
   ! grep -q '^wifi\.enabled=1$' /userdata/system/batocera.conf 2>/dev/null; then
	exit 0
fi

(
	i=0
	while [ "$i" -lt 15 ]; do
		if connmanctl enable wifi >/dev/null 2>&1; then
			break
		fi
		i=$((i + 1))
		sleep 1
	done
) </dev/null >/dev/null 2>&1 &
