#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace imfwizard
{

struct SdiDevice
{
  uint32_t index = 0;
  std::string name;
  std::string driver; // "decklink", "aja", etc.
};

struct SdiOutputOptions
{
  std::filesystem::path input_dir; // J2K frame directory or MXF file
  std::filesystem::path audio_file; // optional audio file for embedded SDI audio
  uint32_t device_index = 0; // DeckLink device number
  uint32_t fps_num = 24;
  uint32_t fps_den = 1;
  uint32_t width = 1920;
  uint32_t height = 1080;
  bool loop = false; // loop playback
  uint32_t start_frame = 0;
  uint32_t end_frame = 0; // 0 = all
  std::string pixel_format = "v210"; // v210 (10-bit YCbCr) or ARGB
};

struct SdiOutputResult
{
  uint32_t frames_played = 0;
  uint32_t dropped_frames = 0;
  bool success = false;
  std::string error;
};

// List available SDI output devices (via GStreamer device monitor)
std::vector<SdiDevice> list_sdi_devices();

// Play frames out over SDI via GStreamer + decklinkvideosink
SdiOutputResult sdi_preview(const SdiOutputOptions& opts);

// Check if GStreamer DeckLink plugin is available
bool decklink_available();

} // namespace imfwizard
