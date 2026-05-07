#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

struct FrameDiff
{
  uint32_t frame_number = 0;
  double psnr = 0; // Peak Signal-to-Noise Ratio (dB)
  double ssim = 0; // Structural Similarity (0–1)
  double mse = 0; // Mean Squared Error
  bool significant = false; // above threshold
};

struct CompareOptions
{
  std::filesystem::path imp_a; // first IMP directory
  std::filesystem::path imp_b; // second IMP directory
  double threshold_psnr = 40.0; // dB threshold for "significant" diff
  uint32_t sample_interval = 1; // compare every Nth frame (1 = all)
  std::filesystem::path output_dir; // optional: write diff frames here
  bool generate_report = true;
};

struct CompareResult
{
  uint32_t frames_compared = 0;
  uint32_t frames_different = 0;
  double avg_psnr = 0;
  double min_psnr = 0;
  double avg_ssim = 0;
  std::vector<FrameDiff> diffs; // only significant diffs
  std::filesystem::path report_path;
  bool identical = false;
  bool success = false;
  std::string error;
};

// Compare two IMPs frame-by-frame using PSNR/SSIM
CompareResult compare_imps(const CompareOptions& opts);

} // namespace imfwizard
