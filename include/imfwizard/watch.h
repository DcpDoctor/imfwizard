#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <atomic>

namespace imfwizard {

// Watch folder configuration for auto-IMP creation
struct WatchConfig {
    std::filesystem::path watch_dir;        // Directory to monitor
    std::filesystem::path output_dir;       // IMP output base directory
    std::string default_title = "Auto IMP";
    std::string issuer = "IMF Wizard";
    uint32_t fps_num = 24;
    uint32_t fps_den = 1;
    bool auto_encode = true;                // Auto-encode non-J2K inputs
    bool validate_after = false;            // Run Photon validation after creation

    // Callback for status updates
    std::function<void(const std::string&)> on_status;
};

// Start watching a directory for new content
// Returns when stop is set to true
void watch_folder(const WatchConfig& config, std::atomic<bool>& stop);

} // namespace imfwizard
