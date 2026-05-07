#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

enum class EditDecisionFormat
{
  CMX_EDL, // CMX 3600 EDL
  AAF, // Advanced Authoring Format
  FCP_XML, // Final Cut Pro XML
  OTIO, // OpenTimelineIO
};

struct EditEvent
{
  uint32_t index = 0;
  std::string reel_name;
  uint32_t src_in = 0; // source in (frames)
  uint32_t src_out = 0; // source out (frames)
  uint32_t rec_in = 0; // record in (frames)
  uint32_t rec_out = 0; // record out (frames)
  std::string track_type; // "V", "A1", "A2", etc.
  std::string transition; // "Cut", "Dissolve"
  std::filesystem::path source_file;
};

struct EdlParseOptions
{
  std::filesystem::path input_file;
  EditDecisionFormat format = EditDecisionFormat::CMX_EDL;
  uint32_t fps_num = 24;
  uint32_t fps_den = 1;
};

struct EdlParseResult
{
  std::vector<EditEvent> events;
  std::string title;
  double fps = 24.0;
  uint32_t total_frames = 0;
  bool success = false;
  std::string error;
};

// Parse an EDL/AAF/FCP XML file into a list of edit events
EdlParseResult parse_edl(const EdlParseOptions& opts);

// Auto-detect EDL format from file extension
EditDecisionFormat detect_edl_format(const std::filesystem::path& file);

} // namespace imfwizard
