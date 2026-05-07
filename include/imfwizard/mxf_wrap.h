#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard {

struct MxfTrackFile {
    std::filesystem::path path;
    std::string uuid;
    std::string hash;       // SHA-1 base64
    uint64_t size = 0;
    uint32_t duration = 0;  // frames or samples
};

enum class EssenceType {
    J2K,    // JPEG 2000 codestream
    WAV,    // PCM audio
};

struct WrapOptions {
    EssenceType type;
    std::filesystem::path input_dir;   // directory of J2K frames or single WAV
    std::filesystem::path output_path; // output .mxf path
    uint32_t edit_rate_num = 24;
    uint32_t edit_rate_den = 1;
    // Audio-specific
    uint32_t sample_rate = 48000;
    uint16_t bit_depth = 24;
    uint16_t channels = 0; // auto-detect from WAV if 0
};

// Wrap essence into AS-02 MXF track file
MxfTrackFile wrap_essence(const WrapOptions& opts);

} // namespace imfwizard
