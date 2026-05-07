#include <spdlog/spdlog.h>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "imfwizard/lut.h"
#include "imfwizard/portable.h"

namespace fs = std::filesystem;

namespace imfwizard
{

bool validate_cube_lut(const fs::path& lut_file)
{
  if(!fs::exists(lut_file))
    return false;

  std::ifstream f(lut_file);
  std::string line;
  bool found_size = false;
  while(std::getline(f, line))
  {
    if(line.find("LUT_3D_SIZE") != std::string::npos)
    {
      found_size = true;
      break;
    }
  }
  return found_size;
}

LutResult apply_lut(const LutOptions& opts)
{
  LutResult result;

  if(!fs::exists(opts.input_dir))
  {
    result.error = "Input directory not found: " + opts.input_dir.string();
    return result;
  }

  if(!fs::exists(opts.lut_file))
  {
    result.error = "LUT file not found: " + opts.lut_file.string();
    return result;
  }

  if(!validate_cube_lut(opts.lut_file))
  {
    result.error = "Invalid .cube LUT file: " + opts.lut_file.string();
    return result;
  }

  fs::create_directories(opts.output_dir);

  // Count input frames
  uint32_t frame_count = 0;
  for(auto& entry : fs::directory_iterator(opts.input_dir))
  {
    auto ext = entry.path().extension().string();
    if(ext == ".tiff" || ext == ".tif" || ext == ".dpx" || ext == ".exr" || ext == ".png")
      frame_count++;
  }

  if(frame_count == 0)
  {
    result.error = "No image files found in input directory";
    return result;
  }

  // Use ffmpeg to apply 3D LUT to image sequence
  // Detect input format
  std::string input_pattern;
  for(auto& entry : fs::directory_iterator(opts.input_dir))
  {
    auto ext = entry.path().extension().string();
    if(ext == ".tiff" || ext == ".tif" || ext == ".dpx" || ext == ".exr" || ext == ".png")
    {
      // Construct ffmpeg glob pattern
      input_pattern = (opts.input_dir / ("*" + ext)).string();
      break;
    }
  }

  std::string output_ext = ".tiff";
  std::string codec = "tiff";
  if(opts.bit_depth == 16)
  {
    codec = "tiff -pix_fmt rgb48le";
  }

  uint32_t threads = opts.threads ? opts.threads : 0;
  std::string thread_opt = threads ? " -threads " + std::to_string(threads) : "";

  std::string cmd = "ffmpeg -y -pattern_type glob -i \"" + input_pattern +
                    "\" -vf \"lut3d='" + opts.lut_file.string() + "'\"" + thread_opt + " -c:v " +
                    codec + " \"" + (opts.output_dir / ("frame_%06d" + output_ext)).string() +
                    "\" 2>/dev/null";

  int ret = system(cmd.c_str());
  if(ret != 0)
  {
    result.error = "ffmpeg LUT application failed";
    return result;
  }

  result.output_dir = opts.output_dir;
  result.frames_processed = frame_count;
  result.success = true;
  return result;
}

} // namespace imfwizard
