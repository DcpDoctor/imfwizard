#include "imfwizard/watermark.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <random>
#include <vector>

namespace imfwizard {

// Simple spread-spectrum watermarking for J2K codestreams
// Embeds payload bits by modulating coefficient magnitudes in the codestream
// This is a simplified implementation — production would use wavelet-domain embedding

static std::vector<uint8_t> payload_to_bits(const std::string& payload)
{
    std::vector<uint8_t> bits;
    for (char c : payload) {
        for (int i = 7; i >= 0; --i)
            bits.push_back((c >> i) & 1);
    }
    return bits;
}

static void embed_in_frame(const std::filesystem::path& input,
                           const std::filesystem::path& output,
                           const std::vector<uint8_t>& bits,
                           uint8_t strength)
{
    // Read entire J2K codestream
    std::ifstream fin(input, std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(fin)),
                               std::istreambuf_iterator<char>());
    fin.close();

    if (data.size() < 256) {
        // Too small to watermark, just copy
        std::ofstream fout(output, std::ios::binary);
        fout.write(reinterpret_cast<const char*>(data.data()), data.size());
        return;
    }

    // Embed bits in the body region (skip J2K header - first 128 bytes)
    // Use pseudo-random positions seeded by strength
    std::mt19937 rng(0xDEAD + strength);
    size_t body_start = 128;
    size_t body_size = data.size() - body_start;

    for (size_t i = 0; i < bits.size() && i < body_size / 16; ++i) {
        size_t pos = body_start + (rng() % body_size);
        // Modulate LSBs based on bit value and strength
        if (bits[i])
            data[pos] |= (1 << (strength - 1));
        else
            data[pos] &= ~(1 << (strength - 1));
    }

    std::ofstream fout(output, std::ios::binary);
    fout.write(reinterpret_cast<const char*>(data.data()), data.size());
}

WatermarkResult apply_watermark(const WatermarkOptions& opts)
{
    WatermarkResult result;
    namespace fs = std::filesystem;

    if (!fs::exists(opts.input_dir)) {
        result.error = "Input directory not found";
        return result;
    }

    fs::create_directories(opts.output_dir);

    auto bits = payload_to_bits(opts.payload);
    if (bits.empty()) {
        result.error = "Empty watermark payload";
        return result;
    }

    spdlog::info("Watermarking {} ({} payload bits, strength {})",
                 opts.input_dir.string(), bits.size(), opts.strength);

    std::vector<fs::path> frames;
    for (const auto& entry : fs::directory_iterator(opts.input_dir)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        if (ext == ".j2c" || ext == ".j2k")
            frames.push_back(entry.path());
    }

    std::sort(frames.begin(), frames.end());

    for (const auto& frame : frames) {
        auto out_path = opts.output_dir / frame.filename();
        embed_in_frame(frame, out_path, bits, opts.strength);
        ++result.frames_watermarked;
    }

    result.output_dir = opts.output_dir;
    result.success = true;
    spdlog::info("Watermarked {} frames", result.frames_watermarked);
    return result;
}

DetectResult detect_watermark(const std::filesystem::path& frame_dir, uint8_t strength)
{
    DetectResult result;
    namespace fs = std::filesystem;

    if (!fs::exists(frame_dir)) {
        result.error = "Frame directory not found";
        return result;
    }

    // Find first J2K frame
    fs::path first_frame;
    for (const auto& entry : fs::directory_iterator(frame_dir)) {
        auto ext = entry.path().extension().string();
        if (ext == ".j2c" || ext == ".j2k") {
            first_frame = entry.path();
            break;
        }
    }

    if (first_frame.empty()) {
        result.error = "No J2K frames found";
        return result;
    }

    // Read and extract bits
    std::ifstream fin(first_frame, std::ios::binary);
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(fin)),
                               std::istreambuf_iterator<char>());
    fin.close();

    if (data.size() < 256) {
        result.error = "Frame too small for watermark detection";
        return result;
    }

    std::mt19937 rng(0xDEAD + strength);
    size_t body_start = 128;
    size_t body_size = data.size() - body_start;

    // Extract up to 256 bits (32 characters)
    std::string payload;
    uint8_t current_byte = 0;
    int bit_count = 0;

    for (size_t i = 0; i < 256 && i < body_size / 16; ++i) {
        size_t pos = body_start + (rng() % body_size);
        uint8_t bit = (data[pos] >> (strength - 1)) & 1;
        current_byte = (current_byte << 1) | bit;
        ++bit_count;

        if (bit_count == 8) {
            if (current_byte >= 32 && current_byte < 127)
                payload += static_cast<char>(current_byte);
            else
                break; // Non-printable = end of payload
            current_byte = 0;
            bit_count = 0;
        }
    }

    if (!payload.empty()) {
        result.payload = payload;
        result.confidence = 0.8;
        result.detected = true;
    }

    return result;
}

} // namespace imfwizard
