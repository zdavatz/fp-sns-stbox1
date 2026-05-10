# Handoff for zeno — BLE FileSync bringup, 10 May 2026

Peter is at training camp today and asked me (Claude) to package the current
state so you can pick up.

## TL;DR

We've been trying for weeks to get BLE working in SDDataLogFileX. As of last
night we proved that BLE works fine in BLEDualProgram on the same hardware,
so the BlueNRG-LP chip and the SPI/IRQ wiring are fine — the bug is
SDDataLogFileX-specific.

Yesterday I localized the hang to a deterministic point in the very first
HCI command (hci_reset). 7 boots in a row, all hang at exactly the same
log marker. Diagnosed it last night to a probable **missed-rising-edge after
spi_reset** issue. **Fix applied this morning as v35** — pre-built binary
ready in `~/Downloads/firmware-v35-spi-drain-fix.bin`, SHA1
`6660c30d80c020970b17c0fd9a2796c9c0918f7b`.

If v35 works → done after weeks. If not → see fallback section below.

## Read first

- **`CLAUDE.md`** at the repo root — long-term project context. Read sections
  "BLE FileSync — download SD-card files over Bluetooth (SDDataLogFileX)"
  and "BLE EXTI11 IRQ-storm fix + ThreadX byte-pool sizing".
- **Branch:** `fix/ble-tx-pool-and-halt-diag` (based on `main`)
- **Last 6 commits** are mine, see `git log -6`. Working tree has
  uncommitted v35 changes (see "Files modified for v35" below).

## The hypothesis (why I made the v35 fix)

After a clean spi_reset, the chip's IRQ line goes HIGH because the BlueNRG-LP
posts a "boot ready" event with pending bytes. We mask EXTI11 during the
reset+settle window (150 ms total) so the rising edge happens while we
can't see it.

EXTI11 is **rising-edge triggered**, so once we re-enable it, the
already-high line never produces a fresh edge. The ISR never fires. The
chip's pending bytes stay in its TX FIFO. Subsequent events (e.g. our
hci_reset's Command Complete response) try to raise the IRQ — it's already
high, no rising edge, no ISR. The host's `hci_send_req` busy-waits forever
on an empty rx_queue.

Smoking-gun evidence in `Error_Log_Pump_Tsueri_10.05.2026.log` (on the
last SD card I read at 00:44 UTC):
```
spi_send: header xfer rc=0 slave[1..4]=1c 02 07 00
                                       ^^^^^ ^^^^^
                                       RBUFLEN=540  WBUFLEN=7
```
The `slave[3..4] = 07 00` shows the chip already has 7 bytes pending for
us **before we send any HCI command**.

## The fix (what v35 does)

In `Projects/STEVAL-MKBOXPRO/Applications/Rev_C/SDDataLogFileX/Core/Src/hci_tl_interface.c`,
function `hci_tl_spi_reset()`:

After re-enabling EXTI11, while `is_data_available()` returns true, call
`hci_notify_asynch_evt(NULL)` (the middleware's drain primitive). Capped at
32 events to bound time. Logs `spi_reset: drained N boot events` so we can
verify in the SD error log whether the drain found anything.

Code: see the diff in `hci_tl_interface.c` around `int32_t hci_tl_spi_reset(void)`.

## Test procedure for v35

1. Plug box into Mac via USB-C with **user button held down** to enter DFU.
2. Flash:
   ```sh
   dfu-util -d 0483:df11 -a 0 \
     -s 0x08000000:mass-erase:force \
     -D ~/Downloads/firmware-v35-spi-drain-fix.bin
   ```
   `:mass-erase:force` to bypass the bank-swap bug (see CLAUDE.md
   "Stale-flash-bank recovery").
3. Power cycle (USB out, USB in without button).
4. Wait at least 30 seconds for the BLE thread to wake (5 s sleep) and run.
5. Open BLE scanner on Mac or Android. Look for **`STBoxFs`**.
6. **Always pull the SD card** and check both:
   - `FW_INFO.TXT` — first line should be `fw: v35-spi-drain-fix build May 10 2026 ...`
     (otherwise v35 didn't actually flash; bank-swap might have failed)
   - `Error_Log_Pump_Tsueri_10.05.2026.log` — look for the new line
     `spi_reset: drained N boot events`. If `N > 0`, the hypothesis was
     correct (chip had pending data we never read). If `N == 0`, the
     hypothesis is wrong but the fix should still be a no-op.
7. If `STBoxFs` shows up in scanner: try connecting (PIN `123456`).
   See CLAUDE.md "BLE FileSync" section for the GATT protocol.

### Expected outcomes

| Scenario | What you'll see |
|---|---|
| Fix works (likely) | Error log: `drained N` with N>=1, BLE scanner shows `STBoxFs`, advertising stays up |
| Drain happened, BLE still not visible | `drained N` with N>=1 but no scanner hit — bug is downstream, not the EXTI edge issue |
| Drain found nothing | `drained 0` — hypothesis wrong, hang must be elsewhere |
| Same hang as v34 | Log stops at `spi_send: payload xfer rc=0` like before — chip is responsive but our drain didn't help; investigate ISR wiring |

## If v35 doesn't fix it

Three avenues to investigate:

### Avenue A: ISR not firing despite drain
Check `hci_tl_lowlevel_isr` in `Core/Src/hci_tl_interface.c` — it's our
ISR handler. Compare to BLEDualProgram's version in
`Projects/STEVAL-MKBOXPRO/Applications/Rev_C/BLEDualProgram/Src/hci_tl_interface.c`.

### Avenue B: ThreadX-specific issue
The ThreadX scheduler may be eating something. Try adding
`tx_thread_relinquish()` to the HCI wait loop in the middleware (NOT
recommended yet — Peter and I tried that earlier and it didn't help).

Or — the cleanest path — port the BLEDualProgram baseline forward (which
is "Pfad II" we'd discussed). Background:

- We made a CLI Makefile for BLEDualProgram that builds + flashes a
  working BLE binary. See
  `Projects/STEVAL-MKBOXPRO/Applications/Rev_C/BLEDualProgram/STM32CubeIDE/Makefile`.
  `cd <dir> && make` produces `build/firmware.bin`. Verified yesterday:
  shows up as `FFoTABP` on BLE scanner. So BLE *radiates* in this
  baseline.
- The full port of the SD logger + GPS + sensor threads onto the
  BLEDualProgram base is a 1-2 week effort but bounded.

### Avenue C: ST FAE writeup
Peter said he'd post on the ST community forum on Monday. If that yields
a quick "you're missing aci_X" answer, we save the port. Watch
`https://community.st.com/` for any responses to a thread Peter creates.

## Files modified for v35 (uncommitted, in working tree)

```
M Projects/.../SDDataLogFileX/Core/Src/hci_tl_interface.c   # The drain fix
M Projects/.../SDDataLogFileX/Core/inc/stbox1_config.h      # DIAG_VERSION = "v35-spi-drain-fix"
```

Also still in working tree from earlier sessions (don't revert these):
```
M Middlewares/ST/STM32_BLE_Manager/Src/ble_manager.c        # Just diagnostic markers, harmless
M Middlewares/ST/STM32WB07_06/hci/hci_tl_patterns/Basic/hci_tl.c   # Reverted to clean ST code last night
M Projects/.../SDDataLogFileX/Core/Src/main.c               # Various boot markers, no funcl change
M Projects/.../SDDataLogFileX/Core/Src/ble_sync.c           # BLE thread structure
M Projects/.../SDDataLogFileX/Core/Src/stm32u5xx_hal_msp.c  # SMPS+VDDIO2 config
M Projects/.../SDDataLogFileX/Core/Src/stm32u5xx_hal_timebase_tim.c   # TIM6 priority 13
M Projects/.../SDDataLogFileX/Core/inc/ble_implementation.h # BLE_FW_PACKAGENAME = "STBoxFs"
M Projects/.../SDDataLogFileX/Core/inc/ble_manager_conf.h
M Projects/.../SDDataLogFileX/AZURE_RTOS/App/app_azure_rtos.c   # Note in CLAUDE.md re: byte-pool sizing
M Projects/.../SDDataLogFileX/FileX/App/app_filex.c         # Boot markers + battery + GPS + flush logic
```

## Quick rebuild instructions

```sh
cd Projects/STEVAL-MKBOXPRO/Applications/Rev_C/SDDataLogFileX/STM32CubeIDE
make clean && make
# Output: build/firmware.bin (and build/SDDataLogFileX.bin, identical)
```

The Makefile is at `STM32CubeIDE/Makefile` and uses
`arm-gnu-toolchain` from `~/.software/arm-gnu-toolchain/bin/`. If toolchain
is elsewhere, override: `make TOOLCHAIN=/other/path`.

For the BLE-only BLEDualProgram baseline:
```sh
cd Projects/STEVAL-MKBOXPRO/Applications/Rev_C/BLEDualProgram/STM32CubeIDE
make clean && make
```

## Useful pointers

- **CLAUDE.md** at repo root — comprehensive project docs (~600 lines)
- **HANDOFF.md** — this file
- Conversation transcript with Claude: `~/.claude/projects/-Users-peterschmidlin-Documents-software-fp-sns-stbox1/*.jsonl`
- Pre-built binaries in `~/Downloads/`:
  - `firmware-v35-spi-drain-fix.bin` — SDDataLogFileX with the fix (test this first)
  - `firmware-bledualprogram-clean.bin` — BLEDualProgram known-good baseline
- Box hardware: SensorTile.box PRO Rev_C, STM32U585 host + BlueNRG-LP/STM32WB07_06 BLE chip on SPI1

## What "success" looks like

- Box flashes v35
- Boots, 3 green LED blinks, then solid green
- BLE scanner shows `STBoxFs` device
- Connect to it (PIN 123456) — see two GATT characteristics: FileCmd
  (`00000080-0010-11e1-ac36-0002a5d5c51b`, write) and FileData
  (`00000040-0010-11e1-ac36-0002a5d5c51b`, notify)
- Write `01` to FileCmd → FileData notifies with `Sens000.csv,28175\n...`
  listing of SD-card root

If you reach that point, message Peter — that's the milestone we've been
chasing for weeks.

Good luck.
