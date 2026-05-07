#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

// SMPTE ST 377-4 / ST 2067-2 MCA label definitions
enum class McaTagSymbol
{
  // Standard channels
  L, R, C, LFE, Ls, Rs, // 5.1
  Lss, Rss, Lrs, Rrs, // 7.1
  Lt, Rt, // Stereo downmix
  M1, M2, // Mono
  // Height channels
  Ltf, Rtf, Ltr, Rtr, // Top
  // Descriptive
  VI, // Visually Impaired (audio description)
  HI, // Hearing Impaired
};

struct McaLabel
{
  McaTagSymbol symbol;
  std::string tag_name; // e.g. "Left", "Right", "Center"
  std::string tag_symbol; // e.g. "chL", "chR", "chC"
  uint32_t channel_index = 0; // 0-based
  std::string spoken_language; // RFC 5646 language tag (optional)
};

struct McaSoundfield
{
  std::string name; // e.g. "51", "71", "20", "Atmos"
  std::vector<McaLabel> channels;
};

// Standard soundfield definitions
McaSoundfield soundfield_stereo();
McaSoundfield soundfield_51();
McaSoundfield soundfield_71();
McaSoundfield soundfield_51_with_hi_vi(); // 5.1 + HI + VI

// Get MCA tag name from symbol
std::string mca_tag_name(McaTagSymbol symbol);

// Get MCA tag symbol string from enum
std::string mca_tag_symbol_string(McaTagSymbol symbol);

// Generate MCA subdescriptor XML for CPL
std::string generate_mca_xml(const McaSoundfield& sf);

// Auto-detect soundfield from channel count
McaSoundfield detect_soundfield(uint32_t channel_count);

} // namespace imfwizard
