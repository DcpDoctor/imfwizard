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
#include "imfwizard/plugin.h"
#include "imfwizard/atmos.h"
#include "imfwizard/mca.h"
#include "imfwizard/audio_desc.h"
#include "imfwizard/lut.h"
#include "imfwizard/aces.h"
#include "imfwizard/cpl_annotation.h"
#include "imfwizard/partial_version.h"
#include "imfwizard/slate.h"
#include "imfwizard/subtitle_retime.h"
#include "imfwizard/sdi_output.h"
#include "imfwizard/imp_diff.h"
#include "imfwizard/otioz_import.h"
#include "imfwizard/multi_node.h"
#include "imfwizard/kdm_gen.h"
#include "imfwizard/mxf_playback.h"
#include "imfwizard/webhook.h"
#include "imfwizard/shell_completion.h"
#include "imfwizard/pdf_report.h"
#include "imfwizard/subtitle_convert.h"
#include "imfwizard/timecode.h"
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
  std::string watch_dir, watch_output, watch_custom_cmd;
  bool watch_recursive = false, watch_no_inotify = false;
  uint32_t watch_poll_ms = 2000, watch_stability_ms = 3000;
  std::vector<std::string> watch_actions_str;
  std::vector<std::string> watch_extensions;

  watch_cmd->add_option("-w,--watch", watch_dir, "Directory to watch")
      ->required()
      ->check(CLI::ExistingDirectory);
  watch_cmd->add_option("-o,--output", watch_output, "Output base directory")->required();
  watch_cmd->add_flag("-r,--recursive", watch_recursive, "Watch subdirectories recursively");
  watch_cmd->add_flag("--no-inotify", watch_no_inotify, "Disable inotify, use polling");
  watch_cmd->add_option("--poll-interval", watch_poll_ms, "Poll interval in ms")->default_val(2000);
  watch_cmd->add_option("--stability-delay", watch_stability_ms, "Wait time before processing")->default_val(3000);
  watch_cmd->add_option("--actions", watch_actions_str, "Actions: imp,qc,validate,checksum,hdr,custom");
  watch_cmd->add_option("--extensions", watch_extensions, "File extensions to watch (e.g. .mxf .wav)");
  watch_cmd->add_option("--custom-cmd", watch_custom_cmd, "Custom command ($FILE, $OUTPUT)");

  // === REPORT subcommand ===
  auto* report_cmd = app.add_subcommand("report", "Generate QC report for an IMP");
  std::string rpt_imp_dir, rpt_output;

  report_cmd->add_option("imp_dir", rpt_imp_dir, "IMP directory")
      ->required()
      ->check(CLI::ExistingDirectory);
  report_cmd->add_option("-o,--output", rpt_output, "Output HTML file")->required();

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

  // === SDI-PREVIEW subcommand ===
  auto* sdi_cmd = app.add_subcommand("sdi-preview", "Play frames over SDI via DeckLink (GStreamer)");
  std::string sdi_input, sdi_audio, sdi_pixel = "v210";
  uint32_t sdi_device = 0, sdi_fps_n = 24, sdi_fps_d = 1;
  uint32_t sdi_width = 1920, sdi_height = 1080;
  uint32_t sdi_start = 0, sdi_end = 0;
  bool sdi_loop = false, sdi_list = false;
  sdi_cmd->add_option("--input,-i", sdi_input, "J2K frame directory or MXF file");
  sdi_cmd->add_option("--audio", sdi_audio, "Audio file for embedded SDI audio");
  sdi_cmd->add_option("--device", sdi_device, "DeckLink device number")->default_val(0);
  sdi_cmd->add_option("--fps-num", sdi_fps_n, "Frame rate numerator")->default_val(24);
  sdi_cmd->add_option("--fps-den", sdi_fps_d, "Frame rate denominator")->default_val(1);
  sdi_cmd->add_option("--width", sdi_width, "Frame width")->default_val(1920);
  sdi_cmd->add_option("--height", sdi_height, "Frame height")->default_val(1080);
  sdi_cmd->add_option("--pixel-format", sdi_pixel, "Pixel format (v210, ARGB)")->default_val("v210");
  sdi_cmd->add_option("--start", sdi_start, "Start frame");
  sdi_cmd->add_option("--end", sdi_end, "End frame (0=all)");
  sdi_cmd->add_flag("--loop", sdi_loop, "Loop playback");
  sdi_cmd->add_flag("--list-devices", sdi_list, "List available SDI devices");

  // === IMP-DIFF subcommand ===
  auto* diff_cmd = app.add_subcommand("imp-diff", "Compare two IMF packages and show differences");
  std::string diff_imp_a, diff_imp_b;
  bool diff_hashes = false, diff_json = false, diff_show_unchanged = false;
  diff_cmd->add_option("--imp-a,-a", diff_imp_a, "First IMP directory (OV)")->required();
  diff_cmd->add_option("--imp-b,-b", diff_imp_b, "Second IMP directory (supplemental)")->required();
  diff_cmd->add_flag("--hashes", diff_hashes, "Include file hash comparison");
  diff_cmd->add_flag("--json", diff_json, "Output as JSON");
  diff_cmd->add_flag("--show-unchanged", diff_show_unchanged, "Show unchanged tracks");

  // === OTIOZ-IMPORT subcommand ===
  auto* otioz_cmd = app.add_subcommand("otioz-import", "Import OpenTimelineIO bundle (.otioz/.otio)");
  std::string otioz_input, otioz_output, otioz_title;
  bool otioz_generate_cpl = false, otioz_no_extract = false;
  double otioz_fps = 24.0;
  otioz_cmd->add_option("--input,-i", otioz_input, "OTIOZ/OTIO file path")->required();
  otioz_cmd->add_option("--output,-o", otioz_output, "Output directory")->required();
  otioz_cmd->add_option("--title,-t", otioz_title, "Title for generated CPL");
  otioz_cmd->add_option("--fps", otioz_fps, "Target FPS for CPL")->default_val(24.0);
  otioz_cmd->add_flag("--generate-cpl", otioz_generate_cpl, "Generate CPL from timeline");
  otioz_cmd->add_flag("--no-extract", otioz_no_extract, "Don't extract media files");

  // === MULTI-NODE subcommand ===
  auto* mnode_cmd = app.add_subcommand("multi-node", "Distribute encoding across multiple render nodes");
  std::string mnode_input, mnode_output, mnode_codec = "j2k";
  std::vector<std::string> mnode_nodes;
  uint32_t mnode_chunk = 100, mnode_bitrate = 250;
  uint16_t mnode_port = 9090;
  bool mnode_worker = false, mnode_verify = true;
  mnode_cmd->add_option("--input,-i", mnode_input, "Source frame directory");
  mnode_cmd->add_option("--output,-o", mnode_output, "Encoded output directory");
  mnode_cmd->add_option("--nodes,-n", mnode_nodes, "Node addresses (host:port)");
  mnode_cmd->add_option("--codec", mnode_codec, "Codec (j2k, prores, dnxhr)")->default_val("j2k");
  mnode_cmd->add_option("--chunk-size", mnode_chunk, "Frames per chunk")->default_val(100);
  mnode_cmd->add_option("--bitrate", mnode_bitrate, "Target bitrate (Mbps)")->default_val(250);
  mnode_cmd->add_option("--port", mnode_port, "Worker/coordinator port")->default_val(9090);
  mnode_cmd->add_flag("--worker", mnode_worker, "Run as render worker node");
  mnode_cmd->add_flag("--no-verify", mnode_verify, "Skip verification after encode");

  // === KDM subcommand ===
  auto* kdm_cmd = app.add_subcommand("kdm", "Generate Key Delivery Messages for encrypted DCP");
  std::string kdm_dcp, kdm_cpl, kdm_signer_key, kdm_signer_cert, kdm_output;
  std::string kdm_from, kdm_to, kdm_title;
  std::vector<std::string> kdm_recipients;
  bool kdm_forensic = false, kdm_interop = false;
  kdm_cmd->add_option("--dcp,-d", kdm_dcp, "DCP directory")->required();
  kdm_cmd->add_option("--cpl", kdm_cpl, "CPL file (if DCP has multiple)");
  kdm_cmd->add_option("--signer-key", kdm_signer_key, "Signer private key")->required();
  kdm_cmd->add_option("--signer-cert", kdm_signer_cert, "Signer certificate")->required();
  kdm_cmd->add_option("--recipients,-r", kdm_recipients, "Recipient cert files")->required();
  kdm_cmd->add_option("--output,-o", kdm_output, "Output directory")->required();
  kdm_cmd->add_option("--from", kdm_from, "Valid from (ISO 8601)")->required();
  kdm_cmd->add_option("--to", kdm_to, "Valid to (ISO 8601)")->required();
  kdm_cmd->add_option("--title,-t", kdm_title, "Content title override");
  kdm_cmd->add_flag("--forensic-mark", kdm_forensic, "Enable forensic marking");
  kdm_cmd->add_flag("--interop", kdm_interop, "Use Interop format (legacy)");

  // === DV81 subcommand ===
  auto* dv81_cmd = app.add_subcommand("dv81", "Inject Dolby Vision Profile 8.1 metadata (HDR10 compatible)");
  std::string dv81_input, dv81_rpu, dv81_output, dv81_display = "default", dv81_tonemap = "polynomial";
  uint16_t dv81_max_lum = 1000, dv81_min_lum = 0;
  bool dv81_from_p4 = false;
  dv81_cmd->add_option("--input,-i", dv81_input, "Source MXF file")->required();
  dv81_cmd->add_option("--rpu", dv81_rpu, "RPU metadata file")->required();
  dv81_cmd->add_option("--output,-o", dv81_output, "Output directory")->required();
  dv81_cmd->add_option("--target-display", dv81_display, "Target display (default, cinema, home_hdr)");
  dv81_cmd->add_option("--tone-map", dv81_tonemap, "Tone map method (polynomial, mmr, pivoted)");
  dv81_cmd->add_option("--max-luminance", dv81_max_lum, "Target max luminance (nits)")->default_val(1000);
  dv81_cmd->add_option("--min-luminance", dv81_min_lum, "Target min luminance (nits)")->default_val(0);
  dv81_cmd->add_flag("--from-profile4", dv81_from_p4, "Convert from Profile 4 RPU");

  // === MXF-PLAY subcommand ===
  auto* mxfplay_cmd = app.add_subcommand("mxf-play", "Play or probe MXF track files");
  std::string mxfplay_input, mxfplay_thumbdir;
  uint32_t mxfplay_start = 0, mxfplay_end = 0, mxfplay_interval = 24;
  bool mxfplay_probe = false, mxfplay_thumbs = false;
  mxfplay_cmd->add_option("--input,-i", mxfplay_input, "MXF file")->required();
  mxfplay_cmd->add_option("--start", mxfplay_start, "Start frame");
  mxfplay_cmd->add_option("--end", mxfplay_end, "End frame (0=all)");
  mxfplay_cmd->add_flag("--probe", mxfplay_probe, "Probe file info only (no playback)");
  mxfplay_cmd->add_flag("--thumbnails", mxfplay_thumbs, "Generate thumbnail images");
  mxfplay_cmd->add_option("--thumb-dir", mxfplay_thumbdir, "Thumbnail output directory");
  mxfplay_cmd->add_option("--thumb-interval", mxfplay_interval, "Thumbnail every N frames")->default_val(24);

  // === Webhook subcommand ===
  auto* webhook_cmd = app.add_subcommand("webhook", "Send or test webhook notifications");
  std::string webhook_url, webhook_secret, webhook_event = "ping", webhook_payload;
  uint32_t webhook_timeout = 10, webhook_retries = 3;
  bool webhook_test = false, webhook_no_ssl = false;
  webhook_cmd->add_option("--url", webhook_url, "Webhook POST endpoint URL")->required();
  webhook_cmd->add_option("--secret", webhook_secret, "HMAC signing secret");
  webhook_cmd->add_option("--event", webhook_event, "Event type")->default_val("ping");
  webhook_cmd->add_option("--payload", webhook_payload, "JSON payload string");
  webhook_cmd->add_option("--timeout", webhook_timeout, "Timeout in seconds")->default_val(10);
  webhook_cmd->add_option("--retries", webhook_retries, "Max retry attempts")->default_val(3);
  webhook_cmd->add_flag("--test", webhook_test, "Send a test ping event");
  webhook_cmd->add_flag("--no-verify-ssl", webhook_no_ssl, "Disable SSL verification");

  // === Completions subcommand ===
  auto* completions_cmd = app.add_subcommand("completions", "Generate shell tab-completion scripts");
  bool comp_bash = false, comp_zsh = false, comp_fish = false, comp_install = false;
  completions_cmd->add_flag("--bash", comp_bash, "Generate bash completion");
  completions_cmd->add_flag("--zsh", comp_zsh, "Generate zsh completion");
  completions_cmd->add_flag("--fish", comp_fish, "Generate fish completion");
  completions_cmd->add_flag("--install", comp_install, "Show installation instructions");

  // === PDF report subcommand ===
  auto* pdfreport_cmd = app.add_subcommand("pdf-report", "Generate PDF QC report for an IMP");
  std::string pdfreport_imp, pdfreport_output, pdfreport_title, pdfreport_author = "IMF Wizard";
  pdfreport_cmd->add_option("--input,-i", pdfreport_imp, "IMP directory")->required();
  pdfreport_cmd->add_option("--output,-o", pdfreport_output, "Output PDF path");
  pdfreport_cmd->add_option("--title", pdfreport_title, "Report title");
  pdfreport_cmd->add_option("--author", pdfreport_author, "Report author")->default_val("IMF Wizard");

  // === Subtitle convert subcommand ===
  auto* subconv_cmd = app.add_subcommand("subtitle-convert", "Convert between subtitle formats (SRT/TTML/WebVTT/SCC/STL/IMSC)");
  std::string subconv_input, subconv_output, subconv_format, subconv_lang = "en";
  double subconv_fps = 24.0, subconv_offset = 0.0;
  subconv_cmd->add_option("--input,-i", subconv_input, "Input subtitle file")->required();
  subconv_cmd->add_option("--output,-o", subconv_output, "Output file path")->required();
  subconv_cmd->add_option("--format,-f", subconv_format, "Target format (srt/ttml/webvtt/imsc)")->required();
  subconv_cmd->add_option("--lang", subconv_lang, "Language code")->default_val("en");
  subconv_cmd->add_option("--fps", subconv_fps, "Frame rate (for SCC timecodes)")->default_val(24.0);
  subconv_cmd->add_option("--offset", subconv_offset, "Time offset in seconds");

  // === Timecode convert subcommand ===
  auto* tcconv_cmd = app.add_subcommand("timecode-convert", "Convert timecode between frame rates");
  std::string tcconv_input;
  double tcconv_src_fps = 24.0, tcconv_dst_fps = 25.0;
  bool tcconv_src_drop = false, tcconv_dst_drop = false;
  tcconv_cmd->add_option("--tc,-t", tcconv_input, "Timecode to convert (HH:MM:SS:FF)")->required();
  tcconv_cmd->add_option("--src-fps", tcconv_src_fps, "Source frame rate")->default_val(24.0);
  tcconv_cmd->add_option("--dst-fps", tcconv_dst_fps, "Target frame rate")->default_val(25.0);
  tcconv_cmd->add_flag("--src-drop", tcconv_src_drop, "Source uses drop-frame");
  tcconv_cmd->add_flag("--dst-drop", tcconv_dst_drop, "Target uses drop-frame");

  // === Timecode drift subcommand ===
  auto* tcdrift_cmd = app.add_subcommand("timecode-drift", "Check timecode continuity in a video");
  std::string tcdrift_video, tcdrift_expected;
  double tcdrift_fps = 24.0;
  tcdrift_cmd->add_option("--input,-i", tcdrift_video, "Video file")->required();
  tcdrift_cmd->add_option("--expected,-e", tcdrift_expected, "Expected start timecode");
  tcdrift_cmd->add_option("--fps", tcdrift_fps, "Expected frame rate")->default_val(24.0);

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
    config.recursive = watch_recursive;
    config.use_inotify = !watch_no_inotify;
    config.poll_interval_ms = watch_poll_ms;
    config.stability_delay_ms = watch_stability_ms;
    config.include_extensions = watch_extensions;
    config.custom_command = watch_custom_cmd;
    config.on_status = [](const std::string& msg) { std::cout << "[watch] " << msg << "\n"; };

    // Parse action strings
    if(!watch_actions_str.empty())
    {
      config.actions.clear();
      for(const auto& a : watch_actions_str)
      {
        if(a == "imp")
          config.actions.push_back(imfwizard::WatchAction::CreateImp);
        else if(a == "qc")
          config.actions.push_back(imfwizard::WatchAction::AutoQc);
        else if(a == "validate")
          config.actions.push_back(imfwizard::WatchAction::Validate);
        else if(a == "checksum")
          config.actions.push_back(imfwizard::WatchAction::ChecksumVerify);
        else if(a == "hdr")
          config.actions.push_back(imfwizard::WatchAction::HdrValidate);
        else if(a == "custom")
          config.actions.push_back(imfwizard::WatchAction::Custom);
      }
    }

    std::atomic<bool> stop{false};
    std::cout << "Watching " << watch_dir << " (Ctrl+C to stop)...\n";
    imfwizard::watch_folder(config, stop);
    return 0;
  }

  if(report_cmd->parsed())
  {
    // Validation has moved to dcpdoctor; generate report shell
    imfwizard::ValidationResult validation;
    int ret = system(("dcpdoctor validate-imp \"" + rpt_imp_dir + "\" > /dev/null 2>&1").c_str());
    validation.valid = (ret == 0);
    if(!validation.valid)
    {
      imfwizard::ValidationNote note;
      note.severity = imfwizard::ValidationNote::Severity::warning;
      note.message = "dcpdoctor validation returned non-zero (run dcpdoctor validate-imp for details)";
      validation.notes.push_back(note);
    }
    imfwizard::QcReportOptions opts;
    opts.imp_dir = rpt_imp_dir;
    opts.validation = validation;
    opts.title = std::filesystem::path(rpt_imp_dir).filename().string();
    imfwizard::write_qc_report(opts, rpt_output);
    std::cout << "QC report written to " << rpt_output << "\n";
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
  if(sdi_cmd->parsed())
  {
    if(sdi_list)
    {
      auto devices = imfwizard::list_sdi_devices();
      if(devices.empty())
      {
        std::cout << "No DeckLink SDI devices found.\n";
        if(!imfwizard::decklink_available())
          std::cout << "Note: GStreamer decklink plugin not installed.\n";
      }
      else
      {
        for(auto& d : devices)
          std::cout << "  [" << d.index << "] " << d.name << " (" << d.driver << ")\n";
      }
      return 0;
    }
    if(sdi_input.empty())
    {
      std::cerr << "Error: --input is required (use --list-devices to enumerate)\n";
      return 1;
    }
    imfwizard::SdiOutputOptions opts;
    opts.input_dir = sdi_input;
    if(!sdi_audio.empty())
      opts.audio_file = sdi_audio;
    opts.device_index = sdi_device;
    opts.fps_num = sdi_fps_n;
    opts.fps_den = sdi_fps_d;
    opts.width = sdi_width;
    opts.height = sdi_height;
    opts.pixel_format = sdi_pixel;
    opts.start_frame = sdi_start;
    opts.end_frame = sdi_end;
    opts.loop = sdi_loop;
    auto result = imfwizard::sdi_preview(opts);
    if(!result.error.empty())
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "SDI playback complete.\n";
    return 0;
  }
  if(diff_cmd->parsed())
  {
    imfwizard::ImpDiffOptions opts;
    opts.imp_a = diff_imp_a;
    opts.imp_b = diff_imp_b;
    opts.include_hashes = diff_hashes;
    opts.json_output = diff_json;
    opts.show_unchanged = diff_show_unchanged;
    auto result = imfwizard::diff_packages(opts);
    if(!result.success)
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    if(diff_json)
    {
      std::cout << "{\"tracks_added\":" << result.tracks_added
                << ",\"tracks_removed\":" << result.tracks_removed
                << ",\"tracks_modified\":" << result.tracks_modified
                << ",\"segments_changed\":" << result.segments_changed << "}\n";
    }
    else
    {
      std::cout << "=== IMF Package Diff ===\n";
      std::cout << "Tracks added: " << result.tracks_added << "\n";
      std::cout << "Tracks removed: " << result.tracks_removed << "\n";
      std::cout << "Tracks modified: " << result.tracks_modified << "\n";
      std::cout << "Segments changed: " << result.segments_changed << "\n";
      if(result.cpl_title_changed)
        std::cout << "CPL title: CHANGED\n";
      if(result.edit_rate_changed)
        std::cout << "Edit rate: CHANGED\n";
      for(auto& d : result.track_diffs)
        std::cout << "  [" << d.status << "] " << d.track_id << " " << d.detail << "\n";
    }
    return 0;
  }
  if(otioz_cmd->parsed())
  {
    imfwizard::OtiozImportOptions opts;
    opts.input_file = otioz_input;
    opts.output_dir = otioz_output;
    opts.title = otioz_title;
    opts.fps = otioz_fps;
    opts.generate_cpl = otioz_generate_cpl;
    opts.extract_media = !otioz_no_extract;
    auto result = imfwizard::import_otioz(opts);
    if(!result.success)
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "OTIOZ imported: " << result.clips.size() << " clips\n";
    std::cout << "  Video tracks: " << result.video_tracks << "\n";
    std::cout << "  Audio tracks: " << result.audio_tracks << "\n";
    if(!result.extracted_dir.empty())
      std::cout << "  Media extracted to: " << result.extracted_dir.string() << "\n";
    if(!result.generated_cpl.empty())
      std::cout << "  CPL generated: " << result.generated_cpl.string() << "\n";
    return 0;
  }
  if(mnode_cmd->parsed())
  {
    if(mnode_worker)
    {
      return imfwizard::run_render_worker(mnode_port, 0);
    }
    if(mnode_input.empty() || mnode_output.empty() || mnode_nodes.empty())
    {
      std::cerr << "Error: --input, --output, and --nodes required\n";
      return 1;
    }
    imfwizard::MultiNodeOptions opts;
    opts.input_dir = mnode_input;
    opts.output_dir = mnode_output;
    opts.nodes = mnode_nodes;
    opts.codec = mnode_codec;
    opts.chunk_size = mnode_chunk;
    opts.bitrate = mnode_bitrate;
    opts.coordinator_port = mnode_port;
    opts.verify = mnode_verify;
    auto result = imfwizard::distribute_encode(opts);
    if(!result.success)
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "Distributed encode complete\n";
    std::cout << "  Frames: " << result.frames_encoded << "/" << result.total_frames << "\n";
    std::cout << "  Nodes used: " << result.nodes_used << "\n";
    std::cout << "  Elapsed: " << result.elapsed_seconds << "s\n";
    std::cout << "  Speedup: " << result.speedup_factor << "x\n";
    return 0;
  }
  if(kdm_cmd->parsed())
  {
    imfwizard::KdmOptions opts;
    opts.dcp_dir = kdm_dcp;
    if(!kdm_cpl.empty())
      opts.cpl_file = kdm_cpl;
    opts.signer_key = kdm_signer_key;
    opts.signer_cert = kdm_signer_cert;
    opts.output_dir = kdm_output;
    opts.valid_from = kdm_from;
    opts.valid_to = kdm_to;
    opts.content_title = kdm_title;
    opts.forensic_mark = kdm_forensic;
    opts.interop = kdm_interop;
    for(auto& cert_file : kdm_recipients)
    {
      imfwizard::KdmRecipient r;
      r.name = std::filesystem::path(cert_file).stem().string();
      r.certificate_file = cert_file;
      opts.recipients.push_back(r);
    }
    auto result = imfwizard::generate_kdm(opts);
    if(!result.success)
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "Generated " << result.kdms_generated << " KDM(s):\n";
    for(auto& f : result.output_files)
      std::cout << "  " << f.string() << "\n";
    return 0;
  }
  if(dv81_cmd->parsed())
  {
    imfwizard::DolbyVision81Config cfg;
    cfg.source_mxf = dv81_input;
    cfg.rpu_file = dv81_rpu;
    cfg.output_dir = dv81_output;
    cfg.target_display = dv81_display;
    cfg.tone_map_method = dv81_tonemap;
    cfg.target_max_luminance = dv81_max_lum;
    cfg.target_min_luminance = dv81_min_lum;
    imfwizard::DolbyVision81Result result;
    if(dv81_from_p4)
      result = imfwizard::convert_profile4_to_81(dv81_rpu, cfg);
    else
      result = imfwizard::inject_dv81_metadata(cfg);
    if(!result.success)
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "Dolby Vision Profile 8.1 applied\n";
    std::cout << "  Output: " << result.output_mxf.string() << "\n";
    std::cout << "  HDR10 compatible: " << (result.hdr10_compatible ? "yes" : "no") << "\n";
    return 0;
  }
  if(mxfplay_cmd->parsed())
  {
    if(mxfplay_probe)
    {
      auto info = imfwizard::probe_mxf(mxfplay_input);
      if(!info.success)
      {
        std::cerr << "Error: " << info.error << "\n";
        return 1;
      }
      std::cout << "MXF Info:\n";
      std::cout << "  Resolution: " << info.width << "x" << info.height << "\n";
      std::cout << "  FPS: " << info.fps << "\n";
      std::cout << "  Codec: " << info.codec << "\n";
      std::cout << "  Frames: " << info.total_frames << "\n";
      std::cout << "  Duration: " << info.duration_seconds << "s\n";
      return 0;
    }
    if(mxfplay_thumbs)
    {
      imfwizard::MxfPlaybackOptions opts;
      opts.mxf_file = mxfplay_input;
      opts.start_frame = mxfplay_start;
      opts.end_frame = mxfplay_end;
      opts.thumbnail_dir = mxfplay_thumbdir.empty() ? "/tmp/mxf_thumbs" : mxfplay_thumbdir;
      opts.thumbnail_interval = mxfplay_interval;
      auto result = imfwizard::generate_thumbnails(opts);
      if(!result.success)
      {
        std::cerr << "Error: " << result.error << "\n";
        return 1;
      }
      std::cout << "Generated " << result.thumbnails.size() << " thumbnails\n";
      return 0;
    }
    // Launch playback
    imfwizard::MxfPlaybackOptions opts;
    opts.mxf_file = mxfplay_input;
    opts.start_frame = mxfplay_start;
    opts.end_frame = mxfplay_end;
    imfwizard::launch_playback(opts);
    return 0;
  }

  // === Webhook handler ===
  if(webhook_cmd->parsed())
  {
    imfwizard::WebhookConfig config;
    config.url = webhook_url;
    config.secret = webhook_secret;
    config.timeout_seconds = webhook_timeout;
    config.max_retries = webhook_retries;
    config.verify_ssl = !webhook_no_ssl;

    if(webhook_test)
    {
      auto result = imfwizard::test_webhook(config);
      if(result.success)
      {
        std::cout << "Webhook test successful (HTTP " << result.http_status << ")\n";
        return 0;
      }
      std::cerr << "Webhook test failed: " << result.error << "\n";
      return 1;
    }

    imfwizard::WebhookEvent event;
    event.type = webhook_event;
    event.payload_json = webhook_payload.empty() ? "{}" : webhook_payload;
    auto result = imfwizard::send_webhook(config, event);
    if(result.success)
    {
      std::cout << "Webhook sent (HTTP " << result.http_status << ", "
                << result.attempts << " attempt(s))\n";
      return 0;
    }
    std::cerr << "Webhook failed: " << result.error << "\n";
    return 1;
  }

  // === Completions handler ===
  if(completions_cmd->parsed())
  {
    imfwizard::ShellType shell;
    if(comp_bash)
      shell = imfwizard::ShellType::bash;
    else if(comp_zsh)
      shell = imfwizard::ShellType::zsh;
    else if(comp_fish)
      shell = imfwizard::ShellType::fish;
    else
      shell = imfwizard::detect_shell();

    if(comp_install)
    {
      std::cout << imfwizard::completion_install_instructions(shell);
      return 0;
    }

    std::cout << imfwizard::generate_completion_script(shell);
    return 0;
  }

  // === PDF report handler ===
  if(pdfreport_cmd->parsed())
  {
    if(!imfwizard::pdf_renderer_available())
    {
      std::cerr << "Error: No PDF renderer found.\n"
                << "Install wkhtmltopdf or weasyprint:\n"
                << "  apt install wkhtmltopdf   # Debian/Ubuntu\n"
                << "  brew install wkhtmltopdf   # macOS\n"
                << "  pip install weasyprint     # Python\n";
      return 1;
    }

    // Run validation via dcpdoctor
    imfwizard::ValidationResult validation;
    int vret = system(("dcpdoctor validate-imp \"" + pdfreport_imp + "\" > /dev/null 2>&1").c_str());
    validation.valid = (vret == 0);
    if(!validation.valid)
    {
      imfwizard::ValidationNote note;
      note.severity = imfwizard::ValidationNote::Severity::warning;
      note.message = "dcpdoctor validation returned non-zero (run dcpdoctor validate-imp for details)";
      validation.notes.push_back(note);
    }

    imfwizard::PdfReportOptions opts;
    opts.imp_dir = pdfreport_imp;
    opts.output_path = pdfreport_output;
    opts.validation = validation;
    opts.title = pdfreport_title;
    opts.author = pdfreport_author;

    auto result = imfwizard::generate_pdf_report(opts);
    if(result.success)
    {
      std::cout << "PDF report: " << result.output_path.string() << "\n";
      return 0;
    }
    std::cerr << "Error: " << result.error << "\n";
    return 1;
  }

  // === Auto QC, MXF unwrap/probe, and checksum verification
  // NOTE: These tools have been moved to dcpdoctor (shared DCP/IMF validation tool).
  // Use: dcpdoctor auto-qc, dcpdoctor mxf-extract, dcpdoctor checksum-verify

  // === Subtitle convert handler ===
  if(subconv_cmd->parsed())
  {
    imfwizard::SubtitleConvertOptions opts;
    opts.input = subconv_input;
    opts.output = subconv_output;
    opts.target_format = imfwizard::parse_subtitle_format(subconv_format);
    opts.language = subconv_lang;
    opts.fps = subconv_fps;
    opts.offset_sec = subconv_offset;

    if(opts.target_format == imfwizard::SubtitleFormat::Unknown)
    {
      std::cerr << "Error: unknown target format '" << subconv_format << "'\n";
      std::cerr << "Valid formats: srt, ttml, webvtt, imsc\n";
      return 1;
    }

    auto result = imfwizard::convert_subtitles(opts);
    if(!result.success)
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }
    std::cout << "Converted " << result.cue_count << " cues -> " << result.output_path.string() << "\n";
    return 0;
  }

  // === Timecode convert handler ===
  if(tcconv_cmd->parsed())
  {
    imfwizard::TimecodeConvertOptions opts;
    opts.input_tc = tcconv_input;
    opts.source_fps = tcconv_src_fps;
    opts.target_fps = tcconv_dst_fps;
    opts.source_drop = tcconv_src_drop;
    opts.target_drop = tcconv_dst_drop;

    auto result = imfwizard::convert_timecode(opts);
    std::cout << tcconv_input << " @ " << tcconv_src_fps << "fps -> "
              << result.to_string() << " @ " << tcconv_dst_fps << "fps\n";
    return 0;
  }

  // === Timecode drift handler ===
  if(tcdrift_cmd->parsed())
  {
    imfwizard::TimecodeDriftOptions opts;
    opts.video_path = tcdrift_video;
    opts.expected_start_tc = tcdrift_expected;
    opts.expected_fps = tcdrift_fps;

    auto result = imfwizard::check_timecode_drift(opts);
    if(!result.success)
    {
      std::cerr << "Error: " << result.error << "\n";
      return 1;
    }

    if(!result.has_timecode)
    {
      std::cout << "No timecode track found in video\n";
      return 0;
    }

    std::cout << "Detected TC: " << result.detected_start_tc << "\n";
    std::cout << "Detected FPS: " << result.detected_fps << "\n";
    if(!tcdrift_expected.empty())
    {
      std::cout << "Drift: " << result.max_drift_frames << " frames ("
                << result.max_drift_seconds << "s)\n";
      std::cout << "Within tolerance: " << (result.within_tolerance ? "YES" : "NO") << "\n";
    }
    return result.within_tolerance ? 0 : 1;
  }

  return 0;
}
