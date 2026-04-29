# MovementLogger

Cross-platform (Win/Mac/Linux) drag-and-drop GUI for SensorTile.box pumpfoil session telemetry. Drop a sensor CSV (auto-pairs the matching `_gps.csv`), an optional camera `.mov`/`.mp4`, and an optional board `.stl`; fill in the few optional fields; click **Generate**. The GUI shells out to the bundled `stbox-viz animate` CLI to render the annotated session video.

## Window layout

- **OS title bar** shows `MovementLogger <version>` (and the app icon — Dock on macOS, taskbar on Win/Linux). The version is read from `env!("CARGO_PKG_VERSION")` so it always matches the binary you're looking at.
- **Top strip** (inside the window):
  - Left: a hyperlink `SensorTile.box pumpfoil session video generator` → opens [the project on GitHub](https://github.com/zdavatz/fp-sns-stbox1) in the default browser.
  - Right: the MovementLogger logo. Clicking it launches the default mail client composing to `zdavatz@ywesee.com` — quick path for field testers to send a bug report or a session that didn't render right.

## Drop zone

| File pattern | Routed to |
|---|---|
| `Sens*.csv` (or any `.csv` not ending in `_gps.csv`) | Sensor input |
| `*_gps.csv` | GPS input (auto-paired by stem when a sensor is dropped first) |
| `.mov` / `.mp4` / `.mkv` / `.m4v` / `.avi` | Camera video |
| `.stl` | Board mesh for the 3D foil overlay |

Drop multiple files at once — the classifier routes each to the right slot.

## Form fields

| Field | Maps to | Notes |
|---|---|---|
| Start time | `--at HH:MM:SS` | Local-time anchor for the GPS-tick slicer. Empty = pitch-oscillation auto-detect. |
| Timezone offset | `--tz-offset-h` | 3 = Greek summer (EEST), 2 = Swiss summer (CEST), 1 = Swiss winter (CET). |
| Date | `--date YYYY-MM-DD` | Empty = sensor file mtime. |
| Duration | `--duration` | Seconds; 0 = video length / 60 s fallback. |
| Mount | `--mount mast \| deck` | Picks `R_mount`, camera angle and the 3D-attitude path. |
| Dock height | `--dock-height-m` | Metres; 0.75 for the Ermioni harbour wall. |
| Auto-skip | `--auto-skip` | Skip carry/transition seconds at start. |
| Title / Subtitle | `--title` / `--subtitle` | Title-card overlay text. |
| FPS | `--fps` | Default 15. |
| Output folder | `-o` | Default `gif/` next to where MovementLogger was launched. |

## How the binaries are wired

The GUI is intentionally thin: it builds a `stbox-viz animate …` command line, spawns it as a child process, streams stdout/stderr into the live log panel, and lets the user cancel mid-run.

`stbox-viz` lookup order:
1. Next to the GUI executable (`current_exe()` parent dir).
2. PATH.

The release archives ship both binaries side by side, and the macOS `.app` bundles `stbox-viz` inside `Contents/MacOS/`, so step 1 always succeeds for installed builds.

`ffmpeg` must be in PATH for `--video` mode (`stbox-viz animate` shells out to it for the `hstack=inputs=2` side-by-side MOV).

## Building locally

```sh
cd Utilities/rust
cargo build --release -p movement-logger -p stbox-viz
./target/release/MovementLogger
```

After every release build on macOS, verify the App-Store-critical patch held:

```sh
nm target/release/MovementLogger | grep CGSSetWindowBackgroundBlur
# Must return nothing.
```

If the symbol comes back, it means the `[patch.crates-io] winit` entry in the workspace `Cargo.toml` no longer matches the winit version eframe pulls in transitively (eframe 0.28 dragged in winit 0.29, which our 0.30 fork did not cover; eframe 0.29+ is fine). See the workspace `CLAUDE.md` for the patch details.

## Releases

Push a `vX.Y.Z` tag and the `.github/workflows/release.yml` workflow builds tarballs/zips for Linux x86_64 + aarch64, macOS Intel + Apple Silicon, and Windows x86_64, plus a macOS `.app` bundle inside the macOS tarballs. Optional store paths (Mac App Store `.pkg` upload via altool, Windows MSIX submission via the devcenter REST API) are gated on `vars.MACOS_STORE_ENABLED` and `vars.MSSTORE_ENABLED` respectively, and reuse the same secret names as the rust2xml repo.
