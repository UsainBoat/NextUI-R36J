#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/stat.h>
#include <string.h>
#include <stddef.h>

#include "displaycal.h"
#include "msettings.h"

#define SETTINGS_VERSION 2
typedef struct Settings {
	int version;
	int brightness;
	int colortemperature;
	int headphones;
	int speaker;
	int contrast;
	int saturation;
	int exposure;
	int unused[2];
	int jack;
	int audiosink;
	int displaycal_enabled;
	int displaycal_red_gain;
	int displaycal_green_gain;
	int displaycal_blue_gain;
	int mute;
	int muted_brightness;
	int muted_colortemperature;
	int muted_contrast;
	int muted_saturation;
	int muted_exposure;
	int muted_volume;
	int mute_disables_dpad;
	int mute_emulates_joystick;
	int turbo_a;
	int turbo_b;
	int turbo_x;
	int turbo_y;
	int turbo_l1;
	int turbo_l2;
	int turbo_r1;
	int turbo_r2;
	int nightmode_mode;
	int nightmode_temp;
	int nightmode_start;
	int nightmode_end;
	int nightmode_fade;
	int nightmode_dim;
} Settings;

static Settings DefaultSettings = {
	.version = SETTINGS_VERSION,
	.brightness = SETTINGS_DEFAULT_BRIGHTNESS,
	.colortemperature = SETTINGS_DEFAULT_COLORTEMP,
	.headphones = SETTINGS_DEFAULT_HEADPHONE_VOLUME,
	.speaker = SETTINGS_DEFAULT_VOLUME,
	.contrast = SETTINGS_DEFAULT_CONTRAST,
	.saturation = SETTINGS_DEFAULT_SATURATION,
	.exposure = SETTINGS_DEFAULT_EXPOSURE,
	.jack = 0,
	.audiosink = AUDIO_SINK_DEFAULT,
	.displaycal_enabled = 0,
	.displaycal_red_gain = 100,
	.displaycal_green_gain = 100,
	.displaycal_blue_gain = 100,
	.mute = 0,
	.muted_brightness = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.muted_colortemperature = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.muted_contrast = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.muted_saturation = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.muted_exposure = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.muted_volume = SETTINGS_DEFAULT_MUTE_NO_CHANGE,
	.mute_disables_dpad = 0,
	.mute_emulates_joystick = 0,
	.turbo_a = 0,
	.turbo_b = 0,
	.turbo_x = 0,
	.turbo_y = 0,
	.turbo_l1 = 0,
	.turbo_l2 = 0,
	.turbo_r1 = 0,
	.turbo_r2 = 0,
	.nightmode_mode = SETTINGS_DEFAULT_NIGHTMODE_MODE,
	.nightmode_temp = SETTINGS_DEFAULT_NIGHTMODE_TEMP,
	.nightmode_start = SETTINGS_DEFAULT_NIGHTMODE_START,
	.nightmode_end = SETTINGS_DEFAULT_NIGHTMODE_END,
	.nightmode_fade = SETTINGS_DEFAULT_NIGHTMODE_FADE,
	.nightmode_dim = SETTINGS_DEFAULT_NIGHTMODE_DIM,
};

static Settings* settings = NULL;
#define SHM_KEY "/SharedSettings"
static char SettingsPath[256];
static int shm_fd = -1;
static int is_host = 0;
static int shm_size = sizeof(Settings);

static void loadSettings(void) {
	FILE *file = fopen(SettingsPath, "rb");
	if (!file) return;

	Settings saved;
	size_t nread = fread(&saved, 1, sizeof(saved), file);
	fclose(file);

	if (nread == sizeof(saved) && saved.version == SETTINGS_VERSION) {
		memcpy(settings, &saved, sizeof(saved));
	} else {
		// Migrate v1 settings files without losing existing preferences.
		size_t v1_size = offsetof(Settings, nightmode_mode);
		if (nread >= v1_size && saved.version == 1) {
			memcpy(settings, &saved, v1_size);
			settings->version = SETTINGS_VERSION;
		}
	}
}

static inline void SaveSettings(void) {
	if (!settings) return;
	int fd = open(SettingsPath, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (fd >= 0) {
		write(fd, settings, shm_size);
		close(fd);
		sync();
	}
}

int InitializedSettings(void) {
	return settings != NULL;
}

void InitSettings(void) {
	const char* userdata = getenv("USERDATA_PATH");
	if (!userdata) userdata = "/userdata";
	snprintf(SettingsPath, sizeof(SettingsPath), "%s/msettings.bin", userdata);

	shm_fd = shm_open(SHM_KEY, O_RDWR | O_CREAT | O_EXCL, 0644);
	if (shm_fd == -1 && errno == EEXIST) {
		shm_fd = shm_open(SHM_KEY, O_RDWR, 0644);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
	} else if (shm_fd >= 0) {
		is_host = 1;
		ftruncate(shm_fd, shm_size);
		settings = mmap(NULL, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
		memcpy(settings, &DefaultSettings, shm_size);
		loadSettings();
	}

	if (is_host) {
		system(
			"amixer -q -c 0 cset name='AIF1IN0L Mux' 'AIF1_DA0L' >/dev/null 2>&1; "
			"amixer -q -c 0 cset name='AIF1IN0R Mux' 'AIF1_DA0R' >/dev/null 2>&1; "
			"amixer -q -c 0 cset name='DACL Mixer AIF1DA0L Switch' on >/dev/null 2>&1; "
			"amixer -q -c 0 cset name='DACR Mixer AIF1DA0R Switch' on >/dev/null 2>&1; "
			"amixer -q -c 0 cset name='HP_L Mux' 'DACL HPL Switch' >/dev/null 2>&1; "
			"amixer -q -c 0 cset name='HP_R Mux' 'DACR HPR Switch' >/dev/null 2>&1; "
			"amixer -q -c 0 cset name='Headphone Switch' on >/dev/null 2>&1"
		);
	}

	SetVolume(GetVolume());
	SetBrightness(GetBrightness());
}

void QuitSettings(void) {
	if (settings && settings != MAP_FAILED) munmap(settings, shm_size);
	if (shm_fd >= 0) close(shm_fd);
	if (is_host) shm_unlink(SHM_KEY);
	settings = NULL;
}

// Brightness
int GetBrightness(void) { return settings ? settings->brightness : SETTINGS_DEFAULT_BRIGHTNESS; }

#define DISP_LCD_SET_BRIGHTNESS 0x102
void SetRawBrightness(int val) {
	if (val < 0) val = 0;
	if (val > 255) val = 255;

	FILE* f = fopen("/sys/class/backlight/sunxi_backlight/brightness", "w");
	if (!f) f = fopen("/sys/class/backlight/pwm-backlight/brightness", "w");
	if (!f) f = fopen("/sys/class/backlight/backlight/brightness", "w");
	if (f) {
		fprintf(f, "%d\n", val);
		fclose(f);
		return;
	}

	int fd = open("/dev/disp", O_RDWR);
	if (fd >= 0) {
		unsigned long param[4] = {0, (unsigned long)val, 0, 0};
		ioctl(fd, DISP_LCD_SET_BRIGHTNESS, &param);
		close(fd);
	}
}

void SetBrightness(int value) {
	if (!settings) return;
	if (value < 0) value = 0;
	if (value > 10) value = 10;
	settings->brightness = value;

	int raw = (value * 255) / 10;
	if (raw < 10 && value > 0) raw = 10;
	SetRawBrightness(raw);
	SaveSettings();
}

// Colortemp
int GetColortemp(void) { return settings ? settings->colortemperature : SETTINGS_DEFAULT_COLORTEMP; }
void SetRawColortemp(int val) {}
void SetColortemp(int value) {
	if (!settings) return;
	settings->colortemperature = value;
	SaveSettings();
}

// Contrast, Saturation, Exposure
int GetContrast(void) { return settings ? settings->contrast : SETTINGS_DEFAULT_CONTRAST; }
void SetRawContrast(int val) {}
void SetContrast(int value) {
	if (!settings) return;
	settings->contrast = value;
	SaveSettings();
}

int GetSaturation(void) { return settings ? settings->saturation : SETTINGS_DEFAULT_SATURATION; }
void SetRawSaturation(int val) {}
void SetSaturation(int value) {
	if (!settings) return;
	settings->saturation = value;
	SaveSettings();
}

int GetExposure(void) { return settings ? settings->exposure : SETTINGS_DEFAULT_EXPOSURE; }
void SetRawExposure(int val) {}
void SetExposure(int value) {
	if (!settings) return;
	settings->exposure = value;
	SaveSettings();
}

// Display calibration
int GetDisplayCalEnabled(void) { return settings ? settings->displaycal_enabled : 0; }
int GetDisplayCalRedGain(void) { return settings ? settings->displaycal_red_gain : 100; }
int GetDisplayCalGreenGain(void) { return settings ? settings->displaycal_green_gain : 100; }
int GetDisplayCalBlueGain(void) { return settings ? settings->displaycal_blue_gain : 100; }

void SetRawDisplayCal(int enabled, int red_gain, int green_gain, int blue_gain) {
	if (enabled) DisplayCal_enableWithValues(red_gain, green_gain, blue_gain);
	else DisplayCal_disable();
}

void SetDisplayCalEnabled(int value) {
	if (!settings) return;
	settings->displaycal_enabled = value;
	SetRawDisplayCal(value, settings->displaycal_red_gain, settings->displaycal_green_gain, settings->displaycal_blue_gain);
	SaveSettings();
}
void SetDisplayCalRedGain(int value) {
	if (!settings) return;
	settings->displaycal_red_gain = value;
	SetRawDisplayCal(settings->displaycal_enabled, value, settings->displaycal_green_gain, settings->displaycal_blue_gain);
	SaveSettings();
}
void SetDisplayCalGreenGain(int value) {
	if (!settings) return;
	settings->displaycal_green_gain = value;
	SetRawDisplayCal(settings->displaycal_enabled, settings->displaycal_red_gain, value, settings->displaycal_blue_gain);
	SaveSettings();
}
void SetDisplayCalBlueGain(int value) {
	if (!settings) return;
	settings->displaycal_blue_gain = value;
	SetRawDisplayCal(settings->displaycal_enabled, settings->displaycal_red_gain, settings->displaycal_green_gain, value);
	SaveSettings();
}

// Volume
int GetVolume(void) {
	if (!settings) return SETTINGS_DEFAULT_VOLUME;
	return settings->jack ? settings->headphones : settings->speaker;
}

void SetRawVolume(int val) {
	if (val < 0) val = 0;
	if (val > 100) val = 100;

	char cmd[256];
	if (val == 0) {
		snprintf(cmd, sizeof(cmd),
			"amixer -q -c 0 cset name='Master' 0%% >/dev/null 2>&1; "
			"amixer -q -c 0 cset name='External Speaker Switch' off >/dev/null 2>&1");
	} else {
		snprintf(cmd, sizeof(cmd),
			"amixer -q -M -c 0 sset Master %d%% >/dev/null 2>&1; "
			"amixer -q -c 0 cset name='headphone volume' 58 >/dev/null 2>&1; "
			"amixer -q -c 0 cset name='External Speaker Switch' on >/dev/null 2>&1",
			val);
	}
	system(cmd);
}

void SetVolume(int value) {
	if (!settings) return;
	if (value < 0) value = 0;
	if (value > 20) value = 20;

	if (settings->jack) settings->headphones = value;
	else settings->speaker = value;

	int raw = (value * 100) / 20;
	SetRawVolume(raw);
	SaveSettings();
}

int GetJack(void) { return settings ? settings->jack : 0; }
void SetJack(int value) {
	if (!settings) return;
	settings->jack = value;
	SetVolume(GetVolume());
}

int GetAudioSink(void) { return settings ? settings->audiosink : AUDIO_SINK_DEFAULT; }
void SetAudioSink(int value) {
	if (!settings) return;
	settings->audiosink = value;
	SaveSettings();
}

int GetHDMI(void) { return 0; }
void SetHDMI(int value) {}

int GetMute(void) { return settings ? settings->mute : 0; }
void SetMute(int value) {
	if (!settings) return;
	settings->mute = value;
	SaveSettings();
}

int GetMutedBrightness(void) { return settings ? settings->muted_brightness : SETTINGS_DEFAULT_MUTE_NO_CHANGE; }
void SetMutedBrightness(int val) { if (settings) { settings->muted_brightness = val; SaveSettings(); } }
int GetMutedColortemp(void) { return settings ? settings->muted_colortemperature : SETTINGS_DEFAULT_MUTE_NO_CHANGE; }
void SetMutedColortemp(int val) { if (settings) { settings->muted_colortemperature = val; SaveSettings(); } }
int GetMutedContrast(void) { return settings ? settings->muted_contrast : SETTINGS_DEFAULT_MUTE_NO_CHANGE; }
void SetMutedContrast(int val) { if (settings) { settings->muted_contrast = val; SaveSettings(); } }
int GetMutedSaturation(void) { return settings ? settings->muted_saturation : SETTINGS_DEFAULT_MUTE_NO_CHANGE; }
void SetMutedSaturation(int val) { if (settings) { settings->muted_saturation = val; SaveSettings(); } }
int GetMutedExposure(void) { return settings ? settings->muted_exposure : SETTINGS_DEFAULT_MUTE_NO_CHANGE; }
void SetMutedExposure(int val) { if (settings) { settings->muted_exposure = val; SaveSettings(); } }
int GetMutedVolume(void) { return settings ? settings->muted_volume : SETTINGS_DEFAULT_MUTE_NO_CHANGE; }
void SetMutedVolume(int val) { if (settings) { settings->muted_volume = val; SaveSettings(); } }

int GetMuteDisablesDpad(void) { return settings ? settings->mute_disables_dpad : 0; }
void SetMuteDisablesDpad(int val) { if (settings) { settings->mute_disables_dpad = val; SaveSettings(); } }
int GetMuteEmulatesJoystick(void) { return settings ? settings->mute_emulates_joystick : 0; }
void SetMuteEmulatesJoystick(int val) { if (settings) { settings->mute_emulates_joystick = val; SaveSettings(); } }

int GetMuteTurboA(void) { return settings ? settings->turbo_a : 0; }
void SetMuteTurboA(int val) { if (settings) { settings->turbo_a = val; SaveSettings(); } }
int GetMuteTurboB(void) { return settings ? settings->turbo_b : 0; }
void SetMuteTurboB(int val) { if (settings) { settings->turbo_b = val; SaveSettings(); } }
int GetMuteTurboX(void) { return settings ? settings->turbo_x : 0; }
void SetMuteTurboX(int val) { if (settings) { settings->turbo_x = val; SaveSettings(); } }
int GetMuteTurboY(void) { return settings ? settings->turbo_y : 0; }
void SetMuteTurboY(int val) { if (settings) { settings->turbo_y = val; SaveSettings(); } }
int GetMuteTurboL1(void) { return settings ? settings->turbo_l1 : 0; }
void SetMuteTurboL1(int val) { if (settings) { settings->turbo_l1 = val; SaveSettings(); } }
int GetMuteTurboL2(void) { return settings ? settings->turbo_l2 : 0; }
void SetMuteTurboL2(int val) { if (settings) { settings->turbo_l2 = val; SaveSettings(); } }
int GetMuteTurboR1(void) { return settings ? settings->turbo_r1 : 0; }
void SetMuteTurboR1(int val) { if (settings) { settings->turbo_r1 = val; SaveSettings(); } }
int GetMuteTurboR2(void) { return settings ? settings->turbo_r2 : 0; }
void SetMuteTurboR2(int val) { if (settings) { settings->turbo_r2 = val; SaveSettings(); } }

int GetNightMode(void) { return settings ? settings->nightmode_mode : SETTINGS_DEFAULT_NIGHTMODE_MODE; }
void SetNightMode(int val) {
	if (!settings) return;
	if (val < NIGHTMODE_OFF) val = NIGHTMODE_OFF;
	if (val > NIGHTMODE_SCHEDULED) val = NIGHTMODE_SCHEDULED;
	settings->nightmode_mode = val;
	SaveSettings();
}

int GetNightModeTemp(void) { return settings ? settings->nightmode_temp : SETTINGS_DEFAULT_NIGHTMODE_TEMP; }
void SetNightModeTemp(int val) {
	if (!settings) return;
	if (val < 2000) val = 2000;
	if (val > 6500) val = 6500;
	settings->nightmode_temp = val;
	SaveSettings();
}

int GetNightModeStart(void) { return settings ? settings->nightmode_start : SETTINGS_DEFAULT_NIGHTMODE_START; }
void SetNightModeStart(int val) {
	if (!settings) return;
	val %= 1440;
	if (val < 0) val += 1440;
	settings->nightmode_start = val;
	SaveSettings();
}

int GetNightModeEnd(void) { return settings ? settings->nightmode_end : SETTINGS_DEFAULT_NIGHTMODE_END; }
void SetNightModeEnd(int val) {
	if (!settings) return;
	val %= 1440;
	if (val < 0) val += 1440;
	settings->nightmode_end = val;
	SaveSettings();
}

int GetNightModeFade(void) { return settings ? settings->nightmode_fade : SETTINGS_DEFAULT_NIGHTMODE_FADE; }
void SetNightModeFade(int val) {
	if (!settings) return;
	if (val < 0) val = 0;
	if (val > 120) val = 120;
	settings->nightmode_fade = val;
	SaveSettings();
}

int GetNightModeDim(void) { return settings ? settings->nightmode_dim : SETTINGS_DEFAULT_NIGHTMODE_DIM; }
void SetNightModeDim(int val) {
	if (!settings) return;
	if (val < 40) val = 40;
	if (val > 100) val = 100;
	settings->nightmode_dim = val;
	SaveSettings();
}
