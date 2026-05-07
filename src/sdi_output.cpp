#include "imfwizard/sdi_output.h"
#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

namespace imfwizard
{

bool decklink_available()
{
  // Check if GStreamer decklinkvideosink element is available
  int ret = system("gst-inspect-1.0 decklinkvideosink >/dev/null 2>&1");
  return ret == 0;
}

std::vector<SdiDevice> list_sdi_devices()
{
  std::vector<SdiDevice> devices;

  // Use gst-device-monitor to list DeckLink devices
  FILE* fp = popen("gst-device-monitor-1.0 Video/Sink 2>/dev/null", "r");
  if(!fp)
    return devices;

  char buf[1024];
  std::string output;
  while(fgets(buf, sizeof(buf), fp))
    output += buf;
  pclose(fp);

  // Parse device names from gst-device-monitor output
  // Format: "Device found:\n  name  : DeckLink ...\n  class : Video/Sink"
  std::regex name_re(R"(name\s*:\s*(.+))");
  std::regex driver_re(R"(device-name\s*=\s*(.+))");
  auto it = std::sregex_iterator(output.begin(), output.end(), name_re);
  uint32_t idx = 0;
  for(; it != std::sregex_iterator(); ++it)
  {
    std::string name = (*it)[1].str();
    // Only include DeckLink devices
    if(name.find("DeckLink") != std::string::npos ||
       name.find("decklink") != std::string::npos)
    {
      SdiDevice dev;
      dev.index = idx;
      dev.name = name;
      dev.driver = "decklink";
      devices.push_back(dev);
      idx++;
    }
  }

  return devices;
}

SdiOutputResult sdi_preview(const SdiOutputOptions& opts)
{
  SdiOutputResult result;

  if(!decklink_available())
  {
    result.error = "GStreamer decklinkvideosink plugin not available. "
                   "Install gstreamer1.0-plugins-bad with DeckLink support.";
    return result;
  }

  if(!fs::exists(opts.input_dir))
  {
    result.error = "Input not found: " + opts.input_dir.string();
    return result;
  }

  double fps = static_cast<double>(opts.fps_num) / static_cast<double>(opts.fps_den);

  // Determine video mode string for DeckLink
  std::string mode;
  if(opts.width == 3840 && opts.height == 2160)
  {
    if(opts.fps_num == 24 && opts.fps_den == 1)
      mode = "2160p24";
    else if(opts.fps_num == 25 && opts.fps_den == 1)
      mode = "2160p25";
    else if(opts.fps_num == 30 && opts.fps_den == 1)
      mode = "2160p30";
    else
      mode = "2160p24";
  }
  else if(opts.width == 2048 && opts.height == 1080)
  {
    if(opts.fps_num == 24 && opts.fps_den == 1)
      mode = "1080p24";
    else if(opts.fps_num == 25 && opts.fps_den == 1)
      mode = "1080p25";
    else
      mode = "1080p24";
  }
  else
  {
    // Default HD modes
    if(opts.fps_num == 24 && opts.fps_den == 1)
      mode = "1080p24";
    else if(opts.fps_num == 25 && opts.fps_den == 1)
      mode = "1080p25";
    else if(opts.fps_num == 30 && opts.fps_den == 1)
      mode = "1080p30";
    else if(opts.fps_num == 50 && opts.fps_den == 1)
      mode = "1080p50";
    else if(opts.fps_num == 60 && opts.fps_den == 1)
      mode = "1080p60";
    else if(opts.fps_num == 24000 && opts.fps_den == 1001)
      mode = "1080p2398";
    else if(opts.fps_num == 30000 && opts.fps_den == 1001)
      mode = "1080p2997";
    else
      mode = "1080p24";
  }

  // Build GStreamer pipeline
  // Source: multifilesrc for J2K frames → openjpeg dec → videoconvert → decklinkvideosink
  std::ostringstream pipeline;

  // Determine source type
  auto ext = opts.input_dir.extension().string();
  if(fs::is_directory(opts.input_dir))
  {
    // Image sequence directory
    // Find first file to detect pattern
    std::string pattern;
    for(auto& entry : fs::directory_iterator(opts.input_dir))
    {
      auto fname = entry.path().filename().string();
      // Detect numbered pattern (frame_000001.j2c etc.)
      std::regex num_re(R"((.+?)(\d{4,})(\..+))");
      std::smatch m;
      if(std::regex_match(fname, m, num_re))
      {
        pattern = (opts.input_dir / (m[1].str() + "%0" +
                                     std::to_string(m[2].str().size()) + "d" + m[3].str()))
                      .string();
        break;
      }
    }
    if(pattern.empty())
    {
      result.error = "Cannot detect frame numbering pattern in " + opts.input_dir.string();
      return result;
    }

    pipeline << "multifilesrc location=\"" << pattern << "\" "
             << "index=" << opts.start_frame << " ";
    if(opts.end_frame > opts.start_frame)
      pipeline << "stop-index=" << opts.end_frame << " ";
    if(opts.loop)
      pipeline << "loop=true ";

    // Detect codec from extension
    auto frame_ext = fs::path(pattern).extension().string();
    if(frame_ext == ".j2c" || frame_ext == ".j2k" || frame_ext == ".jp2")
      pipeline << "caps=\"image/x-j2c,framerate=" << opts.fps_num << "/" << opts.fps_den << "\" "
               << "! openjpegdec ";
    else if(frame_ext == ".tiff" || frame_ext == ".tif")
      pipeline << "caps=\"image/tiff,framerate=" << opts.fps_num << "/" << opts.fps_den << "\" "
               << "! tiffdec ";
    else if(frame_ext == ".dpx")
      pipeline << "caps=\"image/x-dpx,framerate=" << opts.fps_num << "/" << opts.fps_den << "\" "
               << "! dpxdec ";
    else
      pipeline << "caps=\"image/png,framerate=" << opts.fps_num << "/" << opts.fps_den << "\" "
               << "! pngdec ";
  }
  else
  {
    // Single MXF file
    pipeline << "filesrc location=\"" << opts.input_dir.string() << "\" "
             << "! decodebin ";
  }

  // Video processing and output
  pipeline << "! videoconvert "
           << "! video/x-raw,format=" << opts.pixel_format
           << ",width=" << opts.width << ",height=" << opts.height << " "
           << "! decklinkvideosink device-number=" << opts.device_index
           << " mode=" << mode;

  // Audio if provided
  std::string audio_pipeline;
  if(!opts.audio_file.empty() && fs::exists(opts.audio_file))
  {
    audio_pipeline = " filesrc location=\"" + opts.audio_file.string() +
                     "\" ! decodebin ! audioconvert ! decklinkaudiosink device-number=" +
                     std::to_string(opts.device_index);
  }

  std::string full_pipeline = "gst-launch-1.0 " + pipeline.str() + audio_pipeline;

  spdlog::info("SDI output pipeline: {}", full_pipeline);
  spdlog::info("Mode: {}, Device: {}", mode, opts.device_index);

  int ret = system(full_pipeline.c_str());
  if(ret != 0)
  {
    result.error = "GStreamer pipeline failed with code " + std::to_string(ret);
    return result;
  }

  result.success = true;
  return result;
}

} // namespace imfwizard
