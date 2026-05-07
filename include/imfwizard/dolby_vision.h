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

// === Dolby Vision Profile 8.1 (HDR10+ backward compatible) ===
// Profile 8.1: Single-layer, BL+RPU, backward-compatible with HDR10
// Used for OTT/streaming delivery with player-side tone mapping
struct DolbyVision81Config
{
  uint8_t profile = 8;
  uint8_t level = 6;            // Typically 6 for 4K60
  uint8_t bl_signal_compatibility_id = 1; // 1 = HDR10 compatible
  bool cross_compatibility = true; // Allow HDR10+ fallback

  // Source content
  std::filesystem::path source_mxf;       // Input MXF (base layer)
  std::filesystem::path rpu_file;          // RPU metadata (binary or XML)
  std::filesystem::path output_dir;

  // MEL/FEL mapping
  bool mel_flag = true;          // Minimum Enhancement Layer mapping
  bool fel_flag = false;         // Full Enhancement Layer (requires profile 7)

  // Target display parameters
  uint16_t target_max_luminance = 1000;  // nits
  uint16_t target_min_luminance = 0;     // nits (0.0001 typical)
  std::string target_display = "default"; // "default", "cinema", "home_hdr"

  // Tone mapping
  std::string tone_map_method = "polynomial"; // "polynomial", "mmr", "pivoted"
};

struct DolbyVision81Result
{
  bool success = false;
  std::string error;
  std::filesystem::path output_mxf;
  uint32_t rpu_frames_injected = 0;
  bool hdr10_compatible = false; // Verified backward compatibility
};

// Inject DV Profile 8.1 metadata (HDR10-compatible single-layer)
DolbyVision81Result inject_dv81_metadata(const DolbyVision81Config& config);

// Convert Profile 4 RPU to Profile 8.1 (for IMF-to-OTT conversion)
DolbyVision81Result convert_profile4_to_81(const std::filesystem::path& profile4_rpu,
                                           const DolbyVision81Config& config);

// Verify HDR10 backward compatibility of a Profile 8.1 deliverable
bool verify_hdr10_compatibility(const std::filesystem::path& mxf_file);

} // namespace imfwizard
