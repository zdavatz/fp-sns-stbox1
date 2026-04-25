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

**I²C4 timing investigation for STC3115 (24.4.2026)**: the ST v1.6.0 → v2.0.0 update replaced the I²C4 timing `0xA040184A` (~145 kHz at PCLK1=160 MHz, explicitly chosen to stay under the STC3115's 400 kHz Fast-Mode ceiling) with `0x00F07BFF` (~421 kHz) across all four I²C instances. The 421 kHz value is above the STC3115 Fast-Mode spec, so we reverted `MX_I2C4_Init` in `Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_bus.c` only (I2C1/2/3 keep the newer timing since the MEMS sensors there are Fast-Mode Plus capable). **The revert alone did not fix the failure** — Peter's 24.4.2026 13:26 field test on the reverted build still logged `gas gauge init FAIL`, falsifying the timing hypothesis. The safer timing stays in place, but the real root cause is elsewhere. See the diagnostic probe below.

**STC3115 diagnostic probe (24.4.2026, build ≥ 16:59)**: `BSP_GG_Init` is opaque — it returns `COMPONENT_OK`/`COMPONENT_ERROR` without telling us which I²C step failed, so we kept guessing. `app_filex.c` now calls `BSP_I2C4_Init()` + `HAL_I2C_IsDeviceReady(&hi2c4, 0xE0, 3, 200)` right before the opaque driver call and writes `gauge: i2c4_init=<rc> ping_0xE0=<ACK|NAK> halerr=0x<hex>` to the error log once per boot (gated by `BatteryInitAttempted`). The three cases that decide next steps: `ping=ACK` → chip is on the bus, the failure is in the ST driver (dig into `STC3115_Status` / `STC3115_Startup`); `halerr=0x04` (`HAL_I2C_ERROR_AF`) → address NAK, chip not answering at 0xE0 — hardware, power rail, or wrong address; `halerr=0x20` (`HAL_I2C_ERROR_TIMEOUT`) → bus stuck low — pull-up / wiring. Worst-case added boot latency is 600 ms (3 × 200 ms ping timeouts), one-shot per boot, negligible against the rest of START_LOG.

**I²C4 pull-up fix (24.4.2026, build ≥ 18:48)**: Peter's 16:59 test returned `gauge: i2c4_init=0 ping_0xE0=NAK halerr=0x00000020` — `HAL_I2C_ERROR_TIMEOUT`, which means SCL/SDA never released to high between bits. Classic missing-pull-up signature. `I2C4_MspInit` in `SensorTileBoxPro_bus.c` had both PD12 (SCL) and PD13 (SDA) configured as `GPIO_NOPULL`. I²C1/2/3 (MEMS sensors) work on this board so external pull-ups are populated there; I²C4 (STC3115) apparently either lacks them on this Rev_C revision or they're too weak. Changed both pins to `GPIO_PULLUP` to enable the STM32U5's internal ~40 kΩ pull-ups. Marginal for 400 kHz Fast-Mode but adequate at the 145 kHz we run I²C4 at (`0xA040184A` timing) with the short onboard trace. Expected outcome on next field test: `ping_0xE0=ACK` → `BatNNN.csv` starts being written. If the symptom persists, external 4.7 kΩ pull-ups need to be soldered onto PD12 → 3V3 and PD13 → 3V3. Change is `bus.c:2234-2246` only, no application code change.

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

**LED signals during SD update (24.4.2026)**: two distinct patterns make the flash process visible without serial access — motivated by Peter's "kein direktes Feedback ob es funktioniert hat" after the field tests.
- **"Firmware detected"**: 10× rapid green+red alternation (~2 s total) the moment `firmware.bin` is opened. Very distinct from the 1/2/3-green `BootStageBlink` pattern so you can tell immediately whether an SD-update is starting vs a plain boot into logging.
- **During programming**: red LED toggles every ~512 B read from SD (existing behavior).
- **"Flash successful"**: 3× slow green blinks (~1.8 s) after the programming loop completes, right before the bank-swap/reset. Clear "we succeeded" feedback — distinct from both the rapid detected signal above and the single solid-green "logging active" post-boot state.

**Makefile builds `firmware.bin` directly (24.4.2026)**: the `all` target now emits `build/firmware.bin` alongside `build/SDDataLogFileX.bin` via a simple `cp`. The SD-update path looks for exactly that filename, so the binary can be dropped onto the SD card as-is — no renaming step, which Peter had been hitting on every flash (and missing at least once, leaving him on the old firmware for a full test).

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

**gps_thread polls at the module rate**, not at a fixed 1 Hz. `gps_poll_ticks = GPS_MEAS_PERIOD_MS / 10` (since 1 tick = 10 ms), so for `GPS_RATE_HZ = 10` the thread sleeps 10 ticks (100 ms) between reads — matching the module's fix rate. Before this change the thread was hard-coded to `tx_thread_sleep(100)` = 1 second, which silently dropped 9 of every 10 fixes when the module was pushed to 10 Hz (observed 22.4.2026: `rate=OK(10Hz)` in the error log but GpsNNN.csv still showed 100-tick row deltas). `save=NAK` from UBX-CFG-CFG has been observed but is non-blocking — the RAM layer of the config still takes effect immediately, so 10 Hz output works even without successful flash persistence; flash persistence just means the config survives power cycles.

**Firmware fingerprint in the error log**: the boot marker now includes a second line with build date/time, GPS rate, feature flags, and total flash footprint, e.g. `fw: build Apr 23 2026 10:15:02 | GPS 10Hz | AUDIO=0 BATTERY=1 | flash ~116KB`. Flash size is computed at runtime from the `_sidata`, `_sdata`, `_edata` linker symbols (end of `.data` load-addr in flash) minus the 0x08000000 flash base. Post-session we can tell immediately which binary actually produced the data — previously the compile-date `--- Boot ---` marker looked identical across builds on the same day.

**Reset-reason in the error log (24.4.2026)**: boot marker line 3 decodes `RCC->CSR` so field-test reboots can be told apart from user-initiated power-ups. `main()` snapshots `RCC->CSR` into the global `BootResetCsr` right after `HAL_Init()` — before anything else touches the register — then calls `__HAL_RCC_CLEAR_RESET_FLAGS()` so the *next* reset's flags are captured cleanly. `ErrorLog_Open()` in `app_filex.c` decodes the snapshot into one-or-more of `POR` (clean cold-boot, both `BORRSTF+PINRSTF` set), `BOR` (brown-out — supply dip from weak battery under SD-write load), `PIN` (external NRST), `SOFTWARE` (`NVIC_SystemReset()` or HardFault→default handler path), `IWDG`/`WWDG` (watchdog — both disabled in this build, should never appear), `LPWR`, `OBL`. Raw `CSR=0x...` is appended for ambiguous combinations.

Motivation: Ayano's 23.4.2026 Zürich session aborted at tick 5337 (~53 s) with no Error_Handler trace and a second `--- Boot ---` marker appearing in the same log 42 ms later. Sensor + GPS both clean up to the cut, then instant silence. Without reset-reason decoding we couldn't distinguish "user powered off/on" from "brown-out killed us mid-session" from "firmware crashed". The `reset:` line in the second boot marker of the next such incident immediately pins the cause — and the STC3115 gauge init FAIL on the same session (I²C4 unresponsive, possibly due to marginal supply) supports the weak-battery hypothesis that can now be confirmed or ruled out from the log alone. Cost: ~40 bytes RAM, 448 bytes flash.

Implementation in `Core/Src/gps_nmea.c`. With only two sentences enabled, 10 Hz fits in ~1.5 kB/s on the 38400-baud UART (ceiling ~3.8 kB/s). Raising the fix rate further (25 Hz max for single-GNSS) would require either disabling even more output or bumping `GPS_UART_BAUDRATE`, but bumping the baud breaks the auto-config flow on already-persisted boards — the factory-default 9600 fallback only works once.

The firmware parses only `$GNRMC` and `$GNGGA` sentences. All other NMEA sentences from the module are silently ignored.

**UART4 IRQ → GPS-thread split (24.4.2026)**: NMEA line assembly + sentence parsing (`nmea_checksum_ok`, `nmea_split`, `parse_rmc`/`parse_gga` with `atof`/`strtod`) used to run directly inside `HAL_UART_RxCpltCallback` at NVIC priority 6. SDMMC1 sits at priority 14, so every UART4 byte IRQ preempted in-progress SD transfers. At 1 Hz GPS (~400 IRQ/s) this was tolerated. At 10 Hz GPS (~4000 IRQ/s with variable-length parse spikes per completed sentence) the SDMMC timing budget blew out after ~90 s: the FileX writer backed up, `MessageQueue` filled to its 100-slot cap, the sensor thread blocked on `tx_queue_send(TX_WAIT_FOREVER)`, and sampled rate collapsed from 100 Hz to ~7.7 Hz. Signature in the CSV is a sustained run of Δtick = 13 after ~9 400 rows (Ayano 23.4.2026 s2: 9 371 rows Δ1 then 54 473 rows Δ13). The IRQ now only pushes incoming bytes into a 1 KB ring buffer (`RxRing[]` in `gps_nmea.c`) — no parsing in IRQ context. `gps_thread_entry` calls `GPS_Process()` once per poll cycle (after the `tx_thread_sleep(gps_poll_ticks)`) to drain the ring, assemble lines, and call the sentence parsers at thread priority 11. UART IRQ now completes in < 10 µs per byte regardless of GPS rate.

Validated on Peter's 23.4.2026 15:11 UTC test with `build Apr 23 2026 17:04:49`: 82 701 sensor rows over 826 s, of which 82 320 at Δtick = 1 (100 Hz) + 368 at Δtick = 0 (same-tick burst-writes) — no Δtick = 13 runs anywhere in the file. GPS held 10 Hz (7 543 fixes at Δtick = 10, 336 at Δtick = 20 from occasional dropped fixes, 1 × 1.3 s gap). Sustained 100 Hz sensor + 10 Hz GPS across 13.8 minutes — the starvation is gone.

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

**Time-axis flags**: by default the x-axis runs in UTC anchored to the GPS clock. Pass `--tz-offset-h <h>` to shift to local time — `3` for Greek summer (EEST, used at Ermioni), `2` for Swiss summer (CEST), `1` for Swiss winter (CET). The axis title, ride-list times, and hover tooltips all reflect the chosen offset (`Local time (UTC+3)` etc.). The recording date defaults to the sensor file's mtime; override with `--date YYYY-MM-DD` if you've moved files around (`cp` resets mtime to today). Date only affects the rendered date string — time-of-day reads off the GPS clock regardless of date.

Single interactive HTML per recording (saved to `html/`) with:
- **Plotly Scattermap** on `carto-positron` tiles. **One trace per detected ride**, coloured by GPS speed (Viridis, 0–25 km/h). Only the selected ride's trace is drawn at a time — there is deliberately no "All rides" view, because the on-shore/between-ride GPS points drowned out the actual action and the full-session x-axis made the time-series panels extend past where any ride happened. Hover shows UTC + speed + nose angle + height-above-water at every point.
- **Board nose angle** time-series (drift-corrected via 1 s + 60 s rolling medians on the rotated sensor Y-axis elevation).
- **Board height above water** from the LPS22DF baro via `height_above_water_m()`, overlaid with a GPS altitude trace (orange dots) zeroed at the median GPS altitude over stationary samples. Two-stage baro correction: (1) temperature-compensate pressure (the LPS22DF sits in a semi-sealed SensorTile enclosure, so P couples to T via ideal gas — a 10 °C swing between indoor storage and cold seawater produces ~15 hPa of fake altitude, far larger than the 0.08 hPa signal from an 80 cm mast); (2) use GPS speed < 3 km/h as ground truth that the board is in the water (knee-start, between rides, pre/post-session), linear-interpolate the TC'd pressure at those anchors across flying segments, and compute height from the local hypsometric approximation (8434 × (1 − P/P_ref)). Earlier versions used a 10 s rolling-min of altitude which collapsed to ~0 m during sustained flight because every sample in the window was "up". Display pipeline: 250 ms rolling-mean pre-smoothing to kill 10 Hz sensor noise (preserves 1 Hz pump oscillation at ≤4 % attenuation), then bin to 100 ms display buckets. Y-axis clamped to `[-0.1, 0.9] m` to match the physical mast length (80 cm) + small margins, with values outside `[-0.15, 0.95] m` NaN'd so thermal-drift excursions show as **gaps rather than lines saturated at the axis edge** — an honest "here the baro is trustworthy, here it isn't" view. Ticks every 10 cm (`dtick: 0.1`, `tickformat: .1f`). The orange GPS-altitude overlay won't resolve pump strokes (MAX-M10S vertical scatter ~3–10 m) but makes GPS dropouts visible when the board lies flat on water and the antenna loses fix quality. Sub-meter precision would require a mast-mounted ultrasonic height sensor, or a redesigned SensorTile enclosure that keeps the baro out of the water (Peter Schmidlin's idea: integrate antenna + sensors into one smaller housing that sits higher on the board).
- **Speed**, **position-derived** (haversine on GPS fixes) because the module's Doppler Speed column is unreliable on this board (observed median 0.12 km/h while position deltas showed sustained 10–30 km/h). Two-stage filter: (1) `reject_acc_outliers` flags samples where |Δspeed|/Δt > 15 km/h/s (≈4 m/s²) and v > 15 km/h — SUPfoil paddle-strokes produce 1–3 m/s², anything above ~4 m/s² implies a position jump of ≥ 4 m in 1 s, inside the module's 5–10 m horizontal-error envelope. (2) Linear-interp rejected samples, 5 s rolling median, y-axis pinned to 0–30 km/h. Peter's 22.4.2026 Ermioni data: raw peak 83.7 km/h → after gate 29.5 km/h → after smoothing 17.6 km/h — consistent with a paddle-start accelerating gradually to ~15 km/h then gliding. Without the accel gate the 5 s median alone couldn't suppress 30 km/h multipath spikes because any 3-of-5 consecutive bad samples would win.

**Panel captions**: drawn as horizontal annotations centred above each panel (dedicated title strip between panels). Earlier versions used rotated y-axis titles, but the ~180 px panel height couldn't fit any caption longer than ~25 chars without bleeding into the neighbouring panel. Y-axes carry just the unit now (°, m, km/h).

**Ride buttons**: one per detected ride (`Ride 1` / `Ride 2` / …), rendered as a button bar above the chart. Each click uses Plotly's `update` method to do three things atomically:
1. `restyle visible` to show only that ride's map trace
2. `relayout xaxis.range` to zoom the shared time-series x-axis to the ride's window
3. `relayout map.center + map.zoom` to recentre the map on the ride's extent

Page loads with ride 1 selected by default (button `active: 0`).

**Cross-panel hover + click**: `hovermode: "x unified"` with `xaxis.showspikes` draws a vertical grey line through every time-series panel at the cursor x-coordinate, with a single tooltip listing nose / baro / GPS-alt / speed values at that x. Clicking a time-series point pins a dashed red numbered line (1, 2, 3, …) across all panels at that x, via a `plotly_click` JS handler that appends `shapes` and `annotations` to the current layout. A **"Clear click-marks"** button above the chart resets the marker list to the base-layout annotations (panel captions). Map clicks are ignored because scattermap uses lat/lon, not the time axis.

**Map colorbar alignment**: positioned with `y = 1 - map_frac/2`, `yanchor: "middle"`, `len = map_frac × 0.9` so its top + bottom line up with the map's top + bottom in paper coordinates — prevents it from spilling into the time-series area below.

**Ride detection**: sustained GPS movement above 3 km/h for ≥10 s, merging gaps <30 s, padded by 3 s. The pitch-oscillation detector used by `animate` misses smooth flying (board barely pitches); GPS-based detection catches every real ride and skips on-shore activity.

**GPS loader does not decimate** (fixed 23.4.2026). An earlier `dedupe_by_second()` call kept only the first GPS row per second, which silently threw away 9 of every 10 fixes once the firmware moved to 10 Hz — visible as `9 / 76 ≈ 55 / 543 ≈ 10 %` fix retention in the loader's "{n} GPS fixes" log line. The haversine-based speed calculation then looked at 1-Hz-spaced positions and reported near-zero km/h even for actual rides. Now all fix rows from the CSV are kept, and position-derived speed is computed over the native sample interval.

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

Per-session GIFs showing board orientation in real time. **Five panels when GPS is available** (board side view + pump detail + height-over-water + speed + nasenwinkel), three panels otherwise. Nose angle is 4th-order Butterworth **0.7 Hz** low-pass with `filtfilt` zero-phase + 10 s rolling median baseline (cutoff lowered from 2 Hz to 0.7 Hz so 1–2 Hz sensor / fusion noise doesn't make the trace look "shaky"; pump fundamental at ~0.5 Hz still sits in the passband). Drop-in flash for 2 s on the steepest negative angle inside the first 10 s of the session. History lines build progressively; x-axis grows with the cursor.

**Wall-clock window mode (`--at HH:MM[:SS]`)** bypasses pitch-oscillation session detection and renders one GIF for an exact wall-clock time slice — needed when the rider is on smooth flight (no pitch oscillation triggers) or when you want to align with external footage. Pair with `--tz-offset-h` (3 for Greek summer / EEST, 2 for CEST, etc.) and `--date YYYY-MM-DD` (defaults to sensor file mtime). Window length is `--duration <s>`; if `--video` is given the video's full length is the default. The GIF is anchored to the GPS clock so it lines up with the same wall-clock axis as `combined`. The video's `creation_time` metadata is also probed and printed for diagnostics — if it disagrees with `--at`, use the precise value to align video and animation to the millisecond. Optional `--auto-skip` advances both video and GIF past the carry/transition seconds (irregular pitch before pumping starts) so the side-by-side opens directly on the foiling action.

**Side-by-side MOV (`--video FILE`)** shells out to `ffmpeg` + `ffprobe` for an `hstack=inputs=2` combined MOV with optional 2 s title-card concat (`--title`, `--subtitle`). `ffmpeg` must be in PATH.

**Per-frame dynamic scales**: every panel's y-axis grows with the running max of the data shown so far — no headroom above the current max. The Nasenwinkel (detrended) panel uses a **95th-percentile-based** y-limit so a single outlier pump doesn't stretch the axis and leave the typical pump activity in the bottom 30 % of the panel; outliers above p95 are visually clipped at the edge. Speed and height panels use plain max. Earlier frames have tight axes that highlight small early movements; later frames expand as peaks come in.

**Scrolling water surface**: the board side view shows a sinusoidal waterline (amplitude 2 cm, wavelength 0.8 m) that scrolls backward at 0.6 m/s, giving the visual impression that the board is moving forward over the water while it pumps. The board's lift uses the GPS-anchored baro height, smoothed with a 0.5-sec rolling mean to suppress pressure-noise spikes that would otherwise make the board jitter visibly up/down between actual pump cycles.

**Panel labels are horizontal** (drawn in their own 40-px title strip above each chart), not the rotated `y_desc` plotters defaults. Title strips also keep the labels from being overdrawn by chart grid pixels.

Without `--at`, session detection is **pitch-oscillation based** (≥ 0.3 Hz over ≥ 30 s, merging < 60 s gaps). Smooth-flight pumpfoil data without clear pitch oscillation won't register — use `combined` (GPS-based) instead for those.

### Compass Validity (`stbox-viz compass`)

Answers "is the LIS2MDL magnetometer usable as a heading reference on this board?" Takes a sensor CSV + its companion GPS CSV, runs Madgwick 6DOF for orientation, extracts roll + pitch (ignores the yaw from 6DOF since it has no absolute reference), tilt-compensates the body-frame mag vector via the Honeywell AN203 identities, and computes heading = `atan2(-x_h, y_h)` CW from magnetic north (using body-Y as the board's nose direction, matching `fusion.rs::nose_angle_series_deg`). Compares against the u-blox GPS course over ground at samples where ground speed > 2 km/h.

Output: `png/plot_compass_<stem>.png` with two stacked panels — mag heading and GPS course overlaid on top, residual (mag − GPS) below. Console prints median residual (should match the local magnetic declination: ~+4° E in Greece, ~+2.5° E in Zürich) and p5–p95 span (> ~30° suggests iron distortion dominates over real heading changes). First run on the 22.4.2026 Ermioni data: median −14°, p5–p95 span 307° — essentially random, consistent with the metal-screw hard/soft-iron distortion on the current housing. The fix is hardware: plastic screws or an all-plastic SensorTile enclosure with the sensors away from ferromagnetic mounting hardware.

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

## WhatsApp CLI

`whatsapp/` contains a small Baileys-based Node.js CLI for sending firmware binaries (and plots) to contacts or groups without opening the WhatsApp app. Four scripts, identical session store (`whatsapp/auth/`):

| Script | Purpose |
|---|---|
| `login.mjs` | Pair the CLI with your phone (QR scan, once per auth reset). `--force` wipes the existing session. |
| `list-groups.mjs` | Dump JIDs of every group the paired account is in — needed to find the target JID before the first `send.mjs` call. |
| `send.mjs <jid-or-phone> <file> [caption]` | Auto-detects by extension: `.png/.jpg/.jpeg` → image (with caption), everything else → document. So the same tool sends `build/SDDataLogFileX.bin` to Peter and `png/plot_combined_*.png` to the pumpfoil group. Phone-number shorthand (`41791234567`) is expanded to `<num>@s.whatsapp.net`. |
| `leave-group.mjs <jid>[,…]` | Leave one or more groups (comma-separated). |

Setup: `cd whatsapp && npm install`, then `node login.mjs`. Session is persisted in `whatsapp/auth/` — git-ignored (together with `node_modules/` and the regenerable `package-lock.json`). Adapted from `~/software/pegelstand/whatsapp/`. Primary use case on this repo: ship a fresh firmware `.bin` to the field-tester (Peter) right after a build, without Airdrop/email round-trips.

## Known Limitations

- Some Android devices have issues with BLE secure PIN connections — disable `STBOX1_BLE_SECURE_CONNECTION` as workaround
- Some Android devices have issues with BLE force rescan — disable `BLE_FORCE_RESCAN` as workaround
- Clean device BLE cache when switching between applications
