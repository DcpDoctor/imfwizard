#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

// Edit Decision List entry for CPL conforming
struct EdlEntry
{
  std::string reel_name;
  uint32_t source_in = 0; // Source start frame
  uint32_t source_out = 0; // Source end frame
  uint32_t record_in = 0; // Timeline start frame
  uint32_t record_out = 0; // Timeline end frame
  std::string track_type; // "V" or "A" or "A2", etc.
};

struct ConformOptions
{
  std::filesystem::path edl_file; // EDL or AAF file
  std::filesystem::path media_dir; // Directory containing source reels
  std::filesystem::path output_dir; // Output IMP directory
  std::string title;
  uint32_t fps_num = 24;
  uint32_t fps_den = 1;
};

struct ConformResult
{
  std::vector<EdlEntry> entries;
  uint32_t total_duration = 0;
  bool success = false;
  std::string error;
};

// Parse an EDL file (CMX3600 format)
std::vector<EdlEntry> parse_edl(const std::filesystem::path& edl_file);

// Parse an AAF file for edit decisions
std::vector<EdlEntry> parse_aaf(const std::filesystem::path& aaf_file);

// Conform media from EDL/AAF into an IMP
ConformResult conform_from_edl(const ConformOptions& opts);

} // namespace imfwizard
