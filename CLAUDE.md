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
- `STBOX1_ENABLE_BLE_SYNC` (SDDataLogFileX only) — gates the BLE FileSync stack (advertising + GATT + ThreadX BLE thread). Default `1`. Set to `0` to remove ~28 KB flash + 4.5 KB BSS; the rest of the logger is byte-identical and the `BleSync_ThreadX_Init()` call from `App_ThreadX_Init` becomes a no-op returning `TX_SUCCESS`.

**STC3115 fuel-gauge diagnostic probe**: `BSP_GG_Init` is opaque (returns only `COMPONENT_OK` / `COMPONENT_ERROR`), so `app_filex.c` calls `BSP_I2C4_Init()` + `HAL_I2C_IsDeviceReady(&hi2c4, 0xE0, 3, 200)` before it and writes `gauge: i2c4_init=<rc> ping_0xE0=<ACK|NAK> halerr=0x<hex>` to the error log once per boot. Decode: `ping=ACK` → chip is on the bus, dig into the ST driver (`STC3115_Status` / `STC3115_Startup`); `halerr=0x04` (`HAL_I2C_ERROR_AF`) → address NAK, hardware/power rail/wrong address; `halerr=0x20` (`HAL_I2C_ERROR_TIMEOUT`) → bus stuck low, pull-up/wiring. I²C4 timing stays at `0xA040184A` (~145 kHz, the pre-v2.0.0 ST value — v2.0.0's `0x00F07BFF` ~421 kHz overshoots the STC3115 Fast-Mode ceiling), and PD12/PD13 use `GPIO_PULLUP` for the STM32U5's internal ~40 kΩ pulls. If a board still logs `halerr=0x20` after that, external 4.7 kΩ pull-ups need to be soldered on PD12 → 3V3 and PD13 → 3V3 (internal pulls have been observed to be too weak in the field). Worst-case added boot latency is 600 ms, one-shot per boot.

**Per-sensor diagnostic blinks in InitMemsSensors()**: between `BootStageBlink(2)` and `BootStageBlink(3)`, `main.c` calls `DiagBlinkRed(n)` before each MEMS sensor init so a "boot hangs at 2 green" can be pinned to the specific I²C/SPI transaction that blocks. Steps: 1× = entered `InitMemsSensors`, 2× = LIS2MDL (I²C2), 3× = LSM6DSV16X (SPI), 4× = LPS22DF (I²C2), 5× = STTS22H (I²C2), 6× = function exit. Costs ~3 s of boot time but invaluable when serial debug is unavailable. Zero red blinks at all = `InitMemsSensors()` never called → suspect a stale-flash-bank problem rather than firmware code (see below).

**Stale-flash-bank recovery via mass-erase**: STM32U585 has dual-bank flash with a `SWAP_BANK` option byte. The SD-update path's bank-swap can leave `SWAP_BANK` inconsistent — DFU then writes new firmware to the inactive bank while the chip continues booting from the active (stale) one. Symptom: identical hang behaviour across multiple completely different firmware builds. Recovery is a `dfu-util` mass-erase before the next flash:
```sh
dfu-util -d 0483:df11 -a 0 -s 0x08000000:mass-erase:force -D firmware.bin
```
The `:mass-erase:force` modifier wipes both banks before downloading; afterwards the chip has only one firmware copy and the bank-mapping question is irrelevant. Append `:leave` (`mass-erase:force:leave`) to make `dfu-util` exit DFU mode and reboot into the freshly-flashed image in one shot. STM32CubeProgrammer's "Full Chip Erase" via the GUI achieves the same.

**BLE EXTI11 IRQ-storm fix + ThreadX byte-pool sizing**: the BLE init path has two sharp edges that both manifest as "3 green boot blinks then dark, SD empty/truncated, no `STBoxSync` advertise". Both are now mitigated:

1. **Bounded ISR + low NVIC priority** — `Core/Src/hci_tl_interface.c::hci_tl_lowlevel_isr` caps its inner loop at 16 events per IRQ; even on a chattering BlueNRG-LP IRQ line the handler returns in bounded time and the next pending edge re-fires through the NVIC instead of spinning forever inside one entry. `init_ble_int_for_blue_nrglp` (in `Core/Src/ble_implementation.c`) sets `HCI_TL_SPI_EXTI_IRQ_N` priority to 14 (was 0). Same level as SDMMC1 (`BSP_SD_IT_PRIORITY = 14`) so an HCI event never preempts an in-flight SD write, and below UART4-GPS at 6 so the GPS line keeps assembling. Not lowered to 15 because that would let normal BLE traffic be preempted by a stuck SDMMC handler, risking deadlock against a BLE-clock-domain resource.
2. **Chip-presence gate** — `bluetooth_init()` returns the `init_ble_manager()` status (was `void`; signature `extern uint8_t bluetooth_init(void);` in `Core/inc/ble_implementation.h`). On non-zero rc the BLE thread writes `ble: init FAIL rc=N - thread bailing` to the error log and parks itself in a `tx_thread_sleep(1000)` loop *without* ever calling `init_ble_int_for_blue_nrglp`. fx_thread keeps logging untouched. On success: `ble: init ok - arming EXTI11 at NVIC prio 14` lands in the error log first, so post-session the log tells you whether BLE was up or skipped.
3. **ThreadX byte-pool size** — `TX_APP_MEM_POOL_SIZE` in `AZURE_RTOS/App/app_azure_rtos_config.h` is 8 KB (was 1 KB). `BleSync_ThreadX_Init` allocates 4 KB for the BLE thread stack from this pool — at 1 KB the allocation returned `TX_NO_MEMORY` and `tx_application_define`'s error path (bare `while(1){}`) ran *before* `tx_kernel_enter`, so no thread (including fx_thread) was ever created. The 3 green `BootStageBlink` calls in `main()` still complete because they happen prior to `MX_ThreadX_Init`. The header has a comment explaining the failure mode so it can't be re-introduced — same idiom as `FX_APP_MEM_POOL_SIZE` (14 KB after gps_thread was added).

Lesson: **after bumping any allocation, flash to a real board and confirm the chip reaches `tx_kernel_enter`** — visible signal: green LED goes solid on after the 3 boot blinks. The cheapest way to confirm a BLE-init regression vs. an existing-logger regression is to build with `STBOX1_ENABLE_BLE_SYNC 0` (drops ~28 KB flash + 4.5 KB BSS, byte-identical logger) and bisect.

**Two-stage BLE chip-alive probe**: a half-dead BlueNRG-LP can ACK SPI bytes but still hang the kernel inside `init_ble_manager`'s GATT/GAP setup, starving `fx_thread`. So a SPI-ACK-only probe isn't enough. Both stages must pass: (1) `hci_tl_spi_send(HCI_Reset)` returns 0 (chip raised IRQ in <15 ms when we asked to send), AND (2) chip raises IRQ again within 500 ms with a Command Complete response. On either failure `bluetooth_init` is never called, the BLE thread parks, and the logger keeps writing. Implementation in `Core/Src/ble_sync.c::ble_chip_alive_probe()`. Bounded worst case: 5 ms RST low + 150 ms boot wait + 15 ms TX timeout + 500 ms RX timeout = ~670 ms. The probe outcome is recorded in a global `g_ble_probe_status` (0=dead, 1=alive+init OK, 2=alive but init failed, 0xFF=still pending) which `ErrorLog_Open()` snapshots into a single line on the SD so post-session you can tell from the log whether BLE was up or skipped.

The probe is **necessary but not sufficient on the 3.3V-modded box** — even on builds where BLE is fully disabled (`STBOX1_ENABLE_BLE_SYNC 0`), the same hardware shows non-deterministic SDMMC hangs at random `fx_file_*` operations within the first 50 ticks (~500 ms) of fx_thread (different boots stop at `fx_file_create`, `fx_file_open`, or `fx_file_seek`, with no pattern). This rules BLE out as the *sole* cause and points at SDMMC signal/power-integrity on the modded supply. Suspected root causes (still open): SDMMC kernel clock (currently `SDMMC_HSPEED_CLK_DIV` = 25 MHz at 100 MHz kernel) might be too aggressive at 3.3V; voltage-regulator ripple under SDMMC write current; SD card itself wedging under a borderline supply. Workaround for the field tester: `STBOX1_ENABLE_BLE_SYNC 0` build improves the ratio of successful boots (gets further on average than BLE-on) but doesn't make it deterministic. Future investigation: try `SDMMC_NSPEED_CLK_DIV` (12.5 MHz) for power-integrity-margin, or add an SDMMC-op retry/timeout wrapper in the FileX driver glue.

**Per-step ErrorLog markers in COMMAND_START_LOG**: `app_filex.c`'s `COMMAND_START_LOG` case writes `fx: before <op>` / `fx: <op> returned status=0xN` markers around every `fx_file_*` call so non-deterministic hangs can be pinned exactly — the last marker on disk pinpoints which call hung, and non-zero status codes get captured before `Error_Handler` closes the log. Grep `ErrorLog_Write` in that case for the current marker set. Cost: ~10 extra writes per session start, ~50 ms of added start-up latency.

**Build versioning — `FW_BUILD_NUM` counter**: each `make` invocation in the SDDataLogFileX `STM32CubeIDE/` directory bumps a counter in `.build_counter` (**committed** so build numbers are globally unique across the repo, not per-developer; means a normal `make` produces a working-tree change to that file that should be committed alongside any source changes that produced the binary you're shipping) and bakes it into the firmware as `-DFW_BUILD_NUM=N` via a `$(shell …)` expression. Three places use the value:

1. **Filename**: `make` emits `build/firmware_v<N>.bin` *alongside* the unversioned `build/firmware.bin` (the latter still required for the SD-update path that looks for an exact filename). Field testers can have multiple .bin files in their Downloads folder and tell them apart at a glance — when sending via WhatsApp use the versioned filename so the conversation log naturally documents which build is in flight.
2. **`FW_INFO.txt`** at the SD root: first line is `fw: v<N> build May  5 2026 08:25:34`. Field tester pops the SD into a Mac, opens the file, knows immediately which firmware is running. Especially important after an SD-update where the bank-swap can silently fail — if `FW_INFO.txt` shows the *old* `v<N-1>` after copying, the swap didn't take.
3. **Error log boot marker**: `--- Boot v<N> May  5 2026 08:25:34 ---` and `fw: v<N> build … flash ~150KB` lines.

`-DFW_BUILD_NUM=N` is a command-line define which `make` doesn't track as a dependency, so an unchanged `app_filex.c` would otherwise reuse a stale `.o` and v<N+1>.bin would report v<N> inside. The Makefile has a `$(BUILD_DIR)/app_filex.o: FORCE` rule to always rebuild that single .o (other .o files don't reference `FW_BUILD_NUM`); costs a few seconds per build, cheap insurance for honest version labels. Default fallback for IDE builds (STM32CubeIDE / IAR / Keil) that don't pass `-DFW_BUILD_NUM`: `stbox1_config.h` defines it as `0` so the field tester sees `v0` and knows it wasn't a CLI build.

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

**Firmware update — DFU is always the preferred method.** On the SensorTile.box PRO, BOOT0 is wired to the user button: hold the user button while plugging in USB-C and the STM32U585 enters its built-in USB-DFU bootloader. From there `STM32CubeProgrammer` (or `dfu-util`) writes the new firmware directly to `0x08000000`. This bypasses all user-code update logic and uses the chip's read-only ROM bootloader instead — three-step (USB receive → erase → program), can't be bricked, and Bank-Swap-Bug is irrelevant. Full step-by-step in `Documentation/Flash_Firmware_Mac.html`.

SD card firmware update (fallback): On boot, the app checks for `firmware.bin` on the SD card. If found, it programs the inactive flash bank (dual-bank STM32U585), renames the file to `firmware.done`, swaps banks via option bytes, and resets. Max firmware size ~1016 KB. Implementation in `CheckAndApplyFirmwareUpdate()` in `app_filex.c`. **Known bug:** the bank-swap step (`HAL_FLASHEx_OBProgram` + `HAL_FLASH_OB_Launch`) fails silently in certain conditions — `firmware.bin` gets read, written to the inactive bank, and renamed to `firmware.done` (so it looks like the update worked), but the chip continues booting from the old bank. If you need to update Peter's box and the SD-update doesn't take, use DFU instead. Fix is pending on a development build (re-unlock OB before launch + read-back verify + retry on failure).

**LED signals during SD update**: two distinct patterns make the flash process visible without serial access.
- **"Firmware detected"**: 10× rapid green+red alternation (~2 s total) the moment `firmware.bin` is opened. Very distinct from the 1/2/3-green `BootStageBlink` pattern so you can tell immediately whether an SD-update is starting vs a plain boot into logging.
- **During programming**: red LED toggles every ~512 B read from SD (existing behavior).
- **"Flash successful"**: 3× slow green blinks (~1.8 s) after the programming loop completes, right before the bank-swap/reset. Clear "we succeeded" feedback — distinct from both the rapid detected signal above and the single solid-green "logging active" post-boot state.

**Makefile builds `firmware.bin` directly**: the `all` target emits `build/firmware.bin` alongside `build/SDDataLogFileX.bin` via a simple `cp`. The SD-update path looks for exactly that filename, so the binary can be dropped onto the SD card as-is — no renaming step.

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

**gps_thread polls at the module rate**, not at a fixed 1 Hz. `gps_poll_ticks = GPS_MEAS_PERIOD_MS / 10` (since 1 tick = 10 ms), so for `GPS_RATE_HZ = 10` the thread sleeps 10 ticks (100 ms) between reads — matching the module's fix rate. Before this change the thread was hard-coded to `tx_thread_sleep(100)` = 1 second, which silently dropped 9 of every 10 fixes when the module was pushed to 10 Hz — `rate=OK(10Hz)` in the error log but GpsNNN.csv still showed 100-tick row deltas. `save=NAK` from UBX-CFG-CFG has been observed but is non-blocking — the RAM layer of the config still takes effect immediately, so 10 Hz output works even without successful flash persistence; flash persistence just means the config survives power cycles.

**Firmware fingerprint in the error log**: the boot marker now includes a second line with build date/time, GPS rate, feature flags, and total flash footprint, e.g. `fw: build Apr 23 2026 10:15:02 | GPS 10Hz | AUDIO=0 BATTERY=1 | flash ~116KB`. Flash size is computed at runtime from the `_sidata`, `_sdata`, `_edata` linker symbols (end of `.data` load-addr in flash) minus the 0x08000000 flash base. Post-session we can tell immediately which binary actually produced the data — previously the compile-date `--- Boot ---` marker looked identical across builds on the same day.

**FW_INFO.TXT at SD-card root**: same fingerprint content as the error-log boot marker, but written to a fixed file at the SD root (`FW_INFO.TXT`) on every boot — no need to grep through `Error_Log_Pump_Tsueri_*.log` to confirm which firmware is running. Field tester pops the SD card into a computer and the file's modification timestamp + first line tell them immediately which build is on the box. Especially important after an SD-update: if `FW_INFO.TXT` still shows the old build date after copying `firmware.bin`, the bank-swap silently failed (known bug — DFU is the workaround). Written from `WriteFwInfoFile()` in `app_filex.c`, called from `CheckAndApplyFirmwareUpdate()` right after `fx_media_open` succeeds, so it runs on every boot regardless of whether the user starts a logging session. The file is deleted+recreated each boot so a shorter new content cannot leave stale tail bytes from a previous build.

**Reset-reason in the error log**: boot marker line 3 decodes `RCC->CSR` so unexpected reboots can be told apart from user-initiated power-ups. `main()` snapshots `RCC->CSR` into the global `BootResetCsr` right after `HAL_Init()` — before anything else touches the register — then calls `__HAL_RCC_CLEAR_RESET_FLAGS()` so the *next* reset's flags are captured cleanly. `ErrorLog_Open()` in `app_filex.c` decodes the snapshot into one-or-more of `POR` (clean cold-boot, both `BORRSTF+PINRSTF` set), `BOR` (brown-out — supply dip from weak battery under SD-write load), `PIN` (external NRST), `SOFTWARE` (`NVIC_SystemReset()` or HardFault→default handler path), `IWDG`/`WWDG` (watchdog — both disabled in this build, should never appear), `LPWR`, `OBL`. Raw `CSR=0x...` is appended for ambiguous combinations. Lets you distinguish "user powered off/on" from "brown-out killed us mid-session" from "firmware crashed" without ambiguity. Cost: ~40 bytes RAM, 448 bytes flash.

Implementation in `Core/Src/gps_nmea.c`. With only two sentences enabled, 10 Hz fits in ~1.5 kB/s on the 38400-baud UART (ceiling ~3.8 kB/s). Raising the fix rate further (25 Hz max for single-GNSS) would require either disabling even more output or bumping `GPS_UART_BAUDRATE`, but bumping the baud breaks the auto-config flow on already-persisted boards — the factory-default 9600 fallback only works once.

The firmware parses only `$GNRMC` and `$GNGGA` sentences. All other NMEA sentences from the module are silently ignored.

**UART4 IRQ → GPS-thread split**: NMEA line assembly + sentence parsing (`nmea_checksum_ok`, `nmea_split`, `parse_rmc`/`parse_gga` with `atof`/`strtod`) runs in `gps_thread`, not in `HAL_UART_RxCpltCallback`. The IRQ only pushes incoming bytes into a 1 KB ring buffer (`RxRing[]` in `gps_nmea.c`); `gps_thread_entry` calls `GPS_Process()` once per poll cycle to drain the ring, assemble lines, and call the sentence parsers at thread priority 11. UART IRQ now completes in < 10 µs per byte regardless of GPS rate. Earlier the parsing ran in-IRQ at NVIC priority 6, which preempted SDMMC1 (priority 14): at 10 Hz GPS (~4000 IRQ/s with variable-length parse spikes) the SDMMC timing budget blew out after ~90 s, the FileX writer backed up, `MessageQueue` filled to its 100-slot cap, and sampled rate collapsed from 100 Hz to ~7.7 Hz. Signature in the CSV was a sustained run of Δtick = 13. Validated post-fix: sustained 100 Hz sensor + 10 Hz GPS across 13.8 minutes with no Δtick > 1 runs anywhere in the file.

Data collected via the ST BLE Sensor app uses a slightly different format (date/time columns instead of raw ms timestamp).

## BLE FileSync — download SD-card files over Bluetooth (SDDataLogFileX)

The SDDataLogFileX firmware advertises as `STBoxSync` with PIN-secure pairing (PIN `123456`) and exposes a tiny custom GATT service so the host can download recorded files without removing the SD card. Two characteristics live under the BlueST features service (`00000000-0001-11e1-9ab4-0002a5d5c51b`):

| Characteristic | UUID | Properties |
|---|---|---|
| FileCmd  | `00000080-0010-11e1-ac36-0002a5d5c51b` | write w/o response |
| FileData | `00000040-0010-11e1-ac36-0002a5d5c51b` | notify |

Opcodes (one byte + optional payload — payload is the filename without trailing NUL):

| Opcode | Meaning | FileData reply |
|---|---|---|
| `0x01` LIST | enumerate SD root | `name,size\n` rows + single `\n` terminator |
| `0x02` READ `<name>` | stream file body | raw bytes; total length matches the LIST size |
| `0x03` DELETE `<name>` | drop file | single status byte |
| `0x04` STOP_LOG | gracefully close active session | no FileData reply (host re-checks via LIST) |

Status bytes used by READ/DELETE replies: `0x00` OK, `0xB0` BUSY (logging in progress, send STOP_LOG first), `0xE1` NOT_FOUND, `0xE2` IO_ERROR, `0xE3` BAD_REQUEST. READ and DELETE are rejected with `BUSY` while a `Sens*.csv` or `Gps*.csv` is open for writing — host calls STOP_LOG first to flush the active session.

**Firmware-side architecture** (all under `Projects/STEVAL-MKBOXPRO/Applications/Rev_C/SDDataLogFileX/`):

- `Core/Src/ble_sync.c` — owns one ThreadX thread (priority 14, below the FileX writer at 12 and the GPS thread at 11 so SD bandwidth always wins). Calls `bluetooth_init()` once, then loops `if (hci_event) { hci_event=0; hci_user_evt_proc(); } BleFileSync_Tick(); tx_thread_sleep(1);`. The Tick is what advances the LIST/READ state machine without blocking the HCI event pump.
- `Core/Src/ble_filesync.c` — the FileCmd/FileData characteristics + the `CurrentOp` state machine. On notify congestion `aci_gatt_srv_notify` returns INSUFFICIENT_RESOURCES; we don't drop bytes — the LIST stays in `ST_LIST_EMIT` and the READ rewinds the file cursor with `fx_file_relative_seek(SEEK_BACK)`, both retry on next Tick.
- `Core/Src/ble_implementation.c`, `ble_function.c` — minimal X-CUBE-BLEMGR glue (mandatory globals + `set_board_name` + connection/pair callbacks + the ext-config callbacks for the flags we leave on in `ble_implementation.h`).
- `Core/Src/ble_spi.c`, `hci_tl_interface.c` — isolated SPI1 driver bypassing the shared `Drivers/BSP/SensorTileBoxPro/` BSP entirely (the BLEDualProgram BSP and the SDDataLogFileX BSP both export the same `BSP_*` symbols, so they can't coexist — bypassing the BSP for the BLE link sidesteps the collision and keeps the stack debuggable in isolation).
- `Core/inc/RTE_Components.h`, `ble_manager_conf.h`, `stm32wb07_06_conf.h`, `ble_list_utils.h` — config headers expected by the X-CUBE-BLEMGR + STM32WB07_06 middleware. Copied from BLEDualProgram and trimmed for our use.
- `FileX/App/app_filex.c` — adds two public hooks: `Ble_RequestStopLog()` posts `COMMAND_STOP_LOG` into `MessageQueue` via a dedicated static `MessageData_t` (separate from the read-thread ring buffer to avoid races); `Ble_IsLoggingActive()` returns whether `SensorsFileOpen || GpsFileOpen` so the BUSY guard can decide.

Build cost: text +28.7 KB / bss +4.5 KB vs the no-BLE build. Gated on `STBOX1_ENABLE_BLE_SYNC 1` in `Core/inc/stbox1_config.h` (default on); flip to `0` to remove the entire stack.

The most ergonomic way to use this is the MovementLogger GUI's BLE FileSync panel (see "MovementLogger GUI" below). For ad-hoc poking, any generic GATT client works — write `01` to FileCmd, watch FileData stream the listing.

**User-facing BLE FileSync explainer** at `Documentation/BLE_FileSync.{html,pdf}` (same render pattern as `Sensor_Fusion.{html,pdf}` — `"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless --disable-gpu --no-pdf-header-footer --print-to-pdf=BLE_FileSync.pdf "file://$PWD/BLE_FileSync.html"`). Covers the wire protocol (UUIDs / opcodes / status bytes), firmware-side ThreadX architecture + state-machine Tick, GUI-side btleplug+tokio worker with the single-stream-per-connection design, and platform notes (macOS sandbox / Linux DBus / Windows WinRT). The HTML's top "Get the GUI" box has per-platform direct-download links to the latest tagged release; bump those links whenever a new release ships and re-render the PDF. Send to field testers via `~/software/pegelstand/whatsapp/send-doc.mjs <jid> Documentation/BLE_FileSync.pdf "<caption>"`.

## Visualization

**User-facing fusion explainer** at `Documentation/Sensor_Fusion.{html,pdf}` (rendered by headless Chrome: `"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless --disable-gpu --no-pdf-header-footer --print-to-pdf=Sensor_Fusion.pdf "file://$PWD/Sensor_Fusion.html"`). Short walkthrough of the six recorded inputs (3D gyro, 3D accel, 3D compass, baro, GPS pos, GPS speed), which two go into Madgwick (gyro+accel), why the LIS2MDL is dropped, and what each downstream consumer does with the quaternion / baro / GPS streams. Send to field testers via `~/software/pegelstand/whatsapp/send-doc.mjs <jid> Documentation/Sensor_Fusion.pdf "<caption>"`. Update the HTML and re-render the PDF whenever the fusion pipeline changes.

All visualisation is done by the `stbox-viz` Rust crate at `Utilities/rust/stbox-viz/`. Single binary, no Python runtime. Four subcommands:

| Subcommand | Output | Input |
|---|---|---|
| `combined` | interactive Plotly HTML (map + nose angle + baro height + speed) | SensNNN.csv + auto-detected GpsNNN.csv |
| `sensors` | 5-panel sensor summary PNG + quaternion/Euler PNG | SensNNN.csv |
| `pumpfoil` | pump-cadence spectrogram PNG + movement-phase PNG | SensNNN.csv |
| `animate` | animated GIF of board orientation per session, optional combined MOV | SensNNN.csv + optional camera video |

Build: `cd Utilities/rust/stbox-viz && cargo build --release`. Binary at `target/release/stbox-viz`. Crate source layout: `io.rs`, `fusion.rs` (Madgwick 6DOF), `euler.rs`, `session.rs` (pitch-oscillation detection), `gps.rs` (haversine + ride detection), `baro.rs` (TC + GPS-anchored water reference), `butter.rs` (4th-order Butterworth + filtfilt), `spectrogram.rs` (scipy-equivalent STFT via `rustfft`), `html.rs` (Plotly JSON emission), `plot_common.rs` (shared `plotters` helpers), and one `*_cmd.rs` per subcommand.

GPS auto-detection (used by `combined` and `animate`) accepts two naming forms next to the sensor CSV: the firmware's on-card layout `SensNNN.csv` ↔ `GpsNNN.csv` (preferred — works directly from a mounted SD card), and the legacy `<stem>.csv` ↔ `<stem>_gps.csv` form for renamed/exported CSVs. Earlier versions only matched the legacy form, which silently failed on raw SD content (`combined` would log "no GPS CSV found" and skip ride detection). See `guess_gps_path` in `main.rs` and `animate_cmd.rs`.

### Combined HTML (`stbox-viz combined`)

**Time-axis flags**: by default the x-axis runs in UTC anchored to the GPS clock. Pass `--tz-offset-h <h>` to shift to local time — `3` for Greek summer (EEST, used at Ermioni), `2` for Swiss summer (CEST), `1` for Swiss winter (CET). The axis title, ride-list times, and hover tooltips all reflect the chosen offset (`Local time (UTC+3)` etc.). The recording date defaults to the sensor file's mtime; override with `--date YYYY-MM-DD` if you've moved files around (`cp` resets mtime to today). Date only affects the rendered date string — time-of-day reads off the GPS clock regardless of date.

Single interactive HTML per recording (saved to `html/`) with:
- **Plotly Scattermap** on `carto-positron` tiles. **One trace per detected ride**, coloured by GPS speed (Viridis, 0–25 km/h). Only the selected ride's trace is drawn at a time — there is deliberately no "All rides" view, because the on-shore/between-ride GPS points drowned out the actual action and the full-session x-axis made the time-series panels extend past where any ride happened. Hover shows UTC + speed + nose angle + height-above-water at every point.
- **Board nose angle** time-series (drift-corrected via 1 s + 60 s rolling medians on the rotated sensor Y-axis elevation).
- **Board height above water** from the LPS22DF baro via `height_above_water_m()`, overlaid with a GPS altitude trace (orange dots) zeroed at the median GPS altitude over stationary samples. Two-stage baro correction: (1) temperature-compensate pressure (the LPS22DF sits in a semi-sealed SensorTile enclosure, so P couples to T via ideal gas — a 10 °C swing between indoor storage and cold seawater produces ~15 hPa of fake altitude, far larger than the 0.08 hPa signal from an 80 cm mast); (2) use GPS speed < 3 km/h as ground truth that the board is in the water (knee-start, between rides, pre/post-session), linear-interpolate the TC'd pressure at those anchors across flying segments, and compute height from the local hypsometric approximation (8434 × (1 − P/P_ref)). Earlier versions used a 10 s rolling-min of altitude which collapsed to ~0 m during sustained flight because every sample in the window was "up". Display pipeline: 250 ms rolling-mean pre-smoothing to kill 10 Hz sensor noise (preserves 1 Hz pump oscillation at ≤4 % attenuation), then bin to 100 ms display buckets. Y-axis clamped to `[-0.1, 0.9] m` to match the physical mast length (80 cm) + small margins, with values outside `[-0.15, 0.95] m` NaN'd so thermal-drift excursions show as **gaps rather than lines saturated at the axis edge** — an honest "here the baro is trustworthy, here it isn't" view. Ticks every 10 cm (`dtick: 0.1`, `tickformat: .1f`). The orange GPS-altitude overlay won't resolve pump strokes (MAX-M10S vertical scatter ~3–10 m) but makes GPS dropouts visible when the board lies flat on water and the antenna loses fix quality. Sub-meter precision would require a mast-mounted ultrasonic height sensor, or a redesigned SensorTile enclosure that keeps the baro out of the water (Peter Schmidlin's idea: integrate antenna + sensors into one smaller housing that sits higher on the board).
- **Speed**, **position-derived** (haversine on GPS fixes) because the module's Doppler Speed column is unreliable on this board (observed median 0.12 km/h while position deltas showed sustained 10–30 km/h). Two-stage filter: (1) `reject_acc_outliers` flags samples where |Δspeed|/Δt > 15 km/h/s (≈4 m/s²) and v > 15 km/h — SUPfoil paddle-strokes produce 1–3 m/s², anything above ~4 m/s² implies a position jump of ≥ 4 m in 1 s, inside the module's 5–10 m horizontal-error envelope. (2) Linear-interp rejected samples, 5 s rolling median, y-axis pinned to 0–30 km/h. Real-data validation: raw peak 83.7 km/h → after gate 29.5 km/h → after smoothing 17.6 km/h — consistent with a paddle-start accelerating gradually to ~15 km/h then gliding. Without the accel gate the 5 s median alone couldn't suppress 30 km/h multipath spikes because any 3-of-5 consecutive bad samples would win.

**Panel captions**: drawn as horizontal annotations centred above each panel (dedicated title strip between panels). Earlier versions used rotated y-axis titles, but the ~180 px panel height couldn't fit any caption longer than ~25 chars without bleeding into the neighbouring panel. Y-axes carry just the unit now (°, m, km/h).

**Ride buttons**: one per detected ride (`Ride 1` / `Ride 2` / …), rendered as a button bar above the chart. Each click uses Plotly's `update` method to do three things atomically:
1. `restyle visible` to show only that ride's map trace
2. `relayout xaxis.range` to zoom the shared time-series x-axis to the ride's window
3. `relayout map.center + map.zoom` to recentre the map on the ride's extent

Page loads with ride 1 selected by default (button `active: 0`).

**Cross-panel hover + click**: `hovermode: "x unified"` with `xaxis.showspikes` draws a vertical grey line through every time-series panel at the cursor x-coordinate, with a single tooltip listing nose / baro / GPS-alt / speed values at that x. Clicking a time-series point pins a dashed red numbered line (1, 2, 3, …) across all panels at that x, via a `plotly_click` JS handler that appends `shapes` and `annotations` to the current layout. A **"Clear click-marks"** button above the chart resets the marker list to the base-layout annotations (panel captions). Map clicks are ignored because scattermap uses lat/lon, not the time axis.

**Map colorbar alignment**: positioned with `y = 1 - map_frac/2`, `yanchor: "middle"`, `len = map_frac × 0.9` so its top + bottom line up with the map's top + bottom in paper coordinates — prevents it from spilling into the time-series area below.

**Ride detection**: sustained GPS movement above 3 km/h for ≥10 s, merging gaps <30 s, padded by 3 s. The pitch-oscillation detector used by `animate` misses smooth flying (board barely pitches); GPS-based detection catches every real ride and skips on-shore activity.

**GPS loader does not decimate**. An earlier `dedupe_by_second()` call kept only the first GPS row per second, which silently threw away 9 of every 10 fixes once the firmware moved to 10 Hz — visible as `9 / 76 ≈ 55 / 543 ≈ 10 %` fix retention in the loader's "{n} GPS fixes" log line. The haversine-based speed calculation then looked at 1-Hz-spaced positions and reported near-zero km/h even for actual rides. Now all fix rows from the CSV are kept, and position-derived speed is computed over the native sample interval.

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

**Wall-clock window mode (`--at HH:MM[:SS]`)** bypasses pitch-oscillation session detection and renders one GIF for an exact wall-clock time slice — needed when the rider is on smooth flight (no pitch oscillation triggers) or when you want to align with external footage. Pair with `--tz-offset-h` (3 for Greek summer / EEST, 2 for CEST, etc.) and `--date YYYY-MM-DD` (defaults to sensor file mtime). Window length is `--duration <s>`; if `--video` is given the video's full length is the default. The video's `creation_time` metadata is probed and printed for diagnostics — if it disagrees with `--at`, use the precise value to align video and animation to the millisecond. Optional `--auto-skip` advances both video and GIF past the carry/transition seconds (irregular pitch before pumping starts) so the side-by-side opens directly on the foiling action.

**GPS-tick anchored slicing — both ends**: the at-window is anchored by finding the GPS row whose UTC is closest to the requested wall-clock time and using its tick directly, rather than linearly extrapolating from the first GPS sample. ThreadX runs on HSI (internal RC, ±1 % accuracy) and drifts ~7 s over a 21-min session; linear extrapolation would slice the wrong sensor range. Tick-anchored slicing is exact regardless of drift because both sensor and GPS rows share the same ThreadX tick counter. **Both** ends of the window use this — earlier the end was computed as `anchor_tick + dur*100`, which assumes 100 ticks = 1 wall-clock-second; that ~1 % HSI drift compounded with the GIF-rate rounding (below) into seconds of late GPS-track-marker position vs. paired camera video at the end of long windows.

**Float-step frame generation + GIF-spec delay rounding** so the GIF's wall-clock playback length exactly matches the data window. Earlier integer step (e.g. `100/15 = 6` ticks per frame at 15 fps) made the GIF run 1.117× too long, drifting the GIF behind a paired video by ~4 s over a 39 s window. Float `step_f = sample_hz / fps` + nearest-sample indexing fixed that *for the data side*. Second pitfall: GIF spec encodes per-frame delay in 1/100-sec units (10 ms resolution), so `1000/15 = 66.67 ms` gets rounded by the gif crate to `70 ms` → real playback rate 14.286 fps, not 15. We pre-round `delay_ms` to a 10-ms multiple, derive `effective_fps = 1000 / delay_ms`, and use that for `step_f` so the encoded GIF rate matches the data step.

**Side-by-side MOV (`--video FILE`)** shells out to `ffmpeg` + `ffprobe` for an `hstack=inputs=2` combined MOV with optional 2 s title-card concat (`--title`, `--subtitle`). `ffmpeg` must be in PATH.

**Phase detection + colour-shaded backgrounds**: in `--at` mode, we detect two phase boundaries and shade the time-series panels accordingly — gray for **Tragen** (board carried, no movement), yellow for **Anschieben/Rennen** (rider in motion but not yet on the foil), green for **Foilen** (sustained foil lift). The push-off transition is detected by the first sample where a 1-sec rolling mean of *raw position-derived speed* exceeds 4 km/h sustainedly for 0.5 s. Earlier attempts using the 5-sec smoothed speed (`speed_at_sensor`) lagged by ~2.5 s and made the flash trigger after the rider was already foiling. The `Foilen` boundary is the first index where the 3-sec mean baro height exceeds 0.15 m. Labels (Tragen / Rennen / Foilen) are centred above each band in the speed panel.

**Push-off-Winkel flash** appears for 2 s at the detected push-off moment, showing the board pitch at that instant ("Push-off-Winkel: ±X.X°"). The angle displayed is the steepest negative nose angle in a ±1 s window around push-off, capturing the actual stepping-down motion. Without `--at`, falls back to the legacy "steepest negative angle in first 10 s" heuristic.

**GPS-track map panel** (top-right, 5-panel layout only): a 2D track of the at-window GPS positions, with the rider's current position drawn as a red dot that grows the track as time advances. Lon × cos(median lat) keeps the aspect ratio physically correct (Greece at 37 °N would otherwise show ~1.25× horizontal stretch). No basemap tiles — keeps the renderer dependency-free.

**Hybrid height construction**: the bare baro `height_above_water_m()` algorithm anchors to any speed < 3 km/h period as "water", which for a dock launch interprets the dock itself as water reference (resulting in dock = 0 m, water = −0.75 m, foiling = −0.25 m; nonsensical). With `--dock-height-m <h>` set (e.g. 0.75 for the Ermioni harbour wall), the dock phase is plotted as a flat constant at `dock_height_m`, the water-set transition crosses 0 m via a 2-sec linear ramp, and the foiling phase uses baro re-anchored to the pressure averaged ±0.5 s around the detected push-off moment. Result: dock shows correct +0.75 m, water = 0 m, foiling shows real mast lift. Two final guards on the foiling phase: (a) the baro height is **clamped to the physical range [−0.1, 0.9] m** (mast = 0.8 m, anything outside is thermal drift) — frame-by-frame video comparison showed end-of-window drift of +1.5 to +2 m without the clamp; (b) **outlier-reject NaN's any sample whose 10-ms pressure delta exceeds 1 hPa** so single-sample sensor glitches don't become phantom −25 m altitude jumps. The smoothing pass is NaN-aware so finite values around an outlier gap still render correctly.

**Per-frame dynamic scales**: every panel's y-axis grows with the running max of the data shown so far — no headroom above the current max. The Nasenwinkel (detrended) panel uses a **95th-percentile-based** y-limit so a single outlier pump doesn't stretch the axis and leave the typical pump activity in the bottom 30 % of the panel; outliers above p95 are visually clipped at the edge. Speed and height panels use plain max. Earlier frames have tight axes that highlight small early movements; later frames expand as peaks come in.

**Scrolling water surface**: the board side view shows a sinusoidal waterline (amplitude 2 cm, wavelength 0.8 m) that scrolls backward at 0.6 m/s, giving the visual impression that the board is moving forward over the water while it pumps. The board's lift uses the GPS-anchored baro height, smoothed with a 0.5-sec rolling mean to suppress pressure-noise spikes that would otherwise make the board jitter visibly up/down between actual pump cycles.

**Panel labels are horizontal** (drawn in their own 40-px title strip above each chart), not the rotated `y_desc` plotters defaults. Title strips also keep the labels from being overdrawn by chart grid pixels.

**3D foil mesh in the side-view panel (`--board-stl <FILE>`)**: when set, the side-view panel renders the full hydrofoil STL (`/Users/zdavatz/software/fingerfoil/stl/0_combined.stl` is the canonical one) as a 3D-rasterized mesh whose orientation comes from the Madgwick quaternion. A pure software rasterizer in `board3d.rs` (no GPU, no winit dependency) loads the binary STL once, pre-rotates it by a hard-coded mount transform `R_mount` (board frame → IMU body frame), then per-frame applies the body-to-world quaternion and projects through a fixed camera into a 600×400 RGBA buffer. Camera is set up as a **pure side view from port** (`eye = (0, 3.2, 0.5)`, `up = world +Z`) so the board's nose-tail axis projects horizontally on screen and a pitch rotation around the lateral axis becomes purely vertical screen motion — no diagonal upper-left/lower-right wobble that came from the earlier 3/4-isometric angle.

Mount transform (derived empirically): `IMU +X = -board +Z` (mast-down direction, anchored by AccX ≈ −1g during foiling), `IMU +Y = +board +X` (nose direction), `IMU +Z = +board +Y` (port direction). The pump-axis swap (Y/Z compared to a plausibly-symmetric earlier mount) was needed because pumps physically rotate the board around its lateral axis, and with the swap that maps to gyro Y oscillations rather than Z — making the rendered pump motion read as pitch (forward) instead of roll (sideways).

**3D model only rendered after push-off**: the side-view 3D foil is **hidden during the entire carry phase** and only appears once `water_set_t` is reached (= sustained GPS speed > 4 km/h) **OR** a fixed time threshold of 10.5 s has elapsed (for clips where the rider is already foiling at the start of the at-window and push-off falls outside it). During carry the panel shows a centered "Tragen — keine 3D-Daten" placeholder over the scrolling water surface.

Reason: the LSM6DSV16X has no magnetometer fusion (the on-board LIS2MDL is unusable due to iron distortion — see the `compass` subcommand notes below) and GPS course-over-ground is unreliable below ~3 km/h. So during the carry phase Madgwick's gyro-integrated yaw drifts freely with no absolute reference. We tried compensating with hand-tuned 180° + 90° corrections that we faded in/out at sec 13, but every fade introduced a visible rotation the rider never made (the SLERP itself, not the IMU), and every snap left a one-frame discontinuity. The cleanest fix is to admit we don't have enough sensor data during carry to render the orientation faithfully, and just hide it until we do.

**Foiling-phase 3D rendering uses accel-only tilt (replaces Madgwick)**: the 3D mesh's body-to-world quaternion is now derived directly from the accelerometer (gravity direction in body frame), not from Madgwick. The accelerometer is low-pass-filtered with a 0.25-sec rolling mean to suppress sub-100 ms linear-acceleration spikes (e.g. brief pump-push impulses or wave hits) while still tracking the ~1 Hz pump-pitch signal. Pitch+roll are extracted via standard atan2 formulas from the smoothed accel vector and reconstructed into a quaternion with no yaw component.

Why drop Madgwick for the 3D rendering: gyro bias integrated around body-Z manifests as a slow rotation around the rendered foil's mast axis even after `quat_strip_yaw` — strip_yaw only removes rotation around the **world** Z axis, but during pumping the body-Z (mast direction) tilts away from world-Z, so a body-Z gyro drift survives. Visually this read as the rendered foil "spinning around its mast" during a clean straight ride. Accel-only attitude has its own failure mode (centripetal acceleration during a turn inflates apparent roll, and a brief sub-200 ms accel spike during a pump push can flip the rendered foil sideways for a frame), but the 0.25-sec smoothing tames both. Madgwick's quaternions are still computed via `fusion::compute_quaternions` and used for the **other** outputs (nose-angle pump trace, push-off-Winkel flash); only the 3D mesh path uses the accel-only path.

**Mount transform for deck-mount boxes**: when the SensorTile.box is mounted on top of the deck rather than on the mast, the chip's printed +Z axis faces *down* through the deck (AccZ ≈ −1 g at level pose). The accel-only tilt code pre-flips Y and Z (180° around X) on the smoothed accelerometer reading before computing pitch+roll, so the resulting quaternion is in a "right-side-up" body frame regardless of which direction the chip's +Z is mounted. `R_mount` is identity for this path. A 180°-world-Z rotation is then pre-multiplied onto the rendered quaternion to align the STL's nose with world+X (away from the camera at world−X = view from behind the tail). Camera position is `eye = (-3, 0, 0.7)`, looking forward toward the nose.

The chip's longer dimension is along chip-Y (= board's nose-tail), and chip-X is along the board's lateral axis. So pitch (nose-up forward pump) shows up as a change in AccY, and roll (port-up lateral lean) shows up as a change in AccX — the formulas use `pitch = atan2(-ay, sqrt(ax²+az²))` and `roll = atan2(ax, az)` accordingly.

**Crash exposes the deck-mount weakness**: a confirmed wipeout clip cut both sensor and GPS log writes off in the *same* tick — classic brown-out signature from water entering the box electronics when the board flips. Mast-mount sessions don't have this exposure because the mast typically stays attached and the IMU stays out of the water. Trade-off: deck-mount gives a cleaner attitude estimate (chip-Z aligned with gravity at level), mast-mount survives crashes.

**`--mount mast|deck` flag**: both setups live behind `--mount`, default `mast`. The flag picks three things together: (1) `R_mount` — `(-z, x, y)` for mast, identity for deck; (2) camera eye — port-side `(0, 3.2, 0.5)` for mast, behind-the-tail `(-3, 0, 0.7)` for deck; (3) per-frame quaternion source for the 3D mesh — Madgwick `quat_strip_yaw(quat_conj(q))` for mast, accel-only tilt with the 180°-X pre-flip for deck. **Mast-mount also recovers the carry-phase tail-place pose**: Madgwick integrates the gyro continuously, so a 90° pitch around the lateral axis (board placed vertical with the tail on the water just before push-off) renders correctly. The deck path's accel-only attitude can't represent that transient pose because gravity briefly aligns with the board's nose-tail direction. Use `--mount deck` only when the box is genuinely deck-mounted; default for everything else.

**Per-cursor value labels in time-series panels**: each of the four time-series panels (Pump-Detail, Höhe über Wasser, Geschwindigkeit, Nasenwinkel) draws a small filled red circle at the current data point and a red text label just to the right of the cursor showing the live value (`±X.X°` / `X.XX m` / `X.X km/h` / `±X.X°`). Source-of-truth is the latest finite sample in each panel's history slice. Y position is panel-top minus ~12 % of the y-range so the badge doesn't collide with the static phase-band labels (Tragen / Rennen / Foilen) at `speed_top * 0.92`.

**Carry-phase suppression of nose-angle + pump counter overlays**: the "Nasenwinkel: ±X.X°" and "Pumps: N" text in the side-view panel are also hidden until push-off — they're meaningless while the rider is just walking (the IMU's "pitch" reading is whatever angle the carry hold puts the mast at, and pump count is gated to speed > 4 km/h anyway). The `Zeit:` clock stays on always and now shows hundredths-of-a-second resolution (`M:SS.HH`) for tighter video sync.

Without `--at`, session detection is **pitch-oscillation based** (≥ 0.3 Hz over ≥ 30 s, merging < 60 s gaps). Smooth-flight pumpfoil data without clear pitch oscillation won't register — use `combined` (GPS-based) instead for those.

### Compass Validity (`stbox-viz compass`)

Answers "is the LIS2MDL magnetometer usable as a heading reference on this board?" Takes a sensor CSV + its companion GPS CSV, runs Madgwick 6DOF for orientation, extracts roll + pitch (ignores the yaw from 6DOF since it has no absolute reference), tilt-compensates the body-frame mag vector via the Honeywell AN203 identities, and computes heading = `atan2(-x_h, y_h)` CW from magnetic north (using body-Y as the board's nose direction, matching `fusion.rs::nose_angle_series_deg`). Compares against the u-blox GPS course over ground at samples where ground speed > 2 km/h.

Output: `png/plot_compass_<stem>.png` with two stacked panels — mag heading and GPS course overlaid on top, residual (mag − GPS) below. Console prints median residual (should match the local magnetic declination: ~+4° E in Greece, ~+2.5° E in Zürich) and p5–p95 span (> ~30° suggests iron distortion dominates over real heading changes). First run on real-board data: median −14°, p5–p95 span 307° — essentially random, consistent with the metal-screw hard/soft-iron distortion on the current housing. The fix is hardware: plastic screws or an all-plastic SensorTile enclosure with the sensors away from ferromagnetic mounting hardware.

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

## MovementLogger GUI

Cross-platform (Win/Mac/Linux) drag-and-drop wrapper around `stbox-viz animate`. Source at `Utilities/rust/stbox-viz-gui/`, binary called `MovementLogger`. Drop a sensor CSV (auto-pairs the matching `_gps.csv`), an optional camera `.mov`/`.mp4`, and an optional board `.stl`; fill in `--at`, `--tz-offset-h`, `--mount`, etc. in the form; click Generate. The GUI shells out to the bundled `stbox-viz` CLI (looked up next to its own `current_exe()` first, then PATH) so the heavy plotters/rustfft/gif deps stay out of the GUI binary, and stdout/stderr stream into the live log panel.

**Window-layout conventions** (these apply to *all* egui apps, not just MovementLogger):

- The version goes in the **OS window title only**, via `ViewportBuilder::with_title(format!("AppName {}", env!("CARGO_PKG_VERSION")))` and the same string as the `eframe::run_native` app id. **Don't** also put `ui.heading("AppName 1.2.3")` at the top of the central panel — the OS chrome already shows it on every platform; duplicating it is just noise.
- A short tagline inside the window is fine if it adds context the title bar doesn't (e.g. "SensorTile.box pumpfoil session video generator"). Make it `ui.hyperlink_to(tagline, repo_url)` so users can jump to the project page.
- Top-right of the title strip: the app logo, drawn as a frameless `egui::ImageButton` that opens `mailto:<support>` via `ctx.open_url(OpenUrl::new_tab("mailto:..."))`. Bake the PNG into the binary with `include_bytes!("../assets/icon.png")`, decode once with `image::load_from_memory` (default-features off, only `png` enabled), and lazy-upload to `ctx.load_texture` on the first frame. Reuse the same PNG bytes in `egui::IconData` passed to `ViewportBuilder::with_icon` so the OS chrome / Dock / taskbar carry the logo too.

Workspace at `Utilities/rust/Cargo.toml` lists `stbox-viz` and `stbox-viz-gui` as members. The vendored `Utilities/rust/winit-patched/` is `exclude`d from the workspace (it ships its own workspace) and wired in via `[patch.crates-io] winit = { path = "winit-patched" }`. **The Mac App Store winit patch from the workspace `CLAUDE.md` is required** — eframe → winit 0.30 calls `_CGSSetWindowBackgroundBlurRadius` (private CoreGraphics) which Apple's binary scanner rejects. The fork's patch replaces `Window::set_blur` with a no-op and comments out the now-unused `NSInteger`/`AnyObject` imports in `ffi.rs` so `RUSTFLAGS=-Dwarnings` doesn't trip on them. Verify after every release build: `nm target/release/MovementLogger | grep CGSSetWindowBackgroundBlur` must return nothing. **eframe must stay on 0.29 or newer**: 0.28 drags in winit 0.29 alongside 0.30, the patch only matches the 0.30 path, and the private symbol slips back into the binary through the unpatched 0.29 dep.

### BLE FileSync panel (v0.1.4+)

Collapsible "BLE FileSync" section in the central panel — talks to a SensorTile.box running the SDDataLogFileX firmware (see "BLE FileSync — download SD-card files over Bluetooth" above for the wire protocol). Workflow: Scan (5 s) → click STBoxSync → Connect (OS pops Bluetooth permission + PIN dialog, PIN `123456`) → Refresh file list → tick rows → Download selected. Files saved to `csv/` (configurable). `Sens*.csv` and `*_gps.csv` auto-route into the form's Sensor / GPS slots so the user can hit Generate without re-dragging.

Backend lives in `src/ble.rs`. Uses `btleplug` (cross-platform: CoreBluetooth on macOS, BlueZ on Linux, WinRT on Windows) on a tokio current-thread runtime that lives on a single dedicated worker thread. `std::sync::mpsc` channels shuttle commands and events to/from the egui side. **One notification stream per connection, not per op** — opened on `Connect`, demuxed inside a `tokio::select!` between the command channel and the stream itself. Per-op streams (`p.notifications().await` per LIST/READ) risk losing the first packet if the box notifies before the `await` is parked. A 200 ms watchdog tick wakes the loop frequently enough to surface a stuck transfer instead of spinning.

Status-byte detection on READ: only treat a single-byte first notify as an error when the byte is in `{0xB0, 0xE1, 0xE2, 0xE3}` (`is_status_byte`). A 1-byte CSV/log file (whose lone byte is plain ASCII) streams correctly because no legitimate file ever starts with one of those high-range bytes.

Disconnect mid-op: tearing down the peripheral surfaces an explicit error like `READ Sens005.csv aborted by disconnect at 1234/5678 B` and sets state back to Idle — no orphan worker spinning on a dead stream.

macOS specifics: the bundled `.app` carries `NSBluetoothAlwaysUsageDescription` (`assets/Info.plist.template`) and the App-Sandbox build adds `com.apple.security.device.bluetooth` (`entitlements-appstore.plist`). A bare `cargo run` on a fresh user account may not trigger the consent prompt and `btleplug` will silently report no adapter — install the `.app` for the proper permission flow.

### Releases

GitHub Actions workflow at `.github/workflows/release.yml`. Tag `vX.Y.Z` and push — the workflow runs the per-platform matrix (Linux x86_64 + aarch64 CLI-only, macOS Apple Silicon, Windows x86_64), packages each as `MovementLogger-vX.Y.Z-<target>.tar.gz`/`.zip` with SHA256, and attaches everything to a GitHub Release. The macOS path also assembles a `MovementLogger.app` bundle (with `stbox-viz` shipped inside `Contents/MacOS/` so the GUI's `current_exe()`-relative lookup finds it) and a notarized DMG named `MovementLogger-vX.Y.Z-macos-aarch64.dmg`.

**Intel Mac dropped after v0.1.5.** Through v0.1.5 the workflow built both `x86_64-apple-darwin` and `aarch64-apple-darwin`, then `lipo`'d into a universal DMG. Removed because (a) every Mac shipped since late 2020 is Apple Silicon, (b) the `macos-13` Actions runner queue was wedging release builds for 30+ minutes waiting for an Intel runner that never picked up. The change is one matrix entry deleted from `build` and the `macos-store` job simplified to a single aarch64 cargo build (no `lipo`, no universal binary). Anyone needing an Intel build builds locally: `cargo build --release --target x86_64-apple-darwin -p movement-logger`.

**Re-using a tag after deletion is allowed but races the publish step.** The publish job runs with `if: always() && (one of build/macos-store/windows-msix succeeded)`, which means even a *cancelled* run still triggers publish if at least one job had completed successfully. So `gh run cancel` on a release run after `macos-store` succeeded WILL publish the partial release before the workflow fully terminates. If you then delete the tag and re-push to fix something, the new run can't `gh release create` because the release already exists. Either delete the GitHub release first (`gh release delete vX.Y.Z --yes --cleanup-tag`) or accept that the originally-published artefacts stay live and the new run's matching-name uploads collide.

**Notarize + staple the .app, not just the DMG (v0.1.6+).** v0.1.5 stapled only the DMG file. When the user drags the .app out to /Applications, Gatekeeper looks for a notarization ticket on the .app itself; if none is found locally it falls back to an online lookup. Field tester Peter, often offline on a sailboat, hit `nicht geöffnet` because the online lookup failed. The fix re-orders the steps in `macos-store` so the .app is notarized + stapled BEFORE the DMG is built (`ditto -c -k --keepParent .app .zip` → `notarytool submit zip --wait` → `stapler staple .app` → `hdiutil create dmg`), with the DMG also notarized + stapled afterwards as belt-and-suspenders. A `spctl -a -vvv --type execute` sanity check after the .app staple makes a future workflow regression fail loud instead of silently shipping an unverified .app.

**The Apple signing secrets must be set per-repo — there is no org-level default for personal accounts.** The original global CLAUDE.md note ("the same secret names as `~/software/rust2xml` so org-level secrets carry over") is wrong for `zdavatz/*` repos because `zdavatz` is a user account, not a GitHub org. Each new repo that wants notarized DMGs needs all 10 secrets set explicitly:

```sh
REPO=zdavatz/<new-repo>
gh secret set APPLE_TEAM_ID         --repo "$REPO" --body '4B37356EGR'
gh secret set APPLE_API_KEY_ID      --repo "$REPO" --body '7B9HFNP99B'
gh secret set APPLE_API_ISSUER_ID   --repo "$REPO" --body '69a6de70-0490-47e3-e053-5b8c7c11a4d1'
gh secret set APPLE_API_KEY_P8                  --repo "$REPO" < <(base64 -i ~/.apple/AuthKey_7B9HFNP99B.p8)
gh secret set MACOS_DEVELOPER_ID_CERTIFICATE    --repo "$REPO" < <(base64 -i ~/Library/Mobile\ Documents/com~apple~CloudDocs/ywesee/p12/developer_id_application.p12)
gh secret set MACOS_CERTIFICATE                 --repo "$REPO" < <(base64 -i ~/Library/Mobile\ Documents/com~apple~CloudDocs/ywesee/p12/mac_app_distribution.p12)
gh secret set MACOS_INSTALLER_CERTIFICATE       --repo "$REPO" < <(base64 -i ~/Library/Mobile\ Documents/com~apple~CloudDocs/ywesee/p12/mac_installer_distribution.p12)
read -s -p "p12 password: " P; echo
for S in MACOS_CERTIFICATE_PASSWORD MACOS_INSTALLER_CERTIFICATE_PASSWORD MACOS_DEVELOPER_ID_CERTIFICATE_PASSWORD; do
  printf '%s' "$P" | gh secret set "$S" --repo "$REPO"
done; unset P
```

The `if:` gates on `env.APPLE_API_KEY_P8 != '' && env.MACOS_DEVELOPER_ID_CERTIFICATE != ''` will silently skip notarization when any required secret is missing, so a fresh repo's first release ships an unsigned binary unless these are set first. Verify with `gh secret list --repo "$REPO"` — should show all 10 names.

Optional store paths (mirrored from rust2xml, gated on repo/org variables):

| Var | Effect |
|---|---|
| `vars.MACOS_STORE_ENABLED == 'true'` | aarch64 `.app` → signed DMG (Developer ID, notarized) → optional Mac App Store `.pkg` upload via altool/iTMSTransporter. **Must be set explicitly per repo** (`gh variable set MACOS_STORE_ENABLED --repo … --body 'true'`) — there's no org-level default. Without it the macos-store job is silently skipped and no DMG ever lands on the GitHub Release. |
| `vars.MSSTORE_ENABLED == 'true'` | MSIX pack → signed → optional Microsoft Store devcenter REST submission. |

The workflow uses **the same secret names as `~/software/rust2xml`** so org-level secrets carry over without repo-level setup: `MACOS_CERTIFICATE`, `MACOS_CERTIFICATE_PASSWORD`, `MACOS_INSTALLER_CERTIFICATE`, `MACOS_INSTALLER_CERTIFICATE_PASSWORD`, `MACOS_DEVELOPER_ID_CERTIFICATE`, `MACOS_DEVELOPER_ID_CERTIFICATE_PASSWORD`, `APPLE_API_KEY_P8`, `APPLE_API_KEY_ID`, `APPLE_API_ISSUER_ID`, `APPLE_TEAM_ID`, `MACOS_PROVISIONING_PROFILE`, `WINDOWS_CERTIFICATE`, `WINDOWS_CERTIFICATE_PASSWORD`, `MSSTORE_TENANT_ID`, `MSSTORE_CLIENT_ID`, `MSSTORE_CLIENT_SECRET`, plus `vars.MSSTORE_APP_ID`. Bundle id is `com.ywesee.movementlogger` (separate from rust2xml's `com.ywesee.rust2xml`) — needs its own App Store Connect record + provisioning profile before the Mac App Store gate flips on.

Local sanity build: `cd Utilities/rust && cargo build --release -p movement-logger -p stbox-viz`. Re-run the `nm` check above whenever bumping eframe/winit.

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
