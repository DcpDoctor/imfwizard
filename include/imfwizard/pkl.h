#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

struct PklAsset
{
  std::string uuid;
  std::string hash; // SHA-1 base64
  uint64_t size = 0;
  std::string type; // MIME type
  std::string original_filename;
};

struct PklOptions
{
  std::string issuer;
  std::vector<PklAsset> assets;
};

struct PklResult
{
  std::string uuid;
  std::filesystem::path path;
  std::string hash;
  uint64_t size = 0;
};

// Generate PKL XML (ST 429-8)
PklResult generate_pkl(const PklOptions& opts, const std::filesystem::path& output_dir);

} // namespace imfwizard
