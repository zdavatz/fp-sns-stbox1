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
- `STBOX1_LOG_AUDIO` (SDDataLogFileX only) — gates all `BSP_AUDIO_IN_*` calls and `.wav` file creation. Default `0`. Set to `1` only on unmodified hardware. On the 3.3V-modded board `BSP_AUDIO_IN_Init` blocks with no return, which hangs `fx_thread` mid-START_LOG before it can write sensor or GPS samples. Keep it off unless you actively need the on-board microphone.
- `STBOX1_LOG_BATTERY` (SDDataLogFileX only) — gates the STC3115 fuel-gauge path and `BatNNN.csv` file creation. Default `1`. Gauge init (`BSP_GG_Init`) runs once per boot on first START_LOG; I²C failure is non-fatal — writes a marker to the error log and skips battery logging for the remainder of the boot, without taking sensor/GPS logging down. Set to `0` only if the STC3115 ever hangs the way the MIC did on hardware-modified boards.

## SD Card Data Format (SDDataLogFileX)

The data logging application creates up to four files per session on the SD card:
- `SensNNN.csv` — sensor CSV at ~100 Hz: timestamp (ticks), acc XYZ (mg), gyro XYZ (mdps), mag XYZ (mgauss), pressure (hPa), temperature (°C)
- `MicNNN.wav` — mono 16-bit PCM WAV at 16 kHz from the onboard digital microphone
- `GpsNNN.csv` — GPS fixes at 10 Hz from u-blox MAX-M10S: timestamp (ticks), UTC (hhmmss.ss), lat/lon (decimal degrees, signed), alt (m), speed (km/h), course (deg), fix quality, num satellites, HDOP. Rows only when a new fix is parsed. Empty (header only) if GPS has no fix or module not connected.
- `BatNNN.csv` — battery at 1 Hz from STC3115 fuel gauge on I²C4: timestamp (ticks), voltage (mV), SOC (0.1%), current (100 µA, signed: positive = charging). Written from the same `COMMAND_SAVE_SENSORS` flush tick as the sensor data, so ungraceful power-off never leaves a 0-byte battery CSV. Start-of-session and end-of-session readings also land in the error log as `start: battery 4150 mV 98.3%` / `stop: battery 3820 mV 61.2%` markers.

Gyroscope full-scale is 500 dps (17.5 mdps/LSB) for good fusion resolution. Accelerometer is 4g (0.122 mg/LSB).
Timestamps are ThreadX tick counts (1 tick = 10ms), not raw milliseconds.

Logging starts automatically on power-on and can be stopped/restarted with the user button. File counter auto-increments to avoid overwrites. The first 200 ms of audio is discarded (mic glitch workaround). The core logging logic is in `FileX/App/app_filex.c` within each SDDataLogFileX project.

LED behavior: On every boot main() runs `BootStageBlink(n)` — 1 green blink after clock/ICache/LED init, 2 after `GPS_Init()`, 3 after `InitMemsSensors()`. Then ThreadX starts and green LED goes solid on = logging active. Green off = logging stopped. Red LED blinking = `Error_Handler` fatal error (see `Error_Log_Pump_Tsueri_*.log`). Red LED solid = either an in-progress SD firmware update (`firmware.bin` present) or a hang *before* the green-blink sequence — useful for distinguishing a clock/power-stage crash from an application-level error. Remaining sensor data in the queue is drained (written to file) before closing, preventing empty files on stop.

`app_filex.c` calls `fx_media_flush(&sdio_disk)` every 100 sensor samples (~1 s @ 100 Hz) inside the `COMMAND_SAVE_SENSORS` case. This keeps FAT directory entries (file sizes) up-to-date so an ungraceful power-off — the normal termination on this hardware, since the user button is physically disconnected — still leaves readable Sens/Gps/error-log files. Without the flush, FAT only updates file size on `fx_file_close`, so power-off mid-session shows 0-byte files even though data sectors were written. The WAV header is *not* updated in the same path (size is only written back on graceful stop), so the .wav header keeps pointing at the init dummy size (60 s) after ungraceful stop — audio data is on the card, but players may truncate or overread.

`UpdateFileXClock()` stamps FileX's system date/time from a wall-clock *base* plus `(tx_time_get() - ClockBaseTick)/100` seconds. The base starts as `__DATE__` + `__TIME__` (compile date) at boot, but `gps_thread` overwrites it via `SetClockBaseFromGPS(...)` as soon as the first `$GNRMC` arrives with a valid date — so files created before GPS lock land with the stale compile date, and everything after gets today's real UTC. The error log filename is still constructed from `__DATE__` once at boot (before any GPS fix) so it keeps the compile-date format; only the FAT directory entries migrate to GPS time. Month/year rollover past the base date is not handled.

START_LOG / gps_thread writes a `clock: seeded from GPS YYYY-MM-DD HH:MM:SS UTC` line to the error log at the moment of seeding so you can see exactly when the FAT timestamps switched over.

START_LOG writes progress markers to the error log between each init step (`sens header written`, `gps header written`, `gas gauge init ok` / `init FAIL`, `battery START mV START%`, `mic init begin`, `mic init ok` / `mic init FAIL`, `mic running`). A silent hang leaves the last successful marker on SD, so you can tell exactly where the firmware got stuck. The `mic init begin` marker without a following ok/FAIL is the signature of a hardware-modified-board MIC hang — the fix is to set `STBOX1_LOG_AUDIO 0`. Same pattern applies for the gauge: `gas gauge init FAIL` means the STC3115 isn't responding on I²C4; the rest of the logger still runs (battery CSV is skipped for that boot).

STOP_LOG closes all files and the SD media regardless of `AudioFileOpen`. The earlier version nested `fx_media_close` inside the audio block, which meant that when MIC init failed or was disabled, the SD media stayed open and the card was left in an inconsistent state on graceful stop. Also the GPS file is created and flushed *before* the MIC init block so a MIC hang cannot prevent GPS data from landing on the card.

Clock config uses **HSI** (internal 16 MHz) as the PLL source, not HSE. The 3.3V board mod (for the GPS module) made the external crystal unreliable on battery-power boot. PLL / PLL2 / PLL3 all source from HSI — same 160 MHz sysclk.

The user-button pin (PC13, EXTI13 at NVIC priority 0) is configured in the BSP with `GPIO_PULLDOWN` — not the stock `GPIO_NOPULL`. Boards with the user button physically disconnected otherwise float PC13, which triggers a continuous highest-priority EXTI storm and hangs the firmware.

SD card firmware update: On boot, the app checks for `firmware.bin` on the SD card. If found, it programs the inactive flash bank (dual-bank STM32U585), renames the file to `firmware.done`, swaps banks via option bytes, and resets. No BLE/JTAG/ST-Link needed. Max firmware size ~1016 KB. Implementation in `CheckAndApplyFirmwareUpdate()` in `app_filex.c`.

An error log file `Error_Log_Pump_Tsueri_dd.mm.yyyy.log` (compile date) is created on the SD card alongside the sensor data. It logs boot markers and fatal errors with timestamps. The `Error_Handler` writes the error location to this file before halting.

### GPS module (u-blox MAX-M10S) — wiring

Wiring (SparkFun MAX-M10S breakout → SensorTile.box PRO Rev_C):
- GPS TX → PA1 (UART4 RX)
- GPS RX → PA0 (UART4 TX — required for auto-config)
- GPS 3V3 → 3V3
- GPS GND → GND

Because UART4 now drives the GPS link, `STBOX1_ENABLE_PRINTF` is disabled by default — there is no debug UART while the GPS is connected (error log on SD card still works).

**No manual u-center setup needed.** On every boot, `GPS_Init()` auto-configures the module with full UBX-ACK verification (ACK-ACK / ACK-NAK parsing via `ubx_wait_ack()` + up to 3 retries per command in `ubx_send_retry()`, 50–100 ms spacing between commands):

1. UBX-CFG-PRT at 9600 baud (u-blox factory default) → switch UART1 to `GPS_UART_BAUDRATE` (38400). Best-effort, **no ACK wait** — the baudrate is switching mid-command, so any reply would come back on the new rate and we can't read it at 9600.
2. Switch UART4 to 38400 baud.
3. UBX-CFG-RATE sent **first** (most important command) → set measurement rate from `GPS_MEAS_PERIOD_MS` (derived from the `GPS_RATE_HZ` macro, default 10 Hz → 100 ms; override at build time via `make GPS_RATE_HZ=<n>`). nav solution every cycle, GPS time reference. Earlier order put CFG-MSG first, but a dropped RATE packet then left the module on its persisted 1 Hz config with no signal in the logs.
4. UBX-CFG-MSG × 4 → disable the NMEA sentences we don't parse (GLL, GSA, GSV, VTG). Only GGA + RMC remain enabled.
5. UBX-CFG-CFG → save all sections (BBR + Flash + EEPROM) so the config survives power cycles.

The ACK status of each command is accumulated into a static buffer and flushed to the SD error log on the first `START_LOG` as `gps: rate=OK(10Hz) msg=OK save=OK`. Exposed via `GPS_GetInitLog()` in `Core/Inc/gps_nmea.h`, consumed in `app_filex.c` COMMAND_START_LOG alongside the other boot markers. When the RATE command times out or is NAK'd (and retries all fail), the marker reads `rate=TO(10Hz)` or `rate=NAK(10Hz)` and the on-card `GpsNNN.csv` will show 100-tick row spacing (= 1 s = persisted 1 Hz config) — the exact symptom that motivated adding the ACK path.

Implementation in `Core/Src/gps_nmea.c`. With only two sentences enabled, 10 Hz fits in ~1.5 kB/s on the 38400-baud UART (ceiling ~3.8 kB/s). Raising the fix rate further (25 Hz max for single-GNSS) would require either disabling even more output or bumping `GPS_UART_BAUDRATE`, but bumping the baud breaks the auto-config flow on already-persisted boards — the factory-default 9600 fallback only works once.

The firmware parses only `$GNRMC` and `$GNGGA` sentences. All other NMEA sentences from the module are silently ignored.

Data collected via the ST BLE Sensor app uses a slightly different format (date/time columns instead of raw ms timestamp).

## Visualization

All visualisation is done by the `stbox-viz` Rust crate at `Utilities/rust/stbox-viz/`. Single binary, no Python runtime. Four subcommands:

| Subcommand | Output | Input |
|---|---|---|
| `combined` | interactive Plotly HTML (map + nose angle + baro height + speed) | SensNNN.csv + auto-detected GpsNNN.csv |
| `sensors` | 5-panel sensor summary PNG + quaternion/Euler PNG | SensNNN.csv |
| `pumpfoil` | pump-cadence spectrogram PNG + movement-phase PNG | SensNNN.csv |
| `animate` | animated GIF of board orientation per session, optional combined MOV | SensNNN.csv + optional camera video |

Build: `cd Utilities/rust/stbox-viz && cargo build --release`. Binary at `target/release/stbox-viz`. Crate source layout: `io.rs`, `fusion.rs` (Madgwick 6DOF), `euler.rs`, `session.rs` (pitch-oscillation detection), `gps.rs` (haversine + ride detection), `baro.rs` (TC + GPS-anchored water reference), `butter.rs` (4th-order Butterworth + filtfilt), `spectrogram.rs` (scipy-equivalent STFT via `rustfft`), `html.rs` (Plotly JSON emission), `plot_common.rs` (shared `plotters` helpers), and one `*_cmd.rs` per subcommand.

### Combined HTML (`stbox-viz combined`)

Single interactive HTML per recording (saved to `html/`) with:
- **Plotly Scattermap** on `carto-positron` tiles. **One trace per detected ride**, coloured by GPS speed (Viridis, 0–25 km/h). Only the selected ride's trace is drawn at a time — there is deliberately no "All rides" view, because the on-shore/between-ride GPS points drowned out the actual action and the full-session x-axis made the time-series panels extend past where any ride happened. Hover shows UTC + speed + nose angle + height-above-water at every point.
- **Board nose angle** time-series (drift-corrected via 1 s + 60 s rolling medians on the rotated sensor Y-axis elevation).
- **Board height above water** from the LPS22DF baro via `height_above_water_m()`. Two-stage correction: (1) temperature-compensate pressure (the LPS22DF sits in a semi-sealed SensorTile enclosure, so P couples to T via ideal gas — a 10 °C swing between indoor storage and cold seawater produces ~15 hPa of fake altitude, far larger than the 0.08 hPa signal from an 80 cm mast); (2) use GPS speed < 3 km/h as ground truth that the board is in the water (knee-start, between rides, pre/post-session), linear-interpolate the TC'd pressure at those anchors across flying segments, and compute height from the local hypsometric approximation (8434 × (1 − P/P_ref)). Earlier versions used a 10 s rolling-min of altitude which collapsed to ~0 m during sustained flight because every sample in the window was "up". Y-axis is auto-scaled (not pinned 0–80 cm) because residual thermal drift on this hardware is ~0.5–3 m over a 5-min ride even with GPS anchoring — enough to tell "flying" from "in water", but not sub-meter mast height. GPS altitude is deliberately not used — the MAX-M10S's ~5–10 m vertical error can't resolve a pump stroke. Sub-meter precision would require a mast-mounted ultrasonic height sensor.
- **Speed**, **position-derived** (haversine on 1 Hz fixes) because the module's Doppler Speed column is unreliable on this board (observed median 0.12 km/h while position deltas showed sustained 10–30 km/h). Clamped at 60 km/h (multipath glitches → linear interp), 5 s rolling median, y-axis pinned to 0–30 km/h.

**Panel captions**: drawn as horizontal annotations centred above each panel (dedicated title strip between panels). Earlier versions used rotated y-axis titles, but the ~180 px panel height couldn't fit any caption longer than ~25 chars without bleeding into the neighbouring panel. Y-axes carry just the unit now (°, m, km/h).

**Ride buttons**: one per detected ride (`Ride 1` / `Ride 2` / …), rendered as a button bar above the chart. Each click uses Plotly's `update` method to do three things atomically:
1. `restyle visible` to show only that ride's map trace
2. `relayout xaxis.range` to zoom the shared time-series x-axis to the ride's window
3. `relayout map.center + map.zoom` to recentre the map on the ride's extent

Page loads with ride 1 selected by default (button `active: 0`).

**Ride detection**: sustained GPS movement above 3 km/h for ≥10 s, merging gaps <30 s, padded by 3 s. The pitch-oscillation detector used by `animate` misses smooth flying (board barely pitches); GPS-based detection catches every real ride and skips on-shore activity.

**Display rate reduction**: sensor series at 100 Hz are binned to 100 ms buckets (10 Hz display rate) before emission so the HTML stays ~1.2 MB. `bin_to_resolution` uses a `BTreeMap` keyed by bucket index — not a "if bucket != last_bucket" running-index loop — so any non-monotonic or duplicate input time can't emit the same bucket twice and make Plotly draw ghost zig-zags past the data end. Plotly.js is CDN-linked (`cdn.plot.ly/plotly-2.35.2.min.js`) — matches the Python version's `include_plotlyjs='cdn'`.

Missing vs the last Python version: URL anchors (`#s1`, `#s2`, …), rider classification via `--rider-split-utc`, per-session-per-HTML output mode. Add back as needed; the per-ride trace structure already makes each of them small JS-only additions.

### Sensor PNGs (`stbox-viz sensors`)

5-panel summary: accel/gyro/mag XYZ + temperature + pressure, min:sek x-axis. Then a quaternion/Euler plot with gimbal-lock shading (red vspans where |pitch| > 85°, representational artefacts, not real motion). Runs sub-second for a 23-min session at 100 Hz.

SD-format only — the BLE (ST BLE Sensor app) format loader from the Python era has not been ported. Live firmware writes SD format.

### Pump Cadence (`stbox-viz pumpfoil`)

Two PNGs:
- `plot_pump_cadence.png` — dynamic acceleration + envelope, 4 s / 90 % STFT spectrogram (inferno colour-map, 0.3–5 Hz band), dominant-frequency scatter coloured by coolwarm. Energy threshold at the 40th percentile gates the scatter to active pumping only. Median cadence is drawn as a horizontal reference line.
- `plot_pump_phases.png` — gyro-RMS activity coloured by rest/active/crash phase (60th / 95th percentile thresholds), per-axis gyro breakdown, |acc|, pressure (inverted y-axis).

### Board Animation (`stbox-viz animate`)

Per-session GIFs showing board orientation in real time, 3 panels (board side view + pump detail ±5° + full-range nose angle). Nose angle is 4th-order Butterworth 2 Hz low-pass with `filtfilt` zero-phase + 10 s rolling median baseline. Drop-in flash for 2 s on the steepest negative angle inside the first 10 s of the session. History lines build progressively; x-axis grows with the cursor.

With `--video FILE`, shells out to `ffmpeg` + `ffprobe` for the same `hstack=inputs=2` side-by-side MOV + optional 2 s title card concat that the old Python pipeline produced. `ffmpeg` must be in PATH.

Session detection is **pitch-oscillation based** (≥ 0.3 Hz over ≥ 30 s, merging < 60 s gaps). Smooth-flight pumpfoil data without clear pitch oscillation won't register — use `combined` (GPS-based) instead for those.

### CSV header convention

`Time [10ms]` (current firmware) or `Time [mS]` (legacy, misnamed — always ticks, never milliseconds). Both accepted by the loader via the column-lookup in `io.rs`.

## Magnetometer Firmware Improvements

The LIS2MDL driver (`Drivers/BSP/Components/lis2mdl/lis2mdl.c`) Init function has three added settings to reduce drift:
- Offset cancellation every ODR cycle (`LIS2MDL_SENS_OFF_CANC_EVERY_ODR`) — continuous offset correction
- Temperature compensation (`lis2mdl_offset_temp_comp_set`) — compensates for thermal drift
- Low-pass filter at ODR/4 (`LIS2MDL_ODR_DIV_4`) — 25 Hz bandwidth at 100 Hz ODR

### Command-Line Build

Makefiles are provided for building firmware without STM32CubeIDE:
- `Projects/STEVAL-MKBOXPRO/Applications/Rev_C/BLESensorsPnPL/STM32CubeIDE/Makefile` → `build/BLESensorsPnPL.bin`
- `Projects/STEVAL-MKBOXPRO/Applications/Rev_C/SDDataLogFileX/STM32CubeIDE/Makefile` → `build/SDDataLogFileX.bin`

The toolchain path is auto-detected by platform in `config.mk` at the repository root (using `uname -s`):
- **macOS (Darwin)**: `$(HOME)/.software/arm-gnu-toolchain/bin`
- **Linux**: `/usr/bin`

Both Makefiles include `config.mk` via `-include $(ROOT)/config.mk` with a fallback default. The path can also be overridden per invocation: `make TOOLCHAIN=/other/path`.

SDDataLogFileX Makefile also exposes `GPS_RATE_HZ` (default 10) that gets passed into `stbox1_config.h` as `-DGPS_RATE_HZ=<n>` and drives `UBX-CFG-RATE` in `gps_nmea.c`. Override per invocation, e.g. `make GPS_RATE_HZ=5` for 5 Hz or `make GPS_RATE_HZ=25` for the max rate. The header has a `[1, 25]` `#error` guard (UART ceiling at 38400 baud with only GGA + RMC enabled). IDE builds that don't define the macro get the 10 Hz default.

## Known Limitations

- Some Android devices have issues with BLE secure PIN connections — disable `STBOX1_BLE_SECURE_CONNECTION` as workaround
- Some Android devices have issues with BLE force rescan — disable `BLE_FORCE_RESCAN` as workaround
- Clean device BLE cache when switching between applications
