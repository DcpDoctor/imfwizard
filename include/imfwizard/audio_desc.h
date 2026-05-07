#pragma once

#include "imfwizard/mca.h"
#include <filesystem>
#include <string>
#include <cstdint>

namespace imfwizard
{

struct AudioDescOptions
{
  std::filesystem::path main_audio; // main 5.1/7.1 mix
  std::filesystem::path ad_audio; // audio description narration track
  std::filesystem::path output_file; // output combined MXF
  float ad_mix_level = -6.0f; // AD ducking level (dB)
  std::string language = "en"; // RFC 5646 language tag
  uint32_t sample_rate = 48000;
  uint16_t bit_depth = 24;
};

struct AudioDescResult
{
  std::filesystem::path output_file;
  McaSoundfield soundfield; // MCA labels for the output
  uint32_t total_channels = 0;
  bool success = false;
  std::string error;
};

// Create audio description track with proper MCA labeling
AudioDescResult create_audio_description(const AudioDescOptions& opts);

} // namespace imfwizard
