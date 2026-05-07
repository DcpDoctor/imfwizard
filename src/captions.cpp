#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <regex>

#include "imfwizard/captions.h"

namespace imfwizard
{

static uint32_t srt_time_to_frames(const std::string& tc, uint32_t fps)
{
  uint32_t h = 0, m = 0, s = 0, ms = 0;
  if(sscanf(tc.c_str(), "%u:%u:%u,%u", &h, &m, &s, &ms) == 4)
    return ((h * 3600 + m * 60 + s) * fps) + (ms * fps / 1000);
  return 0;
}

static uint32_t scc_time_to_frames(const std::string& tc, uint32_t fps)
{
  uint32_t h = 0, m = 0, s = 0, f = 0;
  if(sscanf(tc.c_str(), "%u:%u:%u:%u", &h, &m, &s, &f) == 4)
    return (h * 3600 + m * 60 + s) * fps + f;
  return 0;
}

CaptionParseResult parse_captions(const std::filesystem::path& file, uint32_t fps)
{
  CaptionParseResult result;

  std::ifstream f(file);
  if(!f)
  {
    result.error = "Cannot open caption file: " + file.string();
    return result;
  }

  auto ext = file.extension().string();

  if(ext == ".srt")
  {
    result.format = CaptionFormat::SRT;
    std::string line;
    std::regex time_re(R"((\d{2}:\d{2}:\d{2},\d{3})\s*-->\s*(\d{2}:\d{2}:\d{2},\d{3}))");

    CaptionEntry entry;
    bool in_text = false;
    std::string text_buf;

    while(std::getline(f, line))
    {
      if(line.empty() && in_text)
      {
        entry.text = text_buf;
        result.entries.push_back(entry);
        text_buf.clear();
        in_text = false;
        continue;
      }

      std::smatch match;
      if(std::regex_search(line, match, time_re))
      {
        entry.start_frame = srt_time_to_frames(match[1].str(), fps);
        entry.end_frame = srt_time_to_frames(match[2].str(), fps);
        in_text = true;
        text_buf.clear();
      }
      else if(in_text)
      {
        if(!text_buf.empty())
          text_buf += "\n";
        text_buf += line;
      }
    }
    if(in_text && !text_buf.empty())
    {
      entry.text = text_buf;
      result.entries.push_back(entry);
    }
  }
  else if(ext == ".scc")
  {
    result.format = CaptionFormat::SCC;
    std::string line;
    std::regex tc_re(R"((\d{2}:\d{2}:\d{2}:\d{2})\s+(.+))");

    while(std::getline(f, line))
    {
      std::smatch match;
      if(std::regex_search(line, match, tc_re))
      {
        CaptionEntry entry;
        entry.start_frame = scc_time_to_frames(match[1].str(), fps);
        entry.end_frame = entry.start_frame + fps * 2; // Default 2 second display
        entry.text = match[2].str(); // Hex-encoded caption data
        result.entries.push_back(entry);
      }
    }
  }
  else
  {
    result.error = "Unsupported caption format: " + ext;
    return result;
  }

  result.success = true;
  spdlog::info("Parsed {} caption entries from {}", result.entries.size(), file.string());
  return result;
}

std::string captions_to_ttml(const std::vector<CaptionEntry>& entries, uint32_t fps_num,
                             uint32_t fps_den)
{
  double fps = static_cast<double>(fps_num) / fps_den;

  std::ostringstream xml;
  xml << R"(<?xml version="1.0" encoding="UTF-8"?>)"
         "\n";
  xml << R"(<tt xmlns="http://www.w3.org/ns/ttml" xmlns:ttp="http://www.w3.org/ns/ttml#parameter")"
         "\n";
  xml << R"(    ttp:frameRate=")" << fps_num << R"(" ttp:frameRateMultiplier="1 )" << fps_den
      << R"(">)"
         "\n";
  xml << "  <body>\n    <div>\n";

  for(const auto& e : entries)
  {
    double start_s = e.start_frame / fps;
    double end_s = e.end_frame / fps;

    int sh = static_cast<int>(start_s) / 3600;
    int sm = (static_cast<int>(start_s) % 3600) / 60;
    int ss = static_cast<int>(start_s) % 60;
    int sf = static_cast<int>((start_s - static_cast<int>(start_s)) * 1000);

    int eh = static_cast<int>(end_s) / 3600;
    int em = (static_cast<int>(end_s) % 3600) / 60;
    int es = static_cast<int>(end_s) % 60;
    int ef = static_cast<int>((end_s - static_cast<int>(end_s)) * 1000);

    char begin_tc[32], end_tc[32];
    snprintf(begin_tc, sizeof(begin_tc), "%02d:%02d:%02d.%03d", sh, sm, ss, sf);
    snprintf(end_tc, sizeof(end_tc), "%02d:%02d:%02d.%03d", eh, em, es, ef);

    xml << "      <p begin=\"" << begin_tc << "\" end=\"" << end_tc << "\">" << e.text << "</p>\n";
  }

  xml << "    </div>\n  </body>\n</tt>\n";
  return xml.str();
}

void write_ttml_captions(const std::vector<CaptionEntry>& entries,
                         const std::filesystem::path& output, uint32_t fps_num, uint32_t fps_den)
{
  auto ttml = captions_to_ttml(entries, fps_num, fps_den);
  std::ofstream f(output);
  f << ttml;
  spdlog::info("Wrote TTML captions to {}", output.string());
}

} // namespace imfwizard
