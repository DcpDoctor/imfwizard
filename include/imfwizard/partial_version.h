#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

struct ReelSegment
{
  uint32_t reel_index = 0;
  uint32_t start_frame = 0; // within reel
  uint32_t end_frame = 0; // within reel (0 = to end)
};

struct PartialVersionOptions
{
  std::filesystem::path ov_imp_dir; // Original Version IMP
  std::filesystem::path output_dir; // output Supplemental IMP
  std::vector<ReelSegment> segments; // segments to replace
  std::filesystem::path replacement_video; // new video for replaced segments
  std::filesystem::path replacement_audio; // new audio (optional)
  std::string title;
  std::string issuer;
};

struct PartialVersionResult
{
  std::filesystem::path output_dir;
  std::string cpl_uuid;
  uint32_t segments_replaced = 0;
  uint32_t total_frames = 0;
  bool success = false;
  std::string error;
};

// Create a supplemental IMP that replaces specific reel segments
PartialVersionResult create_partial_version(const PartialVersionOptions& opts);

} // namespace imfwizard
