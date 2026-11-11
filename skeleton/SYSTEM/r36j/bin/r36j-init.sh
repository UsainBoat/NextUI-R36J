#!/bin/sh
# r36j (Allwinner A33 R36S Clone) NextUI runtime initialization

r36j_mount_sdcard() {
	if [ -d "/userdata" ]; then
		# Batocera Linux runtime
		export COMPAT_SDCARD_PATH="/userdata"
	elif [ -d "/mnt/sdcard" ]; then
		export COMPAT_SDCARD_PATH="/mnt/sdcard"
	elif [ -d "/mnt/SDCARD" ]; then
		export COMPAT_SDCARD_PATH="/mnt/SDCARD"
	else
		export COMPAT_SDCARD_PATH="$(pwd)"
	fi

	mkdir -p "$COMPAT_SDCARD_PATH"
}

r36j_setup_runtime() {
	export SDL_AUDIODRIVER="alsa"
	export SDL_JOYSTICK_DISABLE_UDEV=1
	export SDL_HIDAPI_JOYSTICK_DISABLE_UDEV=1

	# Find available GLES / EGL libraries if not already in system lib
	for egl_path in /usr/lib/libEGL.so /usr/lib/libEGL.so.1 /usr/lib/arm-linux-gnueabihf/libEGL.so /usr/lib/arm-linux-gnueabihf/libEGL.so.1; do
		if [ -e "$egl_path" ]; then
			export SDL_VIDEO_EGL_DRIVER="$egl_path"
			break
		fi
	done
	for gles_path in /usr/lib/libGLESv2.so /usr/lib/libGLESv2.so.2 /usr/lib/arm-linux-gnueabihf/libGLESv2.so /usr/lib/arm-linux-gnueabihf/libGLESv2.so.2; do
		if [ -e "$gles_path" ]; then
			export SDL_VIDEO_GL_DRIVER="$gles_path"
			break
		fi
	done
}
