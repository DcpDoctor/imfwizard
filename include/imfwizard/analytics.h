#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

// Per-frame analytics entry
struct FrameAnalytics
{
  uint32_t frame_number = 0;
  uint64_t size_bytes = 0;
  double bitrate_mbps = 0;
  uint16_t tile_count = 0;
  uint8_t decomposition_levels = 0;
  uint16_t quality_layers = 0;
};

// Overall analytics result
struct AnalyticsResult
{
  std::vector<FrameAnalytics> frames;
  double avg_bitrate_mbps = 0;
  double peak_bitrate_mbps = 0;
  double min_bitrate_mbps = 0;
  double stddev_bitrate_mbps = 0;
  uint64_t total_bytes = 0;
  uint32_t total_frames = 0;
  double duration_seconds = 0;

  // Distribution buckets for histogram (10 buckets)
  std::vector<uint32_t> bitrate_histogram;
  double histogram_min = 0;
  double histogram_max = 0;

  // Per-second averages for time series
  std::vector<double> per_second_bitrate;

  bool success = false;
  std::string error;
};

struct AnalyticsOptions
{
  std::filesystem::path source; // J2K directory or IMP directory
  uint32_t fps_num = 24;
  uint32_t fps_den = 1;
  bool parse_headers = true; // Parse J2K headers for tile/level info
};

// Generate full analytics for J2K stream
AnalyticsResult compute_analytics(const AnalyticsOptions& opts);

// Export analytics to JSON (for GUI dashboard)
std::string analytics_to_json(const AnalyticsResult& result);

} // namespace imfwizard
