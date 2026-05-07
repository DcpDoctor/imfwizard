#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>

#include "imfwizard/hdr10plus.h"

namespace imfwizard
{

bool hdr10plus_tool_available()
{
#ifdef _WIN32
  return system("where hdr10plus_tool >NUL 2>&1") == 0;
#else
  return system("which hdr10plus_tool >/dev/null 2>&1") == 0;
#endif
}

Hdr10PlusMetadata parse_hdr10plus_json(const std::filesystem::path& json_file)
{
  Hdr10PlusMetadata meta;
  meta.json_path = json_file.string();

  if(!std::filesystem::exists(json_file))
  {
    spdlog::error("HDR10+ JSON not found: {}", json_file.string());
    return meta;
  }

  // The actual JSON parsing would use a JSON library
  // For now we just store the path — hdr10plus_tool handles parsing
  spdlog::info("HDR10+ metadata: {}", json_file.string());
  return meta;
}

Hdr10PlusResult inject_hdr10plus(const std::filesystem::path& input_file,
                                 const std::filesystem::path& metadata_json,
                                 const std::filesystem::path& output_file)
{
  Hdr10PlusResult result;

  if(!std::filesystem::exists(input_file))
  {
    result.error = "Input file not found";
    return result;
  }

  if(!std::filesystem::exists(metadata_json))
  {
    result.error = "HDR10+ metadata JSON not found";
    return result;
  }

  if(!hdr10plus_tool_available())
  {
    result.error = "hdr10plus_tool not found in PATH";
    return result;
  }

  std::string cmd = "hdr10plus_tool inject"
                    " -i " +
                    input_file.string() + " -j " + metadata_json.string() + " -o " +
                    output_file.string() + " 2>&1";

  spdlog::info("Injecting HDR10+ metadata: {}", cmd);
  int ret = system(cmd.c_str());

  if(ret != 0)
  {
    result.error = "hdr10plus_tool inject failed with code " + std::to_string(ret);
    return result;
  }

  result.output_file = output_file;
  result.success = true;
  spdlog::info("HDR10+ metadata injected to {}", output_file.string());
  return result;
}

} // namespace imfwizard
