#!/usr/bin/env python3
"""
Madgwick AHRS sensor fusion for STEVAL-MKBOXPRO SD card CSV data.

Converts raw accelerometer, gyroscope, and magnetometer readings into
quaternions using the Madgwick gradient-descent filter.

Usage:
    from sensor_fusion import compute_quaternions_from_csv
    df = compute_quaternions_from_csv("Sens001.csv")
    # df has columns: Qi, Qj, Qk, Qs

Quaternion convention: Hamilton (qi, qj, qk, qs) matching the existing
visualization scripts where nose_z = 2 * (qj * qk - qs * qi).
"""
import numpy as np
import pandas as pd


def _normalize(v):
    """Normalize a vector, return zeros if magnitude is zero."""
    n = np.linalg.norm(v)
    return v / n if n > 0 else v


class MadgwickAHRS:
    """Madgwick AHRS filter producing (qi, qj, qk, qs) quaternions."""

    def __init__(self, sample_rate=100.0, beta=0.1):
        self.sample_rate = sample_rate
        self.beta = beta
        # Initial quaternion: identity (scalar-last internally, output as qi,qj,qk,qs)
        # Internal: [w, x, y, z] = [1, 0, 0, 0]
        self.q = np.array([1.0, 0.0, 0.0, 0.0])

    def update(self, accel, gyro, mag):
        """
        Update filter with one sample.

        Parameters
        ----------
        accel : array-like, shape (3,)
            Accelerometer in g.
        gyro : array-like, shape (3,)
            Gyroscope in rad/s.
        mag : array-like, shape (3,)
            Magnetometer (normalized internally).
        """
        q = self.q.copy()
        q0, q1, q2, q3 = q

        ax, ay, az = accel
        gx, gy, gz = gyro
        mx, my, mz = mag

        # Rate of change from gyroscope
        qDot = 0.5 * np.array([
            -q1 * gx - q2 * gy - q3 * gz,
             q0 * gx + q2 * gz - q3 * gy,
             q0 * gy - q1 * gz + q3 * gx,
             q0 * gz + q1 * gy - q2 * gx,
        ])

        # Normalize accelerometer
        a_norm = np.linalg.norm([ax, ay, az])
        if a_norm == 0:
            self.q = _normalize(q + qDot / self.sample_rate)
            return

        ax, ay, az = ax / a_norm, ay / a_norm, az / a_norm

        # Normalize magnetometer
        m_norm = np.linalg.norm([mx, my, mz])
        if m_norm == 0:
            # Fall back to IMU-only (no mag correction)
            self.q = _normalize(q + qDot / self.sample_rate)
            return

        mx, my, mz = mx / m_norm, my / m_norm, mz / m_norm

        # Reference direction of Earth's magnetic field
        _2q0 = 2.0 * q0
        _2q1 = 2.0 * q1
        _2q2 = 2.0 * q2
        _2q3 = 2.0 * q3
        q0q0 = q0 * q0
        q0q1 = q0 * q1
        q0q2 = q0 * q2
        q0q3 = q0 * q3
        q1q1 = q1 * q1
        q1q2 = q1 * q2
        q1q3 = q1 * q3
        q2q2 = q2 * q2
        q2q3 = q2 * q3
        q3q3 = q3 * q3

        hx = mx * (q0q0 + q1q1 - q2q2 - q3q3) + 2.0 * my * (q1q2 - q0q3) + 2.0 * mz * (q1q3 + q0q2)
        hy = 2.0 * mx * (q1q2 + q0q3) + my * (q0q0 - q1q1 + q2q2 - q3q3) + 2.0 * mz * (q2q3 - q0q1)

        _2bx = np.sqrt(hx * hx + hy * hy)
        _2bz = 2.0 * mx * (q1q3 - q0q2) + 2.0 * my * (q2q3 + q0q1) + mz * (q0q0 - q1q1 - q2q2 + q3q3)

        # Gradient descent corrective step
        f = np.array([
            _2q1 * q3 - _2q0 * q2 - ax,
            _2q0 * q1 + _2q2 * q3 - ay,
            1.0 - _2q1 * q1 - _2q2 * q2 - az,
            _2bx * (0.5 - q2q2 - q3q3) + _2bz * (q1q3 - q0q2) - mx,
            _2bx * (q1q2 - q0q3) + _2bz * (q0q1 + q2q3) - my,
            _2bx * (q0q2 + q1q3) + _2bz * (0.5 - q1q1 - q2q2) - mz,
        ])

        J = np.array([
            [-_2q2, _2q3, -_2q0, _2q1],
            [_2q1, _2q0, _2q3, _2q2],
            [0.0, -2.0 * _2q1, -2.0 * _2q2, 0.0],
            [-_2bz * q2, _2bz * q3, -2.0 * _2bx * q2 - _2bz * q0, -2.0 * _2bx * q3 + _2bz * q1],
            [-_2bx * q3 + _2bz * q1, _2bx * q2 + _2bz * q0, _2bx * q1 + _2bz * q3, -_2bx * q0 + _2bz * q2],
            [_2bx * q2, _2bx * q3 - 2.0 * _2bz * q1, _2bx * q0 - 2.0 * _2bz * q2, _2bx * q1],
        ])

        step = J.T @ f
        step = _normalize(step)

        # Apply feedback
        qDot -= self.beta * step

        # Integrate
        self.q = _normalize(q + qDot / self.sample_rate)

    @property
    def quaternion_ijks(self):
        """Return quaternion as (qi, qj, qk, qs) matching visualization convention."""
        # Internal: [w, x, y, z] -> output: (x, y, z, w) = (qi, qj, qk, qs)
        return self.q[1], self.q[2], self.q[3], self.q[0]


def load_sd_csv(csv_path):
    """
    Load SD card CSV and return raw data with units as recorded.

    Returns
    -------
    df : pd.DataFrame
        Columns: time_ms, acc_x/y/z (mg), gyro_x/y/z (mdps),
        mag_x/y/z (mgauss), pressure (mB), temperature (C).
    """
    df = pd.read_csv(csv_path)
    df.columns = [c.strip().rstrip(',') for c in df.columns]

    out = pd.DataFrame()
    out['time_ms'] = df["Time [mS]"].values.astype(float)
    out['acc_x'] = df["AccX [mg]"].values.astype(float)
    out['acc_y'] = df["AccY [mg]"].values.astype(float)
    out['acc_z'] = df["AccZ [mg]"].values.astype(float)
    out['gyro_x'] = df["GyroX [mdps]"].values.astype(float)
    out['gyro_y'] = df["GyroY [mdps]"].values.astype(float)
    out['gyro_z'] = df["GyroZ [mdps]"].values.astype(float)
    out['mag_x'] = df["MagX [mgauss]"].values.astype(float)
    out['mag_y'] = df["MagY [mgauss]"].values.astype(float)
    out['mag_z'] = df["MagZ [mgauss]"].values.astype(float)
    out['pressure'] = df["P [mB]"].values.astype(float)
    out['temperature'] = df["T ['C]"].values.astype(float)
    return out


def compute_quaternions_from_csv(csv_path, beta=0.1):
    """
    Run Madgwick AHRS on SD card CSV and return quaternion DataFrame.

    Parameters
    ----------
    csv_path : str
        Path to sensor CSV with SD card format headers.
    beta : float
        Madgwick filter gain (default 0.1). Higher = more acc/mag trust,
        lower = more gyro trust.

    Returns
    -------
    pd.DataFrame
        Columns: Qi, Qj, Qk, Qs (one row per sample).
    """
    data = load_sd_csv(csv_path)

    # Auto-detect sample rate from timestamps
    # Note: Time column is ThreadX tick counter (1 tick = 10ms), not raw ms.
    # If median dt is ~1, these are tick counts at 100Hz (10ms ticks).
    # If median dt is ~10, these are actual ms at 100Hz.
    dt_raw = np.diff(data['time_ms'].values)
    dt_raw = dt_raw[dt_raw > 0]  # skip duplicates
    median_dt = np.median(dt_raw)
    if median_dt < 5:
        # Tick-based timestamps (1 tick = 10ms)
        sample_rate = 100.0
        print(f"sensor_fusion: tick-based timestamps (dt={median_dt:.1f} ticks), "
              f"using {sample_rate:.0f} Hz")
    else:
        sample_rate = 1000.0 / median_dt
        print(f"sensor_fusion: detected sample rate {sample_rate:.1f} Hz "
              f"(median dt = {median_dt:.1f} ms)")

    ahrs = MadgwickAHRS(sample_rate=sample_rate, beta=beta)

    n = len(data)
    quats = np.zeros((n, 4))

    # Pre-convert all units to numpy arrays for speed
    acc_all = np.column_stack([
        data['acc_x'].values / 1000.0,
        data['acc_y'].values / 1000.0,
        data['acc_z'].values / 1000.0,
    ])
    gyro_all = np.column_stack([
        data['gyro_x'].values / 1000.0 * np.pi / 180.0,
        data['gyro_y'].values / 1000.0 * np.pi / 180.0,
        data['gyro_z'].values / 1000.0 * np.pi / 180.0,
    ])
    mag_all = np.column_stack([
        data['mag_x'].values,
        data['mag_y'].values,
        data['mag_z'].values,
    ])

    for i in range(n):
        ahrs.update(acc_all[i], gyro_all[i], mag_all[i])
        qi, qj, qk, qs = ahrs.quaternion_ijks
        quats[i] = [qi, qj, qk, qs]

    return pd.DataFrame(quats, columns=['Qi', 'Qj', 'Qk', 'Qs'])


if __name__ == '__main__':
    import sys

    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <sensor_csv> [beta] [output_csv]")
        sys.exit(1)

    csv_path = sys.argv[1]
    beta = float(sys.argv[2]) if len(sys.argv) > 2 else 0.1
    output = sys.argv[3] if len(sys.argv) > 3 else None

    df = compute_quaternions_from_csv(csv_path, beta=beta)

    if output:
        df.to_csv(output, index=False)
        print(f"Wrote {len(df)} quaternions to {output}")
    else:
        print(df.head(20).to_string())
        print(f"... ({len(df)} total rows)")
