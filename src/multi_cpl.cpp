#include <spdlog/spdlog.h>
#include <stdexcept>

#include "imfwizard/multi_cpl.h"
#include "imfwizard/pkl.h"
#include "imfwizard/assetmap.h"
#include "imfwizard/uuid.h"
#include "imfwizard/hash.h"

namespace imfwizard
{

MultiCplImpResult create_multi_cpl_imp(const MultiCplImpOptions& opts)
{
  MultiCplImpResult result;
  result.output_dir = opts.output_dir;

  try
  {
    std::filesystem::create_directories(opts.output_dir);

    PklOptions pkl_opts;
    pkl_opts.issuer = opts.issuer;

    AssetMapOptions am_opts;
    am_opts.issuer = opts.issuer;

    // Add track files to PKL and AssetMap
    for(const auto& tf : opts.track_files)
    {
      PklAsset pa;
      pa.uuid = tf.uuid;
      pa.hash = tf.hash;
      pa.size = tf.size;
      pa.type = "application/mxf";
      pa.original_filename = tf.path.filename().string();
      pkl_opts.assets.push_back(pa);

      AssetMapEntry ae;
      ae.uuid = tf.uuid;
      ae.path = tf.path.filename();
      ae.size = tf.size;
      am_opts.assets.push_back(ae);
    }

    // Generate each CPL
    for(const auto& entry : opts.cpls)
    {
      CplOptions cpl_opts = entry.cpl_opts;

      // Resolve track references
      cpl_opts.video_tracks.clear();
      for(size_t idx : entry.video_track_indices)
      {
        if(idx < opts.track_files.size())
          cpl_opts.video_tracks.push_back(opts.track_files[idx]);
      }
      cpl_opts.audio_tracks.clear();
      for(size_t idx : entry.audio_track_indices)
      {
        if(idx < opts.track_files.size())
          cpl_opts.audio_tracks.push_back(opts.track_files[idx]);
      }

      CplResult cpl = generate_cpl(cpl_opts, opts.output_dir);
      result.cpl_uuids.push_back(cpl.uuid);

      // Add CPL to PKL/AssetMap
      PklAsset pa;
      pa.uuid = cpl.uuid;
      pa.hash = cpl.hash;
      pa.size = cpl.size;
      pa.type = "text/xml";
      pa.original_filename = cpl.path.filename().string();
      pkl_opts.assets.push_back(pa);

      AssetMapEntry ae;
      ae.uuid = cpl.uuid;
      ae.path = cpl.path.filename();
      ae.size = cpl.size;
      am_opts.assets.push_back(ae);

      if(opts.sign.has_value())
        sign_xml(cpl.path, opts.sign.value());
    }

    // Generate PKL
    PklResult pkl = generate_pkl(pkl_opts, opts.output_dir);
    result.pkl_uuid = pkl.uuid;

    // Add PKL to AssetMap
    AssetMapEntry pkl_entry;
    pkl_entry.uuid = pkl.uuid;
    pkl_entry.path = pkl.path.filename();
    pkl_entry.size = pkl.size;
    am_opts.assets.push_back(pkl_entry);

    if(opts.sign.has_value())
      sign_xml(pkl.path, opts.sign.value());

    // Generate AssetMap
    generate_assetmap(am_opts, opts.output_dir);

    result.success = true;
    spdlog::info("Multi-CPL IMP created with {} CPLs", result.cpl_uuids.size());
  }
  catch(const std::exception& e)
  {
    result.success = false;
    result.error = e.what();
    spdlog::error("Multi-CPL IMP creation failed: {}", e.what());
  }

  return result;
}

} // namespace imfwizard
