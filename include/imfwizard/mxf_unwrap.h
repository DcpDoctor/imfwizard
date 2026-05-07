#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace imfwizard
{

// Information about an MXF track
struct MxfTrackInfo
{
  std::string track_id;
  std::string essence_type;  // "video", "audio", "timed_text"
  std::string codec;         // "jpeg2000", "pcm", "ttml"
  uint32_t duration = 0;     // edit units (frames or sample groups)
  uint32_t edit_rate_num = 0;
  uint32_t edit_rate_den = 0;
  uint16_t channels = 0;     // audio only
  uint32_t sample_rate = 0;  // audio only
  uint16_t bit_depth = 0;    // audio or video
  uint32_t width = 0;        // video only
  uint32_t height = 0;       // video only
  std::string label;         // MCA label or track description
};

struct MxfProbeResult
{
  std::vector<MxfTrackInfo> tracks;
  std::string container_label; // AS-02, AS-DCP, OP1a, etc.
  uint64_t file_size = 0;
  bool success = false;
  std::string error;
};

struct MxfUnwrapOptions
{
  std::filesystem::path input;      // MXF file
  std::filesystem::path output_dir; // Where to extract essences
  bool extract_video = true;
  bool extract_audio = true;
  bool extract_timed_text = true;
  uint32_t start_frame = 0;        // 0 = from beginning
  uint32_t end_frame = 0;          // 0 = to end
};

struct MxfUnwrapResult
{
  std::vector<std::filesystem::path> extracted_files;
  uint32_t frames_extracted = 0;
  bool success = false;
  std::string error;
};

// Probe MXF file for detailed track information
MxfProbeResult probe_mxf_detailed(const std::filesystem::path& mxf_path);

// Unwrap (extract) essence from MXF container
MxfUnwrapResult unwrap_mxf(const MxfUnwrapOptions& opts);

} // namespace imfwizard
