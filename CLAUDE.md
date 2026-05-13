# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

FP-SNS-STBOX1 is an STM32Cube Function Pack for two STMicroelectronics evaluation boards:
- **STEVAL-MKBOXPRO** (SensorTile.box PRO) — STM32U585 MCU, BlueNRG-LP BLE, sensors: STTS22H, LPS22DF, LSM6DSV16X, LIS2DU12, LIS2MDL
- **STEVAL-STWINBX1** (STWIN.box) — STM32U585 MCU, BlueNRG-2 BLE, sensors: IIS2DLPC, IIS2MDC, IIS3DWB, ISM330DHCX, IIS2ICLX, ILPS22QS, STTS22H

All firmware is bare-metal C targeting STM32U5xx with STM32 HAL.

## Build & Toolchains

IDE projects (each application ships all three):
- **STM32CubeIDE** V1.18.1 — `<app>/STM32CubeIDE/` (`.cproject`/`.project`)
- **IAR EWARM** V9.60.3 — `<app>/EWARM/` (`.ewp`/`.eww`)
- **Keil MDK-ARM** V5.38.0 — `<app>/MDK-ARM/` (`.uvprojx`)

STM32CubeMX `.ioc` files in each application root for regenerating peripheral init code.

**Command-line build** (BLESensorsPnPL and SDDataLogFileX only): `make` in the `STM32CubeIDE/` directory. Toolchain path is auto-detected by platform in `config.mk` at the repo root via `uname -s`:
- **macOS**: `$(HOME)/.software/arm-gnu-toolchain/bin`
- **Linux**: `/usr/bin`

Override per invocation: `make TOOLCHAIN=/other/path`.

SDDataLogFileX exposes `GPS_RATE_HZ` (default 10, valid 1..25): `make GPS_RATE_HZ=5`. Passed through as `-DGPS_RATE_HZ=<n>` to drive `UBX-CFG-RATE` in `gps_nmea.c`. Header has a `#error` guard.

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
    Components/       # Sensor/NFC drivers (PID-based names)
  CMSIS/
  STM32U5xx_HAL_Driver/
Middlewares/
  ST/
    BlueNRG-2/        # BLE stack for STWINBX1
    BlueNRG-LP/       # BLE stack for MKBOXPRO
    STM32WB07_06/     # Additional BLE stack variant
    STM32_BLE_Manager/
    PnPLCompManager/  # PnP-Like Component Manager
    ST25FTM/          # NFC Fast Transfer Mode
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
- `BLEDualProgram` — BLE FOTA with secure PIN connection + NFC pairing
- `BLEMLC` — BLE + Machine Learning Core (LSM6DSV16X)
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

Each application:
- `Inc/` — headers; `Src/` — source files
- `Inc/stbox1_config.h` — Central config: feature flags, timing, BLE options
- `Inc/main.h` / `Src/main.c` — HAL init, peripheral setup, main loop
- `Src/app_<name>.c` — Application logic entry point
- `Src/ble_implementation.c` — BLE characteristic callbacks
- `Src/ble_function.c` — BLE connection management
- `Src/ota.c` — OTA firmware update (when applicable)
- `Binary/` — Pre-built `.bin` files

## Configuration Flags (stbox1_config.h)

- `STBOX1_ENABLE_PRINTF` — debug output via UART. Default off in SDDataLogFileX because UART4 is wired to GPS.
- `STBOX1_BLE_SECURE_CONNECTION` — BLE PIN security. Disable for some Android compatibility issues.
- `BLE_FORCE_RESCAN` — force BLE service re-scan. Disable for some Android compatibility issues.
- `STBOX1_UPDATE_ENV` / `STBOX1_UPDATE_INV` — sensor polling intervals (timer ticks).
- `STBOX1_LOG_AUDIO` (SDDataLogFileX) — gates `BSP_AUDIO_IN_*` calls and `.wav` files. Default `0`. Set to `1` only on unmodified hardware; on the 3.3V-modded board `BSP_AUDIO_IN_Init` blocks indefinitely.
- `STBOX1_LOG_BATTERY` (SDDataLogFileX) — gates the STC3115 fuel-gauge path and `BatNNN.csv`. Default `1`. I²C failure is non-fatal — writes a marker to the error log and skips battery logging for the rest of the boot.
- `STBOX1_ENABLE_BLE_SYNC` (SDDataLogFileX) — gates BLE FileSync (advertising + GATT + ThreadX BLE thread). **Default `0`** for builds that don't need BLE; flip to `1` for the issue #15 mode-switch firmware. When `1`: box advertises as `PumpTsueri` within ~8 s of boot, iPhone/Linux GUI can LIST/READ/DELETE/START_LOG. Cost when on: +28 KB flash, +4.5 KB BSS. Unblocked 2026-05-11 by adding the missing `EXTI11_IRQHandler` strong override (`stm32u5xx_it.c` had EXTI13 only; weak-bound EXTI11 fell through to `Default_Handler` infinite-loop, wedging the whole chip on first BlueNRG-LP IRQ). See "BLE bring-up — current state".
- `STBOX1_ENABLE_WLC` (SDDataLogFileX) — BlueNRG-LP OTP programmer (reverse-engineered from ST firmware). Default `0`, **untested on real hardware**, no call site wired in. The flag-off compile is byte-identical to a build without the file.
- `STBOX1_ENABLE_USB_CDC` (SDDataLogFileX) — USB CDC ACM virtual COM port over USB-C. Default `0`. When `1`, brings up OTG_FS via TinyUSB and routes `printf` / `STBOX1_PRINTF` over USB. Build with `make USB_CDC_ENABLED=1` (Makefile flag overrides the header default and pulls in the vendored TinyUSB stack). Cost when on: +15 KB flash, +5 KB BSS. **macOS Sequoia 15+ does NOT expose `/dev/tty.usbmodem*` for our descriptor** (kernel CDC driver `AppleUSBCDCCompositeDevice` partial-attaches in `IOMatchDefer = Yes` state and never registers; libusb can't claim the bulk endpoint either, even as root). Use a Linux box for live debug — `cdc_acm` attaches cleanly and `cat /dev/ttyACM0` streams the heartbeat. See "USB CDC ACM debug console" section below.

## SDDataLogFileX — Critical Configuration

Things that have been changed from CubeMX defaults and **must stay set**:

### Clock
**HSI** (internal 16 MHz) is the PLL source, not HSE. The 3.3V board mod (for the GPS module) made the external crystal unreliable on battery boot. PLL/PLL2/PLL3 all source from HSI; sysclk = 160 MHz.

### MSP / power
**`HAL_PWREx_EnableVddIO2()` in `Core/Src/stm32u5xx_hal_msp.c::HAL_MspInit`**. STM32U585 gates the GPIO bank fed by VddIO2 (PG[15:2] on Rev_C) behind a separate IO supply rail. Without this enable, BLE init fails on a chip that works fine under stock ST firmware. Easy to lose during a CubeMX regenerate — verify if `bluetooth_init()` fails after a regenerate.

### NVIC priorities
| IRQ | Priority | Why |
|---|---|---|
| EXTI13 (user button) | 0 | Stock |
| UART4 (GPS) | 6 | Above SDMMC, below SD |
| SDMMC1 | 14 (`BSP_SD_IT_PRIORITY`) | Below UART4 so GPS line keeps assembling |
| EXTI11 (BLE HCI IRQ) | 14 | Same as SDMMC so BLE never preempts in-flight SD write; bounded ISR loop in `hci_tl_interface.c::hci_tl_lowlevel_isr` caps at 16 events per IRQ |

### GPIO
- **PC13 (user button) = `GPIO_PULLDOWN`** in BSP. Boards with the button physically disconnected float PC13 → continuous EXTI storm at priority 0 → firmware hangs.
- **PD12/PD13 (I²C4) = `GPIO_PULLUP`** for the STM32U5 internal ~40 kΩ pulls (STC3115). External 4.7 kΩ may be needed if a board still NAKs.

### SDMMC
- **`SDMMC_NSPEED_CLK_DIV`** (~12.5 MHz), not `SDMMC_HSPEED_CLK_DIV` (~25 MHz). Gives the 3.3V rail more setup/hold margin. NSPEED's ~6 MB/s ceiling is well above the logger's ~5 KB/s budget. See `Drivers/BSP/SensorTileBoxPro/SensorTileBoxPro_sd.c`.

### I²C4 (STC3115 fuel gauge)
- Timing: **`0xA040184A`** (~145 kHz). The v2.0.0 default `0x00F07BFF` (~421 kHz) overshoots STC3115 Fast-Mode.
- `app_filex.c` runs a one-shot probe before `BSP_GG_Init`: `BSP_I2C4_Init()` + `HAL_I2C_IsDeviceReady(&hi2c4, 0xE0, 3, 200)`. Result logged as `gauge: i2c4_init=<rc> ping_0xE0=<ACK|NAK> halerr=0x<hex>`. Decode: `halerr=0x04` = address NAK (hardware/power); `halerr=0x20` = bus stuck low (pull-ups/wiring).

### ThreadX
- `TX_APP_MEM_POOL_SIZE` = **16 KB** in `AZURE_RTOS/App/app_azure_rtos_config.h` (covers BLE thread's 4 KB stack + headers/alignment + margin).
- `FX_APP_MEM_POOL_SIZE` = **14 KB** (after gps_thread was added).
- The four `tx_application_define` failure paths (tx pool create, App_ThreadX_Init, fx pool create, MX_FileX_Init) call `halt_red_morse(N)` in `app_azure_rtos.c`. Patterns: 2 = tx pool, 4 = App_ThreadX_Init, 6 = fx pool, 8 = MX_FileX_Init.

### BLE bring-up (when `STBOX1_ENABLE_BLE_SYNC=1`)
- **`bluetooth_init()` returns `uint8_t`** (`extern uint8_t bluetooth_init(void);` in `ble_implementation.h`) — chip-presence gate. On non-zero rc the BLE thread parks in `tx_thread_sleep(1000)` and never arms EXTI11; logger continues unaffected.
- **Two-stage chip-alive probe** in `Core/Src/ble_sync.c::ble_chip_alive_probe()` before `bluetooth_init()`: (1) `hci_tl_spi_send(HCI_Reset)` returns 0 within 15 ms, (2) chip raises IRQ within 500 ms with Command Complete. Bounded worst case ~670 ms. A SPI-ACK-only probe isn't enough — half-dead BlueNRG-LP can ACK SPI bytes but hang inside `init_ble_manager`.
- **2-second `HAL_Delay(2000)` between probe and `bluetooth_init()`** — mirrors ST's production pattern (decompiled `FUN_08031468`). BlueNRG-LP needs this gap after HCI Reset for RF subsystem bring-up.
- `g_ble_probe_status` codes (consumed by `app_filex.c::COMMAND_SAVE_SENSORS` periodic logger): 0xF0=thread entered, 0xF1=in probe, 0xF2=in `bluetooth_init`, 0xF3=transient, 0xF4=in `init_ble_int_for_blue_nrglp`, 0=probe failed, 1=advertising, 2=init failed, 0xFF=pending.

#### Issue #15 mode-switch architecture (2026-05-10, work in progress)

In response to issue #12 (BLE EXTI11 NVIC enable wedges SDMMC, even at priority 14/15), BLE and SDMMC are now structured to never run concurrently:

1. Boot → BLE mode by default, advertising as `PumpTsueri`. Logger does not auto-start. fx_thread idle on `MessageQueue` receive.
2. iPhone connects, sees `PumpTsueri`. App can list / read / delete existing SD files via `OP_LIST` / `OP_READ` / `OP_DELETE`.
3. App writes `OP_START_LOG` (`0x05`) + 4 LE bytes duration → box stores `BKP1R = 0x4C4F4720` ("LOG ") + `BKP2R = duration` then `NVIC_SystemReset`.
4. After reset main.c reads BKP1R; if magic, sets `g_app_mode = APP_MODE_LOG` + `g_log_duration_seconds` from BKP2R, clears BKP1R. fx_thread sees LOG mode, opens files, runs ST's reference logger. ble_sync_thread parks.
5. fx_thread monitors `tx_time_get()/100 >= g_log_duration_seconds` per `COMMAND_SAVE_SENSORS` tick. On expiry: closes files, flushes, `fx_media_close`, clears BKP1R/BKP2R, `NVIC_SystemReset`.
6. Reboot lands back in BLE mode. iPhone reconnects, downloads new files.

Wire-up:
- `Core/inc/main.h` — `app_mode_t` enum, `g_app_mode` / `g_log_duration_seconds`.
- `Core/Src/main.c` — TAMP read at boot, sets globals, clears magic.
- `Core/Src/ble_filesync.c` — `OP_START_LOG` handler.
- `FileX/App/app_filex.c::read_thread_entry` — auto-`COMMAND_START_LOG` gated on `g_app_mode == APP_MODE_LOG`; expiry path in `COMMAND_SAVE_SENSORS`.
- `Utilities/rust/stbox-viz-gui/src/{ble.rs,main.rs}` — `BleCmd::StartLog{duration_seconds}`, "Start session" button + DragValue.

User-button gate (v227, 2026-05-10): in BLE mode a short button press **starts a 5-minute LOG session** via the same TAMP-magic + reset path the GUI's `OP_START_LOG` uses. Originally the issue #15 spec was "GUI-only", but with BLE init currently broken the iPhone never sees `PumpTsueri` and there is no way to start a session — the button is the only fallback. In LOG mode the button still aborts a session early. See `app_filex.c::read_thread_entry`.

ErrorLog at boot (v227): `ErrorLog_Open()` is called unconditionally right after `CheckAndApplyFirmwareUpdate()` — boot markers, BLE bring-up trace, and any later `ErrorLog_Write` calls now land on SD in BOTH modes. Previously gated inside `case COMMAND_START_LOG:` so BLE-mode boots produced no log file at all.

Mode-switch state survives via `TAMP->BKP1R` (LOG magic) + `TAMP->BKP2R` (duration in seconds) + `TAMP->BKP0R` (DFU magic — unrelated, see "Software DFU loop" below).

#### BLE bring-up — current state (2026-05-11, v254, working end-to-end)

> **TL;DR — Issue #15 mode-switch works end-to-end on Linux/iPhone.** Box advertises as `PumpTsueri` within ~8 s of boot; MovementLogger GUI and nRF Connect both connect, LIST, READ, START_LOG. Logger reboot cycle (LOG mode → reboot → BLE mode) tested. Diagnostic markers from the bring-up bisect (v218 → v254) are still in tree and can be cleaned up.

**Root cause of the v218-v247 wedge** (commit `96c6f829`): `stm32u5xx_it.c` had a strong handler for `EXTI13_IRQHandler` (user button) but no strong override of `EXTI11_IRQHandler` (BlueNRG-LP HCI IRQ). The GCC startup file weak-binds unhandled interrupts to `Default_Handler`, which is literally `b Default_Handler` — an infinite loop. The moment `hci_tl_spi_reset` re-enabled the EXTI11 NVIC and the BlueNRG-LP raised its IRQ line, the CPU vectored into `Default_Handler` and froze: no thread runs, USB heartbeat stops, BLE init wedges with no further trace. BLEDualProgram's `stm32u5xx_it.c:324` has the handler (`HAL_EXTI_IRQHandler(&H_EXTI_11)`); we didn't. Fix: add the same handler plus `g_exti11_irq_count` so future bring-up issues can be triangulated against ISR activity (surfaced in the USB heartbeat as `exti11=N`).

**Advertising name `PumpTsueri`** (commit `1c8017e0`): the BLE manager middleware hardcoded a 7-char name field in the advertising packet (`manuf_data[3] = 8` = 1 AD-type byte + 7 name bytes), so longer names truncated silently. Three changes:
- `Middlewares/ST/STM32_BLE_Manager/Inc/ble_manager.h`: `board_name[8]` → `board_name[16]`, `BLE_MANAGER_ADVERTISE_DATA_LENGHT` 28 → 31 (BlueNRG-LP / STM32WB07_06 branch only).
- `Middlewares/ST/STM32_BLE_Manager/Src/ble_manager.c::set_connectable_ble`: rewritten as dynamic-length packet construction. Name field length = 1 + strlen(board_name); manufacturer-specific section starts at pos = 5 + name_len; final packet size (3 + 2 + name_len + 16 = 21 + name_len) passed to `aci_gap_set_advertising_data_nwk`. Apps with shorter names still work — packet just gets shorter.
- `Core/inc/ble_implementation.h`: `BLE_FW_PACKAGENAME "STBoxFs"` → `"PumpTsueri"`.

**Header dependency gotcha**: the SDDataLogFileX Makefile doesn't generate `.d` files, so changes to headers don't trigger rebuilds of `.c` files that include them. Widening `board_name[8]` → `board_name[16]` in `ble_manager.h` rebuilt only `ble_manager.c` and `ble_implementation.c`, leaving `ble_function.c` linked with the old 8-byte struct layout. `ble_function.c::ble_set_custom_advertise_data` then wrote `board_id` to the old offset; `ble_manager.c::init_ble_manager` read from the new offset and got 0 → `Error ble_stack_value.board_id Not Defined`. **Always `make clean` after header changes** until `-MMD -MP` is wired in.

**GUI side** (`Utilities/rust/stbox-viz-gui/src/ble.rs`): `BOX_NAME` filter updated to `"PumpTsueri"`. Scan emits per-peripheral debug lines (`seen: addr=… name=…`) plus a summary count so future name mismatches are easy to diagnose. Status label is `egui::Label::selectable(true)` so users can copy error text. Pairing on Linux requires a `bluetoothctl agent KeyboardOnly + default-agent + pair + trust` once — after that the bond is cached and the GUI's `connect()` succeeds in 1-2 s without re-prompting.

**Known open issues:**
- LIST state machine doesn't always return to `Idle` after the firmware's terminator notify, so subsequent Download hits "another op is in flight" until the 20 s watchdog fires. Workaround: Disconnect + Reconnect. Real fix: shorten LIST watchdog to ~2 s or use inactivity heuristic.
- Diagnostic markers left in the BLE init path can be cleaned up:
  - `Middlewares/ST/STM32WB07_06/hci/hci_tl_patterns/Basic/hci_tl.c` — `send_cmd` + `hci_send_req` markers + `HAL_GetTick` shim
  - `Projects/STEVAL-MKBOXPRO/Applications/Rev_C/SDDataLogFileX/Core/Src/hci_tl_interface.c` — bisect markers in `hci_tl_spi_send` + `hci_tl_spi_reset`
  - `Middlewares/ST/STM32_BLE_Manager/Src/ble_manager.c` — flushes around hci_init/hci_reset

**Verified flow (2026-05-11):**
```
power on → ~8 s → bluetoothctl scan le shows "PumpTsueri" DE:32:D2:B8:27:78
nRF Connect (iPhone) → CONNECT → PIN 123456 → LIST opcode (0x01) → file rows + terminator
MovementLogger GUI (Linux, after bluetoothctl pair+trust) → Scan → Connect → Refresh → file list populates
```

#### USB CDC `tud_mounted` gate (v224, 2026-05-10)

`Core/Src/usb_cdc.c::UsbCdc_Write` now gates on `tud_mounted()` instead of `tud_cdc_connected()`. The latter requires the host to assert DTR via SET_CONTROL_LINE_STATE — Linux `cdc_acm` does this on `open()`, **macOS does not** (`cat`/`pyserial` both leave DTR low; only `screen`/`picocom` set it). Gating on enumeration alone makes the heartbeat work on Mac without DTR ceremony. Heartbeat printf is also hand-formatted (no newlib stdio) so it never deadlocks against another thread holding `_REENT`.

### LIS2MDL magnetometer (`Drivers/BSP/Components/lis2mdl/lis2mdl.c` Init)
Three additions for drift reduction:
- Offset cancellation every ODR cycle (`LIS2MDL_SENS_OFF_CANC_EVERY_ODR`)
- Temperature compensation (`lis2mdl_offset_temp_comp_set`)
- Low-pass filter at ODR/4 (`LIS2MDL_ODR_DIV_4`) — 25 Hz BW at 100 Hz ODR

## SD Card Data Format (SDDataLogFileX)

Up to four files per session:
- `SensNNN.csv` — sensors at ~100 Hz: timestamp (ticks), acc XYZ (mg), gyro XYZ (mdps), mag XYZ (mgauss), pressure (hPa), temperature (°C). Gyro FS = 500 dps (17.5 mdps/LSB), accel FS = 4 g (0.122 mg/LSB).
- `MicNNN.wav` — mono 16-bit PCM WAV at 16 kHz.
- `GpsNNN.csv` — GPS fixes at 10 Hz: timestamp (ticks), UTC (hhmmss.ss), lat/lon (decimal degrees, signed), alt (m), speed (km/h), course (deg), fix quality, num satellites, HDOP. Rows only on a new fix; empty (header only) if module not connected or no fix.
- `BatNNN.csv` — STC3115 at 1 Hz: timestamp (ticks), voltage (mV), SOC (0.1%), current (100 µA, signed: positive = charging). Written from the same `COMMAND_SAVE_SENSORS` flush tick as sensor data so ungraceful power-off never leaves a 0-byte file. Start/stop readings also land in error log: `start: battery 4150 mV 98.3%`.

Timestamps are **ThreadX tick counts** (1 tick = 10 ms), not raw milliseconds. CSV header column is `Time [10ms]` (current) or `Time [mS]` (legacy, misnamed).

Logging starts automatically on power-on; user button stops/restarts. File counter auto-increments. First 200 ms of audio is discarded (mic glitch workaround). Core logging logic in `FileX/App/app_filex.c`.

### LED behaviour
- 1 green blink after clock/ICache/LED init
- 2 green after `GPS_Init()`
- 3 green after `InitMemsSensors()`
- Green solid on after ThreadX starts = logging active
- Green off = logging stopped
- Red blinking = `Error_Handler` fatal error (see error log)
- Red solid = SD firmware update in progress, or pre-green-blink hang
- Red morse 2/4/6/8 = ThreadX/FileX init failure (see ThreadX section above)
- Red morse N during sensor init = which sensor blocked (see below)

`InitMemsSensors()` calls `DiagBlinkRed(n)` before each MEMS init: 1 = entered, 2 = LIS2MDL, 3 = LSM6DSV16X, 4 = LPS22DF, 5 = STTS22H, 6 = exit. Costs ~3 s of boot but pinpoints which I²C/SPI transaction blocks. `DiagBlinkRed` is exported in `main.h` for use elsewhere.

### Audible boot signals (piezo on PE13, TIM1_CH3 PWM, `Core/Src/buzzer.c`)
- `Buzzer_BootDone()` — two ascending tones (1500 Hz / 3000 Hz, 90 ms each) from `main()` right before `MX_ThreadX_Init`. Two-tone + green-solid = healthy boot.
- `Buzzer_FixAcquired()` — three quick 2500 Hz chirps on first valid GPS fix from `gps_thread`, latched once per boot.

### Error log
`Error_Log_Pump_Tsueri_dd.mm.yyyy.log` (compile date) on the SD card.

Boot marker block written by `ErrorLog_Open()`:
1. `--- Boot v<N> May  5 2026 08:25:34 ---`
2. `fw: v<N> build … | GPS 10Hz | AUDIO=0 BATTERY=1 | flash ~150KB` (flash size computed at runtime from `_sidata`/`_sdata`/`_edata` linker symbols)
3. Reset reason decoded from `BootResetCsr` (snapshotted from `RCC->CSR` in `main()` right after `HAL_Init()`, then `__HAL_RCC_CLEAR_RESET_FLAGS()`): `POR` / `BOR` / `PIN` / `SOFTWARE` / `IWDG` / `WWDG` / `LPWR` / `OBL`, plus raw `CSR=0x...` for ambiguous combinations.
4. BLE probe outcome (`g_ble_probe_status` snapshot)
5. Gauge probe outcome
6. GPS init log (`GPS_GetInitLog()`): `gps: rate=OK(10Hz) msg=OK save=OK`

`COMMAND_START_LOG` writes a forward-looking manifest of seven `fx: about to call <op>` lines + flushes once, then writes `fx: <op> returned status=0xN` markers around each `fx_file_*` call. The last marker on disk pinpoints any hang.

`ErrorLog_Write` does **not** call `fx_media_flush` after every line — accumulates in the FileX cache. Explicit `ErrorLog_Flush()` (declared in `Core/inc/main.h`) commits when needed; per-file flushes in `START_LOG` body also commit the whole media cache.

### Flush cadence
`app_filex.c` calls `fx_media_flush(&sdio_disk)` every 100 sensor samples (~1 s @ 100 Hz) inside `COMMAND_SAVE_SENSORS`. Without this, FAT directory entries only update on `fx_file_close`, so power-off mid-session shows 0-byte files.

WAV header is **not** updated by the periodic flush — only on graceful stop. Audio data is on the card after ungraceful stop, but players may truncate.

`STOP_LOG` closes all files and SD media regardless of `AudioFileOpen` (earlier nesting inside the audio block left the card in an inconsistent state when MIC was off).

### Clock seeding
`UpdateFileXClock()` stamps FileX's date/time from a wall-clock base + `(tx_time_get() - ClockBaseTick)/100` seconds. Base starts as `__DATE__` + `__TIME__`; `gps_thread` overwrites it via `SetClockBaseFromGPS(...)` on the first valid `$GNRMC`. Files created before GPS lock get the compile date; everything after gets real UTC. Error log filename keeps the compile date (constructed once at boot before any fix). Month/year rollover past base date is not handled. Seeding event is logged: `clock: seeded from GPS YYYY-MM-DD HH:MM:SS UTC`.

## Firmware Update

**DFU is the preferred method** on the SensorTile.box PRO. BOOT0 is wired to the user button: hold button while plugging USB-C → STM32U585 enters built-in USB-DFU bootloader → `STM32CubeProgrammer` or `dfu-util` writes to `0x08000000`. Bypasses all user-code update logic, can't be bricked, no Bank-Swap-Bug. Full step-by-step in `Documentation/Flash_Firmware_Mac.html`.

Local DFU sequence (Rev_B box, two physical buttons labelled 1 and 2 where button 2 is BOOT0): slider OFF → hold button 2 → slider ON (button still held) → wait 2 s → release → `dfu-util -l` confirms `[0483:df11]`.

Stuck flash recovery (use when multiple different firmwares hang identically — symptom of stale-bank pointer):
```sh
dfu-util -d 0483:df11 -a 0 -s 0x08000000:mass-erase:force:leave -D firmware.bin
```
`mass-erase:force` wipes both banks; `:leave` reboots into the freshly-flashed image. STM32CubeProgrammer "Full Chip Erase" achieves the same.

**SD card update** (fallback): On boot, the app checks for `firmware.bin` on the SD card. If found, programs the inactive flash bank (dual-bank STM32U585), renames to `firmware.done`, swaps banks via option bytes, resets. Max ~1016 KB. Implementation in `CheckAndApplyFirmwareUpdate()` in `app_filex.c`. **Known bug**: bank-swap (`HAL_FLASHEx_OBProgram` + `HAL_FLASH_OB_Launch`) fails silently in some conditions — file gets renamed (looks successful) but chip continues booting from the old bank. Use DFU when SD-update doesn't take.

LED signals during SD update:
- 10× rapid green+red alternation (~2 s) when `firmware.bin` opened (distinct from boot blinks)
- Red toggles every ~512 B during programming
- 3× slow green blinks (~1.8 s) on success, before bank-swap/reset

Makefile builds `firmware.bin` directly: `all` target emits `build/firmware.bin` alongside `build/SDDataLogFileX.bin` via `cp` so the binary can drop onto the SD card without renaming.

## Build Versioning

Each `make` in the SDDataLogFileX `STM32CubeIDE/` directory bumps a counter in `.build_counter` (**committed** so build numbers are globally unique, not per-developer) and bakes it as `-DFW_BUILD_NUM=N` via a `$(shell …)` expression. The bump happens at *Makefile parse time* via `:=`, so any invocation — even `make clean` or a build that errors out — burns a number. Consequence: don't ask "build firmware vN" for a specific N; the next build is always one higher than whatever's in `.build_counter`. If a session's notes refer to "vN" but the binary was lost, rebuilding the same source emits "vN+1" with byte-different metadata (the version is baked into a string + the FW_INFO filename). Three uses:

1. **Filename**: `make` emits `build/firmware_v<N>.bin` alongside `build/firmware.bin` (latter still required for SD-update). Use the versioned name when sending via WhatsApp.
2. **`FW_INFO_v<N>.TXT`** at SD root: filename carries the version, so the field tester can identify the running firmware from the SD listing alone. On every boot `WriteFwInfoFile()` walks the SD root and deletes any pre-existing `FW_INFO*` entry (legacy fixed-name `FW_INFO.TXT` and any older `FW_INFO_v*.TXT`) before writing the new one. Two-pass directory iteration (`fx_directory_first_entry_find` + collect; then delete) because deletion-during-iteration invalidates the FX iterator. Up to 8 stale entries cleaned per boot.
3. **Error log boot markers** (see "Error log" section above).

`-DFW_BUILD_NUM=N` is a CLI define that `make` doesn't track as a dependency — the Makefile has a `$(BUILD_DIR)/app_filex.o: FORCE` rule to always rebuild that one .o (other .o files don't reference the macro). IDE builds (CubeIDE/IAR/Keil) get fallback `0` from `stbox1_config.h` so the field tester sees `v0` and knows it wasn't a CLI build.

## GPS module (u-blox MAX-M10S)

Wiring (SparkFun MAX-M10S breakout → SensorTile.box PRO Rev_C):
- GPS TX → PA1 (UART4 RX)
- GPS RX → PA0 (UART4 TX — required for auto-config)
- GPS 3V3 → 3V3
- GPS GND → GND

UART4 is owned by GPS, so `STBOX1_ENABLE_PRINTF` is off by default. Error log on SD remains the primary diagnostic.

### Auto-configuration on boot
`GPS_Init()` (in `Core/Src/gps_nmea.c`) configures the module each boot with full UBX-ACK verification (ACK-ACK / ACK-NAK parsing via `ubx_wait_ack()` + up to 3 retries per command in `ubx_send_retry()`, 50–100 ms spacing):

1. UBX-CFG-PRT at 9600 baud (u-blox factory default) → switch to `GPS_UART_BAUDRATE` (38400). Best-effort, **no ACK wait** (baudrate is switching mid-command).
2. Switch local UART4 to 38400.
3. UBX-CFG-RATE — set measurement rate from `GPS_MEAS_PERIOD_MS` (1000 / `GPS_RATE_HZ`, default 100 ms). **Sent first** (most important); a dropped RATE leaves the module on its persisted 1 Hz.
4. UBX-CFG-MSG × 4 — disable GLL, GSA, GSV, VTG. Only GGA + RMC remain.
5. UBX-CFG-VALSET — set CFG-NAVSPG-DYNMODEL = 5 (Sea). The Portable default suppresses Doppler velocity reporting on water. **Layers must be `0x01` (RAM only)**, not `0x07`; some MAX-M10S firmware variants reject the multi-layer form atomically. Persistence comes from the next step.
6. UBX-CFG-CFG — save all sections (BBR + Flash + EEPROM).

ACK status accumulated in static buffer, flushed to error log on first START_LOG via `GPS_GetInitLog()`. Format: `gps: rate=OK(10Hz) msg=OK save=OK`. `rate=TO/NAK(10Hz)` means the module fell back to its persisted config.

### gps_thread
Polls at module rate, not fixed 1 Hz. `gps_poll_ticks = GPS_MEAS_PERIOD_MS / 10`, so for 10 Hz the thread sleeps 10 ticks between reads.

### UART4 IRQ split
NMEA line assembly + sentence parsing (`nmea_checksum_ok`, `nmea_split`, `parse_rmc`/`parse_gga` with `atof`/`strtod`) runs in `gps_thread`, **not** in `HAL_UART_RxCpltCallback`. The IRQ only pushes incoming bytes into a 1 KB ring buffer (`RxRing[]`); `gps_thread_entry` calls `GPS_Process()` once per poll cycle to drain and parse at thread priority 11. UART IRQ completes in <10 µs/byte regardless of GPS rate.

Earlier in-IRQ parsing at NVIC priority 6 preempted SDMMC1: at 10 Hz GPS the SDMMC budget blew out after ~90 s, MessageQueue filled, sample rate collapsed to ~7.7 Hz (Δtick = 13 in CSV).

The firmware parses only `$GNRMC` and `$GNGGA`. With only those two enabled, 10 Hz fits in ~1.5 kB/s on the 38400-baud UART (ceiling ~3.8 kB/s). 25 Hz max for single-GNSS; raising the baud breaks auto-config on already-persisted boards.

ST BLE Sensor app uses a slightly different CSV format (date/time columns instead of raw tick timestamp).

## BLE FileSync — download SD-card files over Bluetooth

When `STBOX1_ENABLE_BLE_SYNC=1`, the SDDataLogFileX firmware advertises as `PumpTsueri` with PIN-secure pairing (PIN `123456`). Two characteristics under the BlueST features service (`00000000-0001-11e1-9ab4-0002a5d5c51b`):

| Characteristic | UUID | Properties |
|---|---|---|
| FileCmd  | `00000080-0010-11e1-ac36-0002a5d5c51b` | write w/o response |
| FileData | `00000040-0010-11e1-ac36-0002a5d5c51b` | notify |

Opcodes (one byte + optional payload — payload is filename without trailing NUL):

| Opcode | Meaning | FileData reply |
|---|---|---|
| `0x01` LIST | enumerate SD root | `name,size\n` rows + single `\n` terminator |
| `0x02` READ `<name>` | stream file body | raw bytes; total length matches LIST size |
| `0x03` DELETE `<name>` | drop file | single status byte |
| `0x04` STOP_LOG | gracefully close active session | no reply (host re-checks via LIST) |

Status bytes for READ/DELETE: `0x00` OK, `0xB0` BUSY (logging in progress), `0xE1` NOT_FOUND, `0xE2` IO_ERROR, `0xE3` BAD_REQUEST. READ/DELETE rejected with `BUSY` while a `Sens*.csv` or `Gps*.csv` is open for writing — host calls STOP_LOG first.

### Firmware-side architecture
- `Core/Src/ble_sync.c` — owns one ThreadX thread (priority 14, below FileX writer at 12 and GPS at 11 so SD bandwidth always wins). Loops `if (hci_event) { hci_event=0; hci_user_evt_proc(); } BleFileSync_Tick(); tx_thread_sleep(1);`.
- `Core/Src/ble_filesync.c` — FileCmd/FileData characteristics + `CurrentOp` state machine. On notify congestion (`aci_gatt_srv_notify` returns INSUFFICIENT_RESOURCES) we don't drop bytes — LIST stays in `ST_LIST_EMIT`, READ rewinds with `fx_file_relative_seek(SEEK_BACK)`, both retry on next Tick. Exports `BleFileSync_Reset()` (v255+) called from `disconnection_completed_function` so a host that drops mid-LIST or mid-READ doesn't leave `state != ST_IDLE` — without the reset, the next reconnect's OP_LIST gets silently swallowed by the `state != ST_IDLE && opcode != STOP_LOG` guard in `write_request_filecmd` and the GUI shows an empty file list with no error.
- `Core/Src/ble_implementation.c`, `ble_function.c` — minimal X-CUBE-BLEMGR glue. `ble_function.c::disconnection_completed_function` calls `BleFileSync_Reset()` after clearing `connected`/`paired` so per-host state is fresh for the next connection.
- `Core/Src/ble_spi.c`, `hci_tl_interface.c` — isolated SPI1 driver bypassing the shared `Drivers/BSP/SensorTileBoxPro/` BSP (the BLEDualProgram BSP and SDDataLogFileX BSP both export the same `BSP_*` symbols, so they can't coexist).
- `Core/inc/RTE_Components.h`, `ble_manager_conf.h`, `stm32wb07_06_conf.h`, `ble_list_utils.h` — config headers for X-CUBE-BLEMGR + STM32WB07_06.
- `FileX/App/app_filex.c` — public hooks: `Ble_RequestStopLog()` posts `COMMAND_STOP_LOG` into `MessageQueue` via a dedicated static `MessageData_t`; `Ble_IsLoggingActive()` returns whether `SensorsFileOpen || GpsFileOpen`.

For ad-hoc poking, any generic GATT client works — write `01` to FileCmd, watch FileData stream. Most ergonomic: MovementLogger GUI's BLE FileSync panel.

User-facing explainer at `Documentation/BLE_FileSync.{html,pdf}`. Render: `"/Applications/Google Chrome.app/Contents/MacOS/Google Chrome" --headless --disable-gpu --no-pdf-header-footer --print-to-pdf=BLE_FileSync.pdf "file://$PWD/BLE_FileSync.html"`. Send via `~/software/pegelstand/whatsapp/send-doc.mjs`.

## USB CDC ACM debug console

When `STBOX1_ENABLE_USB_CDC=1` (set via Makefile flag `USB_CDC_ENABLED=1`), SDDataLogFileX brings up the STM32U585 OTG_FS peripheral as a USB CDC ACM virtual COM port and routes `printf` / `STBOX1_PRINTF` through bulk-IN endpoint 0x82. Vendored TinyUSB stack at `Middlewares/Third_Party/tinyusb/`. Glue at `Core/Src/usb_cdc.c`, `Core/Src/usb_descriptors.c`, `Core/inc/{tusb_config.h,usb_cdc.h}`. Service thread spawned from `App_ThreadX_Init`, priority 13.

### Critical hardware setup quirks discovered the hard way

All in `usb_cdc.c::usb_cdc_hw_init()`:

- **`__HAL_RCC_PWR_CLK_ENABLE()` BEFORE `HAL_PWREx_EnableVddUSB()`** — `HAL_PWREx_EnableVddUSB` writes `PWR->SVMCR.USV`, but the write silently does nothing if the PWR peripheral clock is gated. Reference: NFC_FTM's `HAL_PCD_MspInit` does the same dance.
- **CRS auto-trim of HSI48 from USB SOF** — `__HAL_RCC_CRS_CLK_ENABLE()` + `HAL_RCCEx_CRSConfig` with `RCC_CRS_SYNC_SOURCE_USB`. HSI48 alone is ±1-2 % out of reset, borderline for FS line-rate spec.
- **`USB_OTG_FS->GUSBCFG |= USB_OTG_GUSBCFG_PHYSEL` after the OTG_FS RCC clock is enabled, before `tud_rhport_init`** — STM32U585 needs PHYSEL set to select the FS embedded PHY. ST's `HAL_PCD_Init` does this in `USB_CoreInit` (`stm32u5xx_ll_usb.c`); TinyUSB's `dwc2_phy_init` for STM32U5 does NOT. Without PHYSEL the data lines stay floating, `tud_rhport_init` returns true, but **zero OTG_FS interrupts ever fire**. This was the symptom that took the longest to diagnose; once PHYSEL was set the chip enumerated immediately.
- **`RCC_PERIPHCLK_ICLK` + `IclkClockSelection = RCC_ICLK_CLKSOURCE_HSI48`**, not `RCC_PERIPHCLK_USB` (doesn't exist on U5) or `RCC_PERIPHCLK_CLK48` (legacy alias). On STM32U5 the USB / SDMMC / RNG share one ICLK selector.

### LED conventions during USB bringup

- **1 long green flash (~600 ms)** between the 3 short boot blinks and solid green = `tud_rhport_init` returned OK.
- **7 quick red bursts** = `tud_rhport_init` failed (Synopsys-ID register read failed, almost always missing clock/power above).
- **Red LED ON solid** while green is solid = `tud_mounted()` is true (host enumerated). Goes off on disconnect.
- **Red LED slow blink** (250 ms on / 750 ms off) while green is solid = OTG_FS interrupts firing but `tud_mounted()` is still false → host sees us but enumeration stalled.

### Diagnostic globals

- `g_otg_fs_irq_count` (volatile uint32_t in `Core/Src/stm32u5xx_it.c`) — incremented on every OTG_FS_IRQHandler entry. Lets you tell whether the chip is even on the host bus.
- `g_usb_diag_lines[4][200]` + `g_usb_diag_seq` — in-memory ring of OTG_FS register snapshots (gotgctl, gccfg, dctl, dcfg, gintsts, dsts, mounted) for SWD/JTAG inspection. Cannot write from USB thread to the FileX-owned error log because that races fx_thread.

### Heartbeat

When `tud_mounted()` goes true, the USB thread emits a 1 Hz `printf` of:
```
hb t=12345 otg_irq=87 ble=0xF2 conn=1 mounted=1
```
Where `t=` is the ThreadX tick count, `otg_irq=` total OTG_FS interrupts, `ble=` is `g_ble_probe_status` (BLE bringup phase — see `ble_sync.c`), `conn`/`mounted` are `tud_connected()` / `tud_mounted()`. With this stream you can watch BLE init progress live without ever touching the SD card.

### macOS Sequoia 15+ blocker

Sequoia's kernel CDC ACM driver (`AppleUSBCDCCompositeDevice`) **partial-attaches** in `IOMatchDefer = Yes` state and never registers — `/dev/tty.usbmodem*` is never created, AND libusb (rusb / pyusb) can't claim the bulk endpoint either, even as root. Tried CDC class with IAD, pure CDC class, vendor-specific class — all hit the same wall (vendor-class is silently blocked by the accessory authorization framework with no prompt). Apparently a regression in the DriverKit-era USB stack; affects TinyUSB-based devices broadly per the TinyUSB issue tracker.

**Workaround: use Linux for the live debug viewer.**

```sh
# On Linux, after plugging the box in:
dmesg | tail              # confirm cdc_acm attached
sudo cat /dev/ttyACM0     # streams the heartbeat live
# (drop sudo with a udev rule that gives the dialout group access to 0xCAFE/0x4001)
```

For Mac users without a Linux box, two leftover host-side helpers are checked in:
- `Utilities/usb_console.py` — pyusb script run as `sudo uv run usb_console.py`. Hits "Other error" on macOS Sequoia regardless. Works on Linux.
- `Utilities/rust/usb-console/` — the Rust port (`rusb`-based). Same behavior — works on Linux, blocked on Sequoia. Build with `cargo build --release -p usb-console`. Run as `sudo ./target/release/usb-console`.

### Software-triggered DFU bootloader (no button dance)

When USB CDC is on, writing `DFU\n` to `/dev/ttyACM0` reboots the box into the STM32 system bootloader so the next `dfu-util` flash needs no BOOT0+slider gymnastics. The full Linux iteration loop:

```sh
make USB_CDC_ENABLED=1
echo DFU > /dev/ttyACM0                       # firmware acks "DFU: entering bootloader"
sleep 1
sudo dfu-util -d 0483:df11 -a 0 -s 0x08000000:mass-erase:force:leave -D build/firmware.bin
# heartbeat resumes on /dev/ttyACM0 — ready to iterate again
```

Implementation: **soft-reset + magic-value pattern**, NOT a direct jump from running firmware. Direct-jump (HAL_DeInit → set MSP → branch to `0x0BF90000`) leaks too much state on STM32U5 — OTG_FS dirty registers + threadx pending ticks make the system bootloader silently fail to bring up USB DFU (verified empirically: ack prints, chip dies, no `0483:df11` ever appears). Instead:

1. `usb_cdc.c::jump_to_bootloader()` writes `0xDEADBEEF` to `TAMP->BKP0R` (survives system reset), then calls `NVIC_SystemReset()`.
2. `main.c` checks `TAMP->BKP0R` *before* `HAL_Init()` — needs only PWR clock + DBP bit + RTCAPB clock to read TAMP. If magic is set, it clears it and chains directly into the bootloader (`SCB->VTOR = 0x0BF90000; __set_MSP(*0x0BF90000); jump *(0x0BF90004)`). Bootloader sees a clean post-reset chip.

Address `0x0BF90000` is the STM32U5 system memory entry per AN2606 §52.

The DFU listener parses lines of up to 15 bytes terminated by `\r` or `\n`. Currently the only command is `DFU`; adding more would just extend `usb_handle_command()`.

### Linux dialout group (one-time setup)

`/dev/ttyACM0` is owned by group `dialout`. On Debian/Ubuntu the user is not in that group by default — without it, every `cat`/`echo`/`stty` needs `sudo`. One-time fix:

```sh
sudo usermod -aG dialout $USER
# log out + log back in for the group change to apply (newgrp dialout works in a fresh sub-shell only)
```

After that the iteration loop is sudo-free *except* for `dfu-util` itself, since libusb's `uaccess` udev tag (`/lib/udev/rules.d/*-libusb.rules` on most distros — `ATTRS{idVendor}=="0483", ATTRS{idProduct}=="df11", TAG+="uaccess"`) only applies to interactive logind sessions. `dfu-util` from a non-tty bash (`!`-prefix in Claude Code, etc.) hits `LIBUSB_ERROR_ACCESS` and needs an interactive shell.

### SD-card auto-update interaction

The same firmware build that runs the DFU loop also auto-applies `firmware.bin` from the SD root on boot via `CheckAndApplyFirmwareUpdate()` (see "Firmware Update" section). This means: if a stale `firmware.bin` is on the SD card from earlier testing, every successful DFU flash gets *immediately overwritten* by the SD-update path — flash → boot → SD-update programs the inactive bank with the OLD file → bank-swap → boot OLD firmware → DFU listener gone → loop broken. **Delete or rename `firmware.bin` on the SD card once before starting the DFU iteration session.** The SD-update field-deployment path stays intact for production.

### Build invocations

```sh
make                            # default: USB off, BLE off (byte-identical to pre-USB-CDC build)
make USB_CDC_ENABLED=1          # USB on, BLE off
# Edit stbox1_config.h: STBOX1_ENABLE_BLE_SYNC 1
make USB_CDC_ENABLED=1          # both on (issue #12 live-debug build)
```

Default-off build is byte-identical to a pre-USB build because DCE drops the entire integration when the flag is off.

### BLE-on kills USB enumeration (active issue #12 lead — bisected)

With `STBOX1_ENABLE_BLE_SYNC=1` the host never enumerates the box. Initial `dmesg` showed `device descriptor read/64, error -110` retries — looked like the BLE thread was racing host enum and killing it early. **Important correction:** the right diagnostic is to wait *much* longer. With BLE thread running, USB enum is delayed (typically 15-20 s on the dev box) but still completes if the thread eventually parks. Earlier "USB dead at 10 s" verdicts were premature.

**Bisected killer (2026-05-09):** `Core/Src/hci_tl_interface.c::hci_tl_spi_send`. Walking a `for(;;) tx_thread_sleep(...)` park forward through `ble_chip_alive_probe` shows USB enumerates (delayed) at every park *before* `hci_tl_spi_send` and stays dead for 60+ seconds at every park *after* it. Ruled out: thread spawn / byte-pool allocation / 3 GPIO inits in `hci_tl_spi_init` / `BleSpi_Init` / `hci_tl_spi_reset`. Suspects (not yet validated):

1. **Board-level EMI** — SPI1 lines (PA5/6/7) physically near USB DM/DP (PA11/12) on Rev_C; SPI bursts may inject noise during enum. Not fixable in firmware.
2. **BlueNRG-LP brown-out** — chip wakes from RST and draws inrush current; if VddUSB rail dips, USB transceiver glitches. Could be fixed by adding a settle delay before `hci_tl_spi_send` and/or skipping probe if `is_data_available()` is false at entry.
3. **EXTI11 disable/enable side-effect** — `hci_tl_spi_send` does `HAL_NVIC_DisableIRQ(EXTI11_IRQn)` for the duration. Doesn't directly affect USB, but the do-while loop busy-waits up to ~1 s (`TIMEOUT_DURATION` 15 ms × N + `TIMEOUT_IRQ_HIGH` 1 s) when BlueNRG-LP is unresponsive — long enough to disrupt host-side enum retries.

The earlier "HAL_RCCEx_PeriphCLKConfig clobbers ICLK" hypothesis was wrong; with that call already removed from `ble_spi_msp_init`, USB still dies at `hci_tl_spi_send`. The fix in `ble_spi.c` is harmless either way (no clock selector clash) but is not the BLE-on USB-kill fix.

Diagnostic prints in `ble_sync.c` and `hci_tl_interface.c` (e.g. `printf("probe: post-hci_tl_spi_init OK\r\n")`) are left in tree to make future bisects faster — they're stripped by DCE when BLE is off. The same trace mirrors to the SD error log via `ErrorLog_Write`/`ErrorLog_Flush` so a non-USB-visible boot still leaves a breadcrumb.

## BlueNRG-LP OTP / WLC

The SensorTile.box PRO has two MCUs: **STM32U585** (host, runs SDDataLogFileX) and **BlueNRG-LP** (BLE controller, talked to via SPI1 + EXTI11). The BlueNRG-LP needs an OTP-burned stack image (`dtm.bin`, ~200 KB) before it can answer HCI commands. The chip is normally factory-pre-programmed; the manual recovery procedure documented in `README.md` ("First-time BlueNRG-LP OTP setup") asks the user to DFU `SensorTile.boxPRO.bin` + put `dtm.bin` on SD, but the WLC machinery in that ST binary appears unreachable from a Ghidra xref scan, so it likely doesn't actually program OTP — chips just ship pre-programmed. **There is no validated in-firmware OTP-programming path.**

`Core/Src/wlc.c` is a Ghidra-reverse-engineered port of `FUN_08068f50` (the WLC main entry) wired behind `STBOX1_ENABLE_WLC` (default `0`, no call site). Untested, kept for reference. The flag-off compile is byte-identical to a build without the file because the linker DCEs everything.

## Visualization (`stbox-viz`)

All visualisation by the `stbox-viz` Rust crate at `Utilities/rust/stbox-viz/`. Single binary, no Python runtime. Build: `cd Utilities/rust/stbox-viz && cargo build --release`.

| Subcommand | Output | Input |
|---|---|---|
| `combined` | interactive Plotly HTML (map + nose angle + baro height + speed) | SensNNN.csv + auto-detected GpsNNN.csv |
| `sensors` | 5-panel sensor summary PNG + quaternion/Euler PNG | SensNNN.csv |
| `pumpfoil` | pump-cadence spectrogram PNG + movement-phase PNG | SensNNN.csv |
| `animate` | animated GIF of board orientation per session, optional combined MOV | SensNNN.csv + optional camera video |
| `compass` | magnetometer-vs-GPS heading residual PNG | SensNNN.csv + GpsNNN.csv |

Crate layout: `io.rs`, `fusion.rs` (Madgwick 6DOF), `euler.rs`, `session.rs` (pitch-oscillation detection), `gps.rs` (haversine + ride detection), `baro.rs` (TC + GPS-anchored water reference), `butter.rs` (4th-order Butterworth + filtfilt), `spectrogram.rs` (scipy-equivalent STFT via `rustfft`), `html.rs` (Plotly JSON emission), `plot_common.rs` (shared `plotters` helpers), one `*_cmd.rs` per subcommand.

GPS auto-detection (used by `combined` and `animate`) accepts two naming forms next to the sensor CSV: firmware on-card layout `SensNNN.csv` ↔ `GpsNNN.csv` (preferred — works directly from a mounted SD card), and legacy `<stem>.csv` ↔ `<stem>_gps.csv`. See `guess_gps_path` in `main.rs` and `animate_cmd.rs`.

User-facing fusion explainer at `Documentation/Sensor_Fusion.{html,pdf}` — render with the same headless-Chrome command pattern as `BLE_FileSync.{html,pdf}`.

Project narrative / external-audience write-up at `Documentation/Odyssey.{html,pdf}` — short field-notes article for [medevel.com](https://medevel.com/) covering the 3.3 V mod, Issue #12, EXTI11 weak-symbol infinite loop, BLE advertising-name truncation, macOS Sequoia USB CDC blocker, and the Rust visualizer + GUI. Refresh whenever a new long-running bug story closes; matches the A4 CSS style of the other Documentation/*.html files. Same render command as the other PDFs.

### `combined` HTML
Time axis: default UTC anchored to GPS clock. `--tz-offset-h <h>` shifts to local (`3` for EEST, `2` for CEST, `1` for CET). Date defaults to sensor file mtime; override with `--date YYYY-MM-DD`.

Per-recording HTML (saved to `html/`) with:
- **Plotly Scattermap** (`carto-positron` tiles), one trace per detected ride, coloured by GPS speed (Viridis 0–25 km/h). One ride visible at a time.
- **Board nose angle** time-series (drift-corrected via 1 s + 60 s rolling medians on the rotated sensor Y-axis elevation).
- **Board height above water** from LPS22DF baro via `height_above_water_m()`, overlaid with GPS altitude (orange dots, zeroed at median over stationary samples). Two-stage baro: (1) temperature-compensate; (2) use GPS speed < 3 km/h as water reference, linear-interp pressure across flying segments, hypsometric `8434 × (1 − P/P_ref)`. Display: 250 ms rolling-mean → 100 ms display buckets. Y-clamp `[-0.1, 0.9] m` (mast = 80 cm); values outside `[-0.15, 0.95] m` → NaN so thermal drift shows as gaps.
- **Speed**, **position-derived** (haversine; module's Doppler unreliable on this board). Two-stage filter: reject `|Δspeed|/Δt > 15 km/h/s` & `v > 15 km/h`, then linear-interp + 5 s rolling median, y pinned 0–30 km/h.

Cross-panel hover (`hovermode: "x unified"` + `xaxis.showspikes`); click pins a dashed red numbered line across all panels. "Clear click-marks" button resets. Ride buttons restyle visible trace + relayout x-range + recentre map.

Ride detection: sustained GPS movement >3 km/h for ≥10 s, merge gaps <30 s, pad ±3 s.

GPS loader does **not** decimate. Sensor series at 100 Hz binned to 100 ms display buckets via `BTreeMap` keyed by bucket index (so non-monotonic input can't double-emit). Plotly.js CDN-linked.

### `animate`
Per-session GIFs of board orientation. **Five panels with GPS** (board side view + pump detail + height-over-water + speed + nose angle), three otherwise.

Nose angle: 4th-order Butterworth 0.7 Hz low-pass + filtfilt + 10 s rolling median baseline.

`--at HH:MM[:SS]` mode: bypasses session detection, renders one GIF for an exact wall-clock slice. Pair with `--tz-offset-h` and `--date`. Window length is `--duration <s>` (defaults to video length when `--video` set). The video's `creation_time` is probed for diagnostics.

GPS-tick anchored slicing on **both ends**: anchor by finding the GPS row closest to requested wall-clock UTC and use its tick directly. ThreadX runs on HSI (±1 % drift, ~7 s over 21 min) — extrapolating from first GPS sample slices the wrong sensor range.

Float-step frame generation + GIF-spec delay rounding so encoded GIF rate matches data step. Pre-round `delay_ms` to a 10 ms multiple, derive `effective_fps = 1000 / delay_ms`, use that for `step_f`.

`--video FILE` shells out to `ffmpeg`/`ffprobe` for `hstack=inputs=2` MOV with optional title-card concat (`--title`, `--subtitle`). `ffmpeg` must be in PATH.

Phase detection in `--at` mode: gray Tragen / yellow Anschieben/Rennen / green Foilen. Push-off = first sample where 1 s rolling raw position-derived speed > 4 km/h sustainedly for 0.5 s. Foilen = first index where 3 s mean baro height > 0.15 m. Push-off-Winkel flash for 2 s at push-off shows steepest negative pitch in ±1 s window.

Hybrid height construction with `--dock-height-m <h>` (e.g. 0.75 for Ermioni harbour wall): dock phase plotted as flat constant, water transition crosses 0 m via 2 s linear ramp, foiling phase uses baro re-anchored to pressure averaged ±0.5 s around push-off. Foiling height clamped to `[-0.1, 0.9] m`; samples with 10 ms pressure delta > 1 hPa NaN'd.

Per-frame dynamic scales — y-axis grows with running max. Nose-angle panel uses 95th-percentile-based y-limit. Scrolling sinusoidal water surface at 0.6 m/s (board lift uses 0.5 s rolling-mean baro height).

Per-cursor value labels (small red dot + text) in each time-series panel showing live value at the current frame. Panel labels horizontal in their own 40 px title strip.

#### 3D foil mesh (`--board-stl <FILE>`, canonical: `/Users/zdavatz/software/fingerfoil/stl/0_combined.stl`)
Pure software rasterizer in `board3d.rs` (no GPU, no winit dep). Loads STL once, per-frame projects through fixed camera into 600×400 RGBA buffer.

`--mount mast|deck` (default `mast`) picks three things together:
- `R_mount`: `(-z, x, y)` for mast (IMU+X = -board+Z mast-down; IMU+Y = +board+X nose; IMU+Z = +board+Y port), identity for deck.
- Camera eye: port-side `(0, 3.2, 0.5)` for mast, behind-the-tail `(-3, 0, 0.7)` for deck.
- Body-to-world quaternion source for the 3D mesh: Madgwick `quat_strip_yaw(quat_conj(q))` for mast; **accel-only tilt** (0.25 s rolling-mean smoothing → atan2 pitch+roll → no yaw) with 180°-X pre-flip for deck. Mast-mount also recovers carry-phase tail-place pose because Madgwick integrates gyro continuously.

3D mesh **hidden during carry phase**, only appears once `water_set_t` reached (sustained GPS speed > 4 km/h) OR 10.5 s elapsed. Carry shows centred "Tragen — keine 3D-Daten" placeholder over the scrolling water. Carry-phase nose-angle + pump-counter overlays also suppressed; `Zeit:` clock stays on, hundredths resolution (`M:SS.HH`).

Rationale for accel-only tilt on deck: LIS2MDL is unusable for yaw (iron distortion), GPS course unreliable below 3 km/h, gyro bias around body-Z survives `quat_strip_yaw` (which only kills world-Z). Accel-only fails on centripetal turns and brief pump impulses, but 0.25 s smoothing tames both. Madgwick is still used for nose-angle pump trace and push-off-Winkel flash.

Deck-mount weakness: a confirmed wipeout cut sensor + GPS log writes off in the same tick — brown-out from water entering the box. Mast-mount sessions don't have this exposure.

Without `--at`, session detection is **pitch-oscillation based** (≥0.3 Hz over ≥30 s, merge <60 s gaps). Smooth-flight data without clear pitch oscillation won't register — use `combined` (GPS-based) for those.

### `sensors`
5-panel summary: accel/gyro/mag XYZ + temperature + pressure, min:sek x-axis. Quaternion/Euler plot with red vspans where |pitch| > 85° (gimbal-lock artefact). Sub-second runtime for 23-min session at 100 Hz. SD format only (BLE-app format loader not ported).

### `pumpfoil`
Two PNGs: `plot_pump_cadence.png` (dynamic accel + envelope, 4 s / 90 % STFT 0.3–5 Hz, dominant-frequency scatter, median cadence reference line); `plot_pump_phases.png` (gyro-RMS rest/active/crash colouring at 60th/95th percentile thresholds, per-axis gyro, |acc|, inverted-pressure).

### `compass`
Tilt-compensates body-frame mag via Honeywell AN203, computes heading vs GPS course. Output: `png/plot_compass_<stem>.png`, two stacked panels (heading overlaid + residual). Console prints median residual (should match local declination: ~+4° E in Greece, ~+2.5° E in Zürich) and p5–p95 span. Real-board first run: median −14°, span 307° — essentially random, ferromagnetic distortion dominates. Fix is hardware (plastic screws / sensor placement away from iron).

## MovementLogger GUI

Cross-platform (Win/Mac/Linux) drag-and-drop wrapper around `stbox-viz animate`. Source at `Utilities/rust/stbox-viz-gui/`, binary called `MovementLogger`. Drop a sensor CSV (auto-pairs `_gps.csv`), optional camera `.mov`/`.mp4`, optional board `.stl`; fill in `--at`, `--tz-offset-h`, `--mount`, etc.; click Generate. Shells out to bundled `stbox-viz` CLI (looked up next to `current_exe()` first, then PATH); stdout/stderr stream into the live log panel.

Workspace at `Utilities/rust/Cargo.toml` lists `stbox-viz` and `stbox-viz-gui`. Vendored `Utilities/rust/winit-patched/` is `exclude`d (ships its own workspace) and wired via `[patch.crates-io] winit = { path = "winit-patched" }`.

**Mac App Store winit patch is required** (see workspace `CLAUDE.md` in `~/software/CLAUDE.md`). eframe → winit 0.30 calls `_CGSSetWindowBackgroundBlurRadius` (private CoreGraphics) which Apple's binary scanner rejects. Fork patch replaces `Window::set_blur` with no-op, comments out the now-unused `NSInteger`/`AnyObject` imports in `ffi.rs` so `RUSTFLAGS=-Dwarnings` doesn't trip. Verify: `nm target/release/MovementLogger | grep CGSSetWindowBackgroundBlur` returns nothing. **eframe must stay on 0.29+** — 0.28 drags in winit 0.29 alongside 0.30, the patch only matches 0.30 path.

### Window-layout conventions (apply to all egui apps)
- Version goes in the **OS window title only** via `ViewportBuilder::with_title(format!("AppName {}", env!("CARGO_PKG_VERSION")))` and the same `eframe::run_native` app id. **Don't** also `ui.heading("AppName 1.2.3")` — duplicates OS chrome.
- Short tagline inside is fine if it adds context the title doesn't. Use `ui.hyperlink_to(tagline, repo_url)`.
- Top-right of title strip: app logo as frameless `egui::ImageButton` opening `mailto:<support>` via `ctx.open_url(OpenUrl::new_tab("mailto:..."))`. PNG baked with `include_bytes!`, decoded once with `image::load_from_memory` (default-features off, only `png`), lazy-uploaded to `ctx.load_texture` on first frame. Reuse PNG bytes in `egui::IconData` passed to `ViewportBuilder::with_icon`.

### BLE FileSync panel (v0.1.4+)
Collapsible section. Workflow: Scan (5 s) → click PumpTsueri → Connect (OS pops Bluetooth permission + PIN dialog `123456`) → Refresh file list → tick rows → Download selected. Files saved to `csv/` (configurable). `Sens*.csv` and `*_gps.csv` auto-route into form's Sensor / GPS slots.

Backend in `src/ble.rs` uses `btleplug` (CoreBluetooth/BlueZ/WinRT) on a tokio current-thread runtime on a single dedicated worker thread. `std::sync::mpsc` channels shuttle commands and events to/from egui. **One notification stream per connection, not per op** — opened on Connect, demuxed inside `tokio::select!` between command channel and stream. Per-op streams risk losing the first packet if box notifies before `await` is parked. 200 ms watchdog tick surfaces stuck transfers.

Status-byte detection on READ: only treat single-byte first notify as error when byte ∈ `{0xB0, 0xE1, 0xE2, 0xE3}` (`is_status_byte`). 1-byte CSV/log files stream correctly because no legitimate file ever starts with one of those high-range bytes.

Disconnect mid-op: tearing down peripheral surfaces explicit error like `READ Sens005.csv aborted by disconnect at 1234/5678 B`, sets state to Idle.

macOS: bundled `.app` carries `NSBluetoothAlwaysUsageDescription` (`assets/Info.plist.template`) and App-Sandbox build adds `com.apple.security.device.bluetooth` (`entitlements-appstore.plist`). Bare `cargo run` on a fresh user account may not trigger consent prompt; install the `.app`.

**Sensor / Debug grouping + serial queue (v0.1.7+, 2026-05-11).** LIST rows split into two headed groups in `render_file_group`: **Sensor** (Sens*.csv, Gps*.csv, Bat*.csv, Mic*.wav) and **Debug** (everything else — FW_INFO, CHK, error log, macOS `._*` AppleDouble, 0-byte phantoms like `PUMPTSUE.RI`). Only Sensor rows default-ticked. Each header has a `Tick all` / `Untick all` toggle that flips its group's checkboxes in one click.

"Download selected" no longer blasts all reads at the worker in one shot. The original blast-mode hit a guard in `WorkerState::read()` (`!matches!(self.op, CurrentOp::Idle)`) — first read claimed the slot, every subsequent one returned `another op is in flight — wait or Disconnect`. Now `ble_dl_queue: VecDeque<(String, u64)>` queues selected files; `advance_download_queue()` pops the head on each `ReadDone` or `READ`-side error event (NOT_FOUND, BUSY, IO_ERROR, BAD_REQUEST, timeout, disconnect mid-op). The "another op in flight" message is filtered out as a non-queue-advancing error so a stale-state click doesn't skip the next file. Button label shows `Download selected (N queued)` and disables while in flight.

**Per-row progress bar.** `BleFile { bytes_done }` advanced by `BleEvent::ReadProgress`. Emit threshold lowered from 16 KB to 4 KB in `ble.rs` so the bar updates every 1–4 s at the observed BLE FileSync throughput (~1–3 KB/s) instead of every 5–16 s. Bar replaced by a green ✓ on completion.

**Trash button per row** wires `OP_DELETE` (0x03). Disabled while that row's Read is in flight. Single-byte status reply parsed in `CurrentOp::Deleting`: `0x00` → emit `DeleteDone` (row removed from list); `0xB0/0xE1/0xE2/0xE3` → surface as `DELETE <name>: <reason>` in the log. Delete is dispatched outside the per-row loop via a `delete_target: Option<String>` since the row closure already holds `&mut self.ble_files`.

**Log panel selectable + adaptive.** The `.interactive(false)` flag was removed from the `TextEdit::multiline` — text is now selectable (Cmd-A / Cmd-C). Any typing lands in a local string discarded on the next frame, so the buffer is effectively read-only without blocking mouse selection. `desired_rows` computed from `ui.available_height() / row_height` so the log grows with the window; a `Copy all` button in the header strip dumps the whole buffer to the clipboard via `ui.output_mut(|o| o.copied_text = ...)`.

**Graceful disconnect on window close.** `eframe::App::on_exit` sends `BleCmd::Disconnect` and sleeps 250 ms so btleplug emits `LL_TERMINATE_IND` to the box before the process exits. Without this, a quit (Cmd-Q / red traffic light) leaves the connection alive until the host stack's supervision timeout (~10–30 s on macOS) — and during that window a fresh GUI can't connect because the firmware is still advertising as non-connectable. A hard `pkill -9` skips this path; only fix in that case is box reboot. The Disconnect button additionally flips GUI state to Idle optimistically (without waiting for the worker's Disconnected event) so Scan is clickable while btleplug is still tearing the link down — `p.disconnect().await` on CoreBluetooth can take several seconds.

**Auto-LIST on every Connected event.** The `BleEvent::Connected` handler clears `ble_files` and sends `BleCmd::List` automatically. Without this, a Disconnect → Connect cycle (or a fresh Connect) showed "No file list yet — hit Refresh." which looked like a transient state; users missed that they had to click Refresh manually. Paired with the firmware-side state reset on disconnect (v255+) this gives a one-click reconnect that actually populates the list.

**In-app updater (v0.1.9 macOS-only; v0.1.10 cross-platform).** Originally ported from `~/.software/old2new/create_shorts_gui`; v0.1.10 extended to Linux + Windows so the same one-click "download, swap, restart" works everywhere (Firefox model).

On startup `spawn_update_check()` hits `GET https://api.github.com/repos/zdavatz/fp-sns-stbox1/releases?per_page=30` (15 s timeout, `rustls-tls` so no OpenSSL/Schannel dep), filters non-prereleases with tag prefix `v`, picks the highest version strictly greater than `env!("CARGO_PKG_VERSION")`, and pulls out the asset whose name matches `target_asset_suffix()` (compile-time `cfg!` ladder → `-macos-aarch64.dmg` / `-x86_64-unknown-linux-gnu.tar.gz` / `-x86_64-pc-windows-msvc.zip`). A blue banner at the top of the central panel surfaces the result. "Check for updates" button next to "Clear log" re-triggers manually.

Per-platform install paths in `installer.rs`:

- **macOS** (`install_macos`): download DMG to `$TMPDIR/movement_logger_update_<pid>` → `hdiutil attach -nobrowse -readonly` → `codesign --verify --deep --strict` the inner `.app` → `ditto` to sibling staging `.MovementLogger.app.new.<pid>` → bash helper waits for PID, `mv` swap, `open` relaunch.
- **Linux** (`install_linux`): download tar.gz → shell out to `tar -xzf` (always available) → `find_recursive` for `MovementLogger` binary inside the stem folder → stage to `.MovementLogger.new.<pid>` next to the running exe with `chmod 0o755` → bash helper waits, `mv` swap both `MovementLogger` and sibling `stbox-viz`, `setsid -f` relaunches detached.
- **Windows** (`install_windows`): download zip → PowerShell `Expand-Archive -Force` (built-in since Win10) → `find_recursive` for `MovementLogger.exe` → stage to `MovementLogger.exe.new.<pid>` → PowerShell helper polls `Get-Process -Id <pid>`, then `Move-Item` swap (Windows allows renaming a running .exe — the open handle keeps the old name alive, just can't delete it directly), `Remove-Item` the `.old`, `Start-Process` relaunch.

Helper script is spawned detached in every case; the install thread sleeps 600 ms then `process::exit(0)` so the helper's `kill -0`/`Get-Process` loop unblocks. `installer::can_in_app_update()` gates the UI: target asset suffix is non-empty AND (macOS → `current_app_bundle()` succeeds; Linux/Windows → `current_exe()` succeeds). Unsupported targets (aarch64 Linux CLI-only, BSD, etc.) fall through to "Open release page".

`reqwest` is the only added dep — `default-features = false` + `features = ["blocking", "json", "rustls-tls"]` keeps the build self-contained. `serde` (with derive) for the GitHub API JSON. No `crossbeam-channel` — `std::sync::mpsc` matches the existing BLE-events pattern.

### Releases
GitHub Actions workflow at `.github/workflows/release.yml`. Tag `vX.Y.Z` and push — runs the per-platform matrix (Linux x86_64 + aarch64 CLI-only, macOS Apple Silicon, Windows x86_64), packages each as `MovementLogger-vX.Y.Z-<target>.tar.gz`/`.zip` with SHA256, attaches to GitHub Release. macOS path also assembles a `MovementLogger.app` (with `stbox-viz` shipped inside `Contents/MacOS/` for the GUI's `current_exe()`-relative lookup) and a notarized DMG named `MovementLogger-vX.Y.Z-macos-aarch64.dmg`.

**Intel Mac dropped after v0.1.5** — every Mac since late 2020 is Apple Silicon, and `macos-13` Actions runner queue was wedging release builds for 30+ minutes. Anyone needing an Intel build builds locally: `cargo build --release --target x86_64-apple-darwin -p movement-logger`.

**Re-using a tag after deletion races the publish step.** Publish job runs with `if: always() && (one of build/macos-store/windows-msix succeeded)` — even a *cancelled* run triggers publish if at least one job finished successfully. After `gh run cancel`, either `gh release delete vX.Y.Z --yes --cleanup-tag` or accept the partial release.

**Notarize + staple the .app, not just the DMG.** Order: `ditto -c -k --keepParent .app .zip` → `notarytool submit zip --wait` → `stapler staple .app` → `hdiutil create dmg` → notarize+staple DMG. Sanity check: `spctl -a -vvv --type execute` after the .app staple. Reason: when the user drags the .app out to /Applications, Gatekeeper looks for a notarization ticket on the .app itself; offline users get `nicht geöffnet` if only the DMG was stapled.

**Apple signing secrets must be set per-repo** — `zdavatz/*` is a user account, not an org, so there's no org-level default. Each new repo needs all 10 secrets:

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

The `if:` gates on `env.APPLE_API_KEY_P8 != '' && env.MACOS_DEVELOPER_ID_CERTIFICATE != ''` silently skip notarization when secrets are missing, so the first release of a fresh repo ships unsigned unless these are set first. `gh secret list --repo "$REPO"` should show all 10.

Optional store paths gated on repo/org variables:

| Var | Effect |
|---|---|
| `vars.MACOS_STORE_ENABLED == 'true'` | aarch64 `.app` → signed DMG (Developer ID, notarized) → optional Mac App Store `.pkg` upload via altool/iTMSTransporter. **Must be set explicitly per repo** — no org default. Without it the macos-store job is silently skipped. |
| `vars.MSSTORE_ENABLED == 'true'` | MSIX pack → signed → optional Microsoft Store devcenter REST submission. |

Bundle id is `com.ywesee.movementlogger` (separate from rust2xml's `com.ywesee.rust2xml`) — needs its own App Store Connect record + provisioning profile before the Mac App Store gate flips on.

Local sanity build: `cd Utilities/rust && cargo build --release -p movement-logger -p stbox-viz`. Re-run the `nm` check whenever bumping eframe/winit.

## WhatsApp CLI

`whatsapp/` — small Baileys-based Node.js CLI for sending firmware binaries (and plots). Four scripts, identical session store (`whatsapp/auth/`, git-ignored):

| Script | Purpose |
|---|---|
| `login.mjs` | Pair the CLI with your phone (QR scan, once per auth reset). `--force` wipes session. |
| `list-groups.mjs` | Dump JIDs of every group. |
| `send.mjs <jid-or-phone> <file> [caption]` | Auto-detects by extension: `.png/.jpg/.jpeg` → image, everything else → document. Phone-number shorthand (`41791234567`) is expanded to `<num>@s.whatsapp.net`. |
| `leave-group.mjs <jid>[,…]` | Leave one or more groups (comma-separated). |

Setup: `cd whatsapp && npm install`, then `node login.mjs`. Adapted from `~/software/pegelstand/whatsapp/`.

## Known Limitations

- Some Android devices have issues with BLE secure PIN connections — disable `STBOX1_BLE_SECURE_CONNECTION`.
- Some Android devices have issues with BLE force rescan — disable `BLE_FORCE_RESCAN`.
- Clean device BLE cache when switching between applications.
