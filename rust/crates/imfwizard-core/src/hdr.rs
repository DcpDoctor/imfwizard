use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::process::Command;

/// HDR metadata.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct HdrMetadata {
    pub max_cll: u32,
    pub max_fall: u32,
    pub mastering_display: String,
    pub color_primaries: String,
    pub transfer_characteristics: String,
}

/// HDR10+ metadata.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct Hdr10PlusMetadata {
    pub json_path: PathBuf,
    pub scene_count: usize,
}

/// Extract HDR10+ metadata using hdr10plus_tool.
pub fn extract_hdr10plus(
    input: &std::path::Path,
    output_json: &std::path::Path,
) -> Result<Hdr10PlusMetadata, String> {
    let out = Command::new("hdr10plus_tool")
        .arg("extract")
        .arg("-i")
        .arg(input)
        .arg("-o")
        .arg(output_json)
        .output()
        .map_err(|e| format!("Failed to run hdr10plus_tool: {e}"))?;

    if !out.status.success() {
        return Err(String::from_utf8_lossy(&out.stderr).into_owned());
    }

    Ok(Hdr10PlusMetadata {
        json_path: output_json.to_path_buf(),
        scene_count: 0,
    })
}

/// Inject HDR10+ metadata using hdr10plus_tool.
pub fn inject_hdr10plus(
    input: &std::path::Path,
    json: &std::path::Path,
    output: &std::path::Path,
) -> Result<(), String> {
    let out = Command::new("hdr10plus_tool")
        .arg("inject")
        .arg("-i")
        .arg(input)
        .arg("-j")
        .arg(json)
        .arg("-o")
        .arg(output)
        .output()
        .map_err(|e| format!("Failed to run hdr10plus_tool: {e}"))?;

    if !out.status.success() {
        return Err(String::from_utf8_lossy(&out.stderr).into_owned());
    }
    Ok(())
}
