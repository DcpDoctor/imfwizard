#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

// ProRes codec IDs for IMF wrapping
enum class ProResProfile
{
  Proxy,    // 'apco'
  LT,       // 'apcs'
  Standard, // 'apcn'
  HQ,       // 'apch'
  XQ        // 'ap4h'
};

struct ProResImpOptions
{
  std::filesystem::path input_file; // ProRes .mov file
  std::filesystem::path output_dir; // Output IMP directory
  std::string title;
  std::string issuer = "IMF Wizard";
  ProResProfile profile = ProResProfile::HQ;
  uint32_t fps_num = 24;
  uint32_t fps_den = 1;
  uint32_t sample_rate = 48000;
  uint16_t audio_bit_depth = 24;
};

struct ProResImpResult
{
  std::filesystem::path output_dir;
  std::string cpl_uuid;
  std::string pkl_uuid;
  uint32_t frame_count = 0;
  bool success = false;
  std::string error;
};

// Create an IMF package with ProRes essence (Application 2E ProRes)
ProResImpResult create_prores_imp(const ProResImpOptions& opts);

// Detect ProRes profile from a QuickTime/MOV file
ProResProfile detect_prores_profile(const std::filesystem::path& file);

// Check if the input is a ProRes file
bool is_prores_file(const std::filesystem::path& file);

} // namespace imfwizard
