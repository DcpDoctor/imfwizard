#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace imfwizard
{

struct PluginInfo
{
  std::string name;
  std::string version;
  std::string description;
  std::string author;
  std::filesystem::path path;
};

enum class PluginHook
{
  PreEncode, // before J2K encoding
  PostEncode, // after J2K encoding
  PreWrap, // before MXF wrapping
  PostWrap, // after MXF wrapping
  PreValidate, // before validation
  PostValidate, // after validation
  PreCreate, // before IMP creation
  PostCreate, // after IMP creation
};

struct PluginOptions
{
  std::filesystem::path plugins_dir; // directory to scan for plugins
  bool verbose = false;
};

// Discover and list available plugins
std::vector<PluginInfo> discover_plugins(const std::filesystem::path& plugins_dir);

// Execute a plugin hook (runs matching Python scripts)
bool execute_hook(PluginHook hook, const std::filesystem::path& plugins_dir,
                  const std::string& context_json);

// Get plugin hook name as string
std::string hook_name(PluginHook hook);

} // namespace imfwizard
