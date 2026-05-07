#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

#include "imfwizard/subtitle_retime.h"

namespace fs = std::filesystem;

namespace imfwizard
{

static double parse_ttml_time(const std::string& t, double fps)
{
  // Support both HH:MM:SS:FF and HH:MM:SS.mmm
  uint32_t h = 0, m = 0, s = 0, f = 0;
  if(sscanf(t.c_str(), "%u:%u:%u:%u", &h, &m, &s, &f) == 4)
  {
    return (h * 3600 + m * 60 + s) + static_cast<double>(f) / fps;
  }
  double ms = 0;
  if(sscanf(t.c_str(), "%u:%u:%u.%lf", &h, &m, &s, &ms) == 4)
  {
    return (h * 3600 + m * 60 + s) + ms / 1000.0;
  }
  return 0;
}

static std::string format_ttml_time(double seconds, double fps)
{
  uint32_t h = static_cast<uint32_t>(seconds / 3600);
  seconds -= h * 3600;
  uint32_t m = static_cast<uint32_t>(seconds / 60);
  seconds -= m * 60;
  uint32_t s = static_cast<uint32_t>(seconds);
  double frac = seconds - s;
  uint32_t f = static_cast<uint32_t>(std::round(frac * fps));

  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(2) << h << ":" << std::setw(2) << m << ":"
      << std::setw(2) << s << ":" << std::setw(2) << f;
  return oss.str();
}

SubtitleRetimeResult retime_subtitles(const SubtitleRetimeOptions& opts)
{
  SubtitleRetimeResult result;

  if(!fs::exists(opts.input_file))
  {
    result.error = "Input file not found: " + opts.input_file.string();
    return result;
  }

  double src_fps =
      static_cast<double>(opts.source_fps_num) / static_cast<double>(opts.source_fps_den);
  double tgt_fps =
      static_cast<double>(opts.target_fps_num) / static_cast<double>(opts.target_fps_den);

  if(src_fps <= 0 || tgt_fps <= 0)
  {
    result.error = "Invalid framerate";
    return result;
  }

  double ratio = tgt_fps / src_fps; // time scaling factor

  std::ifstream in(opts.input_file);
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  in.close();

  // Detect format (TTML or SRT)
  bool is_ttml = (content.find("<tt") != std::string::npos);

  std::string output;
  uint32_t entries = 0;

  if(is_ttml)
  {
    // TTML: find begin="..." end="..." attributes and retime
    std::regex time_re(R"RE((begin|end)="(\d{2}:\d{2}:\d{2}[:.]\d{2,3})")RE");
    // Process all time attributes
    std::smatch match;
    std::string processed;
    auto it = content.begin();
    std::string remaining = content;

    // Replace all time values
    while(std::regex_search(remaining, match, time_re))
    {
      processed += match.prefix().str();
      std::string attr = match[1].str();
      std::string time_val = match[2].str();

      double t = parse_ttml_time(time_val, src_fps);
      double new_t = opts.stretch ? (t / ratio) : t;
      std::string new_time = format_ttml_time(new_t, tgt_fps);

      processed += attr + "=\"" + new_time + "\"";
      remaining = match.suffix().str();
      entries++;
    }
    processed += remaining;
    output = processed;
  }
  else
  {
    // SRT format: retime HH:MM:SS,mmm --> HH:MM:SS,mmm
    std::regex srt_time_re(
        R"((\d{2}:\d{2}:\d{2}),(\d{3})\s*-->\s*(\d{2}:\d{2}:\d{2}),(\d{3}))");
    std::string remaining = content;
    std::smatch match;

    while(std::regex_search(remaining, match, srt_time_re))
    {
      output += match.prefix().str();

      auto retime_srt = [&](const std::string& hms, const std::string& ms_str) -> std::string {
        uint32_t h, m, s;
        sscanf(hms.c_str(), "%u:%u:%u", &h, &m, &s);
        double ms = std::stod(ms_str) / 1000.0;
        double t = h * 3600 + m * 60 + s + ms;
        double new_t = opts.stretch ? (t / ratio) : t;

        uint32_t nh = static_cast<uint32_t>(new_t / 3600);
        new_t -= nh * 3600;
        uint32_t nm = static_cast<uint32_t>(new_t / 60);
        new_t -= nm * 60;
        uint32_t ns = static_cast<uint32_t>(new_t);
        uint32_t nms = static_cast<uint32_t>((new_t - ns) * 1000);

        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << nh << ":" << std::setw(2) << nm << ":"
            << std::setw(2) << ns << "," << std::setw(3) << nms;
        return oss.str();
      };

      output += retime_srt(match[1].str(), match[2].str()) + " --> " +
                retime_srt(match[3].str(), match[4].str());
      remaining = match.suffix().str();
      entries++;
    }
    output += remaining;
  }

  // Write output
  std::ofstream out(opts.output_file);
  out << output;
  out.close();

  // Calculate total time shift
  double total_duration_src = 0; // would need to parse last cue
  result.time_shift_ms = (1.0 - ratio) * total_duration_src * 1000.0;

  result.output_file = opts.output_file;
  result.entries_processed = entries / 2; // begin+end = 1 entry
  result.success = true;
  return result;
}

} // namespace imfwizard
