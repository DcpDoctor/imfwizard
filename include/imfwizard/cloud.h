#pragma once

#include <filesystem>
#include <string>
#include <functional>

namespace imfwizard {

// S3 upload configuration
struct S3UploadOptions {
    std::filesystem::path source_dir;       // IMP directory to upload
    std::string bucket;                     // S3 bucket name
    std::string prefix;                     // Key prefix (folder in bucket)
    std::string region = "us-east-1";
    std::string profile;                    // AWS profile (empty = default)
    uint32_t max_concurrent = 4;
    std::function<void(const std::string&, double)> on_progress; // filename, progress 0-1
};

struct S3UploadResult {
    uint32_t files_uploaded = 0;
    uint64_t total_bytes = 0;
    bool success = false;
    std::string error;
};

// Upload IMP to S3 using AWS CLI
S3UploadResult upload_to_s3(const S3UploadOptions& opts);

// Check if AWS CLI is available
bool aws_cli_available();

} // namespace imfwizard
