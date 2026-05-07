#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

struct DeliveryTarget
{
  std::string profile; // e.g. "netflix", "disney", "amazon", "apple"
  std::filesystem::path output_dir; // Per-target output
};

struct BatchDeliverOptions
{
  std::filesystem::path video_dir; // Source video (J2K or image sequence)
  std::filesystem::path audio_file; // Source audio
  std::filesystem::path subtitle_file; // Optional subtitle
  std::string title;
  std::string issuer = "IMF Wizard";
  std::vector<DeliveryTarget> targets; // Which platforms to deliver to
  uint32_t fps_num = 24;
  uint32_t fps_den = 1;
  uint32_t threads = 0;
};

struct BatchDeliverResult
{
  struct TargetResult
  {
    std::string profile;
    std::filesystem::path output_dir;
    bool success = false;
    std::string error;
  };

  std::vector<TargetResult> results;
  uint32_t succeeded = 0;
  uint32_t failed = 0;
  bool all_success = false;
};

// Create IMPs for multiple delivery platforms in one pass
BatchDeliverResult batch_deliver(const BatchDeliverOptions& opts);

// List of all available delivery targets
extern const std::vector<std::string> available_delivery_targets;

} // namespace imfwizard
