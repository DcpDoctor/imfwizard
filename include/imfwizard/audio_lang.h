#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard {

// Audio track with language metadata for multi-language IMPs
struct AudioLanguageTrack {
    std::filesystem::path file;         // WAV file path
    std::string language;               // RFC 5646 language tag (e.g. "en", "fr", "de")
    std::string mca_tag_symbol;         // MCA tag (e.g. "chL", "chR" for stereo)
    uint32_t channel_count = 2;
};

// Multi-language audio configuration
struct MultiLangAudioOptions {
    std::vector<AudioLanguageTrack> tracks;
    uint32_t sample_rate = 48000;
    uint16_t bit_depth = 24;
};

// Generate RFC 5646 soundfield group label for CPL
std::string soundfield_group_xml(const AudioLanguageTrack& track);

} // namespace imfwizard
