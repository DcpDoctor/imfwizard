#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

struct PlaybackFrame
{
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint8_t> rgb_data; // Raw RGB24 pixel data
  uint32_t frame_number = 0;
};

struct MxfPlaybackOptions
{
  std::filesystem::path mxf_file;
  uint32_t start_frame = 0;
  uint32_t end_frame = 0; // 0 = to end
  double playback_fps = 0.0; // 0 = use native rate
  bool decode_audio = false;
  std::filesystem::path thumbnail_dir; // If set, save thumbnails here
  uint32_t thumbnail_interval = 24;   // Every N frames
};

struct MxfPlaybackResult
{
  bool success = false;
  std::string error;
  uint32_t total_frames = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  double fps = 0.0;
  std::string codec; // "j2k", "prores", "mpeg2", etc.
  double duration_seconds = 0.0;
  std::vector<std::filesystem::path> thumbnails;
};

// Get MXF file info without decoding frames
MxfPlaybackResult probe_mxf(const std::filesystem::path& mxf_file);

// Extract a single frame as RGB data (for GUI preview)
PlaybackFrame extract_frame(const std::filesystem::path& mxf_file, uint32_t frame_number);

// Generate thumbnails from MXF track file
MxfPlaybackResult generate_thumbnails(const MxfPlaybackOptions& opts);

// Launch GStreamer playback pipeline (returns PID for control)
int launch_playback(const MxfPlaybackOptions& opts);

} // namespace imfwizard
