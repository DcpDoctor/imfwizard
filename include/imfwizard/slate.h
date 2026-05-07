#pragma once

#include <filesystem>
#include <string>
#include <cstdint>

namespace imfwizard
{

enum class SlateType
{
  Countdown, // 10-second countdown
  AcademyLeader, // SMPTE universal leader
  ColorBars, // SMPTE color bars + tone
  Slate, // Text slate (title, date, reel info)
  Black, // Black frames
};

struct SlateOptions
{
  SlateType type = SlateType::Countdown;
  std::filesystem::path output_dir; // output image sequence directory
  uint32_t width = 1920;
  uint32_t height = 1080;
  uint32_t fps_num = 24;
  uint32_t fps_den = 1;
  uint32_t duration_frames = 0; // 0 = auto (countdown=240, bars=48, slate=48, black=24)

  // Slate-specific options
  std::string title;
  std::string date;
  std::string reel_info;
  std::string client;

  // Audio options
  bool generate_tone = false; // 1kHz reference tone (for bars)
  std::filesystem::path tone_output; // output WAV for tone
};

struct SlateResult
{
  std::filesystem::path output_dir;
  uint32_t frames_generated = 0;
  std::filesystem::path tone_file; // if tone was generated
  bool success = false;
  std::string error;
};

// Generate leader/slate image sequence
SlateResult generate_slate(const SlateOptions& opts);

} // namespace imfwizard
