#include "imfwizard/watch.h"
#include "imfwizard/imp.h"
#include "imfwizard/portable.h"
#include "imfwizard/validate.h"
#include "dcpdoctor/validate.h"
#include "dcpdoctor/auto_qc.h"
#include "dcpdoctor/hdr_validate.h"
#include "dcpdoctor/checksum_verify.h"
#include <spdlog/spdlog.h>

#include <chrono>
#include <set>
#include <thread>

#ifdef __linux__
#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace imfwizard
{

// Check if a path has finished being written to (file size stable)
static bool is_stable(const fs::path& path, uint32_t delay_ms)
{
  try
  {
    auto size1 = fs::file_size(path);
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    auto size2 = fs::file_size(path);
    return size1 == size2;
  }
  catch(...)
  {
    return false;
  }
}

// Check if directory contents have stabilized
static bool is_dir_stable(const fs::path& dir, uint32_t delay_ms)
{
  try
  {
    size_t count1 = 0;
    uintmax_t size1 = 0;
    for(const auto& e : fs::recursive_directory_iterator(dir))
    {
      if(e.is_regular_file())
      {
        count1++;
        size1 += e.file_size();
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    size_t count2 = 0;
    uintmax_t size2 = 0;
    for(const auto& e : fs::recursive_directory_iterator(dir))
    {
      if(e.is_regular_file())
      {
        count2++;
        size2 += e.file_size();
      }
    }
    return count1 == count2 && size1 == size2;
  }
  catch(...)
  {
    return false;
  }
}

static bool matches_extension(const fs::path& path, const std::vector<std::string>& exts)
{
  if(exts.empty())
    return true;
  auto ext = path.extension().string();
  for(const auto& e : exts)
  {
    if(ext == e)
      return true;
  }
  return false;
}

static bool has_media_content(const fs::path& dir, const std::vector<std::string>& filter)
{
  static const std::vector<std::string> default_exts = {".j2c", ".j2k", ".dpx", ".tiff", ".tif",
                                                        ".exr", ".png", ".wav", ".mxf"};
  const auto& exts = filter.empty() ? default_exts : filter;

  for(const auto& f : fs::directory_iterator(dir))
  {
    if(f.is_regular_file() && matches_extension(f.path(), exts))
      return true;
  }
  return false;
}

static void run_action_pipeline(const WatchConfig& config, const fs::path& content_dir)
{
  for(auto action : config.actions)
  {
    bool ok = false;
    std::string msg;

    switch(action)
    {
    case WatchAction::CreateImp: {
      // Scan for video/audio
      fs::path video_dir, audio_file;
      for(const auto& f : fs::directory_iterator(content_dir))
      {
        auto ext = f.path().extension().string();
        if(ext == ".j2c" || ext == ".j2k" || ext == ".dpx" || ext == ".tiff" || ext == ".tif" ||
           ext == ".exr" || ext == ".png")
          video_dir = content_dir;
        else if(ext == ".wav")
          audio_file = f.path();
      }

      if(video_dir.empty())
      {
        msg = "No video frames found";
        break;
      }

      ImpOptions opts;
      opts.title = content_dir.filename().string();
      opts.issuer = config.issuer;
      opts.video_dir = video_dir;
      opts.audio_file = audio_file;
      opts.output_dir = config.output_dir / content_dir.filename();
      opts.edit_rate_num = config.fps_num;
      opts.edit_rate_den = config.fps_den;

      auto result = create_ov_imp(opts);
      ok = result.success;
      msg = ok ? result.output_dir.string() : result.error;
      break;
    }
    case WatchAction::AutoQc:
    case WatchAction::ChecksumVerify:
    case WatchAction::Validate:
    case WatchAction::HdrValidate:
      {
        if(action == WatchAction::Validate)
        {
          auto result = dcpdoctor::validate_with_photon(content_dir);
          ok = result.valid;
          msg = ok ? "Validation passed" : "Validation failed (" + std::to_string(result.notes.size()) + " notes)";
        }
        else if(action == WatchAction::HdrValidate)
        {
          bool found = false;
          for(const auto& f : fs::directory_iterator(content_dir))
          {
            auto ext = f.path().extension().string();
            if(ext == ".mxf" || ext == ".mp4" || ext == ".mov")
            {
              dcpdoctor::HdrValidateOptions opts;
              opts.video_path = f.path();
              opts.target_spec = "hdr10";
              auto result = dcpdoctor::validate_hdr_metadata(opts);
              ok = result.valid;
              msg = ok ? "HDR validation passed" : "HDR validation failed";
              found = true;
              break;
            }
          }
          if(!found) msg = "No suitable video file found for HDR validation";
        }
        else if(action == WatchAction::AutoQc)
        {
          bool found = false;
          for(const auto& f : fs::directory_iterator(content_dir))
          {
            auto ext = f.path().extension().string();
            if(ext == ".mxf" || ext == ".mp4" || ext == ".mov")
            {
              dcpdoctor::AutoQcOptions opts;
              opts.video_path = f.path();
              auto result = dcpdoctor::run_auto_qc(opts);
              ok = result.success && result.issues.empty();
              msg = ok ? "Auto QC passed" : "Auto QC found " + std::to_string(result.issues.size()) + " issues";
              found = true;
              break;
            }
          }
          if(!found) msg = "No suitable video file found for auto QC";
        }
        else if(action == WatchAction::ChecksumVerify)
        {
          dcpdoctor::ChecksumVerifyOptions opts;
          opts.package_dir = content_dir;
          auto result = dcpdoctor::verify_package_checksums(opts);
          ok = result.all_valid;
          msg = ok ? "Checksums verified" : "Checksum verification failed";
        }
      }
      break;
    case WatchAction::Custom: {
      if(config.custom_command.empty())
      {
        msg = "No custom command configured";
        break;
      }
      // Substitute $FILE and $OUTPUT
      std::string cmd = config.custom_command;
      auto file_str = content_dir.string();
      auto out_str = (config.output_dir / content_dir.filename()).string();
      // Replace all $FILE with the path
      for(auto pos = cmd.find("$FILE"); pos != std::string::npos; pos = cmd.find("$FILE"))
        cmd.replace(pos, 5, file_str);
      for(auto pos = cmd.find("$OUTPUT"); pos != std::string::npos; pos = cmd.find("$OUTPUT"))
        cmd.replace(pos, 7, out_str);

      FILE* pipe = portable_popen(cmd.c_str(), "r");
      if(pipe)
      {
        char buf[256];
        while(fgets(buf, sizeof(buf), pipe))
          ;
        int ret = portable_pclose(pipe);
        ok = (ret == 0);
        msg = ok ? "Command succeeded" : "Command failed with code " + std::to_string(ret);
      }
      else
      {
        msg = "Failed to execute command";
      }
      break;
    }
    }

    spdlog::info("  Action {}: {} — {}",
                 static_cast<int>(action), ok ? "OK" : "FAIL", msg);
    if(config.on_action_complete)
      config.on_action_complete(action, ok, msg);
  }
}

#ifdef __linux__
static void watch_inotify(const WatchConfig& config, std::atomic<bool>& stop)
{
  int fd = inotify_init1(IN_NONBLOCK);
  if(fd < 0)
  {
    spdlog::warn("inotify_init failed, falling back to polling");
    // Fall through to polling (handled by caller)
    return;
  }

  uint32_t mask = IN_CREATE | IN_MOVED_TO | IN_CLOSE_WRITE;
  int wd = inotify_add_watch(fd, config.watch_dir.c_str(), mask);
  if(wd < 0)
  {
    spdlog::warn("inotify_add_watch failed");
    close(fd);
    return;
  }

  std::set<std::string> processed;
  spdlog::info("Watching {} via inotify...", config.watch_dir.string());
  if(config.on_status)
    config.on_status("Watching " + config.watch_dir.string() + " (inotify)");

  constexpr size_t buf_len = 4096;
  alignas(struct inotify_event) char buf[buf_len];

  while(!stop.load())
  {
    struct pollfd pfd = {fd, POLLIN, 0};
    int ret = poll(&pfd, 1, 500); // 500ms timeout for checking stop flag
    if(ret <= 0)
      continue;

    ssize_t len = read(fd, buf, buf_len);
    if(len <= 0)
      continue;

    for(char* ptr = buf; ptr < buf + len;)
    {
      auto* event = reinterpret_cast<struct inotify_event*>(ptr);
      ptr += sizeof(struct inotify_event) + event->len;

      if(event->len == 0)
        continue;

      std::string name(event->name);
      fs::path full = config.watch_dir / name;

      if(processed.count(name))
        continue;

      // Only process directories (content folders)
      if(!fs::is_directory(full))
        continue;

      // Wait for content to stabilize
      if(!is_dir_stable(full, config.stability_delay_ms))
      {
        spdlog::debug("Content still arriving for {}, will retry", name);
        continue;
      }

      if(!has_media_content(full, config.include_extensions))
        continue;

      spdlog::info("New content detected (inotify): {}", name);
      if(config.on_status)
        config.on_status("Processing " + name);

      processed.insert(name);
      run_action_pipeline(config, full);
    }
  }

  inotify_rm_watch(fd, wd);
  close(fd);
  spdlog::info("Watch folder stopped (inotify).");
}
#endif

void watch_folder(const WatchConfig& config, std::atomic<bool>& stop)
{
  fs::create_directories(config.watch_dir);
  fs::create_directories(config.output_dir);

#ifdef __linux__
  if(config.use_inotify)
  {
    watch_inotify(config, stop);
    if(!stop.load())
    {
      // inotify failed, fall through to polling
      spdlog::info("Falling back to polling mode");
    }
    else
    {
      return;
    }
  }
#endif

  // Polling fallback
  std::set<std::string> processed;
  spdlog::info("Watching {} (polling every {}ms)...", config.watch_dir.string(),
               config.poll_interval_ms);
  if(config.on_status)
    config.on_status("Watching " + config.watch_dir.string());

  while(!stop.load())
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(config.poll_interval_ms));

    auto iter_opts = config.recursive ? fs::directory_options::none : fs::directory_options::none;
    (void)iter_opts;

    for(const auto& entry : fs::directory_iterator(config.watch_dir))
    {
      if(!entry.is_directory())
        continue;

      auto dir_name = entry.path().filename().string();
      if(processed.count(dir_name))
        continue;

      // Stability check
      if(!is_dir_stable(entry.path(), config.stability_delay_ms))
        continue;

      if(!has_media_content(entry.path(), config.include_extensions))
        continue;

      spdlog::info("New content detected: {}", dir_name);
      if(config.on_status)
        config.on_status("Processing " + dir_name);

      processed.insert(dir_name);
      run_action_pipeline(config, entry.path());
    }
  }

  spdlog::info("Watch folder stopped.");
}

} // namespace imfwizard
