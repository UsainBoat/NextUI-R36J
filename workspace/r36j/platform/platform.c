// r36j (Allwinner A33 R36S Clone)
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>

#include <msettings.h>

#include "defines.h"
#include "platform.h"
#include "api.h"
#include "utils.h"
#include "config.h"

#include <ctype.h>
#include <time.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int panel_w = 640;
int panel_h = 480;
double panel_fps = 60.0;
int needs_portrait_sdl = 0;
int dev_has_lstick = 1;
int dev_has_rstick = 1;
int dev_has_rgb = 0;
int dev_num_leds = 0;

struct input_event {
	struct timeval time;
	uint16_t type;
	uint16_t code;
	int32_t value;
};

#define R36J_INPUT_COUNT 8
#define R36J_ABS_MAX 8
#define EV_KEY 0x01
#define EV_ABS 0x03

struct r36j_input_absinfo {
	int32_t value;
	int32_t minimum;
	int32_t maximum;
	int32_t fuzz;
	int32_t flat;
	int32_t resolution;
};
#define R36J_EVIOCGABS(abs) _IOR('E', 0x40 + (abs), struct r36j_input_absinfo)

static int input_fds[R36J_INPUT_COUNT];
static int abs_min[R36J_INPUT_COUNT][R36J_ABS_MAX];
static int abs_max[R36J_INPUT_COUNT][R36J_ABS_MAX];
static int abs_valid[R36J_INPUT_COUNT][R36J_ABS_MAX];
static uint32_t last_input_scan = 0;

static void close_evdev_input(int i) {
	if (i < 0 || i >= R36J_INPUT_COUNT || input_fds[i] < 0) return;
	close(input_fds[i]);
	input_fds[i] = -1;
	memset(abs_valid[i], 0, sizeof(abs_valid[i]));
}

static void open_evdev_input(int i) {
	char name_path[128];
	char name[128] = {0};
	snprintf(name_path, sizeof(name_path), "/sys/class/input/event%i/device/name", i);
	getFile(name_path, name, sizeof(name));
	trimTrailingNewlines(name);

	// Skip duplicate userspace uinput mirrors and non-controller sensors
	if (strcmp(name, "sunxi-joypad") == 0 ||
		strcmp(name, "batocera hotkeys") == 0 ||
		strcmp(name, "sunxi-ths") == 0) {
		return;
	}

	char path[64];
	snprintf(path, sizeof(path), "/dev/input/event%i", i);
	input_fds[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
	if (input_fds[i] < 0) return;

	memset(abs_valid[i], 0, sizeof(abs_valid[i]));
	const int axes[] = { AXIS_LX, AXIS_LY, AXIS_RX, AXIS_RY };
	for (size_t a = 0; a < sizeof(axes) / sizeof(axes[0]); a++) {
		int code = axes[a];
		if (code < 0 || code >= R36J_ABS_MAX) continue;
		struct r36j_input_absinfo info;
		if (ioctl(input_fds[i], R36J_EVIOCGABS(code), &info) == 0 && info.maximum > info.minimum) {
			abs_min[i][code] = info.minimum;
			abs_max[i][code] = info.maximum;
			abs_valid[i][code] = 1;
		}
	}
}

static int scale_abs_axis(int fd_idx, int code, int val) {
	int min_v = -900;
	int max_v = 900;
	if (fd_idx >= 0 && fd_idx < R36J_INPUT_COUNT && code >= 0 && code < R36J_ABS_MAX && abs_valid[fd_idx][code]) {
		min_v = abs_min[fd_idx][code];
		max_v = abs_max[fd_idx][code];
	}
	int center = (min_v + max_v) / 2;
	int half = (max_v - min_v) / 2;
	if (half <= 0) return 0;

	int scaled = ((val - center) * 32767) / half;
	if (scaled < -32767) scaled = -32767;
	if (scaled > 32767) scaled = 32767;
	if (scaled > -2500 && scaled < 2500) scaled = 0;
	return scaled;
}

static void scan_evdev_inputs(void) {
	uint32_t now = SDL_GetTicks();
	if (last_input_scan && now - last_input_scan < 2000) return;
	last_input_scan = now;

	for (int i = 0; i < R36J_INPUT_COUNT; i++) {
		char path[64];
		snprintf(path, sizeof(path), "/dev/input/event%i", i);
		int connected = exists(path);
		if (input_fds[i] < 0 && connected) open_evdev_input(i);
		else if (input_fds[i] >= 0 && !connected) close_evdev_input(i);
	}
}

static void apply_button_state(int btn, int id, int pressed, uint32_t tick) {
	if (btn == BTN_NONE || id < 0) return;

	if (!pressed) {
		if (pad.is_pressed & btn) {
			pad.is_pressed &= ~btn;
			pad.just_repeated &= ~btn;
			pad.just_released |= btn;
		}
	} else {
		if (!(pad.is_pressed & btn)) {
			pad.just_pressed |= btn;
			pad.just_repeated |= btn;
			pad.is_pressed |= btn;
			pad.repeat_at[id] = tick + PAD_REPEAT_DELAY;
		}
	}
}

void PLAT_initPlatform(void) {}

void PLAT_initInput(void) {
	for (int i = 0; i < R36J_INPUT_COUNT; i++) input_fds[i] = -1;
	scan_evdev_inputs();

	// Drain any pending/stale boot events (e.g. user holding power button to boot)
	struct input_event ev;
	for (int i = 0; i < R36J_INPUT_COUNT; i++) {
		if (input_fds[i] < 0) continue;
		while (read(input_fds[i], &ev, sizeof(ev)) == sizeof(ev)) {
			// Discard initial buffered events
		}
	}
	memset(&pad, 0, sizeof(pad));
}

void PLAT_quitInput(void) {
	for (int i = 0; i < R36J_INPUT_COUNT; i++) close_evdev_input(i);
}

static uint32_t startup_time = 0;

void PLAT_pollInput(void) {
	pad.just_pressed = BTN_NONE;
	pad.just_released = BTN_NONE;
	pad.just_repeated = BTN_NONE;

	uint32_t tick = SDL_GetTicks();
	if (!startup_time) startup_time = tick;

	for (int i = 0; i < BTN_ID_COUNT; i++) {
		int btn = 1 << i;
		if ((pad.is_pressed & btn) && (tick >= pad.repeat_at[i])) {
			pad.just_repeated |= btn;
			pad.repeat_at[i] += PAD_REPEAT_INTERVAL;
		}
	}

	scan_evdev_inputs();

	struct input_event ev;
	for (int i = 0; i < R36J_INPUT_COUNT; i++) {
		if (input_fds[i] < 0) continue;

		while (read(input_fds[i], &ev, sizeof(ev)) == sizeof(ev)) {
			if (ev.type == EV_KEY) {
				if (ev.value > 1) continue;
				int pressed = ev.value > 0;
				switch (ev.code) {
					case CODE_UP:     apply_button_state(BTN_DPAD_UP, BTN_ID_DPAD_UP, pressed, tick); break;
					case CODE_DOWN:   apply_button_state(BTN_DPAD_DOWN, BTN_ID_DPAD_DOWN, pressed, tick); break;
					case CODE_LEFT:   apply_button_state(BTN_DPAD_LEFT, BTN_ID_DPAD_LEFT, pressed, tick); break;
					case CODE_RIGHT:  apply_button_state(BTN_DPAD_RIGHT, BTN_ID_DPAD_RIGHT, pressed, tick); break;
					case CODE_A:      apply_button_state(BTN_A, BTN_ID_A, pressed, tick); break;
					case CODE_B:      apply_button_state(BTN_B, BTN_ID_B, pressed, tick); break;
					case CODE_X:      apply_button_state(BTN_X, BTN_ID_X, pressed, tick); break;
					case CODE_Y:      apply_button_state(BTN_Y, BTN_ID_Y, pressed, tick); break;
					case CODE_START:  apply_button_state(BTN_START, BTN_ID_START, pressed, tick); break;
					case CODE_SELECT: apply_button_state(BTN_SELECT, BTN_ID_SELECT, pressed, tick); break;
					case CODE_MENU:   apply_button_state(BTN_MENU, BTN_ID_MENU, pressed, tick); break;
					case CODE_L1:     apply_button_state(BTN_L1, BTN_ID_L1, pressed, tick); break;
					case CODE_R1:     apply_button_state(BTN_R1, BTN_ID_R1, pressed, tick); break;
					case CODE_L2:     apply_button_state(BTN_L2, BTN_ID_L2, pressed, tick); break;
					case CODE_R2:     apply_button_state(BTN_R2, BTN_ID_R2, pressed, tick); break;
					case CODE_L3:     apply_button_state(BTN_L3, BTN_ID_L3, pressed, tick); break;
					case CODE_R3:     apply_button_state(BTN_R3, BTN_ID_R3, pressed, tick); break;
					case CODE_PLUS:   apply_button_state(BTN_PLUS, BTN_ID_PLUS, pressed, tick); break;
					case CODE_MINUS:  apply_button_state(BTN_MINUS, BTN_ID_MINUS, pressed, tick); break;
					case CODE_POWER:
						// Ignore power button during the first 3 seconds after boot
						if (tick - startup_time > 3000) {
							apply_button_state(BTN_POWER, BTN_ID_POWER, pressed, tick);
						}
						break;
				}
			} else if (ev.type == EV_ABS) {
				int scaled = scale_abs_axis(i, ev.code, ev.value);
				if (ev.code == AXIS_LX) {
					pad.laxis.x = scaled;
					PAD_setAnalog(BTN_ID_ANALOG_LEFT, BTN_ID_ANALOG_RIGHT, pad.laxis.x, tick + PAD_REPEAT_DELAY);
				} else if (ev.code == AXIS_LY) {
					pad.laxis.y = scaled;
					PAD_setAnalog(BTN_ID_ANALOG_UP, BTN_ID_ANALOG_DOWN, pad.laxis.y, tick + PAD_REPEAT_DELAY);
				} else if (ev.code == AXIS_RX) {
					pad.raxis.x = scaled;
				} else if (ev.code == AXIS_RY) {
					pad.raxis.y = scaled;
				}
			}
		}
	}
}

void PLAT_getBatteryStatusFine(int *is_charging, int *charge) {
	if (is_charging) *is_charging = 0;
	if (charge) *charge = 50;

	char status[32] = {0};
	getFile("/sys/class/power_supply/battery/status", status, sizeof(status));
	if (!status[0]) getFile("/sys/class/power_supply/axp20x-battery/status", status, sizeof(status));
	if (status[0] && is_charging) *is_charging = (strncmp(status, "Charging", 8) == 0);

	char cap[16] = {0};
	getFile("/sys/class/power_supply/battery/capacity", cap, sizeof(cap));
	if (!cap[0]) getFile("/sys/class/power_supply/axp20x-battery/capacity", cap, sizeof(cap));
	if (cap[0] && charge) *charge = atoi(cap);
}

void PLAT_getBatteryStatus(int *is_charging, int *charge) {
	PLAT_getBatteryStatusFine(is_charging, charge);
	if (charge) {
		if (*charge > 80) *charge = 100;
		else if (*charge > 60) *charge = 80;
		else if (*charge > 40) *charge = 60;
		else if (*charge > 20) *charge = 40;
		else if (*charge > 10) *charge = 20;
		else *charge = 10;
	}
}

int PLAT_isUSBConnected(void) {
	DIR *dir = opendir("/sys/class/udc");
	if (!dir) return 0;
	int connected = 0;
	struct dirent *entry;
	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.') continue;
		char path[512];
		snprintf(path, sizeof(path), "/sys/class/udc/%s/state", entry->d_name);
		char state[32] = {0};
		getFile(path, state, sizeof(state));
		if (strncmp(state, "configured", 10) == 0) {
			connected = 1;
			break;
		}
	}
	closedir(dir);
	return connected;
}

void PLAT_getNetworkStatus(int *is_online) {
	if (is_online) *is_online = 0;
}

void PLAT_enableBacklight(int enable) {
	FILE* pf = fopen("/sys/class/backlight/sunxi_backlight/bl_power", "w");
	if (pf) {
		fprintf(pf, "%d\n", enable ? 0 : 4);
		fclose(pf);
	}
	if (!enable) {
		SetRawBrightness(0);
		return;
	}
	int value = GetBrightness();
	int raw = (value * 255) / 10;
	if (raw < 10 && value > 0) raw = 10;
	SetRawBrightness(raw);
}

static void set_axp22_chgled(bool enable) {
	const char *path = "/sys/devices/platform/axp22_board/axp22_reg";
	if (access(path, W_OK) != 0) return;
	putFile((char *)path, "0x32\n");
	char buf[64] = {0};
	getFile((char *)path, buf, sizeof(buf));
	char *eq = strchr(buf, '=');
	if (!eq) return;
	unsigned int cur = (unsigned int)strtoul(eq + 1, NULL, 16);
	unsigned int next = enable ? ((cur & ~0xb8U) | 0x08U) : (cur & ~0xb8U);
	char cmd[32];
	snprintf(cmd, sizeof(cmd), "0x32%02x\n", next & 0xffU);
	putFile((char *)path, cmd);
}

void PLAT_powerOff(int reboot) {
	set_axp22_chgled(true);
	PLAT_enableBacklight(0);
	if (reboot) system("reboot");
	else system("poweroff");
	while (1) pause();
}

int PLAT_supportsDeepSleep(void) { return 0; }

int PLAT_shouldWake(void) {
	scan_evdev_inputs();
	struct input_event ev;
	int should_wake = 0;
	for (int i = 0; i < R36J_INPUT_COUNT; i++) {
		if (input_fds[i] < 0) continue;
		while (read(input_fds[i], &ev, sizeof(ev)) == sizeof(ev)) {
			if (ev.type == EV_KEY && (ev.code == CODE_POWER || ev.code == CODE_MENU) && ev.value == 0) {
				should_wake = 1;
			}
		}
	}
	return should_wake;
}

void PLAT_setCPUSpeed(int speed) {
	const char *gov = "interactive";
	int max_khz = 1200000;
	if (speed == CPU_SPEED_POWERSAVE) {
		gov = "conservative";
		max_khz = 1008000;
	} else if (speed == CPU_SPEED_PERFORMANCE) {
		gov = "performance";
		max_khz = 1344000;
	}
	char max_str[32];
	snprintf(max_str, sizeof(max_str), "%d", max_khz);
	putFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", (char*)gov);
	putFile("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", max_str);
}

void PLAT_setRumble(int strength) {}
int PLAT_pickSampleRate(int requested, int max) { return MIN(requested, max); }
void PLAT_overrideMute(int mute) {}
char* PLAT_getModel(void) { return "R36J (Allwinner A33)"; }
void PLAT_getOsVersionInfo(char* output_str, size_t max_len) {
	snprintf(output_str, max_len, "NextUI r36j");
}

static bool has_wifi_iface(char *ifname_out, size_t ifname_sz);
static void get_iface_ipv4(const char *ifname, char *ip_out, size_t ip_sz);
static void read_proc_wireless(const char *ifname, int *rssi_out, int *noise_out);

ConnectionStrength PLAT_connectionStrength(void) {
	if (!CFG_getWifi()) return SIGNAL_STRENGTH_OFF;
	char ifname[64] = {0};
	if (!has_wifi_iface(ifname, sizeof(ifname))) return SIGNAL_STRENGTH_DISCONNECTED;
	char ip[32] = {0};
	get_iface_ipv4(ifname, ip, sizeof(ip));
	if (!ip[0]) return SIGNAL_STRENGTH_DISCONNECTED;
	int rssi = -60, noise = 0;
	read_proc_wireless(ifname, &rssi, &noise);
	if (rssi >= -60) return SIGNAL_STRENGTH_HIGH;
	if (rssi >= -75) return SIGNAL_STRENGTH_MED;
	return SIGNAL_STRENGTH_LOW;
}

// Timezones & NTP (Batocera /etc/localtime + /etc/timezone + S49ntp)
#define ZONE_PATH "/usr/share/zoneinfo"
#define ZONE_TAB_PATH "/usr/share/zoneinfo/zone.tab"

static char tz_list[MAX_TIMEZONES][MAX_TZ_LENGTH];
static int tz_list_count = 0;

static int cmp_tz(const void *a, const void *b) {
	return strcmp((const char *)a, (const char *)b);
}

static bool is_valid_tz_name(const char *tz) {
	if (!tz || !tz[0] || strlen(tz) >= MAX_TZ_LENGTH) return false;
	if (tz[0] == '/' || strstr(tz, "..")) return false;
	for (const char *p = tz; *p; p++) {
		unsigned char c = (unsigned char)*p;
		if (!(isalnum(c) || c == '/' || c == '_' || c == '-' || c == '+' || c == '.'))
			return false;
	}
	char full[256];
	snprintf(full, sizeof(full), "%s/%s", ZONE_PATH, tz);
	return access(full, F_OK) == 0;
}

static void shell_quote(const char *src, char *dst, size_t dst_sz) {
	size_t j = 0;
	if (dst_sz < 3) {
		if (dst_sz > 0) dst[0] = '\0';
		return;
	}
	dst[j++] = '\'';
	for (size_t i = 0; src && src[i] && j + 5 < dst_sz; i++) {
		if (src[i] == '\'') {
			memcpy(dst + j, "'\\''", 4);
			j += 4;
		} else {
			dst[j++] = src[i];
		}
	}
	dst[j++] = '\'';
	dst[j] = '\0';
}

void PLAT_initTimezones(void) {
	tz_list_count = 0;
	FILE *f = fopen(ZONE_TAB_PATH, "r");
	if (f) {
		char line[512];
		while (fgets(line, sizeof(line), f) && tz_list_count < MAX_TIMEZONES) {
			if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
			// zone.tab columns: country<TAB>coords<TAB>TZ[<TAB>comments]
			char *c1 = strchr(line, '\t');
			if (!c1) continue;
			char *c2 = strchr(c1 + 1, '\t');
			if (!c2) continue;
			char *tz = c2 + 1;
			char *end = tz;
			while (*end && *end != '\t' && *end != '\r' && *end != '\n') end++;
			*end = '\0';
			if (!is_valid_tz_name(tz)) continue;

			bool dup = false;
			for (int i = 0; i < tz_list_count; i++) {
				if (strcmp(tz_list[i], tz) == 0) {
					dup = true;
					break;
				}
			}
			if (!dup) {
				snprintf(tz_list[tz_list_count], MAX_TZ_LENGTH, "%s", tz);
				tz_list_count++;
			}
		}
		fclose(f);
	}
	if (tz_list_count < MAX_TIMEZONES && is_valid_tz_name("UTC")) {
		bool has_utc = false;
		for (int i = 0; i < tz_list_count; i++) {
			if (strcmp(tz_list[i], "UTC") == 0) {
				has_utc = true;
				break;
			}
		}
		if (!has_utc) {
			snprintf(tz_list[tz_list_count++], MAX_TZ_LENGTH, "UTC");
		}
	}
	if (tz_list_count > 1) {
		qsort(tz_list, (size_t)tz_list_count, MAX_TZ_LENGTH, cmp_tz);
	}
}

void PLAT_getTimezones(char timezones[MAX_TIMEZONES][MAX_TZ_LENGTH], int *tz_count) {
	if (tz_list_count == 0) PLAT_initTimezones();
	int count = tz_list_count;
	if (count > MAX_TIMEZONES) count = MAX_TIMEZONES;
	for (int i = 0; i < count; i++) {
		snprintf(timezones[i], MAX_TZ_LENGTH, "%s", tz_list[i]);
	}
	if (tz_count) *tz_count = count;
}

char *PLAT_getCurrentTimezone(void) {
	char linkbuf[256] = {0};
	ssize_t n = readlink("/etc/localtime", linkbuf, sizeof(linkbuf) - 1);
	if (n > 0) {
		linkbuf[n] = '\0';
		const char *marker = "zoneinfo/";
		char *pos = strstr(linkbuf, marker);
		if (pos) {
			pos += strlen(marker);
			if (*pos) return strdup(pos);
		}
	}
	char tzbuf[MAX_TZ_LENGTH] = {0};
	getFile("/etc/timezone", tzbuf, sizeof(tzbuf));
	trimTrailingNewlines(tzbuf);
	if (tzbuf[0]) return strdup(tzbuf);
	return strdup("UTC");
}

void PLAT_setCurrentTimezone(const char* tz) {
	if (!is_valid_tz_name(tz)) return;
	setenv("TZ", tz, 1);
	tzset();

	// Avoid unnecessary overlayfs mutations and batocera-settings writes on startup
	char curlink[256] = {0};
	ssize_t n = readlink("/etc/localtime", curlink, sizeof(curlink) - 1);
	if (n > 0) {
		curlink[n] = '\0';
		const char *marker = "zoneinfo/";
		char *pos = strstr(curlink, marker);
		if (pos && strcmp(pos + strlen(marker), tz) == 0) {
			char curfile[MAX_TZ_LENGTH] = {0};
			getFile("/etc/timezone", curfile, sizeof(curfile));
			trimTrailingNewlines(curfile);
			if (strcmp(curfile, tz) == 0) return;
		}
	}

	char target[256];
	snprintf(target, sizeof(target), "%s/%s", ZONE_PATH, tz);
	unlink("/etc/localtime.nextui");
	if (symlink(target, "/etc/localtime.nextui") == 0) {
		if (rename("/etc/localtime.nextui", "/etc/localtime") != 0) {
			LOG_error("Failed to replace /etc/localtime -> %s: %s\n", target, strerror(errno));
			unlink("/etc/localtime.nextui");
		}
	} else {
		LOG_error("Failed to create temp symlink for /etc/localtime -> %s: %s\n", target, strerror(errno));
	}
	char tzline[MAX_TZ_LENGTH + 2];
	snprintf(tzline, sizeof(tzline), "%s\n", tz);
	putFile("/etc/timezone", tzline);

	char qtz[MAX_TZ_LENGTH * 4 + 8];
	shell_quote(tz, qtz, sizeof(qtz));
	char cmd[512];
	snprintf(cmd, sizeof(cmd), "batocera-settings-set system.timezone %s >/dev/null 2>&1", qtz);
	system(cmd);
}

bool PLAT_getNetworkTimeSync(void) {
	return system("pidof ntpd >/dev/null 2>&1") == 0;
}

void PLAT_setNetworkTimeSync(bool on) {
	if (on) {
		system("/etc/init.d/S49ntp restart >/dev/null 2>&1 &");
	} else {
		system("/etc/init.d/S49ntp stop >/dev/null 2>&1");
	}
}

// WiFi (Batocera ConnMan + USB Wi-Fi dongles)
static pthread_mutex_t wifi_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct WIFI_network wifi_scan_cache[SCAN_MAX_RESULTS];
static int wifi_scan_cache_count = 0;
static uint64_t wifi_last_scan_ms = 0;
static uint64_t wifi_last_cache_ms = 0;

static uint64_t monotonic_ms(void) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)(ts.tv_nsec / 1000000ULL);
}

static bool has_wifi_iface(char *ifname_out, size_t ifname_sz) {
	DIR *d = opendir("/sys/class/net");
	if (!d) return false;
	struct dirent *ent;
	bool found = false;
	while ((ent = readdir(d)) != NULL) {
		if (ent->d_name[0] == '.') continue;
		char wpath[320];
		snprintf(wpath, sizeof(wpath), "/sys/class/net/%s/wireless", ent->d_name);
		if (access(wpath, F_OK) == 0) {
			if (ifname_out && ifname_sz > 0) snprintf(ifname_out, ifname_sz, "%s", ent->d_name);
			found = true;
			break;
		}
		snprintf(wpath, sizeof(wpath), "/sys/class/net/%s/phy80211", ent->d_name);
		if (access(wpath, F_OK) == 0) {
			if (ifname_out && ifname_sz > 0) snprintf(ifname_out, ifname_sz, "%s", ent->d_name);
			found = true;
			break;
		}
	}
	closedir(d);
	return found;
}

static bool is_connman_running(void) {
	return system("pidof connmand >/dev/null 2>&1") == 0;
}

static void ensure_connman_running(bool async) {
	if (is_connman_running()) return;
	if (async) {
		system("/etc/init.d/S08connman start >/dev/null 2>&1 &");
	} else {
		system("/etc/init.d/S08connman start >/dev/null 2>&1");
	}
}

static void ssid_to_hex(const char *ssid, char *hex_out, size_t hex_sz) {
	size_t j = 0;
	for (size_t i = 0; ssid && ssid[i] && j + 3 <= hex_sz; i++) {
		snprintf(hex_out + j, hex_sz - j, "%02x", (unsigned char)ssid[i]);
		j += 2;
	}
	if (j < hex_sz) hex_out[j] = '\0';
}

static void sanitize_ini_value(const char *src, char *dst, size_t dst_sz) {
	size_t j = 0;
	for (size_t i = 0; src && src[i] && j + 1 < dst_sz; i++) {
		if (src[i] == '\r' || src[i] == '\n') continue;
		dst[j++] = src[i];
	}
	if (dst_sz > 0) dst[j] = '\0';
}

static void connman_config_path(const char *ssid, char *path_out, size_t path_sz) {
	char hex[SSID_MAX * 2 + 1];
	ssid_to_hex(ssid, hex, sizeof(hex));
	snprintf(path_out, path_sz, "/var/lib/connman/nextui_%s.config", hex);
}

static WifiSecurityType parse_connman_security(const char *svc) {
	if (!svc) return SECURITY_NONE;
	const char *m = strstr(svc, "_managed_");
	if (!m) return SECURITY_UNSUPPORTED;
	m += 9;
	if (strncmp(m, "psk", 3) == 0) return SECURITY_WPA2_PSK;
	if (strncmp(m, "none", 4) == 0) return SECURITY_NONE;
	if (strncmp(m, "wep", 3) == 0) return SECURITY_WEP;
	return SECURITY_UNSUPPORTED;
}

static bool parse_connman_service_line(char *line, char *status_out, char *ssid_out, size_t ssid_sz, char *svc_out, size_t svc_sz) {
	trimTrailingNewlines(line);
	size_t len = strlen(line);
	if (len < 6) return false;

	// Find last " wifi_" token separating SSID from service id
	char *svc = NULL;
	for (char *p = line; (p = strstr(p, " wifi_")) != NULL; p++) {
		svc = p + 1;
	}
	if (!svc) return false;

	// Validate service id chars ([A-Za-z0-9_])
	char *svc_end = svc;
	while (*svc_end && !isspace((unsigned char)*svc_end)) {
		unsigned char c = (unsigned char)*svc_end;
		if (!(isalnum(c) || c == '_')) return false;
		svc_end++;
	}
	*svc_end = '\0';
	snprintf(svc_out, svc_sz, "%s", svc);

	if (status_out) {
		memcpy(status_out, line, 4);
		status_out[4] = '\0';
	}

	// SSID sits after the 4-char status prefix and before trailing spaces preceding svc
	char *name_start = line + 4;
	while (name_start < svc && *name_start == ' ') name_start++;
	char *name_end = svc - 1;
	while (name_end > name_start && isspace((unsigned char)*(name_end - 1))) name_end--;
	if (name_end <= name_start) return false; // skip hidden/unnamed entries

	size_t name_len = (size_t)(name_end - name_start);
	if (name_len >= ssid_sz) name_len = ssid_sz - 1;
	memcpy(ssid_out, name_start, name_len);
	ssid_out[name_len] = '\0';
	return true;
}

static int connman_service_strength(const char *svc) {
	if (!svc || !svc[0]) return 0;
	char qsvc[256];
	shell_quote(svc, qsvc, sizeof(qsvc));
	char cmd[320];
	snprintf(cmd, sizeof(cmd), "connmanctl services %s 2>/dev/null", qsvc);
	FILE *fp = popen(cmd, "r");
	if (!fp) return 0;
	int strength = 0;
	char line[256];
	while (fgets(line, sizeof(line), fp)) {
		char *p = strstr(line, "Strength = ");
		if (p) {
			strength = atoi(p + 11);
			break;
		}
	}
	pclose(fp);
	return strength;
}

static bool find_connman_service_by_ssid(const char *ssid, char *svc_out, size_t svc_sz, bool *is_known_out) {
	if (!ssid || !ssid[0] || !is_connman_running()) return false;
	FILE *fp = popen("connmanctl services 2>/dev/null", "r");
	if (!fp) return false;
	char line[512];
	bool found = false;
	while (fgets(line, sizeof(line), fp)) {
		char status[5] = {0};
		char entry_ssid[SSID_MAX] = {0};
		char svc[160] = {0};
		if (!parse_connman_service_line(line, status, entry_ssid, sizeof(entry_ssid), svc, sizeof(svc)))
			continue;
		if (strcmp(entry_ssid, ssid) == 0) {
			if (svc_out && svc_sz > 0) snprintf(svc_out, svc_sz, "%s", svc);
			if (is_known_out) *is_known_out = (status[0] == '*');
			found = true;
			break;
		}
	}
	pclose(fp);
	return found;
}

static void get_iface_ipv4(const char *ifname, char *ip_out, size_t ip_sz) {
	if (ip_out && ip_sz > 0) ip_out[0] = '\0';
	if (!ifname || !ifname[0]) return;
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) return;
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	snprintf(ifr.ifr_name, IFNAMSIZ, "%s", ifname);
	if (ioctl(fd, SIOCGIFADDR, &ifr) == 0) {
		struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
		inet_ntop(AF_INET, &sin->sin_addr, ip_out, (socklen_t)ip_sz);
	}
	close(fd);
}

static void read_proc_wireless(const char *ifname, int *rssi_out, int *noise_out) {
	FILE *f = fopen("/proc/net/wireless", "r");
	if (!f) return;
	char line[256];
	char prefix[64];
	snprintf(prefix, sizeof(prefix), "%s:", ifname);
	while (fgets(line, sizeof(line), f)) {
		char *p = line;
		while (*p && isspace((unsigned char)*p)) p++;
		if (strncmp(p, prefix, strlen(prefix)) == 0) {
			unsigned int status = 0;
			double link = 0, level = 0, noise = 0;
			p += strlen(prefix);
			if (sscanf(p, "%x %lf %lf %lf", &status, &link, &level, &noise) >= 3) {
				int rssi = (int)level;
				if (rssi > 0 && rssi <= 100) rssi = -100 + rssi / 2;
				if (rssi_out) *rssi_out = rssi;
				if (noise_out) *noise_out = (int)noise;
			}
			break;
		}
	}
	fclose(f);
}

void PLAT_wifiInit(void) {
	PLAT_wifiDiagnosticsEnable(CFG_getWifiDiagnostics());
	if (CFG_getWifi()) {
		ensure_connman_running(true);
	}
}

bool PLAT_hasWifi(void) {
	return true;
}

bool PLAT_wifiEnabled(void) {
	return CFG_getWifi();
}

void PLAT_wifiEnable(bool on) {
	CFG_setWifi(on);
	char cmd[128];
	snprintf(cmd, sizeof(cmd), "batocera-settings-set wifi.enabled %d >/dev/null 2>&1", on ? 1 : 0);
	system(cmd);
	ensure_connman_running(false);
	if (on) {
		system("connmanctl enable wifi >/dev/null 2>&1");
		system("connmanctl scan wifi >/dev/null 2>&1 &");
	} else {
		system("connmanctl disable wifi >/dev/null 2>&1");
	}
	pthread_mutex_lock(&wifi_mutex);
	wifi_last_scan_ms = 0;
	wifi_last_cache_ms = 0;
	wifi_scan_cache_count = 0;
	pthread_mutex_unlock(&wifi_mutex);
}

int PLAT_wifiScan(struct WIFI_network *networks, int max) {
	if (!networks || max <= 0 || !CFG_getWifi() || !has_wifi_iface(NULL, 0))
		return 0;

	ensure_connman_running(false);

	uint64_t now = monotonic_ms();
	pthread_mutex_lock(&wifi_mutex);
	if (wifi_last_scan_ms == 0 || (now - wifi_last_scan_ms) >= 8000ULL) {
		wifi_last_scan_ms = now;
		pthread_mutex_unlock(&wifi_mutex);
		system("connmanctl scan wifi >/dev/null 2>&1");
		pthread_mutex_lock(&wifi_mutex);
		wifi_last_cache_ms = 0;
	}

	if (wifi_last_cache_ms == 0 || (monotonic_ms() - wifi_last_cache_ms) >= 3000ULL) {
		int cnt = 0;
		FILE *fp = popen("connmanctl services 2>/dev/null", "r");
		if (fp) {
			char line[512];
			while (fgets(line, sizeof(line), fp) && cnt < SCAN_MAX_RESULTS) {
				char status[5] = {0};
				char ssid[SSID_MAX] = {0};
				char svc[160] = {0};
				if (!parse_connman_service_line(line, status, ssid, sizeof(ssid), svc, sizeof(svc)))
					continue;
				bool dup = false;
				for (int i = 0; i < cnt; i++) {
					if (strcmp(wifi_scan_cache[i].ssid, ssid) == 0) {
						dup = true;
						break;
					}
				}
				if (dup) continue;

				struct WIFI_network *net = &wifi_scan_cache[cnt];
				memset(net, 0, sizeof(*net));
				snprintf(net->bssid, sizeof(net->bssid), "%s", svc);
				snprintf(net->ssid, sizeof(net->ssid), "%s", ssid);
				net->security = parse_connman_security(svc);
				int strength = connman_service_strength(svc);
				net->rssi = (strength > 0) ? (-100 + strength / 2) : -90;
				cnt++;
			}
			pclose(fp);
		}
		wifi_scan_cache_count = cnt;
		wifi_last_cache_ms = monotonic_ms();
	}

	int copy_cnt = wifi_scan_cache_count < max ? wifi_scan_cache_count : max;
	if (copy_cnt > 0) {
		memcpy(networks, wifi_scan_cache, (size_t)copy_cnt * sizeof(struct WIFI_network));
	}
	pthread_mutex_unlock(&wifi_mutex);
	return copy_cnt;
}

bool PLAT_wifiConnected(void) {
	struct WIFI_connection info;
	if (PLAT_wifiConnection(&info) < 0) return false;
	return info.valid;
}

int PLAT_wifiConnection(struct WIFI_connection *connection_info) {
	if (!connection_info) return -1;
	memset(connection_info, 0, sizeof(*connection_info));
	if (!CFG_getWifi()) return 0;

	char ifname[64] = {0};
	if (!has_wifi_iface(ifname, sizeof(ifname))) return 0;
	if (!is_connman_running()) return 0;

	FILE *fp = popen("connmanctl services 2>/dev/null", "r");
	if (!fp) return 0;
	char line[512];
	char connected_svc[160] = {0};
	while (fgets(line, sizeof(line), fp)) {
		char status[5] = {0};
		char ssid[SSID_MAX] = {0};
		char svc[160] = {0};
		if (!parse_connman_service_line(line, status, ssid, sizeof(ssid), svc, sizeof(svc)))
			continue;
		if (status[1] == 'O' || status[1] == 'R' || status[2] == 'O' || status[2] == 'R') {
			connection_info->valid = true;
			snprintf(connection_info->ssid, sizeof(connection_info->ssid), "%s", ssid);
			snprintf(connected_svc, sizeof(connected_svc), "%s", svc);
			break;
		}
	}
	pclose(fp);

	if (connection_info->valid) {
		get_iface_ipv4(ifname, connection_info->ip, sizeof(connection_info->ip));
		read_proc_wireless(ifname, &connection_info->rssi, &connection_info->noise);
		if (connection_info->rssi == 0 && connected_svc[0]) {
			int strength = connman_service_strength(connected_svc);
			if (strength > 0) connection_info->rssi = -100 + strength / 2;
		}
	}
	return 0;
}

bool PLAT_wifiHasCredentials(char *ssid, WifiSecurityType sec) {
	(void)sec;
	if (!ssid || !ssid[0]) return false;
	char cfg_path[256];
	connman_config_path(ssid, cfg_path, sizeof(cfg_path));
	if (access(cfg_path, F_OK) == 0) return true;
	bool known = false;
	if (find_connman_service_by_ssid(ssid, NULL, 0, &known) && known) return true;
	return false;
}

void PLAT_wifiForget(char *ssid, WifiSecurityType sec) {
	(void)sec;
	if (!ssid || !ssid[0]) return;
	char cfg_path[256];
	connman_config_path(ssid, cfg_path, sizeof(cfg_path));
	unlink(cfg_path);

	char svc[160] = {0};
	if (find_connman_service_by_ssid(ssid, svc, sizeof(svc), NULL)) {
		char qsvc[256];
		shell_quote(svc, qsvc, sizeof(qsvc));
		char cmd[384];
		snprintf(cmd, sizeof(cmd), "connmanctl disconnect %s >/dev/null 2>&1", qsvc);
		system(cmd);
		snprintf(cmd, sizeof(cmd), "connmanctl config %s --remove >/dev/null 2>&1", qsvc);
		system(cmd);
	}

	FILE *fp = popen("batocera-settings-get wifi.ssid 2>/dev/null", "r");
	if (fp) {
		char saved[SSID_MAX] = {0};
		if (fgets(saved, sizeof(saved), fp)) {
			trimTrailingNewlines(saved);
			if (strcmp(saved, ssid) == 0) {
				system("batocera-settings-set wifi.ssid '' >/dev/null 2>&1");
				system("batocera-settings-set wifi.key '' >/dev/null 2>&1");
				unlink("/var/lib/connman/batocera_wifi.config");
			}
		}
		pclose(fp);
	}

	pthread_mutex_lock(&wifi_mutex);
	wifi_last_cache_ms = 0;
	pthread_mutex_unlock(&wifi_mutex);
}

void PLAT_wifiConnect(char *ssid, WifiSecurityType sec) {
	(void)sec;
	if (!ssid || !ssid[0]) return;
	if (!CFG_getWifi()) PLAT_wifiEnable(true);
	else ensure_connman_running(false);

	char svc[160] = {0};
	if (!find_connman_service_by_ssid(ssid, svc, sizeof(svc), NULL)) {
		system("connmanctl scan wifi >/dev/null 2>&1");
		find_connman_service_by_ssid(ssid, svc, sizeof(svc), NULL);
	}
	if (svc[0]) {
		char qsvc[256];
		shell_quote(svc, qsvc, sizeof(qsvc));
		char cmd[384];
		snprintf(cmd, sizeof(cmd), "connmanctl connect %s >/dev/null 2>&1", qsvc);
		system(cmd);
	}
	pthread_mutex_lock(&wifi_mutex);
	wifi_last_cache_ms = 0;
	pthread_mutex_unlock(&wifi_mutex);
}

void PLAT_wifiConnectPass(const char *ssid, WifiSecurityType sec, const char* pass) {
	if (!ssid || !ssid[0]) return;
	if (!CFG_getWifi()) PLAT_wifiEnable(true);
	else ensure_connman_running(false);

	char clean_ssid[SSID_MAX];
	char clean_pass[256];
	sanitize_ini_value(ssid, clean_ssid, sizeof(clean_ssid));
	sanitize_ini_value(pass ? pass : "", clean_pass, sizeof(clean_pass));

	char hex[SSID_MAX * 2 + 1];
	ssid_to_hex(clean_ssid, hex, sizeof(hex));

	mkdir("/var/lib/connman", 0755);
	char cfg_path[256];
	connman_config_path(clean_ssid, cfg_path, sizeof(cfg_path));
	FILE *f = fopen(cfg_path, "w");
	if (f) {
		fprintf(f, "[global]\nName=nextui\n[service_nextui_%s]\nType=wifi\nName=%s\nHidden=false\n", hex, clean_ssid);
		if (sec != SECURITY_NONE && clean_pass[0]) {
			fprintf(f, "Passphrase=%s\n", clean_pass);
		}
		fprintf(f, "Autoconnect=true\n");
		fclose(f);
	}

	char qssid[SSID_MAX * 4 + 8];
	char qpass[256 * 4 + 8];
	shell_quote(clean_ssid, qssid, sizeof(qssid));
	shell_quote(clean_pass, qpass, sizeof(qpass));
	char cmd[1536];
	snprintf(cmd, sizeof(cmd), "batocera-settings-set wifi.enabled 1 >/dev/null 2>&1; batocera-settings-set wifi.ssid %s >/dev/null 2>&1; batocera-settings-set wifi.key %s >/dev/null 2>&1", qssid, qpass);
	system(cmd);

	usleep(300000);
	PLAT_wifiConnect(clean_ssid, sec);
}

void PLAT_wifiDisconnect(void) {
	if (!is_connman_running()) return;
	FILE *fp = popen("connmanctl services 2>/dev/null", "r");
	if (!fp) return;
	char line[512];
	while (fgets(line, sizeof(line), fp)) {
		char status[5] = {0};
		char ssid[SSID_MAX] = {0};
		char svc[160] = {0};
		if (!parse_connman_service_line(line, status, ssid, sizeof(ssid), svc, sizeof(svc)))
			continue;
		if (status[1] == 'O' || status[1] == 'R' || status[2] == 'O' || status[2] == 'R') {
			char qsvc[256];
			shell_quote(svc, qsvc, sizeof(qsvc));
			char cmd[320];
			snprintf(cmd, sizeof(cmd), "connmanctl disconnect %s >/dev/null 2>&1", qsvc);
			system(cmd);
		}
	}
	pclose(fp);
}

bool PLAT_wifiDiagnosticsEnabled(void) {
	return CFG_getWifiDiagnostics();
}

void PLAT_wifiDiagnosticsEnable(bool on) {
	CFG_setWifiDiagnostics(on);
}

// LED & Turbo
int PLAT_getNumLeds(void) { return 0; }
bool PLAT_canTurbo(void) { return true; }
int PLAT_toggleTurbo(int btn_id) { return 0; }
void PLAT_clearTurbo(void) {}

// Video implementation
#include "generic_video.c"
