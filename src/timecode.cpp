#include <spdlog/spdlog.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <regex>
#include <sstream>

#include "imfwizard/timecode.h"
#include "imfwizard/portable.h"

namespace imfwizard
{

uint64_t Timecode::to_frames(double fps) const
{
  if(drop_frame && (std::abs(fps - 29.97) < 0.01 || std::abs(fps - 59.94) < 0.01))
  {
    // SMPTE drop-frame calculation
    int d = static_cast<int>(std::round(fps / 30.0)) * 2; // frames dropped per minute
    uint64_t total_minutes = hours * 60 + minutes;
    uint64_t frame_count =
        static_cast<uint64_t>(hours) * 108000 + // for 29.97
        static_cast<uint64_t>(minutes) * 1800 + static_cast<uint64_t>(seconds) * 30 + frames -
        d * (total_minutes - total_minutes / 10);
    return frame_count;
  }

  uint32_t fps_int = static_cast<uint32_t>(std::round(fps));
  return static_cast<uint64_t>(hours) * 3600 * fps_int +
         static_cast<uint64_t>(minutes) * 60 * fps_int +
         static_cast<uint64_t>(seconds) * fps_int + frames;
}

double Timecode::to_seconds(double fps) const
{
  return static_cast<double>(to_frames(fps)) / fps;
}

std::string Timecode::to_string() const
{
  char buf[16];
  char sep = drop_frame ? ';' : ':';
  std::snprintf(buf, sizeof(buf), "%02u:%02u:%02u%c%02u", hours, minutes, seconds, sep, frames);
  return buf;
}

Timecode Timecode::from_frames(uint64_t total_frames, double fps, bool drop_frame_flag)
{
  Timecode tc;
  tc.drop_frame = drop_frame_flag;

  if(drop_frame_flag && (std::abs(fps - 29.97) < 0.01 || std::abs(fps - 59.94) < 0.01))
  {
    // Drop-frame timecode calculation
    int d = static_cast<int>(std::round(fps / 30.0)) * 2;
    int frames_per_10min = static_cast<int>(std::round(fps * 600));
    int frames_per_min = frames_per_10min / 10;

    int64_t f = static_cast<int64_t>(total_frames);
    int ten_min_blocks = static_cast<int>(f / frames_per_10min);
    int remainder = static_cast<int>(f % frames_per_10min);

    int additional_minutes = 0;
    if(remainder >= d)
      additional_minutes = (remainder - d) / (frames_per_min - d);

    int total_minutes = ten_min_blocks * 10 + additional_minutes;

    tc.hours = total_minutes / 60;
    tc.minutes = total_minutes % 60;

    int frames_into_minute = static_cast<int>(f) -
                             (total_minutes * frames_per_min) +
                             (d * (total_minutes - total_minutes / 10));
    tc.seconds = frames_into_minute / static_cast<int>(std::round(fps));
    tc.frames = frames_into_minute % static_cast<int>(std::round(fps));
  }
  else
  {
    uint32_t fps_int = static_cast<uint32_t>(std::round(fps));
    tc.frames = static_cast<uint32_t>(total_frames % fps_int);
    uint64_t remaining = total_frames / fps_int;
    tc.seconds = static_cast<uint32_t>(remaining % 60);
    remaining /= 60;
    tc.minutes = static_cast<uint32_t>(remaining % 60);
    tc.hours = static_cast<uint32_t>(remaining / 60);
  }

  return tc;
}

Timecode Timecode::from_seconds(double seconds, double fps, bool drop_frame_flag)
{
  uint64_t total_frames = static_cast<uint64_t>(std::round(seconds * fps));
  return from_frames(total_frames, fps, drop_frame_flag);
}

Timecode Timecode::parse(const std::string& tc_str, double fps)
{
  Timecode tc;

  std::regex re(R"((\d{1,2}):(\d{2}):(\d{2})([;:])(\d{2}))");
  std::smatch m;
  if(!std::regex_match(tc_str, m, re))
  {
    spdlog::warn("Invalid timecode format: {}", tc_str);
    return tc;
  }

  tc.hours = static_cast<uint32_t>(std::stoul(m[1].str()));
  tc.minutes = static_cast<uint32_t>(std::stoul(m[2].str()));
  tc.seconds = static_cast<uint32_t>(std::stoul(m[3].str()));
  tc.drop_frame = (m[4].str() == ";");
  tc.frames = static_cast<uint32_t>(std::stoul(m[5].str()));

  return tc;
}

Timecode convert_timecode(const TimecodeConvertOptions& opts)
{
  // Parse source timecode
  Timecode src = Timecode::parse(opts.input_tc, opts.source_fps);
  src.drop_frame = opts.source_drop;

  // Convert to seconds (universal intermediate)
  double seconds = src.to_seconds(opts.source_fps);

  // Convert to target framerate
  return Timecode::from_seconds(seconds, opts.target_fps, opts.target_drop);
}

static std::string exec_tc(const std::string& cmd)
{
  std::array<char, 4096> buffer;
  std::string result;
  FILE* pipe = portable_popen(cmd.c_str(), "r");
  if(!pipe)
    return {};
  while(fgets(buffer.data(), buffer.size(), pipe))
    result += buffer.data();
  portable_pclose(pipe);
  return result;
}

TimecodeDriftResult check_timecode_drift(const TimecodeDriftOptions& opts)
{
  TimecodeDriftResult result;

  // Use ffprobe to get timecode from video
  std::ostringstream cmd;
  cmd << "ffprobe -v error -select_streams v:0 "
      << "-show_entries stream_tags=timecode "
      << "-show_entries format_tags=timecode "
      << "-of csv=p=0 \"" << opts.video_path << "\" 2>&1";

  auto output = exec_tc(cmd.str());

  // Trim
  while(!output.empty() && (output.back() == '\n' || output.back() == '\r'))
    output.pop_back();

  if(output.empty() || output == "N/A")
  {
    result.error = "No timecode track found in video";
    result.has_timecode = false;
    result.success = true;
    return result;
  }

  result.has_timecode = true;
  result.detected_start_tc = output;

  // Get FPS
  std::ostringstream fps_cmd;
  fps_cmd << "ffprobe -v error -select_streams v:0 "
          << "-show_entries stream=r_frame_rate -of csv=p=0 \""
          << opts.video_path << "\" 2>&1";
  auto fps_out = exec_tc(fps_cmd.str());
  // Parse "24000/1001" or "24/1"
  std::regex fps_re(R"((\d+)/(\d+))");
  std::smatch fm;
  if(std::regex_search(fps_out, fm, fps_re))
  {
    result.detected_fps = std::stod(fm[1].str()) / std::stod(fm[2].str());
  }
  else
  {
    result.detected_fps = opts.expected_fps;
  }

  // Compare detected start TC with expected
  if(!opts.expected_start_tc.empty())
  {
    auto expected = Timecode::parse(opts.expected_start_tc, opts.expected_fps);
    auto detected = Timecode::parse(result.detected_start_tc, result.detected_fps);

    double expected_sec = expected.to_seconds(opts.expected_fps);
    double detected_sec = detected.to_seconds(result.detected_fps);

    result.max_drift_seconds = std::abs(detected_sec - expected_sec);
    result.max_drift_frames = result.max_drift_seconds * opts.expected_fps;
    result.within_tolerance = (result.max_drift_frames <= 1.0);
  }
  else
  {
    result.within_tolerance = true;
  }

  result.success = true;
  spdlog::info("Timecode: {} (detected fps: {:.3f}, drift: {:.3f} frames)",
               result.detected_start_tc, result.detected_fps, result.max_drift_frames);
  return result;
}

} // namespace imfwizard
