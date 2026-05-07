#pragma once

#include <filesystem>
#include <string>

namespace imfwizard
{

struct SignOptions
{
  std::filesystem::path cert_file;
  std::filesystem::path key_file;
  std::filesystem::path ca_file; // optional intermediate CA
};

// Sign an XML file in-place (enveloped XML-DSIG)
bool sign_xml(const std::filesystem::path& xml_path, const SignOptions& opts);

} // namespace imfwizard
