#include "imfwizard/checksum_verify.h"
#include "imfwizard/portable.h"
#include <spdlog/spdlog.h>
#include <array>
#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace imfwizard
{

static std::string exec_hash(const std::string& cmd)
{
  std::array<char, 4096> buffer;
  std::string result;
  FILE* pipe = portable_popen(cmd.c_str(), "r");
  if(!pipe)
    return {};
  while(fgets(buffer.data(), buffer.size(), pipe))
    result += buffer.data();
  portable_pclose(pipe);
  return result;
}

std::string compute_sha1_base64(const std::filesystem::path& file)
{
  if(!std::filesystem::exists(file))
    return {};

  // Use openssl or sha1sum + base64
  std::ostringstream cmd;
  cmd << "openssl dgst -sha1 -binary \"" << file.string() << "\" | openssl base64 -A 2>/dev/null";
  auto result = exec_hash(cmd.str());

  // Trim whitespace
  while(!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
    result.pop_back();

  return result;
}

// Parse PKL XML to extract asset IDs and their hashes
static std::vector<ChecksumEntry> parse_pkl(const std::filesystem::path& pkl_path,
                                            const std::filesystem::path& imp_dir)
{
  std::vector<ChecksumEntry> entries;
  std::ifstream ifs(pkl_path);
  if(!ifs.is_open())
    return entries;

  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  // Match <Asset> blocks in PKL
  // Each has: <Id>, <Hash>, <Size>, and <OriginalFileName> or can be resolved via AssetMap
  std::regex asset_re(
      R"(<Asset>[\s\S]*?<Id>urn:uuid:([^<]+)</Id>[\s\S]*?<Hash(?:\s+Algorithm="[^"]*")?>([\s\S]*?)</Hash>[\s\S]*?<Size>(\d+)</Size>[\s\S]*?</Asset>)");

  std::sregex_iterator it(content.begin(), content.end(), asset_re);
  std::sregex_iterator end;

  for(; it != end; ++it)
  {
    ChecksumEntry entry;
    std::string uuid = (*it)[1].str();
    entry.expected_hash = (*it)[2].str();
    entry.expected_size = std::stoull((*it)[3].str());
    entry.algorithm = "SHA-1"; // IMF PKL uses SHA-1 by default

    // Trim hash whitespace
    auto& h = entry.expected_hash;
    h.erase(std::remove_if(h.begin(), h.end(), ::isspace), h.end());

    entries.push_back(entry);
  }

  return entries;
}

// Parse AssetMap to resolve UUIDs to file paths
static void resolve_asset_paths(std::vector<ChecksumEntry>& entries,
                                const std::filesystem::path& imp_dir)
{
  // Find ASSETMAP.xml
  std::filesystem::path assetmap;
  for(const auto& f : std::filesystem::directory_iterator(imp_dir))
  {
    auto name = f.path().filename().string();
    if(name == "ASSETMAP.xml" || name == "ASSETMAP")
    {
      assetmap = f.path();
      break;
    }
  }

  if(assetmap.empty())
  {
    // Fall back: try to find files by extension in imp_dir
    for(auto& entry : entries)
    {
      for(const auto& f : std::filesystem::directory_iterator(imp_dir))
      {
        if(f.is_regular_file() && std::filesystem::file_size(f.path()) == entry.expected_size)
        {
          entry.file_path = f.path();
          break;
        }
      }
    }
    return;
  }

  std::ifstream ifs(assetmap);
  std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  // Parse AssetMap: <Asset><Id>urn:uuid:X</Id>...<Path>filename.mxf</Path>...</Asset>
  std::regex asset_re(
      R"(<Asset>[\s\S]*?<Id>urn:uuid:([^<]+)</Id>[\s\S]*?<Path>([^<]+)</Path>[\s\S]*?</Asset>)");

  std::sregex_iterator it(content.begin(), content.end(), asset_re);
  std::sregex_iterator end;

  // Build UUID -> path map
  std::unordered_map<std::string, std::string> uuid_to_path;
  for(; it != end; ++it)
  {
    uuid_to_path[(*it)[1].str()] = (*it)[2].str();
  }

  // Now also try matching by file size since we may not have UUID correlation
  // between PKL entries and the parsed AssetMap entries
  for(auto& entry : entries)
  {
    if(entry.file_path.empty())
    {
      // Try matching by size
      for(const auto& f : std::filesystem::directory_iterator(imp_dir))
      {
        if(!f.is_regular_file())
          continue;
        auto ext = f.path().extension().string();
        if(ext == ".xml" && f.path().filename().string().find("PKL") != std::string::npos)
          continue;
        if(ext == ".xml" && f.path().filename().string().find("ASSETMAP") != std::string::npos)
          continue;
        if(std::filesystem::file_size(f.path()) == entry.expected_size)
        {
          entry.file_path = f.path();
          break;
        }
      }
    }
  }
}

ChecksumVerifyResult verify_imp_checksums(const ChecksumVerifyOptions& opts)
{
  ChecksumVerifyResult result;
  namespace fs = std::filesystem;

  if(!fs::exists(opts.imp_dir))
  {
    result.error = "IMP directory not found: " + opts.imp_dir.string();
    return result;
  }

  // Find PKL file
  fs::path pkl_path;
  for(const auto& f : fs::directory_iterator(opts.imp_dir))
  {
    auto name = f.path().filename().string();
    if(name.find("PKL") != std::string::npos && f.path().extension() == ".xml")
    {
      pkl_path = f.path();
      break;
    }
  }

  if(pkl_path.empty())
  {
    result.error = "No PKL found in " + opts.imp_dir.string();
    return result;
  }

  spdlog::info("Parsing PKL: {}", pkl_path.filename().string());
  result.entries = parse_pkl(pkl_path, opts.imp_dir);
  resolve_asset_paths(result.entries, opts.imp_dir);

  result.total_assets = static_cast<uint32_t>(result.entries.size());

  for(auto& entry : result.entries)
  {
    // Check file exists
    entry.file_exists = !entry.file_path.empty() && fs::exists(entry.file_path);
    if(!entry.file_exists)
    {
      ++result.missing_files;
      spdlog::warn("Missing file for asset (expected size: {} bytes)", entry.expected_size);
      if(opts.stop_on_first_error)
        break;
      continue;
    }

    // Check size
    if(opts.verify_sizes)
    {
      entry.actual_size = fs::file_size(entry.file_path);
      entry.size_match = (entry.actual_size == entry.expected_size);
      if(!entry.size_match)
      {
        ++result.size_mismatches;
        spdlog::warn("Size mismatch: {} (expected {} got {})",
                     entry.file_path.filename().string(), entry.expected_size, entry.actual_size);
        if(opts.stop_on_first_error)
          break;
      }
    }

    // Check hash
    if(opts.verify_hashes && !entry.expected_hash.empty())
    {
      spdlog::info("Verifying hash: {}", entry.file_path.filename().string());
      entry.actual_hash = compute_sha1_base64(entry.file_path);
      entry.hash_match = (entry.actual_hash == entry.expected_hash);
      if(!entry.hash_match)
      {
        ++result.hash_mismatches;
        spdlog::error("Hash mismatch: {} (expected {} got {})",
                      entry.file_path.filename().string(), entry.expected_hash, entry.actual_hash);
        if(opts.stop_on_first_error)
          break;
      }
      else
      {
        ++result.verified_ok;
      }
    }
    else if(!opts.verify_hashes)
    {
      if(entry.size_match)
        ++result.verified_ok;
    }
  }

  result.all_valid = (result.hash_mismatches == 0 && result.size_mismatches == 0 &&
                      result.missing_files == 0);
  result.success = true;

  spdlog::info("Checksum verification: {}/{} OK, {} hash mismatches, {} size mismatches, {} missing",
               result.verified_ok, result.total_assets,
               result.hash_mismatches, result.size_mismatches, result.missing_files);
  return result;
}

} // namespace imfwizard
