#include <spdlog/spdlog.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <regex>

#include "imfwizard/preview.h"
#include "imfwizard/portable.h"

namespace imfwizard
{

static std::string find_decoder()
{
  std::string cmd = "grk_decompress";
  std::string check = std::string("which ") + cmd + " >/dev/null 2>&1";
#ifdef _WIN32
  check = std::string("where ") + cmd + " >NUL 2>&1";
#endif
  if(system(check.c_str()) == 0)
    return cmd;
  return "";
}

static std::vector<std::filesystem::path> list_j2k_files(const std::filesystem::path& dir)
{
  std::vector<std::filesystem::path> files;
  if(!std::filesystem::is_directory(dir))
    return files;

  for(auto& entry : std::filesystem::directory_iterator(dir))
  {
    if(!entry.is_regular_file())
      continue;
    auto ext = entry.path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if(ext == ".j2c" || ext == ".j2k" || ext == ".jp2")
      files.push_back(entry.path());
  }
  std::sort(files.begin(), files.end());
  return files;
}

uint32_t count_j2k_frames(const std::filesystem::path& dir)
{
  return static_cast<uint32_t>(list_j2k_files(dir).size());
}

PreviewResult decode_preview_frame(const PreviewOptions& opts)
{
  PreviewResult result;

  auto source_dir = opts.j2k_dir.empty() ? opts.imp_dir : opts.j2k_dir;
  if(source_dir.empty())
  {
    result.error = "No source directory specified";
    return result;
  }

  auto files = list_j2k_files(source_dir);
  if(files.empty())
  {
    result.error = "No J2K files found in " + source_dir.string();
    return result;
  }

  if(opts.frame >= files.size())
  {
    result.error = "Frame " + std::to_string(opts.frame) + " out of range (0-" +
                   std::to_string(files.size() - 1) + ")";
    return result;
  }

  auto decoder = find_decoder();
  if(decoder.empty())
  {
    result.error = "No J2K decoder found (need grk_decompress)";
    return result;
  }

  auto input_file = files[opts.frame];
  auto out_dir = opts.output_dir.empty() ? std::filesystem::temp_directory_path() : opts.output_dir;
  std::filesystem::create_directories(out_dir);

  auto output_file = out_dir / ("preview_" + std::to_string(opts.frame) + ".png");

  // Decode J2K to PNG
  std::string cmd = decoder + " -i \"" + input_file.string() + "\" -o \"" + output_file.string() + "\"";

  // If thumbnail requested, pipe through ffmpeg for resize
  if(opts.generate_thumbnail && opts.thumbnail_width > 0)
  {
    auto thumb_file = out_dir / ("thumb_" + std::to_string(opts.frame) + ".png");
    std::string resize_cmd = cmd + " 2>/dev/null && ffmpeg -y -i \"" + output_file.string() +
                             "\" -vf scale=" + std::to_string(opts.thumbnail_width) + ":-1 \"" +
                             thumb_file.string() + "\" 2>/dev/null";
    if(system(resize_cmd.c_str()) == 0)
    {
      result.frame.thumbnail_path = thumb_file.string();
      result.frame.frame_number = opts.frame;
      result.success = true;
      return result;
    }
  }

  // Fallback: just decode
  cmd += " 2>/dev/null";
  if(system(cmd.c_str()) == 0)
  {
    result.frame.thumbnail_path = output_file.string();
    result.frame.frame_number = opts.frame;
    result.success = true;
  }
  else
  {
    result.error = "Failed to decode frame " + std::to_string(opts.frame);
  }

  return result;
}

ThumbnailStripResult generate_thumbnail_strip(const ThumbnailStripOptions& opts)
{
  ThumbnailStripResult result;

  auto files = list_j2k_files(opts.source_dir);
  if(files.empty())
  {
    result.error = "No J2K files found in " + opts.source_dir.string();
    return result;
  }

  result.total_frames = static_cast<uint32_t>(files.size());
  std::filesystem::create_directories(opts.output_dir);

  uint32_t count = std::min(opts.count, result.total_frames);
  uint32_t step = result.total_frames / count;

  for(uint32_t i = 0; i < count; ++i)
  {
    uint32_t frame_idx = i * step;
    PreviewOptions po;
    po.j2k_dir = opts.source_dir;
    po.frame = frame_idx;
    po.thumbnail_width = opts.width;
    po.generate_thumbnail = true;
    po.output_dir = opts.output_dir;

    auto r = decode_preview_frame(po);
    if(r.success)
      result.thumbnails.push_back(r.frame.thumbnail_path);
  }

  result.success = !result.thumbnails.empty();
  return result;
}

} // namespace imfwizard
