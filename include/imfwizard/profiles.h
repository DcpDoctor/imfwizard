#pragma once

#include "imfwizard/hdr.h"
#include <string>
#include <cstdint>

namespace imfwizard
{

// Delivery profile presets for major platforms
struct DeliveryProfile
{
  std::string name;
  std::string description;

  // Video requirements
  uint32_t max_width = 0; // 0 = no limit
  uint32_t max_height = 0;
  float max_bitrate_mbps = 0;
  uint16_t bit_depth = 10;
  bool requires_hdr = false;
  HdrMetadata hdr_config;

  // Audio requirements
  uint32_t audio_sample_rate = 48000;
  uint16_t audio_bit_depth = 24;
  uint32_t max_audio_channels = 16;

  // Subtitle
  bool requires_subtitles = false;
  std::string subtitle_format = "ttml"; // ttml or imsc

  // Packaging
  bool require_signing = false;
  std::string app_constraint; // "App2E", "App2E+", "App4", "App5"
};

// Get a delivery profile by platform name
DeliveryProfile get_profile(const std::string& platform);

// Available platform names
inline const char* available_profiles[] = {
    "netflix",   "disney",    "amazon",       "apple",         "hbo",
    "cinema-2k", "cinema-4k", "broadcast-hd", "broadcast-uhd", "archival"};

// Netflix IMF delivery spec
DeliveryProfile netflix_profile();
// Disney+ delivery spec
DeliveryProfile disney_profile();
// Amazon Prime Video delivery spec
DeliveryProfile amazon_profile();
// Apple TV+ delivery spec
DeliveryProfile apple_profile();
// Cinema DCI 2K
DeliveryProfile cinema_2k_profile();
// Cinema DCI 4K
DeliveryProfile cinema_4k_profile();
// Broadcast HD (EBU)
DeliveryProfile broadcast_hd_profile();
// Broadcast UHD
DeliveryProfile broadcast_uhd_profile();
// Archival (ACES)
DeliveryProfile archival_profile();

} // namespace imfwizard
