#include "imfwizard/aspera.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <chrono>

namespace imfwizard {

bool ascp_available()
{
#ifdef _WIN32
    return system("where ascp >NUL 2>&1") == 0;
#else
    return system("which ascp >/dev/null 2>&1") == 0;
#endif
}

AsperaResult aspera_transfer(const AsperaOptions& opts)
{
    AsperaResult result;

    if (!ascp_available()) {
        result.error = "ascp (Aspera) not found in PATH. Install IBM Aspera CLI.";
        return result;
    }

    if (!std::filesystem::exists(opts.source_dir)) {
        result.error = "Source directory not found: " + opts.source_dir.string();
        return result;
    }

    // Count files
    uint64_t total_bytes = 0;
    uint32_t file_count = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(opts.source_dir)) {
        if (entry.is_regular_file()) {
            total_bytes += entry.file_size();
            ++file_count;
        }
    }

    spdlog::info("Aspera transfer: {} files ({} bytes) to {}:{}",
                 file_count, total_bytes, opts.remote_host, opts.remote_path);

    // Build ascp command
    std::ostringstream cmd;
    cmd << "ascp";
    cmd << " -l " << opts.target_rate_mbps << "m";
    cmd << " -m " << opts.min_rate_mbps << "m";

    if (opts.encrypt)
        cmd << " -T"; // Encryption

    if (opts.use_ssh_key)
        cmd << " -i " << opts.token;

    cmd << " -d"; // Create target directory
    cmd << " " << opts.source_dir.string() << "/";
    cmd << " " << opts.username << "@" << opts.remote_host << ":" << opts.remote_path;
    cmd << " 2>&1";

    auto start = std::chrono::steady_clock::now();
    int ret = system(cmd.str().c_str());
    auto end = std::chrono::steady_clock::now();

    double elapsed = std::chrono::duration<double>(end - start).count();

    if (ret != 0) {
        result.error = "ascp failed with code " + std::to_string(ret);
        return result;
    }

    result.files_transferred = file_count;
    result.total_bytes = total_bytes;
    result.elapsed_seconds = elapsed;
    result.effective_rate_mbps = (total_bytes * 8.0) / (elapsed * 1e6);
    result.success = true;

    spdlog::info("Aspera transfer complete: {:.1f} Mbps effective rate", result.effective_rate_mbps);
    return result;
}

} // namespace imfwizard
