# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FP-SNS-STBOX1 is an STM32Cube Function Pack for two STMicroelectronics evaluation boards:
- **STEVAL-MKBOXPRO** (SensorTile.box PRO) — STM32U585 MCU, BlueNRG-LP BLE, sensors: STTS22H, LPS22DF, LSM6DSV16X, LIS2DU12, LIS2MDL
- **STEVAL-STWINBX1** (STWIN.box) — STM32U585 MCU, BlueNRG-2 BLE, sensors: IIS2DLPC, IIS2MDC, IIS3DWB, ISM330DHCX, IIS2ICLX, ILPS22QS, STTS22H

All firmware is bare-metal C targeting STM32U5xx with STM32 HAL.

## Build & Toolchains

No command-line build system (no top-level Makefile/CMake). Projects are built via IDE:
- **STM32CubeIDE** V1.18.1 — projects in `<app>/STM32CubeIDE/` (`.cproject`/`.project`)
- **IAR EWARM** V9.60.3 — projects in `<app>/EWARM/` (`.ewp`/`.eww`)
- **Keil MDK-ARM** V5.38.0 — projects in `<app>/MDK-ARM/` (`.uvprojx`)

STM32CubeMX `.ioc` files are in each application root for regenerating peripheral init code.

**Important**: Keep installation paths short — deep paths cause build failures in some toolchains.

## Repository Structure

```
Projects/
  STEVAL-MKBOXPRO/Applications/
    Rev_A_B/          # Board revisions A & B
    Rev_C/            # Board revision C
  STEVAL-STWINBX1/Applications/
Drivers/
  BSP/
    SensorTileBoxPro/ # BSP for MKBOXPRO
    STWIN.box/        # BSP for STWINBX1
    Components/       # Individual sensor/NFC drivers (PID-based names)
  CMSIS/
  STM32U5xx_HAL_Driver/
Middlewares/
  ST/
    BlueNRG-2/        # BLE stack for STWINBX1
    BlueNRG-LP/       # BLE stack for MKBOXPRO
    STM32WB07_06/     # Additional BLE stack variant
    STM32_BLE_Manager/ # BLE service abstraction layer
    PnPLCompManager/  # PnP-Like Component Manager
    ST25FTM/          # NFC Fast Transfer Mode protocol
    lib_nfc/          # NDEF library
    threadx/          # Azure RTOS ThreadX
    filex/            # Azure RTOS FileX
    cmsis_rtos_threadx/
  Third_Party/
    parson/           # JSON parser
    uzlib/            # Decompression library
```

## Applications per Board

**MKBOXPRO** (Rev_A_B and Rev_C have the same set):
- `BLEDualProgram` — BLE FOTA (firmware over-the-air) with secure PIN connection + NFC pairing
- `BLEMLC` — BLE + Machine Learning Core (LSM6DSV16X MLC/FSM)
- `BLEPiano` — BLE + piezo buzzer piano demo
- `BLESensorsPnPL` — BLE sensor streaming with PnP-Like component model
- `NFC_FTM` — NFC Fast Transfer Mode demo
- `SDDataLogFileX` — SD card data logging via FileX

**STWINBX1**:
- `BLEDefaultFw` — Default BLE firmware
- `BLEMLC` — BLE + Machine Learning Core
- `BLESensorsPnPL` — BLE sensor streaming with PnP-Like
- `ExampleCubeMxDataLog` — CubeMX-generated data logging example
- `NFC_FTM` — NFC Fast Transfer Mode
- `SDDataLogFileX` — SD card data logging

## Application Code Conventions

Each application follows the same layout:
- `Inc/` — headers; `Src/` — source files
- `Inc/stbox1_config.h` — Central config: feature flags, timing, BLE options
- `Inc/main.h` / `Src/main.c` — HAL init, peripheral setup, main loop
- `Src/app_<name>.c` — Application logic entry point
- `Src/ble_implementation.c` — BLE characteristic callbacks
- `Src/ble_function.c` — BLE connection management
- `Src/ota.c` — OTA firmware update logic (when applicable)
- `Binary/` — Pre-built `.bin` files

## Key Configuration Flags (stbox1_config.h)

- `STBOX1_ENABLE_PRINTF` — enable debug output via UART
- `STBOX1_BLE_SECURE_CONNECTION` — enable BLE PIN security (disable for some Android compatibility)
- `BLE_FORCE_RESCAN` — force BLE service re-scan (disable for some Android compatibility)
- `STBOX1_UPDATE_ENV` / `STBOX1_UPDATE_INV` — sensor polling intervals (timer ticks)

## SD Card Data Format (SDDataLogFileX)

The data logging application creates two files per session on the SD card:
- `SensNNN.csv` — sensor CSV at ~100 Hz: timestamp (ms), acc XYZ (mg), gyro XYZ (mdps), mag XYZ (mgauss), pressure (hPa), temperature (°C)
- `MicNNN.wav` — mono 16-bit PCM WAV at 16 kHz from the onboard digital microphone

Logging starts automatically on power-on and can be stopped/restarted with the user button. File counter auto-increments to avoid overwrites. The first 200 ms of audio is discarded (mic glitch workaround). The core logging logic is in `FileX/App/app_filex.c` within each SDDataLogFileX project.

An error log file `Error_Log_Pump_Tsueri_dd.mm.yyyy.log` (compile date) is created on the SD card alongside the sensor data. It logs boot markers and fatal errors with timestamps. The `Error_Handler` writes the error location to this file before halting.

Data collected via the ST BLE Sensor app uses a slightly different format (date/time columns instead of raw ms timestamp).

## Visualization Scripts

Python scripts in `Utilities/scripts/` for plotting sensor data:
- `visualize_sensors.py` — sensor CSV + optional quaternion CSV plotting
- `visualize_pumpfoil.py` — pumpfoil session analysis (cadence spectrogram, movement phases)
- `animate_board_3d.py` — animated board side-view GIF per session from quaternion data

Generated plots go to `png/` with filenames derived from the input CSV (e.g. `plot_quaternions_mirco_7.3.2026.png`).
Quaternion plots use a min:sek time axis based on 120 Hz sample rate.
Pumpfoil sessions are auto-detected by pitch oscillation frequency (>0.3 Hz = pumping, <0.3 Hz = walking).
Each session gets a zoomed plot with quaternions, Euler angles, and nose angle to water.
Nose angle uses rotated sensor Y-axis (Breitachse mounting), 1s median filter (removes magnetometer spikes),
60s rolling median baseline (crash-masked). Drop-in (green dashed line) and end crash (red shaded area) are marked.
Combined nose angle plot includes FFT frequency analysis per session (pump frequency ~1 Hz vs magnetometer drift ~0.1 Hz).
Mast is carbon (non-magnetic), but small metal screws next to sensor box cause hard/soft iron magnetometer interference. Use plastic screws instead. Session drift caused by gyro bias + stale mag calibration without restart.
Raw CSV data lives in `csv/`.

### Board Animation (`animate_board_3d.py`)

Generates per-session animated GIFs showing board orientation in real time:
- **Top panel**: Board side-view (Heck/Nase) tilting and translating vertically according to nose angle, with water surface line
- **Middle panel**: Pump detail at ±5° scale — shows fine pumping oscillations (~3-4°) invisible in the full-range plot
- **Bottom panel**: Full-range nose angle timeline
- All graph lines build up progressively (no future data shown), x-axis grows dynamically with the cursor
- Nose angle uses Butterworth 2 Hz low-pass filter (preserves ~1 Hz pump, removes wobble) and 10s centered rolling median baseline (tracks rider position, removes sensor drift)
- Board translates vertically proportional to angle (0.05m per degree) for visible pump movement
- Drop-in detection: steepest angle in first 10s shown as "Dropwinkel: X.X°" flash (bold red, 2s fade-out)

CLI options for combined MOV output:
```
python animate_board_3d.py quaternion.csv -o mov/ \
  --video camera.MOV --video-offset 56 --sensor-offset 1.5 \
  --session 1 --title "PumpGraph Mirco" --subtitle "ONIX Albatross 1160"
```
Options: `--session N` (single session), `--video` (camera file), `--video-offset`/`--sensor-offset` (sync in seconds), `--title`/`--subtitle` (2s title card, green/lightblue bold text).

Board animation GIFs are saved to `gif/` (e.g. `anim_board_mirco_7.3.2026_session1.gif`).
Combined video+sensor outputs in `mov/` merge the board animation with synchronized camera footage (side by side). The MOV format allows pause/scrub playback. Camera video frames are extracted to `frames/sessionN/` per session.

Note: matplotlib `fig.suptitle()` has a font rendering bug with German characters (umlauts garbled in bold). Workaround: use `ax.set_title()` on the first axes with a two-line title instead.

## Magnetometer Firmware Improvements

The LIS2MDL driver (`Drivers/BSP/Components/lis2mdl/lis2mdl.c`) Init function has three added settings to reduce drift:
- Offset cancellation every ODR cycle (`LIS2MDL_SENS_OFF_CANC_EVERY_ODR`) — continuous offset correction
- Temperature compensation (`lis2mdl_offset_temp_comp_set`) — compensates for thermal drift
- Low-pass filter at ODR/4 (`LIS2MDL_ODR_DIV_4`) — 25 Hz bandwidth at 100 Hz ODR

### Command-Line Build

Makefiles are provided for building firmware without STM32CubeIDE:
- `Projects/STEVAL-MKBOXPRO/Applications/Rev_C/BLESensorsPnPL/STM32CubeIDE/Makefile` → `build/BLESensorsPnPL.bin`
- `Projects/STEVAL-MKBOXPRO/Applications/Rev_C/SDDataLogFileX/STM32CubeIDE/Makefile` → `build/SDDataLogFileX.bin`

The toolchain path is configured in `config.mk` at the repository root:
```makefile
TOOLCHAIN = $(HOME)/.software/arm-gnu-toolchain/bin
```
Both Makefiles include `config.mk` via `-include $(ROOT)/config.mk` with a fallback default. The path can also be overridden per invocation: `make TOOLCHAIN=/other/path`.

## Known Limitations

- Some Android devices have issues with BLE secure PIN connections — disable `STBOX1_BLE_SECURE_CONNECTION` as workaround
- Some Android devices have issues with BLE force rescan — disable `BLE_FORCE_RESCAN` as workaround
- Clean device BLE cache when switching between applications
