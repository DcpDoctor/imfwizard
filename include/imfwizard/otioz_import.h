#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace imfwizard
{

struct OtiozClip
{
  std::string name;
  std::string media_reference; // Path within the bundle
  double start_time = 0.0;    // In seconds
  double duration = 0.0;
  std::string track_kind;     // "Video", "Audio", "Subtitle"
};

struct OtiozImportOptions
{
  std::filesystem::path input_file;  // .otioz bundle path
  std::filesystem::path output_dir;  // Where to extract media
  bool extract_media = true;         // Extract referenced media files
  bool generate_cpl = false;         // Auto-create CPL from timeline
  std::string title;                 // Title for generated CPL
  double fps = 24.0;                 // Target frame rate for CPL
};

struct OtiozImportResult
{
  bool success = false;
  std::string error;
  std::vector<OtiozClip> clips;
  uint32_t video_tracks = 0;
  uint32_t audio_tracks = 0;
  uint32_t subtitle_tracks = 0;
  std::filesystem::path extracted_dir;
  std::filesystem::path generated_cpl; // If generate_cpl was true
};

// Import an OTIOZ (OpenTimelineIO zip bundle) file
OtiozImportResult import_otioz(const OtiozImportOptions& opts);

} // namespace imfwizard
