#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

struct AtmosBedChannel
{
  std::string label; // e.g. "L", "R", "C", "LFE", "Ls", "Rs"
  uint32_t channel_index = 0;
};

struct AtmosObject
{
  std::string id;
  uint32_t start_frame = 0;
  uint32_t duration = 0;
  float azimuth = 0; // -180 to 180
  float elevation = 0; // -90 to 90
  float distance = 1.0f;
};

struct AtmosImportOptions
{
  std::filesystem::path input_file; // ADM BWF or Atmos master
  std::filesystem::path output_dir; // output MXF directory
  uint32_t edit_rate_num = 24;
  uint32_t edit_rate_den = 1;
  uint32_t sample_rate = 48000;
  uint16_t bit_depth = 24;
};

struct AtmosImportResult
{
  std::filesystem::path iab_mxf; // IAB track file
  std::vector<AtmosBedChannel> bed_channels;
  uint32_t object_count = 0;
  uint32_t bed_channel_count = 0;
  uint32_t sample_count = 0;
  bool success = false;
  std::string error;
};

// Import ADM BWF and wrap as IAB MXF (ST 2067-201)
AtmosImportResult import_atmos(const AtmosImportOptions& opts);

// Check if ADM/Atmos tools are available
bool atmos_tools_available();

} // namespace imfwizard
