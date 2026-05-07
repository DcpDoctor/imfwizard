#include <spdlog/spdlog.h>
#include <fstream>
#include <cstdio>

#include "imfwizard/extract.h"
#include "imfwizard/info.h"

namespace imfwizard
{

ExtractResult extract_from_imp(const ExtractOptions& opts)
{
  ExtractResult result;
  namespace fs = std::filesystem;

  if(!fs::exists(opts.imp_dir))
  {
    result.error = "IMP directory not found: " + opts.imp_dir.string();
    return result;
  }

  fs::create_directories(opts.output_dir);

  // Read IMP info to find track files
  auto info = read_imp_info(opts.imp_dir);
  if(!info.valid)
  {
    result.error = "Cannot read IMP: " + info.error;
    return result;
  }

  for(const auto& track : info.tracks)
  {
    // Filter by UUID if specified
    if(!opts.track_uuids.empty())
    {
      bool found = false;
      for(const auto& uuid : opts.track_uuids)
      {
        if(track.uuid == uuid)
        {
          found = true;
          break;
        }
      }
      if(!found)
        continue;
    }

    auto mxf_path = opts.imp_dir / track.filename;
    if(!fs::exists(mxf_path))
      continue;

    if(track.type == "video")
    {
      // Use ffmpeg to extract frames from MXF
      auto video_out = opts.output_dir / "video";
      fs::create_directories(video_out);

      std::string cmd = "ffmpeg -y -i " + mxf_path.string();
      if(opts.start_frame > 0)
        cmd += " -ss " + std::to_string(opts.start_frame);

      auto pattern = video_out / ("frame_%06d." + opts.output_format);
      cmd += " " + pattern.string() + " 2>/dev/null";

      int ret = system(cmd.c_str());
      if(ret != 0)
      {
        spdlog::warn("ffmpeg extract failed for {} (code {})", track.filename, ret);
        continue;
      }

      // Count extracted frames
      for(const auto& f : fs::directory_iterator(video_out))
      {
        if(f.is_regular_file())
        {
          result.extracted_files.push_back(f.path());
          ++result.video_frames;
        }
      }

      spdlog::info("Extracted {} video frames from {}", result.video_frames, track.filename);
    }
    else if(track.type == "audio")
    {
      // Extract audio to WAV via ffmpeg
      auto audio_out = opts.output_dir / (track.filename + ".wav");

      std::string cmd =
          "ffmpeg -y -i " + mxf_path.string() + " " + audio_out.string() + " 2>/dev/null";
      int ret = system(cmd.c_str());
      if(ret != 0)
      {
        spdlog::warn("ffmpeg audio extract failed for {}", track.filename);
        continue;
      }

      result.extracted_files.push_back(audio_out);
      spdlog::info("Extracted audio from {}", track.filename);
    }
  }

  result.output_dir = opts.output_dir;
  result.success = !result.extracted_files.empty();
  if(!result.success)
    result.error = "No tracks extracted (ffmpeg required)";
  return result;
}

} // namespace imfwizard
