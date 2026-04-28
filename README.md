# FP-SNS-STBOX1 Firmware Package

![latest tag](https://img.shields.io/github/v/tag/STMicroelectronics/fp-sns-stbox1.svg?color=brightgreen)

The FP-SNS-STBOX1 is STM32Cube Function Pack for:
- SensorTile.box PRO discovery box with multi-sensors and wireless connectivity (STEVAL-MKBOXPRO) 
- STWIN.BOX Industrial Node Development Kit (STEVAL-STWINBX1) 
The purpose of this functional pack is to provide simple applications and examples that show how to build custom applications for STEVAL-MKBOXPRO Pro Mode and STEVAL-STWINBX1.

The expansion is built on STM32Cube software technology to ease portability across different STM32 microcontrollers.

**FP-SNS-STBOX1 software features**:

- Complete examples and applications to develop node with BLE connectivity, analog microphone, environmental and motion sensors, and perform real-time monitoring of sensors and audio data
- Example of how create one Boot Loader and one application for allowing Firmware Over the Air update
- Firmware compatible with ST BLE Sensor applications for Android/iOS, to perform sensor data reading, motion algorithm features demo and firmware update (FOTA)
- Easy portability across different MCU families, thanks to STM32Cube
- Free, user-friendly license terms

This firmware package includes Components Device Drivers, Board Support Package and example application for the:

- STMicroelectronics STEVAL-MKBOXPRO (SensorTile.box-Pro)  evaluation board that contains the following components:
  - MEMS sensor devices: STTS22H, LPS22DF, LSM6DSV16X, LIS2DU12, LIS2MDL
  - Dynamic NFC tag: ST25DV04K (board rev. A) - ST25DV64KC (board rev. B and board rev. C)
  - Digital Microphone: MP23db01HP
  
- STMicroelectronics STEVAL-STWINBX1 (STWIN.BOX) evaluation board that contains the following components:
  - MEMS sensor devices: IIS2DLPC, IIS2MDC, IIS3DWB, ISM330DHCX,IIS2ICLX,ILPS22QS,STTS22H
  - analog/digital microphone 
  - ST25dv 64K
  - BlueNRG-2 Bluetooth Low Energy System On Chip

[![The FP-SNS-STBOX1 package contents](_htmresc/FP-SNS-STBOX1_Software_Architecture.png)]()

Here is the list of references to user documents:

- [DB4008: STM32Cube function pack for the Pro Mode of the SensorTile.box wireless multi sensor development kit ](https://www.st.com/resource/en/data_brief/fp-sns-STBOX1.pdf)
- [UM2626: Getting started with the STM32Cube function pack for the Pro Mode of the SensorTile.box wireless multi sensor development kit](https://www.st.com/resource/en/user_manual/um2626-getting-started-with-the-stm32cube-function-pack-for-the-pro-mode-of-the-sensortilebox-wireless-multi-sensor-development-kit-stmicroelectronics.pdf)
- [FP-SNS-STBOX1 Quick Start Guide](https://www.st.com/content/ccc/resource/sales_and_marketing/presentation/product_presentation/group0/5c/4e/96/c2/a6/98/4a/7f/FP-SNS-STBOX1_Quick_Start_Guide/files/FP-SNS-STBOX1_Quick_Start_Guide.pdf/jcr:content/translations/en.FP-SNS-STBOX1_Quick_Start_Guide.pdf)

## Supported Devices and Boards

- STEVAL-MKBOXPRO (SensorTile.box PRO) discovery box with multi-sensors and wireless connectivity for any intelligent IoT node\[[STEVAL-MKBOXPRO](https://www.st.com/en/evaluation-tools/steval-mkboxpro.html)\]
- STEVAL-STWINBX1 (STWIN.box) SensorTile Wireless Industrial Node Development Kit \[[STEVAL-STWINBX1](https://www.st.com/en/evaluation-tools/steval-stwinbx1.html)\]

## SD Card Data Format (SDDataLogFileX)

Logging starts automatically on power-on. Press the user button to stop, press again to restart. An error log (`Error_Log_Pump_Tsueri_dd.mm.yyyy.log`, using compile date) is written to the SD card with boot markers and fatal errors. The FAT directory (file sizes) is flushed every ~1 s during logging so that a sudden power-off still leaves valid, non-zero-byte files — at most the final second of data is lost. Each session creates up to three files on the SD card (GPS file only when a u-blox MAX-M10S module is connected):

### Sensor CSV: `SensNNN.csv`

CSV at ~100 Hz with header:

```
Time [10ms], AccX [mg], AccY [mg], AccZ [mg], GyroX [mdps], GyroY [mdps], GyroZ [mdps],MagX [mgauss],MagY [mgauss],MagZ [mgauss],P [mB],T ['C]
```

The `Time` column is the raw `tx_time_get()` value — **ThreadX ticks, 1 tick = 10 ms**. Divide by 100 for wall-clock seconds. Older recordings use the header `Time [mS]` with the same semantics; it was renamed to `Time [10ms]` in the 22.4.2026 firmware to match reality. Viz scripts accept both.

| Column | Unit | Resolution | Sensor |
|--------|------|------------|--------|
| Time | ThreadX ticks (1 tick = 10ms) | — | System |
| AccX/Y/Z | milli-g | 0.122 mg/LSB (4g FS) | LSM6DSV16X (MKBOXPRO) / ISM330DHCX (STWINBX1) |
| GyroX/Y/Z | milli-degrees/sec | 17.5 mdps/LSB (500 dps FS) | LSM6DSV16X / ISM330DHCX |
| MagX/Y/Z | milli-gauss | 1.5 mgauss/LSB | LIS2MDL (MKBOXPRO) / IIS2MDC (STWINBX1) |
| P | millibar (hPa) | 0.01 hPa | LPS22DF (MKBOXPRO) / ILPS22QS (STWINBX1) |
| T | degrees Celsius | 0.01 °C | STTS22H |

### Audio WAV: `MicNNN.wav` (optional, off by default)

Standard RIFF WAV — mono, 16-bit PCM, 16 kHz sample rate (MP23DB01HP microphone). The file number `NNN` matches the corresponding sensor CSV.

Audio logging is **disabled by default** via `STBOX1_LOG_AUDIO 0` in `stbox1_config.h`. On hardware-modified boards (e.g. the 3.3V mod for the GPS module) `BSP_AUDIO_IN_Init()` has been observed to hang with no return value — which would block the entire logging pipeline. Set `STBOX1_LOG_AUDIO 1` only on an unmodified SensorTile.box PRO. When disabled, no `MicNNN.wav` files are created.

### GPS CSV: `GpsNNN.csv` (optional, u-blox MAX-M10S)

CSV at 10 Hz with header:

```
Time [10ms], UTC, Lat, Lon, Alt [m], Speed [km/h], Course [deg], Fix, NumSat, HDOP
```

| Column | Unit | Source |
|--------|------|--------|
| Time | ThreadX ticks (1 tick = 10 ms) | System |
| UTC | `hhmmss.ss` | `$GNRMC` |
| Lat/Lon | decimal degrees, signed (N/E = +, S/W = −) | `$GNRMC` |
| Alt | metres above mean sea level | `$GNGGA` |
| Speed | km/h (knots × 1.852, **unreliable — see below**) | `$GNRMC` |
| Course | degrees true | `$GNRMC` |
| Fix | 0 = no fix, 1 = GPS, 2 = DGPS | `$GNGGA` |
| NumSat | satellites used | `$GNGGA` |
| HDOP | horizontal dilution of precision | `$GNGGA` |

**Note on the `Speed` column**: the u-blox MAX-M10S's Doppler-based speed measurement was observed to wildly under-report real motion on this board — median 0.12 km/h during the 22.4.2026 Ermioni recording while position-derived speed (haversine on lat/lon deltas) showed sustained 10–30 km/h pumpfoil flight. Root cause not yet isolated (antenna placement, UBX-NAV-PVT mode, or multipath over water). `visualize_combined.py` ignores this column and recomputes speed from position deltas; when writing your own analysis, prefer position-derived speed until the module's speed output is verified against a known reference.

The GPS module (SparkFun u-blox MAX-M10S) connects via UART4 at 38400 baud, 8N1, NMEA:

- GPS TX → PA1 (UART4 RX)
- GPS RX → PA0 (UART4 TX — required for auto-config)
- 3V3 → 3V3
- GND → GND

No manual setup needed — the firmware auto-configures the GPS on every boot by sending UBX-CFG-PRT at 9600 baud (u-blox factory default) to switch UART1 to 38400, then UBX-CFG-MSG to disable the NMEA sentences we don't parse (GLL, GSA, GSV, VTG), then UBX-CFG-RATE to set the measurement rate (default 100 ms = 10 Hz, configurable at build time — see below), then UBX-CFG-CFG to persist the config in BBR + Flash. With only `$GNGGA` + `$GNRMC` enabled, 10 Hz fits in ~1.5 kB/s on the 38400-baud UART; the 25 Hz ceiling is the UART budget. Requires PA0 → GPS RX to be wired so the firmware can talk to the GPS. `STBOX1_ENABLE_PRINTF` is disabled by default because UART4 is now the GPS link — fatal errors still land in the SD error log.

The GPS measurement rate is a build-time knob. The Makefile exposes `GPS_RATE_HZ` (default 10) which is passed to the compiler as `-DGPS_RATE_HZ=<n>` and drives the UBX-CFG-RATE payload:

```sh
# default (10 Hz)
make

# 5 Hz — halves the Gps CSV volume for longer sessions on smaller SD cards
make GPS_RATE_HZ=5

# 25 Hz — maximum supported at 38400 baud
make GPS_RATE_HZ=25
```

Values outside `[1, 25]` trip a compile-time `#error` in `stbox1_config.h`. IDE builds that don't define the macro get the 10 Hz default.

Once the first `$GNRMC` with a valid date+time arrives, `gps_thread` seeds FileX's wall-clock base with the GPS UTC — every file created or flushed after that moment gets today's real FAT timestamp (`gps_nmea.c: GPS_GetWallClock`, `app_filex.c: SetClockBaseFromGPS`). Files written before GPS lock still carry the compile-date fallback. The moment of seeding is logged to the SD error log as `clock: seeded from GPS YYYY-MM-DD HH:MM:SS UTC`. The error-log filename itself is built once at boot from `__DATE__` and is not renamed — only FAT directory entries on new files migrate to GPS time.

NMEA line assembly + sentence parsing runs in `gps_thread` (priority 11), not inside the UART4 RX ISR. The IRQ just pushes incoming bytes into a 1 KB ring buffer (`RxRing[]` in `gps_nmea.c`) and the thread calls `GPS_Process()` once per poll cycle to drain it. Before this split (fixed 24.4.2026), parsing ran in the UART4 IRQ at NVIC priority 6 and preempted the SDMMC1 IRQ (priority 14); at 10 Hz GPS that starved the SD writer enough to drop the sensor rate from 100 Hz to ~7.7 Hz after ~90 s. Now the IRQ path is O(1) per byte and SD transfers are uninterrupted. Validated on Peter's 23.4.2026 15:11 UTC session (fw `build Apr 23 2026 17:04:49`): 82 701 sensor rows across 13.8 min, 82 320 at Δtick = 1 (100 Hz) and no Δtick = 13 runs.

### Battery CSV: `BatNNN.csv` (on by default)

CSV at 1 Hz from the on-board STC3115 fuel gauge on I²C4:

```
Time [10ms], Voltage [mV], SOC [0.1%], Current [100uA]
```

| Column | Unit | Source |
|--------|------|--------|
| Time | ThreadX ticks (1 tick = 10 ms) | System |
| Voltage | millivolts (e.g. 4150 = 4.15 V) | STC3115 |
| SOC | tenths of a percent (e.g. 983 = 98.3 %) | STC3115 |
| Current | 100 µA units, signed (positive = charging, negative = discharging) | STC3115 |

One row per second, written from the same 100-sample flush tick as the sensor data so the FAT directory entry stays up to date. Battery CSV `NNN` matches the corresponding `SensNNN.csv` / `GpsNNN.csv`.

Start-of-session and end-of-session readings also land in the error log as human-readable markers (`start: battery 4150 mV 98.3%` / `stop: battery 3820 mV 61.2%`), so you can see session-over-session drain without opening the CSV.

Gated by `STBOX1_LOG_BATTERY 1` in `stbox1_config.h` (default on). Gauge init runs once per boot on first START_LOG; I²C failure writes a `start: gas gauge init FAIL - continuing without battery log` marker to the error log and skips battery logging for the rest of the boot, without taking sensor/GPS logging down. Set to `0` only if the STC3115 ever hangs the way the MIC did on hardware-modified boards.

### LED Behavior

Boot sequence (every power-on):
1. Brief red flash (power-on default → turned off)
2. **1 green blink** — clock + ICache + LED init OK
3. **2 green blinks** — GPS_Init (UART4 + auto-config) OK
4. **3 green blinks** — sensor init OK
5. **Green LED solid on** — ThreadX + FileX running, logging active

Runtime:
- **Green LED off** — logging stopped (after user-button press)
- **Red LED blinking** — fatal error (`Error_Handler`), details in `Error_Log_Pump_Tsueri_dd.mm.yyyy.log` on SD
- **Red LED solid** — stuck in firmware update (`firmware.bin` on SD), or hang before the green-blink sequence (clock / power / crystal issue)

When stopping, all queued sensor data is written to the file before closing (no data loss). Even without a graceful stop (e.g. user button disconnected, power simply switched off), the periodic 1 Hz `fx_media_flush` keeps the FAT directory up-to-date — the sensor CSV and GPS CSV remain readable up to ~1 s before power loss. The WAV header is only finalized on graceful stop; ungraceful power-off leaves the header pointing at the initial dummy size (60 s), but the audio data itself is still on the card.

File timestamps: FileX has no RTC, so `UpdateFileXClock()` seeds the FAT date/time from `__DATE__` / `__TIME__` plus `tx_time_get()` seconds-since-boot. Files get a stamp close to wall-clock time (instead of FAT's default 31.12.16). Called at FileX init, before each file create, and before each periodic flush, so timestamps advance during a session.

The error log also contains stage markers from START_LOG: `sens header written`, `gps header written`, and (if audio is enabled) `mic init begin` / `mic init ok` / `mic init FAIL`. A hang during boot leaves the last successful marker visible on the SD, which makes it trivial to localize which init step is the problem.

### Firmware Update via SD Card

The firmware can be updated without BLE, JTAG, or ST-Link — just the SD card:

1. Copy `firmware.bin` to the SD card root directory
2. Insert the SD card and power on (or reset) the device
3. The red LED blinks during the update
4. The device automatically reboots with the new firmware
5. `firmware.bin` is renamed to `firmware.done` (prevents re-flashing on next boot)

If no `firmware.bin` is found, normal SD logging starts as usual. The update uses the STM32U585 dual-bank flash: the new firmware is written to the inactive bank, then the banks are swapped. Max firmware size: ~1016 KB.

See [**Documentation/Flash_Firmware_Mac.pdf**](Documentation/Flash_Firmware_Mac.pdf) for the full Mac flashing guide — covers the SD card method plus STM32CubeProgrammer GUI and CLI (`STM32_Programmer_CLI`, `dfu-util`) alternatives.

## Visualization

Rust crate at `Utilities/rust/stbox-viz/` — single binary, no Python/venv required. Build once with `cd Utilities/rust/stbox-viz && cargo build --release` then run from the repo root:

```sh
# Interactive Plotly HTML (map + nose angle + baro height + speed)
./Utilities/rust/stbox-viz/target/release/stbox-viz combined csv/peter_22.4.2026_1250.csv -o html/

# 5-panel sensor summary + quaternion/Euler PNGs
./Utilities/rust/stbox-viz/target/release/stbox-viz sensors csv/peter_22.4.2026_1250.csv -o png/

# Pump cadence spectrogram + movement phase PNGs
./Utilities/rust/stbox-viz/target/release/stbox-viz pumpfoil csv/peter_22.4.2026_1250.csv -o png/

# Animated GIF of board side view per session (needs pumping pitch oscillation)
./Utilities/rust/stbox-viz/target/release/stbox-viz animate csv/mirco_7.3.2026.csv -o gif/ --session 1

# Compass vs GPS course sanity check (LIS2MDL iron-distortion diagnostic)
./Utilities/rust/stbox-viz/target/release/stbox-viz compass csv/peter_22.4.2026_1250.csv -o png/

# Combined camera + sensor MOV (needs ffmpeg in PATH)
./Utilities/rust/stbox-viz/target/release/stbox-viz animate csv/mirco_7.3.2026.csv -o mov/ \
    --session 1 --fps 15 \
    --video camera.MOV --video-offset 56 --sensor-offset 1.5 \
    --title "PumpGraph Mirco" --subtitle "ONIX Albatross 1160"
```

A reader-friendly walkthrough of the full fusion pipeline — which sensor inputs go into Madgwick, why the magnetometer is dropped, and what each downstream consumer does with the quaternion / baro / GPS streams — is at [**Documentation/Sensor_Fusion.pdf**](Documentation/Sensor_Fusion.pdf).

`combined` runs Madgwick 6DOF fusion on raw acc/gyro, detects rides from sustained GPS movement (≥3 km/h, ≥10 s, merged across <30 s gaps), anchors the baro water reference at GPS-stationary samples with temperature compensation, and emits a Plotly HTML using the CDN JS. 23-minute session → 1.2 MB HTML in ~10 s. The HTML opens on the **first ride** by default; a button bar above the chart (`Ride 1` / `Ride 2` / …) swaps between rides — each button updates the map (only that ride's GPS trace is drawn), the shared x-axis range of the time-series panels, and the map centre + zoom. There's deliberately no "All rides" button: showing every on-shore GPS point at once drowned out the actual action, and the full-session x-axis made the time-series panels extend past where any ride actually happened.

The speed panel uses an **acceleration-gated** position-derived speed (haversine on GPS lat/lon deltas). Samples where `|Δspeed|/Δt > 15 km/h/s` (≈4 m/s²) and `v > 15 km/h` are rejected as multipath-induced position jumps before the 5 s rolling median. SUPfoil paddle strokes produce 1–3 m/s², anything above ~4 m/s² implies a ≥4 m position jump inside the module's ~5–10 m horizontal-error envelope. Measured effect on the 22.4.2026 Ermioni session: raw max 83.7 km/h → after gate 29.5 km/h → after smoothing 17.6 km/h. Without the gate the 5 s median alone couldn't suppress 30 km/h multipath spikes.

The height panel shows the baro-derived height (green line, temperature-compensated + GPS-anchored water reference, 250 ms pre-smoothed to kill sensor noise) and a GPS altitude overlay (orange dots, zeroed at the stationary-period median). Y-axis is clamped to the physical mast range `[-0.1, 0.9] m` (0.80 m mast + small noise/overshoot margins); values outside `[-0.15, 0.95] m` are NaN'd so baro thermal drift shows as **gaps** rather than as a line saturated at the axis edge — an honest "here the baro is trustworthy, here it isn't" view. Labels every 10 cm. The GPS overlay can't resolve individual pumps (MAX-M10S vertical scatter ~3–10 m) but makes fix dropouts visible when the board lies flat on water and the antenna loses reception.

Cross-panel inspection: hovering shows a vertical spike line through every panel with a unified tooltip (nose / baro / GPS-alt / speed at the same x). Clicking a time-series point pins a dashed red numbered marker (1, 2, 3, …) across all panels; a "Clear click-marks" button above the chart resets them. A `compass` subcommand (`stbox-viz compass <sensor.csv>`) compares tilt-compensated mag heading against GPS course over ground — a quick quality check for the LIS2MDL on this hardware (currently poor, ~300° p5–p95 span, due to iron distortion from metal mounting screws near the sensor).

`sensors` / `pumpfoil` write 2100×2400 and 2400×1800 PNGs via `plotters` + `rustfft` — sub-second end-to-end for a full 23-minute session.

`animate` writes per-session GIFs directly via `plotters` bitmap + `gif` encoder; the optional combined MOV shells out to `ffmpeg` with the same `hstack` filter + title-card concat pipeline that the old Python version used.

| Flag | Applies to | Description |
|------|------------|-------------|
| `-o` | all | output directory |
| `--beta` | `combined` | Madgwick filter gain (default 0.1, 6DOF) |
| `--fps` | `animate` | GIF frame rate (default 15) |
| `--session N` | `animate` | render only session N |
| `--video FILE` | `animate` | camera video for side-by-side MOV |
| `--video-offset SEC` | `animate` | start time in camera video |
| `--sensor-offset SEC` | `animate` | start time in GIF |
| `--title TEXT` | `animate` | title card (green, bold, 2 s intro) |
| `--subtitle TEXT` | `animate` | subtitle (light blue) |
| `--at HH:MM[:SS]` | `animate` | render one GIF for an exact wall-clock window (bypasses pitch-oscillation session detection) |
| `--tz-offset-h H` | `animate` | UTC offset for `--at` (3 = EEST/Greek summer, 2 = CEST) |
| `--date YYYY-MM-DD` | `animate` | recording date for `--at` (defaults to sensor file mtime) |
| `--duration SEC` | `animate` | length of the `--at` window (default = video duration if `--video`, else 60 s) |
| `--auto-skip` | `animate` | skip carry/transition seconds before pumping starts |
| `--dock-height-m H` | `animate` | dock height above water for the height panel (Ermioni harbour wall = 0.75) |
| `--board-stl FILE` | `animate` | render the side-view panel as a 3D rotating mesh (`/Users/zdavatz/software/fingerfoil/stl/0_combined.stl` is canonical) instead of a 2D line |

CSV column names from the on-device firmware (both `Time [mS]` legacy and the current `Time [10ms]`) are auto-detected. GPS CSV is picked up automatically as `<stem>_gps.csv` next to the sensor CSV.

  The animation automatically detects the steepest board angle during the drop-in (first 10s) and flashes **"Dropwinkel: X.X°"** in bold red, fading out over 2 seconds.

Generated plots are saved to the `png/` directory with filenames derived from the input CSV (e.g. `plot_quaternions_mirco_7.3.2026.png`).
Board animations are saved to `gif/` as `anim_board_*_sessionN.gif`.
Combined video+sensor side-by-side outputs (synced camera footage with board animation) are in `mov/` as MOV files for pause/scrub playback. When `--title`/`--subtitle` are provided, a 2-second title card is prepended.
Quaternion plots display time in min:sek format (120 Hz for BLE data, 100 Hz for SD card data).
Euler angle plots automatically detect and shade **gimbal lock** regions (red, where pitch approaches ±90°) — the sharp roll/yaw spikes in these zones are mathematical artifacts, not real motion.
Pumpfoil sessions are auto-detected (rhythmic pitch oscillation > 0.3 Hz filters out walking).
Each session plot includes quaternion components, Euler angles, and board nose angle relative to the water surface. The nose angle is computed from the rotated sensor Y-axis (sensor mounted in Breitachse = X across board), with a Butterworth 2 Hz low-pass filter (preserves ~1 Hz pump oscillation, removes high-frequency noise) and a 10-second centered rolling median baseline (tracks rider position, removes sensor drift). The board visualization shows both tilt and vertical translation proportional to the nose angle. Drop-in (green dashed line) and end-of-session crash (red shaded area) are marked.
The combined nose angle comparison plot (`plot_nose_angle_*.png`) includes FFT frequency analysis per session to distinguish pump frequency (~1 Hz) from magnetometer drift (~0.1 Hz).

**Sensor mounting note:** The carbon mast does not affect the magnetometer, but small metal screws near the sensor box cause hard/soft iron interference. Use plastic screws or adhesive tape instead. Data quality degrades in later sessions without sensor restart (gyroscope bias drift, stale magnetometer calibration). Restart the sensor between runs for best results.

## Magnetometer Firmware Improvements

The LIS2MDL magnetometer driver (`Drivers/BSP/Components/lis2mdl/lis2mdl.c`) has been modified to reduce drift during long sessions:

1. **Offset cancellation every ODR cycle** — `LIS2MDL_SENS_OFF_CANC_EVERY_ODR` continuously corrects sensor offsets instead of only at power-on
2. **Temperature compensation** — compensates for magnetometer offset drift caused by temperature changes during operation
3. **Low-pass filter at ODR/4** — 25 Hz bandwidth at 100 Hz ODR, filters high-frequency noise for cleaner signal

### Command-Line Build

Makefiles are provided for building firmware without STM32CubeIDE:

```
cd Projects/STEVAL-MKBOXPRO/Applications/Rev_C/BLESensorsPnPL/STM32CubeIDE
make

cd Projects/STEVAL-MKBOXPRO/Applications/Rev_C/SDDataLogFileX/STM32CubeIDE
make
```

Requires ARM GNU Toolchain (`arm-none-eabi-gcc`). Output binaries are in `build/`.

The toolchain path is auto-detected in `config.mk` at the repository root:
- **macOS**: `$(HOME)/.software/arm-gnu-toolchain/bin` (full ARM GNU Toolchain via .pkg)
- **Linux**: `/usr/bin` (system-wide install via apt/dnf)

Override per invocation: `make TOOLCHAIN=/other/path`.

Requires: `pandas`, `numpy`, `matplotlib`, `scipy`.

## Known Limitations

With some Android phones there are some compatibility issues for:

- using the PIN for BLE security connection (BLEDualProgram application). In this case disable the STBOX1_BLE_SECURE_CONNECTION on respective Inc/STBOX1_config.h file

- for forcing a full BLE rescan (BLEMLC). In this case disable the BLE_FORCE_RESCAN on their Inc/STBOX1_config.h files

In all these situations, before to connect to the SensorTile.box PRO, each time you change the running application, clean the Device Cache for forcing a rescan of BLE services.

## Development Toolchains and Compilers

-   IAR Embedded Workbench for ARM (EWARM) toolchain V9.60.3 + STLink/V2 or STLink/V3
-   RealView Microcontroller Development Kit (MDK-ARM) toolchain V5.38.0 + ST-LINK/V2 or STLink/V3
-   Integrated Development Environment for STM32 (STM32CubeIDE) V1.18.1 + ST-LINK or STLink/V3
	
## Dependencies 

This software release is compatible with:

- [**ST BLE Sensor Android application**](https://play.google.com/store/apps/details?id=com.st.bluems)  V5.0.0 (or higher)
- [**ST BLE Sensor iOS application**](https://apps.apple.com/it/app/st-ble-sensor/id993670214)  V5.0.0 (or higher)
