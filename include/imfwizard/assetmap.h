#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

struct AssetMapEntry
{
  std::string uuid;
  std::filesystem::path path; // relative to IMP root
  uint64_t size = 0;
};

struct AssetMapOptions
{
  std::string issuer;
  std::vector<AssetMapEntry> assets;
};

// Generate ASSETMAP.xml (ST 429-9)
void generate_assetmap(const AssetMapOptions& opts, const std::filesystem::path& output_dir);

} // namespace imfwizard
