#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

struct TimedTextOptions
{
  std::filesystem::path ttml_file; // TTML/IMSC XML file
  std::filesystem::path output_path; // Output MXF path
  uint32_t edit_rate_num = 24;
  uint32_t edit_rate_den = 1;
  std::vector<std::filesystem::path> font_files; // Referenced fonts
};

struct TimedTextTrackFile
{
  std::filesystem::path path;
  std::string uuid;
  std::string hash;
  uint64_t size = 0;
  uint32_t duration = 0;
};

// Wrap TTML/IMSC subtitle into AS-02 MXF timed text track file
TimedTextTrackFile wrap_timed_text(const TimedTextOptions& opts);

} // namespace imfwizard
