#include "imfwizard/job_queue.h"
#include "imfwizard/portable.h"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <thread>

#ifndef _WIN32
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

namespace imfwizard
{

std::string job_type_to_string(JobType type)
{
  switch(type)
  {
  case JobType::Transcode: return "transcode";
  case JobType::Encode:    return "encode";
  case JobType::Create:    return "create";
  case JobType::Validate:  return "validate";
  case JobType::Loudness:  return "loudness";
  case JobType::QC:        return "qc";
  }
  return "unknown";
}

JobType job_type_from_string(const std::string& s)
{
  if(s == "transcode") return JobType::Transcode;
  if(s == "encode")    return JobType::Encode;
  if(s == "create")    return JobType::Create;
  if(s == "validate")  return JobType::Validate;
  if(s == "loudness")  return JobType::Loudness;
  if(s == "qc")        return JobType::QC;
  return JobType::Transcode;
}

std::string job_state_to_string(JobState state)
{
  switch(state)
  {
  case JobState::Queued:    return "queued";
  case JobState::Running:   return "running";
  case JobState::Completed: return "completed";
  case JobState::Failed:    return "failed";
  case JobState::Cancelled: return "cancelled";
  }
  return "unknown";
}

JobState job_state_from_string(const std::string& s)
{
  if(s == "queued")    return JobState::Queued;
  if(s == "running")   return JobState::Running;
  if(s == "completed") return JobState::Completed;
  if(s == "failed")    return JobState::Failed;
  if(s == "cancelled") return JobState::Cancelled;
  return JobState::Queued;
}

std::filesystem::path jobs_file_path()
{
#ifdef _WIN32
  const char* appdata = std::getenv("LOCALAPPDATA");
  if(!appdata) appdata = "C:\\Temp";
  auto dir = std::filesystem::path(appdata) / "imfwizard";
#else
  const char* home = std::getenv("HOME");
  if(!home) home = "/tmp";
  auto dir = std::filesystem::path(home) / ".config" / "imfwizard";
#endif
  std::filesystem::create_directories(dir);
  return dir / "jobs.json";
}

std::filesystem::path socket_path()
{
#ifdef _WIN32
  return std::filesystem::path("\\\\.\\pipe\\imfwizard");
#else
  return std::filesystem::path("/tmp") / "imfwizard.sock";
#endif
}

// Simple JSON serialization (no external dependency)
static std::string escape_json(const std::string& s)
{
  std::string out;
  out.reserve(s.size() + 8);
  for(char c : s)
  {
    switch(c)
    {
    case '"':  out += "\\\""; break;
    case '\\': out += "\\\\"; break;
    case '\n': out += "\\n"; break;
    case '\r': out += "\\r"; break;
    case '\t': out += "\\t"; break;
    default:   out += c;
    }
  }
  return out;
}

static std::string job_to_json(const Job& job)
{
  std::ostringstream ss;
  ss << "{";
  ss << "\"id\":" << job.id;
  ss << ",\"type\":\"" << job_type_to_string(job.type) << "\"";
  ss << ",\"state\":\"" << job_state_to_string(job.state) << "\"";
  ss << ",\"description\":\"" << escape_json(job.description) << "\"";
  ss << ",\"progress\":" << job.progress;
  ss << ",\"priority\":" << job.priority;
  ss << ",\"error\":\"" << escape_json(job.error) << "\"";
  ss << ",\"args\":[";
  for(size_t i = 0; i < job.args.size(); ++i)
  {
    if(i > 0) ss << ",";
    ss << "\"" << escape_json(job.args[i]) << "\"";
  }
  ss << "]";
  if(job.depends_on)
    ss << ",\"depends_on\":" << *job.depends_on;
  auto created_ts = std::chrono::system_clock::to_time_t(job.created_at);
  auto started_ts = std::chrono::system_clock::to_time_t(job.started_at);
  auto finished_ts = std::chrono::system_clock::to_time_t(job.finished_at);
  ss << ",\"created_at\":" << created_ts;
  ss << ",\"started_at\":" << started_ts;
  ss << ",\"finished_at\":" << finished_ts;
  ss << "}";
  return ss.str();
}

static std::string jobs_to_json(const std::vector<Job>& jobs)
{
  std::ostringstream ss;
  ss << "[";
  for(size_t i = 0; i < jobs.size(); ++i)
  {
    if(i > 0) ss << ",";
    ss << job_to_json(jobs[i]);
  }
  ss << "]";
  return ss.str();
}

// Minimal JSON parser for job array (no external dep)
static std::string extract_json_string(const std::string& json, const std::string& key)
{
  auto pos = json.find("\"" + key + "\"");
  if(pos == std::string::npos) return "";
  pos = json.find(':', pos);
  if(pos == std::string::npos) return "";
  pos = json.find('"', pos + 1);
  if(pos == std::string::npos) return "";
  ++pos;
  std::string result;
  while(pos < json.size() && json[pos] != '"')
  {
    if(json[pos] == '\\' && pos + 1 < json.size())
    {
      ++pos;
      switch(json[pos])
      {
      case 'n': result += '\n'; break;
      case 'r': result += '\r'; break;
      case 't': result += '\t'; break;
      default:  result += json[pos]; break;
      }
    }
    else
      result += json[pos];
    ++pos;
  }
  return result;
}

static int64_t extract_json_int(const std::string& json, const std::string& key)
{
  auto pos = json.find("\"" + key + "\"");
  if(pos == std::string::npos) return 0;
  pos = json.find(':', pos);
  if(pos == std::string::npos) return 0;
  ++pos;
  while(pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
  return std::atoll(json.c_str() + pos);
}

static float extract_json_float(const std::string& json, const std::string& key)
{
  auto pos = json.find("\"" + key + "\"");
  if(pos == std::string::npos) return 0.0f;
  pos = json.find(':', pos);
  if(pos == std::string::npos) return 0.0f;
  ++pos;
  while(pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
  return std::strtof(json.c_str() + pos, nullptr);
}

static std::vector<std::string> extract_json_string_array(const std::string& json, const std::string& key)
{
  std::vector<std::string> result;
  auto pos = json.find("\"" + key + "\"");
  if(pos == std::string::npos) return result;
  pos = json.find('[', pos);
  if(pos == std::string::npos) return result;
  ++pos;
  while(pos < json.size() && json[pos] != ']')
  {
    if(json[pos] == '"')
    {
      ++pos;
      std::string val;
      while(pos < json.size() && json[pos] != '"')
      {
        if(json[pos] == '\\' && pos + 1 < json.size())
        {
          ++pos;
          switch(json[pos]) {
          case 'n': val += '\n'; break;
          case 'r': val += '\r'; break;
          case 't': val += '\t'; break;
          default: val += json[pos]; break;
          }
        }
        else
          val += json[pos];
        ++pos;
      }
      if(pos < json.size()) ++pos; // skip closing "
      result.push_back(val);
    }
    else
      ++pos;
  }
  return result;
}

static Job parse_job_json(const std::string& json)
{
  Job job;
  job.id = static_cast<uint64_t>(extract_json_int(json, "id"));
  job.type = job_type_from_string(extract_json_string(json, "type"));
  job.state = job_state_from_string(extract_json_string(json, "state"));
  job.description = extract_json_string(json, "description");
  job.progress = extract_json_float(json, "progress");
  job.priority = static_cast<int>(extract_json_int(json, "priority"));
  job.error = extract_json_string(json, "error");
  job.args = extract_json_string_array(json, "args");
  auto dep = extract_json_int(json, "depends_on");
  if(dep > 0) job.depends_on = static_cast<uint64_t>(dep);
  job.created_at = std::chrono::system_clock::from_time_t(
      static_cast<time_t>(extract_json_int(json, "created_at")));
  job.started_at = std::chrono::system_clock::from_time_t(
      static_cast<time_t>(extract_json_int(json, "started_at")));
  job.finished_at = std::chrono::system_clock::from_time_t(
      static_cast<time_t>(extract_json_int(json, "finished_at")));
  return job;
}

static std::vector<Job> parse_jobs_json(const std::string& json)
{
  std::vector<Job> jobs;
  // Split on objects
  size_t pos = 0;
  while(pos < json.size())
  {
    auto start = json.find('{', pos);
    if(start == std::string::npos) break;
    int depth = 0;
    size_t end = start;
    for(; end < json.size(); ++end)
    {
      if(json[end] == '{') ++depth;
      else if(json[end] == '}') { --depth; if(depth == 0) break; }
    }
    if(depth == 0)
    {
      jobs.push_back(parse_job_json(json.substr(start, end - start + 1)));
      pos = end + 1;
    }
    else
      break;
  }
  return jobs;
}

// === Client implementation ===

#ifndef _WIN32

static std::string send_to_daemon(const std::string& msg)
{
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if(fd < 0) return "";

  struct sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  auto path = socket_path().string();
  strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

  if(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
  {
    close(fd);
    return "";
  }

  // Send message
  auto to_send = msg + "\n";
  if(write(fd, to_send.c_str(), to_send.size()) < 0)
  {
    close(fd);
    return "";
  }

  // Read response
  std::string response;
  char buf[4096];
  while(true)
  {
    auto n = read(fd, buf, sizeof(buf) - 1);
    if(n <= 0) break;
    buf[n] = '\0';
    response += buf;
    if(response.find('\n') != std::string::npos) break;
  }
  close(fd);

  // Trim trailing newline
  while(!response.empty() && response.back() == '\n')
    response.pop_back();
  return response;
}

bool JobQueueClient::is_daemon_running()
{
  return !send_to_daemon("PING").empty();
}

std::optional<uint64_t> JobQueueClient::submit(JobType type, const std::string& description,
                                               const std::vector<std::string>& args,
                                               std::optional<uint64_t> depends_on,
                                               int priority)
{
  std::ostringstream msg;
  msg << "SUBMIT " << job_type_to_string(type) << " ";
  msg << "\"" << escape_json(description) << "\" ";
  if(depends_on)
    msg << "dep=" << *depends_on << " ";
  if(priority != 0)
    msg << "pri=" << priority << " ";
  msg << "--";
  for(auto& a : args)
    msg << " \"" << escape_json(a) << "\"";

  auto resp = send_to_daemon(msg.str());
  if(resp.empty()) return std::nullopt;

  // Response: "OK <id>" or "ERR <msg>"
  if(resp.substr(0, 3) == "OK ")
    return static_cast<uint64_t>(std::stoull(resp.substr(3)));
  return std::nullopt;
}

std::vector<Job> JobQueueClient::list()
{
  auto resp = send_to_daemon("LIST");
  if(resp.empty()) return {};
  return parse_jobs_json(resp);
}

bool JobQueueClient::cancel(uint64_t job_id)
{
  auto resp = send_to_daemon("CANCEL " + std::to_string(job_id));
  return resp == "OK";
}

std::optional<Job> JobQueueClient::status(uint64_t job_id)
{
  auto resp = send_to_daemon("STATUS " + std::to_string(job_id));
  if(resp.empty() || resp == "NOT_FOUND") return std::nullopt;
  auto jobs = parse_jobs_json(resp);
  if(jobs.empty()) return std::nullopt;
  return jobs[0];
}

bool JobQueueClient::set_priority(uint64_t job_id, int priority)
{
  auto resp = send_to_daemon("PRIORITY " + std::to_string(job_id) + " " + std::to_string(priority));
  return resp == "OK";
}

bool JobQueueClient::pause()
{
  return send_to_daemon("PAUSE") == "OK";
}

bool JobQueueClient::resume()
{
  return send_to_daemon("RESUME") == "OK";
}

bool JobQueueClient::is_paused()
{
  return send_to_daemon("IS_PAUSED") == "true";
}

#else
// Windows stub — TODO: implement named pipe version
bool JobQueueClient::is_daemon_running() { return false; }
std::optional<uint64_t> JobQueueClient::submit(JobType, const std::string&,
                                               const std::vector<std::string>&,
                                               std::optional<uint64_t>, int) { return std::nullopt; }
std::vector<Job> JobQueueClient::list() { return {}; }
bool JobQueueClient::cancel(uint64_t) { return false; }
bool JobQueueClient::set_priority(uint64_t, int) { return false; }
bool JobQueueClient::pause() { return false; }
bool JobQueueClient::resume() { return false; }
bool JobQueueClient::is_paused() { return false; }
std::optional<Job> JobQueueClient::status(uint64_t) { return std::nullopt; }
#endif

// === Daemon implementation ===

#ifndef _WIN32

JobQueueDaemon::JobQueueDaemon()
{
  load_jobs();
  // Assign next_id_ based on existing jobs
  for(auto& j : jobs_)
    if(j.id >= next_id_)
      next_id_ = j.id + 1;
}

JobQueueDaemon::~JobQueueDaemon()
{
  stop();
}

void JobQueueDaemon::load_jobs()
{
  auto path = jobs_file_path();
  if(!std::filesystem::exists(path)) return;

  std::ifstream f(path);
  std::string content((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
  jobs_ = parse_jobs_json(content);

  // Reset any "running" jobs to "queued" (daemon restarted)
  for(auto& j : jobs_)
    if(j.state == JobState::Running)
      j.state = JobState::Queued;
}

void JobQueueDaemon::save_jobs()
{
  auto path = jobs_file_path();
  std::ofstream f(path);
  f << jobs_to_json(jobs_);
}

void JobQueueDaemon::stop()
{
  running_ = false;
  if(socket_fd_ >= 0)
  {
    close(socket_fd_);
    socket_fd_ = -1;
  }
  unlink(socket_path().c_str());
}

void JobQueueDaemon::process_next()
{
  if(paused_) return;

  // Find the next queued job whose dependencies are satisfied (highest priority first)
  Job* best = nullptr;
  for(auto& job : jobs_)
  {
    if(job.state != JobState::Queued) continue;

    if(job.depends_on)
    {
      auto it = std::find_if(jobs_.begin(), jobs_.end(),
          [&](const Job& j) { return j.id == *job.depends_on; });
      if(it != jobs_.end() && it->state != JobState::Completed)
        continue; // dependency not done
    }

    if(!best || job.priority > best->priority)
      best = &job;
  }

  if(best)
  {
    execute_job(*best);
    save_jobs();
  }
}

void JobQueueDaemon::execute_job(Job& job)
{
  job.state = JobState::Running;
  job.started_at = std::chrono::system_clock::now();
  save_jobs();

  spdlog::info("Starting job {}: {} ({})", job.id, job.description,
               job_type_to_string(job.type));

  // Build command: use our own executable path
  std::ostringstream cmd;
  auto self = std::filesystem::read_symlink("/proc/self/exe");
  cmd << self.string();
  for(auto& arg : job.args)
    cmd << " \"" << arg << "\"";
  cmd << " 2>&1";

  std::array<char, 4096> buffer;
  FILE* pipe = portable_popen(cmd.str().c_str(), "r");
  if(!pipe)
  {
    job.state = JobState::Failed;
    job.error = "Failed to start process";
    job.finished_at = std::chrono::system_clock::now();
    return;
  }

  std::string output;
  while(fgets(buffer.data(), buffer.size(), pipe))
  {
    std::string line = buffer.data();
    output += line;

    // Parse progress from spdlog output
    if(line.find("Progress:") != std::string::npos)
    {
      auto pct_pos = line.find("Progress: ");
      if(pct_pos != std::string::npos)
      {
        auto val = std::atoi(line.c_str() + pct_pos + 10);
        job.progress = static_cast<float>(val);
        save_jobs(); // persist progress
      }
    }
  }

  int status = portable_pclose(pipe);
  job.finished_at = std::chrono::system_clock::now();

  if(status == 0)
  {
    job.state = JobState::Completed;
    job.progress = 100.0f;
    spdlog::info("Job {} completed", job.id);
  }
  else
  {
    job.state = JobState::Failed;
    job.error = "Exit code " + std::to_string(status);
    spdlog::error("Job {} failed: exit code {}", job.id, status);
  }
}

void JobQueueDaemon::handle_client(int client_fd)
{
  char buf[8192];
  auto n = read(client_fd, buf, sizeof(buf) - 1);
  if(n <= 0) { close(client_fd); return; }
  buf[n] = '\0';

  std::string msg(buf);
  // Trim newline
  while(!msg.empty() && msg.back() == '\n') msg.pop_back();

  std::string response;

  if(msg == "PING")
  {
    response = "PONG";
  }
  else if(msg == "LIST")
  {
    response = jobs_to_json(jobs_);
  }
  else if(msg.substr(0, 7) == "CANCEL ")
  {
    auto id = std::stoull(msg.substr(7));
    auto it = std::find_if(jobs_.begin(), jobs_.end(),
        [id](const Job& j) { return j.id == id; });
    if(it != jobs_.end() && (it->state == JobState::Queued || it->state == JobState::Running))
    {
      it->state = JobState::Cancelled;
      it->finished_at = std::chrono::system_clock::now();
      save_jobs();
      response = "OK";
    }
    else
      response = "NOT_FOUND";
  }
  else if(msg.substr(0, 7) == "STATUS ")
  {
    auto id = std::stoull(msg.substr(7));
    auto it = std::find_if(jobs_.begin(), jobs_.end(),
        [id](const Job& j) { return j.id == id; });
    if(it != jobs_.end())
      response = job_to_json(*it);
    else
      response = "NOT_FOUND";
  }
  else if(msg.substr(0, 7) == "SUBMIT ")
  {
    // Parse: SUBMIT <type> "<description>" [dep=<id>] -- <args...>
    auto rest = msg.substr(7);
    auto space = rest.find(' ');
    auto type_str = rest.substr(0, space);
    rest = rest.substr(space + 1);

    // Extract description (in quotes)
    std::string description;
    if(!rest.empty() && rest[0] == '"')
    {
      size_t end = 1;
      while(end < rest.size() && rest[end] != '"')
      {
        if(rest[end] == '\\') ++end;
        ++end;
      }
      description = rest.substr(1, end - 1);
      rest = (end + 1 < rest.size()) ? rest.substr(end + 2) : "";
    }

    // Check for depends_on
    std::optional<uint64_t> depends_on;
    if(rest.find("dep=") == 0)
    {
      auto dep_end = rest.find(' ');
      auto dep_str = rest.substr(4, dep_end - 4);
      depends_on = std::stoull(dep_str);
      rest = (dep_end != std::string::npos) ? rest.substr(dep_end + 1) : "";
    }

    // Check for priority
    int priority = 0;
    if(rest.find("pri=") == 0)
    {
      auto pri_end = rest.find(' ');
      auto pri_str = rest.substr(4, pri_end - 4);
      priority = std::stoi(pri_str);
      rest = (pri_end != std::string::npos) ? rest.substr(pri_end + 1) : "";
    }

    // Skip "--"
    if(rest.find("--") == 0)
      rest = (rest.size() > 3) ? rest.substr(3) : "";

    // Parse args (quoted strings)
    std::vector<std::string> args;
    size_t pos = 0;
    while(pos < rest.size())
    {
      if(rest[pos] == '"')
      {
        ++pos;
        std::string arg;
        while(pos < rest.size() && rest[pos] != '"')
        {
          if(rest[pos] == '\\' && pos + 1 < rest.size()) { ++pos; }
          arg += rest[pos++];
        }
        if(pos < rest.size()) ++pos;
        args.push_back(arg);
      }
      else
        ++pos;
    }

    Job job;
    job.id = next_id_++;
    job.type = job_type_from_string(type_str);
    job.state = JobState::Queued;
    job.description = description;
    job.args = args;
    job.depends_on = depends_on;
    job.priority = priority;
    job.created_at = std::chrono::system_clock::now();
    jobs_.push_back(job);
    save_jobs();

    response = "OK " + std::to_string(job.id);
    spdlog::info("Job {} queued: {}", job.id, description);
  }
  else if(msg == "PAUSE")
  {
    paused_ = true;
    spdlog::info("Queue paused");
    response = "OK";
  }
  else if(msg == "RESUME")
  {
    paused_ = false;
    spdlog::info("Queue resumed");
    response = "OK";
  }
  else if(msg == "IS_PAUSED")
  {
    response = paused_ ? "true" : "false";
  }
  else if(msg.substr(0, 9) == "PRIORITY ")
  {
    // PRIORITY <id> <priority>
    auto rest = msg.substr(9);
    auto sp = rest.find(' ');
    if(sp != std::string::npos)
    {
      auto id = std::stoull(rest.substr(0, sp));
      auto pri = std::stoi(rest.substr(sp + 1));
      auto it = std::find_if(jobs_.begin(), jobs_.end(),
          [id](const Job& j) { return j.id == id; });
      if(it != jobs_.end() && it->state == JobState::Queued)
      {
        it->priority = pri;
        save_jobs();
        response = "OK";
      }
      else
        response = "NOT_FOUND";
    }
    else
      response = "ERR bad format";
  }
  else
  {
    response = "ERR unknown command";
  }

  response += "\n";
  write(client_fd, response.c_str(), response.size());
  close(client_fd);
}

int JobQueueDaemon::run()
{
  // Remove stale socket
  unlink(socket_path().c_str());

  socket_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
  if(socket_fd_ < 0)
  {
    spdlog::error("Failed to create socket");
    return 1;
  }

  struct sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  auto path = socket_path().string();
  strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

  if(bind(socket_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0)
  {
    spdlog::error("Failed to bind socket: {}", strerror(errno));
    close(socket_fd_);
    return 1;
  }

  if(listen(socket_fd_, 5) < 0)
  {
    spdlog::error("Failed to listen on socket");
    close(socket_fd_);
    return 1;
  }

  spdlog::info("IMF Wizard daemon listening on {}", path);
  running_ = true;

  // Ignore SIGPIPE
  signal(SIGPIPE, SIG_IGN);

  // Worker thread for job execution
  std::thread worker([this]() {
    while(running_)
    {
      bool any_running = std::any_of(jobs_.begin(), jobs_.end(),
          [](const Job& j) { return j.state == JobState::Running; });
      if(!any_running)
        process_next();
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
  });

  while(running_)
  {
    struct pollfd pfd{};
    pfd.fd = socket_fd_;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, 1000); // 1s timeout
    if(ret > 0 && (pfd.revents & POLLIN))
    {
      int client_fd = accept(socket_fd_, nullptr, nullptr);
      if(client_fd >= 0)
        handle_client(client_fd);
    }
  }

  if(worker.joinable())
    worker.join();

  stop();
  return 0;
}

#else
// Windows stub
JobQueueDaemon::JobQueueDaemon() {}
JobQueueDaemon::~JobQueueDaemon() {}
int JobQueueDaemon::run() { return 1; }
void JobQueueDaemon::stop() {}
void JobQueueDaemon::load_jobs() {}
void JobQueueDaemon::save_jobs() {}
void JobQueueDaemon::process_next() {}
void JobQueueDaemon::execute_job(Job&) {}
void JobQueueDaemon::handle_client(int) {}
#endif

} // namespace imfwizard
