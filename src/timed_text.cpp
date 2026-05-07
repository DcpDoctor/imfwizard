#include "imfwizard/timed_text.h"
#include "imfwizard/uuid.h"
#include "imfwizard/hash.h"
#include <KM_fileio.h>
#include <AS_02.h>
#include <Metadata.h>
#include <spdlog/spdlog.h>
#include <fstream>
#include <stdexcept>

namespace imfwizard
{

TimedTextTrackFile wrap_timed_text(const TimedTextOptions& opts)
{
  using namespace ASDCP;

  const Dictionary* dict = &DefaultSMPTEDict();

  // Read TTML file
  std::ifstream ifs(opts.ttml_file, std::ios::binary | std::ios::ate);
  if(!ifs)
    throw std::runtime_error("Cannot open TTML file: " + opts.ttml_file.string());

  auto file_size = ifs.tellg();
  ifs.seekg(0);
  std::string ttml_content(static_cast<size_t>(file_size), '\0');
  ifs.read(ttml_content.data(), file_size);

  TimedText::TimedTextDescriptor tdesc;
  tdesc.EditRate = Rational(opts.edit_rate_num, opts.edit_rate_den);
  tdesc.EncodingName = "UTF-8";
  tdesc.ResourceList.clear();

  // Add font resources if provided
  for(const auto& font_path : opts.font_files)
  {
    TimedText::TimedTextResourceDescriptor res;
    memset(res.ResourceID, 0, Kumu::UUID_Length);
    res.Type = TimedText::MT_OPENTYPE;
    tdesc.ResourceList.push_back(res);
  }

  AS_02::TimedText::MXFWriter writer;
  WriterInfo info;
  info.CompanyName = "IMF Wizard";
  info.ProductName = "IMF Wizard";
  info.ProductVersion = "0.1.0";
  Kumu::GenRandomUUID(info.AssetUUID);

  Result_t result = writer.OpenWrite(opts.output_path.string(), info, tdesc);
  if(ASDCP_FAILURE(result))
    throw std::runtime_error("Failed to open timed text MXF for writing");

  result = writer.WriteTimedTextResource(ttml_content);
  if(ASDCP_FAILURE(result))
    throw std::runtime_error("Failed to write timed text resource");

  // Write font ancillary resources
  for(const auto& font_path : opts.font_files)
  {
    std::ifstream font_ifs(font_path, std::ios::binary | std::ios::ate);
    if(!font_ifs)
      continue;
    auto font_size = font_ifs.tellg();
    font_ifs.seekg(0);

    ASDCP::TimedText::FrameBuffer font_buf(static_cast<ui32_t>(font_size));
    font_ifs.read(reinterpret_cast<char*>(font_buf.Data()), font_size);
    font_buf.Size(static_cast<ui32_t>(font_size));

    Kumu::UUID res_id;
    byte_t id_buf[Kumu::UUID_Length];
    Kumu::GenRandomUUID(id_buf);
    res_id.Set(id_buf);

    result = writer.WriteAncillaryResource(font_buf, nullptr, nullptr);
    if(ASDCP_FAILURE(result))
      spdlog::warn("Failed to write font resource: {}", font_path.string());
  }

  result = writer.Finalize();
  if(ASDCP_FAILURE(result))
    throw std::runtime_error("Failed to finalize timed text MXF");

  spdlog::info("Wrapped TTML -> {}", opts.output_path.string());

  TimedTextTrackFile track;
  track.path = opts.output_path;
  Kumu::UUID asset_uuid(info.AssetUUID);
  char uuid_buf[64];
  asset_uuid.EncodeString(uuid_buf, sizeof(uuid_buf));
  track.uuid = uuid_buf;
  track.duration = tdesc.ContainerDuration;
  track.hash = sha1_base64(opts.output_path);
  track.size = std::filesystem::file_size(opts.output_path);

  return track;
}

} // namespace imfwizard
