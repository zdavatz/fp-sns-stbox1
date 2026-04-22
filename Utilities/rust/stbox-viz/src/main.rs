//! stbox-viz — SensorTile.box-Pro pumpfoil session visualiser.
//!
//! Replaces the Python Utilities/scripts/visualize_combined.py. Reads a
//! sensor CSV (SensNNN.csv) + GPS CSV (GpsNNN.csv) from an SD-card
//! session and writes an interactive Plotly HTML to `html/`.

mod baro;
mod bin_util;
mod fusion;
mod gps;
mod html;
mod io;

use anyhow::{Context, Result, anyhow};
use clap::Parser;
use std::path::{Path, PathBuf};

const SAMPLE_HZ: usize = 100; // SensorTile.box-Pro writes at 100 Hz

#[derive(Parser, Debug)]
#[command(
    name = "stbox-viz",
    about = "Combined Plotly HTML report from SensNNN.csv + GpsNNN.csv",
    version,
)]
struct Cli {
    /// Path to SensNNN.csv (the sensor CSV). The GPS CSV is auto-detected
    /// from `<stem>_gps.csv` next to it.
    sensor_csv: PathBuf,

    /// Output directory (default: html/).
    #[arg(short, long, default_value = "html")]
    output: PathBuf,

    /// Madgwick filter gain (default 0.1, 6DOF IMU-only).
    #[arg(long, default_value_t = 0.1)]
    beta: f64,
}

fn main() -> Result<()> {
    let cli = Cli::parse();

    let sensor_path = cli.sensor_csv.canonicalize()
        .with_context(|| format!("canonicalize {}", cli.sensor_csv.display()))?;
    let stem = sensor_path.file_stem().and_then(|s| s.to_str())
        .ok_or_else(|| anyhow!("sensor csv has no stem"))?
        .to_string();

    // Auto-locate GPS CSV alongside.
    let gps_path = guess_gps_path(&sensor_path);

    println!("loading {}", sensor_path.file_name().unwrap().to_string_lossy());
    let sensors = io::load_sensor_csv(&sensor_path)?;
    let n = sensors.len();
    let dur_s = n as f64 / SAMPLE_HZ as f64;
    println!("  {} rows at {} Hz ({:.1} min)", n, SAMPLE_HZ, dur_s / 60.0);

    if n == 0 {
        return Err(anyhow!("sensor CSV is empty"));
    }

    let base_ticks = sensors[0].ticks;
    let t_sensor_s: Vec<f64> = sensors.iter()
        .map(|s| (s.ticks - base_ticks) / gps::TICKS_PER_SEC)
        .collect();

    println!("running Madgwick 6DOF fusion (beta={})", cli.beta);
    let quats = fusion::compute_quaternions(&sensors, cli.beta);
    let nose_deg = fusion::nose_angle_series_deg(&quats, SAMPLE_HZ);

    let (gps_rows, gps_speed, gps_t_s) = if let Some(p) = gps_path.as_ref() {
        println!("loading {}", p.file_name().unwrap().to_string_lossy());
        let mut rows = io::load_gps_csv(p)?;
        // Drop no-fix rows
        rows.retain(|g| g.fix >= 1);
        // De-duplicate by 1-second bucket, keep first fix per second
        rows = dedupe_by_second(rows, base_ticks);
        println!("  {} GPS fixes", rows.len());
        let raw = gps::position_derived_speed_kmh(&rows);
        let smooth = gps::smooth_speed_kmh(&raw);
        let t_s: Vec<f64> = rows.iter()
            .map(|g| (g.ticks - base_ticks) / gps::TICKS_PER_SEC)
            .collect();
        (rows, smooth, t_s)
    } else {
        println!("no GPS CSV found — skipping map + ride detection");
        (Vec::new(), Vec::new(), Vec::new())
    };

    // Ride detection
    let rides = if !gps_rows.is_empty() {
        gps::detect_rides(
            &gps_rows, &gps_speed, n, SAMPLE_HZ, base_ticks,
            3.0, 10.0, 30.0, 3.0,
        )
    } else {
        Vec::new()
    };
    println!("detected {} rides over water", rides.len());
    for (i, r) in rides.iter().enumerate() {
        let m = (r.duration_s / 60.0) as u32;
        let s = ((r.duration_s - m as f64 * 60.0) as u32).min(59);
        println!("  session {}: UTC {}, duration {:02}:{:02}, p90 {:.1} km/h",
                 i + 1, r.utc_start, m, s, r.p90_kmh);
    }

    // Height above water
    let height_m = baro::height_above_water_m(&sensors, &gps_rows, &gps_speed, base_ticks);

    // Bin sensor series to 10 Hz (100 ms buckets) for display — matches the
    // Python visualiser and keeps the HTML small (~1 MB vs ~7 MB raw).
    let (nose_t, nose_binned) =
        bin_util::bin_to_resolution(&t_sensor_s, &nose_deg, 100, bin_util::Agg::Mean);
    let (_, height_binned) =
        bin_util::bin_to_resolution(&t_sensor_s, &height_m, 100, bin_util::Agg::Mean);

    // Render HTML
    let title = format!("{} — {:.1} min · {} ride{}",
        stem,
        dur_s / 60.0,
        rides.len(),
        if rides.len() == 1 { "" } else { "s" });
    let page = html::render(&html::PanelData {
        t_sensor_s: &nose_t,
        nose_deg: &nose_binned,
        height_m: &height_binned,
        gps: &gps_rows,
        gps_speed_kmh: &gps_speed,
        gps_t_s: &gps_t_s,
        rides: &rides,
        title: &title,
    });

    std::fs::create_dir_all(&cli.output)
        .with_context(|| format!("mkdir {}", cli.output.display()))?;
    let out_path = cli.output.join(format!("viz_{}.html", stem));
    std::fs::write(&out_path, page)
        .with_context(|| format!("write {}", out_path.display()))?;
    println!("  wrote {}", out_path.display());
    println!("done.");
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
