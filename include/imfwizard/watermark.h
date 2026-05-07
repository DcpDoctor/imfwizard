#pragma once

#include <filesystem>
#include <string>
#include <cstdint>

namespace imfwizard {

// Forensic watermarking options
struct WatermarkOptions {
    std::filesystem::path input_dir;    // J2K frame directory
    std::filesystem::path output_dir;   // Watermarked output
    std::string payload;                // Embedded payload (UUID, distributor ID, etc.)
    uint8_t strength = 3;              // 1-5, higher = more visible but more robust
    bool qr_mode = false;              // QR code overlay vs spread-spectrum
    uint32_t interval_frames = 24;     // For QR: embed every N frames
};

struct WatermarkResult {
    uint32_t frames_watermarked = 0;
    std::filesystem::path output_dir;
    bool success = false;
    std::string error;
};

// Apply spread-spectrum watermark to J2K frames
// This embeds invisible data in the wavelet domain
WatermarkResult apply_watermark(const WatermarkOptions& opts);

// Detect watermark payload from watermarked frames
struct DetectResult {
    std::string payload;
    double confidence = 0;  // 0-1
    bool detected = false;
    std::string error;
};

DetectResult detect_watermark(const std::filesystem::path& frame_dir, uint8_t strength = 3);

} // namespace imfwizard
