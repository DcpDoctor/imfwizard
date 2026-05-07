#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

#include "imfwizard/kdm_gen.h"
#include "imfwizard/portable.h"

namespace imfwizard
{

namespace
{

// Generate a UUID v4 (simplified, not cryptographically secure)
std::string generate_uuid()
{
  static uint32_t counter = 0;
  auto now = std::chrono::system_clock::now().time_since_epoch();
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

  std::ostringstream ss;
  ss << std::hex << std::setfill('0');
  ss << std::setw(8) << (ms & 0xFFFFFFFF) << "-";
  ss << std::setw(4) << ((ms >> 32) & 0xFFFF) << "-";
  ss << std::setw(4) << (0x4000 | (counter & 0x0FFF)) << "-";
  ss << std::setw(4) << (0x8000 | ((counter >> 12) & 0x3FFF)) << "-";
  ss << std::setw(12) << (ms ^ (counter * 0x123456789));
  counter++;
  return ss.str();
}

// Read certificate and extract subject name
std::string read_cert_subject(const std::filesystem::path& cert_path)
{
  if (!std::filesystem::exists(cert_path))
    return "";

  std::ifstream in(cert_path);
  std::string content((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());

  // Look for CN= in the certificate (simplified)
  std::regex cn_re(R"(CN\s*=\s*([^\n/,]+))");
  std::smatch m;
  if (std::regex_search(content, m, cn_re))
    return m[1].str();

  return cert_path.stem().string();
}

// Generate KDM XML document (SMPTE 430-1)
std::string generate_kdm_xml(const KdmOptions& opts, const KdmRecipient& recipient,
                             const std::string& cpl_id)
{
  std::ostringstream xml;
  std::string kdm_id = generate_uuid();

  xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";

  if (opts.interop)
  {
    xml << "<DCinemaSecurityMessage xmlns=\"http://www.digicine.com/PROTO-ASDCP-KDM-20040311#\">\n";
  }
  else
  {
    xml << "<DCinemaSecurityMessage xmlns=\"http://www.smpte-ra.org/schemas/430-3/2006/ETM\">\n";
  }

  xml << "  <AuthenticatedPublic Id=\"ID_AuthenticatedPublic\">\n";
  xml << "    <MessageId>urn:uuid:" << kdm_id << "</MessageId>\n";
  xml << "    <MessageType>http://www.smpte-ra.org/430-1/2006/KDM#kdm-key-type</MessageType>\n";
  xml << "    <AnnotationText>";
  xml << (opts.content_title.empty() ? "KDM" : opts.content_title);
  xml << "</AnnotationText>\n";
  xml << "    <IssueDate>" << opts.valid_from << "</IssueDate>\n";

  xml << "    <Signer>\n";
  xml << "      <X509IssuerSerial>\n";
  xml << "        <X509IssuerName>" << read_cert_subject(opts.signer_cert) << "</X509IssuerName>\n";
  xml << "        <X509SerialNumber>1</X509SerialNumber>\n";
  xml << "      </X509IssuerSerial>\n";
  xml << "    </Signer>\n";

  xml << "    <RequiredExtensions>\n";
  xml << "      <KDMRequiredExtensions xmlns=\"http://www.smpte-ra.org/schemas/430-1/2006/KDM\">\n";
  xml << "        <Recipient>\n";
  xml << "          <X509IssuerSerial>\n";
  xml << "            <X509IssuerName>" << recipient.name << "</X509IssuerName>\n";
  xml << "            <X509SerialNumber>1</X509SerialNumber>\n";
  xml << "          </X509IssuerSerial>\n";
  xml << "          <X509SubjectName>" << recipient.name << "</X509SubjectName>\n";
  xml << "        </Recipient>\n";
  xml << "        <CompositionPlaylistId>urn:uuid:" << cpl_id << "</CompositionPlaylistId>\n";
  xml << "        <ContentTitleText>" << (opts.content_title.empty() ? "Content" : opts.content_title) << "</ContentTitleText>\n";
  xml << "        <ContentKeysNotValidBefore>" << opts.valid_from << "</ContentKeysNotValidBefore>\n";
  xml << "        <ContentKeysNotValidAfter>" << opts.valid_to << "</ContentKeysNotValidAfter>\n";

  if (opts.forensic_mark)
  {
    xml << "        <ForensicMarkFlagList>\n";
    xml << "          <ForensicMarkFlag>http://www.smpte-ra.org/430-1/2006/KDM#mrkflg-picture-disable</ForensicMarkFlag>\n";
    xml << "          <ForensicMarkFlag>http://www.smpte-ra.org/430-1/2006/KDM#mrkflg-audio-disable</ForensicMarkFlag>\n";
    xml << "        </ForensicMarkFlagList>\n";
  }

  xml << "      </KDMRequiredExtensions>\n";
  xml << "    </RequiredExtensions>\n";
  xml << "  </AuthenticatedPublic>\n";

  xml << "  <AuthenticatedPrivate>\n";
  xml << "    <EncryptedKey xmlns=\"http://www.w3.org/2001/04/xmlenc#\">\n";
  xml << "      <EncryptionMethod Algorithm=\"http://www.w3.org/2001/04/xmlenc#rsa-oaep-mgf1p\"/>\n";
  xml << "      <CipherData>\n";
  xml << "        <CipherValue><!-- Encrypted content key would go here --></CipherValue>\n";
  xml << "      </CipherData>\n";
  xml << "    </EncryptedKey>\n";
  xml << "  </AuthenticatedPrivate>\n";

  xml << "  <Signature xmlns=\"http://www.w3.org/2000/09/xmldsig#\">\n";
  xml << "    <!-- XML-DSIG signature would go here -->\n";
  xml << "  </Signature>\n";

  xml << "</DCinemaSecurityMessage>\n";

  return xml.str();
}

} // anonymous namespace

KdmResult generate_kdm(const KdmOptions& opts)
{
  KdmResult result;

  if (!std::filesystem::exists(opts.dcp_dir))
  {
    result.error = "DCP directory not found: " + opts.dcp_dir.string();
    return result;
  }

  if (opts.recipients.empty())
  {
    result.error = "No recipients specified";
    return result;
  }

  if (opts.valid_from.empty() || opts.valid_to.empty())
  {
    result.error = "Validity window (valid_from/valid_to) required";
    return result;
  }

  // Find CPL ID
  std::string cpl_id;
  std::filesystem::path cpl_file = opts.cpl_file;

  if (cpl_file.empty())
  {
    // Search DCP directory for CPL
    for (auto& entry : std::filesystem::directory_iterator(opts.dcp_dir))
    {
      auto name = entry.path().filename().string();
      if (name.find("CPL") != std::string::npos && entry.path().extension() == ".xml")
      {
        cpl_file = entry.path();
        break;
      }
    }
  }

  if (cpl_file.empty() || !std::filesystem::exists(cpl_file))
  {
    result.error = "No CPL found in DCP directory";
    return result;
  }

  // Extract CPL ID
  std::ifstream cpl_in(cpl_file);
  std::string cpl_content((std::istreambuf_iterator<char>(cpl_in)),
                          std::istreambuf_iterator<char>());
  std::regex id_re(R"(<Id>urn:uuid:([^<]+)</Id>)");
  std::smatch m;
  if (std::regex_search(cpl_content, m, id_re))
    cpl_id = m[1].str();
  else
  {
    result.error = "Could not extract CPL ID";
    return result;
  }

  // Create output directory
  std::filesystem::create_directories(opts.output_dir);

  // Generate a KDM for each recipient
  for (auto& recipient : opts.recipients)
  {
    std::string kdm_xml = generate_kdm_xml(opts, recipient, cpl_id);

    // Write KDM file
    std::string safe_name = recipient.name;
    std::replace(safe_name.begin(), safe_name.end(), ' ', '_');
    std::replace(safe_name.begin(), safe_name.end(), '/', '_');
    auto kdm_path = opts.output_dir / ("KDM_" + safe_name + ".xml");

    std::ofstream out(kdm_path);
    out << kdm_xml;
    out.close();

    result.output_files.push_back(kdm_path);
    result.kdms_generated++;
  }

  result.success = true;
  return result;
}

std::vector<std::string> list_content_keys(const std::filesystem::path& dcp_dir)
{
  std::vector<std::string> keys;

  // Look for encrypted MXF files in the DCP
  for (auto& entry : std::filesystem::directory_iterator(dcp_dir))
  {
    if (entry.path().extension() == ".mxf")
    {
      // In a real implementation, we'd parse MXF headers for key IDs
      keys.push_back(entry.path().filename().string());
    }
  }

  return keys;
}

} // namespace imfwizard
