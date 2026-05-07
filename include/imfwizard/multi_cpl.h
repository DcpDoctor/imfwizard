#pragma once

#include "imfwizard/imp.h"
#include "imfwizard/cpl.h"
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace imfwizard {

struct MultiCplEntry {
    CplOptions cpl_opts;
    // Which tracks from the shared track pool to reference
    std::vector<size_t> video_track_indices;
    std::vector<size_t> audio_track_indices;
};

struct MultiCplImpOptions {
    std::string issuer;
    std::filesystem::path output_dir;

    // Shared track pool — all wrapped track files
    std::vector<MxfTrackFile> track_files;

    // Multiple CPLs referencing subsets of tracks
    std::vector<MultiCplEntry> cpls;

    std::optional<SignOptions> sign;
};

struct MultiCplImpResult {
    std::filesystem::path output_dir;
    std::vector<std::string> cpl_uuids;
    std::string pkl_uuid;
    bool success = false;
    std::string error;
};

// Create a multi-CPL IMP from already-wrapped track files
MultiCplImpResult create_multi_cpl_imp(const MultiCplImpOptions& opts);

} // namespace imfwizard
