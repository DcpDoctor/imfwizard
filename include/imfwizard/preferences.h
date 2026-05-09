#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace imfwizard
{

/// User preferences stored in ~/.config/imfwizard/preferences.json
struct Preferences
{
  // General
  std::string default_app_profile = "App2e";   // "App2", "App2e", "App4", "App5"
  std::string creator_name;
  std::string default_language = "en";         // RFC 5646

  // Encoding
  std::string preferred_encoder = "grok";
  uint32_t default_bandwidth_mbps = 250;
  std::string default_colour_space = "Rec.709";
  int gpu_device = -1;

  // HDR
  std::string default_hdr_mode = "SDR";        // "SDR", "HLG", "PQ"

  // Audio
  std::string default_channel_config = "5.1";
  double loudness_target_lufs = -24.0;

  // Signing
  std::string signing_certificate_path;
  std::string signing_key_path;

  // Delivery
  std::string default_output_dir;
  std::string naming_template;

  // GUI
  std::string theme = "dark";
  bool show_advanced_options = false;
};

/// Get the platform-specific preferences file path.
std::filesystem::path preferences_path();

/// Load preferences from disk (returns defaults if file doesn't exist).
Preferences load_preferences();

/// Save preferences to disk.
int save_preferences(const Preferences& prefs);

} // namespace imfwizard
