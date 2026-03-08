#!/usr/bin/env python3
"""
Board orientation animation from quaternion sensor data.

Creates animated GIFs per pumpfoil session showing the board tilting
according to the quaternion sensor data, with optional combined
side-by-side MOV output synchronized with camera footage.

Usage:
  # GIF only (all sessions)
  python animate_board_3d.py quaternion_data.csv -o gif/

  # Combined MOV with camera video (single session)
  python animate_board_3d.py quaternion_data.csv -o mov/ \
    --video camera.MOV --video-offset 56 --sensor-offset 1.5 \
    --session 1 --title "PumpGraph Mirco" --subtitle "ONIX Albatross 1160"
"""
import argparse
import os
import subprocess
import sys
import tempfile

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from matplotlib.animation import FuncAnimation, PillowWriter
from scipy.signal import butter, filtfilt


SAMPLE_RATE_HZ = 120


def quat_rotate(q, v):
    """Rotate vector v by quaternion q = (qi, qj, qk, qs) (Hamilton convention)."""
    qi, qj, qk, qs = q
    t = 2.0 * np.cross([qi, qj, qk], v)
    return v + qs * t + np.cross([qi, qj, qk], t)


def remove_yaw(qi, qj, qk, qs):
    """Remove yaw (heading) from quaternion, keeping only pitch and roll."""
    siny_cosp = 2.0 * (qs * qk + qi * qj)
    cosy_cosp = 1.0 - 2.0 * (qj * qj + qk * qk)
    yaw = np.arctan2(siny_cosp, cosy_cosp)
    half_yaw = -yaw / 2.0
    qz_i, qz_j, qz_k, qz_s = 0.0, 0.0, np.sin(half_yaw), np.cos(half_yaw)
    r_s = qz_s * qs - qz_i * qi - qz_j * qj - qz_k * qk
    r_i = qz_s * qi + qz_i * qs + qz_j * qk - qz_k * qj
    r_j = qz_s * qj - qz_i * qk + qz_j * qs + qz_k * qi
    r_k = qz_s * qk + qz_i * qj - qz_j * qi + qz_k * qs
    return r_i, r_j, r_k, r_s


def detect_sessions(csv_path):
    """Detect pumping sessions (reuses logic from visualize_sensors.py)."""
    quat_data = pd.read_csv(csv_path, skiprows=1, header=None, usecols=[0, 1, 2, 3])
    quat_data.columns = ['Qi', 'Qj', 'Qk', 'Qs']

    qi = quat_data['Qi'].values
    qj = quat_data['Qj'].values
    qk = quat_data['Qk'].values
    qs = quat_data['Qs'].values

    sinr_cosp = 2.0 * (qs * qi + qj * qk)
    cosr_cosp = 1.0 - 2.0 * (qi * qi + qj * qj)
    roll = np.degrees(np.arctan2(sinr_cosp, cosr_cosp))

    sinp = 2.0 * (qs * qj - qk * qi)
    pitch = np.degrees(np.where(np.abs(sinp) >= 1, np.sign(sinp) * np.pi / 2, np.arcsin(sinp)))

    euler_change = (np.abs(np.diff(pitch, prepend=pitch[0]))
                    + np.abs(np.diff(roll, prepend=roll[0])))
    activity = pd.Series(euler_change).rolling(SAMPLE_RATE_HZ).mean().fillna(0)
    threshold = activity.median() + activity.std()
    active = (activity > threshold).astype(int)
    changes = active.diff().fillna(0)
    starts = changes[changes == 1].index.tolist()
    ends = changes[changes == -1].index.tolist()
    if active.iloc[-1] == 1:
        ends.append(len(activity) - 1)

    merge_gap = 60 * SAMPLE_RATE_HZ
    sessions = []
    for s, e in zip(starts, ends):
        if sessions and (s - sessions[-1][1]) < merge_gap:
            sessions[-1] = (sessions[-1][0], e)
        else:
            sessions.append((s, e))

    min_duration = 30 * SAMPLE_RATE_HZ
    sessions = [(s, e) for s, e in sessions if (e - s) >= min_duration]

    pumping_sessions = []
    for s, e in sessions:
        p = pitch[s:e]
        p_centered = p - np.median(p)
        crossings = np.sum(np.diff(np.sign(p_centered)) != 0)
        duration = (e - s) / SAMPLE_RATE_HZ
        osc_freq = crossings / duration / 2
        if osc_freq >= 0.3:
            pumping_sessions.append((s, e))

    return quat_data, pumping_sessions


def create_board_animation(quat_data, session_start, session_end, session_num,
                           output_dir, base_name, fps=15):
    """Create a 2D side-view animated GIF showing nose angle pumping motion.

    Top panel: board seen from the side (line tilting up/down) with water surface.
    Middle panel: pump detail at ±5° scale.
    Bottom panel: full-range nose angle time series with moving cursor.
    """
    qi_all = quat_data['Qi'].values
    qj_all = quat_data['Qj'].values
    qk_all = quat_data['Qk'].values
    qs_all = quat_data['Qs'].values

    # Compute nose angle on full recording
    nose_z_full = 2 * (qj_all * qk_all - qs_all * qi_all)
    nose_deg_full = np.degrees(np.arcsin(np.clip(nose_z_full, -1, 1)))
    # Butterworth low-pass at 2 Hz: preserves ~1 Hz pump oscillation
    # (amplitude ±3.1° vs ±3.6° raw) while removing high-frequency
    # noise that causes wobbling in the 15fps animation
    b, a = butter(4, 2, fs=SAMPLE_RATE_HZ)
    nose_smooth_full = filtfilt(b, a, nose_deg_full)

    # Baseline drift correction: 10s centered rolling median
    # Tracks rider's actual riding angle, removes sensor drift,
    # without disturbing the ~1 Hz pump oscillation
    baseline = pd.Series(nose_smooth_full).rolling(
        10 * SAMPLE_RATE_HZ, center=True, min_periods=1).median().values
    nose_corrected_full = nose_smooth_full - baseline

    # Slice to session
    nose_smooth = nose_corrected_full[session_start:session_end]
    t_sess = np.arange(session_end - session_start) / SAMPLE_RATE_HZ

    # Detect drop-in: steepest (most negative) angle in the first 10s
    drop_search = nose_smooth[:min(10 * SAMPLE_RATE_HZ, len(nose_smooth))]
    drop_sample_idx = int(np.argmin(drop_search))
    drop_angle = drop_search[drop_sample_idx]
    drop_time = drop_sample_idx / SAMPLE_RATE_HZ
    # Flash duration: show text for 2 seconds after the steepest point
    drop_flash_end = drop_time + 2.0

    # Subsample for animation frames
    step = max(1, SAMPLE_RATE_HZ // fps)
    frame_indices = np.arange(0, len(nose_smooth), step)
    n_frames = len(frame_indices)

    # Board geometry for side view (half-length)
    board_len = 1.6  # total length in display units
    hl = board_len / 2

    duration_s = (session_end - session_start) / SAMPLE_RATE_HZ
    dm, ds = divmod(int(duration_s), 60)

    fig, (ax_board, ax_zoom, ax_graph) = plt.subplots(3, 1, figsize=(12, 9),
                                              gridspec_kw={'height_ratios': [2, 1, 1]})
    fig.suptitle(f'Session {session_num} - {base_name} (Dauer: {dm}:{ds:02d})',
                 fontsize=14)

    # Set up zoom axis (±5° scale)
    zoom_lim = 5.0
    ax_zoom.axhline(0, color='gray', linestyle='-', linewidth=1, alpha=0.5)
    ax_zoom.set_ylabel('Winkel [°]')
    ax_zoom.set_title('Pump-Detail (±5°)', fontsize=11)
    ax_zoom.grid(True, alpha=0.3)

    # Set up full-range axis
    y_lim = max(abs(np.nanmin(nose_smooth)), abs(np.nanmax(nose_smooth))) * 1.1
    y_lim = max(y_lim, 30)
    ax_graph.axhline(0, color='gray', linestyle='-', linewidth=1, alpha=0.5)
    ax_graph.set_ylabel('Nasenwinkel [°]')
    ax_graph.set_xlabel('Zeit [sek]')
    ax_graph.grid(True, alpha=0.3)

    # Animated elements — lines build up progressively
    cursor_line, = ax_graph.plot([], [], color='red', linewidth=2)
    history_line, = ax_graph.plot([], [], color='tab:blue', linewidth=1.2, alpha=0.8)
    cursor_zoom, = ax_zoom.plot([], [], color='red', linewidth=2)
    history_zoom, = ax_zoom.plot([], [], color='tab:green', linewidth=1.2, alpha=0.8)

    def draw_frame(frame_idx):
        fi = frame_indices[frame_idx]
        angle = nose_smooth[fi]
        t_now = t_sess[fi]

        # --- Board side view ---
        ax_board.clear()
        rad = np.radians(angle)
        # Board tilts AND translates vertically:
        # vertical offset proportional to angle (0.05m per degree)
        # so ±3° pump gives ±0.15m visible movement
        lift = angle * 0.05
        tail_y = -hl * np.cos(rad)
        tail_z = -hl * np.sin(rad) + lift
        nose_y = hl * np.cos(rad)
        nose_z_pt = hl * np.sin(rad) + lift

        # Water surface (fixed at 0)
        ax_board.fill_between([-2, 2], [-0.8, -0.8], [0, 0],
                               color='#b3d9ff', alpha=0.4)
        ax_board.axhline(0, color='#0077be', linewidth=2, alpha=0.7)

        # Board body (thick line)
        ax_board.plot([tail_y, nose_y], [tail_z, nose_z_pt],
                      color='#2266aa', linewidth=8, solid_capstyle='round')
        # Nose marker (red dot)
        ax_board.plot(nose_y, nose_z_pt, 'o', color='red', markersize=14, zorder=5)
        ax_board.text(nose_y + 0.08, nose_z_pt + 0.05, 'Nase', color='red',
                      fontsize=10, fontweight='bold')
        # Tail marker
        ax_board.plot(tail_y, tail_z, 's', color='#224466', markersize=10, zorder=5)
        ax_board.text(tail_y - 0.25, tail_z + 0.05, 'Heck', color='#224466',
                      fontsize=10)

        ax_board.set_xlim(-1.3, 1.3)
        ax_board.set_ylim(-0.8, 0.8)
        ax_board.set_aspect('equal')
        ax_board.set_ylabel('Hoehe relativ [m]')
        ax_board.grid(True, alpha=0.2)

        ax_board.text(0.02, 0.88, f'Nasenwinkel: {angle:+.1f}°',
                      transform=ax_board.transAxes, fontsize=16,
                      color='red', fontweight='bold')
        tm, ts_val = divmod(int(t_now), 60)
        ax_board.text(0.98, 0.88, f'Zeit: {tm}:{ts_val:02d}',
                      transform=ax_board.transAxes, fontsize=14,
                      ha='right')

        # Flash "Dropwinkel" text at the steepest drop-in moment
        if drop_time <= t_now <= drop_flash_end:
            # Fade out: alpha goes from 1.0 to 0.0 over 2 seconds
            fade = max(0, 1.0 - (t_now - drop_time) / 2.0)
            ax_board.text(0.5, 0.15, f'Dropwinkel: {drop_angle:.1f}°',
                          transform=ax_board.transAxes, fontsize=22,
                          color='red', fontweight='bold', ha='center',
                          alpha=fade)

        # --- Update cursors on graphs with dynamic x-axis ---
        x_right = t_now + 2
        for ax in (ax_zoom, ax_graph):
            ax.set_xlim(t_sess[0], x_right)
        ax_zoom.set_ylim(-zoom_lim, zoom_lim)
        ax_graph.set_ylim(-y_lim, y_lim)

        cursor_line.set_data([t_now, t_now], [-y_lim, y_lim])
        history_line.set_data(t_sess[:fi + 1], nose_smooth[:fi + 1])
        cursor_zoom.set_data([t_now, t_now], [-zoom_lim, zoom_lim])
        history_zoom.set_data(t_sess[:fi + 1],
                              np.clip(nose_smooth[:fi + 1], -zoom_lim, zoom_lim))

    print(f"  Generating {n_frames} frames for Session {session_num}...")
    anim = FuncAnimation(fig, draw_frame, frames=n_frames, interval=1000 // fps)

    out_path = os.path.join(output_dir, f'anim_board_{base_name}_session{session_num}.gif')
    anim.save(out_path, writer=PillowWriter(fps=fps))
    plt.close(fig)
    print(f"Saved {out_path}")
    return out_path


def create_title_frame(width, height, title, subtitle, path):
    """Create a black title frame PNG with green title and light blue subtitle."""
    fig, ax = plt.subplots(figsize=(width / 100, height / 100), dpi=100)
    fig.patch.set_facecolor('black')
    ax.set_facecolor('black')
    ax.set_xlim(0, 1)
    ax.set_ylim(0, 1)
    ax.axis('off')
    if title:
        y_title = 0.55 if subtitle else 0.5
        ax.text(0.5, y_title, title, color='#00ff00', fontsize=56,
                fontweight='bold', ha='center', va='center', family='sans-serif')
    if subtitle:
        ax.text(0.5, 0.40, subtitle, color='#87CEFA', fontsize=40,
                fontweight='bold', ha='center', va='center', family='sans-serif')
    fig.savefig(path, dpi=100, facecolor='black', pad_inches=0, bbox_inches='tight')
    plt.close()


def create_combined_mov(gif_path, video_path, video_offset, sensor_offset,
                        output_path, title=None, subtitle=None, fps=30):
    """Create a side-by-side MOV: camera video (left) + sensor animation (right).

    Optionally prepends a 2-second title card.
    """
    # Get video duration from offset to end
    probe = subprocess.run(
        ['ffprobe', '-v', 'error', '-show_entries', 'format=duration',
         '-of', 'default=noprint_wrappers=1:nokey=1', video_path],
        capture_output=True, text=True)
    total_duration = float(probe.stdout.strip())
    ride_duration = total_duration - video_offset

    with tempfile.TemporaryDirectory() as tmpdir:
        main_clip = os.path.join(tmpdir, 'main.mov')

        # Create main combined clip
        subprocess.run([
            'ffmpeg', '-y',
            '-ss', str(video_offset), '-i', video_path,
            '-ss', str(sensor_offset), '-i', gif_path,
            '-filter_complex',
            '[0:v]scale=-1:900[vid];[1:v]scale=-1:900[gif];[vid][gif]hstack=inputs=2',
            '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-r', str(fps),
            '-t', str(ride_duration), '-an',
            main_clip
        ], capture_output=True)

        if title or subtitle:
            # Get combined video dimensions
            probe2 = subprocess.run(
                ['ffprobe', '-v', 'error', '-select_streams', 'v:0',
                 '-show_entries', 'stream=width,height',
                 '-of', 'csv=p=0', main_clip],
                capture_output=True, text=True)
            w, h = [int(x) for x in probe2.stdout.strip().split(',')]

            # Create title frame and clip
            title_png = os.path.join(tmpdir, 'title.png')
            title_clip = os.path.join(tmpdir, 'title.mov')
            create_title_frame(w, h, title, subtitle, title_png)

            subprocess.run([
                'ffmpeg', '-y', '-loop', '1', '-i', title_png,
                '-t', '2', '-c:v', 'libx264', '-pix_fmt', 'yuv420p',
                '-vf', f'scale={w}:{h}', '-r', str(fps),
                title_clip
            ], capture_output=True)

            # Concat title + main
            concat_list = os.path.join(tmpdir, 'concat.txt')
            with open(concat_list, 'w') as f:
                f.write(f"file '{title_clip}'\nfile '{main_clip}'\n")

            subprocess.run([
                'ffmpeg', '-y', '-f', 'concat', '-safe', '0', '-i', concat_list,
                '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-movflags', '+faststart',
                output_path
            ], capture_output=True)
        else:
            # No title, just copy main clip with faststart
            subprocess.run([
                'ffmpeg', '-y', '-i', main_clip,
                '-c:v', 'copy', '-movflags', '+faststart',
                output_path
            ], capture_output=True)

    print(f"Saved {output_path}")


def main():
    parser = argparse.ArgumentParser(
        description='Board orientation animation from quaternion sensor data',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Examples:
  # Generate GIFs for all sessions
  python animate_board_3d.py csv/mirco_7.3.2026.csv -o gif/

  # Generate combined MOV for session 1 with camera video
  python animate_board_3d.py csv/mirco_7.3.2026.csv -o mov/ \\
    --video ~/Downloads/session_1_7.3.2026.MOV \\
    --video-offset 56 --sensor-offset 1.5 \\
    --session 1 \\
    --title "PumpGraph Mirco" --subtitle "ONIX Albatross 1160"
""")
    parser.add_argument('quaternion_csv', help='Path to quaternion CSV')
    parser.add_argument('-o', '--output-dir', default='.', help='Output directory (default: .)')
    parser.add_argument('--fps', type=int, default=15, help='Frames per second (default: 15)')
    parser.add_argument('--session', type=int, default=None,
                        help='Generate only this session number (default: all)')
    parser.add_argument('--video', default=None,
                        help='Camera video file (MOV/MP4) for combined side-by-side output')
    parser.add_argument('--video-offset', type=float, default=0,
                        help='Start time in video in seconds (default: 0)')
    parser.add_argument('--sensor-offset', type=float, default=0,
                        help='Start time in sensor GIF in seconds (default: 0)')
    parser.add_argument('--title', default=None,
                        help='Title text for MOV title card (green, bold)')
    parser.add_argument('--subtitle', default=None,
                        help='Subtitle text for MOV title card (light blue, bold)')
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)
    base = os.path.basename(args.quaternion_csv).replace('.csv', '')

    print(f"Loading {args.quaternion_csv}...")
    quat_data, sessions = detect_sessions(args.quaternion_csv)

    if not sessions:
        print("No pumping sessions detected.")
        sys.exit(1)

    print(f"Found {len(sessions)} pumping session(s).")

    for i, (s, e) in enumerate(sessions, 1):
        if args.session is not None and i != args.session:
            continue
        dur = (e - s) / SAMPLE_RATE_HZ
        dm, ds = divmod(int(dur), 60)
        print(f"  Session {i}: {dm}:{ds:02d} ({e - s} samples)")

        gif_dir = args.output_dir if not args.video else tempfile.mkdtemp()
        gif_path = create_board_animation(quat_data, s, e, i,
                                          gif_dir if not args.video else gif_dir,
                                          base, fps=args.fps)

        if args.video:
            # Save GIF alongside MOV for reference
            gif_final = os.path.join(args.output_dir,
                                     f'anim_board_{base}_session{i}.gif')
            if gif_dir != args.output_dir:
                import shutil
                shutil.move(gif_path, gif_final)
                gif_path = gif_final

            mov_path = os.path.join(args.output_dir,
                                    f'combined_session{i}_drop.mov')
            create_combined_mov(gif_path, args.video,
                                args.video_offset, args.sensor_offset,
                                mov_path, args.title, args.subtitle,
                                fps=args.fps * 2)


if __name__ == '__main__':
    main()
