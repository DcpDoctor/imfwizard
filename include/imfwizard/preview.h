#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

struct PreviewFrame
{
  uint32_t frame_number = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint8_t> rgba_data; // Raw RGBA pixels (for GUI display)
  std::string thumbnail_path; // Path to PNG thumbnail if generated
};

struct PreviewOptions
{
  std::filesystem::path imp_dir; // IMP directory
  std::filesystem::path j2k_dir; // Or standalone J2K frame dir
  uint32_t frame = 0; // Frame number to decode
  uint32_t thumbnail_width = 320; // Thumbnail width (proportional height)
  bool generate_thumbnail = true; // Write PNG thumbnail
  std::filesystem::path output_dir; // Where to write thumbnails
};

struct PreviewResult
{
  PreviewFrame frame;
  bool success = false;
  std::string error;
};

struct ThumbnailStripOptions
{
  std::filesystem::path source_dir; // J2K or IMP
  std::filesystem::path output_dir; // Output thumbnails
  uint32_t count = 10; // Number of thumbnails to generate
  uint32_t width = 160; // Thumbnail width
};

struct ThumbnailStripResult
{
  std::vector<std::string> thumbnails; // Paths to generated thumbnails
  uint32_t total_frames = 0;
  bool success = false;
  std::string error;
};

// Decode a single J2K frame to PNG thumbnail (uses grk_decompress)
PreviewResult decode_preview_frame(const PreviewOptions& opts);

// Generate a strip of thumbnails at regular intervals
ThumbnailStripResult generate_thumbnail_strip(const ThumbnailStripOptions& opts);

// Get frame count from J2K directory
uint32_t count_j2k_frames(const std::filesystem::path& dir);

} // namespace imfwizard
