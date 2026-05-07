#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

// Dolby Vision profile 4.0 (MEL — Minimum Enhancement Layer)
// Used for IMF packaging of Dolby Vision HDR content
struct DolbyVisionConfig
{
  uint8_t profile = 4; // DV profile (4 = IMF compatible)
  uint8_t level = 6; // DV level
  bool rpu_present = true; // RPU (Reference Processing Unit) data present
  bool el_present = false; // Enhancement layer present (profile 4 = false)
  bool bl_present = true; // Base layer present
  uint8_t bl_signal_compatibility_id = 0;

  // RPU metadata file (XML or binary RPU)
  std::filesystem::path rpu_file;
};

struct DolbyVisionResult
{
  bool success = false;
  std::string error;
  std::filesystem::path output_mxf; // MXF with DV metadata
};

// Inject Dolby Vision RPU metadata into an existing MXF track
DolbyVisionResult inject_dolby_vision_metadata(const std::filesystem::path& source_mxf,
                                               const DolbyVisionConfig& config,
                                               const std::filesystem::path& output_dir);

// Validate a Dolby Vision RPU file
bool validate_rpu(const std::filesystem::path& rpu_file);

// Check if dovi_tool is available for RPU manipulation
bool dovi_tool_available();

} // namespace imfwizard
