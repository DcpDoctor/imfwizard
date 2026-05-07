#include "imfwizard/audio_desc.h"
#include "imfwizard/portable.h"
#include <spdlog/spdlog.h>

#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

namespace imfwizard
{

AudioDescResult create_audio_description(const AudioDescOptions& opts)
{
  AudioDescResult result;

  if(!fs::exists(opts.main_audio))
  {
    result.error = "Main audio file not found: " + opts.main_audio.string();
    return result;
  }
  if(!fs::exists(opts.ad_audio))
  {
    result.error = "Audio description file not found: " + opts.ad_audio.string();
    return result;
  }

  // Use ffmpeg to mix AD track with ducked main audio
  // The AD track replaces the center channel with a ducked mix + narration
  std::string cmd = "ffmpeg -y -i \"" + opts.main_audio.string() + "\" -i \"" +
                    opts.ad_audio.string() + "\" -filter_complex "
                    "\"[0:a]volume=" +
                    std::to_string(std::pow(10.0, opts.ad_mix_level / 20.0)) +
                    "[ducked];"
                    "[ducked][1:a]amix=inputs=2:duration=first[out]\" "
                    "-map \"[out]\" -c:a pcm_s" +
                    std::to_string(opts.bit_depth) + "le -ar " +
                    std::to_string(opts.sample_rate) + " \"" + opts.output_file.string() +
                    "\" 2>/dev/null";

  int ret = system(cmd.c_str());
  if(ret != 0)
  {
    result.error = "ffmpeg audio description mix failed";
    return result;
  }

  result.output_file = opts.output_file;

  // Create MCA soundfield with AD label
  result.soundfield = soundfield_51();
  result.soundfield.name = "51+VI";
  McaLabel vi_label;
  vi_label.symbol = McaTagSymbol::VI;
  vi_label.tag_name = "Visually Impaired";
  vi_label.tag_symbol = "chVIN";
  vi_label.channel_index = 6;
  vi_label.spoken_language = opts.language;
  result.soundfield.channels.push_back(vi_label);
  result.total_channels = 7;

  result.success = true;
  return result;
}

} // namespace imfwizard
