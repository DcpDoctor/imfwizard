#include "imfwizard/slate.h"
#include "imfwizard/portable.h"
#include <spdlog/spdlog.h>

#include <cstdio>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace imfwizard
{

SlateResult generate_slate(const SlateOptions& opts)
{
  SlateResult result;
  fs::create_directories(opts.output_dir);

  uint32_t duration = opts.duration_frames;
  if(duration == 0)
  {
    switch(opts.type)
    {
    case SlateType::Countdown: duration = 10 * opts.fps_num / opts.fps_den; break;
    case SlateType::AcademyLeader: duration = 12 * opts.fps_num / opts.fps_den; break;
    case SlateType::ColorBars: duration = 2 * opts.fps_num / opts.fps_den; break;
    case SlateType::Slate: duration = 2 * opts.fps_num / opts.fps_den; break;
    case SlateType::Black: duration = opts.fps_num / opts.fps_den; break;
    }
  }

  double fps = static_cast<double>(opts.fps_num) / static_cast<double>(opts.fps_den);
  double duration_sec = static_cast<double>(duration) / fps;

  std::string filter;
  switch(opts.type)
  {
  case SlateType::Countdown:
    // Generate countdown numbers using drawtext
    filter = "color=black:s=" + std::to_string(opts.width) + "x" + std::to_string(opts.height) +
             ":d=" + std::to_string(duration_sec) +
             ",drawtext=text='%{eif\\:10-t\\:d}':fontsize=" + std::to_string(opts.height / 3) +
             ":fontcolor=white:x=(w-text_w)/2:y=(h-text_h)/2";
    break;

  case SlateType::AcademyLeader:
    // SMPTE universal leader approximation
    filter = "color=black:s=" + std::to_string(opts.width) + "x" + std::to_string(opts.height) +
             ":d=" + std::to_string(duration_sec) +
             ",drawtext=text='SMPTE UNIVERSAL LEADER':fontsize=" +
             std::to_string(opts.height / 10) +
             ":fontcolor=white:x=(w-text_w)/2:y=(h-text_h)/2";
    break;

  case SlateType::ColorBars:
    // SMPTE color bars
    filter = "smptebars=s=" + std::to_string(opts.width) + "x" + std::to_string(opts.height) +
             ":d=" + std::to_string(duration_sec);
    break;

  case SlateType::Slate:
  {
    // Text slate
    std::string text = opts.title;
    if(!opts.date.empty())
      text += "\\n" + opts.date;
    if(!opts.reel_info.empty())
      text += "\\n" + opts.reel_info;
    if(!opts.client.empty())
      text += "\\n" + opts.client;

    filter = "color=black:s=" + std::to_string(opts.width) + "x" + std::to_string(opts.height) +
             ":d=" + std::to_string(duration_sec) + ",drawtext=text='" + text + "':fontsize=" +
             std::to_string(opts.height / 15) +
             ":fontcolor=white:x=(w-text_w)/2:y=(h-text_h)/2";
    break;
  }

  case SlateType::Black:
    filter = "color=black:s=" + std::to_string(opts.width) + "x" + std::to_string(opts.height) +
             ":d=" + std::to_string(duration_sec);
    break;
  }

  // Generate image sequence
  std::string cmd = "ffmpeg -y -f lavfi -i \"" + filter + "\" -r " + std::to_string(fps) +
                    " -c:v tiff -pix_fmt rgb48le \"" +
                    (opts.output_dir / "frame_%06d.tiff").string() + "\" 2>/dev/null";

  int ret = system(cmd.c_str());
  if(ret != 0)
  {
    result.error = "ffmpeg slate generation failed";
    return result;
  }

  // Generate reference tone if requested
  if(opts.generate_tone && !opts.tone_output.empty())
  {
    std::string tone_cmd = "ffmpeg -y -f lavfi -i \"sine=frequency=1000:sample_rate=48000:d=" +
                           std::to_string(duration_sec) + "\" -c:a pcm_s24le \"" +
                           opts.tone_output.string() + "\" 2>/dev/null";
    system(tone_cmd.c_str());
    result.tone_file = opts.tone_output;
  }

  result.output_dir = opts.output_dir;
  result.frames_generated = duration;
  result.success = true;
  return result;
}

} // namespace imfwizard
