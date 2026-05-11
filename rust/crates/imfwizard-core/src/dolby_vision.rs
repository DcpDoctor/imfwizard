use serde::{Deserialize, Serialize};
use std::process::Command;

/// Dolby Vision RPU mode.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum DvMode {
    Mode0,
    Mode1,
    Mode2,
    Mode4,
    Mode5,
}

impl DvMode {
    pub fn as_str(&self) -> &str {
        match self {
            Self::Mode0 => "0",
            Self::Mode1 => "1",
            Self::Mode2 => "2",
            Self::Mode4 => "4",
            Self::Mode5 => "5",
        }
    }
}

/// Extract Dolby Vision RPU using dovi_tool.
pub fn extract_rpu(input: &std::path::Path, output: &std::path::Path) -> Result<(), String> {
    let out = Command::new("dovi_tool")
        .arg("extract-rpu")
        .arg("-i")
        .arg(input)
        .arg("-o")
        .arg(output)
        .output()
        .map_err(|e| format!("Failed to run dovi_tool: {e}"))?;

    if !out.status.success() {
        return Err(String::from_utf8_lossy(&out.stderr).into_owned());
    }
    Ok(())
}

/// Convert Dolby Vision mode using dovi_tool.
pub fn convert_mode(
    input: &std::path::Path,
    output: &std::path::Path,
    mode: DvMode,
) -> Result<(), String> {
    let out = Command::new("dovi_tool")
        .arg("convert")
        .arg("--mode")
        .arg(mode.as_str())
        .arg("-i")
        .arg(input)
        .arg("-o")
        .arg(output)
        .output()
        .map_err(|e| format!("Failed to run dovi_tool: {e}"))?;

    if !out.status.success() {
        return Err(String::from_utf8_lossy(&out.stderr).into_owned());
    }
    Ok(())
}

/// Inject RPU into HEVC stream using dovi_tool.
pub fn inject_rpu(
    hevc: &std::path::Path,
    rpu: &std::path::Path,
    output: &std::path::Path,
) -> Result<(), String> {
    let out = Command::new("dovi_tool")
        .arg("inject-rpu")
        .arg("-i")
        .arg(hevc)
        .arg("--rpu-in")
        .arg(rpu)
        .arg("-o")
        .arg(output)
        .output()
        .map_err(|e| format!("Failed to run dovi_tool: {e}"))?;

    if !out.status.success() {
        return Err(String::from_utf8_lossy(&out.stderr).into_owned());
    }
    Ok(())
}
