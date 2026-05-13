# CLAUDE.md

Guidance for Claude Code working in this repository.

## Project Overview

FP-SNS-STBOX1 is an STM32Cube Function Pack for two ST evaluation boards:
- **STEVAL-MKBOXPRO** (SensorTile.box PRO) — STM32U585 + BlueNRG-LP BLE; sensors STTS22H, LPS22DF, LSM6DSV16X, LIS2DU12, LIS2MDL.
- **STEVAL-STWINBX1** (STWIN.box) — STM32U585 + BlueNRG-2 BLE; sensors IIS2DLPC, IIS2MDC, IIS3DWB, ISM330DHCX, IIS2ICLX, ILPS22QS, STTS22H.

Bare-metal C, STM32U5xx HAL.

## Build & Toolchains

IDE projects ship per application: STM32CubeIDE V1.18.1, IAR EWARM V9.60.3, Keil MDK-ARM V5.38.0. CubeMX `.ioc` in each app root.

**Command-line build** (BLESensorsPnPL and SDDataLogFileX only): `make` in `<app>/STM32CubeIDE/`. Toolchain auto-detected in `config.mk` (macOS: `$(HOME)/.software/arm-gnu-toolchain/bin`; Linux: `/usr/bin`). Override with `make TOOLCHAIN=/path`.

SDDataLogFileX flags via Makefile:
- `GPS_RATE_HZ=N` (default 10, 1..25) — drives `UBX-CFG-RATE`.
- `USB_CDC_ENABLED=1` — pulls in vendored TinyUSB, brings up OTG_FS, routes `printf` over USB CDC.

Keep installation paths short — deep paths break some toolchains.

## Repository Structure

```
Projects/STEVAL-MKBOXPRO/Applications/{Rev_A_B,Rev_C}/
Projects/STEVAL-STWINBX1/Applications/
Drivers/{BSP/{SensorTileBoxPro,STWIN.box,Components},CMSIS,STM32U5xx_HAL_Driver}
Middlewares/ST/{BlueNRG-LP,BlueNRG-2,STM32WB07_06,STM32_BLE_Manager,
                PnPLCompManager,ST25FTM,lib_nfc,threadx,filex}
Middlewares/Third_Party/{parson,uzlib,tinyusb}
```

## Applications

**MKBOXPRO** (Rev_A_B and Rev_C share the set): `BLEDualProgram`, `BLEMLC`, `BLEPiano`, `BLESensorsPnPL`, `NFC_FTM`, `SDDataLogFileX`.

**STWINBX1**: `BLEDefaultFw`, `BLEMLC`, `BLESensorsPnPL`, `ExampleCubeMxDataLog`, `NFC_FTM`, `SDDataLogFileX`.

Each app: `Inc/stbox1_config.h` (feature flags), `Src/main.c`, `Src/app_<name>.c`, `Src/ble_implementation.c`, `Src/ble_function.c`, `Src/ota.c`, `Binary/`.

## Configuration Flags (`stbox1_config.h`)

- `STBOX1_ENABLE_PRINTF` — UART debug. Off in SDDataLogFileX (UART4 is GPS).
- `STBOX1_BLE_SECURE_CONNECTION`, `BLE_FORCE_RESCAN` — disable for some Android compatibility issues.
- `STBOX1_UPDATE_ENV` / `STBOX1_UPDATE_INV` — sensor polling intervals (timer ticks).
- `STBOX1_LOG_AUDIO` (SDDataLogFileX) — `0` by default; `1` only on unmodified hardware (on the 3.3 V mod, `BSP_AUDIO_IN_Init` blocks indefinitely).
- `STBOX1_LOG_BATTERY` (SDDataLogFileX) — STC3115 fuel gauge + `BatNNN.csv`. Default `1`. I²C failure is non-fatal.
- `STBOX1_ENABLE_BLE_SYNC` (SDDataLogFileX) — BLE FileSync. Default `0`. When `1`, box advertises as `PumpTsueri` ~8 s after boot. Cost: +28 KB flash, +4.5 KB BSS.
- `STBOX1_ENABLE_WLC` — untested BlueNRG-LP OTP programmer. Default `0`, no call site.
- `STBOX1_ENABLE_USB_CDC` — set via `make USB_CDC_ENABLED=1`. macOS Sequoia 15+ does not enumerate the CDC interface; use Linux for live debug.

## SDDataLogFileX — CubeMX-Default Overrides

These differ from a fresh CubeMX regenerate and **must be re-applied** after `.ioc` edits:

### Clock
**HSI** (internal 16 MHz) is the PLL source, not HSE. The 3.3 V mod made the crystal unreliable on battery boot. PLL/PLL2/PLL3 source from HSI; sysclk = 160 MHz.

### MSP / power
`HAL_PWREx_EnableVddIO2()` in `Core/Src/stm32u5xx_hal_msp.c::HAL_MspInit`. Without it, the GPIO bank on VddIO2 (PG[15:2] on Rev_C) stays gated → BLE init fails.

### NVIC priorities
| IRQ | Priority | Why |
|---|---|---|
| EXTI13 (user button) | 0 | Stock |
| UART4 (GPS) | 6 | Above SDMMC |
| SDMMC1 | 14 | Below UART4 so GPS line keeps assembling |
| EXTI11 (BLE HCI IRQ) | 14 | Same as SDMMC; bounded ISR loop in `hci_tl_interface.c::hci_tl_lowlevel_isr` caps at 16 events per IRQ |

A strong `EXTI11_IRQHandler` override is required in `stm32u5xx_it.c` — default weak binding goes to `Default_Handler` (infinite loop), wedging the chip on the first BlueNRG-LP IRQ.

### GPIO
- **PC13 (user button) = `GPIO_PULLDOWN`** — boards with the button disconnected float PC13 → continuous EXTI storm.
- **PD12/PD13 (I²C4) = `GPIO_PULLUP`** for STC3115. External 4.7 kΩ may be needed on some boards.

### SDMMC
`SDMMC_NSPEED_CLK_DIV` (~12.5 MHz), not HSPEED. NSPEED ceiling ~6 MB/s is well above the ~5 KB/s budget and gives the 3.3 V rail more margin.

### I²C4 (STC3115)
Timing `0xA040184A` (~145 kHz). The CubeMX default `0x00F07BFF` (~421 kHz) overshoots STC3115 Fast-Mode.

### ThreadX / FileX
- `TX_APP_MEM_POOL_SIZE` = 16 KB, `FX_APP_MEM_POOL_SIZE` = 14 KB (in `AZURE_RTOS/App/app_azure_rtos_config.h`).
- `tx_application_define` failure paths call `halt_red_morse(N)`: 2 = tx pool, 4 = App_ThreadX_Init, 6 = fx pool, 8 = MX_FileX_Init.

### LIS2MDL drift reduction (`Drivers/BSP/Components/lis2mdl/lis2mdl.c`)
- `LIS2MDL_SENS_OFF_CANC_EVERY_ODR`
- `lis2mdl_offset_temp_comp_set`
- Low-pass `LIS2MDL_ODR_DIV_4` (25 Hz BW at 100 Hz ODR)

## Issue #15 mode-switch architecture (SDDataLogFileX, BLE_SYNC=1)

BLE and SDMMC never run concurrently. Two boot modes selected by `TAMP->BKP1R`:

1. Boot → BLE mode by default (BKP1R != `0x4C4F4720`). Advertises as `PumpTsueri`, logger idle. iPhone GUI can LIST/READ/DELETE existing files.
2. App writes `OP_START_LOG` (0x05) + 4 LE bytes duration → firmware sets `BKP1R = 'LOG '` + `BKP2R = duration` → `NVIC_SystemReset`.
3. After reset `main.c` reads BKP1R, sets `g_app_mode = APP_MODE_LOG`, clears magic. fx_thread auto-starts logging, ble_sync_thread parks.
4. On duration expiry (`tx_time_get()/100 >= g_log_duration_seconds`), fx_thread closes files, `fx_media_close`, `NVIC_SystemReset` back to BLE mode.

Fallback: in BLE mode a short user-button press starts a 5-minute LOG session via the same TAMP-magic path. In LOG mode the button aborts a session early.

`ErrorLog_Open()` runs unconditionally at boot so the error log captures BLE-mode boots too.

## SD Card Data Format (SDDataLogFileX)

- `SensNNN.csv` — sensors @ ~100 Hz: tick, acc XYZ (mg), gyro XYZ (mdps), mag XYZ (mgauss), pressure (hPa), temp (°C). Gyro FS 500 dps (17.5 mdps/LSB), accel FS 4 g (0.122 mg/LSB).
- `MicNNN.wav` — mono 16-bit PCM @ 16 kHz.
- `GpsNNN.csv` — GPS fixes @ 10 Hz: tick, UTC (hhmmss.ss), lat/lon (signed decimal), alt, speed (km/h), course, fix quality, num sats, HDOP.
- `BatNNN.csv` — STC3115 @ 1 Hz: tick, mV, SOC (0.1 %), current (100 µA, signed).

Timestamps are **ThreadX ticks** (10 ms each). CSV header `Time [10ms]`.

`COMMAND_SAVE_SENSORS` calls `fx_media_flush` every 100 samples so power-off mid-session does not leave 0-byte files. WAV header is only updated on graceful stop.

### LEDs
- 1/2/3 green blinks during boot (after clock+ICache+LED / `GPS_Init` / `InitMemsSensors`).
- Green solid = logging active.
- Red blinking = `Error_Handler`.
- Red solid = SD update in progress, or pre-green-blink hang.
- Red morse 2/4/6/8 = ThreadX/FileX init failure.
- `DiagBlinkRed(n)` in `InitMemsSensors`: 1 entered, 2 LIS2MDL, 3 LSM6DSV16X, 4 LPS22DF, 5 STTS22H, 6 exit. Costs ~3 s, pinpoints blocked sensor.

### Buzzer
PE13, TIM1_CH3 PWM (`Core/Src/buzzer.c`). `Buzzer_BootDone()` — two tones (1500/3000 Hz) just before `MX_ThreadX_Init`. `Buzzer_FixAcquired()` — three chirps on first valid GPS fix, latched once per boot.

### Error log
`Error_Log_Pump_Tsueri_dd.mm.yyyy.log` on SD root. Boot block written by `ErrorLog_Open()`: boot version+date, firmware fingerprint, reset reason from `BootResetCsr` (snapshotted from `RCC->CSR` right after `HAL_Init`), BLE probe outcome, gauge probe outcome, GPS init log. `START_LOG` writes a forward-looking manifest then `fx: <op> returned status=0xN` markers around each FileX call. `ErrorLog_Write` is batched; `ErrorLog_Flush()` commits.

### Clock seeding
`UpdateFileXClock()` stamps FileX time from `__DATE__`/`__TIME__` until `gps_thread` calls `SetClockBaseFromGPS()` on first valid `$GNRMC`. Files created pre-fix get the compile date; later files get UTC. Month/year rollover past base date not handled.

## Firmware Update

**DFU is preferred.** BOOT0 wired to user button → STM32U585 USB-DFU bootloader at `0x0483:df11`. Full step-by-step in `Documentation/Flash_Firmware_Mac.html`.

Rev_B box DFU: slider OFF → hold button 2 → slider ON → wait 2 s → release → `dfu-util -l` shows `[0483:df11]`.

Stuck flash recovery (use when multiple builds hang identically):
```sh
dfu-util -d 0483:df11 -a 0 -s 0x08000000:mass-erase:force:leave -D firmware.bin
```

**SD-card update fallback**: `firmware.bin` on SD root → `CheckAndApplyFirmwareUpdate()` programs the inactive bank, renames to `firmware.done`, bank-swaps, resets. Max ~1016 KB. Known bug: bank-swap can fail silently — file renamed but old bank still runs. Use DFU in that case.

LED signals during SD update: 10× rapid green/red alternation on file open; red toggles every ~512 B during programming; 3× slow green blinks on success.

Makefile emits `build/firmware.bin` and `build/firmware_v<N>.bin` so binaries drop onto SD without renaming.

## Build Versioning (SDDataLogFileX)

Each `make` bumps `.build_counter` (committed; globally unique) and bakes `-DFW_BUILD_NUM=N`. The bump is at *Makefile parse time*, so any invocation — even `make clean` — burns a number. Don't ask "build firmware vN" for a specific N; the next build is always `current+1`. Three uses:

1. **Filename**: `build/firmware_v<N>.bin`.
2. **`FW_INFO_v<N>.TXT`** at SD root — field tester sees running version. `WriteFwInfoFile()` deletes prior `FW_INFO*` entries first (two-pass: `fx_directory_first_full_entry_find` to collect, then delete).
3. **Error log boot markers**.

IDE builds (CubeIDE/IAR/Keil) fallback to `FW_BUILD_NUM=0` so the field tester sees `v0` and knows it wasn't a CLI build.

## GPS (u-blox MAX-M10S)

Wiring → Rev_C: TX→PA1 (UART4 RX), RX→PA0 (UART4 TX), 3V3, GND. UART4 is owned by GPS → `STBOX1_ENABLE_PRINTF` off.

`GPS_Init()` in `Core/Src/gps_nmea.c` configures each boot with UBX-ACK verification:
1. UBX-CFG-PRT 9600 → `GPS_UART_BAUDRATE` (38400). No ACK wait (baud is switching).
2. Switch local UART4 to 38400.
3. UBX-CFG-RATE from `GPS_MEAS_PERIOD_MS` (= 1000 / `GPS_RATE_HZ`). Sent first; dropped RATE leaves persisted 1 Hz.
4. UBX-CFG-MSG ×4 — disable GLL/GSA/GSV/VTG; keep GGA + RMC.
5. UBX-CFG-VALSET — `CFG-NAVSPG-DYNMODEL = 5` (Sea). Layers `0x01` (RAM only); the multi-layer form is rejected on some MAX-M10S firmware.
6. UBX-CFG-CFG — save BBR + Flash + EEPROM.

ACK status accumulated in static buffer, flushed on first START_LOG via `GPS_GetInitLog()`. Format: `gps: rate=OK(10Hz) msg=OK save=OK`. `rate=TO/NAK(...)` means the module fell back to persisted config.

NMEA parsing runs in `gps_thread` (priority 11), not in the UART IRQ. The IRQ only pushes bytes into a 1 KB ring buffer; `GPS_Process()` drains and parses per poll cycle. Earlier in-IRQ parsing at NVIC priority 6 preempted SDMMC and collapsed sample rate to ~7.7 Hz after 90 s.

10 Hz fits in ~1.5 KB/s on 38400 baud (ceiling ~3.8 KB/s). 25 Hz max for single-GNSS.

## BLE FileSync — SD-card download over Bluetooth

When `STBOX1_ENABLE_BLE_SYNC=1`, box advertises as `PumpTsueri` with PIN `123456`. Two characteristics under BlueST features service `00000000-0001-11e1-9ab4-0002a5d5c51b`:

| Characteristic | UUID | Properties |
|---|---|---|
| FileCmd  | `00000080-0010-11e1-ac36-0002a5d5c51b` | write w/o response |
| FileData | `00000040-0010-11e1-ac36-0002a5d5c51b` | notify |

Opcodes (1 byte + optional payload — filename without trailing NUL):

| Opcode | Meaning | FileData reply |
|---|---|---|
| `0x01` LIST | enumerate SD root | `name,size\n` rows + single `\n` terminator |
| `0x02` READ `<name>` | stream body | raw bytes; length matches LIST size |
| `0x03` DELETE `<name>` | drop file | single status byte |
| `0x04` STOP_LOG | gracefully close active session | no reply |
| `0x05` START_LOG + 4 LE bytes | begin LOG session | no reply, firmware resets |

Status bytes: `0x00` OK, `0xB0` BUSY (logging active), `0xE1` NOT_FOUND, `0xE2` IO_ERROR, `0xE3` BAD_REQUEST. READ/DELETE return BUSY while `Sens*.csv` or `Gps*.csv` is open for writing — host calls STOP_LOG first.

### Firmware-side architecture
- `Core/Src/ble_sync.c` — one ThreadX thread (priority 14, below FileX 12 and GPS 11 so SD bandwidth always wins). Loops HCI event + `BleFileSync_Tick()` + 1-tick sleep.
- `Core/Src/ble_filesync.c` — `FileCmd`/`FileData` characteristics + state machine. On notify congestion, LIST stays in `ST_LIST_EMIT`, READ rewinds with `fx_file_relative_seek(SEEK_BACK)` — no bytes dropped. `BleFileSync_Reset()` is called from `disconnection_completed_function` so a mid-LIST/mid-READ drop doesn't leave `state != ST_IDLE` and silently swallow the next OP_LIST.
- `Core/Src/ble_spi.c`, `hci_tl_interface.c` — isolated SPI1 driver; the BLEDualProgram and SDDataLogFileX BSPs both export the same `BSP_*` symbols and can't coexist.
- `FileX/App/app_filex.c` — public hooks `Ble_RequestStopLog()` and `Ble_IsLoggingActive()`.

User-facing explainer at `Documentation/BLE_FileSync.{html,pdf}`. Render: `chrome --headless --disable-gpu --no-pdf-header-footer --print-to-pdf=X.pdf "file://$PWD/X.html"`.

## USB CDC ACM debug console

With `USB_CDC_ENABLED=1`, STM32U585 OTG_FS comes up as a CDC ACM port. Vendored TinyUSB at `Middlewares/Third_Party/tinyusb/`. Glue at `Core/Src/{usb_cdc.c,usb_descriptors.c}`, `Core/inc/{tusb_config.h,usb_cdc.h}`. Service thread at priority 13.

### Hardware setup quirks (in `usb_cdc.c::usb_cdc_hw_init()`)
- `__HAL_RCC_PWR_CLK_ENABLE()` **before** `HAL_PWREx_EnableVddUSB()` — `PWR->SVMCR.USV` silently no-ops when PWR clock is gated.
- `__HAL_RCC_CRS_CLK_ENABLE()` + `HAL_RCCEx_CRSConfig(RCC_CRS_SYNC_SOURCE_USB)` — auto-trims HSI48 from USB SOF (HSI48 is ±1-2 % otherwise).
- `USB_OTG_FS->GUSBCFG |= USB_OTG_GUSBCFG_PHYSEL` after OTG_FS RCC clock is enabled, **before** `tud_rhport_init`. ST's HAL does this in `USB_CoreInit`; TinyUSB's `dwc2_phy_init` for U5 does NOT. Without it, `tud_rhport_init` returns true but zero OTG_FS interrupts ever fire.
- `RCC_PERIPHCLK_ICLK` + `RCC_ICLK_CLKSOURCE_HSI48` — on U5 the USB/SDMMC/RNG share one ICLK selector. Not `RCC_PERIPHCLK_USB`/`_CLK48` (don't exist on U5).

### LED conventions during USB bringup
- 1 long green flash (~600 ms) between boot blinks and solid = `tud_rhport_init` OK.
- 7 quick red bursts = `tud_rhport_init` failed (clock/power above).
- Red solid + green solid = `tud_mounted()` true.
- Red slow blink (250/750 ms) + green solid = OTG_FS IRQs firing but mount stalled.

### Heartbeat
On mount, 1 Hz `printf`:
```
hb t=12345 otg_irq=87 ble=0xF2 conn=1 mounted=1
```
`ble=` is `g_ble_probe_status`: 0xF0 entered, 0xF1 in probe, 0xF2 in `bluetooth_init`, 0xF3 transient, 0xF4 arming EXTI11, 0 probe failed, 1 advertising, 2 init failed, 0xFF pending.

`UsbCdc_Write` gates on `tud_mounted()`, not `tud_cdc_connected()`. The latter requires DTR (SET_CONTROL_LINE_STATE); Linux `cdc_acm` asserts it on `open()`, macOS does not. Heartbeat printf is hand-formatted (no newlib stdio) to avoid `_REENT` deadlocks.

### macOS Sequoia 15+ blocker
`AppleUSBCDCCompositeDevice` partial-attaches in `IOMatchDefer = Yes` and never registers — `/dev/tty.usbmodem*` is never created, AND libusb cannot claim the bulk endpoint even as root. Tried CDC+IAD, pure CDC, vendor-specific — all blocked. **Use Linux for live debug:**

```sh
dmesg | tail
sudo cat /dev/ttyACM0      # heartbeat stream
```

Add user to `dialout` group to drop `sudo`: `sudo usermod -aG dialout $USER` then log out + in. `dfu-util` itself still needs an interactive shell because libusb's `uaccess` udev tag only applies to logind sessions.

Leftover Mac-side helpers (both broken on Sequoia, work on Linux): `Utilities/usb_console.py` (pyusb), `Utilities/rust/usb-console/` (rusb).

### Software-triggered DFU bootloader
Writing `DFU\n` to `/dev/ttyACM0` reboots into the system bootloader without the BOOT0 button dance:

```sh
make USB_CDC_ENABLED=1
echo DFU > /dev/ttyACM0
sleep 1
sudo dfu-util -d 0483:df11 -a 0 -s 0x08000000:mass-erase:force:leave -D build/firmware.bin
```

Implementation: `usb_cdc.c::jump_to_bootloader()` writes `0xDEADBEEF` to `TAMP->BKP0R` and calls `NVIC_SystemReset`. `main.c` checks BKP0R **before** `HAL_Init`, clears it, chains into `0x0BF90000` (STM32U5 system memory per AN2606 §52). Direct-jump from running firmware leaks too much state (OTG_FS dirty, threadx pending ticks) and the bootloader silently fails to bring up USB DFU.

**SD-update interaction**: a stale `firmware.bin` on SD overwrites every successful DFU flash on next boot. Delete it before starting the DFU iteration loop.

## BlueNRG-LP OTP / WLC

The SensorTile.box PRO has two MCUs: STM32U585 (host) and BlueNRG-LP (BLE controller, SPI1 + EXTI11). The BlueNRG-LP needs an OTP-burned stack (`dtm.bin`, ~200 KB) before answering HCI. Chips ship factory-pre-programmed; **there is no validated in-firmware OTP-programming path**. `Core/Src/wlc.c` is a Ghidra-reverse-engineered port behind `STBOX1_ENABLE_WLC` (default 0, no call site) — untested, kept for reference.

## Visualization (`stbox-viz`)

Rust crate at `Utilities/rust/stbox-viz/`. Build: `cargo build --release`.

| Subcommand | Output | Input |
|---|---|---|
| `combined` | interactive Plotly HTML (map + nose angle + baro height + speed) | `SensNNN.csv` + auto-detected `GpsNNN.csv` |
| `sensors` | 5-panel summary PNG + quaternion/Euler PNG | `SensNNN.csv` |
| `pumpfoil` | pump-cadence spectrogram + movement-phase PNGs | `SensNNN.csv` |
| `animate` | per-session GIF (3 or 5 panels) + optional MOV with camera | `SensNNN.csv` + optional video |
| `compass` | mag-vs-GPS heading residual PNG | `SensNNN.csv` + `GpsNNN.csv` |

Crate layout: `io.rs`, `fusion.rs` (Madgwick 6DOF), `euler.rs`, `session.rs` (pitch-oscillation detection), `gps.rs` (haversine + ride detection), `baro.rs` (TC + GPS-anchored water reference), `butter.rs` (4th-order Butterworth + filtfilt), `spectrogram.rs` (scipy-equivalent STFT via `rustfft`), `html.rs` (Plotly JSON), `plot_common.rs`, one `*_cmd.rs` per subcommand.

GPS auto-detection accepts firmware on-card layout `SensNNN.csv` ↔ `GpsNNN.csv` (preferred) and legacy `<stem>.csv` ↔ `<stem>_gps.csv`. See `guess_gps_path`.

User-facing docs: `Documentation/{Sensor_Fusion,BLE_FileSync,Odyssey}.{html,pdf}`.

### `combined` HTML
Time axis: UTC anchored to GPS clock. `--tz-offset-h <h>` shifts to local. Plotly Scattermap (`carto-positron`), one trace per detected ride coloured by speed. Cross-panel `hovermode: "x unified"` + spike lines; click pins a dashed numbered marker. Ride detection: GPS > 3 km/h for ≥10 s, merge gaps < 30 s, pad ±3 s. Sensor data binned to 100 ms display buckets via `BTreeMap` keyed by bucket index.

Baro height: temperature-compensated, then re-anchored using GPS speed < 3 km/h as water reference, hypsometric formula `8434 × (1 − P/P_ref)`. Display Y-clamp `[-0.1, 0.9] m`; samples outside `[-0.15, 0.95]` → NaN so thermal drift shows as gaps.

Speed: position-derived (haversine; Doppler unreliable on this board). Reject `|Δspeed|/Δt > 15 km/h/s & v > 15 km/h`, then 5 s rolling median.

### `animate`
3 panels without GPS, 5 with (board + pump detail + height + speed + nose angle). Nose angle: Butterworth 0.7 Hz LP + filtfilt + 10 s rolling median baseline.

`--at HH:MM[:SS]` mode bypasses session detection and renders one GIF for an exact wall-clock slice. Pair with `--tz-offset-h`, `--date`, `--duration <s>`. **Anchor both ends by GPS tick** — ThreadX runs on HSI (±1 %, ~7 s drift over 21 min); extrapolating from one anchor slices the wrong sensor range.

`--video FILE` shells `ffmpeg`/`ffprobe` for `hstack=inputs=2` MOV with optional title-card (`--title`, `--subtitle`). `ffmpeg` must be in PATH.

Phase detection (`--at` mode): Tragen (gray) → Anschieben/Rennen (yellow) → Foilen (green). Push-off = 1 s rolling speed > 4 km/h for 0.5 s. Foilen = 3 s mean baro height > 0.15 m. Push-off-Winkel flash for 2 s at push-off.

Hybrid height with `--dock-height-m <h>` (e.g. 0.75 for Ermioni harbour): dock flat, 2 s linear ramp across water entry, foiling baro re-anchored to pressure ±0.5 s around push-off. Foiling clamped to `[-0.1, 0.9] m`.

#### 3D foil mesh (`--board-stl <FILE>`)
Software rasterizer in `board3d.rs` (no GPU, no winit). 600×400 RGBA, fixed camera. `--mount mast|deck` (default `mast`) selects:
- `R_mount`: mast `(-z, x, y)`, deck identity.
- Camera eye: mast port-side `(0, 3.2, 0.5)`, deck behind tail `(-3, 0, 0.7)`.
- Body-to-world: mast uses Madgwick `quat_strip_yaw(quat_conj(q))`; deck uses **accel-only tilt** (0.25 s smoothing → atan2 pitch+roll) with 180°-X pre-flip — gyro bias around body-Z survives `quat_strip_yaw` which only kills world-Z. Madgwick still drives nose-angle trace and push-off flash.

Mesh hidden during carry; appears when sustained GPS > 4 km/h OR 10.5 s elapsed. Carry shows "Tragen — keine 3D-Daten" placeholder.

Without `--at`, session detection is pitch-oscillation based (≥ 0.3 Hz over ≥ 30 s, merge < 60 s gaps). Smooth-flight data without clear pitch oscillation won't register — use `combined` for those.

### `compass`
Tilt-compensates body-frame mag (Honeywell AN203), computes heading vs GPS course. Median residual should match local declination (~+4° E Greece, ~+2.5° E Zürich). Real-board first run was essentially random — ferromagnetic distortion dominates; fix is hardware (plastic screws, sensor placement away from iron).

## MovementLogger GUI

Drag-and-drop wrapper around `stbox-viz animate`, source at `Utilities/rust/stbox-viz-gui/`, binary `MovementLogger`. Workspace at `Utilities/rust/Cargo.toml` lists `stbox-viz` and `stbox-viz-gui`. Vendored `winit-patched/` is `exclude`d and wired via `[patch.crates-io]`.

**Mac App Store winit patch is required.** eframe → winit 0.30 calls `_CGSSetWindowBackgroundBlurRadius` (private CoreGraphics); Apple's scanner rejects. Fork replaces `Window::set_blur` with a no-op and drops unused `NSInteger`/`AnyObject` imports. Verify: `nm target/release/MovementLogger | grep CGSSetWindowBackgroundBlur` returns nothing. **eframe must stay ≥ 0.29** — 0.28 drags in winit 0.29 alongside 0.30, and the patch only matches 0.30.

### Window-layout conventions (any egui app here)
- Version in the OS title only via `ViewportBuilder::with_title(format!("App {}", env!("CARGO_PKG_VERSION")))`. Don't also write `ui.heading("App 1.2.3")` — duplicates OS chrome.
- Top-right of title strip: app logo as frameless `ImageButton` opening `mailto:<support>` via `ctx.open_url(...)`. PNG via `include_bytes!`, decoded once with `image::load_from_memory` (default-features off, `png` only), uploaded with `ctx.load_texture`. Reuse the PNG in `IconData` for the window icon.

### BLE FileSync panel
Workflow: Scan (5 s) → click PumpTsueri → Connect (OS prompts Bluetooth + PIN `123456`) → Refresh → tick rows → Download selected. Files saved to `csv/`; `Sens*.csv` and `*_gps.csv` auto-route into Sensor/GPS form slots.

Backend in `src/ble.rs` uses `btleplug` (CoreBluetooth/BlueZ/WinRT) on a tokio current-thread runtime on one worker thread. `std::sync::mpsc` channels to/from egui. **One notification stream per connection** (opened on Connect, demuxed inside `tokio::select!`) — per-op streams risk losing the first packet. 200 ms watchdog tick surfaces stuck transfers.

Status-byte detection on READ: only treat single-byte first notify as error when byte ∈ `{0xB0, 0xE1, 0xE2, 0xE3}` — 1-byte CSV/log files stream correctly because no legitimate file ever starts with one of those bytes.

LIST rows split into **Sensor** (Sens/Gps/Bat/Mic) and **Debug** (everything else — FW_INFO, CHK, error log, AppleDouble `._*`, 0-byte phantoms). Only Sensor default-ticked. Each group has Tick/Untick-all toggles.

Download serial queue: `ble_dl_queue: VecDeque<(String, u64)>`; `advance_download_queue()` pops the head on `ReadDone` or READ-side errors (NOT_FOUND, BUSY, IO_ERROR, BAD_REQUEST, timeout, disconnect mid-op). "another op in flight" is filtered out so a stale-state click doesn't skip the next file.

Trash button per row wires `OP_DELETE`. Dispatched outside the row-render closure via `delete_target: Option<String>` since the closure already holds `&mut self.ble_files`.

Log panel selectable + adaptive: `TextEdit::multiline` without `.interactive(false)` is read-only-by-discard but still selectable (Cmd-A/Cmd-C). `desired_rows = ui.available_height() / row_height`; "Copy all" dumps the buffer to clipboard.

Graceful disconnect: `eframe::App::on_exit` sends `BleCmd::Disconnect` and sleeps 250 ms so btleplug emits `LL_TERMINATE_IND`. Without this, a quit leaves the connection alive until host supervision timeout (~10–30 s on macOS) and a fresh GUI can't connect — only fix is box reboot. Disconnect button optimistically flips state to Idle without waiting for the worker's Disconnected event.

Auto-LIST on every Connected event clears `ble_files` and sends `BleCmd::List` so reconnect doesn't show "No file list yet — hit Refresh".

macOS bundled `.app` carries `NSBluetoothAlwaysUsageDescription` (`assets/Info.plist.template`); App-Sandbox build adds `com.apple.security.device.bluetooth` (`entitlements-appstore.plist`). Bare `cargo run` on a fresh account may not trigger consent — install the `.app`.

### In-app updater (cross-platform from v0.1.10)
`spawn_update_check()` hits `GET https://api.github.com/repos/zdavatz/fp-sns-stbox1/releases?per_page=30` (15 s timeout, `rustls-tls` so no OpenSSL/Schannel dep), picks the highest non-prerelease `v*` tag greater than `env!("CARGO_PKG_VERSION")`, and matches `target_asset_suffix()` (compile-time `cfg!` → `-macos-aarch64.dmg` / `-x86_64-unknown-linux-gnu.tar.gz` / `-x86_64-pc-windows-msvc.zip`). Blue banner surfaces the result; "Check for updates" button re-triggers.

Per-platform install paths in `installer.rs`:
- **macOS**: download DMG → `hdiutil attach -nobrowse -readonly` → `codesign --verify --deep --strict` → `ditto` to `.MovementLogger.app.new.<pid>` → bash helper waits for PID, `mv` swap, `open` relaunch.
- **Linux**: tar.gz → `tar -xzf` → `find_recursive("MovementLogger")` → stage `.MovementLogger.new.<pid>` (`chmod 0o755`) → bash helper, `mv` swap binary + sibling `stbox-viz`, `setsid -f` relaunches.
- **Windows**: zip → PowerShell `Expand-Archive -Force` → `find_recursive("MovementLogger.exe")` → stage `MovementLogger.exe.new.<pid>` → PowerShell helper polls `Get-Process`, `Move-Item` swap (Windows allows renaming a running .exe; open handle keeps the old name alive), `Remove-Item .old`, `Start-Process`.

Install thread sleeps 600 ms then `process::exit(0)` so helper's poll loop unblocks. `installer::can_in_app_update()` gates the UI: target asset suffix non-empty AND (macOS → `current_app_bundle()` succeeds; Linux/Windows → `current_exe()` succeeds). Unsupported targets (aarch64 Linux CLI-only, BSD, etc.) fall through to "Open release page".

Deps: `reqwest` (`default-features = false`, `features = ["blocking", "json", "rustls-tls"]`), `serde` (with derive). `std::sync::mpsc` matches the BLE-events pattern — no `crossbeam-channel`.

### Releases
`.github/workflows/release.yml` — tag `vX.Y.Z` triggers per-platform matrix (Linux x86_64 + aarch64 CLI-only, macOS Apple Silicon, Windows x86_64). Each artifact `MovementLogger-vX.Y.Z-<target>.tar.gz`/`.zip` with SHA256. macOS path assembles a `MovementLogger.app` (with `stbox-viz` inside `Contents/MacOS/`) and a notarized DMG.

Intel Mac dropped after v0.1.5 — every Mac since late 2020 is Apple Silicon, and the `macos-13` runner queue was wedging release builds. Build locally if needed: `cargo build --release --target x86_64-apple-darwin -p movement-logger`.

**Notarize + staple the .app, not just the DMG.** Order: `ditto -c -k --keepParent .app .zip` → `notarytool submit zip --wait` → `stapler staple .app` → `hdiutil create dmg` → notarize+staple DMG. Sanity: `spctl -a -vvv --type execute` on the stapled .app. Without this, users dragging the .app out of the DMG to /Applications get a Gatekeeper "nicht geöffnet" error.

**Re-using a tag after deletion races publish.** Publish runs with `if: always() && (build/macos-store/windows-msix succeeded)` — even a cancelled run triggers publish if at least one job finished. After `gh run cancel`, either `gh release delete vX.Y.Z --yes --cleanup-tag` or accept the partial release.

**Apple signing secrets are per-repo** (`zdavatz/*` is a user account, no org default). Each new repo needs all 10 secrets — see commit log or `.github/workflows/release.yml` for the list and `gh secret set ...` invocations. The `if:` gates on `env.APPLE_API_KEY_P8 != ''` silently skip notarization when missing, so the first release of a fresh repo ships unsigned unless secrets are set first.

Optional store paths:
| Var | Effect |
|---|---|
| `vars.MACOS_STORE_ENABLED == 'true'` | aarch64 `.app` → signed DMG → optional MAS `.pkg`. Set explicitly per repo. |
| `vars.MSSTORE_ENABLED == 'true'` | MSIX pack → signed → optional devcenter REST submission. |

Bundle id `com.ywesee.movementlogger`. Needs an App Store Connect record + provisioning profile before MAS gate is flipped.

Local sanity build: `cd Utilities/rust && cargo build --release -p movement-logger -p stbox-viz`. Re-run the `nm` blur check whenever bumping eframe/winit.

## WhatsApp CLI

`whatsapp/` — Baileys-based Node.js scripts for sending firmware binaries and plots. Identical session store (`whatsapp/auth/`, git-ignored):

| Script | Purpose |
|---|---|
| `login.mjs` | Pair via QR (`--force` wipes session). |
| `list-groups.mjs` | Dump group JIDs. |
| `send.mjs <jid-or-phone> <file> [caption]` | Auto-detects image vs document by extension. Phone shorthand `41791234567` expands to `<num>@s.whatsapp.net`. |
| `leave-group.mjs <jid>[,…]` | Leave one or more groups. |

Setup: `cd whatsapp && npm install && node login.mjs`.

## Known Limitations

- Some Android devices have issues with BLE secure PIN connections — disable `STBOX1_BLE_SECURE_CONNECTION`.
- Some Android devices have issues with BLE force rescan — disable `BLE_FORCE_RESCAN`.
- Clean device BLE cache when switching between applications.
