//! stbox-viz — SensorTile.box-Pro pumpfoil session visualiser.
//!
//! Replaces all of `Utilities/scripts/*.py` with four subcommands:
//!
//!   combined   interactive Plotly HTML (map + nose angle + height + speed)
//!   sensors    5-panel PNG of raw sensors + quaternion/Euler angles
//!   pumpfoil   pump-cadence spectrogram + movement-phase PNGs
//!   animate    animated GIF of board side view + optional combined MOV

mod animate_cmd;
mod baro;
mod bin_util;
mod butter;
mod euler;
mod fusion;
mod gps;
mod html;
mod io;
mod plot_common;
mod pumpfoil_cmd;
mod sensors_cmd;
mod session;
mod spectrogram;

use anyhow::{Context, Result, anyhow};
use clap::{Parser, Subcommand};
use std::path::{Path, PathBuf};

const SAMPLE_HZ: usize = 100;

#[derive(Parser, Debug)]
#[command(name = "stbox-viz", version, about = "SensorTile.box-Pro session visualiser")]
struct Cli {
    #[command(subcommand)]
    cmd: Cmd,
}

#[derive(Subcommand, Debug)]
enum Cmd {
    /// Interactive Plotly HTML combining map + time-series panels.
    Combined {
        sensor_csv: PathBuf,
        #[arg(short, long, default_value = "html")]
        output: PathBuf,
        #[arg(long, default_value_t = 0.1)]
        beta: f64,
    },
    /// Sensor + quaternion PNG plots.
    Sensors {
        sensor_csv: PathBuf,
        #[arg(short, long, default_value = "png")]
        output: PathBuf,
    },
    /// Pump cadence + movement-phase PNGs.
    Pumpfoil {
        sensor_csv: PathBuf,
        #[arg(short, long, default_value = "png")]
        output: PathBuf,
    },
    /// Animated GIF of board orientation per session, with optional
    /// side-by-side camera-video MOV combine (needs ffmpeg in PATH).
    Animate {
        sensor_csv: PathBuf,
        #[arg(short, long, default_value = "gif")]
        output: PathBuf,
        #[arg(long, default_value_t = 15)]
        fps: u32,
        #[arg(long)]
        session: Option<usize>,
        #[arg(long)]
        video: Option<PathBuf>,
        #[arg(long, default_value_t = 0.0)]
        video_offset: f64,
        #[arg(long, default_value_t = 0.0)]
        sensor_offset: f64,
        #[arg(long)]
        title: Option<String>,
        #[arg(long)]
        subtitle: Option<String>,
    },
}

fn main() -> Result<()> {
    let cli = Cli::parse();
    match cli.cmd {
        Cmd::Combined { sensor_csv, output, beta } =>
            run_combined(&sensor_csv, &output, beta),
        Cmd::Sensors { sensor_csv, output } =>
            sensors_cmd::run(&sensor_csv, &output),
        Cmd::Pumpfoil { sensor_csv, output } =>
            pumpfoil_cmd::run(&sensor_csv, &output),
        Cmd::Animate { sensor_csv, output, fps, session, video,
                       video_offset, sensor_offset, title, subtitle } => {
            animate_cmd::run(&animate_cmd::AnimateArgs {
                sensor_csv: &sensor_csv,
                output_dir: &output,
                fps,
                session,
                video: video.as_deref(),
                video_offset,
                sensor_offset,
                title: title.as_deref(),
                subtitle: subtitle.as_deref(),
            })
        }
    }
}

fn run_combined(sensor_path: &Path, output: &Path, beta: f64) -> Result<()> {
    let sensor_path = sensor_path.canonicalize()
        .with_context(|| format!("canonicalize {}", sensor_path.display()))?;
    let stem = sensor_path.file_stem().and_then(|s| s.to_str())
        .ok_or_else(|| anyhow!("sensor csv has no stem"))?.to_string();

    let gps_path = guess_gps_path(&sensor_path);

    println!("loading {}", sensor_path.file_name().unwrap().to_string_lossy());
    let sensors = io::load_sensor_csv(&sensor_path)?;
    let n = sensors.len();
    let dur_s = n as f64 / SAMPLE_HZ as f64;
    println!("  {} rows at {} Hz ({:.1} min)", n, SAMPLE_HZ, dur_s / 60.0);
    if n == 0 { return Err(anyhow!("sensor CSV is empty")); }

    let base_ticks = sensors[0].ticks;
    let t_sensor_s: Vec<f64> = sensors.iter()
        .map(|s| (s.ticks - base_ticks) / gps::TICKS_PER_SEC)
        .collect();

    println!("running Madgwick 6DOF fusion (beta={})", beta);
    let quats = fusion::compute_quaternions(&sensors, beta);
    let nose_deg = fusion::nose_angle_series_deg(&quats, SAMPLE_HZ);

    let (gps_rows, gps_speed, gps_t_s) = if let Some(p) = gps_path.as_ref() {
        println!("loading {}", p.file_name().unwrap().to_string_lossy());
        let mut rows = io::load_gps_csv(p)?;
        rows.retain(|g| g.fix >= 1);
        rows = dedupe_by_second(rows, base_ticks);
        println!("  {} GPS fixes", rows.len());
        let raw = gps::position_derived_speed_kmh(&rows);
        // Reject multipath-induced position jumps: anything where the
        // implied longitudinal acceleration exceeds 15 km/h/s (≈4 m/s²)
        // is almost certainly a bad fix, not a real paddle stroke.
        // SUPfoil paddle-starts produce 1–3 m/s² per stroke.
        let gated = gps::reject_acc_outliers(&rows, &raw, 15.0);
        let smooth = gps::smooth_speed_kmh(&gated);
        let t_s: Vec<f64> = rows.iter()
            .map(|g| (g.ticks - base_ticks) / gps::TICKS_PER_SEC).collect();
        (rows, smooth, t_s)
    } else {
        println!("no GPS CSV found — skipping map + ride detection");
        (Vec::new(), Vec::new(), Vec::new())
    };

    let rides = if !gps_rows.is_empty() {
        gps::detect_rides(&gps_rows, &gps_speed, n, SAMPLE_HZ, base_ticks,
                          3.0, 10.0, 30.0, 3.0)
    } else { Vec::new() };
    println!("detected {} rides over water", rides.len());
    for (i, r) in rides.iter().enumerate() {
        let m = (r.duration_s / 60.0) as u32;
        let s = ((r.duration_s - m as f64 * 60.0) as u32).min(59);
        println!("  session {}: UTC {}, duration {:02}:{:02}, p90 {:.1} km/h",
                 i + 1, r.utc_start, m, s, r.p90_kmh);
    }

    let height_m = baro::height_above_water_m(&sensors, &gps_rows, &gps_speed, base_ticks);

    // GPS altitude, zeroed at the median of stationary samples (speed
    // < 3 km/h) so it lines up with the baro height axis. MAX-M10S
    // vertical scatter is ~3–10 m — the trace won't resolve pumps, but
    // makes fix dropouts visible when the board lies flat on water.
    let gps_height_m: Vec<f64> = if !gps_rows.is_empty() {
        let alt: Vec<f64> = gps_rows.iter().map(|g| g.alt_m).collect();
        let mut stationary: Vec<f64> = alt.iter().zip(gps_speed.iter())
            .filter(|&(_, &s)| s < 3.0)
            .map(|(&a, _)| a)
            .collect();
        let baseline = {
            let pool = if !stationary.is_empty() { &mut stationary } else {
                &mut alt.clone()
            };
            pool.sort_by(|a, b| a.partial_cmp(b).unwrap());
            pool[pool.len() / 2]
        };
        alt.iter().map(|a| a - baseline).collect()
    } else {
        Vec::new()
    };

    let (nose_t, nose_binned) =
        bin_util::bin_to_resolution(&t_sensor_s, &nose_deg, 100, bin_util::Agg::Mean);
    let (_, height_binned) =
        bin_util::bin_to_resolution(&t_sensor_s, &height_m, 100, bin_util::Agg::Mean);

    let title = format!("{} — {:.1} min · {} ride{}",
        stem, dur_s / 60.0, rides.len(),
        if rides.len() == 1 { "" } else { "s" });
    let page = html::render(&html::PanelData {
        t_sensor_s: &nose_t,
        nose_deg: &nose_binned,
        height_m: &height_binned,
        gps: &gps_rows,
        gps_speed_kmh: &gps_speed,
        gps_t_s: &gps_t_s,
        gps_height_m: &gps_height_m,
        rides: &rides,
        title: &title,
    });

    std::fs::create_dir_all(output)
        .with_context(|| format!("mkdir {}", output.display()))?;
    let out_path = output.join(format!("viz_{}.html", stem));
    std::fs::write(&out_path, page)
        .with_context(|| format!("write {}", out_path.display()))?;
    println!("  wrote {}", out_path.display());
    Ok(())
}

fn guess_gps_path(sensor_path: &Path) -> Option<PathBuf> {
    let stem = sensor_path.file_stem()?.to_str()?.to_string();
    let parent = sensor_path.parent()?;
    let gps = parent.join(format!("{}_gps.csv", stem));
    if gps.exists() { Some(gps) } else { None }
}

fn dedupe_by_second(rows: Vec<io::GpsRow>, base_ticks: f64) -> Vec<io::GpsRow> {
    let mut last_sec: i64 = i64::MIN;
    let mut out = Vec::with_capacity(rows.len());
    for r in rows {
        let s = ((r.ticks - base_ticks) / gps::TICKS_PER_SEC).round() as i64;
        if s != last_sec {
            last_sec = s;
            out.push(r);
        }
    }
    out
}
