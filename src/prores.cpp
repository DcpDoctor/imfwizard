#include <spdlog/spdlog.h>
#include <cstdio>
#include <cstring>
#include <sstream>

#include "imfwizard/prores.h"
#include "imfwizard/portable.h"
#include "imfwizard/uuid.h"
#include "imfwizard/pkl.h"
#include "imfwizard/assetmap.h"
#include "imfwizard/cpl.h"
#include "imfwizard/hash.h"

namespace imfwizard
{

static bool has_ffprobe()
{
#ifdef _WIN32
  return system("where ffprobe >NUL 2>&1") == 0;
#else
  return system("which ffprobe >/dev/null 2>&1") == 0;
#endif
}

static bool has_ffmpeg()
{
#ifdef _WIN32
  return system("where ffmpeg >NUL 2>&1") == 0;
#else
  return system("which ffmpeg >/dev/null 2>&1") == 0;
#endif
}

bool is_prores_file(const std::filesystem::path& file)
{
  if(!has_ffprobe())
    return false;
  std::string cmd =
      "ffprobe -v error -select_streams v:0 -show_entries stream=codec_name -of csv=p=0 \"" +
      file.string() + "\" 2>/dev/null";
  FILE* pipe = portable_popen(cmd.c_str(), "r");
  if(!pipe)
    return false;
  char buf[128];
  std::string result;
  while(fgets(buf, sizeof(buf), pipe))
    result += buf;
  portable_pclose(pipe);
  // Trim
  while(!result.empty() && (result.back() == '\n' || result.back() == '\r'))
    result.pop_back();
  return result == "prores";
}

ProResProfile detect_prores_profile(const std::filesystem::path& file)
{
  if(!has_ffprobe())
    return ProResProfile::HQ;
  std::string cmd =
      "ffprobe -v error -select_streams v:0 -show_entries stream=profile -of csv=p=0 \"" +
      file.string() + "\" 2>/dev/null";
  FILE* pipe = portable_popen(cmd.c_str(), "r");
  if(!pipe)
    return ProResProfile::HQ;
  char buf[128];
  std::string result;
  while(fgets(buf, sizeof(buf), pipe))
    result += buf;
  portable_pclose(pipe);
  while(!result.empty() && (result.back() == '\n' || result.back() == '\r'))
    result.pop_back();

  if(result.find("Proxy") != std::string::npos)
    return ProResProfile::Proxy;
  if(result.find("LT") != std::string::npos)
    return ProResProfile::LT;
  if(result.find("Standard") != std::string::npos)
    return ProResProfile::Standard;
  if(result.find("XQ") != std::string::npos || result.find("4444 XQ") != std::string::npos)
    return ProResProfile::XQ;
  return ProResProfile::HQ;
}

ProResImpResult create_prores_imp(const ProResImpOptions& opts)
{
  ProResImpResult result;

  if(!std::filesystem::exists(opts.input_file))
  {
    result.error = "Input file not found: " + opts.input_file.string();
    return result;
  }

  if(!has_ffmpeg())
  {
    result.error = "ffmpeg not found — required for ProRes extraction";
    return result;
  }

  std::filesystem::create_directories(opts.output_dir);

  // Extract ProRes essence to MXF via ffmpeg (ProRes stays as-is in MXF container)
  auto video_uuid = generate_uuid();
  auto video_mxf = opts.output_dir / (video_uuid + ".mxf");

  std::string cmd = "ffmpeg -y -i \"" + opts.input_file.string() + "\" -c:v copy -f mxf \"" +
                    video_mxf.string() + "\" 2>/dev/null";
  if(system(cmd.c_str()) != 0)
  {
    result.error = "Failed to wrap ProRes essence to MXF";
    return result;
  }

  // Get frame count
  std::string count_cmd = "ffprobe -v error -count_frames -select_streams v:0 "
                          "-show_entries stream=nb_read_frames -of csv=p=0 \"" +
                          opts.input_file.string() + "\" 2>/dev/null";
  FILE* pipe = portable_popen(count_cmd.c_str(), "r");
  char buf[64];
  std::string frame_str;
  if(pipe)
  {
    while(fgets(buf, sizeof(buf), pipe))
      frame_str += buf;
    portable_pclose(pipe);
  }
  result.frame_count = frame_str.empty() ? 0 : static_cast<uint32_t>(std::stoul(frame_str));

  // Extract audio if present
  std::string audio_uuid;
  std::filesystem::path audio_mxf;
  std::string audio_check =
      "ffprobe -v error -select_streams a:0 -show_entries stream=codec_type -of csv=p=0 \"" +
      opts.input_file.string() + "\" 2>/dev/null";
  pipe = portable_popen(audio_check.c_str(), "r");
  bool has_audio = false;
  if(pipe)
  {
    std::string aresult;
    while(fgets(buf, sizeof(buf), pipe))
      aresult += buf;
    portable_pclose(pipe);
    has_audio = aresult.find("audio") != std::string::npos;
  }

  if(has_audio)
  {
    audio_uuid = generate_uuid();
    audio_mxf = opts.output_dir / (audio_uuid + ".mxf");
    std::string acmd = "ffmpeg -y -i \"" + opts.input_file.string() + "\" -vn -c:a pcm_s" +
                       std::to_string(opts.audio_bit_depth) + "le -ar " +
                       std::to_string(opts.sample_rate) + " -f mxf \"" + audio_mxf.string() +
                       "\" 2>/dev/null";
    system(acmd.c_str());
  }

  // Build PKL
  PklOptions pkl_opts;
  pkl_opts.issuer = opts.issuer;

  auto video_hash = sha1_base64(video_mxf);
  auto video_size = std::filesystem::file_size(video_mxf);

  PklAsset video_asset;
  video_asset.uuid = video_uuid;
  video_asset.hash = video_hash;
  video_asset.size = video_size;
  video_asset.type = "application/mxf";
  video_asset.original_filename = video_mxf.filename().string();
  pkl_opts.assets.push_back(video_asset);

  std::string audio_hash;
  uint64_t audio_size = 0;
  if(has_audio && std::filesystem::exists(audio_mxf))
  {
    audio_hash = sha1_base64(audio_mxf);
    audio_size = std::filesystem::file_size(audio_mxf);
    PklAsset audio_asset;
    audio_asset.uuid = audio_uuid;
    audio_asset.hash = audio_hash;
    audio_asset.size = audio_size;
    audio_asset.type = "application/mxf";
    audio_asset.original_filename = audio_mxf.filename().string();
    pkl_opts.assets.push_back(audio_asset);
  }

  auto pkl_result = generate_pkl(pkl_opts, opts.output_dir);
  result.pkl_uuid = pkl_result.uuid;

  // Build CPL
  CplOptions cpl_opts;
  cpl_opts.title = opts.title;
  cpl_opts.issuer = opts.issuer;
  cpl_opts.content_kind = "feature";
  cpl_opts.edit_rate_num = opts.fps_num;
  cpl_opts.edit_rate_den = opts.fps_den;

  MxfTrackFile vtf;
  vtf.path = video_mxf;
  vtf.uuid = video_uuid;
  vtf.hash = video_hash;
  vtf.size = video_size;
  vtf.duration = result.frame_count;
  cpl_opts.video_tracks.push_back(vtf);

  if(has_audio && std::filesystem::exists(audio_mxf))
  {
    MxfTrackFile atf;
    atf.path = audio_mxf;
    atf.uuid = audio_uuid;
    atf.hash = audio_hash;
    atf.size = audio_size;
    atf.duration = 0;
    cpl_opts.audio_tracks.push_back(atf);
  }

  auto cpl_result = generate_cpl(cpl_opts, opts.output_dir);
  result.cpl_uuid = cpl_result.uuid;

  // Build AssetMap
  AssetMapOptions am_opts;
  am_opts.issuer = opts.issuer;

  AssetMapEntry ve;
  ve.uuid = video_uuid;
  ve.path = video_mxf.filename();
  ve.size = video_size;
  am_opts.assets.push_back(ve);

  if(has_audio && std::filesystem::exists(audio_mxf))
  {
    AssetMapEntry ae;
    ae.uuid = audio_uuid;
    ae.path = audio_mxf.filename();
    ae.size = audio_size;
    am_opts.assets.push_back(ae);
  }

  AssetMapEntry pe;
  pe.uuid = pkl_result.uuid;
  pe.path = pkl_result.path.filename();
  pe.size = pkl_result.size;
  am_opts.assets.push_back(pe);

  AssetMapEntry ce;
  ce.uuid = cpl_result.uuid;
  ce.path = cpl_result.path.filename();
  ce.size = cpl_result.size;
  am_opts.assets.push_back(ce);

  generate_assetmap(am_opts, opts.output_dir);

  result.output_dir = opts.output_dir;
  result.success = true;
  return result;
}

} // namespace imfwizard
