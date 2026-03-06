#!/usr/bin/env python3
"""
Pumpfoil session analysis for STEVAL-MKBOXPRO sensor data.

Generates two plots:
  1. Pump cadence spectrogram — pumping frequency over time
  2. Movement phases & intensity — active pumping vs rest vs crashes

Usage:
  python visualize_pumpfoil.py sensor_data.csv [-o OUTPUT_DIR]

Requires: pandas, numpy, matplotlib, scipy
"""
import argparse
import os

import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from scipy import signal


def load_sensor_data(csv_path):
    df = pd.read_csv(csv_path)
    df.columns = [c.strip().rstrip(',') for c in df.columns]
    df['timestamp'] = pd.to_datetime(
        df['dd/mm/yyyy'] + ' ' + df['hh:mm:ss.ms'],
        format='%d/%m/%Y %H:%M:%S.%f'
    )
    t_sec = (df['timestamp'] - df['timestamp'].iloc[0]).dt.total_seconds().values

    acc_x = df['IMU accX[mg]'].values.astype(float)
    acc_y = df['IMU accY[mg]'].values.astype(float)
    acc_z = df['accZ[mg]'].values.astype(float)
    gyro_x = df['gyroX[mdps]'].values.astype(float)
    gyro_y = df['gyroY[mdps]'].values.astype(float)
    gyro_z = df['gyroZ[mdps]'].values.astype(float)
    press = df['Press[hPa]'].values.astype(float)

    return t_sec, acc_x, acc_y, acc_z, gyro_x, gyro_y, gyro_z, press


def plot_pump_cadence(t_sec, acc_x, acc_y, acc_z, fs, output_dir):
    plt.rcParams.update({'font.size': 11})

    window = 100
    acc_mag = np.sqrt(acc_x**2 + acc_y**2 + acc_z**2)
    acc_mag_smooth = pd.Series(acc_mag).rolling(window, center=True, min_periods=1).mean().values
    acc_dynamic = acc_mag - acc_mag_smooth

    fig, (ax_top, ax_spec, ax_cad) = plt.subplots(3, 1, figsize=(16, 12),
        gridspec_kw={'height_ratios': [1, 2, 1]}, sharex=True)

    ax_top.plot(t_sec, acc_dynamic, color='steelblue', alpha=0.5, linewidth=0.3)
    env = pd.Series(np.abs(acc_dynamic)).rolling(500, center=True, min_periods=1).mean().values
    ax_top.plot(t_sec, env, color='navy', linewidth=1.5, label='Envelope')
    ax_top.set_ylabel('Dyn. Acc [mg]')
    ax_top.set_title('Pump-Kadenz-Analyse - Pumpfoil Session\nDynamische Beschleunigung (Gravitation entfernt)', fontsize=13)
    ax_top.legend(loc='upper right')
    ax_top.grid(True, alpha=0.3)

    nperseg = int(fs * 4)
    noverlap = int(nperseg * 0.9)
    f, t_spec, Sxx = signal.spectrogram(acc_dynamic, fs=fs, nperseg=nperseg,
                                          noverlap=noverlap, scaling='spectrum')

    freq_mask = (f >= 0.3) & (f <= 5.0)
    f_plot = f[freq_mask]
    Sxx_plot = Sxx[freq_mask, :]

    ax_spec.pcolormesh(t_spec, f_plot, 10 * np.log10(Sxx_plot + 1e-10),
                        shading='gouraud', cmap='inferno')
    ax_spec.set_ylabel('Frequenz [Hz]')
    ax_spec.set_title('Spektrogramm — Pump-Frequenz über Zeit')
    ax_spec.axhline(y=1.0, color='cyan', linestyle='--', alpha=0.5, linewidth=0.8, label='1 Hz')
    ax_spec.axhline(y=2.0, color='lime', linestyle='--', alpha=0.5, linewidth=0.8, label='2 Hz')
    ax_spec.legend(loc='upper right', fontsize=9)

    dominant_freq = f_plot[np.argmax(Sxx_plot, axis=0)]
    energy = np.max(Sxx_plot, axis=0)
    energy_threshold = np.percentile(energy, 40)
    pumping_mask = energy > energy_threshold

    ax_cad.scatter(t_spec[pumping_mask], dominant_freq[pumping_mask],
                   c=dominant_freq[pumping_mask], cmap='coolwarm', s=8, vmin=0.5, vmax=3.0)
    ax_cad.set_ylabel('Kadenz [Hz]')
    ax_cad.set_xlabel('Zeit [s]')
    ax_cad.set_title('Dominante Pump-Frequenz (nur aktive Phasen)')
    ax_cad.set_ylim(0.3, 4.0)
    ax_cad.grid(True, alpha=0.3)
    ax_cad.axhline(y=1.0, color='gray', linestyle=':', alpha=0.5)
    ax_cad.axhline(y=2.0, color='gray', linestyle=':', alpha=0.5)

    if pumping_mask.any():
        med_cad = np.median(dominant_freq[pumping_mask])
        ax_cad.axhline(y=med_cad, color='red', linestyle='-', alpha=0.7, linewidth=1.5)
        ax_cad.text(t_spec[pumping_mask][-1] + 10, med_cad,
                    f'Median: {med_cad:.2f} Hz\n({med_cad*60:.0f} pumps/min)',
                    color='red', fontsize=10, va='center')

    plt.tight_layout()
    out_path = os.path.join(output_dir, 'plot_pump_cadence.png')
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"Saved {out_path}")


def plot_pump_phases(t_sec, acc_x, acc_y, acc_z, gyro_x, gyro_y, gyro_z, press, fs, output_dir):
    plt.rcParams.update({'font.size': 11})

    window = 100
    acc_mag = np.sqrt(acc_x**2 + acc_y**2 + acc_z**2)
    acc_mag_smooth = pd.Series(acc_mag).rolling(window, center=True, min_periods=1).mean().values
    acc_dynamic = acc_mag - acc_mag_smooth
    gyro_mag = np.sqrt(gyro_x**2 + gyro_y**2 + gyro_z**2)

    window_2s = int(fs * 2)
    gyro_rms = pd.Series(gyro_mag).rolling(window_2s, center=True, min_periods=1).apply(
        lambda x: np.sqrt(np.mean(x**2)), raw=True).values / 1000
    acc_rms = pd.Series(np.abs(acc_dynamic)).rolling(window_2s, center=True, min_periods=1).apply(
        lambda x: np.sqrt(np.mean(x**2)), raw=True).values

    phase = np.zeros(len(t_sec), dtype=int)
    gyro_thresh_active = np.percentile(gyro_rms[~np.isnan(gyro_rms)], 60)
    gyro_thresh_crash = np.percentile(gyro_rms[~np.isnan(gyro_rms)], 95)
    acc_thresh_crash = np.percentile(acc_rms[~np.isnan(acc_rms)], 95)

    phase[(gyro_rms > gyro_thresh_active)] = 1
    phase[(gyro_rms > gyro_thresh_crash) & (acc_rms > acc_thresh_crash)] = 2
    phase_smooth = pd.Series(phase).rolling(200, center=True, min_periods=1).median().values.astype(int)

    colors_phase = {0: '#3498db', 1: '#2ecc71', 2: '#e74c3c'}
    labels_phase = {0: 'Ruhe/Gleiten', 1: 'Aktives Pumpen', 2: 'Sturz/Recovery'}

    fig, axes = plt.subplots(4, 1, figsize=(16, 13),
        gridspec_kw={'height_ratios': [2, 1, 1, 1]}, sharex=True)

    ax = axes[0]
    ax.fill_between(t_sec, 0, gyro_rms, alpha=0.3, color='gray')
    for p_val, color in colors_phase.items():
        mask = phase_smooth == p_val
        ax.fill_between(t_sec, 0, np.where(mask, gyro_rms, 0), alpha=0.6,
                         color=color, label=labels_phase[p_val])
    ax.set_ylabel('Rotations-\nintensitaet [dps]')
    ax.set_title('Bewegungsphasen & Intensitaet - Pumpfoil Session\nBewegungsintensitaet (Gyro RMS)', fontsize=13)
    ax.legend(loc='upper right')
    ax.grid(True, alpha=0.3)

    total_time = t_sec[-1] - t_sec[0]
    for p_val, label in labels_phase.items():
        pct = np.sum(phase_smooth == p_val) / len(phase_smooth) * 100
        duration = pct / 100 * total_time
        ax.text(0.01, 0.95 - p_val * 0.08,
                f'{label}: {duration:.0f}s ({pct:.0f}%)',
                transform=ax.transAxes, fontsize=9, color=colors_phase[p_val],
                fontweight='bold', va='top')

    ax1 = axes[1]
    gyro_x_smooth = pd.Series(np.abs(gyro_x / 1000)).rolling(window_2s, center=True, min_periods=1).mean().values
    gyro_y_smooth = pd.Series(np.abs(gyro_y / 1000)).rolling(window_2s, center=True, min_periods=1).mean().values
    gyro_z_smooth = pd.Series(np.abs(gyro_z / 1000)).rolling(window_2s, center=True, min_periods=1).mean().values
    ax1.plot(t_sec, gyro_x_smooth, label='Roll (X)', alpha=0.8)
    ax1.plot(t_sec, gyro_y_smooth, label='Pitch (Y)', alpha=0.8)
    ax1.plot(t_sec, gyro_z_smooth, label='Yaw (Z)', alpha=0.8)
    ax1.set_ylabel('Drehrate [dps]')
    ax1.set_title('Rotationsachsen — welche Achse dominiert die Pump-Bewegung?')
    ax1.legend(loc='upper right')
    ax1.grid(True, alpha=0.3)

    ax2 = axes[2]
    ax2.plot(t_sec, acc_mag, color='steelblue', alpha=0.3, linewidth=0.3)
    acc_env = pd.Series(acc_mag).rolling(500, center=True, min_periods=1).mean().values
    ax2.plot(t_sec, acc_env, color='navy', linewidth=1.5)
    ax2.axhline(y=1000, color='gray', linestyle=':', alpha=0.5, label='1g')
    ax2.set_ylabel('|Acc| [mg]')
    ax2.set_title('Beschleunigungs-Betrag (Spitzen = Stuerze/Landungen)')
    ax2.legend(loc='upper right')
    ax2.grid(True, alpha=0.3)

    ax3 = axes[3]
    ax3.plot(t_sec, press, color='tab:purple', linewidth=1)
    ax3.set_ylabel('P [hPa]')
    ax3.set_xlabel('Zeit [s]')
    ax3.set_title('Luftdruck (niedrigerer Druck = hoeher ueber Wasser / Standort-Wechsel)')
    ax3.grid(True, alpha=0.3)
    ax3.invert_yaxis()

    plt.tight_layout()
    out_path = os.path.join(output_dir, 'plot_pump_phases.png')
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"Saved {out_path}")


def main():
    parser = argparse.ArgumentParser(description='Pumpfoil session analysis')
    parser.add_argument('sensor_csv', help='Path to sensor CSV (ST BLE Sensor app format)')
    parser.add_argument('-o', '--output-dir', default='.',
                        help='Output directory for PNG plots (default: current dir)')
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    t_sec, acc_x, acc_y, acc_z, gyro_x, gyro_y, gyro_z, press = load_sensor_data(args.sensor_csv)

    dt = np.median(np.diff(t_sec))
    fs = 1.0 / dt
    print(f"Estimated sample rate: {fs:.1f} Hz")

    plot_pump_cadence(t_sec, acc_x, acc_y, acc_z, fs, args.output_dir)
    plot_pump_phases(t_sec, acc_x, acc_y, acc_z, gyro_x, gyro_y, gyro_z, press, fs, args.output_dir)


if __name__ == '__main__':
    main()
