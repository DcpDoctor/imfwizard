#include "imfwizard/atmos.h"
#include "imfwizard/portable.h"
#include <spdlog/spdlog.h>

#include <cstdio>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace imfwizard
{

bool atmos_tools_available()
{
#ifdef _WIN32
  return system("where ffmpeg >NUL 2>&1") == 0;
#else
  return system("which ffmpeg >/dev/null 2>&1") == 0;
#endif
}

AtmosImportResult import_atmos(const AtmosImportOptions& opts)
{
  AtmosImportResult result;

  if(!fs::exists(opts.input_file))
  {
    result.error = "Input file not found: " + opts.input_file.string();
    return result;
  }

  if(!atmos_tools_available())
  {
    result.error = "ffmpeg not found — required for ADM/Atmos import";
    return result;
  }

  fs::create_directories(opts.output_dir);

  // Step 1: Extract channel info from ADM BWF using ffprobe
  std::string probe_cmd =
      "ffprobe -v quiet -print_format json -show_streams \"" + opts.input_file.string() + "\"";
  FILE* pipe = portable_popen(probe_cmd.c_str(), "r");
  if(!pipe)
  {
    result.error = "Failed to run ffprobe";
    return result;
  }

  std::string probe_output;
  char buf[4096];
  while(fgets(buf, sizeof(buf), pipe))
    probe_output += buf;
  portable_pclose(pipe);

  // Parse channel count from probe output
  auto ch_pos = probe_output.find("\"channels\"");
  uint32_t channels = 0;
  if(ch_pos != std::string::npos)
  {
    auto colon = probe_output.find(':', ch_pos);
    channels = static_cast<uint32_t>(std::stoul(probe_output.substr(colon + 1)));
  }

  if(channels == 0)
  {
    result.error = "Cannot determine channel count from ADM BWF";
    return result;
  }

  // Step 2: Wrap ADM BWF channels into AS-02 MXF (IAB track file)
  auto output_mxf = opts.output_dir / "iab_track.mxf";
  std::string wrap_cmd = "ffmpeg -y -i \"" + opts.input_file.string() + "\" -c:a pcm_s" +
                         std::to_string(opts.bit_depth) + "le -ar " +
                         std::to_string(opts.sample_rate) + " -f mxf \"" + output_mxf.string() +
                         "\" 2>/dev/null";
  int ret = system(wrap_cmd.c_str());
  if(ret != 0)
  {
    result.error = "ffmpeg MXF wrap failed";
    return result;
  }

  result.iab_mxf = output_mxf;
  result.bed_channel_count = std::min(channels, 10u); // beds typically ≤10
  result.object_count = (channels > 10) ? channels - 10 : 0;

  // Set standard bed channel labels
  static const char* labels[] = {"L", "R", "C", "LFE", "Ls", "Rs", "Lss", "Rss", "Lrs", "Rrs"};
  for(uint32_t i = 0; i < result.bed_channel_count; i++)
  {
    AtmosBedChannel ch;
    ch.label = labels[i];
    ch.channel_index = i;
    result.bed_channels.push_back(ch);
  }

  result.success = true;
  return result;
}

} // namespace imfwizard
