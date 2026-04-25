# stbox-viz

Rust replacement for the Python pumpfoil-session visualisers under
`Utilities/scripts/`. Reads a sensor CSV + auto-detected GPS CSV from an
SD-card recording, runs Madgwick 6DOF fusion, detects rides from sustained
GPS movement, computes board height above water from the temperature-
compensated + GPS-anchored barometer, and emits an interactive Plotly HTML.

## Build

```sh
cd Utilities/rust/stbox-viz
cargo build --release
```

Binary lands at `target/release/stbox-viz`.

## Usage

```sh
./target/release/stbox-viz combined ../../../csv/peter_22.4.2026_1250.csv -o ../../../html/
```

The GPS CSV is picked up automatically as `<stem>_gps.csv` next to the
sensor CSV. Output filename is `viz_<stem>.html`.

### Time-axis options (`combined`)

By default the x-axis runs in UTC anchored to the GPS clock. Use:

- `--tz-offset-h <h>` — shift x-axis to local time. `3` for Ermioni / Greek
  summer (EEST), `2` for Swiss summer (CEST), `1` for Swiss winter (CET).
  Axis title and ride-list times reflect the chosen offset.
- `--date YYYY-MM-DD` — override the recording date. Defaults to the
  sensor file's mtime; pass this if `cp`/`mv` reset the mtime to today.

Time-of-day always reads off the GPS clock — the `--date` flag only
affects the rendered date string in datetime tooltips.

Plotly.js is loaded from the CDN (same as the Python version's
`include_plotlyjs='cdn'`), so the HTML needs internet the first time it's
opened in a browser.

### Wall-clock-aligned animation (`animate --at`)

The default `animate` subcommand finds pumping sessions by pitch
oscillation and renders one GIF per session. To instead pin the
animation to an exact wall-clock time (e.g. to align with external
camera footage), use `--at`:

```sh
./target/release/stbox-viz animate ../../../csv/ayano_25.4.2026_s0.csv \
    --at 10:16 --tz-offset-h 3 --date 2026-04-25 \
    --video /path/to/IMG_1851.MOV \
    --title "Ayano · Ermioni · 25.4.2026" \
    --subtitle "Ride 8 · 10:16 EEST"
```

Flags:

- `--at HH:MM[:SS]` — wall-clock start of the GIF window (in local time
  per `--tz-offset-h`). Bypasses pitch-oscillation session detection.
- `--duration <s>` — window length. Defaults to the video's full length
  when `--video` is given, else 60 s.
- `--video <path>` — overlay the camera video next to the GIF via ffmpeg
  hstack. The video's `creation_time` is probed and printed; pass that
  value back as `--at HH:MM:SS` for millisecond-precise alignment.
- `--auto-skip` — advance both video and GIF past the carry/transition
  seconds before sustained pitch oscillation begins. Off by default.
- `--tz-offset-h`, `--date` — same semantics as `combined`.

The GIF has 5 panels when GPS is available (board side view + pump
detail + height-over-water + speed + nasenwinkel), 3 otherwise. Each
panel's y-axis grows with the running max — no wasted vertical space
above the data. The board side view scrolls a sinusoidal waterline
backward at 0.6 m/s so the board appears to be travelling forward
over the water while it pumps; lift comes from the smoothed baro
height. Panel titles are horizontal (their own title strip above
each chart). The Nasenwinkel panel uses a 95th-percentile-based
y-limit so single-outlier pumps don't stretch the axis.

## Why Rust

- Single binary vs Python + venv + pandas + plotly + numpy + scipy
- Strong typing at the CSV boundary — the 22.4.2026 `Time [mS]` → `Time
  [10ms]` rename broke both Python scripts; `serde`-strict column parsing
  would have flagged it at compile time
- Faster fusion + rolling-median on long sessions (~10× on 2 h recordings)
- Matches the rest of the `~software/` stack, which is mostly Rust

## Scope

Phase 1 (this version): feature-parity with `visualize_combined.py` minus
the checkbox ride isolation and URL anchors. Future phases will port
`visualize_sensors`, `visualize_pumpfoil`, `animate_board_3d` and then
delete the Python under `Utilities/scripts/`.
