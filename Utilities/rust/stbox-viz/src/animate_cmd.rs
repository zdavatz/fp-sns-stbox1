//! `stbox-viz animate` — animated GIF (and optional combined MOV) of
//! board side-view tilting with nose-angle + pump-detail panels.
//! Port of `Utilities/scripts/animate_board_3d.py`.
//!
//! 3-panel layout:
//!   - Top: side view of the board tilting + translating vertically
//!   - Middle: pump detail at ±5°
//!   - Bottom: full-range nose angle with cursor
//!
//! Butterworth 2 Hz low-pass (4th-order, zero-phase) + 10 s rolling-
//! median baseline before animating. Progressive line build-up: history
//! line grows with the cursor, no future data shown.
//!
//! With `--video <path>`, pipes the resulting GIF through ffmpeg into a
//! side-by-side MOV next to camera footage.

use crate::butter::{butter4_lowpass, filtfilt};
use crate::euler::quats_to_euler_deg;
use crate::fusion;
use crate::io::load_sensor_csv;
use crate::plot_common::FONT;
use crate::session::detect_sessions;
use anyhow::{Context, Result};
use plotters::prelude::*;
use std::path::{Path, PathBuf};
use std::process::Command;

pub struct AnimateArgs<'a> {
    pub sensor_csv: &'a Path,
    pub output_dir: &'a Path,
    pub fps: u32,
    pub session: Option<usize>,
    pub video: Option<&'a Path>,
    pub video_offset: f64,
    pub sensor_offset: f64,
    pub title: Option<&'a str>,
    pub subtitle: Option<&'a str>,
}

pub fn run(args: &AnimateArgs) -> Result<()> {
    std::fs::create_dir_all(args.output_dir)
        .with_context(|| format!("mkdir {}", args.output_dir.display()))?;

    println!("loading {}", args.sensor_csv.file_name().unwrap().to_string_lossy());
    let samples = load_sensor_csv(args.sensor_csv)?;
    if samples.is_empty() {
        anyhow::bail!("sensor CSV is empty");
    }
    let base = args.sensor_csv.file_stem().unwrap().to_string_lossy().to_string();

    println!("running Madgwick 6DOF fusion");
    let quats = fusion::compute_quaternions(&samples, 0.1);
    let (roll, pitch, _yaw) = quats_to_euler_deg(&quats);

    let sample_hz: usize = 100;
    let sessions = detect_sessions(&pitch, &roll, sample_hz);
    if sessions.is_empty() {
        anyhow::bail!("no pumping sessions detected");
    }
    println!("found {} pumping session(s)", sessions.len());

    // Nose angle for animation: 2 Hz Butterworth + 10 s baseline.
    let nose_raw = nose_angle_deg_raw(&quats);
    let (b, a) = butter4_lowpass(2.0, sample_hz as f64);
    let nose_smooth = filtfilt(&b, &a, &nose_raw);
    let baseline = rolling_median(&nose_smooth, 10 * sample_hz);
    let nose_corrected: Vec<f64> = nose_smooth.iter().zip(baseline.iter())
        .map(|(s, b)| s - b).collect();

    for (i, &(s, e)) in sessions.iter().enumerate() {
        let session_num = i + 1;
        if let Some(target) = args.session {
            if session_num != target { continue; }
        }

        let dur = (e - s) as f64 / sample_hz as f64;
        println!("  session {}: {:.0} s ({} samples)", session_num, dur, e - s);

        let gif_path = args.output_dir.join(
            format!("anim_board_{}_session{}.gif", base, session_num));
        render_session_gif(
            &nose_corrected[s..e],
            session_num,
            &base,
            sample_hz,
            args.fps,
            &gif_path,
        )?;
        println!("Saved {}", gif_path.display());

        if let Some(video_path) = args.video {
            let mov_path = args.output_dir.join(
                format!("combined_session{}_drop.mov", session_num));
            combine_with_ffmpeg(
                &gif_path, video_path, args.video_offset, args.sensor_offset,
                &mov_path, args.title, args.subtitle, args.fps * 2,
            )?;
            println!("Saved {}", mov_path.display());
        }
    }

    Ok(())
}

/// Raw nose elevation in degrees from quaternions.
fn nose_angle_deg_raw(quats: &[fusion::Quat]) -> Vec<f64> {
    quats.iter().map(|q| {
        let (qs, qi, qj, qk) = (q[0], q[1], q[2], q[3]);
        let nose_z = (2.0 * (qj * qk - qs * qi)).clamp(-1.0, 1.0);
        nose_z.asin().to_degrees()
    }).collect()
}

fn rolling_median(x: &[f64], window: usize) -> Vec<f64> {
    let n = x.len();
    let half = window / 2;
    let mut out = vec![0.0; n];
    for i in 0..n {
        let lo = i.saturating_sub(half);
        let hi = (i + half + 1).min(n);
        let mut s: Vec<f64> = x[lo..hi].to_vec();
        s.sort_by(|a, b| a.partial_cmp(b).unwrap());
        out[i] = s[s.len() / 2];
    }
    out
}

fn render_session_gif(
    nose: &[f64],
    session_num: usize,
    base: &str,
    sample_hz: usize,
    fps: u32,
    out_path: &PathBuf,
) -> Result<()> {
    // Subsample so animation runs at `fps`.
    let step = (sample_hz as u32 / fps).max(1) as usize;
    let frame_indices: Vec<usize> = (0..nose.len()).step_by(step).collect();
    let n_frames = frame_indices.len();

    let t_sess: Vec<f64> = (0..nose.len()).map(|i| i as f64 / sample_hz as f64).collect();
    let duration_s = *t_sess.last().unwrap_or(&0.0);
    let dm = (duration_s / 60.0) as u32;
    let ds = ((duration_s % 60.0) as u32).min(59);

    // Drop-in: steepest (min) nose angle in first 10 s
    let first10 = &nose[..nose.len().min(10 * sample_hz)];
    let drop_idx = first10.iter().enumerate()
        .min_by(|a, b| a.1.partial_cmp(b.1).unwrap())
        .map(|(i, _)| i).unwrap_or(0);
    let drop_angle = nose[drop_idx];
    let drop_time = drop_idx as f64 / sample_hz as f64;
    let drop_flash_end = drop_time + 2.0;

    // Axis bounds
    let y_lim = {
        let mut mx = 30f64;
        for &v in nose {
            if v.is_finite() && v.abs() > mx { mx = v.abs(); }
        }
        mx * 1.1
    };
    let zoom_lim = 5.0f64;

    // Bitmap frame size
    let (w, h) = (1200u32, 900u32);

    let title_line = format!("Session {} — {} (Dauer: {}:{:02})", session_num, base, dm, ds);

    let root = BitMapBackend::gif(out_path, (w, h), 1000 / fps)?
        .into_drawing_area();

    println!("  generating {} frames…", n_frames);
    for (frame, &fi) in frame_indices.iter().enumerate() {
        root.fill(&WHITE)?;
        let t_now = fi as f64 / sample_hz as f64;
        let angle = nose[fi];
        draw_frame(
            &root,
            &title_line,
            &nose[..=fi],
            &t_sess[..=fi],
            t_now,
            angle,
            y_lim,
            zoom_lim,
            drop_time,
            drop_flash_end,
            drop_angle,
        )?;
        root.present()?;
        if frame % 100 == 0 {
            print!("\r    frame {}/{}", frame, n_frames);
            use std::io::Write;
            std::io::stdout().flush().ok();
        }
    }
    println!("\r    frame {}/{}  ", n_frames, n_frames);
    Ok(())
}

fn draw_frame<DB: DrawingBackend>(
    root: &DrawingArea<DB, plotters::coord::Shift>,
    title: &str,
    nose_hist: &[f64],
    t_hist: &[f64],
    t_now: f64,
    angle: f64,
    y_lim: f64,
    zoom_lim: f64,
    drop_time: f64,
    drop_flash_end: f64,
    drop_angle: f64,
) -> Result<(), anyhow::Error>
where
    DB::ErrorType: 'static,
{
    // Title across the top
    root.draw(&Text::new(
        title.to_string(),
        (10, 4),
        (FONT, 18).into_font().color(&BLACK),
    )).map_err(|e| anyhow::anyhow!("title: {e:?}"))?;

    // Split into 3 panels (2:1:1 height)
    let (board_area, rest) = root.split_vertically(430);
    let (zoom_area, graph_area) = rest.split_vertically(225);

    // --- Board side view ---
    let rad = angle.to_radians();
    let hl = 0.8f64;        // half-length
    let lift = angle * 0.05;
    let nose_x = hl * rad.cos();
    let nose_y = hl * rad.sin() + lift;
    let tail_x = -hl * rad.cos();
    let tail_y = -hl * rad.sin() + lift;

    let mut bc = ChartBuilder::on(&board_area)
        .margin(20)
        .x_label_area_size(30)
        .y_label_area_size(60)
        .build_cartesian_2d(-1.3f64..1.3f64, -0.8f64..0.8f64)
        .map_err(|e| anyhow::anyhow!("board chart: {e:?}"))?;
    bc.configure_mesh()
        .y_desc("Höhe [m]")
        .light_line_style(RGBColor(240, 240, 240))
        .draw().map_err(|e| anyhow::anyhow!("mesh: {e:?}"))?;

    // Water
    bc.draw_series(std::iter::once(Rectangle::new(
        [(-2.0, -0.8), (2.0, 0.0)],
        RGBColor(179, 217, 255).mix(0.4).filled(),
    ))).map_err(|e| anyhow::anyhow!("water: {e:?}"))?;
    bc.draw_series(std::iter::once(PathElement::new(
        vec![(-2.0, 0.0), (2.0, 0.0)],
        RGBColor(0, 119, 190).stroke_width(2),
    ))).map_err(|e| anyhow::anyhow!("waterline: {e:?}"))?;

    // Board
    bc.draw_series(std::iter::once(PathElement::new(
        vec![(tail_x, tail_y), (nose_x, nose_y)],
        RGBColor(34, 102, 170).stroke_width(10),
    ))).map_err(|e| anyhow::anyhow!("board: {e:?}"))?;
    bc.draw_series(std::iter::once(Circle::new(
        (nose_x, nose_y), 7, RGBColor(220, 20, 20).filled(),
    ))).map_err(|e| anyhow::anyhow!("nose: {e:?}"))?;
    bc.draw_series(std::iter::once(Rectangle::new(
        [(tail_x - 0.03, tail_y - 0.03), (tail_x + 0.03, tail_y + 0.03)],
        RGBColor(34, 68, 102).filled(),
    ))).map_err(|e| anyhow::anyhow!("tail: {e:?}"))?;

    // Nose angle / time overlays
    let (bw, _bh) = board_area.dim_in_pixel();
    board_area.draw(&Text::new(
        format!("Nasenwinkel: {:+.1}°", angle),
        (30, 40),
        (FONT, 22).into_font().color(&RGBColor(220, 20, 20)),
    )).map_err(|e| anyhow::anyhow!("angle_txt: {e:?}"))?;
    let tm = (t_now / 60.0) as u32;
    let ts = ((t_now % 60.0) as u32).min(59);
    board_area.draw(&Text::new(
        format!("Zeit: {}:{:02}", tm, ts),
        (bw as i32 - 180, 40),
        (FONT, 20).into_font().color(&BLACK),
    )).map_err(|e| anyhow::anyhow!("time_txt: {e:?}"))?;

    // Drop-in flash
    if t_now >= drop_time && t_now <= drop_flash_end {
        let fade = (1.0 - (t_now - drop_time) / 2.0).max(0.0);
        if fade > 0.02 {
            let red = RGBColor(220, 20, 20).mix(fade);
            board_area.draw(&Text::new(
                format!("Dropwinkel: {:.1}°", drop_angle),
                (bw as i32 / 2 - 160, 90),
                (FONT, 32).into_font().color(&red),
            )).map_err(|e| anyhow::anyhow!("drop_txt: {e:?}"))?;
        }
    }

    // --- Zoom panel (±5°) ---
    let x_right = t_now + 2.0;
    let mut zc = ChartBuilder::on(&zoom_area)
        .margin(20)
        .x_label_area_size(30)
        .y_label_area_size(60)
        .build_cartesian_2d(0.0f64..x_right.max(1.0), -zoom_lim..zoom_lim)
        .map_err(|e| anyhow::anyhow!("zoom chart: {e:?}"))?;
    zc.configure_mesh()
        .y_desc("Winkel [°]")
        .light_line_style(RGBColor(240, 240, 240))
        .draw().map_err(|e| anyhow::anyhow!("zoom mesh: {e:?}"))?;
    zc.draw_series(std::iter::once(PathElement::new(
        vec![(0.0, 0.0), (x_right.max(1.0), 0.0)],
        RGBColor(128, 128, 128).stroke_width(1),
    ))).map_err(|e| anyhow::anyhow!("zero: {e:?}"))?;
    zc.draw_series(LineSeries::new(
        t_hist.iter().zip(nose_hist.iter()).map(|(&t, &n)| (t, n.clamp(-zoom_lim, zoom_lim))),
        RGBColor(44, 160, 44).stroke_width(2),
    )).map_err(|e| anyhow::anyhow!("zoom line: {e:?}"))?;
    zc.draw_series(std::iter::once(PathElement::new(
        vec![(t_now, -zoom_lim), (t_now, zoom_lim)],
        RGBColor(220, 20, 20).stroke_width(2),
    ))).map_err(|e| anyhow::anyhow!("cursor: {e:?}"))?;

    // --- Full-range panel ---
    let mut gc = ChartBuilder::on(&graph_area)
        .margin(20)
        .x_label_area_size(40)
        .y_label_area_size(60)
        .build_cartesian_2d(0.0f64..x_right.max(1.0), -y_lim..y_lim)
        .map_err(|e| anyhow::anyhow!("graph chart: {e:?}"))?;
    gc.configure_mesh()
        .y_desc("Nasenwinkel [°]")
        .x_desc("Zeit [s]")
        .light_line_style(RGBColor(240, 240, 240))
        .draw().map_err(|e| anyhow::anyhow!("graph mesh: {e:?}"))?;
    gc.draw_series(std::iter::once(PathElement::new(
        vec![(0.0, 0.0), (x_right.max(1.0), 0.0)],
        RGBColor(128, 128, 128).stroke_width(1),
    ))).map_err(|e| anyhow::anyhow!("zero: {e:?}"))?;
    gc.draw_series(LineSeries::new(
        t_hist.iter().zip(nose_hist.iter()).map(|(&t, &n)| (t, n)),
        RGBColor(31, 119, 180).stroke_width(2),
    )).map_err(|e| anyhow::anyhow!("graph line: {e:?}"))?;
    gc.draw_series(std::iter::once(PathElement::new(
        vec![(t_now, -y_lim), (t_now, y_lim)],
        RGBColor(220, 20, 20).stroke_width(2),
    ))).map_err(|e| anyhow::anyhow!("cursor: {e:?}"))?;

    Ok(())
}

fn combine_with_ffmpeg(
    gif_path: &PathBuf,
    video_path: &Path,
    video_offset: f64,
    sensor_offset: f64,
    out_path: &PathBuf,
    title: Option<&str>,
    subtitle: Option<&str>,
    fps: u32,
) -> Result<()> {
    // Main clip = camera (left) + GIF (right), scaled to 900px tall.
    let probe = Command::new("ffprobe")
        .args(["-v", "error", "-show_entries", "format=duration",
               "-of", "default=noprint_wrappers=1:nokey=1"])
        .arg(video_path)
        .output()
        .with_context(|| "running ffprobe — is it in PATH?")?;
    let total_s: f64 = String::from_utf8_lossy(&probe.stdout).trim().parse()
        .with_context(|| "parse video duration")?;
    let ride_s = (total_s - video_offset).max(1.0);

    let tmp = tempfile::tempdir()?;
    let main_clip = tmp.path().join("main.mov");
    let status = Command::new("ffmpeg")
        .args(["-y",
               "-ss", &video_offset.to_string(), "-i", video_path.to_str().unwrap(),
               "-ss", &sensor_offset.to_string(), "-i", gif_path.to_str().unwrap(),
               "-filter_complex",
               "[0:v]scale=-1:900[vid];[1:v]scale=-1:900[gif];[vid][gif]hstack=inputs=2",
               "-c:v", "libx264", "-pix_fmt", "yuv420p",
               "-r", &fps.to_string(),
               "-t", &ride_s.to_string(), "-an",
               main_clip.to_str().unwrap()])
        .status()
        .with_context(|| "running ffmpeg for main clip")?;
    if !status.success() { anyhow::bail!("ffmpeg main clip failed"); }

    if title.is_some() || subtitle.is_some() {
        // Probe combined dims
        let probe2 = Command::new("ffprobe")
            .args(["-v", "error", "-select_streams", "v:0",
                   "-show_entries", "stream=width,height",
                   "-of", "csv=p=0"])
            .arg(&main_clip)
            .output()
            .with_context(|| "ffprobe combined")?;
        let dims = String::from_utf8_lossy(&probe2.stdout);
        let mut it = dims.trim().split(',');
        let w: u32 = it.next().unwrap_or("1920").parse().unwrap_or(1920);
        let h: u32 = it.next().unwrap_or("900").parse().unwrap_or(900);

        let title_png = tmp.path().join("title.png");
        render_title_frame(w, h, title, subtitle, &title_png)?;
        let title_clip = tmp.path().join("title.mov");
        Command::new("ffmpeg")
            .args(["-y", "-loop", "1", "-i", title_png.to_str().unwrap(),
                   "-t", "2", "-c:v", "libx264", "-pix_fmt", "yuv420p",
                   "-vf", &format!("scale={}:{}", w, h), "-r", &fps.to_string(),
                   title_clip.to_str().unwrap()])
            .status().with_context(|| "ffmpeg title clip")?;

        let concat_txt = tmp.path().join("concat.txt");
        std::fs::write(&concat_txt, format!("file '{}'\nfile '{}'\n",
            title_clip.display(), main_clip.display()))?;
        Command::new("ffmpeg")
            .args(["-y", "-f", "concat", "-safe", "0",
                   "-i", concat_txt.to_str().unwrap(),
                   "-c:v", "libx264", "-pix_fmt", "yuv420p",
                   "-movflags", "+faststart",
                   out_path.to_str().unwrap()])
            .status().with_context(|| "ffmpeg concat")?;
    } else {
        Command::new("ffmpeg")
            .args(["-y", "-i", main_clip.to_str().unwrap(),
                   "-c:v", "copy", "-movflags", "+faststart",
                   out_path.to_str().unwrap()])
            .status().with_context(|| "ffmpeg copy")?;
    }

    Ok(())
}

fn render_title_frame(
    w: u32, h: u32, title: Option<&str>, subtitle: Option<&str>, out: &PathBuf,
) -> Result<()> {
    let root = BitMapBackend::new(out, (w, h)).into_drawing_area();
    root.fill(&BLACK)?;
    if let Some(t) = title {
        let y = if subtitle.is_some() { h as i32 * 45 / 100 } else { h as i32 / 2 - 30 };
        let approx_w = t.len() as i32 * 28;
        root.draw(&Text::new(
            t.to_string(),
            (w as i32 / 2 - approx_w / 2, y),
            (FONT, 56).into_font().color(&RGBColor(0, 255, 0)),
        ))?;
    }
    if let Some(st) = subtitle {
        let approx_w = st.len() as i32 * 20;
        root.draw(&Text::new(
            st.to_string(),
            (w as i32 / 2 - approx_w / 2, h as i32 * 60 / 100),
            (FONT, 40).into_font().color(&RGBColor(135, 206, 250)),
        ))?;
    }
    root.present()?;
    Ok(())
}

// Minimal tempfile substitute (we don't need the full crate).
mod tempfile {
    use std::path::PathBuf;
    pub struct TempDir { path: PathBuf }
    impl TempDir {
        pub fn path(&self) -> &std::path::Path { &self.path }
    }
    impl Drop for TempDir {
        fn drop(&mut self) {
            let _ = std::fs::remove_dir_all(&self.path);
        }
    }
    pub fn tempdir() -> std::io::Result<TempDir> {
        let mut p = std::env::temp_dir();
        let pid = std::process::id();
        let ts = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .map(|d| d.as_nanos()).unwrap_or(0);
        p.push(format!("stbox-viz-{}-{}", pid, ts));
        std::fs::create_dir_all(&p)?;
        Ok(TempDir { path: p })
    }
}
