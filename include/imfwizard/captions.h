#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard {

// CEA-608/708 closed caption support
enum class CaptionFormat {
    SCC,        // CEA-608 Scenarist Closed Captions
    MCC,        // CEA-708 MacCaption format
    SRT,        // SubRip (convert to TTML for IMF)
    STL,        // EBU STL
};

struct CaptionEntry {
    uint32_t start_frame;
    uint32_t end_frame;
    std::string text;
    int channel = 1;    // CC1-CC4 for 608
};

struct CaptionParseResult {
    std::vector<CaptionEntry> entries;
    CaptionFormat format;
    bool success = false;
    std::string error;
};

// Parse a caption file
CaptionParseResult parse_captions(const std::filesystem::path& file, uint32_t fps = 24);

// Convert captions to TTML for IMF packaging
std::string captions_to_ttml(const std::vector<CaptionEntry>& entries,
                             uint32_t fps_num = 24, uint32_t fps_den = 1);

// Write TTML caption file
void write_ttml_captions(const std::vector<CaptionEntry>& entries,
                         const std::filesystem::path& output,
                         uint32_t fps_num = 24, uint32_t fps_den = 1);

} // namespace imfwizard
