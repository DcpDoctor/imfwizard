#pragma once

#include <filesystem>
#include <string>
#include <cstdint>
#include <vector>

namespace imfwizard {

enum class VideoCodec {
    ProRes,
    DNxHR,
    H264,
    H265,
    AV1,
    Unknown
};

struct TranscodeOptions {
    std::filesystem::path input_file;       // Input video file (ProRes MOV, MXF, etc.)
    std::filesystem::path output_dir;       // Output image sequence directory
    std::string output_format = "tiff";     // Output format: tiff, dpx, exr, png
    uint16_t bit_depth = 16;                // Output bit depth
    uint32_t start_frame = 0;               // 0 = from beginning
    uint32_t end_frame = 0;                 // 0 = to end
    uint32_t threads = 0;                   // 0 = auto
};

struct TranscodeResult {
    std::filesystem::path output_dir;
    uint32_t frame_count = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    double fps = 0.0;
    bool success = false;
    std::string error;
};

// Detect codec of a video file using ffprobe
VideoCodec detect_codec(const std::filesystem::path& file);

// Transcode video file to image sequence using ffmpeg
TranscodeResult transcode_to_sequence(const TranscodeOptions& opts);

// Check if ffmpeg is available
bool ffmpeg_available();

// Check if ffprobe is available
bool ffprobe_available();

} // namespace imfwizard
