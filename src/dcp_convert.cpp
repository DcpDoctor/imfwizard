#include "imfwizard/dcp_convert.h"
#include "imfwizard/info.h"
#include "imfwizard/extract.h"
#include "imfwizard/portable.h"
#include "imfwizard/uuid.h"
#include <spdlog/spdlog.h>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace imfwizard
{

bool has_dcp_tools()
{
#ifdef _WIN32
  return system("where asdcp-wrap >NUL 2>&1") == 0;
#else
  return system("which asdcp-wrap >/dev/null 2>&1") == 0;
#endif
}

static void write_dcp_cpl(const std::filesystem::path& path, const std::string& cpl_uuid,
                          const std::string& title, const std::string& content_kind,
                          const std::string& video_uuid, const std::string& audio_uuid,
                          uint32_t frame_count, uint32_t fps_num, uint32_t fps_den)
{
  std::ofstream f(path);
  f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  f << "<CompositionPlaylist xmlns=\"http://www.smpte-ra.org/schemas/429-7/2006/CPL\">\n";
  f << "  <Id>urn:uuid:" << cpl_uuid << "</Id>\n";
  f << "  <ContentTitleText>" << title << "</ContentTitleText>\n";
  f << "  <ContentKind>" << content_kind << "</ContentKind>\n";
  f << "  <ReelList>\n";
  f << "    <Reel>\n";
  f << "      <Id>urn:uuid:" << generate_uuid() << "</Id>\n";
  f << "      <AssetList>\n";
  f << "        <MainPicture>\n";
  f << "          <Id>urn:uuid:" << video_uuid << "</Id>\n";
  f << "          <EditRate>" << fps_num << " " << fps_den << "</EditRate>\n";
  f << "          <IntrinsicDuration>" << frame_count << "</IntrinsicDuration>\n";
  f << "          <Duration>" << frame_count << "</Duration>\n";
  f << "        </MainPicture>\n";
  if(!audio_uuid.empty())
  {
    f << "        <MainSound>\n";
    f << "          <Id>urn:uuid:" << audio_uuid << "</Id>\n";
    f << "          <EditRate>" << fps_num << " " << fps_den << "</EditRate>\n";
    f << "          <IntrinsicDuration>" << frame_count << "</IntrinsicDuration>\n";
    f << "          <Duration>" << frame_count << "</Duration>\n";
    f << "        </MainSound>\n";
  }
  f << "      </AssetList>\n";
  f << "    </Reel>\n";
  f << "  </ReelList>\n";
  f << "</CompositionPlaylist>\n";
}

DcpConvertResult convert_imp_to_dcp(const DcpConvertOptions& opts)
{
  DcpConvertResult result;

  if(!std::filesystem::exists(opts.imp_dir))
  {
    result.error = "IMP directory not found: " + opts.imp_dir.string();
    return result;
  }

  // Read IMP info to get track details
  auto imp_info = read_imp_info(opts.imp_dir.string());
  if(!imp_info.valid)
  {
    result.error = "Cannot read IMP: " + imp_info.error;
    return result;
  }

  std::filesystem::create_directories(opts.output_dir);

  auto title = opts.content_title.empty() ? imp_info.cpl_title : opts.content_title;

  // Find video and audio track files in the IMP
  std::filesystem::path video_mxf, audio_mxf;
  for(const auto& track : imp_info.tracks)
  {
    if(track.type == "video" && video_mxf.empty())
      video_mxf = opts.imp_dir / track.filename;
    else if(track.type == "audio" && audio_mxf.empty())
      audio_mxf = opts.imp_dir / track.filename;
  }

  if(video_mxf.empty())
  {
    result.error = "No video track found in IMP";
    return result;
  }

  // For DCP, we need JPEG 2000 in a SMPTE 429-3 (AS-DCP) MXF container
  // If we have asdcp-wrap, rewrap the essence; otherwise use the MXF directly
  auto video_uuid = generate_uuid();
  auto audio_uuid = audio_mxf.empty() ? "" : generate_uuid();
  auto dcp_video_mxf = opts.output_dir / (video_uuid + ".mxf");
  auto dcp_audio_mxf = opts.output_dir / (audio_uuid + ".mxf");

  if(has_dcp_tools())
  {
    // Use asdcp-wrap for proper DCP MXF wrapping
    std::string wrap_cmd = "asdcp-wrap -v -j \"" + video_mxf.string() + "\" \"" +
                           dcp_video_mxf.string() + "\" 2>/dev/null";
    if(system(wrap_cmd.c_str()) != 0)
    {
      // Fallback: just copy the MXF (may work for cinema servers that accept AS-02)
      std::filesystem::copy_file(video_mxf, dcp_video_mxf,
                                 std::filesystem::copy_options::overwrite_existing);
    }

    if(!audio_mxf.empty())
    {
      std::string awrap_cmd = "asdcp-wrap -v -a \"" + audio_mxf.string() + "\" \"" +
                              dcp_audio_mxf.string() + "\" 2>/dev/null";
      if(system(awrap_cmd.c_str()) != 0)
      {
        std::filesystem::copy_file(audio_mxf, dcp_audio_mxf,
                                   std::filesystem::copy_options::overwrite_existing);
      }
    }
  }
  else
  {
    // No asdcp-wrap: copy MXF files directly (best effort)
    std::filesystem::copy_file(video_mxf, dcp_video_mxf,
                               std::filesystem::copy_options::overwrite_existing);
    if(!audio_mxf.empty())
    {
      std::filesystem::copy_file(audio_mxf, dcp_audio_mxf,
                                 std::filesystem::copy_options::overwrite_existing);
    }
  }

  // Estimate frame count from file size (typical J2K frame ~1MB at 250Mbps/24fps)
  uint32_t frame_count = 0;
  auto vsize = std::filesystem::file_size(dcp_video_mxf);
  frame_count = static_cast<uint32_t>(vsize / (250'000'000 / 8 / 24)); // rough estimate
  if(frame_count == 0)
    frame_count = 1;

  // Write DCP CPL
  result.cpl_uuid = generate_uuid();
  write_dcp_cpl(opts.output_dir / "CPL.xml", result.cpl_uuid, title, opts.content_kind, video_uuid,
                audio_uuid, frame_count, 24, 1);

  // Write VOLINDEX
  {
    std::ofstream f(opts.output_dir / "VOLINDEX.xml");
    f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    f << "<VolumeIndex xmlns=\"http://www.smpte-ra.org/schemas/429-9/2007/AM\">\n";
    f << "  <Index>1</Index>\n";
    f << "</VolumeIndex>\n";
  }

  // Write ASSETMAP
  {
    auto am_uuid = generate_uuid();
    std::ofstream f(opts.output_dir / "ASSETMAP.xml");
    f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    f << "<AssetMap xmlns=\"http://www.smpte-ra.org/schemas/429-9/2007/AM\">\n";
    f << "  <Id>urn:uuid:" << am_uuid << "</Id>\n";
    f << "  <Creator>IMF Wizard</Creator>\n";
    f << "  <AssetList>\n";
    f << "    <Asset><Id>urn:uuid:" << result.cpl_uuid
      << "</Id><ChunkList><Chunk><Path>CPL.xml</Path></Chunk></ChunkList></Asset>\n";
    f << "    <Asset><Id>urn:uuid:" << video_uuid << "</Id><ChunkList><Chunk><Path>" << video_uuid
      << ".mxf</Path></Chunk></ChunkList></Asset>\n";
    if(!audio_uuid.empty())
      f << "    <Asset><Id>urn:uuid:" << audio_uuid << "</Id><ChunkList><Chunk><Path>" << audio_uuid
        << ".mxf</Path></Chunk></ChunkList></Asset>\n";
    f << "  </AssetList>\n";
    f << "</AssetMap>\n";
  }

  result.output_dir = opts.output_dir;
  result.reel_count = 1;
  result.total_size = 0;
  for(auto& entry : std::filesystem::directory_iterator(opts.output_dir))
  {
    if(entry.is_regular_file())
      result.total_size += entry.file_size();
  }
  result.success = true;
  return result;
}

} // namespace imfwizard
