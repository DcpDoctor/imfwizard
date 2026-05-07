#pragma once

#include <filesystem>
#include <string>
#include <cstdint>
#include <functional>

namespace imfwizard
{

// Aspera (IBM FASP) high-speed transfer
struct AsperaOptions
{
  std::filesystem::path source_dir; // IMP to transfer
  std::string remote_host;
  std::string remote_path;
  std::string username;
  std::string token; // Node API token or SSH key path
  bool use_ssh_key = false;
  uint32_t target_rate_mbps = 100; // Target transfer rate
  uint32_t min_rate_mbps = 10;
  bool encrypt = true; // AES-128 in-transit encryption
  std::function<void(double)> on_progress; // Progress callback 0-1
};

struct AsperaResult
{
  uint32_t files_transferred = 0;
  uint64_t total_bytes = 0;
  double elapsed_seconds = 0;
  double effective_rate_mbps = 0;
  bool success = false;
  std::string error;
};

// Transfer via ascp (Aspera command-line)
AsperaResult aspera_transfer(const AsperaOptions& opts);

// Check if ascp is available
bool ascp_available();

} // namespace imfwizard
