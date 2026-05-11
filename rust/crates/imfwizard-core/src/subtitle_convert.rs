use serde::{Deserialize, Serialize};

/// Subtitle format.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub enum SubtitleFormat {
    Srt,
    Vtt,
    Stl,
    Ttml,
    ImscTtml,
}

impl SubtitleFormat {
    pub fn extension(&self) -> &str {
        match self {
            Self::Srt => "srt",
            Self::Vtt => "vtt",
            Self::Stl => "stl",
            Self::Ttml | Self::ImscTtml => "ttml",
        }
    }

    pub fn from_extension(ext: &str) -> Option<Self> {
        match ext.to_lowercase().as_str() {
            "srt" => Some(Self::Srt),
            "vtt" | "webvtt" => Some(Self::Vtt),
            "stl" => Some(Self::Stl),
            "ttml" | "xml" => Some(Self::Ttml),
            _ => None,
        }
    }
}

/// A single subtitle cue.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SubtitleCue {
    pub index: u32,
    pub start_ms: u64,
    pub end_ms: u64,
    pub text: String,
}

/// Convert subtitles between formats.
pub fn convert_subtitles(
    input: &std::path::Path,
    output: &std::path::Path,
    _target_format: SubtitleFormat,
) -> Result<(), String> {
    // Parse input
    let content =
        std::fs::read_to_string(input).map_err(|e| format!("Failed to read input: {e}"))?;

    let ext = input.extension().and_then(|e| e.to_str()).unwrap_or("");
    let _source_format = SubtitleFormat::from_extension(ext)
        .ok_or_else(|| format!("Unknown subtitle format: {ext}"))?;

    let cues = parse_srt(&content)?;

    // Write output as TTML (IMF standard)
    write_ttml(&cues, output)
}

fn parse_srt(content: &str) -> Result<Vec<SubtitleCue>, String> {
    let mut cues = Vec::new();
    let mut lines = content.lines().peekable();

    while lines.peek().is_some() {
        // Skip blank lines
        while lines.peek().is_some_and(|l| l.trim().is_empty()) {
            lines.next();
        }

        // Index
        let index_line = match lines.next() {
            Some(l) => l.trim().to_string(),
            None => break,
        };
        let index: u32 = index_line.parse().unwrap_or(0);

        // Timecodes
        let tc_line = match lines.next() {
            Some(l) => l,
            None => break,
        };
        let (start_ms, end_ms) = parse_srt_timecodes(tc_line)?;

        // Text
        let mut text = String::new();
        while lines.peek().is_some_and(|l| !l.trim().is_empty()) {
            if !text.is_empty() {
                text.push('\n');
            }
            text.push_str(lines.next().unwrap().trim());
        }

        cues.push(SubtitleCue {
            index,
            start_ms,
            end_ms,
            text,
        });
    }

    Ok(cues)
}

fn parse_srt_timecodes(line: &str) -> Result<(u64, u64), String> {
    let parts: Vec<&str> = line.split("-->").collect();
    if parts.len() != 2 {
        return Err(format!("Invalid timecode line: {line}"));
    }
    let start = parse_srt_time(parts[0].trim())?;
    let end = parse_srt_time(parts[1].trim())?;
    Ok((start, end))
}

fn parse_srt_time(s: &str) -> Result<u64, String> {
    // HH:MM:SS,mmm
    let s = s.replace(',', ".");
    let parts: Vec<&str> = s.split(':').collect();
    if parts.len() != 3 {
        return Err(format!("Invalid time: {s}"));
    }
    let h: u64 = parts[0]
        .parse()
        .map_err(|_| format!("Bad hours: {}", parts[0]))?;
    let m: u64 = parts[1]
        .parse()
        .map_err(|_| format!("Bad minutes: {}", parts[1]))?;
    let sec_parts: Vec<&str> = parts[2].split('.').collect();
    let sec: u64 = sec_parts[0]
        .parse()
        .map_err(|_| format!("Bad seconds: {}", sec_parts[0]))?;
    let ms: u64 = if sec_parts.len() > 1 {
        sec_parts[1].parse().unwrap_or(0)
    } else {
        0
    };
    Ok(h * 3_600_000 + m * 60_000 + sec * 1000 + ms)
}

fn write_ttml(cues: &[SubtitleCue], output: &std::path::Path) -> Result<(), String> {
    use std::io::Write;
    let mut f =
        std::fs::File::create(output).map_err(|e| format!("Failed to create output: {e}"))?;

    writeln!(f, r#"<?xml version="1.0" encoding="UTF-8"?>"#).map_err(|e| e.to_string())?;
    writeln!(
        f,
        r#"<tt xmlns="http://www.w3.org/ns/ttml" xmlns:ttp="http://www.w3.org/ns/ttml#parameter">"#
    )
    .map_err(|e| e.to_string())?;
    writeln!(f, "  <body>").map_err(|e| e.to_string())?;
    writeln!(f, "    <div>").map_err(|e| e.to_string())?;

    for cue in cues {
        let start = format_ttml_time(cue.start_ms);
        let end = format_ttml_time(cue.end_ms);
        let escaped = cue
            .text
            .replace('&', "&amp;")
            .replace('<', "&lt;")
            .replace('>', "&gt;");
        writeln!(f, r#"      <p begin="{start}" end="{end}">{escaped}</p>"#)
            .map_err(|e| e.to_string())?;
    }

    writeln!(f, "    </div>").map_err(|e| e.to_string())?;
    writeln!(f, "  </body>").map_err(|e| e.to_string())?;
    writeln!(f, "</tt>").map_err(|e| e.to_string())?;
    Ok(())
}

fn format_ttml_time(ms: u64) -> String {
    let h = ms / 3_600_000;
    let m = (ms % 3_600_000) / 60_000;
    let s = (ms % 60_000) / 1000;
    let f = ms % 1000;
    format!("{h:02}:{m:02}:{s:02}.{f:03}")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_srt_time() {
        assert_eq!(parse_srt_time("01:02:03,456").unwrap(), 3_723_456);
    }

    #[test]
    fn test_format_ttml_time() {
        assert_eq!(format_ttml_time(3_723_456), "01:02:03.456");
    }

    #[test]
    fn test_parse_srt() {
        let srt = "1\n00:00:01,000 --> 00:00:04,000\nHello world\n\n2\n00:00:05,000 --> 00:00:08,000\nSecond cue\n";
        let cues = parse_srt(srt).unwrap();
        assert_eq!(cues.len(), 2);
        assert_eq!(cues[0].text, "Hello world");
        assert_eq!(cues[1].start_ms, 5000);
    }
}
