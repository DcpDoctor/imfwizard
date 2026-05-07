#include <sstream>

#include "imfwizard/accessibility.h"

namespace imfwizard
{

std::string AccessibilityTrack::get_mca_tag() const
{
  switch(type)
  {
    case AccessibilityType::AudioDescription:
      return "chAD";
    case AccessibilityType::HearingImpaired:
      return "chHI";
    case AccessibilityType::SignLanguage:
      return "chSL";
    case AccessibilityType::Commentary:
      return "chCM";
    case AccessibilityType::ForcedNarrative:
      return "chFN";
  }
  return "chAD";
}

std::string AccessibilityTrack::get_content_kind() const
{
  switch(type)
  {
    case AccessibilityType::AudioDescription:
      return "audio-description";
    case AccessibilityType::HearingImpaired:
      return "hearing-impaired";
    case AccessibilityType::SignLanguage:
      return "sign-language-video";
    case AccessibilityType::Commentary:
      return "commentary";
    case AccessibilityType::ForcedNarrative:
      return "forced-narrative";
  }
  return "audio-description";
}

std::string generate_accessibility_xml(const std::vector<AccessibilityTrack>& tracks)
{
  std::ostringstream xml;
  xml << "      <r0:Accessibility>\n";

  for(const auto& t : tracks)
  {
    xml << "        <r0:AccessibilityService>\n";
    xml << "          <r0:Kind>" << t.get_content_kind() << "</r0:Kind>\n";
    xml << "          <r0:Language>" << t.language << "</r0:Language>\n";
    if(!t.description.empty())
      xml << "          <r0:Description>" << t.description << "</r0:Description>\n";
    xml << "        </r0:AccessibilityService>\n";
  }

  xml << "      </r0:Accessibility>\n";
  return xml.str();
}

} // namespace imfwizard
