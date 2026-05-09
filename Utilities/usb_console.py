#!/usr/bin/env -S uv run --quiet --script
# /// script
# requires-python = ">=3.10"
# dependencies = [
#   "pyusb>=1.2",
# ]
# ///
"""
USB CDC console for the SensorTile.box PRO debug build.

Bypasses macOS's CDC ACM driver (which fails to attach on Sequoia 15+ for
TinyUSB descriptors) by using libusb directly. Opens the bulk-IN endpoint
0x82 and streams the firmware's printf output to stdout.

Run:
    uv run usb_console.py

Or, if you don't have uv:
    python3 -m venv .venv && source .venv/bin/activate
    pip install pyusb
    python3 usb_console.py

Requires libusb on the host (macOS: `brew install libusb`).

Press Ctrl-C to exit.
"""
from __future__ import annotations

import sys
import time

import usb.core
import usb.util
import usb.backend.libusb1
import ctypes.util
import os

# pyusb's auto-discovery can't find Homebrew's libusb on Apple Silicon
# (DYLD_LIBRARY_PATH isn't honored by ctypes.util.find_library on recent
# macOS for /opt/homebrew/lib). Look in a few well-known places and load
# the backend explicitly.
def _make_backend():
    # Try several known libusb locations in order. We don't pre-check
    # existence with os.path.exists/lexists — sandboxed shells can lie
    # about paths even when ctypes can dlopen them just fine. Let
    # pyusb's libusb1.get_backend() do the actual load and surface the
    # first one that works.
    candidates = []
    if os.environ.get("LIBUSB_PATH"):
        candidates.append(os.environ["LIBUSB_PATH"])
    candidates += [
        "/opt/homebrew/lib/libusb-1.0.0.dylib",  # Apple Silicon Homebrew
        "/usr/local/lib/libusb-1.0.0.dylib",     # Intel Homebrew
    ]
    # Walk Homebrew Cellar to find the actual versioned dylib (sandboxed
    # shells sometimes block the symlink in /opt/homebrew/lib but allow
    # the Cellar path).
    for cellar in ("/opt/homebrew/Cellar/libusb", "/usr/local/Cellar/libusb"):
        if os.path.isdir(cellar):
            for ver in sorted(os.listdir(cellar), reverse=True):
                p = f"{cellar}/{ver}/lib/libusb-1.0.0.dylib"
                candidates.append(p)
    found = ctypes.util.find_library("usb-1.0") or ctypes.util.find_library("libusb-1.0")
    if found:
        candidates.append(found)
    for path in candidates:
        if not path:
            continue
        backend = usb.backend.libusb1.get_backend(find_library=lambda x, p=path: p)
        if backend is not None:
            return backend
    return None

# Match the descriptor in Core/Src/usb_descriptors.c.
VID = 0xCAFE
PID = 0x4001

# CDC bulk endpoints from the data interface (Core/Src/usb_descriptors.c).
EP_BULK_IN = 0x82
DATA_INTERFACE = 1

# Read in chunks; CDC bulk packets are 64 B at FS so this is comfortably
# above one packet and lets a busy printf burst land in one read call.
READ_SIZE = 256
READ_TIMEOUT_MS = 250


def main() -> int:
    backend = _make_backend()
    if backend is None:
        print("error: libusb-1.0 not found. Install with: brew install libusb", file=sys.stderr)
        return 1

    dev = usb.core.find(idVendor=VID, idProduct=PID, backend=backend)
    if dev is None:
        print(f"error: no device found with VID:PID {VID:04x}:{PID:04x}", file=sys.stderr)
        print("       is the box plugged in and the USB CDC firmware running?", file=sys.stderr)
        return 1

    # macOS can refuse string-descriptor reads on a USB device that has
    # no kernel driver attached — wrap each one. We've already matched on
    # VID/PID so the device identity is known regardless.
    def _safe(getter):
        try:
            return getter() or "?"
        except Exception:
            return "?"
    print(
        f"connected: {_safe(lambda: dev.manufacturer)} / {_safe(lambda: dev.product)}"
        f" (serial {_safe(lambda: dev.serial_number)})",
        file=sys.stderr,
    )

    # macOS does NOT attach a kernel driver to our device on Sequoia 15+
    # (the whole reason this script exists), so detach_kernel_driver would
    # raise — we wrap it. Linux does attach cdc_acm so the detach IS needed.
    try:
        if dev.is_kernel_driver_active(DATA_INTERFACE):
            dev.detach_kernel_driver(DATA_INTERFACE)
            print(f"detached kernel driver from interface {DATA_INTERFACE}", file=sys.stderr)
    except (NotImplementedError, usb.core.USBError):
        pass

    # SET_CONFIGURATION 1 — TinyUSB exposes only one config. macOS often
    # leaves the device unconfigured if no driver attaches; we force it.
    try:
        dev.set_configuration(1)
    except usb.core.USBError as e:
        # If already configured, set_configuration may raise "Resource busy".
        # Carry on — the bulk read will tell us if the EP is alive.
        print(f"set_configuration: {e}", file=sys.stderr)

    try:
        usb.util.claim_interface(dev, DATA_INTERFACE)
    except usb.core.USBError as e:
        print(f"claim_interface: {e}", file=sys.stderr)
        return 2

    print("--- streaming bulk IN (Ctrl-C to exit) ---", file=sys.stderr)
    sys.stdout.write("")  # ensure stdout open
    sys.stdout.flush()

    try:
        while True:
            try:
                data = dev.read(EP_BULK_IN, READ_SIZE, timeout=READ_TIMEOUT_MS)
            except usb.core.USBTimeoutError:
                continue
            except usb.core.USBError as e:
                # Pipe stall, disconnect, etc. — print and exit.
                print(f"\nbulk read error: {e}", file=sys.stderr)
                return 3
            if not data:
                continue
            sys.stdout.buffer.write(bytes(data))
            sys.stdout.flush()
    except KeyboardInterrupt:
        print("\n--- exit ---", file=sys.stderr)
        return 0
    finally:
        try:
            usb.util.release_interface(dev, DATA_INTERFACE)
        except usb.core.USBError:
            pass


if __name__ == "__main__":
    raise SystemExit(main())
