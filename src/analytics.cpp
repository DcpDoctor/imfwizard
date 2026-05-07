#include "imfwizard/analytics.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numeric>
#include <sstream>

namespace imfwizard
{

static std::vector<std::filesystem::path> list_j2k_files(const std::filesystem::path& dir)
{
  std::vector<std::filesystem::path> files;
  if(!std::filesystem::is_directory(dir))
    return files;

  for(auto& entry : std::filesystem::directory_iterator(dir))
  {
    if(!entry.is_regular_file())
      continue;
    auto ext = entry.path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if(ext == ".j2c" || ext == ".j2k" || ext == ".jp2")
      files.push_back(entry.path());
  }
  std::sort(files.begin(), files.end());
  return files;
}

// Parse J2K main header to extract tile/level/layer info
static void parse_j2k_header(const std::filesystem::path& file, FrameAnalytics& fa)
{
  std::ifstream f(file, std::ios::binary);
  if(!f)
    return;

  // Read first 256 bytes (enough for SIZ + COD markers)
  std::vector<uint8_t> header(256);
  f.read(reinterpret_cast<char*>(header.data()), header.size());
  auto bytes_read = f.gcount();
  if(bytes_read < 10)
    return;

  // Look for SIZ marker (0xFF51) for tile info
  for(size_t i = 0; i + 1 < static_cast<size_t>(bytes_read); ++i)
  {
    if(header[i] == 0xFF && header[i + 1] == 0x51 && i + 40 < static_cast<size_t>(bytes_read))
    {
      // SIZ marker: offset +36 and +38 have tile dimensions
      uint16_t xt = (header[i + 36] << 8) | header[i + 37];
      uint16_t yt = (header[i + 38] << 8) | header[i + 39];
      if(xt > 0 && yt > 0)
      {
        // Compute tile count from image size and tile size
        uint32_t xsiz =
            (header[i + 6] << 24) | (header[i + 7] << 16) | (header[i + 8] << 8) | header[i + 9];
        uint32_t ysiz = (header[i + 10] << 24) | (header[i + 11] << 16) | (header[i + 12] << 8) |
                        header[i + 13];
        fa.tile_count = static_cast<uint16_t>(((xsiz + xt - 1) / xt) * ((ysiz + yt - 1) / yt));
      }
      break;
    }
  }

  // Look for COD marker (0xFF52) for decomposition levels and quality layers
  for(size_t i = 0; i + 1 < static_cast<size_t>(bytes_read); ++i)
  {
    if(header[i] == 0xFF && header[i + 1] == 0x52 && i + 10 < static_cast<size_t>(bytes_read))
    {
      // COD marker: Lsiz at +2/+3, then coding style params
      // Number of decomposition levels at offset +7 from marker start
      fa.decomposition_levels = header[i + 7];
      // Number of quality layers at offset +4/+5 (big-endian 16-bit)
      fa.quality_layers = (header[i + 4] << 8) | header[i + 5];
      break;
    }
  }
}

AnalyticsResult compute_analytics(const AnalyticsOptions& opts)
{
  AnalyticsResult result;

  auto files = list_j2k_files(opts.source);
  if(files.empty())
  {
    result.error = "No J2K files found in " + opts.source.string();
    return result;
  }

  double fps = static_cast<double>(opts.fps_num) / opts.fps_den;
  result.total_frames = static_cast<uint32_t>(files.size());
  result.duration_seconds = result.total_frames / fps;

  double sum = 0;
  double sum_sq = 0;
  result.peak_bitrate_mbps = 0;
  result.min_bitrate_mbps = 1e9;

  for(uint32_t i = 0; i < result.total_frames; ++i)
  {
    FrameAnalytics fa;
    fa.frame_number = i;
    fa.size_bytes = std::filesystem::file_size(files[i]);
    fa.bitrate_mbps = (fa.size_bytes * 8.0 * fps) / 1'000'000.0;

    if(opts.parse_headers)
      parse_j2k_header(files[i], fa);

    result.total_bytes += fa.size_bytes;
    sum += fa.bitrate_mbps;
    sum_sq += fa.bitrate_mbps * fa.bitrate_mbps;
    result.peak_bitrate_mbps = std::max(result.peak_bitrate_mbps, fa.bitrate_mbps);
    result.min_bitrate_mbps = std::min(result.min_bitrate_mbps, fa.bitrate_mbps);

    result.frames.push_back(fa);
  }

  result.avg_bitrate_mbps = sum / result.total_frames;
  double variance =
      (sum_sq / result.total_frames) - (result.avg_bitrate_mbps * result.avg_bitrate_mbps);
  result.stddev_bitrate_mbps = variance > 0 ? std::sqrt(variance) : 0;

  // Build histogram (10 buckets)
  result.histogram_min = result.min_bitrate_mbps;
  result.histogram_max = result.peak_bitrate_mbps;
  result.bitrate_histogram.resize(10, 0);
  double bucket_width = (result.histogram_max - result.histogram_min) / 10.0;
  if(bucket_width > 0)
  {
    for(auto& fa : result.frames)
    {
      int bucket = static_cast<int>((fa.bitrate_mbps - result.histogram_min) / bucket_width);
      if(bucket >= 10)
        bucket = 9;
      if(bucket < 0)
        bucket = 0;
      result.bitrate_histogram[bucket]++;
    }
  }

  // Per-second averages
  uint32_t frames_per_sec = static_cast<uint32_t>(fps);
  if(frames_per_sec == 0)
    frames_per_sec = 1;
  for(uint32_t s = 0; s * frames_per_sec < result.total_frames; ++s)
  {
    double sec_sum = 0;
    uint32_t count = 0;
    for(uint32_t f = s * frames_per_sec;
        f < std::min((s + 1) * frames_per_sec, result.total_frames); ++f)
    {
      sec_sum += result.frames[f].bitrate_mbps;
      ++count;
    }
    result.per_second_bitrate.push_back(count > 0 ? sec_sum / count : 0);
  }

  result.success = true;
  return result;
}

std::string analytics_to_json(const AnalyticsResult& result)
{
  std::ostringstream ss;
  ss << "{\n";
  ss << "  \"total_frames\": " << result.total_frames << ",\n";
  ss << "  \"duration_seconds\": " << result.duration_seconds << ",\n";
  ss << "  \"total_bytes\": " << result.total_bytes << ",\n";
  ss << "  \"avg_bitrate_mbps\": " << result.avg_bitrate_mbps << ",\n";
  ss << "  \"peak_bitrate_mbps\": " << result.peak_bitrate_mbps << ",\n";
  ss << "  \"min_bitrate_mbps\": " << result.min_bitrate_mbps << ",\n";
  ss << "  \"stddev_bitrate_mbps\": " << result.stddev_bitrate_mbps << ",\n";

  ss << "  \"histogram\": [";
  for(size_t i = 0; i < result.bitrate_histogram.size(); ++i)
  {
    if(i > 0)
      ss << ", ";
    ss << result.bitrate_histogram[i];
  }
  ss << "],\n";
  ss << "  \"histogram_min\": " << result.histogram_min << ",\n";
  ss << "  \"histogram_max\": " << result.histogram_max << ",\n";

  ss << "  \"per_second_bitrate\": [";
  for(size_t i = 0; i < result.per_second_bitrate.size(); ++i)
  {
    if(i > 0)
      ss << ", ";
    ss << result.per_second_bitrate[i];
  }
  ss << "],\n";

  // Frame data (sample every Nth frame if too many)
  uint32_t step = 1;
  if(result.total_frames > 1000)
    step = result.total_frames / 1000;

  ss << "  \"frame_bitrates\": [";
  bool first = true;
  for(uint32_t i = 0; i < result.total_frames; i += step)
  {
    if(!first)
      ss << ", ";
    ss << result.frames[i].bitrate_mbps;
    first = false;
  }
  ss << "]\n";
  ss << "}\n";
  return ss.str();
}

} // namespace imfwizard
