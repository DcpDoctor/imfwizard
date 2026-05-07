#include "imfwizard/otioz_import.h"
#include "imfwizard/portable.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>

namespace imfwizard
{

namespace
{

// Minimal ZIP local file header parsing for .otioz bundles
struct ZipLocalHeader
{
  std::string filename;
  uint32_t compressed_size = 0;
  uint32_t uncompressed_size = 0;
  uint64_t data_offset = 0;
};

// OTIOZ is a ZIP file containing:
// - content.otio (JSON timeline)
// - media/ directory with referenced files
std::vector<ZipLocalHeader> list_zip_entries(const std::filesystem::path& zip_path)
{
  std::vector<ZipLocalHeader> entries;
  std::ifstream in(zip_path, std::ios::binary);
  if (!in)
    return entries;

  while (in)
  {
    uint32_t sig = 0;
    in.read(reinterpret_cast<char*>(&sig), 4);
    if (sig != 0x04034b50) // PK\x03\x04
      break;

    ZipLocalHeader hdr;
    in.seekg(22, std::ios::cur); // Skip version, flags, compression, etc.

    uint32_t comp_size, uncomp_size;
    in.read(reinterpret_cast<char*>(&comp_size), 4);
    in.read(reinterpret_cast<char*>(&uncomp_size), 4);
    hdr.compressed_size = comp_size;
    hdr.uncompressed_size = uncomp_size;

    uint16_t name_len, extra_len;
    in.read(reinterpret_cast<char*>(&name_len), 2);
    in.read(reinterpret_cast<char*>(&extra_len), 2);

    std::string name(name_len, '\0');
    in.read(name.data(), name_len);
    hdr.filename = name;

    in.seekg(extra_len, std::ios::cur);
    hdr.data_offset = in.tellg();

    // Skip file data
    in.seekg(comp_size, std::ios::cur);
    entries.push_back(hdr);
  }

  return entries;
}

// Parse content.otio JSON for clip information
std::vector<OtiozClip> parse_otio_json(const std::string& json)
{
  std::vector<OtiozClip> clips;

  // Parse clips from OTIO JSON
  // OTIO structure: { "OTIO_SCHEMA": "Timeline.1", "tracks": { "children": [...] } }
  std::regex clip_re(R"RE("OTIO_SCHEMA"\s*:\s*"Clip\.\d+")RE");
  std::regex name_re(R"RE("name"\s*:\s*"([^"]*)")RE");
  std::regex duration_re(R"RE("duration"\s*:\s*\{\s*"OTIO_SCHEMA"\s*:\s*"RationalTime\.\d+"\s*,\s*"value"\s*:\s*([\d.]+))RE");
  std::regex target_re(R"RE("target_url"\s*:\s*"([^"]*)")RE");

  // Simple state machine parser
  size_t pos = 0;
  while ((pos = json.find("\"OTIO_SCHEMA\": \"Clip.", pos)) != std::string::npos)
  {
    // Find the clip block (next ~500 chars should contain it)
    size_t block_end = std::min(pos + 2000, json.size());
    std::string block = json.substr(pos, block_end - pos);

    OtiozClip clip;
    std::smatch m;
    if (std::regex_search(block, m, name_re))
      clip.name = m[1].str();
    if (std::regex_search(block, m, duration_re))
      clip.duration = std::stod(m[1].str());
    if (std::regex_search(block, m, target_re))
      clip.media_reference = m[1].str();

    // Determine track kind from context
    size_t video_check = json.rfind("\"kind\": \"Video\"", pos);
    size_t audio_check = json.rfind("\"kind\": \"Audio\"", pos);
    if (video_check != std::string::npos && (audio_check == std::string::npos || video_check > audio_check))
      clip.track_kind = "Video";
    else
      clip.track_kind = "Audio";

    clips.push_back(clip);
    pos += 10;
  }

  return clips;
}

} // anonymous namespace

OtiozImportResult import_otioz(const OtiozImportOptions& opts)
{
  OtiozImportResult result;

  if (!std::filesystem::exists(opts.input_file))
  {
    result.error = "OTIOZ file not found: " + opts.input_file.string();
    return result;
  }

  auto ext = opts.input_file.extension().string();
  if (ext != ".otioz" && ext != ".otio")
  {
    result.error = "Expected .otioz or .otio file";
    return result;
  }

  // For .otio files (plain JSON), read directly
  std::string otio_content;
  if (ext == ".otio")
  {
    std::ifstream in(opts.input_file);
    otio_content = std::string((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
  }
  else
  {
    // .otioz - read from ZIP
    auto entries = list_zip_entries(opts.input_file);

    // Find content.otio in the zip
    for (auto& entry : entries)
    {
      if (entry.filename == "content.otio")
      {
        std::ifstream in(opts.input_file, std::ios::binary);
        in.seekg(entry.data_offset);
        otio_content.resize(entry.uncompressed_size);
        in.read(otio_content.data(), entry.uncompressed_size);
        break;
      }
    }

    if (otio_content.empty())
    {
      result.error = "No content.otio found in bundle";
      return result;
    }

    // Extract media files if requested
    if (opts.extract_media && !opts.output_dir.empty())
    {
      std::filesystem::create_directories(opts.output_dir / "media");
      result.extracted_dir = opts.output_dir / "media";

      for (auto& entry : entries)
      {
        if (entry.filename.starts_with("media/") && entry.uncompressed_size > 0)
        {
          auto out_path = opts.output_dir / entry.filename;
          std::filesystem::create_directories(out_path.parent_path());
          std::ifstream in(opts.input_file, std::ios::binary);
          in.seekg(entry.data_offset);
          std::ofstream out(out_path, std::ios::binary);
          std::vector<char> buf(entry.uncompressed_size);
          in.read(buf.data(), buf.size());
          out.write(buf.data(), buf.size());
        }
      }
    }
  }

  // Parse the OTIO JSON
  result.clips = parse_otio_json(otio_content);

  // Count by type
  for (auto& clip : result.clips)
  {
    if (clip.track_kind == "Video")
      result.video_tracks++;
    else if (clip.track_kind == "Audio")
      result.audio_tracks++;
    else
      result.subtitle_tracks++;
  }

  // Generate CPL if requested
  if (opts.generate_cpl && !opts.output_dir.empty())
  {
    auto cpl_path = opts.output_dir / "CPL_from_otio.xml";
    std::ofstream cpl(cpl_path);
    cpl << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    cpl << "<CompositionPlaylist xmlns=\"http://www.smpte-ra.org/schemas/2067-3/2016\">\n";
    cpl << "  <Id>urn:uuid:00000000-0000-0000-0000-000000000000</Id>\n";
    cpl << "  <ContentTitle>" << (opts.title.empty() ? "OTIOZ Import" : opts.title) << "</ContentTitle>\n";
    cpl << "  <EditRate>" << static_cast<int>(opts.fps) << " 1</EditRate>\n";
    cpl << "  <!-- Imported from: " << opts.input_file.filename().string() << " -->\n";
    cpl << "  <!-- " << result.clips.size() << " clips imported -->\n";
    cpl << "</CompositionPlaylist>\n";
    result.generated_cpl = cpl_path;
  }

  result.success = true;
  return result;
}

} // namespace imfwizard
