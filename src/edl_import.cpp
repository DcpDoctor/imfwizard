#include "imfwizard/edl_import.h"
#include "imfwizard/portable.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

namespace imfwizard
{

EditDecisionFormat detect_edl_format(const fs::path& file)
{
  auto ext = file.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  if(ext == ".edl")
    return EditDecisionFormat::CMX_EDL;
  if(ext == ".aaf")
    return EditDecisionFormat::AAF;
  if(ext == ".xml" || ext == ".fcpxml")
    return EditDecisionFormat::FCP_XML;
  if(ext == ".otio")
    return EditDecisionFormat::OTIO;
  return EditDecisionFormat::CMX_EDL;
}

static uint32_t timecode_to_frames(const std::string& tc, double fps)
{
  uint32_t h = 0, m = 0, s = 0, f = 0;
  if(sscanf(tc.c_str(), "%u:%u:%u:%u", &h, &m, &s, &f) == 4)
  {
    return static_cast<uint32_t>((h * 3600 + m * 60 + s) * fps + f);
  }
  return 0;
}

static EdlParseResult parse_cmx_edl(const fs::path& file, double fps)
{
  EdlParseResult result;
  std::ifstream in(file);
  if(!in.is_open())
  {
    result.error = "Cannot open file: " + file.string();
    return result;
  }

  std::string line;
  // First line is title
  if(std::getline(in, line))
  {
    if(line.find("TITLE:") != std::string::npos)
      result.title = line.substr(line.find("TITLE:") + 6);
  }

  // CMX 3600 format: EVENT REEL TRACK TRANSITION SRC_IN SRC_OUT REC_IN REC_OUT
  std::regex event_re(
      R"((\d+)\s+(\S+)\s+(\S+)\s+(\S+)\s+(\d{2}:\d{2}:\d{2}:\d{2})\s+(\d{2}:\d{2}:\d{2}:\d{2})\s+(\d{2}:\d{2}:\d{2}:\d{2})\s+(\d{2}:\d{2}:\d{2}:\d{2}))");

  while(std::getline(in, line))
  {
    std::smatch match;
    if(std::regex_search(line, match, event_re))
    {
      EditEvent event;
      event.index = static_cast<uint32_t>(std::stoul(match[1].str()));
      event.reel_name = match[2].str();
      event.track_type = match[3].str();
      event.transition = match[4].str();
      event.src_in = timecode_to_frames(match[5].str(), fps);
      event.src_out = timecode_to_frames(match[6].str(), fps);
      event.rec_in = timecode_to_frames(match[7].str(), fps);
      event.rec_out = timecode_to_frames(match[8].str(), fps);
      result.events.push_back(event);
    }
  }

  if(!result.events.empty())
  {
    result.total_frames = result.events.back().rec_out;
  }

  result.fps = fps;
  result.success = true;
  return result;
}

static EdlParseResult parse_fcp_xml(const fs::path& file, double fps)
{
  EdlParseResult result;
  std::ifstream in(file);
  if(!in.is_open())
  {
    result.error = "Cannot open file: " + file.string();
    return result;
  }

  // Simple XML parsing for clip elements
  std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

  // Extract title
  std::regex title_re(R"(<name>([^<]+)</name>)");
  std::smatch title_match;
  if(std::regex_search(content, title_match, title_re))
    result.title = title_match[1].str();

  // Extract clips
  std::regex clip_re(
      R"(<clip[^>]*>.*?<name>([^<]*)</name>.*?<start>(\d+)</start>.*?<end>(\d+)</end>.*?</clip>)");
  auto it = std::sregex_iterator(content.begin(), content.end(), clip_re);
  uint32_t idx = 1;
  uint32_t rec_pos = 0;
  for(; it != std::sregex_iterator(); ++it, ++idx)
  {
    EditEvent event;
    event.index = idx;
    event.reel_name = (*it)[1].str();
    event.src_in = static_cast<uint32_t>(std::stoul((*it)[2].str()));
    event.src_out = static_cast<uint32_t>(std::stoul((*it)[3].str()));
    event.rec_in = rec_pos;
    event.rec_out = rec_pos + (event.src_out - event.src_in);
    event.track_type = "V";
    event.transition = "Cut";
    rec_pos = event.rec_out;
    result.events.push_back(event);
  }

  result.fps = fps;
  result.total_frames = rec_pos;
  result.success = !result.events.empty();
  if(!result.success)
    result.error = "No clips found in FCP XML";
  return result;
}

EdlParseResult parse_edl(const EdlParseOptions& opts)
{
  if(!fs::exists(opts.input_file))
  {
    EdlParseResult r;
    r.error = "File not found: " + opts.input_file.string();
    return r;
  }

  double fps = static_cast<double>(opts.fps_num) / static_cast<double>(opts.fps_den);

  switch(opts.format)
  {
  case EditDecisionFormat::CMX_EDL: return parse_cmx_edl(opts.input_file, fps);
  case EditDecisionFormat::FCP_XML: return parse_fcp_xml(opts.input_file, fps);
  case EditDecisionFormat::AAF:
  case EditDecisionFormat::OTIO:
  {
    EdlParseResult r;
    r.error = "Format not yet supported (AAF/OTIO require external libraries)";
    return r;
  }
  }

  EdlParseResult r;
  r.error = "Unknown format";
  return r;
}

} // namespace imfwizard
