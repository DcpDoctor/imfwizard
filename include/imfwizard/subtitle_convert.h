#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

enum class SubtitleFormat
{
  SRT,
  TTML,
  WebVTT,
  SCC,
  STL, // EBU STL
  IMSC, // IMSC1 (a profile of TTML)
  Unknown,
};

struct SubtitleCue
{
  uint32_t index = 0;
  double start_time = 0; // seconds
  double end_time = 0;   // seconds
  std::string text;      // plain text content
  std::string style;     // optional style/positioning
  std::string region;    // optional region ID (TTML/IMSC)
};

struct SubtitleConvertOptions
{
  std::filesystem::path input;
  std::filesystem::path output;
  SubtitleFormat source_format = SubtitleFormat::Unknown; // auto-detect if Unknown
  SubtitleFormat target_format = SubtitleFormat::TTML;
  double fps = 24.0;       // needed for SCC timecodes
  double offset_sec = 0.0; // time offset to add/subtract
  std::string language = "en";
  std::string title;       // optional title for TTML/IMSC
};

struct SubtitleConvertResult
{
  uint32_t cue_count = 0;
  double duration_seconds = 0;
  bool success = false;
  std::string error;
  std::filesystem::path output_path;
};

// Detect subtitle format from file extension/content
SubtitleFormat detect_subtitle_format(const std::filesystem::path& file);

// Parse subtitle file into cues
std::vector<SubtitleCue> parse_subtitles(const std::filesystem::path& file,
                                         SubtitleFormat format, double fps = 24.0);

// Write cues to a file in the target format
SubtitleConvertResult write_subtitles(const std::vector<SubtitleCue>& cues,
                                      const SubtitleConvertOptions& opts);

// High-level convert function
SubtitleConvertResult convert_subtitles(const SubtitleConvertOptions& opts);

// Format name string
std::string subtitle_format_name(SubtitleFormat fmt);

// Parse format from string (e.g., "srt", "ttml", "webvtt")
SubtitleFormat parse_subtitle_format(const std::string& name);

} // namespace imfwizard
