#include "imfwizard/plugin.h"
#include "imfwizard/portable.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace imfwizard
{

std::string hook_name(PluginHook hook)
{
  switch(hook)
  {
  case PluginHook::PreEncode: return "pre_encode";
  case PluginHook::PostEncode: return "post_encode";
  case PluginHook::PreWrap: return "pre_wrap";
  case PluginHook::PostWrap: return "post_wrap";
  case PluginHook::PreValidate: return "pre_validate";
  case PluginHook::PostValidate: return "post_validate";
  case PluginHook::PreCreate: return "pre_create";
  case PluginHook::PostCreate: return "post_create";
  }
  return "unknown";
}

std::vector<PluginInfo> discover_plugins(const fs::path& plugins_dir)
{
  std::vector<PluginInfo> plugins;

  if(!fs::exists(plugins_dir))
    return plugins;

  for(auto& entry : fs::directory_iterator(plugins_dir))
  {
    if(!entry.is_directory())
      continue;

    auto manifest = entry.path() / "plugin.json";
    if(!fs::exists(manifest))
      continue;

    std::ifstream f(manifest);
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    PluginInfo info;
    info.path = entry.path();

    // Simple JSON parsing for name, version, description, author
    auto extract = [&](const std::string& key) -> std::string {
      auto pos = content.find("\"" + key + "\"");
      if(pos == std::string::npos)
        return "";
      auto colon = content.find(':', pos);
      auto quote1 = content.find('"', colon);
      auto quote2 = content.find('"', quote1 + 1);
      if(quote1 == std::string::npos || quote2 == std::string::npos)
        return "";
      return content.substr(quote1 + 1, quote2 - quote1 - 1);
    };

    info.name = extract("name");
    info.version = extract("version");
    info.description = extract("description");
    info.author = extract("author");

    if(!info.name.empty())
      plugins.push_back(info);
  }

  return plugins;
}

bool execute_hook(PluginHook hook, const fs::path& plugins_dir, const std::string& context_json)
{
  if(!fs::exists(plugins_dir))
    return true; // no plugins = success

  auto hn = hook_name(hook);
  bool all_ok = true;

  for(auto& entry : fs::directory_iterator(plugins_dir))
  {
    if(!entry.is_directory())
      continue;

    // Look for hook script: hooks/<hook_name>.py
    auto script = entry.path() / "hooks" / (hn + ".py");
    if(!fs::exists(script))
      continue;

    spdlog::info("Plugin: executing {}/{}", entry.path().filename().string(), hn);

    // Write context to temp file
    auto ctx_file = fs::temp_directory_path() / "imfwizard_plugin_ctx.json";
    {
      std::ofstream f(ctx_file);
      f << context_json;
    }

    std::string cmd = "python3 \"" + script.string() + "\" \"" + ctx_file.string() + "\"";
    int ret = system(cmd.c_str());

    fs::remove(ctx_file);

    if(ret != 0)
    {
      spdlog::error("Plugin {} hook {} failed with exit code {}", entry.path().filename().string(),
                    hn, ret);
      all_ok = false;
    }
  }

  return all_ok;
}

} // namespace imfwizard
