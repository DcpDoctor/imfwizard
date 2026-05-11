use serde::{Deserialize, Serialize};

/// SMPTE timecode.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
pub struct Timecode {
    pub hours: u8,
    pub minutes: u8,
    pub seconds: u8,
    pub frames: u8,
    pub fps: u8,
    pub drop_frame: bool,
}

impl Timecode {
    pub fn new(hours: u8, minutes: u8, seconds: u8, frames: u8, fps: u8) -> Self {
        Self {
            hours,
            minutes,
            seconds,
            frames,
            fps,
            drop_frame: false,
        }
    }

    /// Parse timecode from "HH:MM:SS:FF" or "HH:MM:SS;FF" string.
    pub fn parse(s: &str, fps: u8) -> Result<Self, String> {
        let drop_frame = s.contains(';');
        let s = s.replace(';', ":");
        let parts: Vec<&str> = s.split(':').collect();
        if parts.len() != 4 {
            return Err(format!("Invalid timecode format: {s}"));
        }
        Ok(Self {
            hours: parts[0].parse().map_err(|_| "Invalid hours")?,
            minutes: parts[1].parse().map_err(|_| "Invalid minutes")?,
            seconds: parts[2].parse().map_err(|_| "Invalid seconds")?,
            frames: parts[3].parse().map_err(|_| "Invalid frames")?,
            fps,
            drop_frame,
        })
    }

    /// Total frame count.
    pub fn to_frames(&self) -> u64 {
        let fps = self.fps as u64;
        let total = (self.hours as u64) * 3600 * fps
            + (self.minutes as u64) * 60 * fps
            + (self.seconds as u64) * fps
            + (self.frames as u64);

        if self.drop_frame && (self.fps == 30 || self.fps == 60) {
            // SMPTE drop-frame compensation
            let d = if self.fps == 30 { 2u64 } else { 4u64 };
            let total_minutes = (self.hours as u64) * 60 + (self.minutes as u64);
            let dropped = d * (total_minutes - total_minutes / 10);
            total - dropped
        } else {
            total
        }
    }

    /// Convert frame count to timecode.
    pub fn from_frames(mut frame_count: u64, fps: u8) -> Self {
        let f = fps as u64;
        let frames = (frame_count % f) as u8;
        frame_count /= f;
        let seconds = (frame_count % 60) as u8;
        frame_count /= 60;
        let minutes = (frame_count % 60) as u8;
        let hours = (frame_count / 60) as u8;
        Self {
            hours,
            minutes,
            seconds,
            frames,
            fps,
            drop_frame: false,
        }
    }
}

impl std::fmt::Display for Timecode {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let sep = if self.drop_frame { ';' } else { ':' };
        write!(
            f,
            "{:02}:{:02}:{:02}{sep}{:02}",
            self.hours, self.minutes, self.seconds, self.frames
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_timecode() {
        let tc = Timecode::parse("01:02:03:04", 24).unwrap();
        assert_eq!(tc.hours, 1);
        assert_eq!(tc.minutes, 2);
        assert_eq!(tc.seconds, 3);
        assert_eq!(tc.frames, 4);
    }

    #[test]
    fn test_to_frames() {
        let tc = Timecode::new(0, 0, 1, 0, 24);
        assert_eq!(tc.to_frames(), 24);
    }

    #[test]
    fn test_from_frames() {
        let tc = Timecode::from_frames(48, 24);
        assert_eq!(tc.seconds, 2);
        assert_eq!(tc.frames, 0);
    }

    #[test]
    fn test_display() {
        let tc = Timecode::new(1, 2, 3, 4, 24);
        assert_eq!(tc.to_string(), "01:02:03:04");
    }

    #[test]
    fn test_drop_frame_display() {
        let mut tc = Timecode::new(1, 2, 3, 4, 30);
        tc.drop_frame = true;
        assert_eq!(tc.to_string(), "01:02:03;04");
    }
}
