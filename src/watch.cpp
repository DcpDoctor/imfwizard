#include "imfwizard/watch.h"
#include "imfwizard/imp.h"
#include "imfwizard/validate.h"
#include <spdlog/spdlog.h>
#include <thread>
#include <chrono>
#include <set>

namespace imfwizard {

void watch_folder(const WatchConfig& config, std::atomic<bool>& stop)
{
    namespace fs = std::filesystem;

    fs::create_directories(config.watch_dir);
    fs::create_directories(config.output_dir);

    std::set<std::string> processed;
    spdlog::info("Watching {} for new content...", config.watch_dir.string());

    if (config.on_status)
        config.on_status("Watching " + config.watch_dir.string());

    while (!stop.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(2));

        for (const auto& entry : fs::directory_iterator(config.watch_dir)) {
            if (!entry.is_directory()) continue;

            auto dir_name = entry.path().filename().string();
            if (processed.count(dir_name)) continue;

            // Check if directory has media files
            bool has_video = false, has_audio = false;
            fs::path video_dir, audio_file;

            for (const auto& f : fs::directory_iterator(entry.path())) {
                auto ext = f.path().extension().string();
                if (ext == ".j2c" || ext == ".j2k" || ext == ".dpx" ||
                    ext == ".tiff" || ext == ".tif" || ext == ".exr" ||
                    ext == ".png") {
                    has_video = true;
                    video_dir = entry.path();
                } else if (ext == ".wav") {
                    has_audio = true;
                    audio_file = f.path();
                }
            }

            if (!has_video) continue;

            spdlog::info("New content detected: {}", dir_name);
            if (config.on_status)
                config.on_status("Processing " + dir_name);

            processed.insert(dir_name);

            ImpOptions opts;
            opts.title = dir_name;
            opts.issuer = config.issuer;
            opts.video_dir = video_dir;
            opts.audio_file = audio_file;
            opts.output_dir = config.output_dir / dir_name;
            opts.edit_rate_num = config.fps_num;
            opts.edit_rate_den = config.fps_den;

            auto result = create_ov_imp(opts);
            if (result.success) {
                spdlog::info("Created IMP: {}", result.output_dir.string());
                if (config.on_status)
                    config.on_status("Created: " + dir_name);

                if (config.validate_after) {
                    auto vr = validate_with_photon(result.output_dir);
                    if (!vr.valid)
                        spdlog::warn("Validation issues for {}", dir_name);
                }
            } else {
                spdlog::error("Failed to create IMP for {}: {}", dir_name, result.error);
                if (config.on_status)
                    config.on_status("FAILED: " + dir_name + " — " + result.error);
            }
        }
    }

    spdlog::info("Watch folder stopped.");
}

} // namespace imfwizard
