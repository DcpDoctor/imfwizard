#include "imfwizard/subtitle_convert.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

namespace imfwizard
{

std::string subtitle_format_name(SubtitleFormat fmt)
{
  switch(fmt)
  {
    case SubtitleFormat::SRT: return "srt";
    case SubtitleFormat::TTML: return "ttml";
    case SubtitleFormat::WebVTT: return "webvtt";
    case SubtitleFormat::SCC: return "scc";
    case SubtitleFormat::STL: return "stl";
    case SubtitleFormat::IMSC: return "imsc";
    default: return "unknown";
  }
}

SubtitleFormat parse_subtitle_format(const std::string& name)
{
  std::string lower = name;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
  if(lower == "srt") return SubtitleFormat::SRT;
  if(lower == "ttml" || lower == "xml") return SubtitleFormat::TTML;
  if(lower == "vtt" || lower == "webvtt") return SubtitleFormat::WebVTT;
  if(lower == "scc") return SubtitleFormat::SCC;
  if(lower == "stl") return SubtitleFormat::STL;
  if(lower == "imsc") return SubtitleFormat::IMSC;
  return SubtitleFormat::Unknown;
}

SubtitleFormat detect_subtitle_format(const std::filesystem::path& file)
{
  auto ext = file.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  if(ext == ".srt") return SubtitleFormat::SRT;
  if(ext == ".ttml" || ext == ".xml") return SubtitleFormat::TTML;
  if(ext == ".vtt") return SubtitleFormat::WebVTT;
  if(ext == ".scc") return SubtitleFormat::SCC;
  if(ext == ".stl") return SubtitleFormat::STL;
  return SubtitleFormat::Unknown;
}

// === Timecode helpers ===

static double parse_srt_time(const std::string& ts)
{
  // Format: HH:MM:SS,mmm
  int h = 0, m = 0, s = 0, ms = 0;
  char sep;
  std::istringstream ss(ts);
  ss >> h >> sep >> m >> sep >> s >> sep >> ms;
  return h * 3600.0 + m * 60.0 + s + ms / 1000.0;
}

static std::string format_srt_time(double sec)
{
  int total_ms = static_cast<int>(std::round(sec * 1000));
  int h = total_ms / 3600000;
  int m = (total_ms % 3600000) / 60000;
  int s = (total_ms % 60000) / 1000;
  int ms = total_ms % 1000;
  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(2) << h << ":"
      << std::setw(2) << m << ":" << std::setw(2) << s << "," << std::setw(3) << ms;
  return oss.str();
}

static std::string format_vtt_time(double sec)
{
  int total_ms = static_cast<int>(std::round(sec * 1000));
  int h = total_ms / 3600000;
  int m = (total_ms % 3600000) / 60000;
  int s = (total_ms % 60000) / 1000;
  int ms = total_ms % 1000;
  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(2) << h << ":"
      << std::setw(2) << m << ":" << std::setw(2) << s << "." << std::setw(3) << ms;
  return oss.str();
}

static std::string format_ttml_time(double sec)
{
  int total_ms = static_cast<int>(std::round(sec * 1000));
  int h = total_ms / 3600000;
  int m = (total_ms % 3600000) / 60000;
  int s = (total_ms % 60000) / 1000;
  int ms = total_ms % 1000;
  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(2) << h << ":"
      << std::setw(2) << m << ":" << std::setw(2) << s << "." << std::setw(3) << ms;
  return oss.str();
}

static double parse_scc_timecode(const std::string& tc, double fps)
{
  // Format: HH:MM:SS:FF
  int h = 0, m = 0, s = 0, f = 0;
  char sep;
  std::istringstream ss(tc);
  ss >> h >> sep >> m >> sep >> s >> sep >> f;
  return h * 3600.0 + m * 60.0 + s + f / fps;
}

// === Parsers ===

static std::vector<SubtitleCue> parse_srt(const std::filesystem::path& file)
{
  std::vector<SubtitleCue> cues;
  std::ifstream ifs(file);
  if(!ifs.is_open())
    return cues;

  std::string line;
  std::regex time_re(R"((\d{2}:\d{2}:\d{2}[,\.]\d{3})\s*-->\s*(\d{2}:\d{2}:\d{2}[,\.]\d{3}))");

  SubtitleCue current;
  bool reading_text = false;
  uint32_t index = 0;

  while(std::getline(ifs, line))
  {
    // Remove BOM and trailing whitespace
    if(!line.empty() && line.back() == '\r')
      line.pop_back();

    if(line.empty())
    {
      if(reading_text && !current.text.empty())
      {
        // Remove trailing newline from text
        if(!current.text.empty() && current.text.back() == '\n')
          current.text.pop_back();
        cues.push_back(current);
        current = {};
      }
      reading_text = false;
      continue;
    }

    std::smatch match;
    if(!reading_text && std::regex_search(line, match, time_re))
    {
      current.index = ++index;
      std::string start_str = match[1].str();
      std::string end_str = match[2].str();
      // Normalize period to comma for parsing
      std::replace(start_str.begin(), start_str.end(), '.', ',');
      std::replace(end_str.begin(), end_str.end(), '.', ',');
      current.start_time = parse_srt_time(start_str);
      current.end_time = parse_srt_time(end_str);
      reading_text = true;
    }
    else if(reading_text)
    {
      if(!current.text.empty())
        current.text += '\n';
      current.text += line;
    }
  }

  // Last cue
  if(reading_text && !current.text.empty())
  {
    if(!current.text.empty() && current.text.back() == '\n')
      current.text.pop_back();
    cues.push_back(current);
  }

  return cues;
}

static std::vector<SubtitleCue> parse_vtt(const std::filesystem::path& file)
{
  std::vector<SubtitleCue> cues;
  std::ifstream ifs(file);
  if(!ifs.is_open())
    return cues;

  std::string line;
  // Skip header
  std::getline(ifs, line); // "WEBVTT"
  while(std::getline(ifs, line))
  {
    if(!line.empty() && line.back() == '\r')
      line.pop_back();
    if(line.empty())
      break;
  }

  std::regex time_re(R"((\d{2}:\d{2}:\d{2}\.\d{3})\s*-->\s*(\d{2}:\d{2}:\d{2}\.\d{3}))");

  SubtitleCue current;
  bool reading_text = false;
  uint32_t index = 0;

  while(std::getline(ifs, line))
  {
    if(!line.empty() && line.back() == '\r')
      line.pop_back();

    if(line.empty())
    {
      if(reading_text && !current.text.empty())
      {
        if(current.text.back() == '\n')
          current.text.pop_back();
        cues.push_back(current);
        current = {};
      }
      reading_text = false;
      continue;
    }

    std::smatch match;
    if(!reading_text && std::regex_search(line, match, time_re))
    {
      current.index = ++index;
      // VTT uses . instead of , — already compatible with our parse
      std::string start_str = match[1].str();
      std::string end_str = match[2].str();
      std::replace(start_str.begin(), start_str.end(), '.', ',');
      std::replace(end_str.begin(), end_str.end(), '.', ',');
      current.start_time = parse_srt_time(start_str);
      current.end_time = parse_srt_time(end_str);
      reading_text = true;
    }
    else if(reading_text)
    {
      if(!current.text.empty())
        current.text += '\n';
      current.text += line;
    }
  }

  if(reading_text && !current.text.empty())
  {
    if(current.text.back() == '\n')
      current.text.pop_back();
    cues.push_back(current);
  }

  return cues;
}

static std::vector<SubtitleCue> parse_ttml(const std::filesystem::path& file)
{
  std::vector<SubtitleCue> cues;
  std::ifstream ifs(file);
  if(!ifs.is_open())
    return cues;

  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  // Simple regex-based TTML parser for <p begin="..." end="...">text</p>
  std::regex p_re(R"re(<p[^>]*begin="([^"]+)"[^>]*end="([^"]+)"[^>]*>([\s\S]*?)</p>)re");
  std::sregex_iterator it(content.begin(), content.end(), p_re);
  std::sregex_iterator end;

  uint32_t index = 0;
  for(; it != end; ++it)
  {
    SubtitleCue cue;
    cue.index = ++index;
    std::string start_str = (*it)[1].str();
    std::string end_str = (*it)[2].str();

    // TTML time can be HH:MM:SS.mmm or HH:MM:SS:FF
    std::replace(start_str.begin(), start_str.end(), '.', ',');
    std::replace(end_str.begin(), end_str.end(), '.', ',');
    cue.start_time = parse_srt_time(start_str);
    cue.end_time = parse_srt_time(end_str);

    // Strip XML tags from text
    std::string text = (*it)[3].str();
    std::regex tag_re(R"(<[^>]+>)");
    text = std::regex_replace(text, tag_re, "");
    // Trim
    auto ltrim = text.find_first_not_of(" \t\n\r");
    auto rtrim = text.find_last_not_of(" \t\n\r");
    if(ltrim != std::string::npos)
      text = text.substr(ltrim, rtrim - ltrim + 1);
    cue.text = text;
    cues.push_back(cue);
  }

  return cues;
}

static std::vector<SubtitleCue> parse_scc(const std::filesystem::path& file, double fps)
{
  std::vector<SubtitleCue> cues;
  std::ifstream ifs(file);
  if(!ifs.is_open())
    return cues;

  std::string line;
  std::regex tc_re(R"((\d{2}:\d{2}:\d{2}:\d{2})\s+(.+))");
  uint32_t index = 0;

  while(std::getline(ifs, line))
  {
    if(!line.empty() && line.back() == '\r')
      line.pop_back();

    std::smatch match;
    if(std::regex_match(line, match, tc_re))
    {
      SubtitleCue cue;
      cue.index = ++index;
      cue.start_time = parse_scc_timecode(match[1].str(), fps);
      cue.end_time = cue.start_time + 2.0; // Default 2s duration for SCC
      cue.text = "[SCC data: " + match[2].str() + "]"; // SCC is hex-encoded caption data
      cues.push_back(cue);
    }
  }

  // Adjust end times to next cue start
  for(size_t i = 0; i + 1 < cues.size(); ++i)
  {
    cues[i].end_time = cues[i + 1].start_time;
  }

  return cues;
}

std::vector<SubtitleCue> parse_subtitles(const std::filesystem::path& file,
                                         SubtitleFormat format, double fps)
{
  switch(format)
  {
    case SubtitleFormat::SRT: return parse_srt(file);
    case SubtitleFormat::WebVTT: return parse_vtt(file);
    case SubtitleFormat::TTML:
    case SubtitleFormat::IMSC: return parse_ttml(file);
    case SubtitleFormat::SCC: return parse_scc(file, fps);
    default: return {};
  }
}

// === Writers ===

static void write_srt(const std::vector<SubtitleCue>& cues, const std::filesystem::path& output)
{
  std::ofstream ofs(output);
  for(size_t i = 0; i < cues.size(); ++i)
  {
    ofs << (i + 1) << "\n";
    ofs << format_srt_time(cues[i].start_time) << " --> " << format_srt_time(cues[i].end_time) << "\n";
    ofs << cues[i].text << "\n\n";
  }
}

static void write_vtt(const std::vector<SubtitleCue>& cues, const std::filesystem::path& output)
{
  std::ofstream ofs(output);
  ofs << "WEBVTT\n\n";
  for(size_t i = 0; i < cues.size(); ++i)
  {
    ofs << (i + 1) << "\n";
    ofs << format_vtt_time(cues[i].start_time) << " --> " << format_vtt_time(cues[i].end_time) << "\n";
    ofs << cues[i].text << "\n\n";
  }
}

static void write_ttml(const std::vector<SubtitleCue>& cues, const SubtitleConvertOptions& opts,
                       const std::filesystem::path& output)
{
  std::ofstream ofs(output);
  ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  ofs << "<tt xml:lang=\"" << opts.language << "\" xmlns=\"http://www.w3.org/ns/ttml\"\n";
  ofs << "    xmlns:ttp=\"http://www.w3.org/ns/ttml#parameter\"\n";
  ofs << "    xmlns:tts=\"http://www.w3.org/ns/ttml#styling\">\n";
  ofs << "  <head>\n";
  ofs << "    <styling>\n";
  ofs << "      <style xml:id=\"default\" tts:fontFamily=\"proportionalSansSerif\"\n";
  ofs << "             tts:fontSize=\"100%\" tts:textAlign=\"center\"/>\n";
  ofs << "    </styling>\n";
  ofs << "    <layout>\n";
  ofs << "      <region xml:id=\"bottom\" tts:origin=\"10% 80%\" tts:extent=\"80% 20%\"\n";
  ofs << "              tts:displayAlign=\"after\" tts:textAlign=\"center\"/>\n";
  ofs << "    </layout>\n";
  ofs << "  </head>\n";
  ofs << "  <body>\n";
  ofs << "    <div>\n";

  for(const auto& cue : cues)
  {
    ofs << "      <p begin=\"" << format_ttml_time(cue.start_time) << "\" end=\""
        << format_ttml_time(cue.end_time) << "\" region=\"bottom\" style=\"default\">";
    // Escape XML entities
    std::string text = cue.text;
    // Replace & first to avoid double-escaping
    std::string escaped;
    for(char c : text)
    {
      switch(c)
      {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '\n': escaped += "<br/>"; break;
        default: escaped += c; break;
      }
    }
    ofs << escaped << "</p>\n";
  }

  ofs << "    </div>\n";
  ofs << "  </body>\n";
  ofs << "</tt>\n";
}

static void write_imsc(const std::vector<SubtitleCue>& cues, const SubtitleConvertOptions& opts,
                       const std::filesystem::path& output)
{
  std::ofstream ofs(output);
  ofs << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  ofs << "<tt xml:lang=\"" << opts.language << "\"\n";
  ofs << "    xmlns=\"http://www.w3.org/ns/ttml\"\n";
  ofs << "    xmlns:ttp=\"http://www.w3.org/ns/ttml#parameter\"\n";
  ofs << "    xmlns:tts=\"http://www.w3.org/ns/ttml#styling\"\n";
  ofs << "    xmlns:ittp=\"http://www.w3.org/ns/ttml/profile/imsc1#parameter\"\n";
  ofs << "    ttp:profile=\"http://www.w3.org/ns/ttml/profile/imsc1/text\">\n";
  ofs << "  <head>\n";
  ofs << "    <styling>\n";
  ofs << "      <style xml:id=\"default\" tts:fontFamily=\"proportionalSansSerif\"\n";
  ofs << "             tts:fontSize=\"100%\" tts:textAlign=\"center\"\n";
  ofs << "             tts:color=\"white\" tts:backgroundColor=\"transparent\"/>\n";
  ofs << "    </styling>\n";
  ofs << "    <layout>\n";
  ofs << "      <region xml:id=\"bottom\" tts:origin=\"10% 80%\" tts:extent=\"80% 20%\"\n";
  ofs << "              tts:displayAlign=\"after\" tts:textAlign=\"center\"/>\n";
  ofs << "    </layout>\n";
  ofs << "  </head>\n";
  ofs << "  <body>\n";
  ofs << "    <div>\n";

  for(const auto& cue : cues)
  {
    ofs << "      <p begin=\"" << format_ttml_time(cue.start_time) << "\" end=\""
        << format_ttml_time(cue.end_time) << "\" region=\"bottom\" style=\"default\">";
    std::string escaped;
    for(char c : cue.text)
    {
      switch(c)
      {
        case '&': escaped += "&amp;"; break;
        case '<': escaped += "&lt;"; break;
        case '>': escaped += "&gt;"; break;
        case '\n': escaped += "<br/>"; break;
        default: escaped += c; break;
      }
    }
    ofs << escaped << "</p>\n";
  }

  ofs << "    </div>\n";
  ofs << "  </body>\n";
  ofs << "</tt>\n";
}

SubtitleConvertResult write_subtitles(const std::vector<SubtitleCue>& cues,
                                      const SubtitleConvertOptions& opts)
{
  SubtitleConvertResult result;
  result.cue_count = static_cast<uint32_t>(cues.size());

  if(cues.empty())
  {
    result.error = "No cues to write";
    return result;
  }

  result.duration_seconds = cues.back().end_time;

  // Apply offset
  std::vector<SubtitleCue> adjusted = cues;
  if(opts.offset_sec != 0.0)
  {
    for(auto& cue : adjusted)
    {
      cue.start_time += opts.offset_sec;
      cue.end_time += opts.offset_sec;
      if(cue.start_time < 0)
        cue.start_time = 0;
      if(cue.end_time < 0)
        cue.end_time = 0;
    }
  }

  std::filesystem::create_directories(opts.output.parent_path());

  switch(opts.target_format)
  {
    case SubtitleFormat::SRT: write_srt(adjusted, opts.output); break;
    case SubtitleFormat::WebVTT: write_vtt(adjusted, opts.output); break;
    case SubtitleFormat::TTML: write_ttml(adjusted, opts, opts.output); break;
    case SubtitleFormat::IMSC: write_imsc(adjusted, opts, opts.output); break;
    default:
      result.error = "Unsupported target format: " + subtitle_format_name(opts.target_format);
      return result;
  }

  result.output_path = opts.output;
  result.success = true;
  return result;
}

SubtitleConvertResult convert_subtitles(const SubtitleConvertOptions& opts)
{
  SubtitleConvertResult result;

  if(!std::filesystem::exists(opts.input))
  {
    result.error = "Input file not found: " + opts.input.string();
    return result;
  }

  SubtitleFormat src_fmt = opts.source_format;
  if(src_fmt == SubtitleFormat::Unknown)
    src_fmt = detect_subtitle_format(opts.input);

  if(src_fmt == SubtitleFormat::Unknown)
  {
    result.error = "Cannot detect subtitle format for: " + opts.input.string();
    return result;
  }

  spdlog::info("Converting {} ({}) -> {} ({})",
               opts.input.string(), subtitle_format_name(src_fmt),
               opts.output.string(), subtitle_format_name(opts.target_format));

  auto cues = parse_subtitles(opts.input, src_fmt, opts.fps);
  if(cues.empty())
  {
    result.error = "No subtitle cues found in input file";
    return result;
  }

  result = write_subtitles(cues, opts);
  if(result.success)
  {
    spdlog::info("Converted {} cues, duration {:.1f}s", result.cue_count, result.duration_seconds);
  }
  return result;
}

} // namespace imfwizard
