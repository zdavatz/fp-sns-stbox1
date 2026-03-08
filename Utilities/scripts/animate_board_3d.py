#!/usr/bin/env python3
"""
3D board orientation animation from quaternion data.

Creates an animated GIF per pumpfoil session showing the board tilting
according to the quaternion sensor data. The board is rendered as a flat
rectangle; a nose marker (red) shows the front direction.

Usage:
  python animate_board_3d.py quaternion_data.csv [-o OUTPUT_DIR] [--fps 30]
"""
import argparse
import os
import sys

import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection
from matplotlib.animation import FuncAnimation, PillowWriter


SAMPLE_RATE_HZ = 120


def quat_rotate(q, v):
    """Rotate vector v by quaternion q = (qi, qj, qk, qs) (Hamilton convention)."""
    qi, qj, qk, qs = q
    # q * v * q_conj using expanded formula
    t = 2.0 * np.cross([qi, qj, qk], v)
    return v + qs * t + np.cross([qi, qj, qk], t)


def remove_yaw(qi, qj, qk, qs):
    """Remove yaw (heading) from quaternion, keeping only pitch and roll.
    This makes the board always face the same direction in the animation."""
    # Compute yaw angle
    siny_cosp = 2.0 * (qs * qk + qi * qj)
    cosy_cosp = 1.0 - 2.0 * (qj * qj + qk * qk)
    yaw = np.arctan2(siny_cosp, cosy_cosp)
    # Create inverse yaw quaternion (rotation around Z by -yaw)
    half_yaw = -yaw / 2.0
    qz_i, qz_j, qz_k, qz_s = 0.0, 0.0, np.sin(half_yaw), np.cos(half_yaw)
    # Multiply: q_no_yaw = q_inv_yaw * q_original
    # Hamilton product (a * b)
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

    # Euler angles for session detection
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
        ends.append(len(active) - 1)

    merge_gap = 60 * SAMPLE_RATE_HZ
    sessions = []
    for s, e in zip(starts, ends):
        if sessions and (s - sessions[-1][1]) < merge_gap:
            sessions[-1] = (sessions[-1][0], e)
        else:
            sessions.append((s, e))

    min_duration = 30 * SAMPLE_RATE_HZ
    sessions = [(s, e) for s, e in sessions if (e - s) >= min_duration]

    # Filter walking
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
                           output_dir, base_name, fps=30):
    """Create a 2D side-view animated GIF showing nose angle pumping motion.

    Top panel: board seen from the side (line tilting up/down) with water surface.
    Bottom panel: nose angle time series with moving cursor.
    """
    qi_all = quat_data['Qi'].values
    qj_all = quat_data['Qj'].values
    qk_all = quat_data['Qk'].values
    qs_all = quat_data['Qs'].values

    # Pre-compute nose angle for entire session
    nose_z = 2 * (qj_all[session_start:session_end] * qk_all[session_start:session_end]
                  - qs_all[session_start:session_end] * qi_all[session_start:session_end])
    nose_deg = np.degrees(np.arcsin(np.clip(nose_z, -1, 1)))
    # Smooth with 1s median filter (matches visualize_sensors.py)
    nose_smooth = pd.Series(nose_deg).rolling(
        SAMPLE_RATE_HZ, center=True, min_periods=1).median().values

    # Baseline drift correction (same as visualize_sensors.py):
    # Remove magnetometer/gyro drift using crash-masked 60s rolling median
    nose_series = pd.Series(nose_smooth)
    rough_baseline = nose_series.rolling(
        10 * SAMPLE_RATE_HZ, center=True, min_periods=1).median()
    stable_mask = (nose_smooth - rough_baseline).abs() < 20
    nose_stable = nose_series.copy()
    nose_stable[~stable_mask] = np.nan
    baseline = nose_stable.rolling(
        60 * SAMPLE_RATE_HZ, center=True, min_periods=1).median()
    baseline = baseline.interpolate(method='linear').bfill().ffill().values
    nose_smooth = nose_smooth - baseline
    t_sess = np.arange(len(nose_deg)) / SAMPLE_RATE_HZ

    # Subsample for animation frames
    step = max(1, SAMPLE_RATE_HZ // fps)
    frame_indices = np.arange(0, len(nose_deg), step)
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

    # Pre-draw the zoomed time series on ax_zoom (±5° scale)
    ax_zoom.plot(t_sess, nose_smooth, color='tab:green', alpha=0.6, linewidth=1.0)
    ax_zoom.axhline(0, color='gray', linestyle='-', linewidth=1, alpha=0.5)
    ax_zoom.set_xlim(t_sess[0], t_sess[-1])
    zoom_lim = 5.0
    ax_zoom.set_ylim(-zoom_lim, zoom_lim)
    ax_zoom.set_ylabel('Winkel [°]')
    ax_zoom.set_title('Pump-Detail (±5°)', fontsize=11)
    ax_zoom.fill_between(t_sess, 0, np.clip(nose_smooth, 0, zoom_lim),
                         alpha=0.2, color='tab:orange')
    ax_zoom.fill_between(t_sess, np.clip(nose_smooth, -zoom_lim, 0), 0,
                         alpha=0.2, color='tab:blue')
    ax_zoom.grid(True, alpha=0.3)

    # Pre-draw the full time series on ax_graph (static background)
    ax_graph.plot(t_sess, nose_smooth, color='tab:blue', alpha=0.6, linewidth=0.8)
    ax_graph.axhline(0, color='gray', linestyle='-', linewidth=1, alpha=0.5)
    ax_graph.set_xlim(t_sess[0], t_sess[-1])
    y_lim = max(abs(np.nanmin(nose_smooth)), abs(np.nanmax(nose_smooth))) * 1.1
    y_lim = max(y_lim, 30)
    ax_graph.set_ylim(-y_lim, y_lim)
    ax_graph.set_ylabel('Nasenwinkel [°]')
    ax_graph.set_xlabel('Zeit [sek]')
    ax_graph.grid(True, alpha=0.3)

    # Animated elements
    cursor_line, = ax_graph.plot([], [], color='red', linewidth=2)
    history_line, = ax_graph.plot([], [], color='red', linewidth=1.5, alpha=0.8)
    cursor_zoom, = ax_zoom.plot([], [], color='red', linewidth=2)
    history_zoom, = ax_zoom.plot([], [], color='red', linewidth=1.5, alpha=0.8)
    angle_text = ax_board.text(0.02, 0.92, '', transform=ax_board.transAxes,
                                fontsize=16, color='red', fontweight='bold')
    time_text = ax_board.text(0.98, 0.92, '', transform=ax_board.transAxes,
                               fontsize=14, ha='right')

    def draw_frame(frame_idx):
        fi = frame_indices[frame_idx]
        angle = nose_smooth[fi]
        t_now = t_sess[fi]

        # --- Board side view ---
        ax_board.clear()
        rad = np.radians(angle)
        # Board as thick line seen from side: tail on left, nose on right
        tail_y = -hl * np.cos(rad)
        tail_z = -hl * np.sin(rad)
        nose_y = hl * np.cos(rad)
        nose_z_pt = hl * np.sin(rad)

        # Water surface
        ax_board.fill_between([-2, 2], [-0.6, -0.6], [0, 0],
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

        # Angle arc
        if abs(angle) > 1:
            arc_angles = np.linspace(0, rad, 30)
            arc_r = 0.4
            arc_y = arc_r * np.cos(arc_angles)
            arc_z = arc_r * np.sin(arc_angles)
            ax_board.plot(arc_y, arc_z, color='red', linewidth=1.5, alpha=0.7)

        ax_board.set_xlim(-1.3, 1.3)
        ax_board.set_ylim(-0.6, 0.6)
        ax_board.set_aspect('equal')
        ax_board.set_ylabel('Hoehe [m]')
        ax_board.grid(True, alpha=0.2)

        angle_text = ax_board.text(0.02, 0.88, f'Nasenwinkel: {angle:+.1f}°',
                                    transform=ax_board.transAxes, fontsize=16,
                                    color='red', fontweight='bold')
        tm, ts_val = divmod(int(t_now), 60)
        time_text = ax_board.text(0.98, 0.88, f'Zeit: {tm}:{ts_val:02d}',
                                   transform=ax_board.transAxes, fontsize=14,
                                   ha='right')

        # --- Update cursors on graphs ---
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


def main():
    parser = argparse.ArgumentParser(description='3D board orientation animation from quaternion data')
    parser.add_argument('quaternion_csv', help='Path to quaternion CSV')
    parser.add_argument('-o', '--output-dir', default='.', help='Output directory')
    parser.add_argument('--fps', type=int, default=15, help='Frames per second (default: 15)')
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
        dur = (e - s) / SAMPLE_RATE_HZ
        dm, ds = divmod(int(dur), 60)
        print(f"  Session {i}: {dm}:{ds:02d} ({e - s} samples)")
        create_board_animation(quat_data, s, e, i, args.output_dir, base, fps=args.fps)


if __name__ == '__main__':
    main()
