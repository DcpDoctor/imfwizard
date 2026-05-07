#include <sstream>

#include "imfwizard/mca.h"

namespace imfwizard
{

std::string mca_tag_name(McaTagSymbol symbol)
{
  switch(symbol)
  {
  case McaTagSymbol::L: return "Left";
  case McaTagSymbol::R: return "Right";
  case McaTagSymbol::C: return "Center";
  case McaTagSymbol::LFE: return "LFE";
  case McaTagSymbol::Ls: return "Left Surround";
  case McaTagSymbol::Rs: return "Right Surround";
  case McaTagSymbol::Lss: return "Left Side Surround";
  case McaTagSymbol::Rss: return "Right Side Surround";
  case McaTagSymbol::Lrs: return "Left Rear Surround";
  case McaTagSymbol::Rrs: return "Right Rear Surround";
  case McaTagSymbol::Lt: return "Left Total";
  case McaTagSymbol::Rt: return "Right Total";
  case McaTagSymbol::M1: return "Mono One";
  case McaTagSymbol::M2: return "Mono Two";
  case McaTagSymbol::Ltf: return "Left Top Front";
  case McaTagSymbol::Rtf: return "Right Top Front";
  case McaTagSymbol::Ltr: return "Left Top Rear";
  case McaTagSymbol::Rtr: return "Right Top Rear";
  case McaTagSymbol::VI: return "Visually Impaired";
  case McaTagSymbol::HI: return "Hearing Impaired";
  }
  return "Unknown";
}

std::string mca_tag_symbol_string(McaTagSymbol symbol)
{
  switch(symbol)
  {
  case McaTagSymbol::L: return "chL";
  case McaTagSymbol::R: return "chR";
  case McaTagSymbol::C: return "chC";
  case McaTagSymbol::LFE: return "chLFE";
  case McaTagSymbol::Ls: return "chLs";
  case McaTagSymbol::Rs: return "chRs";
  case McaTagSymbol::Lss: return "chLss";
  case McaTagSymbol::Rss: return "chRss";
  case McaTagSymbol::Lrs: return "chLrs";
  case McaTagSymbol::Rrs: return "chRrs";
  case McaTagSymbol::Lt: return "chLt";
  case McaTagSymbol::Rt: return "chRt";
  case McaTagSymbol::M1: return "chM1";
  case McaTagSymbol::M2: return "chM2";
  case McaTagSymbol::Ltf: return "chLtf";
  case McaTagSymbol::Rtf: return "chRtf";
  case McaTagSymbol::Ltr: return "chLtr";
  case McaTagSymbol::Rtr: return "chRtr";
  case McaTagSymbol::VI: return "chVIN";
  case McaTagSymbol::HI: return "chHI";
  }
  return "chUnknown";
}

McaSoundfield soundfield_stereo()
{
  McaSoundfield sf;
  sf.name = "20";
  sf.channels = {{McaTagSymbol::L, "Left", "chL", 0, ""},
                 {McaTagSymbol::R, "Right", "chR", 1, ""}};
  return sf;
}

McaSoundfield soundfield_51()
{
  McaSoundfield sf;
  sf.name = "51";
  sf.channels = {{McaTagSymbol::L, "Left", "chL", 0, ""},
                 {McaTagSymbol::R, "Right", "chR", 1, ""},
                 {McaTagSymbol::C, "Center", "chC", 2, ""},
                 {McaTagSymbol::LFE, "LFE", "chLFE", 3, ""},
                 {McaTagSymbol::Ls, "Left Surround", "chLs", 4, ""},
                 {McaTagSymbol::Rs, "Right Surround", "chRs", 5, ""}};
  return sf;
}

McaSoundfield soundfield_71()
{
  McaSoundfield sf;
  sf.name = "71";
  sf.channels = {{McaTagSymbol::L, "Left", "chL", 0, ""},
                 {McaTagSymbol::R, "Right", "chR", 1, ""},
                 {McaTagSymbol::C, "Center", "chC", 2, ""},
                 {McaTagSymbol::LFE, "LFE", "chLFE", 3, ""},
                 {McaTagSymbol::Ls, "Left Surround", "chLs", 4, ""},
                 {McaTagSymbol::Rs, "Right Surround", "chRs", 5, ""},
                 {McaTagSymbol::Lrs, "Left Rear Surround", "chLrs", 6, ""},
                 {McaTagSymbol::Rrs, "Right Rear Surround", "chRrs", 7, ""}};
  return sf;
}

McaSoundfield soundfield_51_with_hi_vi()
{
  auto sf = soundfield_51();
  sf.name = "51+HI+VI";
  sf.channels.push_back({McaTagSymbol::HI, "Hearing Impaired", "chHI", 6, ""});
  sf.channels.push_back({McaTagSymbol::VI, "Visually Impaired", "chVIN", 7, ""});
  return sf;
}

McaSoundfield detect_soundfield(uint32_t channel_count)
{
  switch(channel_count)
  {
  case 1:
  {
    McaSoundfield sf;
    sf.name = "10";
    sf.channels = {{McaTagSymbol::M1, "Mono One", "chM1", 0, ""}};
    return sf;
  }
  case 2: return soundfield_stereo();
  case 6: return soundfield_51();
  case 8: return soundfield_51_with_hi_vi();
  default: return soundfield_71();
  }
}

std::string generate_mca_xml(const McaSoundfield& sf)
{
  std::ostringstream xml;
  xml << "  <r0:MCALabelSubDescriptors>\n";
  xml << "    <r0:SoundfieldGroupLabelSubDescriptor>\n";
  xml << "      <r0:MCATagSymbol>sg" << sf.name << "</r0:MCATagSymbol>\n";
  xml << "      <r0:MCATagName>Soundfield " << sf.name << "</r0:MCATagName>\n";
  xml << "    </r0:SoundfieldGroupLabelSubDescriptor>\n";

  for(auto& ch : sf.channels)
  {
    xml << "    <r0:AudioChannelLabelSubDescriptor>\n";
    xml << "      <r0:MCAChannelID>" << (ch.channel_index + 1) << "</r0:MCAChannelID>\n";
    xml << "      <r0:MCATagSymbol>" << ch.tag_symbol << "</r0:MCATagSymbol>\n";
    xml << "      <r0:MCATagName>" << ch.tag_name << "</r0:MCATagName>\n";
    if(!ch.spoken_language.empty())
      xml << "      <r0:RFC5646SpokenLanguage>" << ch.spoken_language
          << "</r0:RFC5646SpokenLanguage>\n";
    xml << "    </r0:AudioChannelLabelSubDescriptor>\n";
  }

  xml << "  </r0:MCALabelSubDescriptors>\n";
  return xml.str();
}

} // namespace imfwizard
