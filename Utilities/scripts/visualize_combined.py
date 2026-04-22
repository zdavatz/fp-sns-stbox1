#!/usr/bin/env python3
"""
visualize_combined.py — Interactive HTML per session combining

  - GPS track on an OpenStreetMap basemap (Plotly Scattermap), colored by
    per-second board nose angle so you can see at a glance which parts of
    the track were pumping nose-up vs nose-down.
  - Board nose-angle time series (degrees, drift-compensated).
  - Barometric altitude from LPS22DF, high-pass filtered to show the
    actual pump-stroke oscillation (LPS22DF sees ~1 Pa, 1 hPa ≈ 8.3 m).
  - GPS speed (km/h).
  - GPS altitude from the module (shown for reference but too noisy to
    resolve pump motion — kept so you can spot bad fixes).

Time series share a zoom/pan x-axis so scrubbing one pans all three.
Hover on any series shows the value at that second; hover on the map
shows lat/lon, UTC, and nose angle for that point.

Usage:
    python visualize_combined.py <sensor_csv> [<gps_csv>] [-o html/]

If <gps_csv> is omitted it is auto-resolved from <sensor_csv> by
appending "_gps.csv" to the stem. A cached quaternion CSV next to the
sensor CSV (stem + "_quaternions.csv") is reused if present; otherwise
quaternions are computed via Madgwick fusion on the fly.

Outputs one HTML per auto-detected pumping session
(<stem>_sessionN.html) plus one full-file overview (<stem>.html).
"""
import argparse
import os
import sys
from pathlib import Path

import numpy as np
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

SAMPLE_HZ = 100  # sensor CSV rate

# The "Time [mS]" CSV column is actually ThreadX ticks (tx_time_get),
# where 1 tick = 10 ms. Column header is a firmware-side misnomer; keep
# the conversion explicit so time axes come out in real seconds.
TICKS_PER_SEC = 100


# ---------------------------------------------------------------------------
# Data loading
# ---------------------------------------------------------------------------


def _canonicalize_time_column(df):
    """Accept either the old (misnamed) 'Time [mS]' header or the new
    'Time [10ms]' header and rename it to 'Time [mS]' so the rest of the
    pipeline keeps working unchanged. Both carry ThreadX ticks."""
    for candidate in ('Time [10ms]', 'Time [mS]'):
        if candidate in df.columns:
            if candidate != 'Time [mS]':
                df = df.rename(columns={candidate: 'Time [mS]'})
            return df
    return df


def load_sensor_csv(path):
    df = pd.read_csv(path)
    df.columns = [c.strip() for c in df.columns]
    return _canonicalize_time_column(df)


def load_quaternions(sensor_csv):
    """Prefer a cached <stem>_quaternions.csv next to the sensor CSV;
    fall back to running Madgwick fusion on the sensor CSV directly."""
    stem = Path(sensor_csv).with_suffix('')
    cache = Path(f"{stem}_quaternions.csv")
    if cache.exists():
        q = pd.read_csv(cache)
        if len(q.columns) >= 4 and {'Qi', 'Qj', 'Qk', 'Qs'}.issubset(q.columns):
            return q
    from sensor_fusion import compute_quaternions_from_csv
    return compute_quaternions_from_csv(sensor_csv)


def load_gps_csv(path):
    if not os.path.exists(path):
        return None
    df = pd.read_csv(path)
    df.columns = [c.strip() for c in df.columns]
    return _canonicalize_time_column(df)


# ---------------------------------------------------------------------------
# Derived signals
# ---------------------------------------------------------------------------


def nose_angle_degrees(quat_df, sample_hz):
    """Board nose elevation to horizon (°). Positive = nose up.

    Quaternion rotates body-frame Y-axis (nose direction — sensor is
    mounted Breitachse, X across the board); the z-component of the
    rotated axis equals sin(elevation).
    """
    qi = quat_df['Qi'].values
    qj = quat_df['Qj'].values
    qk = quat_df['Qk'].values
    qs = quat_df['Qs'].values
    nose_z = 2 * (qj * qk - qs * qi)
    elev = np.degrees(np.arcsin(np.clip(nose_z, -1.0, 1.0)))
    # 1 s median filter: suppresses magnetometer-correction spikes without
    # touching the ~1 Hz pump oscillation.
    smooth = pd.Series(elev).rolling(sample_hz, center=True, min_periods=1).median().values
    # Remove slow drift (sensor bias, heading change) with a 60 s baseline.
    baseline = pd.Series(smooth).rolling(60 * sample_hz, center=True, min_periods=1).median()
    baseline = baseline.interpolate(method='linear').bfill().ffill().values
    return smooth - baseline


def baro_altitude_m(pressure_hpa, p0=None):
    """Relative altitude from barometric pressure (international formula).
    p0 defaults to the first sample; result is metres above p0."""
    if p0 is None:
        p0 = float(pressure_hpa[0])
    return 44330.0 * (1.0 - (pressure_hpa / p0) ** (1.0 / 5.255))


def height_above_water_m(t_sensor_ticks, pressure_hpa, temp_celsius,
                          gps_df, base_ticks, speed_kmh,
                          stationary_threshold_kmh=3.0):
    """Board height above water, anchored to GPS-based stationary moments.

    The LPS22DF in the SensorTile.box sits in a semi-sealed enclosure, so the
    air trapped around the sensor is coupled to internal temperature via the
    ideal gas law (P ∝ T at constant V). A 10 °C temperature swing between
    indoor storage and cold seawater produces ~15 hPa of fake "altitude"
    drift — far larger than the ~0.08 hPa signal from an 80 cm mast. The
    rolling-min-within-sensor-only approach can't separate the two, because
    during sustained flight every sample in the window is "up".

    This function works around the hardware limitation by:
      1. Temperature-compensating pressure: P_tc = P × T_ref / T  (Kelvin)
      2. Using GPS speed < ``stationary_threshold_kmh`` as ground truth
         that the board is in the water (knee-start, between rides,
         pre/post-session). The TC'd pressure at those moments defines
         the water reference.
      3. Linear-interpolating the water reference across flying segments
         (board up on foils, GPS speed high).
      4. Height = 8434 × (1 − P_tc / P_waterref). The 8434 m coefficient
         is a local-linearisation of the hypsometric formula near sea
         level — accurate to 1 % for heights < 100 m.

    Returns: height_m aligned 1:1 with ``t_sensor_ticks`` (sensor sample rate).
    Residual drift on this hardware is ~0.5–3 m over a 5-min ride, so the
    result is only reliable at the "flying vs in water" scale, NOT for
    sub-meter mast-height measurement — use a separate height sensor
    (mast-mounted ultrasonic) if you need sub-meter precision.
    """
    t_sec = (t_sensor_ticks - base_ticks) / TICKS_PER_SEC

    # Step 1: temperature compensation.
    tk = temp_celsius + 273.15
    ref_k = float(np.median(tk))
    press_tc = pressure_hpa * (ref_k / tk)

    # Step 2: GPS time grid in the same "seconds since session start" unit.
    if gps_df is None or len(gps_df) == 0 or speed_kmh is None:
        # No GPS — fall back to a long rolling min (still bad, but at least
        # shows relative motion inside the session).
        alt = baro_altitude_m(press_tc, p0=press_tc[0])
        win = 60 * SAMPLE_HZ
        water_ref_alt = pd.Series(alt).rolling(win, center=True, min_periods=1).min()
        return alt - water_ref_alt.bfill().ffill().values

    gt_ticks = gps_df['Time [mS]'].astype(float).values
    g_sec = (gt_ticks - base_ticks) / TICKS_PER_SEC

    # Step 3: interpolate TC'd pressure onto the GPS timeline.
    press_gps = np.interp(g_sec, t_sec, press_tc)

    # Step 4: water reference = press at stationary moments, interpolated.
    water_mask = np.asarray(speed_kmh) < stationary_threshold_kmh
    if not water_mask.any():
        # All flying? Use the session min pressure (i.e. highest altitude)
        # as a desperate fallback. Result will under-estimate height.
        return 8434.0 * (1 - press_tc / float(np.max(press_tc)))

    water_ref_gps = np.where(water_mask, press_gps, np.nan)
    water_ref_gps = pd.Series(water_ref_gps).interpolate('linear').bfill().ffill().values

    # Step 5: map water reference back to sensor timeline and compute height.
    water_ref_sensor = np.interp(t_sec, g_sec, water_ref_gps)
    # Local linearisation: dP/P = -dh/H where H ≈ 8434 m near sea level.
    return 8434.0 * (1 - press_tc / water_ref_sensor)


def bin_to_resolution(t_ticks, values, base_ticks, bucket_ms=100, agg='mean'):
    """Bin a sample series into fixed-width buckets relative to base_ticks.
    bucket_ms defaults to 100 ms → 10 Hz display resolution (ten times the
    old per-second binning, which hid the pump-stroke oscillation inside
    each bucket's mean). t_ticks are ThreadX ticks (1 tick = 10 ms).
    Returns (seconds_array, binned_values_array)."""
    rel_ms = (t_ticks - base_ticks) * 10  # ticks → ms
    bucket = (rel_ms / bucket_ms).astype(int)
    g = pd.Series(values).groupby(bucket).agg(agg)
    # Back to seconds (the x-axis unit used throughout the viz)
    t_s = g.index.values * bucket_ms / 1000.0
    return t_s, g.values


# Kept for back-compat during refactor; prefer bin_to_resolution.
def bin_to_seconds(t_ticks, values, base_ticks, agg='mean'):
    return bin_to_resolution(t_ticks, values, base_ticks,
                              bucket_ms=1000, agg=agg)


# ---------------------------------------------------------------------------
# Session detection (same logic as visualize_sensors / animate_board_3d)
# ---------------------------------------------------------------------------


def detect_sessions(quat_df, sample_hz):
    qi = quat_df['Qi'].values
    qj = quat_df['Qj'].values
    qk = quat_df['Qk'].values
    qs = quat_df['Qs'].values

    sinr = 2.0 * (qs * qi + qj * qk)
    cosr = 1.0 - 2.0 * (qi * qi + qj * qj)
    roll = np.degrees(np.arctan2(sinr, cosr))

    sinp = 2.0 * (qs * qj - qk * qi)
    pitch = np.degrees(np.where(np.abs(sinp) >= 1, np.sign(sinp) * np.pi / 2, np.arcsin(sinp)))

    activity = pd.Series(
        np.abs(np.diff(pitch, prepend=pitch[0])) + np.abs(np.diff(roll, prepend=roll[0]))
    ).rolling(sample_hz).mean().fillna(0)
    threshold = activity.median() + activity.std()
    active = (activity > threshold).astype(int)
    changes = active.diff().fillna(0)
    starts = changes[changes == 1].index.tolist()
    ends = changes[changes == -1].index.tolist()
    if active.iloc[-1] == 1:
        ends.append(len(activity) - 1)

    merge_gap = 60 * sample_hz
    sessions = []
    for s, e in zip(starts, ends):
        if sessions and (s - sessions[-1][1]) < merge_gap:
            sessions[-1] = (sessions[-1][0], e)
        else:
            sessions.append((s, e))

    min_duration = 30 * sample_hz
    sessions = [(s, e) for s, e in sessions if (e - s) >= min_duration]

    pumping = []
    for s, e in sessions:
        p = pitch[s:e]
        p_centered = p - np.median(p)
        crossings = np.sum(np.diff(np.sign(p_centered)) != 0)
        duration = (e - s) / sample_hz
        osc_freq = crossings / duration / 2
        if osc_freq >= 0.3:
            pumping.append((s, e))

    return pumping


# ---------------------------------------------------------------------------
# HTML construction
# ---------------------------------------------------------------------------


def _format_mmss(seconds):
    m, s = divmod(int(round(seconds)), 60)
    return f"{m:02d}:{s:02d}"


def _ride_anchor_key(i):
    """Stable anchor token for a session number. Used both as the URL
    fragment (#s3) and as the key in the JS ride-action table."""
    return f's{i}'


def build_figure(t_s, nose_1hz, alt_1hz, gps_1hz, title, subtitle,
                  sessions_s=None):
    """gps_1hz is a DataFrame with columns: s, lat, lon, speed, alt, fix, utc.
    sessions_s is a list of (start_s, end_s, label) tuples shaded on the
    time series and highlighted on the map.

    Ride-isolation buttons: rendered as a row of pill buttons above the map.
    Each button hides every ride trace except the selected one; a leading
    'All rides' button restores the full overlay. The track line,
    transit/shore markers, and all three time-series stay visible in every
    view.
    """
    have_map = gps_1hz is not None and len(gps_1hz) > 0
    sessions_s = sessions_s or []
    # Trace-index bookkeeping so the isolation buttons can flip exactly
    # the right layers on and off.
    session_trace_indices = []       # ride markers — hide all except selected
    map_background_indices = []      # full track line + off-session fixes
                                     # (hidden when a single ride is isolated)

    # Layout: map + 3 time-series stacked. If no GPS, drop the map row.
    if have_map:
        fig = make_subplots(
            rows=4, cols=1,
            row_heights=[0.45, 0.20, 0.175, 0.175],
            vertical_spacing=0.04,
            specs=[
                [{"type": "scattermap"}],
                [{"type": "xy"}],
                [{"type": "xy"}],
                [{"type": "xy"}],
            ],
            subplot_titles=(
                "GPS track (colored by nose angle)",
                "Board nose angle to water [°] (drift-corrected)",
                "Board height above water [m] — mast = 0.80 m · baro is temperature-drift limited, see README",
                "Speed [km/h] (position-derived, 5 s median, capped at 30)",
            ),
        )
    else:
        fig = make_subplots(
            rows=2, cols=1,
            row_heights=[0.55, 0.45],
            vertical_spacing=0.08,
            subplot_titles=(
                "Board nose angle to water [°] (drift-corrected)",
                "Board height above water [m] — mast = 0.80 m · baro is temperature-drift limited, see README",
            ),
        )

    if have_map:
        # Interpolate nose angle + height-above-water at each GPS fix so
        # hover text carries speed/angle/height together — the three
        # signals the rider wants to correlate visually.
        nose_at_gps = np.interp(gps_1hz['s'].values, t_s, nose_1hz)
        alt_at_gps = np.interp(gps_1hz['s'].values, t_s, alt_1hz)
        gps_secs = gps_1hz['s'].values

        # Full track as a thin grey line (hover skipped so session markers
        # are what the cursor lands on)
        fig.add_trace(
            go.Scattermap(
                lat=gps_1hz['lat'], lon=gps_1hz['lon'],
                mode='lines',
                line=dict(color='rgba(80,80,80,0.45)', width=2),
                name='Track',
                hoverinfo='skip',
                showlegend=False,
            ),
            row=1, col=1,
        )
        map_background_indices.append(len(fig.data) - 1)

        # Off-session fixes (transit/shore) rendered as small neutral dots
        in_session = np.zeros(len(gps_secs), dtype=bool)
        for (ss, se, _label) in sessions_s:
            in_session |= (gps_secs >= ss) & (gps_secs <= se)
        off_mask = ~in_session

        if off_mask.any():
            hover_off = [
                f"t={_format_mmss(s)}<br>UTC={utc}<br>"
                f"lat={lat:.6f}, lon={lon:.6f}<br>"
                f"speed={sp:.1f} km/h · nose={na:+.1f}° · height={h:+.2f} m"
                for s, utc, lat, lon, sp, na, h in zip(
                    gps_secs[off_mask],
                    gps_1hz['utc'].values[off_mask],
                    gps_1hz['lat'].values[off_mask],
                    gps_1hz['lon'].values[off_mask],
                    gps_1hz['speed'].values[off_mask],
                    nose_at_gps[off_mask],
                    alt_at_gps[off_mask],
                )
            ]
            fig.add_trace(
                go.Scattermap(
                    lat=gps_1hz['lat'].values[off_mask],
                    lon=gps_1hz['lon'].values[off_mask],
                    mode='markers',
                    marker=dict(size=4,
                                color='rgba(120,120,120,0.6)'),
                    text=hover_off,
                    hovertemplate='%{text}<extra></extra>',
                    name='transit/shore',
                    legendgroup='off',
                ),
                row=1, col=1,
            )
            map_background_indices.append(len(fig.data) - 1)

        # Per-session fixes: separate trace per session so each gets its
        # own legend entry (toggle on/off). Marker symbol encodes rider
        # (Peter=circle-open, Ayano=circle-filled); colour = nose angle.
        cmax_nose = float(np.nanmax(np.abs(nose_1hz))) if len(nose_1hz) else 10.0
        cmax_nose = max(cmax_nose, 1.0)
        colorbar_shown = False

        for i, (ss, se, label) in enumerate(sessions_s, 1):
            m = (gps_secs >= ss) & (gps_secs <= se)
            if not m.any():
                continue
            hover_on = [
                f"<b>{label}</b><br>t={_format_mmss(s)}<br>UTC={utc}<br>"
                f"lat={lat:.6f}, lon={lon:.6f}<br>"
                f"speed={sp:.1f} km/h · nose={na:+.1f}° · height={h:+.2f} m"
                for s, utc, lat, lon, sp, na, h in zip(
                    gps_secs[m],
                    gps_1hz['utc'].values[m],
                    gps_1hz['lat'].values[m],
                    gps_1hz['lon'].values[m],
                    gps_1hz['speed'].values[m],
                    nose_at_gps[m],
                    alt_at_gps[m],
                )
            ]
            fig.add_trace(
                go.Scattermap(
                    lat=gps_1hz['lat'].values[m],
                    lon=gps_1hz['lon'].values[m],
                    mode='markers',
                    marker=dict(
                        size=10,
                        color=nose_at_gps[m],
                        colorscale='RdBu_r',
                        cmin=-cmax_nose, cmax=cmax_nose,
                        # Colorbar overlays the map's bottom-right corner
                        # (short, inside the plot, translucent) — leaves the
                        # whole right-margin strip for the ride legend.
                        colorbar=(dict(title='Nose [°]',
                                       x=0.99, xanchor='right',
                                       y=0.56, yanchor='top',
                                       len=0.20, thickness=12,
                                       bgcolor='rgba(255,255,255,0.85)',
                                       outlinewidth=0,
                                       tickfont=dict(size=10))
                                  if not colorbar_shown else None),
                        showscale=(not colorbar_shown),
                        opacity=0.9,
                    ),
                    text=hover_on,
                    hovertemplate='%{text}<extra></extra>',
                    name=label,
                    legendgroup=f'session{i}',
                ),
                row=1, col=1,
            )
            # Remember which trace-index this session landed at, so the
            # isolation-button updatemenu can flip exactly one on at a time.
            session_trace_indices.append((len(fig.data) - 1, i, label))
            colorbar_shown = True

        # No-session highlight yet? Fall back to the original all-fixes trace
        # so an empty sessions list still shows a coloured track.
        if not sessions_s:
            hover = [
                f"t={_format_mmss(s)}<br>UTC={utc}<br>"
                f"lat={lat:.6f}, lon={lon:.6f}<br>"
                f"speed={sp:.1f} km/h<br>"
                f"nose={na:+.1f}°"
                for s, utc, lat, lon, sp, na in zip(
                    gps_secs, gps_1hz['utc'].values,
                    gps_1hz['lat'].values, gps_1hz['lon'].values,
                    gps_1hz['speed'].values, nose_at_gps,
                )
            ]
            fig.add_trace(
                go.Scattermap(
                    lat=gps_1hz['lat'], lon=gps_1hz['lon'],
                    mode='markers',
                    marker=dict(
                        size=6, color=nose_at_gps,
                        colorscale='RdBu_r', cmin=-cmax_nose, cmax=cmax_nose,
                        colorbar=dict(title='Nose [°]', x=1.02, len=0.42, y=0.78),
                    ),
                    text=hover,
                    hovertemplate='%{text}<extra></extra>',
                    name='Fix',
                    showlegend=False,
                ),
                row=1, col=1,
            )

        lat_c = float(np.nanmean(gps_1hz['lat']))
        lon_c = float(np.nanmean(gps_1hz['lon']))
        # Rough zoom from extent (deg → Plotly zoom). Fine for a single session.
        lat_span = float(np.nanmax(gps_1hz['lat']) - np.nanmin(gps_1hz['lat']))
        lon_span = float(np.nanmax(gps_1hz['lon']) - np.nanmin(gps_1hz['lon']))
        span = max(lat_span, lon_span, 0.0005)
        zoom = max(10.0, min(18.0, 9.0 - np.log2(span)))
        fig.update_maps(
            # `carto-positron` is a CartoDB-hosted maplibre style that does
            # not require a Mapbox token and — crucially — does not enforce
            # the Referer policy that OSM's tile servers impose on
            # file:// pages. Good clean background for data overlays.
            style='carto-positron',
            center=dict(lat=lat_c, lon=lon_c),
            zoom=zoom,
            row=1, col=1,
        )

    # Time-series rows
    ts_row_offset = 1 if have_map else 0

    # Horizontal-zero guide via Scatter (add_hline trips over Scattermap subplots).
    # Time-series traces are not in the legend (the subplot titles already
    # label each row), so the right-side legend stays clean.
    zero_x = [t_s[0], t_s[-1]] if len(t_s) > 1 else [0, 0]

    fig.add_trace(
        go.Scatter(x=zero_x, y=[0, 0], mode='lines',
                   line=dict(color='gray', width=1, dash='dash'),
                   hoverinfo='skip', showlegend=False),
        row=1 + ts_row_offset, col=1,
    )
    fig.add_trace(
        go.Scatter(x=t_s, y=nose_1hz, mode='lines',
                   line=dict(color='#1f77b4', width=1),
                   name='Nose [°]',
                   showlegend=False,
                   hovertemplate='t=%{x:.0f}s<br>nose=%{y:+.1f}°<extra></extra>'),
        row=1 + ts_row_offset, col=1,
    )

    fig.add_trace(
        go.Scatter(x=zero_x, y=[0, 0], mode='lines',
                   line=dict(color='gray', width=1, dash='dash'),
                   hoverinfo='skip', showlegend=False),
        row=2 + ts_row_offset, col=1,
    )
    fig.add_trace(
        go.Scatter(x=t_s, y=alt_1hz, mode='lines',
                   line=dict(color='#2ca02c', width=1),
                   name='Height [m]',
                   showlegend=False,
                   hovertemplate='t=%{x:.0f}s<br>height=%{y:.2f} m<extra></extra>'),
        row=2 + ts_row_offset, col=1,
    )
    # Mast-length reference line at 0.80 m (physical ceiling).
    # y-range is auto-scaled: this baro on this hardware has residual
    # thermal drift (~0.5–3 m over a 5-min ride even with GPS-anchored
    # baseline), so clipping to [0, 0.95] would leave the panel empty
    # during real flight. The 0.80 m dotted line is still drawn so the
    # reader can see how far above the mast ceiling the drift pushes.
    fig.add_trace(
        go.Scatter(x=zero_x, y=[0.80, 0.80], mode='lines',
                   line=dict(color='rgba(200,0,0,0.5)', width=1, dash='dot'),
                   hoverinfo='skip', showlegend=False,
                   name='0.80 m mast'),
        row=2 + ts_row_offset, col=1,
    )

    if have_map:
        fig.add_trace(
            go.Scatter(x=gps_1hz['s'], y=gps_1hz['speed'], mode='lines',
                       line=dict(color='#d62728', width=1),
                       name='Speed [km/h]',
                       showlegend=False,
                       hovertemplate='t=%{x:.0f}s<br>speed=%{y:.1f} km/h<extra></extra>'),
            row=3 + ts_row_offset, col=1,
        )
        # Fix the speed y-range so a missed multipath spike can't blow the
        # auto-scale up to 200 km/h and flatten the actual ride into a
        # baseline. 30 km/h is a comfortable cap for pumpfoil flight
        # (typical range 10-25 km/h, winning competitions top out ~28).
        fig.update_yaxes(range=[0, 30], row=3 + ts_row_offset, col=1)

    # Share x-axis across the time-series rows so zoom/pan is linked.
    # Scattermap doesn't use xaxis/yaxis — Plotly numbers only the XY
    # subplots, so when have_map the three XY rows get axes x, x2, x3
    # (not x2, x3, x4). Match everything to 'x' (= the first XY axis).
    if have_map:
        fig.update_xaxes(matches='x', row=3, col=1)
        fig.update_xaxes(matches='x', row=4, col=1)
        fig.update_xaxes(title_text='t [s, from session start]', row=4, col=1)
    else:
        fig.update_xaxes(matches='x', row=2, col=1)
        fig.update_xaxes(title_text='t [s, from session start]', row=2, col=1)

    # Session shading on the three time-series rows. Uses add_shape with
    # explicit xref/yref so it plays nicely with the Scattermap subplot.
    # Plotly numbers only the XY subplots (scattermap doesn't take an
    # x/y axis), so rows 2/3/4 correspond to axes x/y, x2/y2, x3/y3.
    ts_xrefs = (['x', 'x2', 'x3'] if have_map else ['x', 'x2'])
    ts_yrefs = (['y domain', 'y2 domain', 'y3 domain']
                if have_map else ['y domain', 'y2 domain'])
    # Vivid but transparent so the traces stay readable
    band_color = 'rgba(255,193,7,0.15)'
    for (ss, se, _label) in sessions_s:
        for xref, yref in zip(ts_xrefs, ts_yrefs):
            fig.add_shape(
                type='rect',
                xref=xref, yref=yref,
                x0=ss, x1=se, y0=0, y1=1,
                fillcolor=band_color,
                line_width=0,
                layer='below',
            )

    # Build isolation buttons ("All" + one per session). Each button sets
    # trace visibility: every ride trace off except the selected one (and
    # "All" turns them all back on). Non-ride traces — track line,
    # transit/shore markers, time-series — stay visible in every view so
    # the map basemap and the zoomed time axis don't go blank.
    updatemenus = []
    if session_trace_indices:
        ride_idxs = [ti for (ti, _, _) in session_trace_indices]
        bg_idxs = set(map_background_indices)
        n_traces = len(fig.data)

        # The session list passed in is (rel_start_s, rel_end_s, label)
        # — same order as session_trace_indices. Match by index so button
        # clicks can zoom to the right window.
        session_ranges = list(sessions_s)  # copy, safe to index

        # Layout key names for the ts x-axis zoom / map center.
        # With have_map=True, XY axes are x, x2, x3 (3 time-series rows).
        if have_map:
            ts_axis_keys = ['xaxis.range', 'xaxis2.range', 'xaxis3.range']
        else:
            ts_axis_keys = ['xaxis.range', 'xaxis2.range']

        def _visibility_for(selected_idx):
            """length-n_traces visibility list. Isolation hides the grey
            map backgrounds (full track + transit/shore) and every ride
            trace except `selected_idx`; time-series traces stay on."""
            vis = [True] * n_traces
            for ti in ride_idxs:
                vis[ti] = (ti == selected_idx)
            for ti in bg_idxs:
                vis[ti] = False
            return vis

        def _layout_update_for(idx):
            """Zoom time-series x-axes to the ride window and — when GPS
            is present — re-center the map on the ride's mean lat/lon."""
            s0, s1, _lbl = session_ranges[idx]
            pad = max(2.0, (s1 - s0) * 0.1)
            x0, x1 = s0 - pad, s1 + pad
            upd = {k: [x0, x1] for k in ts_axis_keys}
            if have_map and gps_1hz is not None:
                gs = gps_1hz['s'].values
                m = (gs >= s0) & (gs <= s1)
                if m.any():
                    lat_c = float(np.nanmean(gps_1hz['lat'].values[m]))
                    lon_c = float(np.nanmean(gps_1hz['lon'].values[m]))
                    lat_span = float(np.nanmax(gps_1hz['lat'].values[m])
                                     - np.nanmin(gps_1hz['lat'].values[m]))
                    lon_span = float(np.nanmax(gps_1hz['lon'].values[m])
                                     - np.nanmin(gps_1hz['lon'].values[m]))
                    span = max(lat_span, lon_span, 5e-5)
                    zoom = max(12.0, min(19.0, 10.0 - np.log2(span)))
                    upd['map.center'] = dict(lat=lat_c, lon=lon_c)
                    upd['map.zoom'] = zoom
            return upd

        # "All rides": restore full visibility + unzoom x-axis + re-center map
        all_upd = {k: None for k in ts_axis_keys}  # None → autorange
        if have_map and gps_1hz is not None and len(gps_1hz):
            lat_c = float(np.nanmean(gps_1hz['lat']))
            lon_c = float(np.nanmean(gps_1hz['lon']))
            all_upd['map.center'] = dict(lat=lat_c, lon=lon_c)
            lat_span = float(np.nanmax(gps_1hz['lat']) - np.nanmin(gps_1hz['lat']))
            lon_span = float(np.nanmax(gps_1hz['lon']) - np.nanmin(gps_1hz['lon']))
            span = max(lat_span, lon_span, 5e-4)
            all_upd['map.zoom'] = max(10.0, min(18.0, 9.0 - np.log2(span)))

        buttons = [dict(
            label='All rides',
            method='update',
            args=[{'visible': [True] * n_traces}, all_upd],
        )]
        # Also build the ride-actions table that the HTML TOC anchors
        # consume — identical payload to the buttons so clicking an
        # anchor or a button produces identical figure state.
        ride_actions = {
            'all': {
                'visible': [True] * n_traces,
                'layout': all_upd,
            }
        }
        for j, (ti, i, label) in enumerate(session_trace_indices):
            vis = _visibility_for(ti)
            lay = _layout_update_for(j)
            buttons.append(dict(
                label=f'S{i}',
                method='update',
                args=[{'visible': vis}, lay],
            ))
            ride_actions[_ride_anchor_key(i)] = {
                'visible': vis,
                'layout': lay,
            }
        # Stash the action table on the fig so the caller (build_html_for_slice)
        # can embed it next to the figure div without refactoring signatures.
        fig._ride_actions = ride_actions
        updatemenus = [dict(
            type='buttons',
            direction='right',
            showactive=True,
            x=0.0, xanchor='left',
            y=1.06, yanchor='bottom',
            bgcolor='white',
            bordercolor='rgba(0,0,0,0.2)',
            font=dict(size=11),
            pad=dict(l=2, r=2, t=2, b=2),
            buttons=buttons,
        )]

    fig.update_layout(
        title=dict(
            text=f"<b>{title}</b><br><sub>{subtitle}</sub>",
            x=0.02, y=0.985, yanchor='top',
            font=dict(size=15),
        ),
        height=1000 if have_map else 600,
        hovermode='x unified',
        # Roomy right margin to host the vertical legend + colorbar without
        # stealing map area; extra top margin so title + button row breathe.
        margin=dict(l=50, r=260, t=130, b=50),
        showlegend=bool(sessions_s),
        legend=dict(
            orientation='v',
            yanchor='top', y=0.98,
            xanchor='left', x=1.01,
            groupclick='togglegroup',
            bgcolor='rgba(255,255,255,0.85)',
            bordercolor='rgba(0,0,0,0.15)',
            borderwidth=1,
            font=dict(size=11),
            itemclick='toggleothers',
            itemdoubleclick='toggle',
        ),
        updatemenus=updatemenus,
    )

    return fig


def build_html_for_slice(sensor_df, quat_df, gps_df, sample_hz,
                          s_idx, e_idx, title, subtitle, out_path,
                          sessions=None):
    """Build a single HTML for rows s_idx..e_idx (set e_idx=None for full file).
    sessions is the optional list of (s_sample, e_sample, label) triples
    detected on the full CSV; they're clipped to the slice and converted to
    seconds relative to the slice start. If labels are omitted (2-tuples),
    a generic "Session N" label is derived."""
    sessions = sessions or []
    # Normalise to (s, e, label)
    norm_sessions = []
    for i, item in enumerate(sessions, 1):
        if len(item) == 3:
            norm_sessions.append(item)
        else:
            s, e = item
            norm_sessions.append((s, e, f'Session {i}'))
    sessions = norm_sessions

    if e_idx is None:
        e_idx = len(sensor_df)
    if s_idx is None:
        s_idx = 0

    sdf = sensor_df.iloc[s_idx:e_idx]
    qdf = quat_df.iloc[s_idx:e_idx]
    t_ms = sdf['Time [mS]'].values
    press = sdf['P [mB]'].values
    temp  = sdf["T ['C]"].values
    if len(t_ms) == 0:
        print(f"  empty slice, skipping {out_path}")
        return
    base_ms = t_ms[0]

    nose = nose_angle_degrees(qdf, sample_hz)

    # GPS first — we need speed to anchor the baro water-reference.
    gps_1hz = None
    gps_sub = None
    speed_for_baro = None
    if gps_df is not None and len(gps_df) > 0:
        gt_ticks = gps_df['Time [mS]'].values
        gt_s = ((gt_ticks - base_ms) / TICKS_PER_SEC).astype(int)
        mask = (gt_s >= 0) & (gt_s <= (t_ms[-1] - base_ms) / TICKS_PER_SEC)
        sub = gps_df[mask].copy()
        sub['s'] = gt_s[mask]
        sub = sub[sub['Fix'].astype(int) >= 1]
        if len(sub) > 0:
            sub = sub.drop_duplicates(subset='s', keep='first').reset_index(drop=True)
            # Override the module's reported Speed with position-derived
            # speed (haversine) — the u-blox MAX-M10S's Doppler Speed field
            # was wildly unreliable on this recording (median 0.12 km/h
            # while position deltas showed sustained 10-30 km/h flight).
            raw_speed = position_derived_speed(sub)
            clipped = np.where(raw_speed > 60.0, np.nan, raw_speed)
            clipped = pd.Series(clipped).interpolate(limit_direction='both').fillna(0).values
            speed_smooth = pd.Series(clipped).rolling(5, center=True, min_periods=1).median().values
            gps_sub = sub
            speed_for_baro = speed_smooth
            gps_1hz = pd.DataFrame({
                's':     sub['s'].values,
                'lat':   sub['Lat'].astype(float).values,
                'lon':   sub['Lon'].astype(float).values,
                'speed': speed_smooth,
                'alt':   sub['Alt [m]'].astype(float).values,
                'fix':   sub['Fix'].astype(int).values,
                'utc':   sub['UTC'].astype(str).values,
            })

    # Height above water: temperature-compensate pressure (baro sits in a
    # semi-sealed enclosure, P couples to T via ideal gas), then anchor the
    # water-reference at GPS-stationary moments (speed < 3 km/h = board in
    # water, knee-start, between rides). Residual drift on this hardware is
    # ~0.5–3 m over a 5-min ride — see `height_above_water_m` for details.
    # The old rolling-min approach collapsed to ~0 m during sustained flight
    # because every sample in the window was "up", leaving the panel empty.
    height_m = height_above_water_m(
        t_ms, press, temp,
        gps_sub, base_ms, speed_for_baro,
        stationary_threshold_kmh=3.0)

    # Sensor display at 10 Hz (100 ms buckets) — fine enough to show the
    # ~1 Hz pump oscillation without drowning the browser in 100 Hz points.
    SENSOR_BUCKET_MS = 100
    t_s_nose, nose_hz = bin_to_resolution(t_ms, nose, base_ms,
                                           bucket_ms=SENSOR_BUCKET_MS)
    t_s_alt, alt_hz = bin_to_resolution(t_ms, height_m, base_ms,
                                         bucket_ms=SENSOR_BUCKET_MS)
    if not np.array_equal(t_s_nose, t_s_alt):
        alt_hz = np.interp(t_s_nose, t_s_alt, alt_hz)
    t_s = t_s_nose
    # Legacy variable names preserved below (build_figure still uses them)
    nose_1hz, alt_1hz = nose_hz, alt_hz

    # Clip each session to the slice and convert sample indices to seconds
    # relative to the slice start.
    sessions_s = []
    for (ss, se, label) in sessions:
        cs = max(ss, s_idx)
        ce = min(se, e_idx)
        if ce <= cs:
            continue
        rel_s0 = (cs - s_idx) / sample_hz
        rel_s1 = (ce - s_idx) / sample_hz
        sessions_s.append((rel_s0, rel_s1, label))

    fig = build_figure(t_s, nose_1hz, alt_1hz, gps_1hz, title, subtitle,
                       sessions_s=sessions_s)

    # Custom HTML wrapper with ride-anchor TOC. Each TOC link has a URL
    # fragment (#s3) AND an onclick handler that calls Plotly.update on
    # the figure div with the same payload as the corresponding toolbar
    # button — so pointing someone at ...html#s3 both scrolls to the
    # right TOC entry and isolates that ride's data.
    import json as _json
    ride_actions = getattr(fig, '_ride_actions', {})
    plot_div_id = 'ride-plot'
    plot_html = fig.to_html(include_plotlyjs='cdn', full_html=False,
                            div_id=plot_div_id,
                            config={'scrollZoom': True})

    toc_rows = []
    for (_s, _e, label) in sessions_s:
        # Extract the "SN" identifier from the trace label, e.g.
        # "Peter S1 · 0:49 · 16 km/h" -> "s1"
        import re
        m = re.search(r'\bS(\d+)\b', label)
        if not m:
            continue
        i = int(m.group(1))
        key = _ride_anchor_key(i)
        toc_rows.append((key, i, label))

    toc_html_parts = [
        '<a class="ride-anchor all" href="#all" '
        'onclick="isolateRide(event, \'all\'); return false;">All rides</a>'
    ]
    for (key, i, label) in toc_rows:
        toc_html_parts.append(
            f'<a class="ride-anchor" id="{key}" href="#{key}" '
            f'onclick="isolateRide(event, \'{key}\'); return false;">{label}</a>'
        )
    toc_html = '\n    '.join(toc_html_parts)

    actions_json = _json.dumps(ride_actions)

    html = f"""<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>{title}</title>
<style>
  body {{
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
    margin: 0; padding: 12px 18px 18px; color: #222;
  }}
  h1 {{ margin: 0 0 2px; font-size: 18px; }}
  .subtitle {{ margin: 0 0 10px; color: #666; font-size: 13px; }}
  nav.ride-toc {{
    display: flex; flex-wrap: wrap; gap: 6px 10px;
    padding: 8px 10px;
    background: #f5f5f5; border: 1px solid #e0e0e0; border-radius: 6px;
    margin-bottom: 8px; font-size: 12px;
  }}
  nav.ride-toc a.ride-anchor {{
    display: inline-block;
    padding: 3px 9px;
    border: 1px solid #d0d0d0; border-radius: 4px;
    background: white; color: #333; text-decoration: none;
    white-space: nowrap;
  }}
  nav.ride-toc a.ride-anchor:hover {{ background: #e9ecef; }}
  nav.ride-toc a.ride-anchor.active {{
    background: #1f77b4; border-color: #1f77b4; color: white;
  }}
  nav.ride-toc a.ride-anchor.all {{ font-weight: 600; }}
  #ride-plot {{ width: 100%; }}
</style>
</head>
<body>
<h1>{title}</h1>
<p class="subtitle">{subtitle}</p>
<nav class="ride-toc" id="ride-toc">
    {toc_html}
</nav>
{plot_html}
<script>
  // Per-ride Plotly.update payloads computed in Python from the same
  // visibility/layout logic that drives the toolbar buttons.
  const RIDE_ACTIONS = {actions_json};
  const PLOT_DIV_ID = '{plot_div_id}';

  function isolateRide(evt, key) {{
    if (evt && evt.preventDefault) evt.preventDefault();
    const action = RIDE_ACTIONS[key];
    if (!action) return;
    const div = document.getElementById(PLOT_DIV_ID);
    if (!div || !window.Plotly) return;
    window.Plotly.update(div, {{visible: action.visible}}, action.layout || {{}});
    document.querySelectorAll('nav.ride-toc a.ride-anchor').forEach(
      a => a.classList.remove('active'));
    const hit = document.querySelector(
      'nav.ride-toc a.ride-anchor[href="#' + key + '"]');
    if (hit) hit.classList.add('active');
    // Update URL hash without re-triggering hashchange → smooth pointing
    // to this ride via a shared link.
    if (window.history && window.history.replaceState) {{
      window.history.replaceState(null, '', '#' + key);
    }}
  }}

  function applyInitialHash() {{
    const h = (window.location.hash || '').replace(/^#/, '').trim();
    if (h && RIDE_ACTIONS[h]) {{
      isolateRide(null, h);
    }} else {{
      isolateRide(null, 'all');
    }}
  }}

  // Wait for Plotly to finish drawing the figure before issuing updates.
  const readyInterval = setInterval(() => {{
    const div = document.getElementById(PLOT_DIV_ID);
    if (div && div.data) {{ clearInterval(readyInterval); applyInitialHash(); }}
  }}, 50);

  window.addEventListener('hashchange', applyInitialHash);
</script>
</body>
</html>
"""
    with open(out_path, 'w') as f:
        f.write(html)
    print(f"  wrote {out_path}")


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def position_derived_speed(gps_df):
    """Compute per-GPS-fix speed (km/h) from lat/lon deltas via the haversine
    formula. The u-blox MAX-M10S's Doppler-based Speed field was observed
    to dramatically under-report real motion on this board — median 0.12
    km/h while position deltas showed bursts >30 km/h. Position derivation
    is slower-reacting but much more honest.

    Returns a numpy array the same length as gps_df (first element = 0).
    """
    lat = gps_df['Lat'].astype(float).values
    lon = gps_df['Lon'].astype(float).values
    # "Time [mS]" is ticks (10 ms each) despite the column name
    t_ticks = gps_df['Time [mS]'].astype(float).values
    R = 6371000.0  # earth radius, m
    lat_r = np.radians(lat)
    lon_r = np.radians(lon)
    dlat = np.diff(lat_r)
    dlon = np.diff(lon_r)
    mlat = (lat_r[1:] + lat_r[:-1]) / 2
    a = np.sin(dlat / 2) ** 2 + np.cos(mlat) * np.sin(dlon / 2) ** 2
    d_m = 2 * R * np.arcsin(np.sqrt(a))
    dt_s = np.diff(t_ticks) / TICKS_PER_SEC
    dt_s = np.where(dt_s <= 0, 1e-9, dt_s)
    speed_kmh = d_m / dt_s * 3.6
    return np.concatenate([[0.0], speed_kmh])


def detect_rides_from_gps(gps_df, sensor_n_samples, sample_hz,
                           speed_threshold_kmh=3.0,
                           max_plausible_kmh=60.0,
                           min_run_s=10,
                           merge_gap_s=30,
                           pad_s=3):
    """Isolate real rides over water from GPS speed.

    A "ride" here means any contiguous window where the rider was actually
    moving across the water (flying, paddling, taxiing out). Stationary
    kneeling drills, board-carry on shore, and rigging time are excluded.

    Method
    ------
    1. Position-derive speed per GPS fix (haversine). The module's
       reported Speed column is unreliable on this board.
    2. Reject obvious multipath glitches — any fix with per-second speed
       above max_plausible_kmh (60 km/h is well above any pumpfoil) is
       treated as NaN and bridged by the rolling smoother.
    3. Smooth over 5 s to absorb stroke-by-stroke speed oscillation.
    4. Threshold at speed_threshold_kmh (3 km/h is a comfortable margin
       above GPS static noise on this unit).
    5. Keep only runs of at least min_run_s seconds; merge runs separated
       by less than merge_gap_s (short glides between pump bursts).
    6. Pad each ride by pad_s seconds on both ends so drop-in and
       touch-down frames are included in the viz.

    Returns: list of (s_sample, e_sample) tuples on the sensor-sample axis.
    """
    if gps_df is None or len(gps_df) == 0:
        return []
    g = gps_df[gps_df['Fix'].astype(int) >= 1].copy()
    if g.empty:
        return []
    g['sec'] = (g['Time [mS]'].astype(float) // TICKS_PER_SEC).astype(int)
    g = g.drop_duplicates('sec', keep='first').sort_values('sec').reset_index(drop=True)
    if len(g) < 3:
        return []

    pos_speed = position_derived_speed(g)
    # Clamp glitches: anything above max_plausible is multipath nonsense.
    pos_speed = np.where(pos_speed > max_plausible_kmh, np.nan, pos_speed)
    # Linear-bridge glitches so the smoother doesn't fall to 0
    pos_speed = pd.Series(pos_speed).interpolate(limit_direction='both').fillna(0).values
    smooth = pd.Series(pos_speed).rolling(5, center=True, min_periods=1).mean().values
    fast = smooth > speed_threshold_kmh

    # Identify runs
    runs = []
    start = None
    for i, f in enumerate(fast):
        if f:
            if start is None:
                start = i
        else:
            if start is not None:
                if i - start >= min_run_s:
                    runs.append((int(g['sec'].iloc[start]),
                                 int(g['sec'].iloc[i - 1])))
                start = None
    if start is not None and (len(fast) - start) >= min_run_s:
        runs.append((int(g['sec'].iloc[start]), int(g['sec'].iloc[-1])))

    if not runs:
        return []

    # Merge nearby runs
    merged = [list(runs[0])]
    for s, e in runs[1:]:
        if s - merged[-1][1] <= merge_gap_s:
            merged[-1][1] = e
        else:
            merged.append([s, e])

    # Convert wall-clock seconds to sensor-sample indices (100 samples/s),
    # padded by pad_s seconds on each side.
    out = []
    for s_sec, e_sec in merged:
        s_samp = max(0, (s_sec - pad_s) * sample_hz)
        e_samp = min(sensor_n_samples, (e_sec + pad_s) * sample_hz)
        if e_samp > s_samp:
            out.append((s_samp, e_samp))
    return out


def classify_rider_by_split(sessions, sensor_n_samples, split_tick=None,
                             first_rider='Peter', second_rider='Ayano'):
    """Assign a rider label to each session based on a fixed split tick
    on the sensor-sample axis. If split_tick is None, splits at the
    midpoint of the recording (simple halves rule).

    For the 21.04.2026 Ermioni recording we know from IMG_1716.MOV
    (creation_time 07:04:18 UTC, Ayano's drop-in) that the real rider
    switch happened ~37 % into the recording, not at 50 %. Pass that
    tick via --rider-split-utc to get the right assignment.

    Returns (label, midpoint_fraction) tuples, one per session.
    """
    if split_tick is None:
        split_tick = sensor_n_samples / 2
    out = []
    for (s, e) in sessions:
        mid = (s + e) / 2
        frac = mid / sensor_n_samples
        if mid <= split_tick:
            out.append((first_rider, frac))
        else:
            out.append((second_rider, frac))
    return out


# Keep the old name as a thin alias so any external callers keep working
classify_rider_by_halves = classify_rider_by_split


def classify_rider_by_speed(gps_df, sessions, sample_hz, sensor_t_ms,
                             fast_threshold_kmh=5.0):
    """Secondary classifier — used only as a sanity check against the
    halves split. Returns (label, p90_pos_speed_kmh) tuples.

    Kept around because sessions with wildly different speed profiles
    across the midpoint are worth annotating ("Peter (kneeling drill)"
    vs "Ayano (flying)") in the hover text even when the halves split
    handles the main rider assignment.
    """
    out = []
    if gps_df is None or len(gps_df) == 0:
        return [('', 0.0)] * len(sessions)
    g_sorted = gps_df.sort_values('Time [mS]').reset_index(drop=True)
    pos_speed = position_derived_speed(g_sorted)
    # Clamp multipath glitches (same filter as ride detection) so the
    # per-session p90 doesn't get yanked by a single bogus 1000 km/h row.
    pos_speed = np.where(pos_speed > 60.0, np.nan, pos_speed)
    pos_speed = pd.Series(pos_speed).interpolate(limit_direction='both').fillna(0).values
    gt_ms = g_sorted['Time [mS]'].astype(float).values
    for (s_idx, e_idx) in sessions:
        s_ms = sensor_t_ms[s_idx]
        e_ms = sensor_t_ms[min(e_idx, len(sensor_t_ms) - 1)]
        m = (gt_ms >= s_ms) & (gt_ms <= e_ms)
        if not m.any():
            out.append(('', 0.0))
            continue
        p90 = float(np.percentile(pos_speed[m], 90))
        activity = 'flying' if p90 >= fast_threshold_kmh else 'kneeling drill'
        out.append((activity, p90))
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('sensor_csv', help='SensNNN.csv (SD-card format)')
    ap.add_argument('gps_csv', nargs='?', default=None,
                    help='GpsNNN.csv; auto-resolved from sensor_csv if omitted')
    ap.add_argument('-o', '--output-dir', default='html',
                    help='Output directory (default: html)')
    ap.add_argument('--per-session', action='store_true',
                    help='Also emit one HTML per detected pumping session')
    ap.add_argument('--rider-split-utc', default=None,
                    help='UTC time HH:MM:SS that divides first rider from '
                         'second (e.g. 07:04:30 for the 21.04.2026 Ermioni '
                         'session where Ayano took over after Peter). '
                         'Overrides the midpoint-halves default.')
    args = ap.parse_args()

    sensor_path = Path(args.sensor_csv)
    if not sensor_path.exists():
        print(f"error: sensor CSV not found: {sensor_path}", file=sys.stderr)
        sys.exit(1)

    if args.gps_csv is None:
        gps_guess = sensor_path.with_suffix('')
        gps_guess = Path(f"{gps_guess}_gps.csv")
        gps_path = gps_guess if gps_guess.exists() else None
    else:
        gps_path = Path(args.gps_csv)
        if not gps_path.exists():
            print(f"error: gps CSV not found: {gps_path}", file=sys.stderr)
            sys.exit(1)

    out_dir = Path(args.output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"loading {sensor_path.name}")
    sensor_df = load_sensor_csv(sensor_path)
    print(f"  {len(sensor_df)} rows at {SAMPLE_HZ} Hz "
          f"({len(sensor_df)/SAMPLE_HZ/60:.1f} min)")

    print(f"loading quaternions")
    quat_df = load_quaternions(sensor_path)
    if len(quat_df) != len(sensor_df):
        print(f"  warning: quat rows {len(quat_df)} != sensor rows {len(sensor_df)}; "
              f"truncating to shorter")
        n = min(len(quat_df), len(sensor_df))
        quat_df = quat_df.iloc[:n].reset_index(drop=True)
        sensor_df = sensor_df.iloc[:n].reset_index(drop=True)

    gps_df = None
    if gps_path is not None:
        print(f"loading {gps_path.name}")
        gps_df = load_gps_csv(gps_path)
        if gps_df is None or len(gps_df) == 0:
            print(f"  gps CSV empty, skipping map")
            gps_df = None
        else:
            print(f"  {len(gps_df)} GPS fixes")
    else:
        print("no GPS CSV — will produce time-series only HTML")

    # GPS-driven ride detection. The pitch-oscillation detector in
    # detect_sessions catches kneeling-drill activity but misses smooth
    # flying (where the board barely pitches). Rides over water are
    # defined by sustained GPS movement — that's what we actually want.
    sessions = detect_rides_from_gps(gps_df, len(sensor_df), SAMPLE_HZ,
                                      speed_threshold_kmh=3.0,
                                      min_run_s=10, merge_gap_s=30,
                                      pad_s=3)
    print(f"detected {len(sessions)} rides over water")
    if not sessions:
        # Fall back to pitch detector so we at least produce something
        print("  no GPS rides found — falling back to pitch-oscillation detector")
        sessions = detect_sessions(quat_df, SAMPLE_HZ)
        print(f"  pitch detector found {len(sessions)} sessions")

    # Rider assignment. Ground-truth rule for this recording (2026-04-22):
    # Peter rode the first half, Ayano the second. Speed/pitch heuristics
    # are kept as an activity-type annotation shown on hover.
    sensor_t_ms = sensor_df['Time [mS]'].values
    # Resolve the rider-split UTC (if provided) to a sensor tick by
    # finding the nearest GPS row matching that wall-clock time.
    split_tick = None
    if args.rider_split_utc and gps_df is not None:
        h, m, s = args.rider_split_utc.split(':')
        target_sec = int(h) * 3600 + int(m) * 60 + float(s)
        def _utc_to_sec(u):
            u = str(u).strip().zfill(8 if '.' in str(u) else 6)
            return int(u[:2]) * 3600 + int(u[2:4]) * 60 + float(u[4:])
        gps_utc_sec = gps_df['UTC'].apply(_utc_to_sec).values
        gt = gps_df['Time [mS]'].values
        idx = int(np.argmin(np.abs(gps_utc_sec - target_sec)))
        split_tick = float(gt[idx])
        print(f"rider split at UTC {args.rider_split_utc} -> sensor tick {split_tick:.0f} "
              f"({split_tick / TICKS_PER_SEC / 60:.1f} min into recording)")

    rider_labels = classify_rider_by_split(sessions, len(sensor_df),
                                            split_tick=split_tick)
    activity_hints = classify_rider_by_speed(gps_df, sessions, SAMPLE_HZ,
                                              sensor_t_ms)

    # Look up a rough UTC for each session by cross-referencing sensor tick
    # with the nearest GPS fix. This is what you compare against the
    # camera's file-creation time to identify which ride matches a video.
    def _utc_at_tick(tick):
        if gps_df is None or gps_df.empty:
            return None
        gt = gps_df['Time [mS]'].values
        # nearest-by-tick
        idx = int(np.argmin(np.abs(gt - tick)))
        u = str(gps_df.iloc[idx]['UTC']).strip()
        # zero-pad to hhmmss.ff
        u = u.zfill(8 if '.' in u else 6)
        if len(u) >= 6:
            return f"{u[:2]}:{u[2:4]}:{u[4:6]}"
        return u

    print("rides detected (GPS-speed based):")
    for i, ((s, e), (rider, frac), (activity, p90)) in enumerate(
            zip(sessions, rider_labels, activity_hints), 1):
        dur = _format_mmss((e - s) / SAMPLE_HZ)
        utc = _utc_at_tick(sensor_t_ms[s]) or '?'
        act = f"[{activity}]" if activity else ""
        print(f"  session {i}: UTC {utc}, duration {dur}, "
              f"p90 {p90:4.1f} km/h, midpoint {frac*100:4.1f}% -> {rider} {act}")

    # Short trace labels for the legend — the full UTC / activity / speed
    # info is carried in each marker's hover text, so the legend just needs
    # to disambiguate rider + session number + duration at a glance.
    labelled_sessions = []
    for i, ((s, e), (rider, _frac), (_activity, p90)) in enumerate(
            zip(sessions, rider_labels, activity_hints), 1):
        dur = _format_mmss((e - s) / SAMPLE_HZ)
        # "Peter S1 · 0:49 · 16 km/h"
        label = f"{rider} S{i} · {dur} · {p90:.0f} km/h"
        labelled_sessions.append((s, e, label))

    stem = sensor_path.stem
    # Subtitle summarises riders + fixes in one glance
    n_ayano = sum(1 for (r, _) in rider_labels if r == 'Ayano')
    n_peter = sum(1 for (r, _) in rider_labels if r == 'Peter')
    subtitle = f"{len(sensor_df)/SAMPLE_HZ/60:.1f} min recording — "
    subtitle += f"{len(sessions)} pump sessions ({n_peter} Peter, {n_ayano} Ayano)"
    if gps_df is not None:
        subtitle += f", {len(gps_df)} GPS fixes"

    # Single combined HTML — this is the default output.
    out = out_dir / f"viz_{stem}.html"
    print(f"building combined HTML")
    build_html_for_slice(sensor_df, quat_df, gps_df, SAMPLE_HZ,
                         None, None,
                         f"{stem}",
                         subtitle, out,
                         sessions=labelled_sessions)

    if args.per_session:
        for i, ((s, e), (lbl, _p90)) in enumerate(zip(sessions, rider_labels), 1):
            dur_s = (e - s) / SAMPLE_HZ
            out = out_dir / f"viz_{stem}_session{i}.html"
            print(f"building session {i} ({lbl})")
            build_html_for_slice(sensor_df, quat_df, gps_df, SAMPLE_HZ,
                                 s, e,
                                 f"{stem} — {lbl}, session {i}",
                                 f"duration {_format_mmss(dur_s)}, "
                                 f"p90 speed {_p90:.1f} km/h",
                                 out,
                                 sessions=[])

    print("done.")


if __name__ == '__main__':
    main()
