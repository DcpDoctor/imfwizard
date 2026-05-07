#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace imfwizard
{

// Actions that can be triggered when new content is detected
enum class WatchAction
{
  CreateImp,      // Package into IMP
  AutoQc,         // Run automated QC checks
  Validate,       // Run Photon validation
  ChecksumVerify, // Verify existing checksums
  HdrValidate,    // Validate HDR metadata
  Custom          // User-defined command
};

// Watch folder configuration for auto-IMP creation
struct WatchConfig
{
  std::filesystem::path watch_dir;  // Directory to monitor
  std::filesystem::path output_dir; // IMP output base directory
  std::string default_title = "Auto IMP";
  std::string issuer = "IMF Wizard";
  uint32_t fps_num = 24;
  uint32_t fps_den = 1;
  bool auto_encode = true;      // Auto-encode non-J2K inputs
  bool validate_after = false;  // Run Photon validation after creation
  bool recursive = false;       // Watch subdirectories recursively
  bool use_inotify = true;      // Use inotify on Linux (falls back to polling)
  uint32_t poll_interval_ms = 2000;   // Polling interval in ms
  uint32_t stability_delay_ms = 3000; // Wait for file to stop changing

  // File extension filter (empty = accept all known types)
  std::vector<std::string> include_extensions;

  // Action pipeline — executed in order for each new content
  std::vector<WatchAction> actions = {WatchAction::CreateImp};

  // Custom command template ($FILE = detected path, $OUTPUT = output dir)
  std::string custom_command;

  // Callback for status updates
  std::function<void(const std::string&)> on_status;

  // Callback for each completed action (action, success, message)
  std::function<void(WatchAction, bool, const std::string&)> on_action_complete;
};

// Start watching a directory for new content
// Returns when stop is set to true
void watch_folder(const WatchConfig& config, std::atomic<bool>& stop);

} // namespace imfwizard
