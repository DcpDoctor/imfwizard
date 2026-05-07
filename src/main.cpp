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
#include "imfwizard/preview.h"
#include "imfwizard/prores.h"
#include "imfwizard/burnin.h"
#include "imfwizard/dcp_convert.h"
#include "imfwizard/batch_deliver.h"
#include "imfwizard/analytics.h"
#include "imfwizard/rest_api.h"
#include "imfwizard/edl_import.h"
#include "imfwizard/frame_compare.h"
#include "imfwizard/plugin.h"
#include "imfwizard/atmos.h"
#include "imfwizard/mca.h"
#include "imfwizard/audio_desc.h"
#include "imfwizard/lut.h"
#include "imfwizard/aces.h"
#include "imfwizard/av_sync.h"
#include "imfwizard/compliance.h"
#include "imfwizard/qc_report.h"
#include "imfwizard/cpl_annotation.h"
#include "imfwizard/partial_version.h"
#include "imfwizard/slate.h"
#include "imfwizard/subtitle_retime.h"
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
  std::string color_space = "BT.709";
  std::string preset;
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
  create_cmd
      ->add_option("--color-space", color_space, "Color space (BT.709, BT.2020, P3-D65, ACES)")
      ->default_val("BT.709");
  create_cmd->add_option(
      "--preset", preset,
      "Delivery preset (netflix, disney, amazon, apple, cinema2k, cinema4k, broadcast, archival)");
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
  batch_add_cmd
      ->add_option("-T,--type", batch_type,
                   "Job type (transcode/encode/create/validate/loudness/qc)")
      ->required();
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
  batch_priority_cmd->add_option("priority", batch_pri_val, "Priority (higher = first)")
      ->required();

  // === DAEMON subcommand ===
  auto* daemon_cmd = app.add_subcommand("daemon", "Run the job queue daemon");

  // === PREVIEW subcommand ===
  auto* preview_cmd = app.add_subcommand("preview", "Generate preview thumbnails from J2K");
  std::string prev_dir, prev_output;
  uint32_t prev_frame = 0, prev_width = 320, prev_count = 10;
  bool prev_strip = false;
  preview_cmd->add_option("-d,--dir", prev_dir, "J2K frame directory or IMP")->required();
  preview_cmd->add_option("-o,--output", prev_output, "Output directory")->required();
  preview_cmd->add_option("-f,--frame", prev_frame, "Frame number to preview")->default_val(0);
  preview_cmd->add_option("-w,--width", prev_width, "Thumbnail width")->default_val(320);
  preview_cmd->add_option("-n,--count", prev_count, "Number of thumbnails for strip")
      ->default_val(10);
  preview_cmd->add_flag("--strip", prev_strip, "Generate thumbnail strip (multiple frames)");

  // === PRORES subcommand ===
  auto* prores_cmd = app.add_subcommand("prores", "Create IMF package from ProRes input");
  std::string pr_input, pr_output, pr_title, pr_issuer = "IMF Wizard";
  uint32_t pr_fps_num = 24, pr_fps_den = 1;
  prores_cmd->add_option("-i,--input", pr_input, "ProRes .mov file")
      ->required()
      ->check(CLI::ExistingFile);
  prores_cmd->add_option("-o,--output", pr_output, "Output IMP directory")->required();
  prores_cmd->add_option("-t,--title", pr_title, "Content title")->required();
  prores_cmd->add_option("--issuer", pr_issuer, "Issuer name")->default_val("IMF Wizard");
  prores_cmd->add_option("--fps-num", pr_fps_num, "Frame rate numerator")->default_val(24);
  prores_cmd->add_option("--fps-den", pr_fps_den, "Frame rate denominator")->default_val(1);

  // === BURN-IN subcommand ===
  auto* burnin_cmd = app.add_subcommand("burn-in", "Burn subtitles into video");
  std::string bi_video, bi_subs, bi_output, bi_font = "DejaVu Sans";
  uint16_t bi_fontsize = 48;
  uint32_t bi_threads = 0;
  burnin_cmd->add_option("-i,--input", bi_video, "Input video file")
      ->required()
      ->check(CLI::ExistingFile);
  burnin_cmd->add_option("-s,--subtitles", bi_subs, "Subtitle file (SRT/TTML/SCC)")
      ->required()
      ->check(CLI::ExistingFile);
  burnin_cmd->add_option("-o,--output", bi_output, "Output video file")->required();
  burnin_cmd->add_option("--font", bi_font, "Font family")->default_val("DejaVu Sans");
  burnin_cmd->add_option("--font-size", bi_fontsize, "Font size")->default_val(48);
  burnin_cmd->add_option("--threads", bi_threads, "Threads (0=auto)")->default_val(0);

  // === TO-DCP subcommand ===
  auto* todcp_cmd = app.add_subcommand("to-dcp", "Convert IMF package to DCP");
  std::string dcp_imp, dcp_output, dcp_title, dcp_kind = "feature";
  todcp_cmd->add_option("-i,--imp", dcp_imp, "Input IMP directory")
      ->required()
      ->check(CLI::ExistingDirectory);
  todcp_cmd->add_option("-o,--output", dcp_output, "Output DCP directory")->required();
  todcp_cmd->add_option("-t,--title", dcp_title, "DCP content title");
  todcp_cmd->add_option("-k,--kind", dcp_kind, "Content kind")->default_val("feature");

  // === DELIVER subcommand ===
  auto* deliver_cmd = app.add_subcommand("deliver", "Batch deliver to multiple platforms");
  std::string del_video, del_audio, del_subtitle, del_title, del_output_base;
  std::vector<std::string> del_targets;
  uint32_t del_fps_num = 24, del_fps_den = 1, del_threads = 0;
  deliver_cmd->add_option("-v,--video", del_video, "Video directory or file")->required();
  deliver_cmd->add_option("-a,--audio", del_audio, "Audio file");
  deliver_cmd->add_option("-s,--subtitle", del_subtitle, "Subtitle file");
  deliver_cmd->add_option("-t,--title", del_title, "Content title")->required();
  deliver_cmd->add_option("-o,--output", del_output_base, "Output base directory")->required();
  deliver_cmd
      ->add_option("--targets", del_targets,
                   "Delivery targets (netflix,disney,amazon,apple,cinema,broadcast,archival)")
      ->required();
  deliver_cmd->add_option("--fps-num", del_fps_num, "Frame rate numerator")->default_val(24);
  deliver_cmd->add_option("--fps-den", del_fps_den, "Frame rate denominator")->default_val(1);
  deliver_cmd->add_option("--threads", del_threads, "Threads (0=auto)")->default_val(0);

  // === ANALYTICS subcommand ===
  auto* analytics_cmd =
      app.add_subcommand("analytics", "Generate bitrate analytics for J2K stream");
  std::string an_dir, an_output;
  uint32_t an_fps_num = 24, an_fps_den = 1;
  bool an_json = false;
  analytics_cmd->add_option("-d,--dir", an_dir, "J2K frame directory")->required();
  analytics_cmd->add_option("-o,--output", an_output, "Output JSON file (optional)");
  analytics_cmd->add_option("--fps-num", an_fps_num, "Frame rate numerator")->default_val(24);
  analytics_cmd->add_option("--fps-den", an_fps_den, "Frame rate denominator")->default_val(1);
  analytics_cmd->add_flag("--json", an_json, "Output as JSON");

  // === REST-API subcommand ===
  auto* rest_cmd = app.add_subcommand("rest-api", "Start REST API server");
  uint16_t rest_port = 8080;
  std::string rest_bind = "127.0.0.1";
  std::string rest_api_key;
  uint32_t rest_max_jobs = 4;
  rest_cmd->add_option("--port", rest_port, "Listen port")->default_val(8080);
  rest_cmd->add_option("--bind", rest_bind, "Bind address")->default_val("127.0.0.1");
  rest_cmd->add_option("--api-key", rest_api_key, "API key for auth");
  rest_cmd->add_option("--max-jobs", rest_max_jobs, "Max concurrent jobs")->default_val(4);

  // === EDL-IMPORT subcommand ===
  auto* edl_cmd = app.add_subcommand("edl-import", "Import edit from EDL/FCP XML");
  std::string edl_file, edl_media_dir, edl_output;
  edl_cmd->add_option("--input,-i", edl_file, "EDL or FCP XML file")->required();
  edl_cmd->add_option("--media-dir", edl_media_dir, "Media search directory");
  edl_cmd->add_option("--output,-o", edl_output, "Output directory");

  // === COMPARE subcommand ===
  auto* cmp_cmd = app.add_subcommand("compare", "Frame-accurate comparison of two IMPs");
  std::string cmp_imp_a, cmp_imp_b, cmp_output;
  bool cmp_psnr = true, cmp_ssim = false;
  cmp_cmd->add_option("--imp-a", cmp_imp_a, "First IMP directory")->required();
  cmp_cmd->add_option("--imp-b", cmp_imp_b, "Second IMP directory")->required();
  cmp_cmd->add_option("--output,-o", cmp_output, "Report output file");
  cmp_cmd->add_flag("--psnr", cmp_psnr, "Calculate PSNR");
  cmp_cmd->add_flag("--ssim", cmp_ssim, "Calculate SSIM");

  // === PLUGIN subcommand ===
  auto* plug_cmd = app.add_subcommand("plugin", "Manage and run plugins");
  std::string plug_dir, plug_hook;
  plug_cmd->add_option("--dir", plug_dir, "Plugin directory");
  plug_cmd->add_option("--hook", plug_hook, "Hook to execute");

  // === ATMOS subcommand ===
  auto* atmos_cmd = app.add_subcommand("atmos", "Import Dolby Atmos ADM BWF");
  std::string atmos_input, atmos_output;
  atmos_cmd->add_option("--input,-i", atmos_input, "ADM BWF input file")->required();
  atmos_cmd->add_option("--output,-o", atmos_output, "Output MXF path")->required();

  // === MCA subcommand ===
  auto* mca_cmd = app.add_subcommand("mca", "Generate MCA labels for audio");
  std::string mca_soundfield, mca_language = "en", mca_output;
  mca_cmd->add_option("--soundfield", mca_soundfield, "Soundfield (5.1, 7.1, stereo)")->required();
  mca_cmd->add_option("--language", mca_language, "RFC 5646 language tag")->default_val("en");
  mca_cmd->add_option("--output,-o", mca_output, "Output XML file")->required();

  // === AUDIO-DESC subcommand ===
  auto* ad_cmd = app.add_subcommand("audio-desc", "Create audio description track");
  std::string ad_main, ad_desc, ad_output;
  float ad_duck = -12.0f;
  ad_cmd->add_option("--main", ad_main, "Main audio file")->required();
  ad_cmd->add_option("--description", ad_desc, "Audio description file")->required();
  ad_cmd->add_option("--output,-o", ad_output, "Output file")->required();
  ad_cmd->add_option("--duck-level", ad_duck, "Ducking level in dB")->default_val(-12.0f);

  // === LUT subcommand ===
  auto* lut_cmd = app.add_subcommand("lut", "Apply 3D LUT to video");
  std::string lut_file, lut_input, lut_output;
  lut_cmd->add_option("--lut", lut_file, ".cube LUT file")->required();
  lut_cmd->add_option("--input,-i", lut_input, "Input video")->required();
  lut_cmd->add_option("--output,-o", lut_output, "Output video")->required();

  // === ACES subcommand ===
  auto* aces_cmd = app.add_subcommand("aces", "Apply ACES color pipeline");
  std::string aces_input, aces_output, aces_idt, aces_odt;
  aces_cmd->add_option("--input,-i", aces_input, "Input video")->required();
  aces_cmd->add_option("--output,-o", aces_output, "Output video")->required();
  aces_cmd->add_option("--idt", aces_idt, "Input Device Transform");
  aces_cmd->add_option("--odt", aces_odt, "Output Device Transform");

  // === AV-SYNC subcommand ===
  auto* avsync_cmd = app.add_subcommand("av-sync", "Detect/fix A/V sync drift");
  std::string avsync_video, avsync_audio, avsync_output;
  bool avsync_fix = false;
  avsync_cmd->add_option("--video", avsync_video, "Video track")->required();
  avsync_cmd->add_option("--audio", avsync_audio, "Audio track")->required();
  avsync_cmd->add_option("--output,-o", avsync_output, "Fixed output");
  avsync_cmd->add_flag("--fix", avsync_fix, "Auto-fix drift");

  // === COMPLIANCE subcommand ===
  auto* comply_cmd = app.add_subcommand("compliance", "Check platform delivery compliance");
  std::string comply_imp, comply_target = "netflix";
  comply_cmd->add_option("--imp", comply_imp, "IMP directory")->required();
  comply_cmd->add_option("--target", comply_target, "Platform target (netflix, disney, amazon, apple, cinema, broadcast)")->default_val("netflix");

  // === QC-REPORT subcommand ===
  auto* qcr_cmd = app.add_subcommand("qc-report", "Generate HTML QC report");
  std::string qcr_imp, qcr_output, qcr_title, qcr_client;
  bool qcr_loudness = true;
  qcr_cmd->add_option("--imp", qcr_imp, "IMP directory")->required();
  qcr_cmd->add_option("--output,-o", qcr_output, "Output HTML file")->required();
  qcr_cmd->add_option("--title", qcr_title, "Report title");
  qcr_cmd->add_option("--client", qcr_client, "Client name");
  qcr_cmd->add_flag("--no-loudness,!--loudness", qcr_loudness, "Skip loudness measurement");

  // === ANNOTATE subcommand ===
  auto* ann_cmd = app.add_subcommand("annotate", "Add annotation/revision to CPL");
  std::string ann_cpl, ann_text, ann_author, ann_revision;
  ann_cmd->add_option("--cpl", ann_cpl, "CPL file")->required();
  ann_cmd->add_option("--text", ann_text, "Annotation text")->required();
  ann_cmd->add_option("--author", ann_author, "Author name")->required();
  ann_cmd->add_option("--revision", ann_revision, "Revision number");

  // === PARTIAL-VERSION subcommand ===
  auto* pv_cmd = app.add_subcommand("partial-version", "Create supplemental partial version");
  std::string pv_ov, pv_video, pv_audio, pv_output, pv_title, pv_issuer;
  uint32_t pv_reel = 0, pv_start = 0, pv_end = 0;
  pv_cmd->add_option("--ov", pv_ov, "Original Version IMP dir")->required();
  pv_cmd->add_option("--video", pv_video, "Replacement video")->required();
  pv_cmd->add_option("--audio", pv_audio, "Replacement audio (optional)");
  pv_cmd->add_option("--output,-o", pv_output, "Output directory")->required();
  pv_cmd->add_option("--title", pv_title, "CPL title");
  pv_cmd->add_option("--issuer", pv_issuer, "Issuer");
  pv_cmd->add_option("--reel", pv_reel, "Reel index to replace");
  pv_cmd->add_option("--start-frame", pv_start, "Start frame");
  pv_cmd->add_option("--end-frame", pv_end, "End frame");

  // === SLATE subcommand ===
  auto* slate_cmd = app.add_subcommand("slate", "Generate slate/leader/countdown");
  std::string slate_type_str = "countdown", slate_output, slate_title, slate_tone_out;
  uint32_t slate_width = 1920, slate_height = 1080, slate_fps_n = 24, slate_fps_d = 1;
  uint32_t slate_duration = 0;
  bool slate_tone = false;
  slate_cmd->add_option("--type", slate_type_str, "Type: countdown, leader, bars, slate, black")->default_val("countdown");
  slate_cmd->add_option("--output,-o", slate_output, "Output directory")->required();
  slate_cmd->add_option("--width", slate_width, "Frame width")->default_val(1920);
  slate_cmd->add_option("--height", slate_height, "Frame height")->default_val(1080);
  slate_cmd->add_option("--fps-num", slate_fps_n, "FPS numerator")->default_val(24);
  slate_cmd->add_option("--fps-den", slate_fps_d, "FPS denominator")->default_val(1);
  slate_cmd->add_option("--duration", slate_duration, "Duration in frames (0=auto)");
  slate_cmd->add_option("--title", slate_title, "Slate title text");
  slate_cmd->add_flag("--tone", slate_tone, "Generate reference tone");
  slate_cmd->add_option("--tone-output", slate_tone_out, "Tone output file");

  // === RETIME subcommand ===
  auto* retime_cmd = app.add_subcommand("retime", "Retime subtitles between framerates");
  std::string rt_input, rt_output;
  uint32_t rt_src_fps_n = 24, rt_src_fps_d = 1, rt_tgt_fps_n = 25, rt_tgt_fps_d = 1;
  bool rt_stretch = true;
  retime_cmd->add_option("--input,-i", rt_input, "Input subtitle file")->required();
  retime_cmd->add_option("--output,-o", rt_output, "Output subtitle file")->required();
  retime_cmd->add_option("--src-fps-num", rt_src_fps_n, "Source FPS numerator")->default_val(24);
  retime_cmd->add_option("--src-fps-den", rt_src_fps_d, "Source FPS denominator")->default_val(1);
  retime_cmd->add_option("--tgt-fps-num", rt_tgt_fps_n, "Target FPS numerator")->default_val(25);
  retime_cmd->add_option("--tgt-fps-den", rt_tgt_fps_d, "Target FPS denominator")->default_val(1);
  retime_cmd->add_flag("--stretch,!--no-stretch", rt_stretch, "Stretch timing (vs recount frames)");


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
        snprintf(line, sizeof(line), "%-3" PRIu64 " %-10s %5.0f%%    %-10s %s\n", j.id,
                 imfwizard::job_state_to_string(j.state).c_str(), j.progress,
                 imfwizard::job_type_to_string(j.type).c_str(), j.description.c_str());
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
      if(batch_dep > 0)
        dep = batch_dep;
      auto id =
          client.submit(imfwizard::job_type_from_string(batch_type), batch_desc, batch_args, dep);
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
        std::cout << "Job " << job->id << ": " << imfwizard::job_state_to_string(job->state) << " ("
                  << job->progress << "%) — " << job->description << "\n";
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

  if(preview_cmd->parsed())
  {
    if(prev_strip)
    {
      imfwizard::ThumbnailStripOptions opts;
      opts.source_dir = prev_dir;
      opts.output_dir = prev_output;
      opts.count = prev_count;
      opts.width = prev_width;
      auto r = imfwizard::generate_thumbnail_strip(opts);
      if(!r.success)
      {
        spdlog::error("{}", r.error);
        return 1;
      }
      std::cout << "Generated " << r.thumbnails.size() << " thumbnails from " << r.total_frames
                << " frames\n";
    }
    else
    {
      imfwizard::PreviewOptions opts;
      opts.j2k_dir = prev_dir;
      opts.frame = prev_frame;
      opts.thumbnail_width = prev_width;
      opts.output_dir = prev_output;
      auto r = imfwizard::decode_preview_frame(opts);
      if(!r.success)
      {
        spdlog::error("{}", r.error);
        return 1;
      }
      std::cout << "Preview: " << r.frame.thumbnail_path << "\n";
    }
    return 0;
  }

  if(prores_cmd->parsed())
  {
    imfwizard::ProResImpOptions opts;
    opts.input_file = pr_input;
    opts.output_dir = pr_output;
    opts.title = pr_title;
    opts.issuer = pr_issuer;
    opts.fps_num = pr_fps_num;
    opts.fps_den = pr_fps_den;

    auto r = imfwizard::create_prores_imp(opts);
    if(!r.success)
    {
      spdlog::error("{}", r.error);
      return 1;
    }
    std::cout << "ProRes IMP created: " << r.output_dir << "\n"
              << "  CPL: urn:uuid:" << r.cpl_uuid << "\n"
              << "  Frames: " << r.frame_count << "\n";
    return 0;
  }

  if(burnin_cmd->parsed())
  {
    imfwizard::BurnInOptions opts;
    opts.video_input = bi_video;
    opts.subtitle_file = bi_subs;
    opts.output = bi_output;
    opts.font = bi_font;
    opts.font_size = bi_fontsize;
    opts.threads = bi_threads;

    auto r = imfwizard::burn_in_subtitles(opts);
    if(!r.success)
    {
      spdlog::error("{}", r.error);
      return 1;
    }
    std::cout << "Burned " << r.frame_count << " frames -> " << r.output_file << "\n";
    return 0;
  }

  if(todcp_cmd->parsed())
  {
    imfwizard::DcpConvertOptions opts;
    opts.imp_dir = dcp_imp;
    opts.output_dir = dcp_output;
    opts.content_title = dcp_title;
    opts.content_kind = dcp_kind;

    auto r = imfwizard::convert_imp_to_dcp(opts);
    if(!r.success)
    {
      spdlog::error("{}", r.error);
      return 1;
    }
    std::cout << "DCP created: " << r.output_dir << "\n"
              << "  CPL: urn:uuid:" << r.cpl_uuid << "\n"
              << "  Reels: " << r.reel_count << "\n"
              << "  Size: " << (r.total_size / (1024 * 1024)) << " MB\n";
    return 0;
  }

  if(deliver_cmd->parsed())
  {
    imfwizard::BatchDeliverOptions opts;
    opts.video_dir = del_video;
    opts.audio_file = del_audio;
    opts.subtitle_file = del_subtitle;
    opts.title = del_title;
    opts.fps_num = del_fps_num;
    opts.fps_den = del_fps_den;
    opts.threads = del_threads;

    for(const auto& t : del_targets)
    {
      imfwizard::DeliveryTarget dt;
      dt.profile = t;
      dt.output_dir = fs::path(del_output_base) / t;
      opts.targets.push_back(dt);
    }

    auto r = imfwizard::batch_deliver(opts);
    std::cout << "Batch delivery: " << r.succeeded << " succeeded, " << r.failed << " failed\n";
    for(const auto& tr : r.results)
    {
      std::cout << "  [" << (tr.success ? "OK" : "FAIL") << "] " << tr.profile;
      if(!tr.error.empty())
        std::cout << " -- " << tr.error;
      std::cout << "\n";
    }
    return r.all_success ? 0 : 1;
  }

  if(analytics_cmd->parsed())
  {
    imfwizard::AnalyticsOptions opts;
    opts.source = an_dir;
    opts.fps_num = an_fps_num;
    opts.fps_den = an_fps_den;

    auto r = imfwizard::compute_analytics(opts);
    if(!r.success)
    {
      spdlog::error("{}", r.error);
      return 1;
    }

    if(an_json || !an_output.empty())
    {
      auto json = imfwizard::analytics_to_json(r);
      if(!an_output.empty())
      {
        std::ofstream f(an_output);
        f << json;
        std::cout << "Analytics written to " << an_output << "\n";
      }
      else
      {
        std::cout << json;
      }
    }
    else
    {
      std::cout << "Frames: " << r.total_frames << "\n"
                << "Duration: " << r.duration_seconds << "s\n"
                << "Total size: " << (r.total_bytes / (1024 * 1024)) << " MB\n"
                << "Avg bitrate: " << r.avg_bitrate_mbps << " Mbps\n"
                << "Peak bitrate: " << r.peak_bitrate_mbps << " Mbps\n"
                << "Min bitrate: " << r.min_bitrate_mbps << " Mbps\n"
                << "Std dev: " << r.stddev_bitrate_mbps << " Mbps\n";
    }
    return 0;
  }


  // --- Dispatch new subcommands ---
  if(rest_cmd->parsed())
  {
    imfwizard::RestApiOptions opts;
    opts.port = rest_port;
    opts.bind_address = rest_bind;
    opts.api_key = rest_api_key;
    opts.max_concurrent_jobs = rest_max_jobs;
    auto result = imfwizard::start_rest_api(opts);
    if(!result.success)
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    return 0; // server exited normally
  }
  if(edl_cmd->parsed())
  {
    imfwizard::EdlParseOptions opts;
    opts.input_file = edl_file;
    if(!edl_media_dir.empty())
      opts.input_file = edl_media_dir; // unused but harmless
    auto result = imfwizard::parse_edl(opts);
    if(!result.error.empty())
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "Format: " << result.title << "\n";
    std::cout << "Events: " << result.events.size() << "\n";
    for(auto& e : result.events)
      std::cout << "  " << e.index << ": " << e.reel_name << " " << e.src_in << "-"
                << e.src_out << "\n";
    return 0;
  }
  if(cmp_cmd->parsed())
  {
    imfwizard::CompareOptions opts;
    opts.imp_a = cmp_imp_a;
    opts.imp_b = cmp_imp_b;
    if(!cmp_output.empty())
      opts.output_dir = cmp_output;
    auto result = imfwizard::compare_imps(opts);
    if(!result.error.empty())
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "Identical: " << (result.identical ? "yes" : "no") << "\n";
    std::cout << "PSNR: " << result.avg_psnr << " dB\n";
    if(cmp_ssim)
      std::cout << "SSIM: " << result.avg_ssim << "\n";
    return 0;
  }
  if(plug_cmd->parsed())
  {
    if(!plug_dir.empty())
    {
      auto plugins = imfwizard::discover_plugins(plug_dir);
      for(auto& p : plugins)
        std::cout << p.name << " v" << p.version << " — " << p.description << "\n";
    }
    return 0;
  }
  if(atmos_cmd->parsed())
  {
    imfwizard::AtmosImportOptions opts;
    opts.input_file = atmos_input;
    opts.output_dir = atmos_output;
    auto result = imfwizard::import_atmos(opts);
    if(!result.error.empty())
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "Atmos import complete: " << result.iab_mxf.string() << "\n";
    std::cout << "Beds: " << result.bed_channel_count << ", Objects: " << result.object_count << "\n";
    return 0;
  }
  if(mca_cmd->parsed())
  {
    imfwizard::McaSoundfield sf;
    if(mca_soundfield == "5.1")
      sf = imfwizard::soundfield_51();
    else if(mca_soundfield == "7.1")
      sf = imfwizard::soundfield_71();
    else
    {
      sf.name = mca_soundfield;
    }
    auto xml = imfwizard::generate_mca_xml(sf);
    std::ofstream out(mca_output);
    out << xml;
    std::cout << "MCA labels written to " << mca_output << "\n";
    return 0;
  }
  if(ad_cmd->parsed())
  {
    imfwizard::AudioDescOptions opts;
    opts.main_audio = ad_main;
    opts.ad_audio = ad_desc;
    opts.output_file = ad_output;
    opts.ad_mix_level = ad_duck;
    auto result = imfwizard::create_audio_description(opts);
    if(!result.error.empty())
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "Audio description created: " << result.output_file.string() << "\n";
    return 0;
  }
  if(lut_cmd->parsed())
  {
    imfwizard::LutOptions opts;
    opts.lut_file = lut_file;
    opts.input_dir = lut_input;
    opts.output_dir = lut_output;
    auto result = imfwizard::apply_lut(opts);
    if(!result.error.empty())
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "LUT applied: " << result.output_dir.string() << "\n";
    return 0;
  }
  if(aces_cmd->parsed())
  {
    imfwizard::AcesOptions opts;
    opts.input_dir = aces_input;
    opts.output_dir = aces_output;
    if(!aces_idt.empty())
      opts.idt_name = aces_idt;
    if(!aces_odt.empty())
      opts.odt_name = aces_odt;
    auto result = imfwizard::apply_aces_pipeline(opts);
    if(!result.error.empty())
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "ACES pipeline applied: " << result.output_dir.string() << "\n";
    return 0;
  }
  if(avsync_cmd->parsed())
  {
    imfwizard::AvSyncOptions opts;
    opts.video_file = avsync_video;
    opts.audio_file = avsync_audio;
    auto result = imfwizard::detect_av_sync(opts);
    if(!result.error.empty())
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "Drift: " << result.drift_ms << " ms\n";
    if(avsync_fix && !avsync_output.empty())
    {
      imfwizard::AvSyncFixOptions fix;
      fix.audio_file = avsync_audio;
      fix.output_file = avsync_output;
      fix.trim_samples = result.drift_samples;
      auto fr = imfwizard::fix_av_sync(fix);
      if(!fr.error.empty())
      {
        std::cerr << "Fix error: " << fr.error << "\n";
        return 1;
      }
      std::cout << "Fixed: " << fr.output_file.string() << "\n";
    }
    return 0;
  }
  if(comply_cmd->parsed())
  {
    imfwizard::ComplianceTarget target = imfwizard::ComplianceTarget::Netflix;
    if(comply_target == "disney")
      target = imfwizard::ComplianceTarget::Disney;
    else if(comply_target == "amazon")
      target = imfwizard::ComplianceTarget::Amazon;
    else if(comply_target == "apple")
      target = imfwizard::ComplianceTarget::Apple;
    else if(comply_target == "cinema")
      target = imfwizard::ComplianceTarget::Cinema2K;
    else if(comply_target == "broadcast")
      target = imfwizard::ComplianceTarget::BroadcastHD;

    imfwizard::ComplianceOptions copts;
    copts.imp_dir = comply_imp;
    copts.target = target;
    auto result = imfwizard::check_compliance(copts);
    std::cout << "Target: " << comply_target << "\n";
    std::cout << "Passed: " << (result.compliant ? "YES" : "NO") << "\n";
    for(auto& c : result.checks)
    {
      std::cout << "  [" << (c.passed ? "PASS" : "FAIL") << "] " << c.rule;
      if(!c.passed)
        std::cout << " — " << c.description;
      std::cout << "\n";
    }
    return 0;
  }
  if(qcr_cmd->parsed())
  {
    imfwizard::DetailedQcOptions opts;
    opts.imp_dir = qcr_imp;
    opts.output_file = qcr_output;
    opts.title = qcr_title;
    opts.client = qcr_client;
    opts.include_loudness = qcr_loudness;
    auto result = imfwizard::generate_detailed_qc(opts);
    if(!result.error.empty())
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "QC report generated: " << result.output_file.string() << "\n";
    return 0;
  }
  if(ann_cmd->parsed())
  {
    imfwizard::CplAnnotation ann;
    ann.text = ann_text;
    ann.author = ann_author;
    ann.revision = ann_revision;
    bool ok = imfwizard::annotate_cpl(ann_cpl, ann);
    if(!ok)
    {
      std::cerr << "Error: Failed to annotate CPL\n";
      return 1;
    }
    std::cout << "Annotation added to " << ann_cpl << "\n";
    return 0;
  }
  if(pv_cmd->parsed())
  {
    imfwizard::PartialVersionOptions opts;
    opts.ov_imp_dir = pv_ov;
    opts.replacement_video = pv_video;
    if(!pv_audio.empty())
      opts.replacement_audio = pv_audio;
    opts.output_dir = pv_output;
    opts.title = pv_title;
    opts.issuer = pv_issuer;
    if(pv_end > pv_start)
    {
      imfwizard::ReelSegment seg;
      seg.reel_index = pv_reel;
      seg.start_frame = pv_start;
      seg.end_frame = pv_end;
      opts.segments.push_back(seg);
    }
    auto result = imfwizard::create_partial_version(opts);
    if(!result.error.empty())
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "Partial version created: " << result.output_dir.string() << "\n";
    std::cout << "CPL: " << result.cpl_uuid << "\n";
    return 0;
  }
  if(slate_cmd->parsed())
  {
    imfwizard::SlateOptions opts;
    if(slate_type_str == "countdown")
      opts.type = imfwizard::SlateType::Countdown;
    else if(slate_type_str == "leader")
      opts.type = imfwizard::SlateType::AcademyLeader;
    else if(slate_type_str == "bars")
      opts.type = imfwizard::SlateType::ColorBars;
    else if(slate_type_str == "slate")
      opts.type = imfwizard::SlateType::Slate;
    else if(slate_type_str == "black")
      opts.type = imfwizard::SlateType::Black;
    opts.output_dir = slate_output;
    opts.width = slate_width;
    opts.height = slate_height;
    opts.fps_num = slate_fps_n;
    opts.fps_den = slate_fps_d;
    opts.duration_frames = slate_duration;
    opts.title = slate_title;
    opts.generate_tone = slate_tone;
    if(!slate_tone_out.empty())
      opts.tone_output = slate_tone_out;
    auto result = imfwizard::generate_slate(opts);
    if(!result.error.empty())
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "Slate generated: " << result.output_dir.string() << "\n";
    std::cout << "Frames: " << result.frames_generated << "\n";
    return 0;
  }
  if(retime_cmd->parsed())
  {
    imfwizard::SubtitleRetimeOptions opts;
    opts.input_file = rt_input;
    opts.output_file = rt_output;
    opts.source_fps_num = rt_src_fps_n;
    opts.source_fps_den = rt_src_fps_d;
    opts.target_fps_num = rt_tgt_fps_n;
    opts.target_fps_den = rt_tgt_fps_d;
    opts.stretch = rt_stretch;
    auto result = imfwizard::retime_subtitles(opts);
    if(!result.error.empty())
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "Retimed: " << result.output_file.string() << "\n";
    std::cout << "Entries: " << result.entries_processed << "\n";
    return 0;
  }

  return 0;
}
