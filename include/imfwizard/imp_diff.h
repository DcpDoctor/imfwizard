#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

struct TrackDiff
{
  std::string track_id;
  std::string essence_type; // "video", "audio", "subtitle"
  std::string status;       // "added", "removed", "modified", "unchanged"
  std::string detail;       // Human-readable description
};

struct SegmentDiff
{
  std::string cpl_id;
  uint64_t entry_point = 0;
  uint64_t duration = 0;
  std::string status; // "added", "removed", "replaced"
  std::string old_track_id;
  std::string new_track_id;
};

struct ImpDiffOptions
{
  std::filesystem::path imp_a;   // First IMP (usually OV)
  std::filesystem::path imp_b;   // Second IMP (usually supplemental)
  bool include_hashes = false;   // Compare file hashes
  bool json_output = false;      // Output as JSON
  bool show_unchanged = false;   // Include unchanged tracks in report
};

struct ImpDiffResult
{
  bool success = false;
  std::string error;

  // Summary
  uint32_t tracks_added = 0;
  uint32_t tracks_removed = 0;
  uint32_t tracks_modified = 0;
  uint32_t segments_changed = 0;

  // Details
  std::vector<TrackDiff> track_diffs;
  std::vector<SegmentDiff> segment_diffs;

  // CPL-level changes
  bool cpl_title_changed = false;
  bool cpl_annotation_changed = false;
  bool edit_rate_changed = false;
};

// Compare two IMF packages and return differences
ImpDiffResult diff_packages(const ImpDiffOptions& opts);

} // namespace imfwizard
