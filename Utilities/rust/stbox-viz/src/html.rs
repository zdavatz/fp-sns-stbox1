//! Plotly-based HTML report. Same visual layout as
//! `Utilities/scripts/visualize_combined.py`: map (coloured by nose angle),
//! then nose angle / height / speed time series rows.
//!
//! The JS library is loaded from the Plotly CDN, identical to the Python
//! `to_html(include_plotlyjs='cdn')` path. 1.2 MB HTML files worked fine
//! there and continue to work here.

use crate::gps::Ride;
use crate::io::GpsRow;
use serde_json::json;
use std::fmt::Write as _;

pub struct PanelData<'a> {
    pub t_sensor_s: &'a [f64],
    pub nose_deg: &'a [f64],
    pub height_m: &'a [f64],
    pub gps: &'a [GpsRow],
    pub gps_speed_kmh: &'a [f64],
    pub gps_t_s: &'a [f64],
    pub rides: &'a [Ride],
    pub title: &'a str,
}

/// Produce the full HTML for one combined report.
pub fn render(data: &PanelData) -> String {
    let have_map = !data.gps.is_empty();

    // --- Map trace (Scattermap, coloured by nose-angle at each fix) ---
    // To colour by nose angle at each GPS moment, we interpolate the
    // sensor-side nose series at each GPS-sample time.
    let nose_at_gps = interp(data.gps_t_s, data.t_sensor_s, data.nose_deg);
    let height_at_gps = interp(data.gps_t_s, data.t_sensor_s, data.height_m);

    let (lat_vals, lon_vals): (Vec<f64>, Vec<f64>) =
        data.gps.iter().map(|g| (g.lat, g.lon)).unzip();

    let hover: Vec<String> = (0..data.gps.len())
        .map(|i| {
            format!(
                "UTC {utc}<br>speed={sp:.1} km/h · nose={na:+.1}° · height={h:+.2} m",
                utc = data.gps[i].utc,
                sp = data.gps_speed_kmh.get(i).copied().unwrap_or(0.0),
                na = nose_at_gps.get(i).copied().unwrap_or(0.0),
                h = height_at_gps.get(i).copied().unwrap_or(0.0),
            )
        })
        .collect();

    // Centre the map on the median lat/lon, zoom to fit ride extent.
    let (centre_lat, centre_lon, zoom) = if have_map {
        let mut lat_sorted = lat_vals.clone();
        lat_sorted.sort_by(|a, b| a.partial_cmp(b).unwrap());
        let mut lon_sorted = lon_vals.clone();
        lon_sorted.sort_by(|a, b| a.partial_cmp(b).unwrap());
        let c_lat = lat_sorted[lat_sorted.len() / 2];
        let c_lon = lon_sorted[lon_sorted.len() / 2];
        let lat_span = lat_sorted.last().unwrap_or(&0.0) - lat_sorted.first().unwrap_or(&0.0);
        let lon_span = lon_sorted.last().unwrap_or(&0.0) - lon_sorted.first().unwrap_or(&0.0);
        let span = lat_span.max(lon_span).max(0.005);
        // Rough zoom from span (empirical, matches Python's auto-fit)
        let zoom = (14.0 - (span * 200.0).log2()).clamp(10.0, 18.0);
        (c_lat, c_lon, zoom)
    } else {
        (0.0, 0.0, 2.0)
    };

    // --- Traces ---
    let mut traces = Vec::<serde_json::Value>::new();

    if have_map {
        traces.push(json!({
            "type": "scattermap",
            "mode": "markers+lines",
            "lat": lat_vals,
            "lon": lon_vals,
            "text": hover,
            "hoverinfo": "text",
            "marker": {
                "size": 6,
                "color": nose_at_gps,
                "colorscale": "RdYlGn",
                "cmin": -5,
                "cmax": 5,
                "colorbar": {"title": {"text": "Nose [°]"}, "thickness": 10, "x": 0.99, "y": 0.78, "len": 0.35}
            },
            "line": {"color": "rgba(80,80,80,0.45)", "width": 2},
            "subplot": "map",
            "name": "track",
            "showlegend": false,
        }));
    }

    // Nose angle time-series (row 2 if map, else row 1)
    let ts_row_offset = if have_map { 1 } else { 0 };
    traces.push(json!({
        "type": "scatter",
        "mode": "lines",
        "x": data.t_sensor_s,
        "y": data.nose_deg,
        "line": {"color": "#1f77b4", "width": 1},
        "name": "Nose [°]",
        "xaxis": "x",
        "yaxis": format!("y{}", 1 + ts_row_offset),
        "hovertemplate": "t=%{x:.0f}s<br>nose=%{y:+.2f}°<extra></extra>",
        "showlegend": false,
    }));

    // Height above water time-series
    traces.push(json!({
        "type": "scatter",
        "mode": "lines",
        "x": data.t_sensor_s,
        "y": data.height_m,
        "line": {"color": "#2ca02c", "width": 1},
        "name": "Height [m]",
        "xaxis": "x",
        "yaxis": format!("y{}", 2 + ts_row_offset),
        "hovertemplate": "t=%{x:.0f}s<br>height=%{y:.2f} m<extra></extra>",
        "showlegend": false,
    }));

    // Mast reference line at 0.80 m
    if !data.t_sensor_s.is_empty() {
        let x_ends = vec![data.t_sensor_s[0], *data.t_sensor_s.last().unwrap()];
        traces.push(json!({
            "type": "scatter",
            "mode": "lines",
            "x": x_ends,
            "y": [0.80, 0.80],
            "line": {"color": "rgba(200,0,0,0.5)", "width": 1, "dash": "dot"},
            "hoverinfo": "skip",
            "xaxis": "x",
            "yaxis": format!("y{}", 2 + ts_row_offset),
            "name": "0.80 m mast",
            "showlegend": false,
        }));
    }

    // Speed time-series
    if have_map {
        traces.push(json!({
            "type": "scatter",
            "mode": "lines",
            "x": data.gps_t_s,
            "y": data.gps_speed_kmh,
            "line": {"color": "#d62728", "width": 1},
            "name": "Speed [km/h]",
            "xaxis": "x",
            "yaxis": format!("y{}", 3 + ts_row_offset),
            "hovertemplate": "t=%{x:.0f}s<br>speed=%{y:.1f} km/h<extra></extra>",
            "showlegend": false,
        }));
    }

    // --- Layout ---
    // Rows: map (if have_map) + nose + height + speed (if have_map)
    let n_ts_rows: usize = if have_map { 3 } else { 2 };
    let map_frac = if have_map { 0.45 } else { 0.0 };
    let ts_total = 1.0 - map_frac;
    let ts_row_h = ts_total / n_ts_rows as f64;

    let mut layout = json!({
        "title": {"text": data.title, "x": 0.5, "xanchor": "center"},
        "height": if have_map { 1000 } else { 600 },
        "margin": {"t": 90, "r": 40, "b": 40, "l": 60},
        "hovermode": "closest",
        "showlegend": false,
    });

    // Build subplot domain/axis definitions
    let layout_obj = layout.as_object_mut().unwrap();

    if have_map {
        layout_obj.insert("map".into(), json!({
            "center": {"lat": centre_lat, "lon": centre_lon},
            "zoom": zoom,
            "style": "carto-positron",
            "domain": {"x": [0.0, 1.0], "y": [1.0 - map_frac, 1.0]},
        }));
    }

    // Axis stacks: y1..yN from bottom to top. Place time-series below map.
    // First TS row is at the top of the TS region (just under the map).
    for r in 0..n_ts_rows {
        let y_hi = 1.0 - map_frac - (r as f64) * ts_row_h;
        let y_lo = y_hi - ts_row_h + 0.03;
        let idx = r + 1; // y1, y2, y3
        let axis_name = if idx == 1 { "yaxis".to_string() } else { format!("yaxis{}", idx) };
        let title = match (have_map, r) {
            (true, 0) => "Board nose angle to water [°]".to_string(),
            (true, 1) => "Board height above water [m] — mast = 0.80 m · baro is temperature-drift limited".to_string(),
            (true, 2) => "Speed [km/h] (position-derived, 5 s median)".to_string(),
            (false, 0) => "Board nose angle to water [°]".to_string(),
            (false, 1) => "Board height above water [m]".to_string(),
            _ => String::new(),
        };
        let mut ax = json!({
            "domain": [y_lo.max(0.0), y_hi.min(1.0)],
            "anchor": "x",
            "title": {"text": title},
        });
        if r == n_ts_rows - 1 && have_map {
            ax["range"] = json!([0, 30]);
        }
        layout_obj.insert(axis_name, ax);
    }
    // Shared x-axis across all TS rows
    layout_obj.insert("xaxis".into(), json!({
        "domain": [0.0, 1.0],
        "anchor": format!("y{}", n_ts_rows),
        "title": {"text": "t [s]"},
    }));

    // --- Ride summary block (plain HTML TOC above the chart) ---
    let mut ride_toc = String::from(
        "<div style=\"font-family: sans-serif; font-size: 13px; margin: 10px 20px;\">"
    );
    write!(ride_toc,
        "<b>{n} ride{s} detected over water</b>",
        n = data.rides.len(),
        s = if data.rides.len() == 1 { "" } else { "s" }).unwrap();
    ride_toc.push_str("<ul style=\"margin: 4px 0;\">");
    for (i, r) in data.rides.iter().enumerate() {
        let dur_min = (r.duration_s / 60.0) as u32;
        let dur_sec = ((r.duration_s - dur_min as f64 * 60.0) as u32).min(59);
        write!(ride_toc,
            "<li>Session {}: UTC {utc}, duration {m:02}:{s:02}, p90 {p:.1} km/h</li>",
            i + 1, utc = r.utc_start, m = dur_min, s = dur_sec, p = r.p90_kmh).unwrap();
    }
    ride_toc.push_str("</ul></div>");

    let traces_json = serde_json::to_string(&traces).unwrap();
    let layout_json = serde_json::to_string(&layout).unwrap();

    format!(
        r#"<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <title>{title}</title>
  <script src="https://cdn.plot.ly/plotly-2.35.2.min.js"></script>
</head>
<body style="margin: 0; background: #fafafa;">
  {toc}
  <div id="plot" style="width: 100%; height: 1040px;"></div>
  <script>
    const data = {traces};
    const layout = {layout};
    Plotly.newPlot('plot', data, layout, {{responsive: true}});
  </script>
</body>
</html>
"#,
        title = data.title,
        toc = ride_toc,
        traces = traces_json,
        layout = layout_json,
    )
}

fn interp(x_new: &[f64], x: &[f64], y: &[f64]) -> Vec<f64> {
    crate::baro::interp_linear(x_new, x, y)
}
