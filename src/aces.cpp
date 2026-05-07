#include "imfwizard/aces.h"
#include "imfwizard/portable.h"
#include <spdlog/spdlog.h>

#include <cstdio>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace imfwizard
{

bool ctl_available()
{
#ifdef _WIN32
  return system("where ctlrender >NUL 2>&1") == 0;
#else
  return system("which ctlrender >/dev/null 2>&1") == 0;
#endif
}

std::vector<std::string> list_available_idts()
{
  return {"ARRI_LogC4",     "ARRI_LogC3",  "RED_Log3G10", "Sony_SLog3",
          "Canon_CLog3",    "Panasonic_VLog", "BMD_Film_Gen5",
          "Generic_Linear", "Generic_sRGB"};
}

std::vector<std::string> list_available_odts()
{
  return {"Rec709_100nits",    "P3D65_108nits",      "P3D65_PQ_1000nits",
          "P3D65_PQ_4000nits", "Rec2020_PQ_1000nits", "Rec2020_PQ_4000nits",
          "Rec2020_HLG_1000nits", "DCDM",            "DCDM_P3D65"};
}

AcesResult apply_aces_pipeline(const AcesOptions& opts)
{
  AcesResult result;

  if(!fs::exists(opts.input_dir))
  {
    result.error = "Input directory not found: " + opts.input_dir.string();
    return result;
  }

  fs::create_directories(opts.output_dir);

  // If ctlrender is available, use CTL transforms
  if(ctl_available() && !opts.ctl_dir.empty() && fs::exists(opts.ctl_dir))
  {
    // Build ctlrender command chain: IDT → RRT → ODT
    auto idt_ctl = opts.ctl_dir / "idt" / (opts.idt_name + ".ctl");
    auto rrt_ctl = opts.ctl_dir / "rrt" / "RRT.ctl";
    auto odt_ctl = opts.ctl_dir / "odt" / (opts.odt_name + ".ctl");

    uint32_t count = 0;
    for(auto& entry : fs::directory_iterator(opts.input_dir))
    {
      auto ext = entry.path().extension().string();
      if(ext != ".exr" && ext != ".tiff" && ext != ".tif" && ext != ".dpx")
        continue;

      auto output_file = opts.output_dir / entry.path().filename();
      std::string cmd = "ctlrender";
      if(fs::exists(idt_ctl))
        cmd += " -ctl \"" + idt_ctl.string() + "\"";
      if(fs::exists(rrt_ctl))
        cmd += " -ctl \"" + rrt_ctl.string() + "\"";
      if(fs::exists(odt_ctl))
        cmd += " -ctl \"" + odt_ctl.string() + "\"";
      cmd += " \"" + entry.path().string() + "\" \"" + output_file.string() + "\" 2>/dev/null";

      if(system(cmd.c_str()) == 0)
        count++;
    }

    result.frames_processed = count;
  }
  else
  {
    // Fallback: use ffmpeg colorspace filters to approximate ACES pipeline
    std::string colorspace_filter;

    // Map IDT to input colorspace
    std::string in_cs = "bt709";
    if(opts.idt_name.find("Log") != std::string::npos ||
       opts.idt_name.find("SLog") != std::string::npos)
      in_cs = "bt2020nc";

    // Map ODT to output colorspace
    std::string out_cs = "bt709";
    std::string out_trc = "bt709";
    if(opts.odt_name.find("PQ") != std::string::npos)
    {
      out_cs = "bt2020nc";
      out_trc = "smpte2084";
    }
    else if(opts.odt_name.find("HLG") != std::string::npos)
    {
      out_cs = "bt2020nc";
      out_trc = "arib-std-b67";
    }
    else if(opts.odt_name.find("P3") != std::string::npos)
    {
      out_cs = "bt2020nc"; // approximate
      out_trc = "smpte2084";
    }

    // Find input pattern
    std::string input_pattern;
    for(auto& entry : fs::directory_iterator(opts.input_dir))
    {
      auto ext = entry.path().extension().string();
      if(ext == ".exr" || ext == ".tiff" || ext == ".tif" || ext == ".dpx")
      {
        input_pattern = (opts.input_dir / ("*" + ext)).string();
        break;
      }
    }

    if(input_pattern.empty())
    {
      result.error = "No image files found in input directory";
      return result;
    }

    std::string cmd =
        "ffmpeg -y -pattern_type glob -i \"" + input_pattern +
        "\" -vf \"colorspace=all=" + out_cs + ":trc=" + out_trc +
        "\" -c:v tiff -pix_fmt rgb48le \"" +
        (opts.output_dir / "frame_%06d.tiff").string() + "\" 2>/dev/null";

    int ret = system(cmd.c_str());
    if(ret != 0)
    {
      result.error = "ffmpeg ACES color transform failed";
      return result;
    }

    // Count output frames
    for(auto& entry : fs::directory_iterator(opts.output_dir))
    {
      if(entry.path().extension() == ".tiff")
        result.frames_processed++;
    }
  }

  result.output_dir = opts.output_dir;
  result.idt_applied = opts.idt_name;
  result.odt_applied = opts.odt_name;

  // Set output HDR metadata based on ODT
  if(opts.odt_name.find("PQ") != std::string::npos)
  {
    result.output_hdr.transfer = TransferFunction::PQ;
    result.output_hdr.colorimetry = Colorimetry::BT2020;
  }
  else if(opts.odt_name.find("HLG") != std::string::npos)
  {
    result.output_hdr.transfer = TransferFunction::HLG;
    result.output_hdr.colorimetry = Colorimetry::BT2020;
  }
  else if(opts.odt_name.find("P3") != std::string::npos)
  {
    result.output_hdr.transfer = TransferFunction::PQ;
    result.output_hdr.colorimetry = Colorimetry::P3D65;
  }

  result.success = (result.frames_processed > 0);
  if(!result.success && result.error.empty())
    result.error = "No frames processed";
  return result;
}

} // namespace imfwizard
