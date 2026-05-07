#include "imfwizard/signature.h"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <spdlog/spdlog.h>
#include <fstream>
#include <stdexcept>

namespace imfwizard
{

bool sign_xml(const std::filesystem::path& xml_path, const SignOptions& opts)
{
  // TODO: Implement XML-DSIG enveloped signature
  // This requires xmlsec1 or manual construction of:
  //   1. Canonicalize the SignedInfo element (C14N)
  //   2. Compute digest of the referenced element
  //   3. Sign with RSA/SHA-256
  //   4. Embed Signature element in the XML
  spdlog::warn("XML signing not yet implemented — skipping {}", xml_path.string());
  return false;
}

} // namespace imfwizard
