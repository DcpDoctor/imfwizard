#include "imfwizard/supplemental.h"
#include "imfwizard/info.h"
#include "imfwizard/mxf_wrap.h"
#include "imfwizard/cpl.h"
#include "imfwizard/pkl.h"
#include "imfwizard/assetmap.h"
#include "imfwizard/uuid.h"
#include "imfwizard/hash.h"
#include <spdlog/spdlog.h>
#include <filesystem>
#include <stdexcept>

namespace imfwizard {

SupplementalResult create_supplemental_imp(const SupplementalOptions& opts)
{
    SupplementalResult result;
    result.output_dir = opts.output_dir;

    try {
        // Read the OV IMP to reference its track files
        ImpInfo ov_info = read_imp_info(opts.ov_imp_dir);
        if (!ov_info.valid) {
            result.error = "Cannot read OV IMP: " + ov_info.error;
            return result;
        }

        std::filesystem::create_directories(opts.output_dir);

        // Wrap new replacement segments
        std::vector<MxfTrackFile> new_video_tracks;
        std::vector<MxfTrackFile> new_audio_tracks;

        for (size_t i = 0; i < opts.segments.size(); ++i) {
            const auto& seg = opts.segments[i];

            if (!seg.video_dir.empty() && std::filesystem::exists(seg.video_dir)) {
                WrapOptions video_opts;
                video_opts.type = EssenceType::J2K;
                video_opts.input_dir = seg.video_dir;
                video_opts.output_path = opts.output_dir /
                    ("VID_SUP_" + std::to_string(i) + "_" + generate_uuid_bare() + ".mxf");
                video_opts.edit_rate_num = opts.edit_rate_num;
                video_opts.edit_rate_den = opts.edit_rate_den;

                spdlog::info("Wrapping supplemental video segment {}...", i);
                MxfTrackFile track = wrap_essence(video_opts);
                new_video_tracks.push_back(track);
                result.new_track_files.push_back(track);
            }

            if (!seg.audio_file.empty() && std::filesystem::exists(seg.audio_file)) {
                WrapOptions audio_opts;
                audio_opts.type = EssenceType::WAV;
                audio_opts.input_dir = seg.audio_file;
                audio_opts.output_path = opts.output_dir /
                    ("AUD_SUP_" + std::to_string(i) + "_" + generate_uuid_bare() + ".mxf");
                audio_opts.edit_rate_num = opts.edit_rate_num;
                audio_opts.edit_rate_den = opts.edit_rate_den;
                audio_opts.sample_rate = opts.audio_sample_rate;
                audio_opts.bit_depth = opts.audio_bit_depth;

                spdlog::info("Wrapping supplemental audio segment {}...", i);
                MxfTrackFile track = wrap_essence(audio_opts);
                new_audio_tracks.push_back(track);
                result.new_track_files.push_back(track);
            }
        }

        // Generate supplemental CPL referencing OV tracks + new segments
        CplOptions cpl_opts;
        cpl_opts.title = opts.title;
        cpl_opts.issuer = opts.issuer;
        cpl_opts.content_kind = "feature";
        cpl_opts.edit_rate_num = opts.edit_rate_num;
        cpl_opts.edit_rate_den = opts.edit_rate_den;
        cpl_opts.video_tracks = new_video_tracks;
        cpl_opts.audio_tracks = new_audio_tracks;

        CplResult cpl = generate_cpl(cpl_opts, opts.output_dir);
        result.cpl_uuid = cpl.uuid;

        // Generate PKL for new assets only
        PklOptions pkl_opts;
        pkl_opts.issuer = opts.issuer;

        for (const auto& tf : result.new_track_files) {
            PklAsset asset;
            asset.uuid = tf.uuid;
            asset.hash = tf.hash;
            asset.size = tf.size;
            asset.type = "application/mxf";
            asset.original_filename = tf.path.filename().string();
            pkl_opts.assets.push_back(asset);
        }

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

        // Generate AssetMap for new assets
        AssetMapOptions am_opts;
        am_opts.issuer = opts.issuer;

        for (const auto& tf : result.new_track_files) {
            AssetMapEntry entry;
            entry.uuid = tf.uuid;
            entry.path = tf.path.filename();
            entry.size = tf.size;
            am_opts.assets.push_back(entry);
        }

        {
            AssetMapEntry entry;
            entry.uuid = cpl.uuid;
            entry.path = cpl.path.filename();
            entry.size = cpl.size;
            am_opts.assets.push_back(entry);
        }

        {
            AssetMapEntry entry;
            entry.uuid = pkl.uuid;
            entry.path = pkl.path.filename();
            entry.size = pkl.size;
            am_opts.assets.push_back(entry);
        }

        generate_assetmap(am_opts, opts.output_dir);

        if (opts.sign.has_value()) {
            sign_xml(cpl.path, opts.sign.value());
            sign_xml(pkl.path, opts.sign.value());
        }

        result.success = true;
        spdlog::info("Supplemental IMP created at {}", opts.output_dir.string());

    } catch (const std::exception& e) {
        result.success = false;
        result.error = e.what();
        spdlog::error("Supplemental IMP creation failed: {}", e.what());
    }

    return result;
}

} // namespace imfwizard
