#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

// Extract tracks/segments from an existing IMP back to raw files
struct ExtractOptions
{
  std::filesystem::path imp_dir; // Input IMP directory
  std::filesystem::path output_dir; // Where to write extracted files
  std::vector<std::string> track_uuids; // Specific tracks to extract (empty = all)
  uint32_t start_frame = 0; // 0 = from beginning
  uint32_t end_frame = 0; // 0 = to end
  std::string output_format = "tiff"; // Output image format
};

struct ExtractResult
{
  std::filesystem::path output_dir;
  uint32_t video_frames = 0;
  uint32_t audio_samples = 0;
  std::vector<std::filesystem::path> extracted_files;
  bool success = false;
  std::string error;
};

// Extract essence from MXF track files
ExtractResult extract_from_imp(const ExtractOptions& opts);

} // namespace imfwizard
