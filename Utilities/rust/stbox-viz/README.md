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
./target/release/stbox-viz ../../../csv/peter_22.4.2026_1250.csv -o ../../../html/
```

The GPS CSV is picked up automatically as `<stem>_gps.csv` next to the
sensor CSV. Output filename is `viz_<stem>.html`.

Plotly.js is loaded from the CDN (same as the Python version's
`include_plotlyjs='cdn'`), so the HTML needs internet the first time it's
opened in a browser.

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
