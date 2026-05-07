#include <spdlog/spdlog.h>
#include <cstdio>
#include <cstring>

#include "imfwizard/burnin.h"
#include "imfwizard/portable.h"

namespace imfwizard
{

static bool has_ffmpeg()
{
#ifdef _WIN32
  return system("where ffmpeg >NUL 2>&1") == 0;
#else
  return system("which ffmpeg >/dev/null 2>&1") == 0;
#endif
}

bool has_subtitle_filter()
{
  if(!has_ffmpeg())
    return false;
  // Check if subtitles filter is available
  std::string cmd = "ffmpeg -filters 2>/dev/null | grep -q subtitles";
#ifdef _WIN32
  cmd = "ffmpeg -filters 2>NUL | findstr subtitles >NUL 2>&1";
#endif
  return system(cmd.c_str()) == 0;
}

BurnInResult burn_in_subtitles(const BurnInOptions& opts)
{
  BurnInResult result;

  if(!has_ffmpeg())
  {
    result.error = "ffmpeg not found";
    return result;
  }

  if(!std::filesystem::exists(opts.video_input))
  {
    result.error = "Video input not found: " + opts.video_input.string();
    return result;
  }

  if(!std::filesystem::exists(opts.subtitle_file))
  {
    result.error = "Subtitle file not found: " + opts.subtitle_file.string();
    return result;
  }

  auto output = opts.output;
  if(output.empty())
  {
    output = opts.video_input.parent_path() /
             (opts.video_input.stem().string() + "_burned" + opts.video_input.extension().string());
  }

  // Determine subtitle filter based on file extension
  auto ext = opts.subtitle_file.extension().string();
  std::string filter;

  if(ext == ".srt" || ext == ".ass" || ext == ".ssa")
  {
    // Use subtitles filter (libass)
    filter = "subtitles='" + opts.subtitle_file.string() + "'";
    filter += ":force_style='FontName=" + opts.font;
    filter += ",FontSize=" + std::to_string(opts.font_size);
    filter += ",PrimaryColour=&H00FFFFFF";
    filter += ",OutlineColour=&H00000000";
    filter += ",Outline=" + std::to_string(opts.outline_width);
    filter += ",MarginV=" + std::to_string(opts.margin_bottom) + "'";
  }
  else if(ext == ".ttml" || ext == ".xml")
  {
    // TTML needs to be converted to ASS first, or use drawtext with parsed cues
    // For simplicity, try the subtitles filter which ffmpeg may support for TTML
    filter = "subtitles='" + opts.subtitle_file.string() + "'";
  }
  else if(ext == ".scc")
  {
    filter = "subtitles='" + opts.subtitle_file.string() + "'";
  }
  else
  {
    result.error = "Unsupported subtitle format: " + ext;
    return result;
  }

  // Build ffmpeg command
  std::string cmd = "ffmpeg -y -i \"" + opts.video_input.string() + "\"";
  if(opts.threads > 0)
    cmd += " -threads " + std::to_string(opts.threads);
  cmd += " -vf \"" + filter + "\"";
  cmd += " -c:a copy";
  cmd += " \"" + output.string() + "\" 2>/dev/null";

  spdlog::debug("burn-in cmd: {}", cmd);

  if(system(cmd.c_str()) != 0)
  {
    result.error = "FFmpeg subtitle burn-in failed";
    return result;
  }

  // Get frame count from output
  std::string count_cmd = "ffprobe -v error -count_frames -select_streams v:0 "
                          "-show_entries stream=nb_read_frames -of csv=p=0 \"" +
                          output.string() + "\" 2>/dev/null";
  FILE* pipe = portable_popen(count_cmd.c_str(), "r");
  if(pipe)
  {
    char buf[64];
    std::string fc;
    while(fgets(buf, sizeof(buf), pipe))
      fc += buf;
    portable_pclose(pipe);
    if(!fc.empty())
      result.frame_count = static_cast<uint32_t>(std::stoul(fc));
  }

  result.output_file = output;
  result.success = true;
  return result;
}

} // namespace imfwizard
