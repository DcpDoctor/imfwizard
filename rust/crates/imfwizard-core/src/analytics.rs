use serde::{Deserialize, Serialize};

/// IMP analytics summary.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct ImpAnalytics {
    pub total_assets: usize,
    pub video_tracks: usize,
    pub audio_tracks: usize,
    pub subtitle_tracks: usize,
    pub total_duration_frames: u64,
    pub total_size_bytes: u64,
    pub video_bitrate_mbps: f64,
}

/// Analyze an IMP directory and return summary statistics.
pub fn analyze_imp(imp_dir: &std::path::Path) -> Result<ImpAnalytics, String> {
    let mut analytics = ImpAnalytics::default();

    // Count files by type
    let entries =
        std::fs::read_dir(imp_dir).map_err(|e| format!("Failed to read IMP directory: {e}"))?;

    for entry in entries {
        let entry = entry.map_err(|e| format!("Failed to read entry: {e}"))?;
        let path = entry.path();
        if path.is_file() {
            analytics.total_assets += 1;
            analytics.total_size_bytes += entry.metadata().map(|m| m.len()).unwrap_or(0);

            if let Some(ext) = path.extension().and_then(|e| e.to_str()) {
                match ext.to_lowercase().as_str() {
                    "mxf" => {
                        // Heuristic: first MXF is video, rest are audio
                        if analytics.video_tracks == 0 {
                            analytics.video_tracks += 1;
                        } else {
                            analytics.audio_tracks += 1;
                        }
                    }
                    "ttml" | "xml" => analytics.subtitle_tracks += 1,
                    _ => {}
                }
            }
        }
    }

    Ok(analytics)
}
