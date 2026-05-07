#pragma once

#include "imfwizard/mxf_wrap.h"
#include <filesystem>
#include <string>
#include <vector>

namespace imfwizard {

struct CplOptions {
    std::string title;
    std::string issuer;
    std::string content_kind = "feature";
    uint32_t edit_rate_num = 24;
    uint32_t edit_rate_den = 1;
    std::vector<MxfTrackFile> video_tracks;
    std::vector<MxfTrackFile> audio_tracks;
};

struct CplResult {
    std::string uuid;
    std::filesystem::path path;
    std::string hash;
    uint64_t size = 0;
};

// Generate CPL XML (ST 2067-3)
CplResult generate_cpl(const CplOptions& opts,
                       const std::filesystem::path& output_dir);

} // namespace imfwizard
