#pragma once

#include <filesystem>
#include <string>
#include <cstdint>

namespace imfwizard
{

struct BurnInOptions
{
  std::filesystem::path video_input; // Video file or J2K dir
  std::filesystem::path subtitle_file; // TTML/SRT/SCC file
  std::filesystem::path output; // Output video file
  std::string font = "DejaVu Sans"; // Font family
  uint16_t font_size = 48; // Font size in pixels
  std::string font_color = "white"; // Font color
  std::string outline_color = "black"; // Outline/shadow color
  uint8_t outline_width = 2; // Outline stroke width
  std::string position = "bottom"; // top, center, bottom
  uint16_t margin_bottom = 60; // Margin from bottom in pixels
  uint32_t threads = 0; // 0 = auto
};

struct BurnInResult
{
  std::filesystem::path output_file;
  uint32_t frame_count = 0;
  bool success = false;
  std::string error;
};

// Burn subtitles into video frames using FFmpeg
BurnInResult burn_in_subtitles(const BurnInOptions& opts);

// Check if ffmpeg subtitle filter is available
bool has_subtitle_filter();

} // namespace imfwizard
