#include <spdlog/spdlog.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "imfwizard/profiles.h"
#include "imfwizard/captions.h"
#include "imfwizard/channel_map.h"
#include "dcpdoctor/qc.h"
#include "imfwizard/watermark.h"
#include "imfwizard/accessibility.h"

namespace fs = std::filesystem;

static fs::path test_tmp()
{
  auto p = fs::temp_directory_path() / "imfwizard_integration_test";
  fs::create_directories(p);
  return p;
}

static void generate_fake_j2k_frames(const fs::path& dir, int count, size_t frame_size = 8192)
{
  fs::create_directories(dir);
  for(int i = 0; i < count; ++i)
  {
    auto path = dir / (std::to_string(i) + ".j2c");
    std::ofstream f(path, std::ios::binary);
    uint8_t header[] = {0xFF, 0x4F, 0xFF, 0x51};
    f.write(reinterpret_cast<char*>(header), 4);
    std::vector<uint8_t> body(frame_size - 4, 0xAB);
    f.write(reinterpret_cast<char*>(body.data()), body.size());
  }
}

static void generate_srt_file(const fs::path& path)
{
  std::ofstream f(path);
  f << "1\n00:00:01,000 --> 00:00:03,000\nHello World\n\n"
    << "2\n00:00:04,000 --> 00:00:06,000\nSecond subtitle\n\n";
}

static void test_profiles()
{
  using namespace imfwizard;
  spdlog::info("  profiles... ");

  auto netflix = get_profile("netflix");
  assert(netflix.name == "netflix");
  assert(netflix.max_width == 3840);
  assert(netflix.requires_hdr == true);
  assert(netflix.app_constraint == "App2E+");

  auto cinema = get_profile("cinema-4k");
  assert(cinema.max_width == 4096);
  assert(cinema.app_constraint == "App4");
  assert(cinema.require_signing == true);

  auto archival = get_profile("archival");
  assert(archival.app_constraint == "App5");
  assert(archival.bit_depth == 16);

  spdlog::info("OK");
}

static void test_captions()
{
  using namespace imfwizard;
  spdlog::info("  captions... ");

  auto tmp = test_tmp();
  auto srt = tmp / "test.srt";
  generate_srt_file(srt);

  auto result = parse_captions(srt, 24);
  assert(result.success);
  assert(result.entries.size() == 2);
  assert(result.entries[0].text == "Hello World");
  assert(result.format == CaptionFormat::SRT);

  auto ttml = captions_to_ttml(result.entries, 24, 1);
  assert(ttml.find("<tt") != std::string::npos);
  assert(ttml.find("Hello World") != std::string::npos);

  fs::remove_all(tmp);
  spdlog::info("OK");
}

static void test_channel_layout()
{
  using namespace imfwizard;
  spdlog::info("  channel_map... ");

  assert(channel_count(ChannelLayout::Mono) == 1);
  assert(channel_count(ChannelLayout::Stereo) == 2);
  assert(channel_count(ChannelLayout::Surround_51) == 6);
  assert(channel_count(ChannelLayout::Surround_71) == 8);
  assert(std::string(layout_to_ffmpeg(ChannelLayout::Stereo)) == "stereo");
  assert(std::string(layout_to_ffmpeg(ChannelLayout::Surround_51)) == "5.1");

  spdlog::info("OK");
}

static void test_frame_qc()
{
  using namespace dcpdoctor;
  spdlog::info("  frame_qc... ");

  auto tmp = test_tmp() / "qc_frames";
  generate_fake_j2k_frames(tmp, 48, 10000);

  FrameQcOptions opts;
  opts.j2k_dir = tmp;
  opts.fps_num = 24;
  opts.fps_den = 1;
  opts.max_bitrate_mbps = 5.0;
  opts.min_bitrate_mbps = 0.1;

  auto result = analyze_frame_qc(opts);
  assert(result.success);
  assert(result.total_frames == 48);
  assert(result.average_bitrate_mbps > 0);
  assert(result.frames.size() == 48);

  fs::remove_all(tmp);
  spdlog::info("OK");
}

static void test_watermark()
{
  using namespace imfwizard;
  spdlog::info("  watermark... ");

  auto tmp = test_tmp() / "wm_test";
  auto input_dir = tmp / "input";
  auto output_dir = tmp / "output";
  generate_fake_j2k_frames(input_dir, 10, 4096);

  WatermarkOptions opts;
  opts.input_dir = input_dir;
  opts.output_dir = output_dir;
  opts.payload = "TEST123";
  opts.strength = 3;

  auto result = apply_watermark(opts);
  assert(result.success);
  assert(result.frames_watermarked == 10);
  assert(fs::exists(output_dir));

  auto detect = detect_watermark(output_dir, 3);
  assert(detect.detected);
  assert(detect.payload.find("TEST") != std::string::npos);

  fs::remove_all(tmp);
  spdlog::info("OK");
}

static void test_accessibility()
{
  using namespace imfwizard;
  spdlog::info("  accessibility... ");

  std::vector<AccessibilityTrack> tracks = {
      {AccessibilityType::AudioDescription, "en", "English Audio Description"},
      {AccessibilityType::HearingImpaired, "en", "English SDH"},
      {AccessibilityType::SignLanguage, "en", "BSL Sign Language"},
  };

  auto xml = generate_accessibility_xml(tracks);
  assert(xml.find("audio-description") != std::string::npos);
  assert(xml.find("hearing-impaired") != std::string::npos);
  assert(xml.find("sign-language-video") != std::string::npos);

  spdlog::info("OK");
}

int main()
{
  spdlog::info("Integration tests:");
  test_profiles();
  test_captions();
  test_channel_layout();
  test_frame_qc();
  test_watermark();
  test_accessibility();
  spdlog::info("All integration tests passed!");
  return 0;
}
