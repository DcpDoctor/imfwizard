use serde::{Deserialize, Serialize};
use std::path::PathBuf;
use std::process::Command;

/// Transcode options (ffmpeg-based).
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct TranscodeOptions {
    pub input: PathBuf,
    pub output: PathBuf,
    pub codec: String,
    pub resolution: Option<(u32, u32)>,
    pub fps: Option<f64>,
    pub bitrate: Option<String>,
    pub audio_codec: Option<String>,
    pub extra_args: Vec<String>,
}

/// Transcode result.
#[derive(Debug, Clone, Default, Serialize, Deserialize)]
pub struct TranscodeResult {
    pub success: bool,
    pub error: String,
    pub output: PathBuf,
}

/// Transcode media via ffmpeg.
pub fn transcode(opts: &TranscodeOptions) -> TranscodeResult {
    let mut cmd = Command::new("ffmpeg");
    cmd.arg("-y").arg("-i").arg(&opts.input);

    if !opts.codec.is_empty() {
        cmd.arg("-c:v").arg(&opts.codec);
    }
    if let Some((w, h)) = opts.resolution {
        cmd.arg("-s").arg(format!("{w}x{h}"));
    }
    if let Some(fps) = opts.fps {
        cmd.arg("-r").arg(fps.to_string());
    }
    if let Some(ref br) = opts.bitrate {
        cmd.arg("-b:v").arg(br);
    }
    if let Some(ref ac) = opts.audio_codec {
        cmd.arg("-c:a").arg(ac);
    }
    for arg in &opts.extra_args {
        cmd.arg(arg);
    }
    cmd.arg(&opts.output);

    match cmd.output() {
        Ok(out) if out.status.success() => TranscodeResult {
            success: true,
            error: String::new(),
            output: opts.output.clone(),
        },
        Ok(out) => TranscodeResult {
            success: false,
            error: String::from_utf8_lossy(&out.stderr).into_owned(),
            output: opts.output.clone(),
        },
        Err(e) => TranscodeResult {
            success: false,
            error: format!("Failed to run ffmpeg: {e}"),
            output: opts.output.clone(),
        },
    }
}
