#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace imfwizard
{

// Individual QC issue found during automated analysis
enum class QcIssueType
{
  BlackFrame,
  FreezeFrame,
  AudioSilence,
  AudioClipping,
  ColorGamutExcursion,
  BarsAndTone,
  DropFrame,
  InvalidTimecode,
};

struct QcIssue
{
  QcIssueType type;
  uint32_t start_frame = 0;
  uint32_t end_frame = 0; // same as start for single-frame issues
  double start_timecode_sec = 0;
  double end_timecode_sec = 0;
  std::string description;
  std::string severity; // "error", "warning", "info"
};

struct AutoQcOptions
{
  std::filesystem::path video_path; // Video file or J2K directory
  std::filesystem::path audio_path; // Audio file (WAV/MXF)
  uint32_t fps_num = 24;
  uint32_t fps_den = 1;

  // Black frame detection
  bool detect_black = true;
  double black_threshold = 16.0;  // pixel value threshold (0-255 scale)
  double black_ratio = 0.98;      // ratio of pixels below threshold
  uint32_t black_min_frames = 1;  // minimum consecutive frames to flag

  // Freeze frame detection
  bool detect_freeze = true;
  double freeze_threshold = 0.001; // max pixel difference to count as frozen
  uint32_t freeze_min_frames = 48; // minimum consecutive frames (~2sec at 24fps)

  // Audio silence detection
  bool detect_silence = true;
  double silence_threshold_db = -60.0; // dBFS
  double silence_min_duration = 2.0;   // seconds

  // Audio clipping detection
  bool detect_clipping = true;
  double clipping_threshold = 0.99; // sample value ratio

  // Gamut excursion detection
  bool detect_gamut = false; // disabled by default (expensive)
  double gamut_threshold = 1.0; // percentage of out-of-gamut pixels

  // Bars/tone detection (first N frames)
  bool detect_bars = true;
  uint32_t bars_check_frames = 120; // check first 5sec at 24fps

  bool json_output = false;
};

struct AutoQcResult
{
  std::vector<QcIssue> issues;
  uint32_t total_frames = 0;
  double duration_seconds = 0;
  uint32_t error_count = 0;
  uint32_t warning_count = 0;
  bool success = false;
  std::string error;
};

// Run full automated QC analysis
AutoQcResult run_auto_qc(const AutoQcOptions& opts);

// Generate JSON report from results
std::string auto_qc_to_json(const AutoQcResult& result);

} // namespace imfwizard
