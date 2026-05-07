#include "imfwizard/imfwizard.h"
#include "imfwizard/info.h"
#include "imfwizard/supplemental.h"
#include "imfwizard/validate.h"
#include "imfwizard/transcode.h"
#include "imfwizard/dolby_vision.h"
#include "imfwizard/extract.h"
#include "imfwizard/report.h"
#include "imfwizard/conform.h"
#include "imfwizard/watch.h"
#include "imfwizard/loudness.h"
#include "imfwizard/qc.h"
#include "imfwizard/channel_map.h"
#include "imfwizard/cloud.h"
#include "imfwizard/captions.h"
#include "imfwizard/watermark.h"
#include "imfwizard/aspera.h"
#include "imfwizard/profiles.h"
#include "imfwizard/job_queue.h"
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>
#include <cinttypes>
#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
  CLI::App app{"IMF Wizard — Interoperable Master Format package creator"};
  app.set_version_flag("--version", "0.1.0");
  app.require_subcommand(1);

  // === CREATE subcommand ===
  auto* create_cmd = app.add_subcommand("create", "Create an Original Version IMP");

  std::string title, issuer, content_kind = "feature";
  std::string video_dir, audio_file, output_dir, subtitle_file;
  uint32_t fps_num = 24, fps_den = 1;
  uint32_t sample_rate = 48000;
  uint16_t bit_depth = 24;
  bool verbose = false;
  std::string cert_file, key_file;

  create_cmd->add_option("-t,--title", title, "Content title")->required();
  create_cmd->add_option("-i,--issuer", issuer, "Issuer name")->default_val("IMF Wizard");
  create_cmd->add_option("-k,--kind", content_kind, "Content kind")->default_val("feature");
  create_cmd->add_option("-v,--video", video_dir, "Directory of J2K codestreams")
      ->required()
      ->check(CLI::ExistingDirectory);
  create_cmd->add_option("-a,--audio", audio_file, "WAV audio file")->check(CLI::ExistingFile);
  create_cmd->add_option("-o,--output", output_dir, "Output IMP directory")->required();
  create_cmd->add_option("--fps-num", fps_num, "Edit rate numerator")->default_val(24);
  create_cmd->add_option("--fps-den", fps_den, "Edit rate denominator")->default_val(1);
  create_cmd->add_option("--sample-rate", sample_rate, "Audio sample rate")->default_val(48000);
  create_cmd->add_option("--bit-depth", bit_depth, "Audio bit depth")->default_val(24);
  create_cmd->add_option("--cert", cert_file, "X.509 certificate for signing");
  create_cmd->add_option("--key", key_file, "Private key for signing");
  create_cmd->add_option("-s,--subtitle", subtitle_file, "TTML/IMSC subtitle file")
      ->check(CLI::ExistingFile);
  create_cmd->add_flag("--verbose", verbose, "Verbose output");

  // === INFO subcommand ===
  auto* info_cmd = app.add_subcommand("info", "Display information about an existing IMP");
  std::string imp_dir;
  info_cmd->add_option("imp_dir", imp_dir, "IMP directory")
      ->required()
      ->check(CLI::ExistingDirectory);

  // === ENCODE subcommand ===
  auto* encode_cmd = app.add_subcommand("encode", "Encode image sequence to JPEG 2000");
  std::string enc_input, enc_output;
  float enc_bitrate = 250.0f;
  uint32_t enc_threads = 0;
  bool enc_cinema = true;

  encode_cmd->add_option("-i,--input", enc_input, "Input image sequence directory")
      ->required()
      ->check(CLI::ExistingDirectory);
  encode_cmd->add_option("-o,--output", enc_output, "Output J2K directory")->required();
  encode_cmd->add_option("--bitrate", enc_bitrate, "Target bitrate (Mbps)")->default_val(250.0f);
  encode_cmd->add_option("--threads", enc_threads, "Number of threads (0=auto)")->default_val(0);
  encode_cmd->add_flag(
      "--no-cinema", [&](int64_t) { enc_cinema = false; }, "Disable cinema profile");

  // === VALIDATE subcommand ===
  auto* val_cmd = app.add_subcommand("validate", "Validate an existing IMP (via Photon)");
  std::string val_dir;
  val_cmd->add_option("imp_dir", val_dir, "IMP directory to validate")
      ->required()
      ->check(CLI::ExistingDirectory);

  // === SUPPLEMENT subcommand ===
  auto* sup_cmd = app.add_subcommand("supplement", "Create a Supplemental IMP");
  std::string sup_title, sup_issuer, sup_ov_dir, sup_video_dir, sup_audio_file, sup_output_dir;
  uint32_t sup_entry_point = 0, sup_duration = 0;

  sup_cmd->add_option("-t,--title", sup_title, "Content title")->required();
  sup_cmd->add_option("-i,--issuer", sup_issuer, "Issuer name")->default_val("IMF Wizard");
  sup_cmd->add_option("--ov", sup_ov_dir, "Original Version IMP directory")
      ->required()
      ->check(CLI::ExistingDirectory);
  sup_cmd->add_option("-v,--video", sup_video_dir, "Replacement J2K directory")
      ->check(CLI::ExistingDirectory);
  sup_cmd->add_option("-a,--audio", sup_audio_file, "Replacement audio WAV")
      ->check(CLI::ExistingFile);
  sup_cmd->add_option("-o,--output", sup_output_dir, "Output supplemental IMP directory")
      ->required();
  sup_cmd->add_option("--entry-point", sup_entry_point, "Frame offset in OV to replace from");
  sup_cmd->add_option("--duration", sup_duration, "Number of frames to replace");

  // === TRANSCODE subcommand ===
  auto* trans_cmd = app.add_subcommand("transcode", "Transcode video file to image sequence");
  std::string trans_input, trans_output, trans_format = "tiff";
  uint16_t trans_bit_depth = 16;
  uint32_t trans_threads = 0;

  trans_cmd->add_option("-i,--input", trans_input, "Input video file (ProRes, DNxHR, etc.)")
      ->required()
      ->check(CLI::ExistingFile);
  trans_cmd->add_option("-o,--output", trans_output, "Output image sequence directory")->required();
  trans_cmd->add_option("-f,--format", trans_format, "Output format (tiff, dpx, exr, png)")
      ->default_val("tiff");
  trans_cmd->add_option("--bit-depth", trans_bit_depth, "Output bit depth")->default_val(16);
  trans_cmd->add_option("--threads", trans_threads, "Number of threads (0=auto)")->default_val(0);

  // === EXTRACT subcommand ===
  auto* extract_cmd = app.add_subcommand("extract", "Extract tracks from an existing IMP");
  std::string ext_imp_dir, ext_output, ext_format = "j2c";
  uint32_t ext_start = 0, ext_end = 0;

  extract_cmd->add_option("imp_dir", ext_imp_dir, "IMP directory")
      ->required()
      ->check(CLI::ExistingDirectory);
  extract_cmd->add_option("-o,--output", ext_output, "Output directory")->required();
  extract_cmd->add_option("-f,--format", ext_format, "Output format")->default_val("j2c");
  extract_cmd->add_option("--start", ext_start, "Start frame")->default_val(0);
  extract_cmd->add_option("--end", ext_end, "End frame (0=all)")->default_val(0);

  // === CONFORM subcommand ===
  auto* conform_cmd = app.add_subcommand("conform", "Create IMP from EDL/AAF");
  std::string conf_edl, conf_media, conf_output, conf_title;

  conform_cmd->add_option("-e,--edl", conf_edl, "EDL or AAF file")
      ->required()
      ->check(CLI::ExistingFile);
  conform_cmd->add_option("-m,--media", conf_media, "Media directory with reels")
      ->required()
      ->check(CLI::ExistingDirectory);
  conform_cmd->add_option("-o,--output", conf_output, "Output IMP directory")->required();
  conform_cmd->add_option("-t,--title", conf_title, "Content title")->default_val("Conformed IMP");

  // === WATCH subcommand ===
  auto* watch_cmd = app.add_subcommand("watch", "Watch folder for auto-IMP creation");
  std::string watch_dir, watch_output;

  watch_cmd->add_option("-w,--watch", watch_dir, "Directory to watch")
      ->required()
      ->check(CLI::ExistingDirectory);
  watch_cmd->add_option("-o,--output", watch_output, "Output base directory")->required();

  // === REPORT subcommand ===
  auto* report_cmd = app.add_subcommand("report", "Generate QC report for an IMP");
  std::string rpt_imp_dir, rpt_output;

  report_cmd->add_option("imp_dir", rpt_imp_dir, "IMP directory")
      ->required()
      ->check(CLI::ExistingDirectory);
  report_cmd->add_option("-o,--output", rpt_output, "Output HTML file")->required();

  // === LOUDNESS subcommand ===
  auto* loudness_cmd = app.add_subcommand("loudness", "Measure/normalize audio loudness");
  std::string loud_input, loud_output;
  double loud_target = -23.0;
  bool loud_normalize = false;
  loudness_cmd->add_option("-i,--input", loud_input, "Audio WAV file")->required();
  loudness_cmd->add_option("-o,--output", loud_output, "Normalized output file");
  loudness_cmd->add_option("--target", loud_target, "Target LUFS")->default_val(-23.0);
  loudness_cmd->add_flag("-n,--normalize", loud_normalize, "Normalize to target");

  // === QC subcommand ===
  auto* qc_cmd = app.add_subcommand("qc", "Frame-level quality control analysis");
  std::string qc_dir, qc_ref, qc_dist;
  double qc_max = 300.0, qc_min = 50.0;
  qc_cmd->add_option("-d,--dir", qc_dir, "J2K frame directory");
  qc_cmd->add_option("--ref", qc_ref, "Reference video (for VMAF)");
  qc_cmd->add_option("--dist", qc_dist, "Distorted video (for VMAF)");
  qc_cmd->add_option("--max", qc_max, "Max bitrate threshold (Mbps)");
  qc_cmd->add_option("--min", qc_min, "Min bitrate threshold (Mbps)");

  // === CHANNEL-MAP subcommand ===
  auto* chmap_cmd = app.add_subcommand("channel-map", "Remap audio channels");
  std::string chmap_input, chmap_output, chmap_target = "stereo";
  chmap_cmd->add_option("-i,--input", chmap_input, "Input WAV")->required();
  chmap_cmd->add_option("-o,--output", chmap_output, "Output WAV")->required();
  chmap_cmd->add_option("-t,--target", chmap_target, "Target layout (mono/stereo/5.1/7.1)");

  // === UPLOAD subcommand ===
  auto* upload_cmd = app.add_subcommand("upload", "Upload IMP to cloud storage");
  std::string up_dir, up_bucket, up_prefix, up_region = "us-east-1", up_profile;
  bool up_aspera = false;
  std::string up_host, up_user, up_token, up_remote_path;
  upload_cmd->add_option("-d,--dir", up_dir, "IMP directory")->required();
  upload_cmd->add_option("-b,--bucket", up_bucket, "S3 bucket name");
  upload_cmd->add_option("-p,--prefix", up_prefix, "S3 key prefix");
  upload_cmd->add_option("--region", up_region, "AWS region");
  upload_cmd->add_option("--profile", up_profile, "AWS profile");
  upload_cmd->add_flag("--aspera", up_aspera, "Use Aspera FASP instead of S3");
  upload_cmd->add_option("--host", up_host, "Aspera remote host");
  upload_cmd->add_option("--user", up_user, "Aspera username");
  upload_cmd->add_option("--token", up_token, "Aspera token/key");
  upload_cmd->add_option("--remote-path", up_remote_path, "Aspera remote path");

  // === CAPTIONS subcommand ===
  auto* captions_cmd = app.add_subcommand("captions", "Convert SRT/SCC to TTML for IMF");
  std::string cap_input, cap_output;
  uint32_t cap_fps = 24;
  captions_cmd->add_option("-i,--input", cap_input, "Caption file (SRT/SCC)")->required();
  captions_cmd->add_option("-o,--output", cap_output, "Output TTML file")->required();
  captions_cmd->add_option("--fps", cap_fps, "Frame rate")->default_val(24);

  // === WATERMARK subcommand ===
  auto* wm_cmd = app.add_subcommand("watermark", "Apply forensic watermark to J2K frames");
  std::string wm_input, wm_output, wm_payload;
  uint8_t wm_strength = 3;
  wm_cmd->add_option("-i,--input", wm_input, "J2K frame directory")->required();
  wm_cmd->add_option("-o,--output", wm_output, "Output directory")->required();
  wm_cmd->add_option("-p,--payload", wm_payload, "Watermark payload")->required();
  wm_cmd->add_option("-s,--strength", wm_strength, "Strength 1-5")->default_val(3);

  // === PROFILES subcommand ===
  auto* profiles_cmd = app.add_subcommand("profiles", "List available delivery profiles");
  std::string prof_name;
  profiles_cmd->add_option("-n,--name", prof_name, "Show details for specific profile");

  // === BATCH subcommand ===
  auto* batch_cmd = app.add_subcommand("batch", "Manage the job queue");
  batch_cmd->require_subcommand(1);
  auto* batch_list_cmd = batch_cmd->add_subcommand("list", "List all jobs");
  auto* batch_add_cmd = batch_cmd->add_subcommand("add", "Submit a new job");
  std::string batch_type, batch_desc;
  std::vector<std::string> batch_args;
  uint64_t batch_dep = 0;
  batch_add_cmd->add_option("-T,--type", batch_type, "Job type (transcode/encode/create/validate/loudness/qc)")->required();
  batch_add_cmd->add_option("-d,--description", batch_desc, "Job description")->required();
  batch_add_cmd->add_option("--depends-on", batch_dep, "Depend on job ID");
  batch_add_cmd->add_option("args", batch_args, "Arguments to pass to imfwizard");
  auto* batch_cancel_cmd = batch_cmd->add_subcommand("cancel", "Cancel a queued job");
  uint64_t batch_cancel_id = 0;
  batch_cancel_cmd->add_option("id", batch_cancel_id, "Job ID to cancel")->required();
  auto* batch_status_cmd = batch_cmd->add_subcommand("status", "Show status of a job");
  uint64_t batch_status_id = 0;
  batch_status_cmd->add_option("id", batch_status_id, "Job ID")->required();
  auto* batch_pause_cmd = batch_cmd->add_subcommand("pause", "Pause job processing");
  auto* batch_resume_cmd = batch_cmd->add_subcommand("resume", "Resume job processing");
  auto* batch_priority_cmd = batch_cmd->add_subcommand("priority", "Set job priority");
  uint64_t batch_pri_id = 0;
  int batch_pri_val = 0;
  batch_priority_cmd->add_option("id", batch_pri_id, "Job ID")->required();
  batch_priority_cmd->add_option("priority", batch_pri_val, "Priority (higher = first)")->required();

  // === DAEMON subcommand ===
  auto* daemon_cmd = app.add_subcommand("daemon", "Run the job queue daemon");

  CLI11_PARSE(app, argc, argv);

  if(verbose)
  {
    spdlog::set_level(spdlog::level::debug);
  }
  else
  {
    spdlog::set_level(spdlog::level::info);
  }

  if(create_cmd->parsed())
  {
    imfwizard::ImpOptions opts;
    opts.title = title;
    opts.issuer = issuer;
    opts.content_kind = content_kind;
    opts.video_dir = video_dir;
    opts.audio_file = audio_file;
    opts.subtitle_file = subtitle_file;
    opts.output_dir = output_dir;
    opts.edit_rate_num = fps_num;
    opts.edit_rate_den = fps_den;
    opts.audio_sample_rate = sample_rate;
    opts.audio_bit_depth = bit_depth;

    if(!cert_file.empty() && !key_file.empty())
    {
      imfwizard::SignOptions sign;
      sign.cert_file = cert_file;
      sign.key_file = key_file;
      opts.sign = sign;
    }

    auto result = imfwizard::create_ov_imp(opts);
    if(!result.success)
    {
      spdlog::error("Failed: {}", result.error);
      return 1;
    }

    std::cout << "IMP created: " << result.output_dir << "\n";
    std::cout << "  CPL: urn:uuid:" << result.cpl_uuid << "\n";
    std::cout << "  PKL: urn:uuid:" << result.pkl_uuid << "\n";
    std::cout << "  Track files: " << result.track_files.size() << "\n";
    return 0;
  }

  if(info_cmd->parsed())
  {
    auto imp_info = imfwizard::read_imp_info(imp_dir);
    if(!imp_info.valid)
    {
      spdlog::error("Failed to read IMP: {}", imp_info.error);
      return 1;
    }

    std::cout << "IMP: " << imp_info.path << "\n";
    std::cout << "  Title: " << imp_info.cpl_title << "\n";
    std::cout << "  CPL: " << imp_info.cpl_uuid << "\n";
    std::cout << "  PKL: " << imp_info.pkl_uuid << "\n";
    std::cout << "  Issuer: " << imp_info.issuer << "\n";
    std::cout << "  Date: " << imp_info.issue_date << "\n";
    std::cout << "  Assets: " << imp_info.tracks.size() << "\n";
    for(const auto& t : imp_info.tracks)
    {
      std::cout << "    [" << t.type << "] " << t.filename << " (" << t.size << " bytes)\n";
    }
    return 0;
  }

  if(sup_cmd->parsed())
  {
    imfwizard::SupplementalOptions opts;
    opts.title = sup_title;
    opts.issuer = sup_issuer;
    opts.ov_imp_dir = sup_ov_dir;
    opts.output_dir = sup_output_dir;
    opts.edit_rate_num = fps_num;
    opts.edit_rate_den = fps_den;

    imfwizard::SupplementalSegment seg;
    seg.entry_point = sup_entry_point;
    seg.duration = sup_duration;
    seg.video_dir = sup_video_dir;
    seg.audio_file = sup_audio_file;
    opts.segments.push_back(seg);

    auto result = imfwizard::create_supplemental_imp(opts);
    if(!result.success)
    {
      spdlog::error("Failed: {}", result.error);
      return 1;
    }

    std::cout << "Supplemental IMP created: " << result.output_dir << "\n";
    std::cout << "  CPL: urn:uuid:" << result.cpl_uuid << "\n";
    std::cout << "  PKL: urn:uuid:" << result.pkl_uuid << "\n";
    std::cout << "  New track files: " << result.new_track_files.size() << "\n";
    return 0;
  }

  if(encode_cmd->parsed())
  {
    imfwizard::EncodeOptions opts;
    opts.input_dir = enc_input;
    opts.output_dir = enc_output;
    opts.target_bitrate_mbps = enc_bitrate;
    opts.num_threads = enc_threads;
    opts.cinema_profile = enc_cinema;

    auto result = imfwizard::encode_to_j2k(opts);
    if(!result.success)
    {
      spdlog::error("Encoding failed: {}", result.error);
      return 1;
    }

    std::cout << "Encoded " << result.frame_count << " frames to " << result.output_dir << "\n";
    return 0;
  }

  if(val_cmd->parsed())
  {
    auto result = imfwizard::validate_with_photon(val_dir);
    if(result.valid)
    {
      std::cout << "IMP is valid.\n";
    }
    else
    {
      std::cout << "IMP validation issues:\n";
    }
    for(const auto& note : result.notes)
    {
      const char* sev = "INFO";
      if(note.severity == imfwizard::ValidationNote::Severity::error)
        sev = "ERROR";
      else if(note.severity == imfwizard::ValidationNote::Severity::warning)
        sev = "WARN";
      std::cout << "  [" << sev << "] " << note.message << "\n";
    }
    return result.valid ? 0 : 1;
  }

  if(trans_cmd->parsed())
  {
    imfwizard::TranscodeOptions opts;
    opts.input_file = trans_input;
    opts.output_dir = trans_output;
    opts.output_format = trans_format;
    opts.bit_depth = trans_bit_depth;
    opts.threads = trans_threads;

    auto result = imfwizard::transcode_to_sequence(opts);
    if(!result.success)
    {
      spdlog::error("Transcode failed: {}", result.error);
      return 1;
    }

    std::cout << "Transcoded " << result.frame_count << " frames (" << result.width << "x"
              << result.height << " @ " << result.fps << " fps) to " << result.output_dir << "\n";
    return 0;
  }

  if(extract_cmd->parsed())
  {
    imfwizard::ExtractOptions opts;
    opts.imp_dir = ext_imp_dir;
    opts.output_dir = ext_output;
    opts.output_format = ext_format;
    opts.start_frame = ext_start;
    opts.end_frame = ext_end;

    auto result = imfwizard::extract_from_imp(opts);
    if(!result.success)
    {
      spdlog::error("Extract failed: {}", result.error);
      return 1;
    }
    std::cout << "Extracted " << result.video_frames << " video frames, "
              << result.extracted_files.size() << " files to " << result.output_dir << "\n";
    return 0;
  }

  if(conform_cmd->parsed())
  {
    imfwizard::ConformOptions opts;
    opts.edl_file = conf_edl;
    opts.media_dir = conf_media;
    opts.output_dir = conf_output;
    opts.title = conf_title;
    opts.fps_num = fps_num;
    opts.fps_den = fps_den;

    auto result = imfwizard::conform_from_edl(opts);
    if(!result.success)
    {
      spdlog::error("Conform failed: {}", result.error);
      return 1;
    }
    std::cout << "Conformed " << result.entries.size() << " events, " << result.total_duration
              << " total frames\n";
    return 0;
  }

  if(watch_cmd->parsed())
  {
    imfwizard::WatchConfig config;
    config.watch_dir = watch_dir;
    config.output_dir = watch_output;
    config.on_status = [](const std::string& msg) { std::cout << "[watch] " << msg << "\n"; };

    std::atomic<bool> stop{false};
    std::cout << "Watching " << watch_dir << " (Ctrl+C to stop)...\n";
    imfwizard::watch_folder(config, stop);
    return 0;
  }

  if(report_cmd->parsed())
  {
    auto validation = imfwizard::validate_with_photon(rpt_imp_dir);
    imfwizard::QcReportOptions opts;
    opts.imp_dir = rpt_imp_dir;
    opts.validation = validation;
    opts.title = std::filesystem::path(rpt_imp_dir).filename().string();
    imfwizard::write_qc_report(opts, rpt_output);
    std::cout << "QC report written to " << rpt_output << "\n";
    return 0;
  }

  if(loudness_cmd->parsed())
  {
    if(loud_normalize && !loud_output.empty())
    {
      imfwizard::NormalizeOptions opts;
      opts.input_file = loud_input;
      opts.output_file = loud_output;
      opts.target_lufs = loud_target;
      auto r = imfwizard::normalize_loudness(opts);
      if(!r.success)
      {
        spdlog::error("{}", r.error);
        return 1;
      }
      std::cout << "Normalized to " << loud_output << "\n";
    }
    else
    {
      auto r = imfwizard::measure_loudness(loud_input);
      if(!r.success)
      {
        spdlog::error("{}", r.error);
        return 1;
      }
      std::cout << "Integrated: " << r.integrated_lufs << " LUFS\n"
                << "LRA: " << r.loudness_range_lu << " LU\n"
                << "True Peak: " << r.true_peak_dbtp << " dBTP\n"
                << "EBU R128: " << (r.compliant_r128 ? "PASS" : "FAIL") << "\n"
                << "ATSC A/85: " << (r.compliant_atsc ? "PASS" : "FAIL") << "\n";
    }
    return 0;
  }

  if(qc_cmd->parsed())
  {
    if(!qc_dir.empty())
    {
      imfwizard::FrameQcOptions opts;
      opts.j2k_dir = qc_dir;
      opts.max_bitrate_mbps = qc_max;
      opts.min_bitrate_mbps = qc_min;
      auto r = imfwizard::analyze_frame_qc(opts);
      if(!r.success)
      {
        spdlog::error("{}", r.error);
        return 1;
      }
      std::cout << "Frames: " << r.total_frames << "\n"
                << "Avg bitrate: " << r.average_bitrate_mbps << " Mbps\n"
                << "Peak: " << r.peak_bitrate_mbps << " Mbps\n"
                << "Over-budget: " << r.over_budget_count << "\n"
                << "Under-budget: " << r.under_budget_count << "\n";
    }
    if(!qc_ref.empty() && !qc_dist.empty())
    {
      imfwizard::QualityOptions opts;
      opts.reference = qc_ref;
      opts.distorted = qc_dist;
      auto r = imfwizard::compute_quality(opts);
      if(!r.success)
      {
        spdlog::error("{}", r.error);
        return 1;
      }
      std::cout << "VMAF: " << r.vmaf_score << "\n"
                << "PSNR: " << r.psnr_avg << " dB\n"
                << "SSIM: " << r.ssim << "\n";
    }
    return 0;
  }

  if(chmap_cmd->parsed())
  {
    imfwizard::ChannelMapOptions opts;
    opts.input_file = chmap_input;
    opts.output_file = chmap_output;
    if(chmap_target == "mono")
      opts.target_layout = imfwizard::ChannelLayout::Mono;
    else if(chmap_target == "stereo")
      opts.target_layout = imfwizard::ChannelLayout::Stereo;
    else if(chmap_target == "5.1")
      opts.target_layout = imfwizard::ChannelLayout::Surround_51;
    else if(chmap_target == "7.1")
      opts.target_layout = imfwizard::ChannelLayout::Surround_71;
    auto r = imfwizard::remap_channels(opts);
    if(!r.success)
    {
      spdlog::error("{}", r.error);
      return 1;
    }
    std::cout << "Remapped " << r.input_channels << "ch -> " << r.output_channels
              << "ch: " << r.output_file.string() << "\n";
    return 0;
  }

  if(upload_cmd->parsed())
  {
    if(up_aspera)
    {
      imfwizard::AsperaOptions opts;
      opts.source_dir = up_dir;
      opts.remote_host = up_host;
      opts.remote_path = up_remote_path;
      opts.username = up_user;
      opts.token = up_token;
      auto r = imfwizard::aspera_transfer(opts);
      if(!r.success)
      {
        spdlog::error("{}", r.error);
        return 1;
      }
      std::cout << "Transferred " << r.files_transferred << " files at " << r.effective_rate_mbps
                << " Mbps\n";
    }
    else
    {
      imfwizard::S3UploadOptions opts;
      opts.source_dir = up_dir;
      opts.bucket = up_bucket;
      opts.prefix = up_prefix;
      opts.region = up_region;
      opts.profile = up_profile;
      auto r = imfwizard::upload_to_s3(opts);
      if(!r.success)
      {
        spdlog::error("{}", r.error);
        return 1;
      }
      std::cout << "Uploaded " << r.files_uploaded << " files to s3://" << up_bucket << "/"
                << up_prefix << "\n";
    }
    return 0;
  }

  if(captions_cmd->parsed())
  {
    auto parsed = imfwizard::parse_captions(cap_input, cap_fps);
    if(!parsed.success)
    {
      spdlog::error("{}", parsed.error);
      return 1;
    }
    imfwizard::write_ttml_captions(parsed.entries, cap_output, cap_fps, 1);
    std::cout << "Converted " << parsed.entries.size() << " captions to " << cap_output << "\n";
    return 0;
  }

  if(wm_cmd->parsed())
  {
    imfwizard::WatermarkOptions opts;
    opts.input_dir = wm_input;
    opts.output_dir = wm_output;
    opts.payload = wm_payload;
    opts.strength = wm_strength;
    auto r = imfwizard::apply_watermark(opts);
    if(!r.success)
    {
      spdlog::error("{}", r.error);
      return 1;
    }
    std::cout << "Watermarked " << r.frames_watermarked << " frames\n";
    return 0;
  }

  if(profiles_cmd->parsed())
  {
    if(prof_name.empty())
    {
      std::cout << "Available delivery profiles:\n";
      for(const auto& name : imfwizard::available_profiles)
        std::cout << "  " << name << "\n";
    }
    else
    {
      auto p = imfwizard::get_profile(prof_name);
      std::cout << p.name << " — " << p.description << "\n"
                << "  Resolution: " << p.max_width << "x" << p.max_height << "\n"
                << "  Bit depth: " << p.bit_depth << "\n"
                << "  Max bitrate: " << p.max_bitrate_mbps << " Mbps\n"
                << "  HDR required: " << (p.requires_hdr ? "yes" : "no") << "\n"
                << "  App constraint: " << p.app_constraint << "\n"
                << "  Signing required: " << (p.require_signing ? "yes" : "no") << "\n";
    }
    return 0;
  }

  if(daemon_cmd->parsed())
  {
    imfwizard::JobQueueDaemon daemon;
    return daemon.run();
  }

  if(batch_cmd->parsed())
  {
    imfwizard::JobQueueClient client;

    if(batch_list_cmd->parsed())
    {
      if(!client.is_daemon_running())
      {
        std::cerr << "Daemon not running. Start with: imfwizard daemon\n";
        return 1;
      }
      auto jobs = client.list();
      if(jobs.empty())
      {
        std::cout << "No jobs in queue.\n";
        return 0;
      }
      std::cout << "ID  State      Progress  Type       Description\n";
      std::cout << "--- ---------- --------- ---------- -----------\n";
      for(auto& j : jobs)
      {
        char line[256];
        snprintf(line, sizeof(line), "%-3" PRIu64 " %-10s %5.0f%%    %-10s %s\n",
                 j.id, imfwizard::job_state_to_string(j.state).c_str(),
                 j.progress, imfwizard::job_type_to_string(j.type).c_str(),
                 j.description.c_str());
        std::cout << line;
      }
      return 0;
    }

    if(batch_add_cmd->parsed())
    {
      if(!client.is_daemon_running())
      {
        std::cerr << "Daemon not running. Start with: imfwizard daemon\n";
        return 1;
      }
      std::optional<uint64_t> dep;
      if(batch_dep > 0) dep = batch_dep;
      auto id = client.submit(imfwizard::job_type_from_string(batch_type),
                              batch_desc, batch_args, dep);
      if(id)
        std::cout << "Job " << *id << " submitted\n";
      else
      {
        std::cerr << "Failed to submit job\n";
        return 1;
      }
      return 0;
    }

    if(batch_cancel_cmd->parsed())
    {
      if(client.cancel(batch_cancel_id))
        std::cout << "Job " << batch_cancel_id << " cancelled\n";
      else
      {
        std::cerr << "Failed to cancel job " << batch_cancel_id << "\n";
        return 1;
      }
      return 0;
    }

    if(batch_status_cmd->parsed())
    {
      auto job = client.status(batch_status_id);
      if(job)
      {
        std::cout << "Job " << job->id << ": " << imfwizard::job_state_to_string(job->state)
                  << " (" << job->progress << "%) — " << job->description << "\n";
        if(!job->error.empty())
          std::cout << "  Error: " << job->error << "\n";
      }
      else
      {
        std::cerr << "Job " << batch_status_id << " not found\n";
        return 1;
      }
      return 0;
    }

    if(batch_pause_cmd->parsed())
    {
      if(client.pause())
        std::cout << "Queue paused\n";
      else
      {
        std::cerr << "Failed to pause (daemon not running?)\n";
        return 1;
      }
      return 0;
    }

    if(batch_resume_cmd->parsed())
    {
      if(client.resume())
        std::cout << "Queue resumed\n";
      else
      {
        std::cerr << "Failed to resume (daemon not running?)\n";
        return 1;
      }
      return 0;
    }

    if(batch_priority_cmd->parsed())
    {
      if(client.set_priority(batch_pri_id, batch_pri_val))
        std::cout << "Job " << batch_pri_id << " priority set to " << batch_pri_val << "\n";
      else
      {
        std::cerr << "Failed to set priority\n";
        return 1;
      }
      return 0;
    }
  }

  return 0;
}
