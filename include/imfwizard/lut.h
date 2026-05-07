#pragma once

#include <filesystem>
#include <string>
#include <cstdint>

namespace imfwizard
{

struct LutOptions
{
  std::filesystem::path input_dir; // image sequence directory
  std::filesystem::path output_dir; // output image sequence
  std::filesystem::path lut_file; // .cube or .3dl LUT file
  uint16_t bit_depth = 16; // output bit depth
  uint32_t threads = 0; // 0 = auto
};

struct LutResult
{
  std::filesystem::path output_dir;
  uint32_t frames_processed = 0;
  bool success = false;
  std::string error;
};

// Apply a 3D LUT to an image sequence using ffmpeg
LutResult apply_lut(const LutOptions& opts);

// Validate a .cube LUT file
bool validate_cube_lut(const std::filesystem::path& lut_file);

} // namespace imfwizard
