#pragma once

#include "imfwizard/hdr.h"
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

enum class AcesTransform
{
  IDT, // Input Device Transform
  RRT, // Reference Rendering Transform
  ODT, // Output Device Transform
};

struct AcesOptions
{
  std::filesystem::path input_dir; // source image sequence
  std::filesystem::path output_dir; // ACES output sequence
  std::string idt_name; // e.g. "ARRI_LogC4", "RED_Log3G10", "Sony_SLog3"
  std::string odt_name; // e.g. "P3D65_108nits", "Rec709_100nits", "P3D65_PQ_4000nits"
  std::filesystem::path ctl_dir; // directory of CTL transform files (optional)
  uint16_t bit_depth = 16;
  uint32_t threads = 0;
};

struct AcesResult
{
  std::filesystem::path output_dir;
  uint32_t frames_processed = 0;
  std::string idt_applied;
  std::string odt_applied;
  HdrMetadata output_hdr; // resulting HDR metadata
  bool success = false;
  std::string error;
};

// Apply ACES IDT→RRT→ODT pipeline to an image sequence
AcesResult apply_aces_pipeline(const AcesOptions& opts);

// List available IDTs
std::vector<std::string> list_available_idts();

// List available ODTs
std::vector<std::string> list_available_odts();

// Check if ctlrender is available
bool ctl_available();

} // namespace imfwizard
