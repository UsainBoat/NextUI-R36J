// r36j (Allwinner A33 R36S Clone)

#ifndef PLATFORM_H
#define PLATFORM_H

///////////////////////////////

#ifdef SDL
#	include "sdl.h"
#endif

///////////////////////////////

#define BUTTON_UP		BUTTON_NA
#define BUTTON_DOWN		BUTTON_NA
#define BUTTON_LEFT		BUTTON_NA
#define BUTTON_RIGHT	BUTTON_NA

#define BUTTON_SELECT	BUTTON_NA
#define BUTTON_START	BUTTON_NA

#define BUTTON_A		BUTTON_NA
#define BUTTON_B		BUTTON_NA
#define BUTTON_X		BUTTON_NA
#define BUTTON_Y		BUTTON_NA

#define BUTTON_L1		BUTTON_NA
#define BUTTON_R1		BUTTON_NA
#define BUTTON_L2		BUTTON_NA
#define BUTTON_R2		BUTTON_NA
#define BUTTON_L3		BUTTON_NA
#define BUTTON_R3		BUTTON_NA
#define BUTTON_L4		BUTTON_NA
#define BUTTON_R4		BUTTON_NA

#define BUTTON_MENU		BUTTON_NA
#define BUTTON_MENU_ALT	BUTTON_NA
#define	BUTTON_POWER	116
#define	BUTTON_PLUS		BUTTON_NA
#define	BUTTON_MINUS	BUTTON_NA

///////////////////////////////
// Raw evdev Linux keycodes from gpio-keys driver

#define CODE_UP			544 /* BTN_DPAD_UP */
#define CODE_DOWN		545 /* BTN_DPAD_DOWN */
#define CODE_LEFT		546 /* BTN_DPAD_LEFT */
#define CODE_RIGHT		547 /* BTN_DPAD_RIGHT */

#define CODE_SELECT		314 /* BTN_SELECT */
#define CODE_START		315 /* BTN_START */

#define CODE_A			304 /* BTN_EAST */
#define CODE_B			305 /* BTN_SOUTH */
#define CODE_X			307 /* BTN_NORTH */
#define CODE_Y			308 /* BTN_WEST */

#define CODE_L1			310 /* BTN_TL */
#define CODE_R1			311 /* BTN_TR */
#define CODE_L2			312 /* BTN_TL2 */
#define CODE_R2			313 /* BTN_TR2 */
#define CODE_L3			317 /* BTN_THUMBL */
#define CODE_R3			318 /* BTN_THUMBR */
#define CODE_L4			CODE_NA
#define CODE_R4			CODE_NA

#define CODE_MENU		704 /* BTN_TRIGGER_HAPPY1 / FN */
#define CODE_POWER		116 /* KEY_POWER */

#define CODE_PLUS		115 /* KEY_VOLUMEUP */
#define CODE_MINUS		114 /* KEY_VOLUMEDOWN */

///////////////////////////////
// SDL Joystick Fallback definitions

#define JOY_UP			JOY_NA
#define JOY_DOWN		JOY_NA
#define JOY_LEFT		JOY_NA
#define JOY_RIGHT		JOY_NA

#define JOY_SELECT		6
#define JOY_START		7

#define JOY_A			0
#define JOY_B			1
#define JOY_X			2
#define JOY_Y			3

#define JOY_L1			4
#define JOY_R1			5
#define JOY_L2			8
#define JOY_R2			9
#define JOY_L3			10
#define JOY_R3			11
#define JOY_L4			JOY_NA
#define JOY_R4			JOY_NA

#define JOY_MENU		12
#define JOY_MENU_ALT	JOY_NA
#define JOY_POWER		JOY_NA
#define JOY_PLUS		JOY_NA
#define JOY_MINUS		JOY_NA

///////////////////////////////

#define AXIS_LX			0 /* ABS_X */
#define AXIS_LY			1 /* ABS_Y */
#define AXIS_RX			3 /* ABS_RX */
#define AXIS_RY			4 /* ABS_RY */

///////////////////////////////

#define BTN_FN1			BTN_NONE
#define BTN_FN2			BTN_NONE
#define BTN_FN3			BTN_NONE
#define BTN_FN1_NAME	""
#define BTN_FN2_NAME	""
#define BTN_FN3_NAME	""

///////////////////////////////

#define BTN_RESUME			BTN_X
#define BTN_SLEEP 			BTN_POWER
#define BTN_WAKE 			BTN_POWER
#define BTN_MOD_VOLUME 		BTN_NONE
#define BTN_MOD_BRIGHTNESS 	BTN_MENU
#define BTN_MOD_COLORTEMP 	BTN_SELECT
#define BTN_MOD_PLUS 		BTN_PLUS
#define BTN_MOD_MINUS 		BTN_MINUS

///////////////////////////////

#define FIXED_SCALE 	2
#define FIXED_WIDTH		640
#define FIXED_HEIGHT	480
#define FIXED_BPP		2
#define FIXED_DEPTH		(FIXED_BPP * 8)
#define FIXED_PITCH		(FIXED_WIDTH * FIXED_BPP)
#define FIXED_SIZE		(FIXED_PITCH * FIXED_HEIGHT)

///////////////////////////////

#define MAIN_ROW_COUNT 6
#define QUICK_SWITCHER_COUNT 3
#define PADDING 10

///////////////////////////////

#define SDCARD_PATH "/userdata"
#define MUTE_VOLUME_RAW 0
#define SCREEN_FPS 60.0
#define MAX_LIGHTS 2

///////////////////////////////

#endif
