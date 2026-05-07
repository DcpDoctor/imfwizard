#include "imfwizard/conform.h"
#include "imfwizard/imp.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <regex>

namespace imfwizard
{

// Parse CMX3600 EDL timecode to frame number
static uint32_t tc_to_frames(const std::string& tc, uint32_t fps)
{
  // Format: HH:MM:SS:FF
  uint32_t h = 0, m = 0, s = 0, f = 0;
  if(sscanf(tc.c_str(), "%u:%u:%u:%u", &h, &m, &s, &f) == 4)
    return ((h * 3600 + m * 60 + s) * fps) + f;
  return 0;
}

std::vector<EdlEntry> parse_edl(const std::filesystem::path& edl_file)
{
  std::vector<EdlEntry> entries;

  std::ifstream f(edl_file);
  if(!f)
  {
    spdlog::error("Cannot open EDL file: {}", edl_file.string());
    return entries;
  }

  // Simple CMX3600 parser
  // Format: NNN  REEL  TRACK  CUT  SRC_IN SRC_OUT REC_IN REC_OUT
  std::string line;
  std::regex event_re(
      R"(^\s*(\d+)\s+(\S+)\s+([VAB]\d*)\s+\w+\s+(\d{2}:\d{2}:\d{2}:\d{2})\s+(\d{2}:\d{2}:\d{2}:\d{2})\s+(\d{2}:\d{2}:\d{2}:\d{2})\s+(\d{2}:\d{2}:\d{2}:\d{2}))");

  uint32_t fps = 24; // Default — could be parsed from header

  while(std::getline(f, line))
  {
    // Check for FCM line
    if(line.find("FCM:") != std::string::npos)
    {
      if(line.find("NON-DROP") != std::string::npos)
        fps = 24;
      else if(line.find("DROP") != std::string::npos)
        fps = 30; // Simplified
      continue;
    }

    std::smatch match;
    if(std::regex_search(line, match, event_re))
    {
      EdlEntry entry;
      entry.reel_name = match[2].str();
      entry.track_type = match[3].str();
      entry.source_in = tc_to_frames(match[4].str(), fps);
      entry.source_out = tc_to_frames(match[5].str(), fps);
      entry.record_in = tc_to_frames(match[6].str(), fps);
      entry.record_out = tc_to_frames(match[7].str(), fps);
      entries.push_back(entry);
    }
  }

  spdlog::info("Parsed {} EDL events from {}", entries.size(), edl_file.string());
  return entries;
}

std::vector<EdlEntry> parse_aaf(const std::filesystem::path& aaf_file)
{
  // AAF parsing requires a dedicated library (e.g. OpenAAF)
  // For now, return empty with a warning
  spdlog::warn("AAF parsing not yet implemented — use EDL format");
  (void)aaf_file;
  return {};
}

ConformResult conform_from_edl(const ConformOptions& opts)
{
  ConformResult result;
  namespace fs = std::filesystem;

  auto ext = opts.edl_file.extension().string();
  if(ext == ".aaf" || ext == ".AAF")
  {
    result.entries = parse_aaf(opts.edl_file);
  }
  else
  {
    result.entries = parse_edl(opts.edl_file);
  }

  if(result.entries.empty())
  {
    result.error = "No edit events found in " + opts.edl_file.string();
    return result;
  }

  // Calculate total duration from record timeline
  uint32_t max_out = 0;
  for(const auto& e : result.entries)
  {
    if(e.record_out > max_out)
      max_out = e.record_out;
  }
  result.total_duration = max_out;

  // For now, create a simple IMP from the first video reel found
  // A full implementation would assemble segments from multiple reels
  for(const auto& entry : result.entries)
  {
    if(entry.track_type[0] != 'V')
      continue;

    // Look for reel directory in media_dir
    auto reel_dir = opts.media_dir / entry.reel_name;
    if(!fs::exists(reel_dir))
    {
      spdlog::warn("Reel not found: {}", reel_dir.string());
      continue;
    }

    ImpOptions imp_opts;
    imp_opts.title = opts.title;
    imp_opts.video_dir = reel_dir;
    imp_opts.output_dir = opts.output_dir;
    imp_opts.edit_rate_num = opts.fps_num;
    imp_opts.edit_rate_den = opts.fps_den;

    auto imp_result = create_ov_imp(imp_opts);
    if(imp_result.success)
    {
      result.success = true;
      spdlog::info("Conformed IMP created: {}", imp_result.output_dir.string());
    }
    else
    {
      result.error = imp_result.error;
    }
    break; // First video reel for now
  }

  if(!result.success && result.error.empty())
    result.error = "No suitable video reel found in media directory";

  return result;
}

} // namespace imfwizard
