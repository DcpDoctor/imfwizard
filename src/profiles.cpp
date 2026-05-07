#include "imfwizard/profiles.h"

namespace imfwizard
{

DeliveryProfile netflix_profile()
{
  DeliveryProfile p;
  p.name = "netflix";
  p.description = "Netflix IMF Delivery (HDR10, 4K)";
  p.max_width = 3840;
  p.max_height = 2160;
  p.max_bitrate_mbps = 250;
  p.bit_depth = 10;
  p.requires_hdr = true;
  p.hdr_config = hdr10_preset();
  p.audio_sample_rate = 48000;
  p.audio_bit_depth = 24;
  p.max_audio_channels = 16;
  p.requires_subtitles = true;
  p.subtitle_format = "imsc";
  p.require_signing = false;
  p.app_constraint = "App2E+";
  return p;
}

DeliveryProfile disney_profile()
{
  DeliveryProfile p;
  p.name = "disney";
  p.description = "Disney+ IMF Delivery (HDR10/DV, 4K)";
  p.max_width = 3840;
  p.max_height = 2160;
  p.max_bitrate_mbps = 250;
  p.bit_depth = 10;
  p.requires_hdr = true;
  p.hdr_config = hdr10_preset();
  p.audio_sample_rate = 48000;
  p.audio_bit_depth = 24;
  p.max_audio_channels = 16;
  p.requires_subtitles = true;
  p.subtitle_format = "imsc";
  p.app_constraint = "App2E+";
  return p;
}

DeliveryProfile amazon_profile()
{
  DeliveryProfile p;
  p.name = "amazon";
  p.description = "Amazon Prime Video IMF (HDR10+, 4K)";
  p.max_width = 3840;
  p.max_height = 2160;
  p.max_bitrate_mbps = 250;
  p.bit_depth = 10;
  p.requires_hdr = true;
  p.hdr_config = hdr10_preset();
  p.audio_sample_rate = 48000;
  p.audio_bit_depth = 24;
  p.max_audio_channels = 8;
  p.requires_subtitles = true;
  p.subtitle_format = "ttml";
  p.app_constraint = "App2E+";
  return p;
}

DeliveryProfile apple_profile()
{
  DeliveryProfile p;
  p.name = "apple";
  p.description = "Apple TV+ IMF Delivery (Dolby Vision, 4K)";
  p.max_width = 3840;
  p.max_height = 2160;
  p.max_bitrate_mbps = 300;
  p.bit_depth = 12;
  p.requires_hdr = true;
  p.hdr_config = hdr10_preset();
  p.audio_sample_rate = 48000;
  p.audio_bit_depth = 24;
  p.max_audio_channels = 16;
  p.requires_subtitles = true;
  p.subtitle_format = "imsc";
  p.app_constraint = "App2E+";
  return p;
}

DeliveryProfile cinema_2k_profile()
{
  DeliveryProfile p;
  p.name = "cinema-2k";
  p.description = "DCI Cinema 2K (2048x1080)";
  p.max_width = 2048;
  p.max_height = 1080;
  p.max_bitrate_mbps = 250;
  p.bit_depth = 12;
  p.requires_hdr = false;
  p.hdr_config = p3d65_sdr_preset();
  p.audio_sample_rate = 48000;
  p.audio_bit_depth = 24;
  p.max_audio_channels = 16;
  p.require_signing = true;
  p.app_constraint = "App4";
  return p;
}

DeliveryProfile cinema_4k_profile()
{
  DeliveryProfile p;
  p.name = "cinema-4k";
  p.description = "DCI Cinema 4K (4096x2160)";
  p.max_width = 4096;
  p.max_height = 2160;
  p.max_bitrate_mbps = 500;
  p.bit_depth = 12;
  p.requires_hdr = false;
  p.hdr_config = p3d65_sdr_preset();
  p.audio_sample_rate = 48000;
  p.audio_bit_depth = 24;
  p.max_audio_channels = 16;
  p.require_signing = true;
  p.app_constraint = "App4";
  return p;
}

DeliveryProfile broadcast_hd_profile()
{
  DeliveryProfile p;
  p.name = "broadcast-hd";
  p.description = "Broadcast HD (1920x1080, EBU)";
  p.max_width = 1920;
  p.max_height = 1080;
  p.max_bitrate_mbps = 150;
  p.bit_depth = 10;
  p.audio_sample_rate = 48000;
  p.audio_bit_depth = 24;
  p.max_audio_channels = 8;
  p.app_constraint = "App2E";
  return p;
}

DeliveryProfile broadcast_uhd_profile()
{
  DeliveryProfile p;
  p.name = "broadcast-uhd";
  p.description = "Broadcast UHD (3840x2160, HLG)";
  p.max_width = 3840;
  p.max_height = 2160;
  p.max_bitrate_mbps = 250;
  p.bit_depth = 10;
  p.requires_hdr = true;
  p.hdr_config = hlg_preset();
  p.audio_sample_rate = 48000;
  p.audio_bit_depth = 24;
  p.max_audio_channels = 16;
  p.app_constraint = "App2E+";
  return p;
}

DeliveryProfile archival_profile()
{
  DeliveryProfile p;
  p.name = "archival";
  p.description = "Archival (ACES, full resolution, lossless)";
  p.max_width = 0;
  p.max_height = 0; // No limit
  p.max_bitrate_mbps = 0; // Lossless
  p.bit_depth = 16;
  p.audio_sample_rate = 96000;
  p.audio_bit_depth = 24;
  p.max_audio_channels = 128;
  p.app_constraint = "App5";
  return p;
}

DeliveryProfile get_profile(const std::string& platform)
{
  if(platform == "netflix")
    return netflix_profile();
  if(platform == "disney")
    return disney_profile();
  if(platform == "amazon")
    return amazon_profile();
  if(platform == "apple")
    return apple_profile();
  if(platform == "cinema-2k")
    return cinema_2k_profile();
  if(platform == "cinema-4k")
    return cinema_4k_profile();
  if(platform == "broadcast-hd")
    return broadcast_hd_profile();
  if(platform == "broadcast-uhd")
    return broadcast_uhd_profile();
  if(platform == "archival")
    return archival_profile();
  return netflix_profile(); // Default
}

} // namespace imfwizard
