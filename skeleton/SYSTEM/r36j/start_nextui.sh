#!/bin/sh
# NextUI Launcher for Batocera on R36J (Allwinner A33)

NEXTUI_DIR="$(cd "$(dirname "$0")" && pwd)"

# Stop EmulationStation and interfering/unused Batocera services to free RAM and prevent input conflicts
/etc/init.d/S90hotkeygen stop >/dev/null 2>&1 || true
/etc/init.d/S60sunxi-joypad stop >/dev/null 2>&1 || true
/etc/init.d/S13irqbalance stop >/dev/null 2>&1 || true
killall -9 power-service thd retro-hotkeys retro-powerkey retro-battery hotkeygen sunxi-joypad irqbalance lircd 2>/dev/null || true

# Disable charging LED (AXP223 REG 0x32 CHGLED -> Hi-Z/off) while console is on
if [ -w /sys/devices/platform/axp22_board/axp22_reg ]; then
	echo 0x32 > /sys/devices/platform/axp22_board/axp22_reg 2>/dev/null || true
	reg32_hex=$(cat /sys/devices/platform/axp22_board/axp22_reg 2>/dev/null | sed -n 's/.*=\([0-9a-fA-F]*\).*/\1/p')
	if [ -n "$reg32_hex" ]; then
		new32=$(( (0x$reg32_hex) & 0x47 ))
		printf '0x32%02x\n' "$new32" > /sys/devices/platform/axp22_board/axp22_reg 2>/dev/null || true
	fi
fi

# Disable status/battery LEDs (PE14 work_led & PL2 bat_led -> Hi-Z + pull-down)
if [ -w /sys/class/sunxi_dump/dump ] && [ -w /sys/class/sunxi_dump/write ]; then
	sunxi_rmw() {
		addr="$1"
		mask="$2"
		val="$3"
		echo "$addr" > /sys/class/sunxi_dump/dump 2>/dev/null || return 0
		cur=$(cat /sys/class/sunxi_dump/dump 2>/dev/null)
		[ -n "$cur" ] || return 0
		next=$(awk -v c="$cur" -v m="$mask" -v v="$val" 'BEGIN {
			c = strtonum(c); m = strtonum(m); v = strtonum(v);
			printf "0x%08x", or(xor(c, and(c, m)), v);
		}')
		[ -n "$next" ] && echo "$addr $next" > /sys/class/sunxi_dump/write 2>/dev/null || true
	}
	sunxi_rmw 0x01c20894 0x07000000 0x07000000 # PE14 mux -> disable (Hi-Z)
	sunxi_rmw 0x01c208ac 0x30000000 0x20000000 # PE14 pull -> pull-down
	sunxi_rmw 0x01f02c00 0x00000700 0x00000700 # PL2 mux -> disable (Hi-Z)
	sunxi_rmw 0x01f02c1c 0x00000030 0x00000020 # PL2 pull -> pull-down
fi
if pidof emulationstation >/dev/null 2>&1; then
	echo "Stopping EmulationStation..."
	/etc/init.d/S31emulationstation stop >/dev/null 2>&1 || killall -15 emulationstation || killall -9 emulationstation
	sleep 1
fi

# Ensure user directory layout exists (and link Capitalized paths used by minarch to Batocera lowercase dirs)
mkdir -p /userdata/bios
mkdir -p /userdata/roms
mkdir -p /userdata/saves
mkdir -p /userdata/cheats
mkdir -p /userdata/overlays
mkdir -p /userdata/.userdata/r36j/logs
mkdir -p /userdata/.system/r36j

ln -snf /userdata/saves /userdata/Saves
ln -snf /userdata/bios /userdata/Bios
ln -snf /userdata/cheats /userdata/Cheats
ln -snf /userdata/overlays /userdata/Overlays

# Set up NextUI Roms directory linking to Batocera's roms
mkdir -p /userdata/Roms
[ -d /userdata/roms/gb ] && ln -snf /userdata/roms/gb "/userdata/Roms/Game Boy (GB)"
[ -d /userdata/roms/gba ] && ln -snf /userdata/roms/gba "/userdata/Roms/Game Boy Advance (GBA)"
[ -d /userdata/roms/gbc ] && ln -snf /userdata/roms/gbc "/userdata/Roms/Game Boy Color (GBC)"
[ -d /userdata/roms/nes ] && ln -snf /userdata/roms/nes "/userdata/Roms/Nintendo Entertainment System (FC)"
[ -d /userdata/roms/famicom ] && ln -snf /userdata/roms/famicom "/userdata/Roms/Famicom (FC)"
[ -d /userdata/roms/snes ] && ln -snf /userdata/roms/snes "/userdata/Roms/Super Nintendo Entertainment System (SFC)"
[ -d /userdata/roms/sfc ] && ln -snf /userdata/roms/sfc "/userdata/Roms/Super Famicom (SFC)"
[ -d /userdata/roms/genesis ] && ln -snf /userdata/roms/genesis "/userdata/Roms/Sega Genesis (MD)"
[ -d /userdata/roms/megadrive ] && ln -snf /userdata/roms/megadrive "/userdata/Roms/Sega Mega Drive (MD)"
[ -d /userdata/roms/mastersystem ] && ln -snf /userdata/roms/mastersystem "/userdata/Roms/Sega Master System (SMS)"
[ -d /userdata/roms/gamegear ] && ln -snf /userdata/roms/gamegear "/userdata/Roms/Sega Game Gear (GG)"
[ -d /userdata/roms/sega32x ] && ln -snf /userdata/roms/sega32x "/userdata/Roms/Sega 32X (32X)"
[ -d /userdata/roms/psx ] && ln -snf /userdata/roms/psx "/userdata/Roms/Sony PlayStation (PS)"
[ -d /userdata/roms/pcengine ] && ln -snf /userdata/roms/pcengine "/userdata/Roms/TurboGrafx-16 (PCE)"
[ -d /userdata/roms/arduboy ] && ln -snf /userdata/roms/arduboy "/userdata/Roms/Arduboy (ARDUBOY)"
[ -d /userdata/roms/ngpc ] && ln -snf /userdata/roms/ngpc "/userdata/Roms/Neo Geo Pocket Color (NGPC)"
[ -d /userdata/roms/fbneo ] && ln -snf /userdata/roms/fbneo "/userdata/Roms/FinalBurn Neo (FBN)"
[ -d /userdata/roms/neogeo ] && ln -snf /userdata/roms/neogeo "/userdata/Roms/Neo Geo (NEOGEO)"
[ -d /userdata/roms/cps1 ] && ln -snf /userdata/roms/cps1 "/userdata/Roms/Capcom Play System 1 (CPS1)"
[ -d /userdata/roms/cps2 ] && ln -snf /userdata/roms/cps2 "/userdata/Roms/Capcom Play System 2 (CPS2)"
[ -d /userdata/roms/cps3 ] && ln -snf /userdata/roms/cps3 "/userdata/Roms/Capcom Play System 3 (CPS3)"
[ -d /userdata/roms/arcade ] && ln -snf /userdata/roms/arcade "/userdata/Roms/Arcade (ARCADE)"
[ -d /userdata/roms/mame2003 ] && ln -snf /userdata/roms/mame2003 "/userdata/Roms/MAME 2003 (MAME2003)"
[ -d /userdata/roms/mame ] && ln -snf /userdata/roms/mame "/userdata/Roms/MAME (MAME)"

# Symlink any pre-installed Batocera libretro cores into NextUI cores directory
for c in /userdata/cores/*_libretro.so; do
	[ -e "$c" ] || continue
	bn="$(basename "$c")"
	if [ ! -e "$NEXTUI_DIR/cores/$bn" ]; then
		ln -snf "$c" "$NEXTUI_DIR/cores/$bn"
	fi
done

# Ensure Tools paks exist for quick menu & main menu
for p in Settings:settings Clock:clock Battery:battery; do
	n="${p%%:*}"
	e="${p##*:}"
	d="/userdata/Tools/r36j/$n.pak"
	if [ ! -f "$d/launch.sh" ]; then
		mkdir -p "$d"
		printf '#!/bin/sh\ncd "$(dirname "$0")" || exit 1\n%s.elf > "$(dirname "$0")/%s.log" 2>&1\n' "$e" "$e" > "$d/launch.sh"
		chmod +x "$d/launch.sh"
	fi
done

# Symlink .system/r36j to this distribution folder if not already linked
if [ ! -e "/userdata/.system/r36j/bin" ]; then
	ln -snf "$NEXTUI_DIR/bin" /userdata/.system/r36j/bin
	ln -snf "$NEXTUI_DIR/lib" /userdata/.system/r36j/lib
	ln -snf "$NEXTUI_DIR/cores" /userdata/.system/r36j/cores
	ln -snf "$NEXTUI_DIR/paks" /userdata/.system/r36j/paks
	ln -snf "$NEXTUI_DIR/res" /userdata/.system/r36j/res
	ln -snf "$NEXTUI_DIR/shaders" /userdata/.system/r36j/shaders
fi

# Symlink NextUI res files and version.txt to .system
mkdir -p /userdata/.system/res
cp -rn "$NEXTUI_DIR/res/"* /userdata/.system/res/ 2>/dev/null || true
[ -f "$NEXTUI_DIR/version.txt" ] && ln -snf "$NEXTUI_DIR/version.txt" /userdata/.system/version.txt

# Remove stale bundled libcrypto so system OpenSSL (e.g. curl for RetroAchievements) matches /usr/lib/libssl.so.3
rm -f "$NEXTUI_DIR/lib/libcrypto.so" "$NEXTUI_DIR/lib/libcrypto.so."* 2>/dev/null || true

# Register out-of-tree RTL8821AU Wi-Fi driver for hotplug and load if attached
if [ -f "$NEXTUI_DIR/lib/8821au.ko" ] && [ -d /lib/modules/3.4.39/extra ]; then
	ln -snf "$NEXTUI_DIR/lib/8821au.ko" /lib/modules/3.4.39/extra/8821au.ko 2>/dev/null || true
	mkdir -p /etc/modprobe.d 2>/dev/null || true
	printf 'options 8821au rtw_wireless_mode=0x0b\n' > /etc/modprobe.d/8821au.conf 2>/dev/null || true
	depmod -a 3.4.39 >/dev/null 2>&1 || true
	if ! grep -q '^8821au ' /proc/modules 2>/dev/null; then
		for m in /sys/bus/usb/devices/*/modalias; do
			[ -r "$m" ] || continue
			mod=$(cat "$m" 2>/dev/null) || continue
			case "$mod" in
				usb:v*p*d*)
					vp="${mod#usb:}"
					vp="${vp%%d*}"
					if grep -qi "^alias usb:${vp}d.* 8821au$" /lib/modules/3.4.39/modules.alias 2>/dev/null; then
						modprobe -q 8821au rtw_wireless_mode=0x0b 2>/dev/null || insmod "$NEXTUI_DIR/lib/8821au.ko" rtw_wireless_mode=0x0b >/dev/null 2>&1 || true
						break
					fi
					;;
			esac
		done
	fi
fi

# Launch NextUI
export COMPAT_SDCARD_PATH="/userdata"
sh "$NEXTUI_DIR/paks/MinUI.pak/launch.sh"

EXIT_STATUS=$?
exit $EXIT_STATUS
