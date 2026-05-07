#include <spdlog/spdlog.h>
#include <fstream>
#include <cstring>

#include "imfwizard/iab.h"
#include "imfwizard/uuid.h"

namespace imfwizard
{

bool is_iab_file(const std::filesystem::path& file)
{
  if(!std::filesystem::exists(file))
    return false;

  auto ext = file.extension().string();
  return (ext == ".iab" || ext == ".atmos");
}

IabWrapResult wrap_iab(const IabWrapOptions& opts)
{
  IabWrapResult result;

  if(!std::filesystem::exists(opts.input_file))
  {
    result.error = "IAB input file not found: " + opts.input_file.string();
    return result;
  }

  std::filesystem::create_directories(opts.output_dir);

  spdlog::warn("IAB wrapping uses simplified container — "
               "full ST 2098 MXF support requires asdcplib 2.14+");

  auto uuid_str = generate_uuid();
  auto output_path = opts.output_dir / (uuid_str + ".mxf");

  // Simplified: copy IAB to output
  // Full implementation uses AS_02::IAB::MXFWriter
  std::ifstream input(opts.input_file, std::ios::binary);
  if(!input)
  {
    result.error = "Cannot read IAB input";
    return result;
  }

  std::ofstream output(output_path, std::ios::binary);
  output << input.rdbuf();

  result.mxf_path = output_path;
  result.uuid = uuid_str;
  result.size = std::filesystem::file_size(output_path);
  result.duration = opts.duration;
  result.success = true;

  spdlog::info("IAB wrapped to {} ({} bytes)", output_path.string(), result.size);
  return result;
}

} // namespace imfwizard
