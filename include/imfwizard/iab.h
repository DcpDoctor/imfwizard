#pragma once

#include <filesystem>
#include <string>
#include <cstdint>

namespace imfwizard {

// SMPTE ST 2098 Immersive Audio Bitstream (IAB) support
// Used for Dolby Atmos in IMF packages
struct IabWrapOptions {
    std::filesystem::path input_file;       // IAB bitstream file (.atmos or .iab)
    std::filesystem::path output_dir;
    uint32_t edit_rate_num = 24;
    uint32_t edit_rate_den = 1;
    uint32_t duration = 0;                  // 0 = auto-detect
};

struct IabWrapResult {
    std::filesystem::path mxf_path;
    std::string uuid;
    uint64_t size = 0;
    uint32_t duration = 0;
    bool success = false;
    std::string error;
};

// Wrap an IAB bitstream into AS-02 MXF track file
IabWrapResult wrap_iab(const IabWrapOptions& opts);

// Detect if a file is a valid IAB bitstream
bool is_iab_file(const std::filesystem::path& file);

} // namespace imfwizard
