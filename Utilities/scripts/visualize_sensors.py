#!/usr/bin/env python3
"""
Visualize STEVAL-MKBOXPRO sensor data and quaternion orientation data.

Expects CSV files as exported by the ST BLE Sensor app:
  - Sensor CSV with columns: dd/mm/yyyy, hh:mm:ss.ms, IMU accX[mg], ...
  - Quaternion CSV with columns: Qi, Qj, Qk, Qs

Usage:
  python visualize_sensors.py sensor_data.csv [quaternion_data.csv] [-o OUTPUT_DIR]
"""
import argparse
import os
import sys

import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np


def plot_sensors(csv_path, output_dir):
    df = pd.read_csv(csv_path)
    df.columns = [c.strip().rstrip(',') for c in df.columns]

    df['timestamp'] = pd.to_datetime(
        df['dd/mm/yyyy'] + ' ' + df['hh:mm:ss.ms'],
        format='%d/%m/%Y %H:%M:%S.%f'
    )
    t = df['timestamp']
    t_sec = (t - t.iloc[0]).dt.total_seconds()

    fig, axes = plt.subplots(5, 1, figsize=(14, 16), sharex=True)
    title = os.path.basename(csv_path).replace('.csv', '')

    axes[0].plot(t_sec, df['IMU accX[mg]'], label='X', alpha=0.8)
    axes[0].plot(t_sec, df['IMU accY[mg]'], label='Y', alpha=0.8)
    axes[0].plot(t_sec, df['accZ[mg]'], label='Z', alpha=0.8)
    axes[0].set_ylabel('Acc [mg]')
    axes[0].set_title(f'Sensordaten (STEVAL-MKBOXPRO) - {title}\nBeschleunigung', fontsize=13)
    axes[0].legend(loc='upper right')
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(t_sec, df['gyroX[mdps]'], label='X', alpha=0.8)
    axes[1].plot(t_sec, df['gyroY[mdps]'], label='Y', alpha=0.8)
    axes[1].plot(t_sec, df['gyroZ[mdps]'], label='Z', alpha=0.8)
    axes[1].set_ylabel('Gyro [mdps]')
    axes[1].set_title('Drehrate')
    axes[1].legend(loc='upper right')
    axes[1].grid(True, alpha=0.3)

    axes[2].plot(t_sec, df['magX[mG]'], label='X', alpha=0.8)
    axes[2].plot(t_sec, df['magY[mG]'], label='Y', alpha=0.8)
    axes[2].plot(t_sec, df['magZ[mG]'], label='Z', alpha=0.8)
    axes[2].set_ylabel('Mag [mGauss]')
    axes[2].set_title('Magnetfeld')
    axes[2].legend(loc='upper right')
    axes[2].grid(True, alpha=0.3)

    axes[3].plot(t_sec, df['T[degC]'], color='tab:red')
    axes[3].set_ylabel('T [°C]')
    axes[3].set_title('Temperatur')
    axes[3].grid(True, alpha=0.3)

    axes[4].plot(t_sec, df['Press[hPa]'], color='tab:purple')
    axes[4].set_ylabel('P [hPa]')
    axes[4].set_title('Luftdruck')
    axes[4].set_xlabel('Zeit [s]')
    axes[4].grid(True, alpha=0.3)

    plt.tight_layout()
    base = os.path.basename(csv_path).replace('.csv', '')
    out_path = os.path.join(output_dir, f'plot_sensors_{base}.png')
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"Saved {out_path}")


def plot_quaternions(csv_path, output_dir):
    quat_data = pd.read_csv(csv_path, skiprows=1, header=None, usecols=[0, 1, 2, 3])
    quat_data.columns = ['Qi', 'Qj', 'Qk', 'Qs']
    # Quaternion sample rate: 120 Hz
    sample_rate_hz = 120
    t_sec = np.arange(len(quat_data)) / sample_rate_hz

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 8))

    def fmt_min_sec(x, pos):
        m, s = divmod(int(x), 60)
        return f'{m:d}:{s:02d}'

    from matplotlib.ticker import FuncFormatter
    time_fmt = FuncFormatter(fmt_min_sec)

    title = os.path.basename(csv_path).replace('.csv', '')
    ax1.plot(t_sec, quat_data['Qi'], label='Qi', alpha=0.8)
    ax1.plot(t_sec, quat_data['Qj'], label='Qj', alpha=0.8)
    ax1.plot(t_sec, quat_data['Qk'], label='Qk', alpha=0.8)
    ax1.plot(t_sec, quat_data['Qs'], label='Qs', alpha=0.8)
    ax1.set_ylabel('Quaternion-Komponenten')
    ax1.set_title(f'Quaternion / Orientierung - {title}\nQuaternion-Verlauf', fontsize=13)
    ax1.legend(loc='upper right')
    ax1.grid(True, alpha=0.3)
    ax1.xaxis.set_major_formatter(time_fmt)
    ax1.set_xlabel('Zeit [min:sek]')

    qi, qj, qk, qs = quat_data['Qi'], quat_data['Qj'], quat_data['Qk'], quat_data['Qs']

    sinr_cosp = 2.0 * (qs * qi + qj * qk)
    cosr_cosp = 1.0 - 2.0 * (qi * qi + qj * qj)
    roll = np.degrees(np.arctan2(sinr_cosp, cosr_cosp))

    sinp = 2.0 * (qs * qj - qk * qi)
    pitch = np.degrees(np.where(np.abs(sinp) >= 1, np.sign(sinp) * np.pi / 2, np.arcsin(sinp)))

    siny_cosp = 2.0 * (qs * qk + qi * qj)
    cosy_cosp = 1.0 - 2.0 * (qj * qj + qk * qk)
    yaw = np.degrees(np.arctan2(siny_cosp, cosy_cosp))

    ax2.plot(t_sec, roll, label='Roll', alpha=0.8)
    ax2.plot(t_sec, pitch, label='Pitch', alpha=0.8)
    ax2.plot(t_sec, yaw, label='Yaw', alpha=0.8)
    ax2.set_ylabel('Winkel [°]')
    ax2.set_title('Euler-Winkel (aus Quaternionen berechnet)')
    ax2.legend(loc='upper right')
    ax2.grid(True, alpha=0.3)
    ax2.xaxis.set_major_formatter(time_fmt)
    ax2.set_xlabel('Zeit [min:sek]')

    plt.tight_layout()
    base = os.path.basename(csv_path).replace('.csv', '')
    out_path = os.path.join(output_dir, f'plot_quaternions_{base}.png')
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"Saved {out_path}")

    # Detect pumpfoil sessions using Euler angle activity
    # During pumping the board oscillates in pitch/roll; idle periods are flat
    euler_change = (np.abs(np.diff(pitch, prepend=pitch[0]))
                    + np.abs(np.diff(roll, prepend=roll[0])))
    activity = pd.Series(euler_change).rolling(sample_rate_hz).mean().fillna(0)
    threshold = activity.median() + activity.std()
    active = (activity > threshold).astype(int)
    changes = active.diff().fillna(0)
    starts = changes[changes == 1].index.tolist()
    ends = changes[changes == -1].index.tolist()
    if active.iloc[-1] == 1:
        ends.append(len(active) - 1)

    # Merge segments with gaps < 60 seconds into sessions
    merge_gap = 60 * sample_rate_hz
    sessions = []
    for s, e in zip(starts, ends):
        if sessions and (s - sessions[-1][1]) < merge_gap:
            sessions[-1] = (sessions[-1][0], e)
        else:
            sessions.append((s, e))

    # Filter out short bursts (< 30 seconds) — real sessions are minutes long
    min_duration = 30 * sample_rate_hz
    sessions = [(s, e) for s, e in sessions if (e - s) >= min_duration]

    # Filter out walking: pumping has rhythmic pitch oscillation > 0.3 Hz
    pumping_sessions = []
    for s, e in sessions:
        p = pitch[s:e]
        p_centered = p - np.median(p)
        crossings = np.sum(np.diff(np.sign(p_centered)) != 0)
        duration = (e - s) / sample_rate_hz
        osc_freq = crossings / duration / 2
        if osc_freq >= 0.3:
            pumping_sessions.append((s, e))
    sessions = pumping_sessions

    if not sessions:
        return

    # Add 5-second padding around each session
    pad = 5 * sample_rate_hz
    for i, (s, e) in enumerate(sessions, 1):
        s_padded = max(0, s - pad)
        e_padded = min(len(quat_data) - 1, e + pad)
        t_start = s_padded / sample_rate_hz
        t_end = e_padded / sample_rate_hz

        fig_z, (az1, az2, az3) = plt.subplots(3, 1, figsize=(14, 12))
        sl = slice(s_padded, e_padded)
        az1.plot(t_sec[sl], quat_data['Qi'].iloc[s_padded:e_padded], label='Qi', alpha=0.8)
        az1.plot(t_sec[sl], quat_data['Qj'].iloc[s_padded:e_padded], label='Qj', alpha=0.8)
        az1.plot(t_sec[sl], quat_data['Qk'].iloc[s_padded:e_padded], label='Qk', alpha=0.8)
        az1.plot(t_sec[sl], quat_data['Qs'].iloc[s_padded:e_padded], label='Qs', alpha=0.8)
        az1.set_ylabel('Quaternion-Komponenten')

        # Detect drop-in: first rapid pitch change > 10° within 0.5s in the session
        drop_in_idx = None
        window = int(0.5 * sample_rate_hz)
        for idx in range(s, min(e - window, len(pitch) - window)):
            delta = abs(pitch[idx + window] - pitch[idx])
            if delta >= 10:
                drop_in_idx = idx
                break

        duration_s = (e - s) / sample_rate_hz
        dm, ds = divmod(int(duration_s), 60)
        session_label = f'Session {i} - {title} (Dauer: {dm}:{ds:02d})'
        if drop_in_idx is not None:
            drop_t = t_sec[drop_in_idx]
            for ax in (az1, az2, az3):
                ax.axvline(drop_t, color='red', linestyle='--', alpha=0.7, label='Drop-in')
            dm2, ds2 = divmod(int(drop_t), 60)
            session_label += f' | Drop-in: {dm2}:{ds2:02d}'

        # Board nose angle to water: rotate sensor Y-axis (nose direction,
        # sensor mounted in Breitachse = X across board) by quaternion,
        # then compute elevation angle from horizontal.
        qi_a = quat_data['Qi'].values
        qj_a = quat_data['Qj'].values
        qk_a = quat_data['Qk'].values
        qs_a = quat_data['Qs'].values
        # Rotated Y-axis [0,1,0] z-component: nose elevation
        nose_z = 2 * (qj_a * qk_a - qs_a * qi_a)
        nose_elev = np.degrees(np.arcsin(np.clip(nose_z, -1, 1)))
        # Median filter: remove magnetometer correction spikes in sensor fusion
        # Use 1-second window to catch multi-sample correction bursts
        smooth_window = sample_rate_hz
        nose_smooth = pd.Series(nose_elev).rolling(
            smooth_window, center=True, min_periods=1).median().values

        # High-pass filter: remove baseline drift
        # Mask crash values before computing baseline
        nose_series = pd.Series(nose_smooth)
        rough_baseline = nose_series.rolling(
            10 * sample_rate_hz, center=True, min_periods=1).median()
        stable_mask = (nose_smooth - rough_baseline).abs() < 20
        nose_stable = nose_series.copy()
        nose_stable[~stable_mask] = np.nan
        baseline = nose_stable.rolling(
            60 * sample_rate_hz, center=True, min_periods=1).median()
        baseline = baseline.interpolate(method='linear').bfill().ffill().values
        nose_angle = nose_smooth - baseline

        # Mark end-of-session crash: last 5 seconds of the session
        crash_start = e - 5 * sample_rate_hz
        crash_count = 1
        for ax in (az1, az2, az3):
            ax.axvspan(t_sec[crash_start], t_sec[e], alpha=0.15, color='red', zorder=0)

        az3.plot(t_sec[sl], nose_angle[s_padded:e_padded], color='tab:blue', alpha=0.8, label='Nasenwinkel')
        az3.axhline(0, color='gray', linestyle='-', linewidth=1, alpha=0.5)
        az3.fill_between(t_sec[sl], 0, nose_angle[s_padded:e_padded],
                         where=nose_angle[s_padded:e_padded] >= 0,
                         alpha=0.2, color='tab:orange', label='Nase hoch')
        az3.fill_between(t_sec[sl], 0, nose_angle[s_padded:e_padded],
                         where=nose_angle[s_padded:e_padded] < 0,
                         alpha=0.2, color='tab:blue', label='Nase runter')
        az3.set_ylabel('Winkel [°]')
        # Auto-scale y-axis to show full data range with some padding
        session_data = nose_angle[s_padded:e_padded]
        y_max = max(abs(np.nanmin(session_data)), abs(np.nanmax(session_data))) * 1.1
        y_max = max(y_max, 10)  # minimum ±10°
        az3.set_ylim(-y_max, y_max)
        az3.set_title('Nasenwinkel zur Wasseroberflaeche (Nase hoch = +, runter = -)')
        az3.grid(True, alpha=0.3)
        az3.xaxis.set_major_formatter(time_fmt)
        az3.set_xlabel('Zeit [min:sek]')

        session_label += ' | Sturz am Ende'

        az1.set_title(f'{session_label}\nQuaternion-Verlauf', fontsize=13)
        az1.grid(True, alpha=0.3)
        az1.xaxis.set_major_formatter(time_fmt)
        az1.set_xlabel('Zeit [min:sek]')

        az2.plot(t_sec[sl], roll[s_padded:e_padded], label='Roll', alpha=0.8)
        az2.plot(t_sec[sl], pitch[s_padded:e_padded], label='Pitch', alpha=0.8)
        az2.plot(t_sec[sl], yaw[s_padded:e_padded], label='Yaw', alpha=0.8)
        az2.set_ylabel('Winkel [°]')
        az2.set_title('Euler-Winkel (aus Quaternionen berechnet)')
        az2.grid(True, alpha=0.3)
        az2.xaxis.set_major_formatter(time_fmt)
        az2.set_xlabel('Zeit [min:sek]')

        # Add legends with crash patch
        from matplotlib.patches import Patch
        crash_patch = Patch(facecolor='red', alpha=0.15, label='Sturz')
        for ax in (az1, az2, az3):
            h, l = ax.get_legend_handles_labels()
            h.append(crash_patch)
            l.append('Sturz')
            ax.legend(handles=h, labels=l, loc='upper right')

        plt.tight_layout()
        out_z = os.path.join(output_dir, f'plot_quaternions_{base}_session{i}.png')
        fig_z.savefig(out_z, dpi=150)
        plt.close(fig_z)
        print(f"Saved {out_z}")


def main():
    parser = argparse.ArgumentParser(description='Visualize STEVAL-MKBOXPRO sensor data')
    parser.add_argument('sensor_csv', help='Path to sensor CSV (ST BLE Sensor app format)')
    parser.add_argument('quaternion_csv', nargs='?', default=None,
                        help='Path to quaternion CSV (optional)')
    parser.add_argument('-o', '--output-dir', default='.',
                        help='Output directory for PNG plots (default: current dir)')
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)
    plot_sensors(args.sensor_csv, args.output_dir)
    if args.quaternion_csv:
        plot_quaternions(args.quaternion_csv, args.output_dir)


if __name__ == '__main__':
    main()
