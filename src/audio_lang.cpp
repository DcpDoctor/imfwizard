#include <sstream>

#include "imfwizard/audio_lang.h"

namespace imfwizard
{

std::string soundfield_group_xml(const AudioLanguageTrack& track)
{
  std::ostringstream xml;
  xml << "        <r0:EssenceDescriptor>\n";
  xml << "          <r0:RFC5646SpokenLanguage>" << track.language
      << "</r0:RFC5646SpokenLanguage>\n";
  xml << "          <r0:MCATagSymbol>" << track.mca_tag_symbol << "</r0:MCATagSymbol>\n";
  xml << "          <r0:MCAChannelID>0</r0:MCAChannelID>\n";
  xml << "        </r0:EssenceDescriptor>\n";
  return xml.str();
}

} // namespace imfwizard
