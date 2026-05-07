#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

struct DcpConvertOptions
{
  std::filesystem::path imp_dir; // Input IMP directory
  std::filesystem::path output_dir; // Output DCP directory
  std::string content_title; // DCP content title (empty = reuse IMP title)
  std::string content_kind = "feature"; // feature, trailer, advertisement, etc.
  std::string rating; // MPAA/BBFC rating string
  bool encrypt = false; // Apply KDM encryption
  std::string cert_file; // X.509 certificate for KDM
  std::string key_file; // Private key
  bool stereo = false; // 3D stereoscopic
};

struct DcpConvertResult
{
  std::filesystem::path output_dir;
  std::string cpl_uuid;
  uint32_t reel_count = 0;
  uint64_t total_size = 0;
  bool success = false;
  std::string error;
};

// Convert an IMF package to a DCP (Digital Cinema Package)
DcpConvertResult convert_imp_to_dcp(const DcpConvertOptions& opts);

// Check if required tools are available (asdcp-wrap, etc.)
bool has_dcp_tools();

} // namespace imfwizard
