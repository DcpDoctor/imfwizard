#include "imfwizard/imp.h"
#include "imfwizard/mxf_wrap.h"
#include "imfwizard/timed_text.h"
#include "imfwizard/encode.h"
#include "imfwizard/cpl.h"
#include "imfwizard/pkl.h"
#include "imfwizard/assetmap.h"
#include "imfwizard/uuid.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <stdexcept>

namespace imfwizard
{

ImpResult create_ov_imp(const ImpOptions& opts)
{
  ImpResult result;
  result.output_dir = opts.output_dir;

  try
  {
    std::filesystem::create_directories(opts.output_dir);

    // Auto-encode if input is not J2K
    std::filesystem::path j2k_dir = opts.video_dir;
    auto seq_format = detect_sequence_format(opts.video_dir);
    if(seq_format != ImageFormat::J2K && seq_format != ImageFormat::Unknown)
    {
      spdlog::info("Input is {} — encoding to J2K first...", static_cast<int>(seq_format));
      EncodeOptions enc_opts;
      enc_opts.input_dir = opts.video_dir;
      enc_opts.output_dir = opts.output_dir / "j2k_encoded";
      enc_opts.cinema_profile = true;
      auto enc_result = encode_to_j2k(enc_opts);
      if(!enc_result.success)
      {
        result.error = "Encoding failed: " + enc_result.error;
        return result;
      }
      j2k_dir = enc_result.output_dir;
    }

    // Wrap video
    WrapOptions video_opts;
    video_opts.type = EssenceType::J2K;
    video_opts.input_dir = j2k_dir;
    video_opts.output_path = opts.output_dir / ("VID_" + generate_uuid_bare() + ".mxf");
    video_opts.edit_rate_num = opts.edit_rate_num;
    video_opts.edit_rate_den = opts.edit_rate_den;

    spdlog::info("Wrapping video essence...");
    MxfTrackFile video_track = wrap_essence(video_opts);
    result.track_files.push_back(video_track);

    // Wrap audio (if provided)
    std::vector<MxfTrackFile> audio_tracks;
    if(!opts.audio_file.empty() && std::filesystem::exists(opts.audio_file))
    {
      WrapOptions audio_opts;
      audio_opts.type = EssenceType::WAV;
      audio_opts.input_dir = opts.audio_file;
      audio_opts.output_path = opts.output_dir / ("AUD_" + generate_uuid_bare() + ".mxf");
      audio_opts.edit_rate_num = opts.edit_rate_num;
      audio_opts.edit_rate_den = opts.edit_rate_den;
      audio_opts.sample_rate = opts.audio_sample_rate;
      audio_opts.bit_depth = opts.audio_bit_depth;

      spdlog::info("Wrapping audio essence...");
      MxfTrackFile audio_track = wrap_essence(audio_opts);
      audio_tracks.push_back(audio_track);
      result.track_files.push_back(audio_track);
    }

    // Wrap subtitle (if provided)
    if(!opts.subtitle_file.empty() && std::filesystem::exists(opts.subtitle_file))
    {
      TimedTextOptions tt_opts;
      tt_opts.ttml_file = opts.subtitle_file;
      tt_opts.output_path = opts.output_dir / ("SUB_" + generate_uuid_bare() + ".mxf");
      tt_opts.edit_rate_num = opts.edit_rate_num;
      tt_opts.edit_rate_den = opts.edit_rate_den;

      spdlog::info("Wrapping subtitle...");
      TimedTextTrackFile sub_track = wrap_timed_text(tt_opts);
      // Add as a generic track file for PKL/AssetMap
      MxfTrackFile sub_mxf;
      sub_mxf.path = sub_track.path;
      sub_mxf.uuid = sub_track.uuid;
      sub_mxf.hash = sub_track.hash;
      sub_mxf.size = sub_track.size;
      sub_mxf.duration = sub_track.duration;
      result.track_files.push_back(sub_mxf);
    }

    // Generate CPL
    CplOptions cpl_opts;
    cpl_opts.title = opts.title;
    cpl_opts.issuer = opts.issuer;
    cpl_opts.content_kind = opts.content_kind;
    cpl_opts.edit_rate_num = opts.edit_rate_num;
    cpl_opts.edit_rate_den = opts.edit_rate_den;
    cpl_opts.video_tracks = {video_track};
    cpl_opts.audio_tracks = audio_tracks;

    CplResult cpl = generate_cpl(cpl_opts, opts.output_dir);
    result.cpl_uuid = cpl.uuid;

    // Generate PKL
    PklOptions pkl_opts;
    pkl_opts.issuer = opts.issuer;

    // Add MXF track files to PKL
    for(const auto& tf : result.track_files)
    {
      PklAsset asset;
      asset.uuid = tf.uuid;
      asset.hash = tf.hash;
      asset.size = tf.size;
      asset.type = "application/mxf";
      asset.original_filename = tf.path.filename().string();
      pkl_opts.assets.push_back(asset);
    }

    // Add CPL to PKL
    {
      PklAsset asset;
      asset.uuid = cpl.uuid;
      asset.hash = cpl.hash;
      asset.size = cpl.size;
      asset.type = "text/xml";
      asset.original_filename = cpl.path.filename().string();
      pkl_opts.assets.push_back(asset);
    }

    PklResult pkl = generate_pkl(pkl_opts, opts.output_dir);
    result.pkl_uuid = pkl.uuid;

    // Generate AssetMap
    AssetMapOptions am_opts;
    am_opts.issuer = opts.issuer;

    for(const auto& tf : result.track_files)
    {
      AssetMapEntry entry;
      entry.uuid = tf.uuid;
      entry.path = tf.path.filename();
      entry.size = tf.size;
      am_opts.assets.push_back(entry);
    }

    // CPL entry
    {
      AssetMapEntry entry;
      entry.uuid = cpl.uuid;
      entry.path = cpl.path.filename();
      entry.size = cpl.size;
      am_opts.assets.push_back(entry);
    }

    // PKL entry
    {
      AssetMapEntry entry;
      entry.uuid = pkl.uuid;
      entry.path = pkl.path.filename();
      entry.size = pkl.size;
      am_opts.assets.push_back(entry);
    }

    generate_assetmap(am_opts, opts.output_dir);

    // Optional signing
    if(opts.sign.has_value())
    {
      sign_xml(cpl.path, opts.sign.value());
      sign_xml(pkl.path, opts.sign.value());
    }

    result.success = true;
    spdlog::info("IMP created successfully at {}", opts.output_dir.string());
  }
  catch(const std::exception& e)
  {
    result.success = false;
    result.error = e.what();
    spdlog::error("IMP creation failed: {}", e.what());
  }

  return result;
}

} // namespace imfwizard
