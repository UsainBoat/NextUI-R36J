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
#define EV_KEY 0x01
#define EV_ABS 0x03

static int input_fds[R36J_INPUT_COUNT];
static uint32_t last_input_scan = 0;

static void close_evdev_input(int i) {
	if (i < 0 || i >= R36J_INPUT_COUNT || input_fds[i] < 0) return;
	close(input_fds[i]);
	input_fds[i] = -1;
}

static void open_evdev_input(int i) {
	char path[64];
	snprintf(path, sizeof(path), "/dev/input/event%i", i);
	input_fds[i] = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
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
				// Analog sticks (LRADC AMUX 0-63)
				int val = ev.value;
				if (ev.code == AXIS_LX) {
					apply_button_state(BTN_ANALOG_LEFT, BTN_ID_ANALOG_LEFT, val < 20, tick);
					apply_button_state(BTN_ANALOG_RIGHT, BTN_ID_ANALOG_RIGHT, val > 44, tick);
				} else if (ev.code == AXIS_LY) {
					apply_button_state(BTN_ANALOG_UP, BTN_ID_ANALOG_UP, val < 20, tick);
					apply_button_state(BTN_ANALOG_DOWN, BTN_ID_ANALOG_DOWN, val > 44, tick);
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
	SetRawBrightness(enable ? GetBrightness() : 0);
}

void PLAT_powerOff(int reboot) {
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

ConnectionStrength PLAT_connectionStrength(void) {
	return SIGNAL_STRENGTH_HIGH;
}

// Timezone stubs
#define ZONE_PATH "/usr/share/zoneinfo"
void PLAT_initTimezones(void) {}
void PLAT_getTimezones(char timezones[MAX_TIMEZONES][MAX_TZ_LENGTH], int *tz_count) {
	*tz_count = 0;
}
char *PLAT_getCurrentTimezone(void) {
	return strdup("UTC");
}
void PLAT_setCurrentTimezone(const char* tz) {}
bool PLAT_getNetworkTimeSync(void) { return false; }
void PLAT_setNetworkTimeSync(bool on) {}

// WiFi stubs
void PLAT_wifiInit(void) {}
bool PLAT_hasWifi(void) { return false; }
bool PLAT_wifiEnabled(void) { return false; }
void PLAT_wifiEnable(bool on) {}
int PLAT_wifiScan(struct WIFI_network *networks, int max) { return 0; }
bool PLAT_wifiConnected(void) { return false; }
int PLAT_wifiConnection(struct WIFI_connection *connection_info) { return -1; }
bool PLAT_wifiHasCredentials(char *ssid, WifiSecurityType sec) { return false; }
void PLAT_wifiForget(char *ssid, WifiSecurityType sec) {}
void PLAT_wifiConnect(char *ssid, WifiSecurityType sec) {}
void PLAT_wifiConnectPass(const char *ssid, WifiSecurityType sec, const char* pass) {}
void PLAT_wifiDisconnect(void) {}

// LED & Turbo
int PLAT_getNumLeds(void) { return 0; }
bool PLAT_canTurbo(void) { return true; }
int PLAT_toggleTurbo(int btn_id) { return 0; }
void PLAT_clearTurbo(void) {}

// Video implementation
#include "generic_video.c"
