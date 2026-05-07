#include "imfwizard/hdr.h"
#include <sstream>

namespace imfwizard
{

HdrMetadata hdr10_preset()
{
  HdrMetadata m;
  m.transfer = TransferFunction::PQ;
  m.colorimetry = Colorimetry::BT2020;
  m.quantization = Quantization::QE1;
  m.bit_depth = 10;

  MasteringDisplay md;
  // BT.2020 / P3 mastering display primaries (typical HDR10 mastering)
  md.red_x = 34000;
  md.red_y = 16000;
  md.green_x = 13250;
  md.green_y = 34500;
  md.blue_x = 7500;
  md.blue_y = 3000;
  md.white_x = 15635;
  md.white_y = 16450;
  md.max_luminance = 1000;
  md.min_luminance = 50; // 0.005 nits
  m.mastering_display = md;

  ContentLightLevel cl;
  cl.max_cll = 1000;
  cl.max_fall = 400;
  m.content_light = cl;

  return m;
}

HdrMetadata hlg_preset()
{
  HdrMetadata m;
  m.transfer = TransferFunction::HLG;
  m.colorimetry = Colorimetry::BT2020;
  m.quantization = Quantization::QE2;
  m.bit_depth = 10;
  return m;
}

HdrMetadata p3d65_sdr_preset()
{
  HdrMetadata m;
  m.transfer = TransferFunction::SDR_BT1886;
  m.colorimetry = Colorimetry::P3D65;
  m.quantization = Quantization::QE2;
  m.bit_depth = 12;
  return m;
}

static const char* transfer_to_string(TransferFunction tf)
{
  switch(tf)
  {
    case TransferFunction::PQ:
      return "SMPTE-ST-2084";
    case TransferFunction::HLG:
      return "ARIB-STD-B67";
    case TransferFunction::Linear:
      return "Linear";
    default:
      return "BT.1886";
  }
}

static const char* colorimetry_to_string(Colorimetry c)
{
  switch(c)
  {
    case Colorimetry::BT2020:
      return "ITU-R-BT.2020";
    case Colorimetry::P3D65:
      return "P3-D65";
    case Colorimetry::P3DCI:
      return "P3-DCI";
    case Colorimetry::ACES:
      return "ACES";
    default:
      return "ITU-R-BT.709";
  }
}

std::string generate_color_metadata_xml(const HdrMetadata& meta)
{
  std::ostringstream xml;
  xml << "      <r1:Color>\n";
  xml << "        <r1:Colorimetry>" << colorimetry_to_string(meta.colorimetry)
      << "</r1:Colorimetry>\n";
  xml << "        <r1:EOTF>" << transfer_to_string(meta.transfer) << "</r1:EOTF>\n";

  if(meta.mastering_display.has_value())
  {
    const auto& md = meta.mastering_display.value();
    xml << "        <r1:MasteringDisplayPrimaries>\n";
    xml << "          <r1:DisplayPrimaries>\n";
    xml << "            <r1:Red><r1:X>" << md.red_x << "</r1:X><r1:Y>" << md.red_y
        << "</r1:Y></r1:Red>\n";
    xml << "            <r1:Green><r1:X>" << md.green_x << "</r1:X><r1:Y>" << md.green_y
        << "</r1:Y></r1:Green>\n";
    xml << "            <r1:Blue><r1:X>" << md.blue_x << "</r1:X><r1:Y>" << md.blue_y
        << "</r1:Y></r1:Blue>\n";
    xml << "          </r1:DisplayPrimaries>\n";
    xml << "          <r1:WhitePoint><r1:X>" << md.white_x << "</r1:X><r1:Y>" << md.white_y
        << "</r1:Y></r1:WhitePoint>\n";
    xml << "          <r1:MaxDisplayMasteringLuminance>" << md.max_luminance
        << "</r1:MaxDisplayMasteringLuminance>\n";
    xml << "          <r1:MinDisplayMasteringLuminance>" << md.min_luminance
        << "</r1:MinDisplayMasteringLuminance>\n";
    xml << "        </r1:MasteringDisplayPrimaries>\n";
  }

  if(meta.content_light.has_value())
  {
    const auto& cl = meta.content_light.value();
    xml << "        <r1:ContentLightLevel>\n";
    xml << "          <r1:MaxCLL>" << cl.max_cll << "</r1:MaxCLL>\n";
    xml << "          <r1:MaxFALL>" << cl.max_fall << "</r1:MaxFALL>\n";
    xml << "        </r1:ContentLightLevel>\n";
  }

  xml << "      </r1:Color>\n";
  return xml.str();
}

} // namespace imfwizard
