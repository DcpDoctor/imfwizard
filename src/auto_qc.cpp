#include "imfwizard/auto_qc.h"
#include "imfwizard/portable.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <regex>
#include <sstream>

namespace imfwizard
{

static std::string exec_cmd_qc(const std::string& cmd)
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

static std::string issue_type_str(QcIssueType type)
{
  switch(type)
  {
    case QcIssueType::BlackFrame: return "black_frame";
    case QcIssueType::FreezeFrame: return "freeze_frame";
    case QcIssueType::AudioSilence: return "audio_silence";
    case QcIssueType::AudioClipping: return "audio_clipping";
    case QcIssueType::ColorGamutExcursion: return "gamut_excursion";
    case QcIssueType::BarsAndTone: return "bars_and_tone";
    case QcIssueType::DropFrame: return "drop_frame";
    case QcIssueType::InvalidTimecode: return "invalid_timecode";
  }
  return "unknown";
}

static double frame_to_sec(uint32_t frame, uint32_t fps_num, uint32_t fps_den)
{
  return static_cast<double>(frame) * fps_den / fps_num;
}

// Detect black frames using ffmpeg blackdetect filter
static void detect_black_frames(const AutoQcOptions& opts, AutoQcResult& result)
{
  std::ostringstream cmd;
  cmd << "ffmpeg -i \"" << opts.video_path.string() << "\" "
      << "-vf \"blackdetect=d=0:pix_th=" << (opts.black_threshold / 255.0)
      << ":pic_th=" << opts.black_ratio << "\" "
      << "-an -f null - 2>&1";

  auto output = exec_cmd_qc(cmd.str());
  if(output.empty())
    return;

  // Parse: [blackdetect @ ...] black_start:1.5 black_end:2.0 black_duration:0.5
  std::regex re(R"(black_start:([\d.]+)\s+black_end:([\d.]+)\s+black_duration:([\d.]+))");
  std::sregex_iterator it(output.begin(), output.end(), re);
  std::sregex_iterator end;

  double fps = static_cast<double>(opts.fps_num) / opts.fps_den;

  for(; it != end; ++it)
  {
    double start = std::stod((*it)[1].str());
    double finish = std::stod((*it)[2].str());
    double duration = std::stod((*it)[3].str());
    uint32_t min_duration_frames = opts.black_min_frames;
    double min_duration_sec = min_duration_frames / fps;

    if(duration < min_duration_sec)
      continue;

    QcIssue issue;
    issue.type = QcIssueType::BlackFrame;
    issue.start_frame = static_cast<uint32_t>(start * fps);
    issue.end_frame = static_cast<uint32_t>(finish * fps);
    issue.start_timecode_sec = start;
    issue.end_timecode_sec = finish;
    issue.severity = (duration > 5.0) ? "error" : "warning";

    std::ostringstream desc;
    desc << "Black frames detected from " << start << "s to " << finish
         << "s (duration: " << duration << "s)";
    issue.description = desc.str();
    result.issues.push_back(issue);
  }
}

// Detect freeze frames using ffmpeg freezedetect filter
static void detect_freeze_frames(const AutoQcOptions& opts, AutoQcResult& result)
{
  double fps = static_cast<double>(opts.fps_num) / opts.fps_den;
  double min_dur = opts.freeze_min_frames / fps;

  std::ostringstream cmd;
  cmd << "ffmpeg -i \"" << opts.video_path.string() << "\" "
      << "-vf \"freezedetect=n=" << opts.freeze_threshold
      << ":d=" << min_dur << "\" "
      << "-an -f null - 2>&1";

  auto output = exec_cmd_qc(cmd.str());
  if(output.empty())
    return;

  // Parse: [freezedetect @ ...] lavfi.freezedetect.freeze_start: 10.5
  //        [freezedetect @ ...] lavfi.freezedetect.freeze_end: 12.5
  std::regex start_re(R"(freeze_start:\s*([\d.]+))");
  std::regex end_re(R"(freeze_end:\s*([\d.]+))");

  std::vector<double> starts, ends;
  std::sregex_iterator it_s(output.begin(), output.end(), start_re);
  std::sregex_iterator it_e(output.begin(), output.end(), end_re);
  std::sregex_iterator iter_end;

  for(; it_s != iter_end; ++it_s)
    starts.push_back(std::stod((*it_s)[1].str()));
  for(; it_e != iter_end; ++it_e)
    ends.push_back(std::stod((*it_e)[1].str()));

  for(size_t i = 0; i < starts.size() && i < ends.size(); ++i)
  {
    QcIssue issue;
    issue.type = QcIssueType::FreezeFrame;
    issue.start_frame = static_cast<uint32_t>(starts[i] * fps);
    issue.end_frame = static_cast<uint32_t>(ends[i] * fps);
    issue.start_timecode_sec = starts[i];
    issue.end_timecode_sec = ends[i];
    issue.severity = "warning";

    std::ostringstream desc;
    desc << "Freeze frame from " << starts[i] << "s to " << ends[i]
         << "s (duration: " << (ends[i] - starts[i]) << "s)";
    issue.description = desc.str();
    result.issues.push_back(issue);
  }
}

// Detect audio silence using ffmpeg silencedetect filter
static void detect_audio_silence(const AutoQcOptions& opts, AutoQcResult& result)
{
  auto path = opts.audio_path.empty() ? opts.video_path : opts.audio_path;
  if(path.empty())
    return;

  std::ostringstream cmd;
  cmd << "ffmpeg -i \"" << path.string() << "\" "
      << "-af \"silencedetect=noise=" << opts.silence_threshold_db
      << "dB:d=" << opts.silence_min_duration << "\" "
      << "-vn -f null - 2>&1";

  auto output = exec_cmd_qc(cmd.str());
  if(output.empty())
    return;

  double fps = static_cast<double>(opts.fps_num) / opts.fps_den;
  std::regex start_re(R"(silence_start:\s*([\d.]+))");
  std::regex end_re(R"(silence_end:\s*([\d.]+)\s*\|\s*silence_duration:\s*([\d.]+))");

  std::vector<double> starts;
  std::sregex_iterator it_s(output.begin(), output.end(), start_re);
  std::sregex_iterator iter_end;
  for(; it_s != iter_end; ++it_s)
    starts.push_back(std::stod((*it_s)[1].str()));

  std::sregex_iterator it_e(output.begin(), output.end(), end_re);
  size_t idx = 0;
  for(; it_e != iter_end; ++it_e, ++idx)
  {
    double end_t = std::stod((*it_e)[1].str());
    double duration = std::stod((*it_e)[2].str());
    double start_t = (idx < starts.size()) ? starts[idx] : (end_t - duration);

    QcIssue issue;
    issue.type = QcIssueType::AudioSilence;
    issue.start_frame = static_cast<uint32_t>(start_t * fps);
    issue.end_frame = static_cast<uint32_t>(end_t * fps);
    issue.start_timecode_sec = start_t;
    issue.end_timecode_sec = end_t;
    issue.severity = (duration > 10.0) ? "error" : "warning";

    std::ostringstream desc;
    desc << "Audio silence from " << start_t << "s to " << end_t
         << "s (duration: " << duration << "s)";
    issue.description = desc.str();
    result.issues.push_back(issue);
  }
}

// Detect audio clipping using ffmpeg astats filter
static void detect_audio_clipping(const AutoQcOptions& opts, AutoQcResult& result)
{
  auto path = opts.audio_path.empty() ? opts.video_path : opts.audio_path;
  if(path.empty())
    return;

  std::ostringstream cmd;
  cmd << "ffmpeg -i \"" << path.string() << "\" "
      << "-af \"astats=metadata=1:reset=1\" -vn -f null - 2>&1";

  auto output = exec_cmd_qc(cmd.str());
  if(output.empty())
    return;

  // Look for Number of samples at peak value - high count indicates clipping
  std::regex clip_re(R"(Number of Nans:\s*(\d+)|Flat factor:\s*([\d.]+))");
  // Simpler approach: use volumedetect and check max volume
  std::ostringstream cmd2;
  cmd2 << "ffmpeg -i \"" << path.string() << "\" "
       << "-af \"volumedetect\" -vn -f null - 2>&1";

  auto output2 = exec_cmd_qc(cmd2.str());
  std::regex maxvol_re(R"(max_volume:\s*([-\d.]+)\s*dB)");
  std::smatch match;
  if(std::regex_search(output2, match, maxvol_re))
  {
    double max_vol = std::stod(match[1].str());
    if(max_vol >= -0.1)
    { // very close to 0 dBFS = likely clipping
      QcIssue issue;
      issue.type = QcIssueType::AudioClipping;
      issue.start_frame = 0;
      issue.end_frame = result.total_frames;
      issue.severity = "error";

      std::ostringstream desc;
      desc << "Audio peak at " << max_vol << " dBFS — possible clipping";
      issue.description = desc.str();
      result.issues.push_back(issue);
    }
  }
}

AutoQcResult run_auto_qc(const AutoQcOptions& opts)
{
  AutoQcResult result;

  if(opts.video_path.empty() && opts.audio_path.empty())
  {
    result.error = "No video or audio path specified";
    return result;
  }

  // Get duration/frame count
  if(!opts.video_path.empty())
  {
    std::ostringstream cmd;
    cmd << "ffprobe -v error -count_frames -select_streams v:0 "
        << "-show_entries stream=nb_read_frames,duration "
        << "-of csv=p=0 \"" << opts.video_path.string() << "\" 2>&1";
    auto output = exec_cmd_qc(cmd.str());
    if(!output.empty())
    {
      // Format: nb_frames,duration or just a number
      auto comma = output.find(',');
      if(comma != std::string::npos)
      {
        try
        {
          result.total_frames = static_cast<uint32_t>(std::stoul(output.substr(0, comma)));
          result.duration_seconds = std::stod(output.substr(comma + 1));
        }
        catch(...)
        {
        }
      }
      else
      {
        try
        {
          result.total_frames = static_cast<uint32_t>(std::stoul(output));
          double fps = static_cast<double>(opts.fps_num) / opts.fps_den;
          result.duration_seconds = result.total_frames / fps;
        }
        catch(...)
        {
        }
      }
    }
  }

  spdlog::info("Auto QC: analyzing {} ({} frames, {:.1f}s)",
               opts.video_path.string(), result.total_frames, result.duration_seconds);

  // Run detections
  if(opts.detect_black && !opts.video_path.empty())
  {
    spdlog::info("Detecting black frames...");
    detect_black_frames(opts, result);
  }

  if(opts.detect_freeze && !opts.video_path.empty())
  {
    spdlog::info("Detecting freeze frames...");
    detect_freeze_frames(opts, result);
  }

  if(opts.detect_silence)
  {
    spdlog::info("Detecting audio silence...");
    detect_audio_silence(opts, result);
  }

  if(opts.detect_clipping)
  {
    spdlog::info("Detecting audio clipping...");
    detect_audio_clipping(opts, result);
  }

  // Count severity
  for(const auto& issue : result.issues)
  {
    if(issue.severity == "error")
      ++result.error_count;
    else if(issue.severity == "warning")
      ++result.warning_count;
  }

  result.success = true;
  spdlog::info("Auto QC complete: {} issues ({} errors, {} warnings)",
               result.issues.size(), result.error_count, result.warning_count);
  return result;
}

std::string auto_qc_to_json(const AutoQcResult& result)
{
  std::ostringstream json;
  json << "{\n";
  json << "  \"success\": " << (result.success ? "true" : "false") << ",\n";
  json << "  \"total_frames\": " << result.total_frames << ",\n";
  json << "  \"duration_seconds\": " << result.duration_seconds << ",\n";
  json << "  \"error_count\": " << result.error_count << ",\n";
  json << "  \"warning_count\": " << result.warning_count << ",\n";
  json << "  \"issues\": [\n";

  for(size_t i = 0; i < result.issues.size(); ++i)
  {
    const auto& issue = result.issues[i];
    json << "    {\n";
    json << "      \"type\": \"" << issue_type_str(issue.type) << "\",\n";
    json << "      \"severity\": \"" << issue.severity << "\",\n";
    json << "      \"start_frame\": " << issue.start_frame << ",\n";
    json << "      \"end_frame\": " << issue.end_frame << ",\n";
    json << "      \"start_timecode_sec\": " << issue.start_timecode_sec << ",\n";
    json << "      \"end_timecode_sec\": " << issue.end_timecode_sec << ",\n";
    json << "      \"description\": \"" << issue.description << "\"\n";
    json << "    }";
    if(i + 1 < result.issues.size())
      json << ",";
    json << "\n";
  }

  json << "  ]\n";
  json << "}\n";
  return json.str();
}

} // namespace imfwizard
