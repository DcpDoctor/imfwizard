#pragma once

#include <filesystem>
#include <string>
#include <cstdint>

namespace imfwizard
{

struct SubtitleRetimeOptions
{
  std::filesystem::path input_file; // TTML/SRT input
  std::filesystem::path output_file; // TTML output
  uint32_t source_fps_num = 24000; // source framerate numerator
  uint32_t source_fps_den = 1001; // source framerate denominator (23.976)
  uint32_t target_fps_num = 24; // target framerate numerator
  uint32_t target_fps_den = 1; // target framerate denominator
  bool stretch = false; // true = stretch timing, false = snap to nearest frame
};

struct SubtitleRetimeResult
{
  std::filesystem::path output_file;
  uint32_t entries_processed = 0;
  double time_shift_ms = 0; // total accumulated shift at end
  bool success = false;
  std::string error;
};

// Re-time subtitle file from one framerate to another
SubtitleRetimeResult retime_subtitles(const SubtitleRetimeOptions& opts);

} // namespace imfwizard
