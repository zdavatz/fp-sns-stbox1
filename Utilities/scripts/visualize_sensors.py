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
    samples = np.arange(len(quat_data))

    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 8))

    title = os.path.basename(csv_path).replace('.csv', '')
    ax1.plot(samples, quat_data['Qi'], label='Qi', alpha=0.8)
    ax1.plot(samples, quat_data['Qj'], label='Qj', alpha=0.8)
    ax1.plot(samples, quat_data['Qk'], label='Qk', alpha=0.8)
    ax1.plot(samples, quat_data['Qs'], label='Qs', alpha=0.8)
    ax1.set_ylabel('Quaternion-Komponenten')
    ax1.set_title(f'Quaternion / Orientierung - {title}\nQuaternion-Verlauf', fontsize=13)
    ax1.legend(loc='upper right')
    ax1.grid(True, alpha=0.3)
    ax1.set_xlabel('Sample')

    qi, qj, qk, qs = quat_data['Qi'], quat_data['Qj'], quat_data['Qk'], quat_data['Qs']

    sinr_cosp = 2.0 * (qs * qi + qj * qk)
    cosr_cosp = 1.0 - 2.0 * (qi * qi + qj * qj)
    roll = np.degrees(np.arctan2(sinr_cosp, cosr_cosp))

    sinp = 2.0 * (qs * qj - qk * qi)
    pitch = np.degrees(np.where(np.abs(sinp) >= 1, np.sign(sinp) * np.pi / 2, np.arcsin(sinp)))

    siny_cosp = 2.0 * (qs * qk + qi * qj)
    cosy_cosp = 1.0 - 2.0 * (qj * qj + qk * qk)
    yaw = np.degrees(np.arctan2(siny_cosp, cosy_cosp))

    ax2.plot(samples, roll, label='Roll', alpha=0.8)
    ax2.plot(samples, pitch, label='Pitch', alpha=0.8)
    ax2.plot(samples, yaw, label='Yaw', alpha=0.8)
    ax2.set_ylabel('Winkel [°]')
    ax2.set_title('Euler-Winkel (aus Quaternionen berechnet)')
    ax2.legend(loc='upper right')
    ax2.grid(True, alpha=0.3)
    ax2.set_xlabel('Sample')

    plt.tight_layout()
    base = os.path.basename(csv_path).replace('.csv', '')
    out_path = os.path.join(output_dir, f'plot_quaternions_{base}.png')
    fig.savefig(out_path, dpi=150)
    plt.close(fig)
    print(f"Saved {out_path}")


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
