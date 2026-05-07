## <b>SDDataLogFileX Application Description for STEVAL-MKBOXPRO (SensorTile.box-Pro) evaluation board</b>

This firmware package includes Components Device Drivers, Board Support Package and example application for the following STMicroelectronics elements:

  - STEVAL-MKBOXPRO Rev C (SensorTile.box-Pro) evaluation board that contains the following components:
      - MEMS sensor devices: STTS22H, LPS22DF, LSM6DSV16X, LIS2DU12, LIS2MDL
	  - Dynamic NFC tag: ST25DV64KC
	  - On-board Bluetooth® Low Energy: STM32WB07KC
      - Digital Microphone: MP23db01HP  
 
The Example application provides one example of one simple SD Data logger
Pressing the User Button is possible to start/stop the logger
For each log session, the board saves:

- one .wav file that it's the output of the digital microphone
- one .csv file with the sensors logged at 100Hz
- one GpsNNN.csv file with 10 Hz GPS fixes (only when a u-blox MAX-M10S module is connected via UART4)
- one BatNNN.csv file with 1 Hz battery readings from the on-board STC3115 fuel gauge

The FAT directory entries are flushed every ~1 s during logging (`fx_media_flush` inside `COMMAND_SAVE_SENSORS`), so the SensNNN.csv, GpsNNN.csv, and BatNNN.csv remain valid even after an ungraceful power-off — necessary on hardware-modified boards where the user button is disconnected and there is no graceful stop. At most the final second of sensor data is lost. The WAV header is only finalized on graceful stop; after an ungraceful power-off the audio data is still on the card, but the header keeps pointing at the init dummy size.

File timestamps start from the compile date + seconds-since-boot (`UpdateFileXClock()`) but are automatically overridden by GPS UTC once the first `$GNRMC` fix with a valid date arrives — `gps_thread` calls `SetClockBaseFromGPS(...)` which shifts the FAT directory-entry stamps on subsequent flushes to today's real date. Without GPS lock, files keep the compile-date fallback.

Audio logging is disabled by default via `STBOX1_LOG_AUDIO 0` in `stbox1_config.h`. On hardware-modified boards (3.3V mod, disconnected connectors) `BSP_AUDIO_IN_Init()` can hang without ever returning, which blocks `fx_thread` mid-START_LOG and stops all sensor and GPS writes. Set `STBOX1_LOG_AUDIO 1` only on an unmodified board. When disabled, no `MicNNN.wav` files are created and the stage-marker `start: mic disabled at compile time` appears in the error log.

### <b>Battery logging</b>

The STEVAL-MKBOXPRO has an STC3115 fuel-gauge IC on I²C4; firmware reads it once per second (in the same 100-sample flush tick as the sensor data) and writes a row to `BatNNN.csv`:

```
Time [mS], Voltage [mV], SOC [0.1%], Current [100uA]
```

- **Voltage** is battery terminal voltage in millivolts (e.g. 4150 for 4.15 V)
- **SOC** is state-of-charge in tenths of a percent (e.g. 983 = 98.3 %)
- **Current** is signed in 100 µA units; positive = charging, negative = discharging

The gauge is initialised once per boot on the first START_LOG (`BSP_GG_Init`); a per-session start-of-session and end-of-session reading also land in the error log as human-readable markers (`start: battery 4150 mV 98.3%` / `stop: battery 3820 mV 61.2%`), so you can see drain at a glance without opening the CSV.

Gated by `STBOX1_LOG_BATTERY 1` in `stbox1_config.h`. Set to `0` if the gauge ever hangs like the MIC did on hardware-modified boards — same non-fatal pattern, the rest of the logger keeps running. If gauge init returns an I²C error, an error-log marker (`start: gas gauge init FAIL - continuing without battery log`) is written and battery logging is skipped for the rest of the boot.

**I²C4 timing investigation (24.4.2026) — hypothesis falsified.** Every field test from 22.–24.4.2026 logged `gas gauge init FAIL` on every boot. ST's v1.6.0 → v2.0.0 update replaced the I²C4 timing `0xA040184A` (~145 kHz at `PCLK1=160 MHz`, explicitly chosen to stay below the STC3115's 400 kHz Fast-Mode ceiling) with `0x00F07BFF` (~421 kHz) — above spec, so we reverted `MX_I2C4_Init` in `SensorTileBoxPro_bus.c` to `0xA040184A`. Peter's test of the reverted build (13:26) still logged `gas gauge init FAIL` — **the timing was not the root cause.** We left the revert in place (safer value within the STC3115 spec) but the real issue was elsewhere.

**Diagnostic probe (24.4.2026, 16:59 build) — root cause found.** `BSP_GG_Init` only returns `COMPONENT_OK`/`COMPONENT_ERROR` without telling us which I²C step failed. To disambiguate between hardware (chip not on the bus) and driver (chip ACKs but ST driver fails), `app_filex.c` calls `BSP_I2C4_Init()` and `HAL_I2C_IsDeviceReady(&hi2c4, 0xE0, 3, 200)` right before the opaque `BSP_GG_Init` call and writes a diagnostic line to the error log. Peter's 16:59 field test result:

```
gauge: i2c4_init=0 ping_0xE0=NAK halerr=0x00000020
```

| halerr code | Meaning | Likely cause |
|---|---|---|
| `0x00000004` (`HAL_I2C_ERROR_AF`) | Address NAK | Chip not responding at 0xE0 — dead chip, wrong address, or unpowered |
| **`0x00000020`** (`HAL_I2C_ERROR_TIMEOUT`) | **Bus timeout** | **Pull-up weak or bus stuck low** ← Peter's symptom |
| `0x00000001` (`HAL_I2C_ERROR_BERR`) | Bus error | Misplaced start/stop, layout glitch |
| `HAL_OK` + still fails later | Chip ACKs but init fails | ST driver bug, register-value mismatch |

Ping happens once per boot (gated by `BatteryInitAttempted`), 3 retries × 200 ms timeout → worst case 600 ms, negligible against the other init steps.

**I²C4 internal pull-ups enabled (24.4.2026, 18:48 build).** `halerr=0x20` means SCL/SDA never release to high — classic missing/weak pull-up signature. Looking at `I2C4_MspInit` in `SensorTileBoxPro_bus.c`, PD12 (SCL) and PD13 (SDA) were configured with `GPIO_NOPULL`. I²C1/2/3 (MEMS sensors) evidently have external pull-ups populated on this board since those buses work fine, but I²C4 (to the STC3115) apparently does not — or they're too weak to drive the bus at all. Changed both pins to `GPIO_PULLUP` to enable the STM32U5's internal ~40 kΩ pull-ups. This is marginal for 400 kHz Fast-Mode but adequate at the 145 kHz we run I²C4 at with the short onboard trace. Next test will reveal whether the internal pull-ups are enough — if yes, `ping_0xE0=ACK` and battery logging starts. If the symptom persists, external 4.7 kΩ resistors need to be soldered on.

### <b>GPS module (u-blox MAX-M10S)</b>

UART4 (PA0/PA1) is wired to the GPS module instead of debug-printf:

- GPS TX → PA1 (UART4 RX)
- GPS RX → PA0 (UART4 TX — required for auto-config)
- GPS 3V3 → 3V3
- GPS GND → GND

No manual setup needed — `GPS_Init()` auto-configures the module on every boot. Commands are sent in this order with full ACK verification (UBX-ACK-ACK / ACK-NAK parsing + up to 3 retries per command, 50–100 ms spacing):

1. UBX-CFG-PRT at 9600 baud (best-effort, no ACK wait — the baudrate is switching mid-command, so any reply would come back on the new rate)
2. Switch UART4 to 38400 baud
3. **UBX-CFG-RATE** sent first because it's the most important: sets measurement period from `GPS_MEAS_PERIOD_MS` (derived from `GPS_RATE_HZ`, default 10 Hz)
4. UBX-CFG-MSG × 4 to disable GLL, GSA, GSV, VTG (only $GNRMC + $GNGGA remain enabled)
5. UBX-CFG-CFG to persist the config in BBR + Flash + EEPROM

The ACK status of every command is logged to a static buffer and emitted to the SD error log on the first START_LOG as `gps: rate=OK(10Hz) msg=OK save=OK` — post-session we can immediately see whether the module accepted the rate command or silently ignored it. Earlier versions sent the same commands without ACK parsing, so a dropped UBX packet would leave the module on its persisted 1 Hz config with no signal in the logs. `save=NAK` from UBX-CFG-CFG is non-blocking — the RAM layer of the config takes effect immediately, so 10 Hz output works even without successful flash persistence; flash persistence just means the config survives the next power cycle.

`gps_thread` polls the fix buffer at the module rate (`GPS_MEAS_PERIOD_MS / 10` ticks — for `GPS_RATE_HZ=10` that's 10 ticks = 100 ms between reads). Before this change the thread was hard-coded to `tx_thread_sleep(100) = 1 s`, which silently dropped 9 of every 10 fixes when the module was pushed to 10 Hz (observed symptom: `rate=OK(10Hz)` in the error log but GpsNNN.csv still showing 100-tick row deltas).

The firmware parses only `$GNRMC` and `$GNGGA` NMEA sentences and logs them to `GpsNNN.csv` (timestamp, UTC, lat, lon, alt, speed km/h, course, fix, num-sat, HDOP). With only two sentences enabled, 10 Hz fits in ~1.5 kB/s on the 38400-baud UART (well under the ~3.8 kB/s ceiling). `STBOX1_ENABLE_PRINTF` is disabled while the GPS occupies UART4 — fatal errors still land in the SD error log.

### <b>Firmware fingerprint in error log</b>

On boot the error log now includes a second line with the exact firmware identity, useful post-session for telling which binary ran:

```
--- Boot Apr 23 2026 10:15:02 ---
fw: build Apr 23 2026 10:15:02 | GPS 10Hz | AUDIO=0 BATTERY=1 | flash ~116KB
reset: POR (CSR=0x0C000000)
```

Flash footprint is computed at runtime from the `_sidata`/`_sdata`/`_edata` linker symbols. The compile-date `--- Boot ---` marker alone was identical across builds on the same day, so the `fw:` line is the one that disambiguates.

### <b>Checking which firmware is running</b>

Three ways, from trivial to definitive — useful in the field when verifying that an SD firmware update actually landed:

1. **Error log on the SD card (definitive).** Pull the card, open `Error_Log_Pump_Tsueri_dd.mm.yyyy.log`, read the `fw: build <date> <time> | ...` line — compare against the build you intended to flash. The filename itself carries the compile date for a quick first glance without opening the file.
2. **LED patterns during the SD update (live, build ≥ 24.4.2026 13:26).** If a `firmware.bin` is present on the card at boot, the 10× rapid green+red alternation (~2 s) confirms "detected + flashing", then 3× slow green blinks (~1.8 s) confirm "flash successful". These patterns only exist in builds from 24.4.2026 13:26 onward — seeing them proves you're at least on that build. See *LED behavior* below for details.
3. **Feature presence (indirect).** `BatNNN.csv` appearing on the card means the I²C4 timing fix is in (build ≥ 24.4.2026 12:53). If `BatNNN.csv` is missing and the error log shows `gas gauge init FAIL - continuing without battery log`, you're either on a pre-12:53 build or the STC3115 genuinely isn't answering.

### <b>Reset-reason in error log</b>

The third boot-marker line decodes `RCC->CSR` — set by the STM32U5 hardware to mark what caused the most recent reset — so field-test reboots can be told apart from user-initiated power-ups. `main()` snapshots `RCC->CSR` into `BootResetCsr` right after `HAL_Init()` and calls `__HAL_RCC_CLEAR_RESET_FLAGS()` so the *next* reset's flags are captured cleanly. `ErrorLog_Open()` decodes the snapshot into one of:

| Stamp | Meaning |
|---|---|
| `reset: POR` | clean power-on (both `BORRSTF` + `PINRSTF` set — battery just plugged in or main switch turned on) |
| `reset: BOR` | brown-out (supply dipped below the BOR threshold mid-session — typically weak battery under SD-write load, loose power connector) |
| `reset: PIN` | external NRST pulled low (reset button, glitch on the NRST net) |
| `reset: SOFTWARE` | firmware called `NVIC_SystemReset()` — or equivalent path from a HardFault via the default handler |
| `reset: IWDG` / `WWDG` | watchdog fired (both are disabled in this build, so should never appear) |
| `reset: LPWR` | illegal low-power mode exit |
| `reset: OBL` | option-byte loader reset (bank swap via firmware update) |

Multiple flags can set simultaneously — e.g. `POR` on a clean power-up. The raw `CSR=0x...` is appended so unusual combinations are still readable.

Motivation: on 23.4.2026 two field test sessions aborted after 53 s with no Error_Handler trace and a second `--- Boot ---` marker appearing immediately in the same log. Without reset-reason decoding we couldn't distinguish "user turned it off then on" from "battery brown-out killed us mid-session" from "firmware crashed". A `BOR` stamp on the second boot marker in the next such incident immediately pins the cause on the power supply (Task #24 STC3115 gauge init FAIL on the same session supports the weak-battery hypothesis).

The GPS fix rate is a build-time option. The Makefile exposes `GPS_RATE_HZ`:

```sh
make                 # default, 10 Hz
make GPS_RATE_HZ=5   # 5 Hz
make GPS_RATE_HZ=25  # 25 Hz (UART-budget ceiling at 38400 baud)
```

Values outside `[1, 25]` trigger a `#error` in `stbox1_config.h`. IDE builds get the 10 Hz default.

### <b>LED behavior</b>

On every boot the firmware emits a green-LED progress sequence (main.c `BootStageBlink()`):

- Brief red flash (power-on default, turned off in main)
- **1 green blink**: clock + ICache + LED init OK
- **2 green blinks**: `GPS_Init()` (UART4 + UBX auto-config) OK
- **3 green blinks**: sensor init OK
- **Green LED solid**: ThreadX + FileX running, logging active

If boot stops before the 3rd blink, one of the init steps hung — useful for narrowing down hardware/driver issues when `STBOX1_PRINTF` is compiled out. Red LED solid = either an SD firmware update in progress (`firmware.bin` on the card) or an early hang (clock / crystal / floating GPIO). Red LED blinking at ~2.5 Hz = `Error_Handler` — see `Error_Log_Pump_Tsueri_*.log` on the SD card for the source-file and line.

**Per-sensor diagnostic blinks inside `InitMemsSensors()` (26.4.2026).** Between the 2nd and 3rd green `BootStageBlink`, the firmware now emits a numbered burst of red blinks (`DiagBlinkRed(n)` in `main.c`) before each sensor init, with a 400 ms gap after each burst:

- **1×** red — entered `InitMemsSensors()`
- **2×** red — about to init LIS2MDL magnetometer (I²C2)
- **3×** red — about to init LSM6DSV16X acc/gyro (SPI)
- **4×** red — about to init LPS22DF pressure (I²C2)
- **5×** red — about to init STTS22H temperature (I²C2)
- **6×** red — all four sensor inits returned, function exiting

If boot hangs at "2 green + N×red + dark", N pins down which sensor's I²C/SPI transaction is blocking. Costs ~3 s of boot time but invaluable when serial debug is unavailable. Discovered Peter's box hang at 2-green-zero-red on 26.4.2026 — meaning `InitMemsSensors()` was never being called, which combined with the fact that newly DFU'd firmware kept exhibiting the same hang regardless of code changes pointed at a flash-bank/option-byte mismatch (the chip was running a stale firmware in one bank while DFU was writing to the other). See "Recovering from a stuck flash bank" below.

**FW_INFO_v<N>.TXT at SD-card root.** On every successful SD-mount the firmware writes a versioned fingerprint file `FW_INFO_v<N>.TXT` (where `<N>` is `FW_BUILD_NUM`) at the SD root, e.g. `FW_INFO_v10.TXT`:

```
fw: v10 build May  5 2026 09:50:12
GPS 10Hz | AUDIO=0 BATTERY=1 | flash ~147KB
```

Same content as the second line of the `--- Boot ---` marker in `Error_Log_Pump_Tsueri_*.log`. The version is in the **filename itself**, so the field tester can tell which firmware ran from the SD listing alone — no need to open any file. Especially useful after a firmware update to confirm DFU/SD-update actually took: if `FW_INFO_v<N+1>.TXT` doesn't appear in the listing after copying the new `firmware.bin`, the chip is running a stale firmware copy.

On every boot the firmware enumerates the SD root and deletes any pre-existing `FW_INFO*` file (the legacy fixed-name `FW_INFO.TXT` from before per-version naming, plus any older `FW_INFO_v*.TXT` from previously-flashed builds) so the listing always shows exactly one fingerprint file matching the running binary. Implementation: `WriteFwInfoFile()` in `app_filex.c`, called from `CheckAndApplyFirmwareUpdate()` right after `fx_media_open` succeeds. Two-pass enumeration (collect names first, delete after) so the FX directory iterator isn't invalidated mid-iteration; up to 8 stale entries cleaned per boot, generous headroom against any realistic flash history.

**Recovering from a stuck flash bank (26.4.2026).** STM32U585 has dual-bank flash with a `SWAP_BANK` option byte that selects which physical bank maps to address `0x08000000` at boot. Successful SD-updates toggle this byte, but the documented bank-swap-bug can leave `SWAP_BANK` in an inconsistent state. Symptoms when this happens:

- DFU reports a successful flash, but the chip continues to run an old firmware on every boot.
- Multiple DFU + SD-update + power-cycle attempts produce identical hang behaviour because the new firmware is being written to the *inactive* bank.
- The diagnostic blinks above show **zero red blinks**, even on a build that emits red blinks before any other init.

To recover, mass-erase both banks via `dfu-util` before the next flash:

```sh
# Hold user button + plug USB-C → DFU mode
dfu-util -d 0483:df11 -a 0 -s 0x08000000:mass-erase:force -D firmware.bin
# Power-cycle (USB out + back in, no button) → boot from cleanly programmed flash
```

The `:mass-erase:force` modifier wipes the entire flash (both banks) before downloading, so after the operation the chip has only one firmware copy and the bank-mapping question becomes irrelevant. STM32CubeProgrammer's "Full Chip Erase" achieves the same outcome via the GUI.

**SD firmware update visual feedback (24.4.2026).** Two distinct LED patterns frame `CheckAndApplyFirmwareUpdate()` so the field tester has direct confirmation that the update ran without needing serial or SD-log access:

- **Firmware detected**: 10× rapid green+red alternation (~2 s total), fired the moment `firmware.bin` is successfully opened. Very distinct from the 1/2/3-green `BootStageBlink` pattern, so you can tell at a glance whether the board is entering an SD-update or booting straight into logging.
- **During programming**: red LED toggles every ~512 B chunk read from the SD card (progress).
- **Flash successful**: 3× slow green blinks (~1.8 s) after the programming loop finishes, right before the option-byte bank-swap + reset. Clear "it worked" signal distinct from both the rapid detected pattern and the steady post-boot green "logging active".

**`firmware.bin` built directly (24.4.2026).** The Makefile `all` target now emits `build/firmware.bin` alongside `build/SDDataLogFileX.bin`. The SD-update code in `app_filex.c` looks for exactly that filename, so the build output can be dropped onto the SD card as-is without renaming. Peter was hitting the rename step on every flash and missed it at least once, which left him testing the previous firmware version for an entire session.

### <b>Keywords</b>

NFC, SPI, I2C, UART, MEMS, BLE, BLE_Manager, STM32WB07KC, GPS

### <b>Hardware and Software environment</b>

- This example runs on STEVAL-MKBOXPRO (SensorTile.box-Pro) evaluation board and it can be easily tailored to any other supported device and development board.

ADDITIONAL_COMP : [STTS22H](https://www.st.com/en/mems-and-sensors/stts22h.html)

ADDITIONAL_COMP : [LPS22DF](https://www.st.com/en/mems-and-sensors/lps22df.html)

ADDITIONAL_COMP : [LSM6DSV16X](https://www.st.com/en/mems-and-sensors/lsm6dsv16x.html)

ADDITIONAL_COMP : [LIS2DU12](https://www.st.com/en/mems-and-sensors/lis2du12.html)

ADDITIONAL_COMP : [LIS2MDL](https://www.st.com/content/st_com/en/products/mems-and-sensors/e-compasses/lis2mdl.html)

ADDITIONAL_COMP : [MP23DB01HP](https://www.st.com/en/mems-and-sensors/mp23db01hp.html)

ADDITIONAL_COMP : [ST25DV64KC](https://www.st.com/en/nfc/st25dv64kc.html)

ADDITIONAL_COMP : [STM32WB07KC](https://www.st.com/en/microcontrollers-microprocessors/stm32wb07kc.html)

### <b>First-time setup: BlueNRG-LP OTP programming (one-shot per box)</b>

The on-board BlueNRG-LP BLE chip needs its OTP (One-Time-Programmable) memory burned with a BLE stack patch (`dtm.bin`, 200 KB) before this firmware's `bluetooth_init()` can talk to it. Factory boxes usually already have the OTP programmed, but on hardware-modified boxes (e.g. 3.3V-rail mod for the GPS daughterboard) the chip can be in an unprogrammed state — `bluetooth_init()` then silently fails, BLE doesn't advertise, and on the 3.3V-modded box the chip's undefined behaviour previously even crashed SDMMC writes (issue #12).

**This firmware does not OTP-program the BlueNRG-LP itself** (TODO — the WLC SPI flasher source from ST's `STSW-BNRGLP-DK` package needs to be integrated). One-shot recovery procedure per box:

1. Put `dtm.bin` (BlueNRG-LP stack image, from ST) on the SD-card root.
2. DFU-flash the **ST original firmware** (`SensorTile.boxPRO.bin`, also from ST) onto the STM32U585.
3. Power-cycle. The ST firmware reads `dtm.bin` from SD, writes it via SPI into the BlueNRG-LP RAM, boots the chip from RAM, and burns the OTP fuses. Permanent — survives all subsequent reflash/reset cycles.
4. (Optional) Verify by connecting with the *ST BLE Sensor* phone app — connection success means the chip is properly programmed.
5. Delete `dtm.bin` from SD, DFU-flash this firmware. BLE FileSync now works alongside SDMMC logging.

The OTP is one-shot. Subsequent firmware updates (DFU or via SD-card path) don't need steps 1–3 repeated — flash the new `firmware.bin` and the chip stays good forever.

`dtm.bin` and `SensorTile.boxPRO.bin` are not part of this repository; pull both from st.com (`STSW-BNRGLP-DK` for `dtm.bin`, the SensorTile.box PRO factory firmware page for `SensorTile.boxPRO.bin`). See top-level `README.md` "First-time BlueNRG-LP OTP setup" for context and CLAUDE.md "BlueNRG-LP OTP / WLC flashing" for the technical detail.

### <b>Known Issues</b>

- The firmware doesn't suite with STM32CubeMX
- Beware of a warning on STM32CubeIDE v1.18.1 during compilation: "SDDataLogFileX.elf has a LOAD segment with RWX permissions"
- BlueNRG-LP OTP programming has to be done once manually per box via ST original firmware — see "First-time setup" section above. Integrating ST's WLC SPI flasher to make this self-healing is a TODO.

### <b>Dependencies</b>

STM32Cube packages:

  - STM32U5xx drivers from STM32CubeU5 V1.7.0
  
STEVAL-MKSBOX1V1:

  - STEVAL-MKBOXPRO V1.5.0

### <b>How to use it?</b>

This package contains projects for 3 IDEs viz- IAR, Keil µVision 5 and Integrated Development Environment for STM32.
In order to make the  program work, you must do the following:

 - WARNING: before opening the project with any toolchain be sure your folder
   installation path is not too in-depth since the toolchain may report errors
   after building.

For IAR:

 - Open IAR toolchain (this firmware has been successfully tested with Embedded Workbench V9.60.3).
 - Open the IAR project file on EWARM directory
 - Rebuild all files and Flash the binary on STEVAL-MKBOXPRO

For Keil µVision 5:

 - Open Keil µVision 5 toolchain (this firmware has been successfully tested with MDK-ARM Professional Version: 5.38.0).
 - Open the µVision project file on MDK-ARM directory
 - Rebuild all files and Flash the binary on STEVAL-MKBOXPRO
		
For Integrated Development Environment for STM32:

- Open STM32CubeIDE (this firmware has been successfully tested with Version 1.18.1)
 - Set the default workspace proposed by the IDE (please be sure that there are not spaces in the workspace path).
 - Press "File" -> "Import" -> "Existing Projects into Workspace"; press "Browse" in the "Select root directory" and choose the path where the STM32CubeIDE project is located (it should be STM32CubeIDE\).
 - Rebuild all files and and Flash the binary on STEVAL-MKBOXPRO
   
### <b>Author</b>

SRA Application Team

### <b>License</b>

Copyright (c) 2025 STMicroelectronics.
All rights reserved.

This software is licensed under terms that can be found in the LICENSE file
in the root directory of this software component.
If no LICENSE file comes with this software, it is provided AS-IS.
