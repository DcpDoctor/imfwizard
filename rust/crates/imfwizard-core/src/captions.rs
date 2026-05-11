use serde::{Deserialize, Serialize};

/// Closed caption format.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum CaptionFormat {
    Scc,
    Mcc,
    Cap,
    Srt,
    ImscTtml,
}

/// Extract captions from MXF/video using ffmpeg.
pub fn extract_captions(
    input: &std::path::Path,
    output: &std::path::Path,
    format: CaptionFormat,
) -> Result<(), String> {
    let format_arg = match format {
        CaptionFormat::Srt => "srt",
        CaptionFormat::Scc => "scc",
        _ => "srt",
    };

    let out = std::process::Command::new("ffmpeg")
        .args(["-y", "-i"])
        .arg(input)
        .args(["-map", "0:s:0", "-f", format_arg])
        .arg(output)
        .output()
        .map_err(|e| format!("Failed to run ffmpeg: {e}"))?;

    if !out.status.success() {
        return Err(String::from_utf8_lossy(&out.stderr).into_owned());
    }
    Ok(())
}
