//! Uniform time-bucket aggregation for display-rate reduction.
//!
//! The sensor CSV is at 100 Hz (138k samples over 23 min). Feeding that
//! directly into Plotly produces a 7 MB HTML that's slow to render. 100 ms
//! buckets (10 Hz display rate) give 10× smaller output while keeping the
//! ~1 Hz pump-stroke oscillation fully resolved.

#[allow(dead_code)] // Median used in phase-2 per-session rendering
pub enum Agg {
    Mean,
    Median,
}

/// Bin `values` (indexed 1:1 with `t_seconds`) into fixed-width buckets,
/// aggregating each bucket with `agg`. Returns (bucket_centre_s, agg_values).
pub fn bin_to_resolution(t_seconds: &[f64], values: &[f64], bucket_ms: u32, agg: Agg) -> (Vec<f64>, Vec<f64>) {
    assert_eq!(t_seconds.len(), values.len());
    if t_seconds.is_empty() {
        return (Vec::new(), Vec::new());
    }

    let bucket_s = bucket_ms as f64 / 1000.0;
    let t0 = t_seconds[0];

    // Group indices by bucket id.
    let mut last_bucket: i64 = i64::MIN;
    let mut bucket_t = Vec::new();
    let mut bucket_vals: Vec<Vec<f64>> = Vec::new();
    for (i, &t) in t_seconds.iter().enumerate() {
        let b = ((t - t0) / bucket_s).floor() as i64;
        if b != last_bucket {
            bucket_t.push(t0 + b as f64 * bucket_s);
            bucket_vals.push(Vec::new());
            last_bucket = b;
        }
        bucket_vals.last_mut().unwrap().push(values[i]);
    }

    let out_values: Vec<f64> = bucket_vals.iter()
        .map(|v| match agg {
            Agg::Mean => v.iter().sum::<f64>() / v.len() as f64,
            Agg::Median => {
                let mut s = v.clone();
                s.sort_by(|a, b| a.partial_cmp(b).unwrap());
                s[s.len() / 2]
            }
        })
        .collect();

    (bucket_t, out_values)
}
