#pragma once

#include "imfwizard/mxf_wrap.h"
#include "imfwizard/cpl.h"
#include "imfwizard/signature.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

struct SupplementalSegment
{
  uint32_t entry_point = 0; // frame offset in OV
  uint32_t duration = 0; // frames to replace
  std::filesystem::path video_dir; // replacement J2K frames (empty = reuse OV)
  std::filesystem::path audio_file; // replacement audio (empty = reuse OV)
};

struct SupplementalOptions
{
  std::string title;
  std::string issuer;

  // Reference to the Original Version IMP
  std::filesystem::path ov_imp_dir;

  // Replacement segments
  std::vector<SupplementalSegment> segments;

  // Timing
  uint32_t edit_rate_num = 24;
  uint32_t edit_rate_den = 1;
  uint32_t audio_sample_rate = 48000;
  uint16_t audio_bit_depth = 24;

  // Output
  std::filesystem::path output_dir;

  // Optional signing
  std::optional<SignOptions> sign;
};

struct SupplementalResult
{
  std::filesystem::path output_dir;
  std::string cpl_uuid;
  std::string pkl_uuid;
  std::vector<MxfTrackFile> new_track_files;
  bool success = false;
  std::string error;
};

// Create a Supplemental IMP referencing an existing OV
SupplementalResult create_supplemental_imp(const SupplementalOptions& opts);

} // namespace imfwizard
