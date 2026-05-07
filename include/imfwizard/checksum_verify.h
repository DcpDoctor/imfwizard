#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace imfwizard
{

struct ChecksumEntry
{
  std::filesystem::path file_path;
  std::string expected_hash;  // from PKL/AssetMap
  std::string actual_hash;    // computed
  std::string algorithm;      // "SHA-1", "SHA-256"
  uint64_t expected_size = 0;
  uint64_t actual_size = 0;
  bool hash_match = false;
  bool size_match = false;
  bool file_exists = false;
};

struct ChecksumVerifyOptions
{
  std::filesystem::path imp_dir;  // IMP directory containing PKL + AssetMap
  bool verify_sizes = true;
  bool verify_hashes = true;      // can be slow for large files
  bool stop_on_first_error = false;
};

struct ChecksumVerifyResult
{
  std::vector<ChecksumEntry> entries;
  uint32_t total_assets = 0;
  uint32_t verified_ok = 0;
  uint32_t hash_mismatches = 0;
  uint32_t size_mismatches = 0;
  uint32_t missing_files = 0;
  bool all_valid = false;
  bool success = false;
  std::string error;
};

// Verify all asset checksums in an IMP against PKL hashes
ChecksumVerifyResult verify_imp_checksums(const ChecksumVerifyOptions& opts);

// Compute SHA-1 hash of a file (base64 encoded)
std::string compute_sha1_base64(const std::filesystem::path& file);

} // namespace imfwizard
