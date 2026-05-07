#pragma once

#include <string>
#include <vector>

namespace imfwizard
{

// Accessibility track types for IMF (ST 2067-2 Annex E)
enum class AccessibilityType
{
  AudioDescription, // AD — visually impaired
  HearingImpaired, // HI — hearing impaired (open captions)
  SignLanguage, // Sign language video track
  Commentary, // Director/creative commentary
  ForcedNarrative, // Forced subtitles for foreign dialogue
};

struct AccessibilityTrack
{
  AccessibilityType type;
  std::string language; // RFC 5646
  std::string description; // Human-readable description

  // For CPL metadata
  std::string get_mca_tag() const;
  std::string get_content_kind() const;
};

// Generate CPL accessibility annotation XML
std::string generate_accessibility_xml(const std::vector<AccessibilityTrack>& tracks);

} // namespace imfwizard
