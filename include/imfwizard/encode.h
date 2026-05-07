#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard {

enum class ImageFormat {
    DPX,
    TIFF,
    EXR,
    PNG,
    JPEG,
    BMP,
    J2K,     // Already encoded — pass through
    Unknown
};

enum class ColorSpace {
    BT709,
    BT2020,
    P3D65,
    ACES,
    Unknown
};

struct EncodeOptions {
    std::filesystem::path input_dir;     // Image sequence directory
    std::filesystem::path output_dir;    // Output J2K codestream directory
    ImageFormat format = ImageFormat::Unknown; // Auto-detect if Unknown

    // Resolution / framing
    uint32_t width = 0;      // 0 = source resolution
    uint32_t height = 0;     // 0 = source resolution
    bool crop = false;       // Crop to target aspect
    bool letterbox = false;  // Letterbox to target resolution
    bool scale = false;      // Scale to target resolution

    // J2K encoding parameters
    float target_bitrate_mbps = 250.0f; // Target bitrate in Mbps
    uint32_t num_resolutions = 6;
    uint32_t code_block_width = 32;
    uint32_t code_block_height = 32;
    bool cinema_profile = true;         // Cinema 2K/4K profile

    // Color
    ColorSpace color_space = ColorSpace::BT709;
    uint16_t bit_depth = 12;

    // Performance
    uint32_t num_threads = 0; // 0 = auto
};

struct EncodeResult {
    std::filesystem::path output_dir;
    uint32_t frame_count = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    bool success = false;
    std::string error;
};

// Detect image format from file extension
ImageFormat detect_format(const std::filesystem::path& file);

// Detect image format from a directory of files
ImageFormat detect_sequence_format(const std::filesystem::path& dir);

// Count frames in an image sequence directory
uint32_t count_frames(const std::filesystem::path& dir);

// Encode an image sequence to JPEG 2000 codestreams
// Uses grok library if available, falls back to external process
EncodeResult encode_to_j2k(const EncodeOptions& opts);

} // namespace imfwizard
