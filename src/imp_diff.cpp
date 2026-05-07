#include <algorithm>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>

#include "imfwizard/imp_diff.h"
#include "imfwizard/portable.h"

namespace imfwizard
{

namespace
{

struct AssetInfo
{
  std::string id;
  std::string type;
  std::string hash;
  uint64_t size = 0;
  std::filesystem::path path;
};

// Parse ASSETMAP.xml to get asset listing
std::map<std::string, AssetInfo> parse_assetmap(const std::filesystem::path& imp_dir)
{
  std::map<std::string, AssetInfo> assets;

  auto assetmap = imp_dir / "ASSETMAP.xml";
  if (!std::filesystem::exists(assetmap))
    assetmap = imp_dir / "ASSETMAP";

  if (!std::filesystem::exists(assetmap))
    return assets;

  std::ifstream in(assetmap);
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());

  // Simple regex-based XML parsing for asset entries
  std::regex asset_re(R"(<Asset>[\s\S]*?<Id>urn:uuid:([^<]+)</Id>[\s\S]*?<Path>([^<]+)</Path>[\s\S]*?</Asset>)");
  auto begin = std::sregex_iterator(content.begin(), content.end(), asset_re);
  auto end = std::sregex_iterator();

  for (auto it = begin; it != end; ++it)
  {
    AssetInfo info;
    info.id = (*it)[1].str();
    info.path = imp_dir / (*it)[2].str();
    if (std::filesystem::exists(info.path))
      info.size = std::filesystem::file_size(info.path);

    // Determine type from extension
    auto ext = info.path.extension().string();
    if (ext == ".mxf" || ext == ".MXF")
      info.type = "mxf";
    else if (ext == ".xml" || ext == ".XML")
      info.type = "xml";
    else
      info.type = "other";

    assets[info.id] = info;
  }

  return assets;
}

// Parse CPL XML for track/segment info
struct CplInfo
{
  std::string id;
  std::string title;
  std::string annotation;
  std::string edit_rate;
  std::vector<std::pair<std::string, std::string>> segments; // track_id, type
};

CplInfo parse_cpl(const std::filesystem::path& cpl_file)
{
  CplInfo info;
  if (!std::filesystem::exists(cpl_file))
    return info;

  std::ifstream in(cpl_file);
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());

  // Extract CPL ID
  std::regex id_re(R"(<Id>urn:uuid:([^<]+)</Id>)");
  std::smatch m;
  if (std::regex_search(content, m, id_re))
    info.id = m[1].str();

  // Extract title
  std::regex title_re(R"(<ContentTitle>([^<]*)</ContentTitle>)");
  if (std::regex_search(content, m, title_re))
    info.title = m[1].str();

  // Extract annotation
  std::regex annot_re(R"(<Annotation>([^<]*)</Annotation>)");
  if (std::regex_search(content, m, annot_re))
    info.annotation = m[1].str();

  // Extract edit rate
  std::regex rate_re(R"(<EditRate>([^<]+)</EditRate>)");
  if (std::regex_search(content, m, rate_re))
    info.edit_rate = m[1].str();

  // Extract track file references
  std::regex track_re(R"(<TrackFileId>urn:uuid:([^<]+)</TrackFileId>)");
  auto begin = std::sregex_iterator(content.begin(), content.end(), track_re);
  auto end_it = std::sregex_iterator();
  for (auto it = begin; it != end_it; ++it)
  {
    info.segments.emplace_back((*it)[1].str(), "track");
  }

  return info;
}

} // anonymous namespace

ImpDiffResult diff_packages(const ImpDiffOptions& opts)
{
  ImpDiffResult result;

  if (!std::filesystem::exists(opts.imp_a))
  {
    result.error = "IMP A does not exist: " + opts.imp_a.string();
    return result;
  }
  if (!std::filesystem::exists(opts.imp_b))
  {
    result.error = "IMP B does not exist: " + opts.imp_b.string();
    return result;
  }

  // Parse both asset maps
  auto assets_a = parse_assetmap(opts.imp_a);
  auto assets_b = parse_assetmap(opts.imp_b);

  // Find CPLs in each
  std::filesystem::path cpl_a, cpl_b;
  for (auto& [id, asset] : assets_a)
  {
    if (asset.type == "xml" && asset.path.filename().string().find("CPL") != std::string::npos)
      cpl_a = asset.path;
  }
  for (auto& [id, asset] : assets_b)
  {
    if (asset.type == "xml" && asset.path.filename().string().find("CPL") != std::string::npos)
      cpl_b = asset.path;
  }

  auto cpl_info_a = parse_cpl(cpl_a);
  auto cpl_info_b = parse_cpl(cpl_b);

  // Compare CPL-level properties
  result.cpl_title_changed = (cpl_info_a.title != cpl_info_b.title);
  result.cpl_annotation_changed = (cpl_info_a.annotation != cpl_info_b.annotation);
  result.edit_rate_changed = (cpl_info_a.edit_rate != cpl_info_b.edit_rate);

  // Compare tracks: find added/removed/modified
  for (auto& [id, asset] : assets_a)
  {
    if (asset.type != "mxf")
      continue;

    TrackDiff diff;
    diff.track_id = id;
    diff.essence_type = "video"; // Simplified

    auto it = assets_b.find(id);
    if (it == assets_b.end())
    {
      diff.status = "removed";
      diff.detail = "Track " + id + " removed in B";
      result.tracks_removed++;
    }
    else if (opts.include_hashes && asset.size != it->second.size)
    {
      diff.status = "modified";
      diff.detail = "Track " + id + " size changed (" +
                    std::to_string(asset.size) + " → " +
                    std::to_string(it->second.size) + ")";
      result.tracks_modified++;
    }
    else
    {
      diff.status = "unchanged";
      if (!opts.show_unchanged)
        continue;
    }

    result.track_diffs.push_back(diff);
  }

  for (auto& [id, asset] : assets_b)
  {
    if (asset.type != "mxf")
      continue;
    if (assets_a.find(id) == assets_a.end())
    {
      TrackDiff diff;
      diff.track_id = id;
      diff.essence_type = "video";
      diff.status = "added";
      diff.detail = "New track " + id + " in B";
      result.tracks_added++;
      result.track_diffs.push_back(diff);
    }
  }

  // Compare segments
  std::set<std::string> seg_ids_a, seg_ids_b;
  for (auto& [tid, type] : cpl_info_a.segments)
    seg_ids_a.insert(tid);
  for (auto& [tid, type] : cpl_info_b.segments)
    seg_ids_b.insert(tid);

  for (auto& tid : seg_ids_a)
  {
    if (seg_ids_b.find(tid) == seg_ids_b.end())
    {
      SegmentDiff sd;
      sd.old_track_id = tid;
      sd.status = "removed";
      result.segment_diffs.push_back(sd);
      result.segments_changed++;
    }
  }
  for (auto& tid : seg_ids_b)
  {
    if (seg_ids_a.find(tid) == seg_ids_a.end())
    {
      SegmentDiff sd;
      sd.new_track_id = tid;
      sd.status = "added";
      result.segment_diffs.push_back(sd);
      result.segments_changed++;
    }
  }

  result.success = true;
  return result;
}

} // namespace imfwizard
