#include "imfwizard/preferences.h"
#include "postkit/preferences.h"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <fstream>
#include <sstream>

static constexpr uint32_t CURRENT_PREFS_VERSION = 1;

static std::vector<postkit::PrefsMigration> migrations()
{
  return {
    {1, "Initial versioned schema", [](std::string const& json) {
      auto j = postkit::json_insert_if_missing(json, "default_app_profile", "\"App2E\"");
      j = postkit::json_insert_if_missing(j, "preferred_encoder", "\"grok\"");
      j = postkit::json_insert_if_missing(j, "default_bandwidth_mbps", "250");
      j = postkit::json_insert_if_missing(j, "default_colour_space", "\"Rec.709\"");
      j = postkit::json_insert_if_missing(j, "gpu_device", "-1");
      j = postkit::json_insert_if_missing(j, "default_hdr_mode", "\"SDR\"");
      j = postkit::json_insert_if_missing(j, "default_channel_config", "\"5.1\"");
      j = postkit::json_insert_if_missing(j, "loudness_target_lufs", "-24.0");
      j = postkit::json_insert_if_missing(j, "theme", "\"dark\"");
      j = postkit::json_insert_if_missing(j, "show_advanced_options", "false");
      return j;
    }},
  };
}

namespace imfwizard
{

std::filesystem::path preferences_path()
{
#ifdef _WIN32
  const char* appdata = std::getenv("APPDATA");
  if (appdata)
    return std::filesystem::path(appdata) / "imfwizard" / "preferences.json";
  return "preferences.json";
#elif defined(__APPLE__)
  const char* home = std::getenv("HOME");
  if (home)
    return std::filesystem::path(home) / "Library" / "Application Support" / "imfwizard" / "preferences.json";
  return "preferences.json";
#else
  const char* xdg = std::getenv("XDG_CONFIG_HOME");
  if (xdg)
    return std::filesystem::path(xdg) / "imfwizard" / "preferences.json";
  const char* home = std::getenv("HOME");
  if (home)
    return std::filesystem::path(home) / ".config" / "imfwizard" / "preferences.json";
  return "preferences.json";
#endif
}

static std::string json_string(const std::string& json, const std::string& key)
{
  auto pos = json.find("\"" + key + "\"");
  if (pos == std::string::npos) return "";
  pos = json.find(':', pos);
  if (pos == std::string::npos) return "";
  pos = json.find('"', pos + 1);
  if (pos == std::string::npos) return "";
  auto end = json.find('"', pos + 1);
  if (end == std::string::npos) return "";
  return json.substr(pos + 1, end - pos - 1);
}

static int json_int(const std::string& json, const std::string& key, int def)
{
  auto pos = json.find("\"" + key + "\"");
  if (pos == std::string::npos) return def;
  pos = json.find(':', pos);
  if (pos == std::string::npos) return def;
  pos++;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
  try { return std::stoi(json.substr(pos)); } catch (...) { return def; }
}

static double json_double(const std::string& json, const std::string& key, double def)
{
  auto pos = json.find("\"" + key + "\"");
  if (pos == std::string::npos) return def;
  pos = json.find(':', pos);
  if (pos == std::string::npos) return def;
  pos++;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
  try { return std::stod(json.substr(pos)); } catch (...) { return def; }
}

static bool json_bool(const std::string& json, const std::string& key, bool def)
{
  auto pos = json.find("\"" + key + "\"");
  if (pos == std::string::npos) return def;
  pos = json.find(':', pos);
  if (pos == std::string::npos) return def;
  pos++;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
  if (json.substr(pos, 4) == "true") return true;
  if (json.substr(pos, 5) == "false") return false;
  return def;
}

Preferences load_preferences()
{
  Preferences prefs;
  auto path = preferences_path();

  if (!std::filesystem::exists(path))
  {
    spdlog::debug("No preferences file at {}, using defaults", path.string());
    return prefs;
  }

  std::ifstream f(path);
  if (!f.is_open()) return prefs;

  std::ostringstream ss;
  ss << f.rdbuf();
  std::string json = ss.str();
  f.close();

  // Run schema migrations if needed
  uint32_t file_version = postkit::prefs_version(json);
  if (file_version < CURRENT_PREFS_VERSION)
  {
    spdlog::info("Migrating preferences from version {} to {}", file_version, CURRENT_PREFS_VERSION);
    json = postkit::migrate_preferences(json, migrations());
    std::ofstream out(path);
    if (out.is_open())
    {
      out << json;
      out.close();
    }
  }

  auto s = [&](const std::string& key, std::string& field) {
    auto v = json_string(json, key);
    if (!v.empty()) field = v;
  };

  s("default_app_profile", prefs.default_app_profile);
  s("creator_name", prefs.creator_name);
  s("default_language", prefs.default_language);
  s("preferred_encoder", prefs.preferred_encoder);
  prefs.default_bandwidth_mbps = static_cast<uint32_t>(json_int(json, "default_bandwidth_mbps", 250));
  s("default_colour_space", prefs.default_colour_space);
  prefs.gpu_device = json_int(json, "gpu_device", -1);
  s("default_hdr_mode", prefs.default_hdr_mode);
  s("default_channel_config", prefs.default_channel_config);
  prefs.loudness_target_lufs = json_double(json, "loudness_target_lufs", -24.0);
  s("signing_certificate_path", prefs.signing_certificate_path);
  s("signing_key_path", prefs.signing_key_path);
  s("default_output_dir", prefs.default_output_dir);
  s("naming_template", prefs.naming_template);
  s("theme", prefs.theme);
  prefs.show_advanced_options = json_bool(json, "show_advanced_options", false);

  spdlog::debug("Loaded preferences from {}", path.string());
  return prefs;
}

static std::string escape_json(const std::string& s)
{
  std::string out;
  for (char c : s)
  {
    if (c == '"') out += "\\\"";
    else if (c == '\\') out += "\\\\";
    else out += c;
  }
  return out;
}

int save_preferences(const Preferences& prefs)
{
  auto path = preferences_path();
  std::filesystem::create_directories(path.parent_path());

  std::ofstream f(path);
  if (!f.is_open())
  {
    spdlog::error("Failed to write preferences to {}", path.string());
    return 1;
  }

  f << "{\n";
  f << "  \"version\": " << CURRENT_PREFS_VERSION << ",\n";
  f << "  \"default_app_profile\": \"" << escape_json(prefs.default_app_profile) << "\",\n";
  f << "  \"creator_name\": \"" << escape_json(prefs.creator_name) << "\",\n";
  f << "  \"default_language\": \"" << escape_json(prefs.default_language) << "\",\n";
  f << "  \"preferred_encoder\": \"" << escape_json(prefs.preferred_encoder) << "\",\n";
  f << "  \"default_bandwidth_mbps\": " << prefs.default_bandwidth_mbps << ",\n";
  f << "  \"default_colour_space\": \"" << escape_json(prefs.default_colour_space) << "\",\n";
  f << "  \"gpu_device\": " << prefs.gpu_device << ",\n";
  f << "  \"default_hdr_mode\": \"" << escape_json(prefs.default_hdr_mode) << "\",\n";
  f << "  \"default_channel_config\": \"" << escape_json(prefs.default_channel_config) << "\",\n";
  f << "  \"loudness_target_lufs\": " << prefs.loudness_target_lufs << ",\n";
  f << "  \"signing_certificate_path\": \"" << escape_json(prefs.signing_certificate_path) << "\",\n";
  f << "  \"signing_key_path\": \"" << escape_json(prefs.signing_key_path) << "\",\n";
  f << "  \"default_output_dir\": \"" << escape_json(prefs.default_output_dir) << "\",\n";
  f << "  \"naming_template\": \"" << escape_json(prefs.naming_template) << "\",\n";
  f << "  \"theme\": \"" << escape_json(prefs.theme) << "\",\n";
  f << "  \"show_advanced_options\": " << (prefs.show_advanced_options ? "true" : "false") << "\n";
  f << "}\n";

  spdlog::info("Saved preferences to {}", path.string());
  return 0;
}

} // namespace imfwizard
