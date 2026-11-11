# NextUI for R36S Clones (Allwinner A33 / `r36j` / `sun8iw5p1`)

This repository contains the port of **NextUI** for R36S clones ("Gaminja" / `r36j` board) running on the **Allwinner A33** SoC with **ARM Mali-400 MP2** graphics and Batocera Linux 3.4.39.

---

## Hardware Target

| Component | Details |
| :--- | :--- |
| **Board / SoC** | `r36j` (`sun8iw5p1`) — Allwinner A33 (Quad-core Cortex-A7 @ 1.2–1.34 GHz) |
| **GPU** | ARM Mali-400 MP2 (OpenGL ES 2.0) |
| **RAM** | 256 MB DDR3 |
| **Display** | 640×480 MIPI DSI |
| **Base OS** | Stock Batocera Linux (`3.4.39-armhf-sun8i`) |

---

## Key Port Changes

1. **OpenGL ES 2.0 Compatibility (`workspace/all/common/generic_video.c`)**:
   - Forced GLES 2.0 context creation and `#version 100` shader compilation for the Utgard Mali-400 MP2 GPU.
   - Replaced GLES 3.0 Vertex Array Object (`glGenVertexArrays` / `glBindVertexArray`) and program binary cache (`glProgramBinary` / `glProgramParameteri`) calls with GLES 2.0 VBO + attribute binding.
   - Used native `SDL_PIXELFORMAT_ABGR8888` pixel readback for menu transitions.
2. **R36J Platform HAL (`workspace/r36j/`)**:
   - Raw `evdev` input polling across `/dev/input/event0..7` with separate D-Pad (`EV_KEY`) and analog stick (`EV_ABS`) state tracking.
   - Dynamic CPU governor and frequency scaling (`interactive` / `conservative` / `performance` up to 1.344 GHz).
   - AXP223 PMIC battery status monitoring and backlight control via `libmsettings`.
3. **Batocera Integration (`skeleton/SYSTEM/r36j/start_nextui.sh`)**:
   - Stops EmulationStation and background Batocera daemons (`hotkeygen`, `retro-hotkeys`, `retro-powerkey`, `connmand`, `wpa_supplicant`) to reclaim RAM and prevent input conflicts.
   - Automatically links Batocera's `/userdata/roms/*` directories into `/userdata/Roms/` with NextUI console tags and links pre-installed `/userdata/cores/*_libretro.so` into NextUI.
   - Links case-sensitive `/userdata/Saves`, `/userdata/Bios`, `/userdata/Cheats`, and `/userdata/Overlays` to Batocera's lowercase directories.

---

## Building from Source

Requires Docker:

```bash
chmod +x build_r36j.sh
./build_r36j.sh
```

This builds the `r36j-toolchain` Docker container (Debian Bookworm `armhf` cross-compiler), compiles all NextUI binaries, libraries, and libretro cores, and packages the distribution into `workspace/build/r36j/dist/nextui/`.

---

## Installing / Deploying to the Handheld via ADB

1. Connect the handheld to your PC via the **DC / USB-C** port with ADB enabled.
2. Push the build using the deployment script:
   ```bash
   ./deploy_adb.sh
   ```
3. To boot directly into NextUI instead of EmulationStation:
   ```bash
   adb shell "sed -i 's/^#*system\.es\.atstartup=.*/system.es.atstartup=0/' /userdata/system/batocera.conf"
   adb shell "grep -q start_nextui.sh /userdata/system/custom.sh 2>/dev/null || printf '\n# NextUI startup\nif [ -x /userdata/nextui/start_nextui.sh ]; then\n    /userdata/nextui/start_nextui.sh > /userdata/system/logs/nextui_boot.log 2>&1 &\nfi\n' >> /userdata/system/custom.sh"
   adb shell "chmod +x /userdata/system/custom.sh && sync"
   ```
