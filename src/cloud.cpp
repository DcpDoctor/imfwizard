#include "imfwizard/cloud.h"
#include <spdlog/spdlog.h>
#include <sstream>

namespace imfwizard {

bool aws_cli_available()
{
#ifdef _WIN32
    return system("where aws >NUL 2>&1") == 0;
#else
    return system("which aws >/dev/null 2>&1") == 0;
#endif
}

S3UploadResult upload_to_s3(const S3UploadOptions& opts)
{
    S3UploadResult result;

    if (!aws_cli_available()) {
        result.error = "AWS CLI not found in PATH";
        return result;
    }

    if (!std::filesystem::exists(opts.source_dir)) {
        result.error = "Source directory not found: " + opts.source_dir.string();
        return result;
    }

    // Count files and total size
    uint64_t total_bytes = 0;
    uint32_t file_count = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(opts.source_dir)) {
        if (entry.is_regular_file()) {
            total_bytes += entry.file_size();
            ++file_count;
        }
    }

    spdlog::info("Uploading {} files ({} bytes) to s3://{}/{}",
                 file_count, total_bytes, opts.bucket, opts.prefix);

    // Use aws s3 sync for efficient upload
    std::ostringstream cmd;
    cmd << "aws s3 sync " << opts.source_dir.string()
        << " s3://" << opts.bucket << "/" << opts.prefix;

    if (!opts.region.empty())
        cmd << " --region " << opts.region;
    if (!opts.profile.empty())
        cmd << " --profile " << opts.profile;

    cmd << " 2>&1";

    int ret = system(cmd.str().c_str());
    if (ret != 0) {
        result.error = "aws s3 sync failed with code " + std::to_string(ret);
        return result;
    }

    result.files_uploaded = file_count;
    result.total_bytes = total_bytes;
    result.success = true;
    spdlog::info("Upload complete: {} files", file_count);

    return result;
}

} // namespace imfwizard
