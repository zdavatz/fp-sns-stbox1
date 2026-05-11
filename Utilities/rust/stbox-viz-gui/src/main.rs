//! MovementLogger — drag-and-drop GUI front-end for `stbox-viz animate`.
//!
//! Drop a sensor CSV (and optionally its companion `_gps.csv`, a camera
//! `.mov`/`.mp4`, and a `.stl` board mesh) onto the window, fill in the
//! handful of optional fields, hit Generate, and the GUI shells out to
//! the bundled `stbox-viz` CLI to produce a side-by-side MOV.
//!
//! The work is intentionally delegated to the existing CLI binary
//! rather than re-linking plotters + rustfft + gif into the GUI. Both
//! binaries ship in the same release archive (and the same .app bundle
//! on macOS, next to MovementLogger inside Contents/MacOS/), so the
//! GUI finds the CLI by looking next to its own executable first and
//! falling back to PATH.

#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

mod ble;

use eframe::egui;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::sync::{
    atomic::{AtomicBool, Ordering},
    mpsc, Arc, Mutex,
};
use std::thread;

use ble::{BleBackend, BleCmd, BleEvent};

/// Bundled-into-binary 512×512 PNG. The build pipeline already
/// generates this file at the canonical asset path; including it as
/// bytes means the standalone binary doesn't depend on any external
/// file at run time. egui rasterises on demand for whichever pixel
/// size the layout asks for.
const ICON_PNG: &[u8] = include_bytes!("../assets/icon.png");

// ---------------------------------------------------------------------------
//  App state
// ---------------------------------------------------------------------------

#[derive(Default)]
struct AppState {
    /// Auto-detected sensor CSV (e.g. `Sens005.csv`). One per session.
    sensor_csv: Option<PathBuf>,
    /// Auto-detected GPS CSV (e.g. `Sens005_gps.csv`). Optional — when
    /// missing, animate falls back to pitch-oscillation session
    /// detection only.
    gps_csv: Option<PathBuf>,
    /// Optional camera video paired with the session.
    video: Option<PathBuf>,
    /// Optional board STL mesh (defaults to fingerfoil's
    /// `0_combined.stl` if the user has it).
    board_stl: Option<PathBuf>,

    /// Output directory for the generated GIF + MOV.
    output_dir: PathBuf,

    // ----- Optional flags surfaced in the UI ----------------------------
    /// Wall-clock start time, e.g. `10:16:09`. Empty = auto-detect via
    /// pitch-oscillation session detector (no video alignment).
    at: String,
    /// Local-time UTC offset in hours. 3 for Greek summer (EEST), 2 for
    /// Swiss summer (CEST), 1 for Swiss winter (CET).
    tz_offset_h: f64,
    /// YYYY-MM-DD recording date. Empty = sensor file mtime.
    date: String,
    /// Window length in seconds. 0 = derive from video length / 60 s
    /// fallback.
    duration_s: f64,
    /// Mast-mount or deck-mount. Picks `R_mount`, camera angle and the
    /// 3D-attitude path inside `stbox-viz animate`.
    mount: Mount,
    /// Dock height above water in metres (Ermioni harbour wall = 0.75).
    dock_height_m: f64,
    /// Skip the carry/transition seconds before sustained pitch
    /// oscillation begins.
    auto_skip: bool,
    /// Title-card overlay text.
    title: String,
    subtitle: String,
    /// Output frame rate. Default 15 fps — anything above 20 makes the
    /// GIF huge without visibly improving the trace.
    fps: u32,

    // ----- Run-time state ----------------------------------------------
    /// Live log lines streamed from the child process.
    log: Arc<Mutex<Vec<String>>>,
    /// Cancellation flag — flipped by the Stop button.
    cancel: Arc<AtomicBool>,
    /// True while a child is alive.
    running: Arc<AtomicBool>,
    /// Last subprocess exit status, displayed in the status bar.
    last_status: Option<RunStatus>,

    /// Cached GPU texture for the top-right logo. Lazily uploaded on
    /// first frame so the egui context is available.
    icon_tex: Option<egui::TextureHandle>,

    // ----- BLE FileSync state -------------------------------------------
    /// Worker-thread backend. Lazily spawned on first BLE button click
    /// so the tokio runtime doesn't start unless the user actually opens
    /// the panel.
    ble: Option<BleBackend>,
    /// Discovered PumpTsueri peripherals from the most recent scan.
    ble_devices: Vec<BleDevice>,
    /// Connection lifecycle state — drives which buttons are enabled.
    ble_state: BleState,
    /// File listing returned by the most recent LIST. Each row carries
    /// a checkbox the user can flip before hitting "Download selected".
    ble_files: Vec<BleFile>,
    /// One-line status badge (last `Status`/`Error` event from the worker).
    ble_status: String,
    /// Where downloaded files land. Defaults to a `csv/` subfolder so
    /// they slot straight into the existing visualisation path.
    ble_out_dir: PathBuf,
    /// Session duration (seconds) the user types into the "Start session"
    /// field. Sent as the START_LOG payload (issue #15). Default 1800 s
    /// = 30 minutes.
    ble_session_duration_s: u32,
}

#[derive(Clone, Debug)]
struct BleDevice {
    id: String,
    name: String,
    rssi: Option<i16>,
}

#[derive(Clone, Debug)]
struct BleFile {
    name: String,
    size: u64,
    selected: bool,
    downloaded: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum BleState {
    Idle,
    Scanning,
    Connecting,
    Connected,
}

impl Default for BleState { fn default() -> Self { BleState::Idle } }

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Mount {
    Mast,
    Deck,
}

impl Default for Mount {
    fn default() -> Self {
        Mount::Mast
    }
}

impl Mount {
    fn flag(self) -> &'static str {
        match self {
            Mount::Mast => "mast",
            Mount::Deck => "deck",
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum RunStatus {
    Ok,
    Failed,
    Cancelled,
}

// ---------------------------------------------------------------------------
//  File classification
// ---------------------------------------------------------------------------

#[derive(Clone, Copy, Debug)]
enum FileKind {
    Sensor,
    Gps,
    Video,
    Stl,
    Unknown,
}

fn classify(path: &Path) -> FileKind {
    let name = path.file_name().and_then(|s| s.to_str()).unwrap_or("");
    let ext = path
        .extension()
        .and_then(|s| s.to_str())
        .unwrap_or("")
        .to_ascii_lowercase();
    match ext.as_str() {
        "csv" => {
            if name.ends_with("_gps.csv") {
                FileKind::Gps
            // _quaternions.csv is a derived stbox-viz output, not an
            // input — silently ignore so the user dropping a result
            // folder doesn't end up with the wrong sensor source.
            } else if name.ends_with("_quaternions.csv") || name.ends_with("_errlog.txt") {
                FileKind::Unknown
            } else {
                FileKind::Sensor
            }
        }
        "mov" | "mp4" | "mkv" | "m4v" | "avi" => FileKind::Video,
        "stl" => FileKind::Stl,
        _ => FileKind::Unknown,
    }
}

/// Best-effort companion-GPS guesser: given `Sens005.csv`, look for
/// `Sens005_gps.csv` next to it.
fn guess_gps_for(sensor: &Path) -> Option<PathBuf> {
    let stem = sensor.file_stem()?.to_str()?.to_string();
    let parent = sensor.parent()?;
    let gps = parent.join(format!("{stem}_gps.csv"));
    if gps.exists() {
        Some(gps)
    } else {
        None
    }
}

// ---------------------------------------------------------------------------
//  Locating the bundled stbox-viz binary
// ---------------------------------------------------------------------------

fn stbox_viz_path() -> PathBuf {
    let exe_name = if cfg!(windows) { "stbox-viz.exe" } else { "stbox-viz" };
    if let Ok(my_exe) = std::env::current_exe() {
        if let Some(dir) = my_exe.parent() {
            let candidate = dir.join(exe_name);
            if candidate.exists() {
                return candidate;
            }
        }
    }
    // Fallback to PATH lookup. Letting `Command` resolve via PATH only
    // works if the binary is in the user's PATH — fine for `cargo run`
    // during development if cargo bin is on PATH, otherwise the user
    // sees the spawn failure in the log.
    PathBuf::from(exe_name)
}

// ---------------------------------------------------------------------------
//  Spawning + log pump
// ---------------------------------------------------------------------------

fn spawn_animate(state: &mut AppState) {
    let Some(sensor) = state.sensor_csv.clone() else {
        push_log(&state.log, "error: no sensor CSV — drop a Sens*.csv first.".into());
        return;
    };

    let mut cmd = Command::new(stbox_viz_path());
    cmd.arg("animate")
        .arg(&sensor)
        .arg("-o")
        .arg(&state.output_dir)
        .arg("--fps")
        .arg(state.fps.to_string())
        .arg("--mount")
        .arg(state.mount.flag());

    if let Some(v) = state.video.as_ref() {
        cmd.arg("--video").arg(v);
    }
    if let Some(stl) = state.board_stl.as_ref() {
        cmd.arg("--board-stl").arg(stl);
    }
    if !state.at.trim().is_empty() {
        cmd.arg("--at").arg(state.at.trim());
    }
    if state.tz_offset_h.abs() > f64::EPSILON {
        cmd.arg("--tz-offset-h").arg(format!("{}", state.tz_offset_h));
    }
    if !state.date.trim().is_empty() {
        cmd.arg("--date").arg(state.date.trim());
    }
    if state.duration_s > 0.0 {
        cmd.arg("--duration").arg(format!("{}", state.duration_s));
    }
    if state.dock_height_m.abs() > f64::EPSILON {
        cmd.arg("--dock-height-m")
            .arg(format!("{}", state.dock_height_m));
    }
    if state.auto_skip {
        cmd.arg("--auto-skip");
    }
    if !state.title.trim().is_empty() {
        cmd.arg("--title").arg(&state.title);
    }
    if !state.subtitle.trim().is_empty() {
        cmd.arg("--subtitle").arg(&state.subtitle);
    }

    cmd.stdout(Stdio::piped()).stderr(Stdio::piped());

    push_log(
        &state.log,
        format!(
            "$ {} {}",
            stbox_viz_path().display(),
            cmd.get_args()
                .map(|s| s.to_string_lossy().into_owned())
                .collect::<Vec<_>>()
                .join(" ")
        ),
    );

    let mut child = match cmd.spawn() {
        Ok(c) => c,
        Err(e) => {
            push_log(
                &state.log,
                format!("error: failed to spawn stbox-viz: {e}. Make sure it sits next to MovementLogger or is on PATH."),
            );
            state.last_status = Some(RunStatus::Failed);
            return;
        }
    };

    state.cancel.store(false, Ordering::SeqCst);
    state.running.store(true, Ordering::SeqCst);
    let log = state.log.clone();
    let cancel = state.cancel.clone();
    let running = state.running.clone();

    // One thread per stream so a deadlock on either pipe doesn't stall
    // the other. Both forward into a shared mpsc that the main thread
    // drains into the log vec.
    let (tx, rx) = mpsc::channel::<String>();

    if let Some(out) = child.stdout.take() {
        let tx = tx.clone();
        thread::spawn(move || pump_pipe(out, tx));
    }
    if let Some(err) = child.stderr.take() {
        let tx = tx.clone();
        thread::spawn(move || pump_pipe(err, tx));
    }
    drop(tx);

    thread::spawn(move || {
        for line in rx {
            push_log(&log, line);
        }
        // After both pipes have closed, wait for exit + reap.
        let mut cancelled = false;
        loop {
            match child.try_wait() {
                Ok(Some(status)) => {
                    let msg = if cancelled {
                        "--- cancelled ---".to_string()
                    } else if status.success() {
                        "--- done ---".to_string()
                    } else {
                        format!("--- exited with {status} ---")
                    };
                    push_log(&log, msg);
                    break;
                }
                Ok(None) => {
                    if cancel.load(Ordering::SeqCst) && !cancelled {
                        // Best-effort kill — child carries on if signal
                        // delivery fails on this platform, but the wait
                        // loop keeps going.
                        let _ = child.kill();
                        cancelled = true;
                    }
                    thread::sleep(std::time::Duration::from_millis(100));
                }
                Err(e) => {
                    push_log(&log, format!("error: try_wait failed: {e}"));
                    break;
                }
            }
        }
        running.store(false, Ordering::SeqCst);
    });
}

fn pump_pipe<R: std::io::Read + Send + 'static>(r: R, tx: mpsc::Sender<String>) {
    use std::io::{BufRead, BufReader};
    let buf = BufReader::new(r);
    for line in buf.lines() {
        match line {
            Ok(s) => {
                if tx.send(s).is_err() {
                    break;
                }
            }
            Err(_) => break,
        }
    }
}

fn push_log(log: &Arc<Mutex<Vec<String>>>, line: String) {
    if let Ok(mut v) = log.lock() {
        // Cap the buffer so a long-running session can't exhaust RAM.
        const CAP: usize = 10_000;
        if v.len() >= CAP {
            v.drain(..CAP / 4);
        }
        v.push(line);
    }
}

// ---------------------------------------------------------------------------
//  egui app
// ---------------------------------------------------------------------------

impl AppState {
    fn new() -> Self {
        let cwd = std::env::current_dir().unwrap_or_else(|_| PathBuf::from("."));
        let output_dir = cwd.join("gif");
        let ble_out_dir = cwd.join("csv");
        Self {
            output_dir,
            ble_out_dir,
            tz_offset_h: 3.0,
            fps: 15,
            ble_session_duration_s: 1800,  // 30-min default
            ..Self::default()
        }
    }

    fn ensure_ble(&mut self) -> &BleBackend {
        if self.ble.is_none() {
            self.ble = Some(BleBackend::spawn());
        }
        self.ble.as_ref().unwrap()
    }

    /// Drain pending BLE events into the visible state. Called once per
    /// frame; egui repaints on a timer while a BLE op is in progress so
    /// events don't sit in the channel for long.
    fn pump_ble_events(&mut self) {
        let Some(b) = self.ble.as_ref() else { return; };
        let events = b.try_recv_all();
        for e in events {
            match e {
                BleEvent::Status(s) => self.ble_status = s,
                BleEvent::Discovered { id, name, rssi } => {
                    if !self.ble_devices.iter().any(|d| d.id == id) {
                        self.ble_devices.push(BleDevice { id, name, rssi });
                    }
                }
                BleEvent::ScanStopped => {
                    self.ble_state = BleState::Idle;
                    self.ble_status = format!("scan done ({} found)", self.ble_devices.len());
                }
                BleEvent::Connected => {
                    self.ble_state = BleState::Connected;
                    self.ble_status = "connected".into();
                }
                BleEvent::Disconnected => {
                    self.ble_state = BleState::Idle;
                    self.ble_files.clear();
                    self.ble_status = "disconnected".into();
                }
                BleEvent::ListEntry { name, size } => {
                    self.ble_files.push(BleFile {
                        name, size, selected: true, downloaded: false,
                    });
                }
                BleEvent::ListDone => {
                    self.ble_status = format!("listing done ({} files)", self.ble_files.len());
                }
                BleEvent::ReadStarted { name, size } => {
                    self.ble_status = format!("reading {name} ({size} B)…");
                }
                BleEvent::ReadProgress { name, bytes_done } => {
                    self.ble_status = format!("reading {name}: {bytes_done} B");
                }
                BleEvent::ReadDone { name, content } => {
                    match save_downloaded_file(&self.ble_out_dir, &name, &content) {
                        Ok(path) => {
                            self.ble_status = format!("saved {} ({} B)", path.display(), content.len());
                            for f in self.ble_files.iter_mut() {
                                if f.name == name { f.downloaded = true; }
                            }
                            // Auto-route into the existing animate
                            // pipeline so the user can immediately hit
                            // Generate without re-dragging.
                            self.auto_route_downloaded(&name, &path);
                            push_log(&self.log, format!("ble: saved {}", path.display()));
                        }
                        Err(e) => {
                            self.ble_status = format!("save failed: {e}");
                            push_log(&self.log, format!("ble error: {e}"));
                        }
                    }
                }
                BleEvent::Error(msg) => {
                    self.ble_status = format!("error: {msg}");
                    push_log(&self.log, format!("ble error: {msg}"));
                    if matches!(self.ble_state, BleState::Scanning | BleState::Connecting) {
                        self.ble_state = BleState::Idle;
                    }
                }
            }
        }
    }

    /// If the saved file is a Sens*.csv or matching _gps.csv, set it on
    /// the top-of-form slots so the user doesn't need to hit Pick…
    fn auto_route_downloaded(&mut self, name: &str, path: &Path) {
        let lower = name.to_ascii_lowercase();
        if lower.ends_with("_gps.csv") {
            self.gps_csv = Some(path.into());
        } else if lower.starts_with("sens") && lower.ends_with(".csv") {
            self.sensor_csv = Some(path.into());
            if self.gps_csv.is_none() {
                self.gps_csv = guess_gps_for(path);
            }
        }
    }
}

/// Save a downloaded file under `dir`, creating the directory if
/// needed. Returns the resolved destination path.
fn save_downloaded_file(dir: &Path, name: &str, content: &[u8]) -> std::io::Result<PathBuf> {
    std::fs::create_dir_all(dir)?;
    let path = dir.join(name);
    std::fs::write(&path, content)?;
    Ok(path)
}

impl AppState {
    fn ui_ble_panel(&mut self, ui: &mut egui::Ui) {
        egui::CollapsingHeader::new("BLE FileSync (download from PumpTsueri)")
            .default_open(false)
            .show(ui, |ui| {
                ui.label(
                    egui::RichText::new(
                        "Scan, connect (PIN 123456), list SD files, download.",
                    )
                    .small()
                    .color(egui::Color32::from_gray(180)),
                );
                ui.add_space(4.0);

                // ----- Action buttons -------------------------------
                ui.horizontal(|ui| {
                    let scanning  = matches!(self.ble_state, BleState::Scanning | BleState::Connecting);
                    let connected = matches!(self.ble_state, BleState::Connected);
                    if ui
                        .add_enabled(!scanning && !connected, egui::Button::new("Scan"))
                        .clicked()
                    {
                        self.ble_devices.clear();
                        self.ble_state = BleState::Scanning;
                        let b = self.ensure_ble();
                        b.send(BleCmd::Scan);
                    }
                    if ui
                        .add_enabled(connected, egui::Button::new("Refresh file list"))
                        .clicked()
                    {
                        self.ble_files.clear();
                        if let Some(b) = self.ble.as_ref() { b.send(BleCmd::List); }
                    }
                    if ui
                        .add_enabled(connected, egui::Button::new("STOP_LOG"))
                        .on_hover_text("Tells the box to gracefully close any active session — required before READ if logging is busy")
                        .clicked()
                    {
                        if let Some(b) = self.ble.as_ref() { b.send(BleCmd::StopLog); }
                    }
                    /* Issue #15 — START_LOG triggers a LOG-mode session of
                     * `ble_session_duration_s` seconds. Box reboots into
                     * LOG mode, runs the logger for that long, then auto-
                     * reboots back to BLE mode. Connection drops during
                     * the LOG session — reconnect after to download files. */
                    ui.add_enabled(
                        connected,
                        egui::DragValue::new(&mut self.ble_session_duration_s)
                            .speed(10)
                            .range(1..=86400)
                            .suffix(" s"),
                    ).on_hover_text("Session duration in seconds (1..86400)");
                    if ui
                        .add_enabled(connected, egui::Button::new("Start session"))
                        .on_hover_text("Box reboots into LOG mode, logs for the duration above, then auto-returns to BLE mode for download")
                        .clicked()
                    {
                        if let Some(b) = self.ble.as_ref() {
                            b.send(BleCmd::StartLog { duration_seconds: self.ble_session_duration_s });
                        }
                    }
                    if ui
                        .add_enabled(connected, egui::Button::new("Disconnect"))
                        .clicked()
                    {
                        if let Some(b) = self.ble.as_ref() { b.send(BleCmd::Disconnect); }
                    }
                    ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                        if !self.ble_status.is_empty() {
                            /* Selectable so the user can copy error text
                               (egui Labels are non-selectable by default). */
                            ui.add(
                                egui::Label::new(
                                    egui::RichText::new(&self.ble_status)
                                        .small()
                                        .color(egui::Color32::LIGHT_BLUE),
                                )
                                .selectable(true),
                            );
                        }
                    });
                });

                // ----- Discovered devices --------------------------
                if !self.ble_devices.is_empty()
                    && !matches!(self.ble_state, BleState::Connected)
                {
                    ui.add_space(4.0);
                    ui.label("Discovered:");
                    let devices = self.ble_devices.clone();
                    for d in devices {
                        ui.horizontal(|ui| {
                            ui.label(format!(
                                "{} [{}]",
                                d.name,
                                d.rssi.map(|r| format!("{r} dBm")).unwrap_or_else(|| "?".into())
                            ));
                            if ui.button("Connect").clicked() {
                                self.ble_state = BleState::Connecting;
                                self.ble_status = "connecting…".into();
                                if let Some(b) = self.ble.as_ref() {
                                    b.send(BleCmd::Connect(d.id.clone()));
                                }
                            }
                        });
                    }
                }

                // ----- File list / download ------------------------
                if matches!(self.ble_state, BleState::Connected) {
                    ui.add_space(6.0);
                    ui.horizontal(|ui| {
                        ui.label("Save to:");
                        let mut s = self.ble_out_dir.display().to_string();
                        if ui.text_edit_singleline(&mut s).changed() {
                            self.ble_out_dir = PathBuf::from(s);
                        }
                        if ui.button("Browse…").clicked() {
                            if let Some(p) = rfd::FileDialog::new().pick_folder() {
                                self.ble_out_dir = p;
                            }
                        }
                    });

                    if self.ble_files.is_empty() {
                        ui.label(
                            egui::RichText::new("No file list yet — hit Refresh.")
                                .small()
                                .color(egui::Color32::from_gray(170)),
                        );
                    } else {
                        ui.add_space(4.0);
                        egui::ScrollArea::vertical()
                            .max_height(160.0)
                            .id_salt("ble-file-list")
                            .show(ui, |ui| {
                                for f in self.ble_files.iter_mut() {
                                    ui.horizontal(|ui| {
                                        ui.checkbox(&mut f.selected, "");
                                        ui.label(format!("{:>10} B  {}", f.size, f.name));
                                        if f.downloaded {
                                            ui.colored_label(egui::Color32::LIGHT_GREEN, "✓");
                                        }
                                    });
                                }
                            });

                        ui.add_space(4.0);
                        if ui.button("Download selected").clicked() {
                            if let Some(b) = self.ble.as_ref() {
                                for f in self.ble_files.iter() {
                                    if f.selected && !f.downloaded {
                                        b.send(BleCmd::Read {
                                            name: f.name.clone(),
                                            size: f.size,
                                        });
                                    }
                                }
                            }
                        }
                    }
                }
            });
    }
}

impl AppState {
    fn ingest_dropped(&mut self, files: &[egui::DroppedFile]) {
        for f in files {
            let Some(path) = f.path.as_ref() else { continue };
            match classify(path) {
                FileKind::Sensor => {
                    self.sensor_csv = Some(path.clone());
                    if self.gps_csv.is_none() {
                        self.gps_csv = guess_gps_for(path);
                    }
                }
                FileKind::Gps => {
                    self.gps_csv = Some(path.clone());
                }
                FileKind::Video => {
                    self.video = Some(path.clone());
                }
                FileKind::Stl => {
                    self.board_stl = Some(path.clone());
                }
                FileKind::Unknown => {
                    push_log(
                        &self.log,
                        format!(
                            "ignored {} (drop a Sens*.csv, *_gps.csv, .mov/.mp4, or .stl)",
                            path.display()
                        ),
                    );
                }
            }
        }
    }
}

impl eframe::App for AppState {
    fn update(&mut self, ctx: &egui::Context, _frame: &mut eframe::Frame) {
        // Drag-and-drop handling. Plain files come through
        // `dropped_files`; `hovered_files` lets us highlight the drop
        // zone before the user releases.
        let dropped = ctx.input(|i| i.raw.dropped_files.clone());
        if !dropped.is_empty() {
            self.ingest_dropped(&dropped);
        }
        let hovering = ctx.input(|i| !i.raw.hovered_files.is_empty());

        // Drain BLE worker events into AppState before laying out the
        // UI so the FileSync panel renders the latest state every frame.
        self.pump_ble_events();

        // Lazy-load the in-app logo on the first frame after the egui
        // context becomes available.
        if self.icon_tex.is_none() {
            self.icon_tex = decode_icon().map(|img| {
                ctx.load_texture("movementlogger-icon", img, egui::TextureOptions::LINEAR)
            });
        }

        egui::TopBottomPanel::top("title").show(ctx, |ui| {
            // No app-name + version heading here — the OS window
            // title already shows it, no point duplicating.
            ui.horizontal(|ui| {
                ui.hyperlink_to(
                    "SensorTile.box pumpfoil session video generator",
                    "https://github.com/zdavatz/fp-sns-stbox1",
                );
                // Right-anchor the logo so it sits in the top-right
                // corner regardless of window width. Clicking it opens
                // a mailto: to support — saves field testers having to
                // hunt for the support address when something goes
                // wrong with a session render.
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    if let Some(tex) = self.icon_tex.as_ref() {
                        let size = egui::vec2(40.0, 40.0);
                        let resp = ui
                            .add(
                                egui::ImageButton::new((tex.id(), size))
                                    .frame(false),
                            )
                            .on_hover_text("Email zdavatz@ywesee.com")
                            .on_hover_cursor(egui::CursorIcon::PointingHand);
                        if resp.clicked() {
                            ui.ctx().open_url(egui::OpenUrl::new_tab(
                                "mailto:zdavatz@ywesee.com",
                            ));
                        }
                    }
                });
            });
        });

        egui::CentralPanel::default().show(ctx, |ui| {
            ui.add_space(6.0);

            // ----- Drop zone ------------------------------------------
            let zone_color = if hovering {
                egui::Color32::from_rgb(70, 110, 70)
            } else {
                egui::Color32::from_rgb(40, 40, 50)
            };
            egui::Frame::none()
                .fill(zone_color)
                .stroke(egui::Stroke::new(1.0, egui::Color32::from_gray(120)))
                .rounding(6.0)
                .inner_margin(egui::Margin::symmetric(12.0, 18.0))
                .show(ui, |ui| {
                    ui.set_width(ui.available_width());
                    ui.vertical_centered(|ui| {
                        ui.label(
                            egui::RichText::new("Drop sensor CSV + GPS CSV + video here")
                                .heading()
                                .color(egui::Color32::WHITE),
                        );
                        ui.label(
                            egui::RichText::new(
                                "Sens*.csv • *_gps.csv (auto-paired) • .mov / .mp4 • .stl (optional)",
                            )
                            .color(egui::Color32::from_gray(200)),
                        );
                    });
                });

            ui.add_space(8.0);

            // ----- Input file summary ---------------------------------
            egui::Grid::new("inputs")
                .num_columns(3)
                .min_col_width(80.0)
                .show(ui, |ui| {
                    file_row(ui, "Sensor", &mut self.sensor_csv, &[("CSV", &["csv"])]);
                    ui.end_row();
                    file_row(ui, "GPS", &mut self.gps_csv, &[("CSV", &["csv"])]);
                    ui.end_row();
                    file_row(
                        ui,
                        "Video",
                        &mut self.video,
                        &[("Video", &["mov", "mp4", "mkv", "m4v", "avi"])],
                    );
                    ui.end_row();
                    file_row(ui, "Board STL", &mut self.board_stl, &[("STL", &["stl"])]);
                    ui.end_row();
                });

            ui.separator();

            // ----- BLE FileSync panel ---------------------------------
            self.ui_ble_panel(ui);

            ui.separator();

            // ----- Optional fields ------------------------------------
            egui::CollapsingHeader::new("Animation parameters")
                .default_open(true)
                .show(ui, |ui| {
                    egui::Grid::new("opts")
                        .num_columns(2)
                        .spacing([10.0, 6.0])
                        .show(ui, |ui| {
                            ui.label("Start time (HH:MM:SS, local)");
                            ui.text_edit_singleline(&mut self.at);
                            ui.end_row();

                            ui.label("Timezone offset (hours)");
                            ui.add(egui::DragValue::new(&mut self.tz_offset_h).speed(1.0).range(-12.0..=14.0));
                            ui.end_row();

                            ui.label("Date (YYYY-MM-DD, blank = sensor mtime)");
                            ui.text_edit_singleline(&mut self.date);
                            ui.end_row();

                            ui.label("Duration (s, 0 = video length)");
                            ui.add(egui::DragValue::new(&mut self.duration_s).speed(1.0).range(0.0..=600.0));
                            ui.end_row();

                            ui.label("Mount");
                            ui.horizontal(|ui| {
                                ui.radio_value(&mut self.mount, Mount::Mast, "Mast");
                                ui.radio_value(&mut self.mount, Mount::Deck, "Deck");
                            });
                            ui.end_row();

                            ui.label("Dock height (m)");
                            ui.add(egui::DragValue::new(&mut self.dock_height_m).speed(0.05).range(0.0..=3.0));
                            ui.end_row();

                            ui.label("Auto-skip carry phase");
                            ui.checkbox(&mut self.auto_skip, "");
                            ui.end_row();

                            ui.label("Title");
                            ui.text_edit_singleline(&mut self.title);
                            ui.end_row();

                            ui.label("Subtitle");
                            ui.text_edit_singleline(&mut self.subtitle);
                            ui.end_row();

                            ui.label("FPS");
                            ui.add(egui::DragValue::new(&mut self.fps).speed(1.0).range(5..=60));
                            ui.end_row();

                            ui.label("Output folder");
                            ui.horizontal(|ui| {
                                let mut s = self.output_dir.display().to_string();
                                let resp = ui.text_edit_singleline(&mut s);
                                if resp.changed() {
                                    self.output_dir = PathBuf::from(s);
                                }
                                if ui.button("Browse…").clicked() {
                                    if let Some(p) = rfd::FileDialog::new().pick_folder() {
                                        self.output_dir = p;
                                    }
                                }
                            });
                            ui.end_row();
                        });
                });

            ui.add_space(6.0);

            // ----- Action buttons -------------------------------------
            ui.horizontal(|ui| {
                let running = self.running.load(Ordering::SeqCst);
                let can_run = !running && self.sensor_csv.is_some();
                if ui
                    .add_enabled(can_run, egui::Button::new("▶ Generate"))
                    .clicked()
                {
                    spawn_animate(self);
                }
                if ui
                    .add_enabled(running, egui::Button::new("■ Stop"))
                    .clicked()
                {
                    self.cancel.store(true, Ordering::SeqCst);
                }
                if ui.button("Open output folder").clicked() {
                    let _ = open_in_filer(&self.output_dir);
                }
                if ui.button("Clear log").clicked() {
                    if let Ok(mut v) = self.log.lock() {
                        v.clear();
                    }
                }
                ui.with_layout(egui::Layout::right_to_left(egui::Align::Center), |ui| {
                    match (running, self.last_status) {
                        (true, _) => {
                            ui.spinner();
                            ui.label("running…");
                        }
                        (false, Some(RunStatus::Ok)) => {
                            ui.colored_label(egui::Color32::LIGHT_GREEN, "ok");
                        }
                        (false, Some(RunStatus::Failed)) => {
                            ui.colored_label(egui::Color32::LIGHT_RED, "failed");
                        }
                        (false, Some(RunStatus::Cancelled)) => {
                            ui.colored_label(egui::Color32::LIGHT_YELLOW, "cancelled");
                        }
                        _ => {}
                    }
                });
            });

            ui.separator();

            // ----- Log panel ------------------------------------------
            ui.label("Log");
            egui::ScrollArea::vertical()
                .max_height(ui.available_height())
                .stick_to_bottom(true)
                .show(ui, |ui| {
                    let lines = self.log.lock().map(|v| v.clone()).unwrap_or_default();
                    let text = lines.join("\n");
                    let mut owned = text;
                    ui.add(
                        egui::TextEdit::multiline(&mut owned)
                            .desired_width(f32::INFINITY)
                            .desired_rows(12)
                            .font(egui::TextStyle::Monospace)
                            .interactive(false),
                    );
                });
        });

        // Watch for the worker thread setting `running=false` and lift
        // the spinner / set the last_status. Tail-line inspection lets
        // us colour the status badge without the worker thread needing
        // a reference to AppState.
        if !self.running.load(Ordering::SeqCst) && self.last_status.is_none() {
            if let Ok(v) = self.log.lock() {
                if let Some(last) = v.last() {
                    if last == "--- done ---" {
                        self.last_status = Some(RunStatus::Ok);
                    } else if last == "--- cancelled ---" {
                        self.last_status = Some(RunStatus::Cancelled);
                    } else if last.starts_with("--- exited with") {
                        self.last_status = Some(RunStatus::Failed);
                    }
                }
            }
        }
        if self.running.load(Ordering::SeqCst) {
            self.last_status = None;
            ctx.request_repaint_after(std::time::Duration::from_millis(150));
        }

        // Same idea for BLE: while a scan / connect / list / read is in
        // flight or has just emitted progress, keep the UI ticking so
        // the status badge updates without waiting on user input.
        if matches!(
            self.ble_state,
            BleState::Scanning | BleState::Connecting | BleState::Connected
        ) {
            ctx.request_repaint_after(std::time::Duration::from_millis(150));
        }
    }
}

fn file_row(
    ui: &mut egui::Ui,
    label: &str,
    target: &mut Option<PathBuf>,
    filters: &[(&str, &[&str])],
) {
    ui.label(label);
    let display = target
        .as_ref()
        .map(|p| {
            p.file_name()
                .map(|n| n.to_string_lossy().into_owned())
                .unwrap_or_else(|| p.display().to_string())
        })
        .unwrap_or_else(|| "—".to_string());
    ui.label(display);
    ui.horizontal(|ui| {
        if ui.button("Pick…").clicked() {
            let mut dialog = rfd::FileDialog::new();
            for (name, exts) in filters {
                dialog = dialog.add_filter(*name, exts);
            }
            if let Some(p) = dialog.pick_file() {
                *target = Some(p);
            }
        }
        if target.is_some() && ui.button("✕").clicked() {
            *target = None;
        }
    });
}

#[cfg(target_os = "macos")]
fn open_in_filer(path: &Path) -> std::io::Result<()> {
    Command::new("open").arg(path).status().map(|_| ())
}
#[cfg(target_os = "windows")]
fn open_in_filer(path: &Path) -> std::io::Result<()> {
    Command::new("explorer").arg(path).status().map(|_| ())
}
#[cfg(all(unix, not(target_os = "macos")))]
fn open_in_filer(path: &Path) -> std::io::Result<()> {
    Command::new("xdg-open").arg(path).status().map(|_| ())
}

// ---------------------------------------------------------------------------
//  Entry point
// ---------------------------------------------------------------------------

/// Decode `ICON_PNG` into an egui-friendly RGBA image. Returns `None`
/// on decode failure — the GUI still works, it just shows no logo.
fn decode_icon() -> Option<egui::ColorImage> {
    let img = image::load_from_memory(ICON_PNG).ok()?.into_rgba8();
    let (w, h) = img.dimensions();
    Some(egui::ColorImage::from_rgba_unmultiplied(
        [w as usize, h as usize],
        img.as_raw(),
    ))
}

/// Decode the icon into the format eframe wants for the OS-level
/// window icon (Dock on macOS, taskbar on Windows / Linux).
fn os_window_icon() -> Option<egui::IconData> {
    let img = image::load_from_memory(ICON_PNG).ok()?.into_rgba8();
    let (w, h) = img.dimensions();
    Some(egui::IconData {
        rgba: img.into_raw(),
        width: w,
        height: h,
    })
}

fn main() -> eframe::Result<()> {
    let title = format!("MovementLogger {}", env!("CARGO_PKG_VERSION"));
    let mut viewport = egui::ViewportBuilder::default()
        .with_inner_size([880.0, 720.0])
        .with_min_inner_size([560.0, 480.0])
        .with_title(&title);
    if let Some(icon) = os_window_icon() {
        viewport = viewport.with_icon(icon);
    }
    let options = eframe::NativeOptions {
        viewport,
        ..Default::default()
    };
    eframe::run_native(
        &title,
        options,
        Box::new(|_cc| Ok(Box::new(AppState::new()))),
    )
}
