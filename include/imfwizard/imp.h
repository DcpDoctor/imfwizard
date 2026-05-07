#pragma once

#include "imfwizard/mxf_wrap.h"
#include "imfwizard/cpl.h"
#include "imfwizard/pkl.h"
#include "imfwizard/signature.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace imfwizard {

struct ImpOptions {
    std::string title;
    std::string issuer;
    std::string content_kind = "feature";

    // Input essence
    std::filesystem::path video_dir;   // directory of J2K codestreams
    std::filesystem::path audio_file;  // WAV file
    std::filesystem::path subtitle_file; // TTML/IMSC file (optional)

    // Timing
    uint32_t edit_rate_num = 24;
    uint32_t edit_rate_den = 1;
    uint32_t audio_sample_rate = 48000;
    uint16_t audio_bit_depth = 24;

    // Output
    std::filesystem::path output_dir;

    // Optional signing
    std::optional<SignOptions> sign;
};

struct ImpResult {
    std::filesystem::path output_dir;
    std::string cpl_uuid;
    std::string pkl_uuid;
    std::vector<MxfTrackFile> track_files;
    bool success = false;
    std::string error;
};

// Create a complete Original Version IMP (App#2E)
ImpResult create_ov_imp(const ImpOptions& opts);

} // namespace imfwizard
