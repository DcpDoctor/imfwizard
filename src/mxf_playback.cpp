#include "imfwizard/mxf_playback.h"
#include "imfwizard/portable.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

namespace imfwizard
{

namespace
{

// Run a command and capture stdout
std::string exec_capture(const std::string& cmd)
{
  std::array<char, 4096> buffer;
  std::string output;
#ifdef _WIN32
  FILE* pipe = _popen(cmd.c_str(), "r");
#else
  FILE* pipe = popen(cmd.c_str(), "r");
#endif
  if (!pipe)
    return "";

  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    output += buffer.data();

#ifdef _WIN32
  _pclose(pipe);
#else
  pclose(pipe);
#endif
  return output;
}

} // anonymous namespace

MxfPlaybackResult probe_mxf(const std::filesystem::path& mxf_file)
{
  MxfPlaybackResult result;

  if (!std::filesystem::exists(mxf_file))
  {
    result.error = "MXF file not found: " + mxf_file.string();
    return result;
  }

  // Use ffprobe to get MXF info
  std::string cmd = "ffprobe -v quiet -print_format json -show_streams -show_format "
                    "\"" + mxf_file.string() + "\" 2>/dev/null";
  std::string output = exec_capture(cmd);

  if (output.empty())
  {
    // Try GStreamer discoverer
    cmd = "gst-discoverer-1.0 \"" + mxf_file.string() + "\" 2>/dev/null";
    output = exec_capture(cmd);

    if (output.empty())
    {
      result.error = "Could not probe MXF file (ffprobe and gst-discoverer-1.0 not available)";
      return result;
    }

    // Parse GStreamer discoverer output
    std::regex width_re(R"(width:\s*(\d+))");
    std::regex height_re(R"(height:\s*(\d+))");
    std::regex fps_re(R"(framerate:\s*(\d+)/(\d+))");
    std::regex codec_re(R"(video/x-([a-z0-9]+))");

    std::smatch m;
    if (std::regex_search(output, m, width_re))
      result.width = std::stoi(m[1].str());
    if (std::regex_search(output, m, height_re))
      result.height = std::stoi(m[1].str());
    if (std::regex_search(output, m, fps_re))
      result.fps = std::stod(m[1].str()) / std::stod(m[2].str());
    if (std::regex_search(output, m, codec_re))
      result.codec = m[1].str();
  }
  else
  {
    // Parse ffprobe JSON output
    std::regex ff_width_re(R"RE("width"\s*:\s*(\d+))RE");
    std::regex ff_height_re(R"RE("height"\s*:\s*(\d+))RE");
    std::regex ff_fps_re(R"RE("r_frame_rate"\s*:\s*"(\d+)/(\d+)")RE");
    std::regex ff_codec_re(R"RE("codec_name"\s*:\s*"([^"]+)")RE");
    std::regex ff_frames_re(R"RE("nb_frames"\s*:\s*"(\d+)")RE");
    std::regex ff_duration_re(R"RE("duration"\s*:\s*"([\d.]+)")RE");

    std::smatch fm;
    if (std::regex_search(output, fm, ff_width_re))
      result.width = std::stoi(fm[1].str());
    if (std::regex_search(output, fm, ff_height_re))
      result.height = std::stoi(fm[1].str());
    if (std::regex_search(output, fm, ff_fps_re))
      result.fps = std::stod(fm[1].str()) / std::stod(fm[2].str());
    if (std::regex_search(output, fm, ff_codec_re))
      result.codec = fm[1].str();
    if (std::regex_search(output, fm, ff_frames_re))
      result.total_frames = std::stoi(fm[1].str());
    if (std::regex_search(output, fm, ff_duration_re))
      result.duration_seconds = std::stod(fm[1].str());
  }

  if (result.fps > 0 && result.total_frames == 0 && result.duration_seconds > 0)
    result.total_frames = static_cast<uint32_t>(result.duration_seconds * result.fps);

  result.success = true;
  return result;
}

PlaybackFrame extract_frame(const std::filesystem::path& mxf_file, uint32_t frame_number)
{
  PlaybackFrame frame;

  // Use ffmpeg to extract a single frame as raw RGB
  auto info = probe_mxf(mxf_file);
  if (!info.success)
    return frame;

  frame.width = info.width;
  frame.height = info.height;
  frame.frame_number = frame_number;

  double timestamp = (info.fps > 0) ? frame_number / info.fps : 0.0;

  // Extract frame as raw RGB via ffmpeg
  std::ostringstream cmd;
  cmd << "ffmpeg -v quiet -ss " << std::fixed << timestamp
      << " -i \"" << mxf_file.string() << "\""
      << " -frames:v 1 -f rawvideo -pix_fmt rgb24 pipe:1 2>/dev/null";

  std::string raw_cmd = cmd.str();
#ifdef _WIN32
  FILE* pipe = _popen(raw_cmd.c_str(), "rb");
#else
  FILE* pipe = popen(raw_cmd.c_str(), "r");
#endif
  if (!pipe)
    return frame;

  size_t expected_size = static_cast<size_t>(frame.width) * frame.height * 3;
  frame.rgb_data.resize(expected_size);

  size_t total_read = 0;
  while (total_read < expected_size)
  {
    size_t n = fread(frame.rgb_data.data() + total_read, 1, expected_size - total_read, pipe);
    if (n == 0)
      break;
    total_read += n;
  }

#ifdef _WIN32
  _pclose(pipe);
#else
  pclose(pipe);
#endif

  if (total_read < expected_size)
    frame.rgb_data.clear(); // Incomplete frame

  return frame;
}

MxfPlaybackResult generate_thumbnails(const MxfPlaybackOptions& opts)
{
  MxfPlaybackResult result;

  auto info = probe_mxf(opts.mxf_file);
  if (!info.success)
  {
    result.error = info.error;
    return result;
  }

  result.width = info.width;
  result.height = info.height;
  result.fps = info.fps;
  result.total_frames = info.total_frames;
  result.codec = info.codec;
  result.duration_seconds = info.duration_seconds;

  if (opts.thumbnail_dir.empty())
  {
    result.error = "Thumbnail directory not specified";
    return result;
  }

  std::filesystem::create_directories(opts.thumbnail_dir);

  uint32_t end = (opts.end_frame > 0) ? opts.end_frame : info.total_frames;
  uint32_t interval = opts.thumbnail_interval;

  for (uint32_t f = opts.start_frame; f < end; f += interval)
  {
    double timestamp = (info.fps > 0) ? f / info.fps : 0.0;
    auto thumb_path = opts.thumbnail_dir / ("thumb_" + std::to_string(f) + ".png");

    std::ostringstream cmd;
    cmd << "ffmpeg -v quiet -ss " << std::fixed << timestamp
        << " -i \"" << opts.mxf_file.string() << "\""
        << " -frames:v 1 -vf scale=320:-1"
        << " \"" << thumb_path.string() << "\" -y 2>/dev/null";

    int ret = std::system(cmd.str().c_str());
    if (ret == 0 && std::filesystem::exists(thumb_path))
      result.thumbnails.push_back(thumb_path);
  }

  result.success = true;
  return result;
}

int launch_playback(const MxfPlaybackOptions& opts)
{
  // Build GStreamer pipeline for MXF playback
  std::ostringstream pipeline;
  pipeline << "gst-launch-1.0 filesrc location=\"" << opts.mxf_file.string() << "\""
           << " ! mxfdemux name=demux"
           << " demux. ! queue ! decodebin ! videoconvert ! autovideosink"
           << " demux. ! queue ! decodebin ! audioconvert ! autoaudiosink"
           << " &";

  int ret = std::system(pipeline.str().c_str());
  return ret;
}

} // namespace imfwizard
