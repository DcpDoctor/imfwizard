#include "imfwizard/encode.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

namespace imfwizard
{

ImageFormat detect_format(const fs::path& file)
{
  auto ext = file.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  if(ext == ".dpx")
    return ImageFormat::DPX;
  if(ext == ".tif" || ext == ".tiff")
    return ImageFormat::TIFF;
  if(ext == ".exr")
    return ImageFormat::EXR;
  if(ext == ".png")
    return ImageFormat::PNG;
  if(ext == ".jpg" || ext == ".jpeg")
    return ImageFormat::JPEG;
  if(ext == ".bmp")
    return ImageFormat::BMP;
  if(ext == ".j2c" || ext == ".j2k" || ext == ".jp2")
    return ImageFormat::J2K;
  return ImageFormat::Unknown;
}

ImageFormat detect_sequence_format(const fs::path& dir)
{
  for(const auto& entry : fs::directory_iterator(dir))
  {
    if(!entry.is_regular_file())
      continue;
    auto fmt = detect_format(entry.path());
    if(fmt != ImageFormat::Unknown)
      return fmt;
  }
  return ImageFormat::Unknown;
}

uint32_t count_frames(const fs::path& dir)
{
  uint32_t count = 0;
  for(const auto& entry : fs::directory_iterator(dir))
  {
    if(!entry.is_regular_file())
      continue;
    auto fmt = detect_format(entry.path());
    if(fmt != ImageFormat::Unknown)
      ++count;
  }
  return count;
}

static std::string find_encoder()
{
  // Prefer grk_compress if available
  for(const auto& cmd : {"grk_compress", "opj_compress"})
  {
    std::string check = std::string("which ") + cmd + " >/dev/null 2>&1";
#ifdef _WIN32
    check = std::string("where ") + cmd + " >NUL 2>&1";
#endif
    if(system(check.c_str()) == 0)
      return cmd;
  }
  return "";
}

static std::string format_to_input_flag(ImageFormat fmt)
{
  switch(fmt)
  {
    case ImageFormat::DPX:
      return "dpx";
    case ImageFormat::TIFF:
      return "tif";
    case ImageFormat::EXR:
      return "exr";
    case ImageFormat::PNG:
      return "png";
    case ImageFormat::BMP:
      return "bmp";
    case ImageFormat::JPEG:
      return "jpg";
    default:
      return "";
  }
}

EncodeResult encode_to_j2k(const EncodeOptions& opts)
{
  EncodeResult result;
  result.output_dir = opts.output_dir;

  // If input is already J2K, just copy/symlink
  ImageFormat fmt = opts.format;
  if(fmt == ImageFormat::Unknown)
    fmt = detect_sequence_format(opts.input_dir);

  if(fmt == ImageFormat::J2K)
  {
    // Already encoded — just point to the input directory
    result.output_dir = opts.input_dir;
    result.frame_count = count_frames(opts.input_dir);
    result.success = true;
    spdlog::info("Input already J2K ({} frames), skipping encode", result.frame_count);
    return result;
  }

  if(fmt == ImageFormat::Unknown)
  {
    result.error = "Cannot detect image format in: " + opts.input_dir.string();
    return result;
  }

  // Find encoder
  std::string encoder = find_encoder();
  if(encoder.empty())
  {
    result.error = "No J2K encoder found. Install grk_compress (grok) or opj_compress (OpenJPEG)";
    return result;
  }

  spdlog::info("Using encoder: {}", encoder);
  fs::create_directories(opts.output_dir);

  // Collect and sort input files
  std::vector<fs::path> input_files;
  for(const auto& entry : fs::directory_iterator(opts.input_dir))
  {
    if(!entry.is_regular_file())
      continue;
    if(detect_format(entry.path()) == fmt)
      input_files.push_back(entry.path());
  }
  std::sort(input_files.begin(), input_files.end());

  result.frame_count = static_cast<uint32_t>(input_files.size());
  spdlog::info("Encoding {} frames ({}) -> J2K", result.frame_count, format_to_input_flag(fmt));

  // Encode each frame
  uint32_t encoded = 0;
  for(const auto& input_file : input_files)
  {
    // Output filename: frame_NNNNNN.j2c
    char out_name[64];
    snprintf(out_name, sizeof(out_name), "frame_%06u.j2c", encoded);
    fs::path out_path = opts.output_dir / out_name;

    // Build encoder command
    std::string cmd;
    if(encoder == "grk_compress")
    {
      cmd = "grk_compress"
            " -i " +
            input_file.string() + " -o " + out_path.string() + " -r " +
            std::to_string(opts.target_bitrate_mbps) + " -n " +
            std::to_string(opts.num_resolutions) + " -b " + std::to_string(opts.code_block_width) +
            "," + std::to_string(opts.code_block_height);
      if(opts.cinema_profile)
        cmd += " -cinema2K 24"; // will be overridden by actual rate
      if(opts.num_threads > 0)
        cmd += " -threads " + std::to_string(opts.num_threads);
      cmd += " 2>/dev/null";
    }
    else
    {
      // opj_compress
      cmd = "opj_compress"
            " -i " +
            input_file.string() + " -o " + out_path.string() + " -r " +
            std::to_string(opts.target_bitrate_mbps) + " -n " +
            std::to_string(opts.num_resolutions) + " -b " + std::to_string(opts.code_block_width) +
            "," + std::to_string(opts.code_block_height);
      if(opts.cinema_profile)
        cmd += " -cinema2K";
      cmd += " 2>/dev/null";
    }

    int ret = system(cmd.c_str());
    if(ret != 0)
    {
      result.error = "Encoder failed on frame: " + input_file.string();
      return result;
    }

    ++encoded;
    if(encoded % 100 == 0)
      spdlog::info("  Encoded {}/{} frames", encoded, result.frame_count);
  }

  result.success = true;
  spdlog::info("Encoding complete: {} frames in {}", encoded, opts.output_dir.string());
  return result;
}

} // namespace imfwizard
