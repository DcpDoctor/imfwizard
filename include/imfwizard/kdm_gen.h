#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

struct KdmRecipient
{
  std::string name;                        // Cinema / device name
  std::filesystem::path certificate_file;  // X.509 certificate (.pem)
  std::string thumbprint;                  // SHA-1 of cert (auto-computed)
};

struct KdmOptions
{
  std::filesystem::path dcp_dir;        // DCP directory with encrypted content
  std::filesystem::path cpl_file;       // CPL to target (if multiple in DCP)
  std::filesystem::path signer_key;     // Private key for signing KDM
  std::filesystem::path signer_cert;    // Certificate chain for signer
  std::vector<KdmRecipient> recipients;
  std::filesystem::path output_dir;     // Where to write KDM XML files

  // Validity window
  std::string valid_from;  // ISO 8601 datetime
  std::string valid_to;    // ISO 8601 datetime

  // Options
  bool forensic_mark = false; // Enable forensic marking flag
  std::string content_title;  // Override content title in KDM
  bool interop = false;       // Interop (old) vs SMPTE format
};

struct KdmResult
{
  bool success = false;
  std::string error;
  uint32_t kdms_generated = 0;
  std::vector<std::filesystem::path> output_files;
};

// Generate Key Delivery Messages for encrypted DCP content
KdmResult generate_kdm(const KdmOptions& opts);

// List content keys in a DCP (for selecting what to include)
std::vector<std::string> list_content_keys(const std::filesystem::path& dcp_dir);

} // namespace imfwizard
