#include "imfwizard/frame_compare.h"
#include "imfwizard/info.h"
#include "imfwizard/portable.h"
#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace imfwizard
{

CompareResult compare_imps(const CompareOptions& opts)
{
  CompareResult result;

  if(!fs::exists(opts.imp_a) || !fs::exists(opts.imp_b))
  {
    result.error = "One or both IMP directories do not exist";
    return result;
  }

  // Use ffmpeg to compare frames via PSNR/SSIM
  // Extract video track from each IMP and compare
  auto info_a = read_imp_info(opts.imp_a);
  auto info_b = read_imp_info(opts.imp_b);

  if(!info_a.valid || !info_b.valid)
  {
    result.error = "Failed to read IMP info: " + info_a.error + " / " + info_b.error;
    return result;
  }

  // Find video track files
  fs::path video_a, video_b;
  for(auto& t : info_a.tracks)
  {
    if(t.type == "video")
    {
      video_a = opts.imp_a / t.filename;
      break;
    }
  }
  for(auto& t : info_b.tracks)
  {
    if(t.type == "video")
    {
      video_b = opts.imp_b / t.filename;
      break;
    }
  }

  if(video_a.empty() || video_b.empty())
  {
    result.error = "Cannot find video tracks in one or both IMPs";
    return result;
  }

  // Run ffmpeg PSNR comparison
  std::string cmd = "ffmpeg -i \"" + video_a.string() + "\" -i \"" + video_b.string() +
                    "\" -lavfi psnr=stats_file=/tmp/imfwizard_psnr.txt -f null - 2>&1";

  FILE* pipe = portable_popen(cmd.c_str(), "r");
  if(!pipe)
  {
    result.error = "Failed to run ffmpeg comparison";
    return result;
  }

  std::string output;
  char buf[1024];
  while(fgets(buf, sizeof(buf), pipe))
    output += buf;
  portable_pclose(pipe);

  // Parse PSNR output
  // Look for "average:XX.XX" in the ffmpeg output
  auto avg_pos = output.find("average:");
  if(avg_pos != std::string::npos)
  {
    result.avg_psnr = std::stod(output.substr(avg_pos + 8));
  }

  // Parse per-frame stats if available
  std::ifstream stats("/tmp/imfwizard_psnr.txt");
  std::string line;
  while(std::getline(stats, line))
  {
    // Format: n:FRAME mse_avg:X.XX psnr_avg:X.XX
    uint32_t frame = 0;
    double psnr = 0;
    if(sscanf(line.c_str(), "n:%u %*s psnr_avg:%lf", &frame, &psnr) >= 2)
    {
      result.frames_compared++;
      if(psnr < opts.threshold_psnr)
      {
        FrameDiff diff;
        diff.frame_number = frame;
        diff.psnr = psnr;
        diff.significant = true;
        result.diffs.push_back(diff);
        result.frames_different++;
      }
      if(psnr < result.min_psnr || result.min_psnr == 0)
        result.min_psnr = psnr;
    }
  }
  fs::remove("/tmp/imfwizard_psnr.txt");

  result.identical = (result.frames_different == 0);
  result.success = true;

  if(result.frames_compared > 0 && opts.generate_report && !opts.output_dir.empty())
  {
    fs::create_directories(opts.output_dir);
    std::ofstream report(opts.output_dir / "comparison_report.json");
    report << "{\n";
    report << "  \"frames_compared\": " << result.frames_compared << ",\n";
    report << "  \"frames_different\": " << result.frames_different << ",\n";
    report << "  \"avg_psnr\": " << result.avg_psnr << ",\n";
    report << "  \"min_psnr\": " << result.min_psnr << ",\n";
    report << "  \"identical\": " << (result.identical ? "true" : "false") << "\n";
    report << "}\n";
    result.report_path = opts.output_dir / "comparison_report.json";
  }

  return result;
}

} // namespace imfwizard
