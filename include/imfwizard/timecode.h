#pragma once

#include <cstdint>
#include <string>

namespace imfwizard
{

// Timecode representation (SMPTE 12M)
struct Timecode
{
  uint32_t hours = 0;
  uint32_t minutes = 0;
  uint32_t seconds = 0;
  uint32_t frames = 0;
  bool drop_frame = false;

  // Convert to total frame count
  uint64_t to_frames(double fps) const;

  // Convert to seconds
  double to_seconds(double fps) const;

  // Format as string HH:MM:SS:FF or HH:MM:SS;FF (drop frame)
  std::string to_string() const;

  // Create from total frame count
  static Timecode from_frames(uint64_t total_frames, double fps, bool drop_frame = false);

  // Create from seconds
  static Timecode from_seconds(double seconds, double fps, bool drop_frame = false);

  // Parse from string "HH:MM:SS:FF" or "HH:MM:SS;FF"
  static Timecode parse(const std::string& tc_str, double fps = 24.0);
};

struct TimecodeConvertOptions
{
  std::string input_tc;      // Timecode string to convert
  double source_fps = 24.0;
  double target_fps = 25.0;
  bool source_drop = false;
  bool target_drop = false;
};

struct TimecodeDriftOptions
{
  std::string video_path;    // Video file to check
  std::string expected_start_tc;
  double expected_fps = 24.0;
  uint32_t check_interval = 300; // Check every N frames
};

struct TimecodeDriftResult
{
  double max_drift_frames = 0;
  double max_drift_seconds = 0;
  uint32_t drift_frame_pos = 0; // Frame where max drift was found
  bool has_timecode = false;
  bool within_tolerance = false; // ±1 frame
  bool success = false;
  std::string error;
  std::string detected_start_tc;
  double detected_fps = 0;
};

// Convert timecode between frame rates
Timecode convert_timecode(const TimecodeConvertOptions& opts);

// Check for timecode drift in a video file
TimecodeDriftResult check_timecode_drift(const TimecodeDriftOptions& opts);

} // namespace imfwizard
