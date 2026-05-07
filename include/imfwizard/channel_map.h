#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

// Audio channel layout definitions
enum class ChannelLayout
{
  Mono, // 1.0
  Stereo, // 2.0
  Surround_51, // 5.1
  Surround_71, // 7.1
  Atmos, // Object-based (7.1.4 bed)
};

struct ChannelMapOptions
{
  std::filesystem::path input_file; // Input WAV
  std::filesystem::path output_file; // Output WAV
  ChannelLayout source_layout = ChannelLayout::Surround_51;
  ChannelLayout target_layout = ChannelLayout::Stereo;
  double center_mix_level = -3.0; // dB
  double surround_mix_level = -3.0; // dB
  double lfe_mix_level = -10.0; // dB
};

struct ChannelMapResult
{
  std::filesystem::path output_file;
  uint32_t input_channels = 0;
  uint32_t output_channels = 0;
  bool success = false;
  std::string error;
};

// Remap audio channels using ffmpeg
ChannelMapResult remap_channels(const ChannelMapOptions& opts);

// Get channel count for a layout
uint32_t channel_count(ChannelLayout layout);

// Get ffmpeg channel layout string
const char* layout_to_ffmpeg(ChannelLayout layout);

} // namespace imfwizard
