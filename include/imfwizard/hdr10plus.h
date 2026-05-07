#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard {

// HDR10+ dynamic metadata (SMPTE ST 2094-40)
struct Hdr10PlusFrame {
    uint32_t frame_number;
    // Tone mapping parameters per frame
    double target_max_luminance;
    double target_min_luminance;
    // Bezier curve knee points
    std::vector<double> bezier_anchors;
};

struct Hdr10PlusMetadata {
    std::vector<Hdr10PlusFrame> frames;
    uint16_t targeted_system_display_max_luminance = 400;
    std::string json_path;      // Path to HDR10+ JSON metadata
};

struct Hdr10PlusResult {
    bool success = false;
    std::string error;
    std::filesystem::path output_file;
};

// Parse HDR10+ JSON metadata file
Hdr10PlusMetadata parse_hdr10plus_json(const std::filesystem::path& json_file);

// Inject HDR10+ metadata into HEVC bitstream using hdr10plus_tool
Hdr10PlusResult inject_hdr10plus(const std::filesystem::path& input_file,
                                  const std::filesystem::path& metadata_json,
                                  const std::filesystem::path& output_file);

// Check if hdr10plus_tool is available
bool hdr10plus_tool_available();

} // namespace imfwizard
