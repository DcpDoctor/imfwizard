#include "imfwizard/mxf_unwrap.h"
#include "imfwizard/portable.h"
#include <spdlog/spdlog.h>
#include <array>
#include <cstdio>
#include <regex>
#include <sstream>

namespace imfwizard
{

static std::string exec_probe(const std::string& cmd)
{
  std::array<char, 4096> buffer;
  std::string result;
  FILE* pipe = portable_popen(cmd.c_str(), "r");
  if(!pipe)
    return {};
  while(fgets(buffer.data(), buffer.size(), pipe))
    result += buffer.data();
  portable_pclose(pipe);
  return result;
}

MxfProbeResult probe_mxf_detailed(const std::filesystem::path& mxf_path)
{
  MxfProbeResult result;

  if(!std::filesystem::exists(mxf_path))
  {
    result.error = "MXF file not found: " + mxf_path.string();
    return result;
  }

  result.file_size = std::filesystem::file_size(mxf_path);

  // Use ffprobe to get track information
  std::ostringstream cmd;
  cmd << "ffprobe -v error -show_streams -show_format -of json \""
      << mxf_path.string() << "\" 2>&1";

  auto output = exec_probe(cmd.str());
  if(output.empty())
  {
    result.error = "ffprobe failed on " + mxf_path.string();
    return result;
  }

  // Parse streams from ffprobe JSON output
  std::regex stream_re(R"re("codec_type"\s*:\s*"(\w+)")re");
  std::regex codec_re(R"re("codec_name"\s*:\s*"(\w+)")re");
  std::regex dur_re(R"re("nb_frames"\s*:\s*"(\d+)")re");
  std::regex width_re(R"re("width"\s*:\s*(\d+))re");
  std::regex height_re(R"re("height"\s*:\s*(\d+))re");
  std::regex sr_re(R"re("sample_rate"\s*:\s*"(\d+)")re");
  std::regex ch_re(R"re("channels"\s*:\s*(\d+))re");
  std::regex rate_re(R"re("r_frame_rate"\s*:\s*"(\d+)/(\d+)")re");
  std::regex bits_re(R"re("bits_per_raw_sample"\s*:\s*"(\d+)")re");

  // Split by stream sections
  std::regex stream_sect(R"re(\{[^{}]*"codec_type"[^{}]*\})re");
  std::sregex_iterator it(output.begin(), output.end(), stream_sect);
  std::sregex_iterator end_it;

  for(; it != end_it; ++it)
  {
    std::string section = (*it)[0].str();
    MxfTrackInfo track;

    std::smatch m;
    if(std::regex_search(section, m, stream_re))
      track.essence_type = m[1].str();
    if(std::regex_search(section, m, codec_re))
      track.codec = m[1].str();
    if(std::regex_search(section, m, dur_re))
      track.duration = static_cast<uint32_t>(std::stoul(m[1].str()));
    if(std::regex_search(section, m, width_re))
      track.width = static_cast<uint32_t>(std::stoul(m[1].str()));
    if(std::regex_search(section, m, height_re))
      track.height = static_cast<uint32_t>(std::stoul(m[1].str()));
    if(std::regex_search(section, m, sr_re))
      track.sample_rate = static_cast<uint32_t>(std::stoul(m[1].str()));
    if(std::regex_search(section, m, ch_re))
      track.channels = static_cast<uint16_t>(std::stoul(m[1].str()));
    if(std::regex_search(section, m, rate_re))
    {
      track.edit_rate_num = static_cast<uint32_t>(std::stoul(m[1].str()));
      track.edit_rate_den = static_cast<uint32_t>(std::stoul(m[2].str()));
    }
    if(std::regex_search(section, m, bits_re))
      track.bit_depth = static_cast<uint16_t>(std::stoul(m[1].str()));

    result.tracks.push_back(track);
  }

  // Detect container type from format
  if(output.find("mxf_opatom") != std::string::npos)
    result.container_label = "OP-Atom";
  else if(output.find("mxf_op1a") != std::string::npos)
    result.container_label = "OP1a";
  else
    result.container_label = "MXF";

  result.success = true;
  spdlog::info("MXF probe: {} tracks in {} ({} bytes)",
               result.tracks.size(), result.container_label, result.file_size);
  return result;
}

MxfUnwrapResult unwrap_mxf(const MxfUnwrapOptions& opts)
{
  MxfUnwrapResult result;

  if(!std::filesystem::exists(opts.input))
  {
    result.error = "MXF file not found: " + opts.input.string();
    return result;
  }

  std::filesystem::create_directories(opts.output_dir);

  // Probe first to determine track types
  auto probe = probe_mxf_detailed(opts.input);
  if(!probe.success)
  {
    result.error = probe.error;
    return result;
  }

  for(const auto& track : probe.tracks)
  {
    if(track.essence_type == "video" && !opts.extract_video)
      continue;
    if(track.essence_type == "audio" && !opts.extract_audio)
      continue;

    if(track.essence_type == "video")
    {
      // Extract video as image sequence or raw bitstream
      std::ostringstream cmd;
      auto out_pattern = opts.output_dir / "frame_%06d.j2c";

      if(track.codec == "jpeg2000" || track.codec == "j2k")
      {
        // Extract J2K codestreams directly
        cmd << "ffmpeg -y -i \"" << opts.input.string() << "\" "
            << "-map 0:v:0 -c:v copy -f image2 \""
            << out_pattern.string() << "\" 2>&1";
      }
      else
      {
        // Decode to PNG
        out_pattern = opts.output_dir / "frame_%06d.png";
        cmd << "ffmpeg -y -i \"" << opts.input.string() << "\" "
            << "-map 0:v:0 \""
            << out_pattern.string() << "\" 2>&1";
      }

      if(opts.start_frame > 0 || opts.end_frame > 0)
      {
        // Add trim filter
        double fps = static_cast<double>(track.edit_rate_num) /
                     (track.edit_rate_den > 0 ? track.edit_rate_den : 1);
        std::ostringstream cmd2;
        cmd2 << "ffmpeg -y ";
        if(opts.start_frame > 0)
          cmd2 << "-ss " << (opts.start_frame / fps) << " ";
        cmd2 << "-i \"" << opts.input.string() << "\" ";
        if(opts.end_frame > 0)
          cmd2 << "-frames:v " << (opts.end_frame - opts.start_frame) << " ";
        cmd2 << "-map 0:v:0 ";
        if(track.codec == "jpeg2000" || track.codec == "j2k")
          cmd2 << "-c:v copy -f image2 ";
        cmd2 << "\"" << out_pattern.string() << "\" 2>&1";
        cmd.str(cmd2.str());
      }

      spdlog::info("Extracting video track...");
      auto output = exec_probe(cmd.str());

      // Count extracted files
      for(const auto& entry : std::filesystem::directory_iterator(opts.output_dir))
      {
        auto ext = entry.path().extension().string();
        if(ext == ".j2c" || ext == ".j2k" || ext == ".png")
        {
          result.extracted_files.push_back(entry.path());
          ++result.frames_extracted;
        }
      }
    }
    else if(track.essence_type == "audio")
    {
      // Extract audio as WAV
      auto out_wav = opts.output_dir / "audio.wav";
      std::ostringstream cmd;
      cmd << "ffmpeg -y -i \"" << opts.input.string() << "\" "
          << "-map 0:a:0 -c:a pcm_s" << (track.bit_depth > 0 ? track.bit_depth : 24) << "le \""
          << out_wav.string() << "\" 2>&1";

      spdlog::info("Extracting audio track...");
      exec_probe(cmd.str());

      if(std::filesystem::exists(out_wav))
        result.extracted_files.push_back(out_wav);
    }
  }

  result.success = !result.extracted_files.empty();
  if(result.success)
  {
    spdlog::info("MXF unwrap: extracted {} files ({} video frames)",
                 result.extracted_files.size(), result.frames_extracted);
  }
  else
  {
    result.error = "No essence could be extracted";
  }
  return result;
}

} // namespace imfwizard
