#pragma once

#include <filesystem>
#include <string>

namespace imfwizard {

// Compute SHA-1 hash of a file, returned as base64
std::string sha1_base64(const std::filesystem::path& file);

} // namespace imfwizard
