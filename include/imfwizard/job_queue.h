#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace imfwizard
{

enum class JobType
{
  Transcode,
  Encode,
  Create,
  Validate,
  Loudness,
  QC
};

enum class JobState
{
  Queued,
  Running,
  Completed,
  Failed,
  Cancelled
};

struct Job
{
  uint64_t id = 0;
  JobType type = JobType::Transcode;
  JobState state = JobState::Queued;
  std::string description;
  std::vector<std::string> args;
  float progress = 0.0f;
  int priority = 0; // Higher = runs first
  std::string error;
  std::optional<uint64_t> depends_on;
  std::chrono::system_clock::time_point created_at;
  std::chrono::system_clock::time_point started_at;
  std::chrono::system_clock::time_point finished_at;
};

std::string job_type_to_string(JobType type);
JobType job_type_from_string(const std::string& s);
std::string job_state_to_string(JobState state);
JobState job_state_from_string(const std::string& s);

// Path to the persistent job store
std::filesystem::path jobs_file_path();

// Path to the daemon socket
std::filesystem::path socket_path();

// Client API — communicates with the daemon via socket
struct JobQueueClient
{
  // Returns the job ID on success
  std::optional<uint64_t> submit(JobType type, const std::string& description,
                                 const std::vector<std::string>& args,
                                 std::optional<uint64_t> depends_on = std::nullopt,
                                 int priority = 0);
  std::vector<Job> list();
  bool cancel(uint64_t job_id);
  bool set_priority(uint64_t job_id, int priority);
  bool pause();
  bool resume();
  bool is_paused();
  std::optional<Job> status(uint64_t job_id);
  bool is_daemon_running();
};

// Daemon — runs jobs sequentially, persists state
class JobQueueDaemon
{
public:
  JobQueueDaemon();
  ~JobQueueDaemon();

  // Run the daemon (blocking — listen on socket, process jobs)
  int run();

  // Stop gracefully
  void stop();

private:
  void load_jobs();
  void save_jobs();
  void process_next();
  void execute_job(Job& job);
  void handle_client(int client_fd);

  std::vector<Job> jobs_;
  uint64_t next_id_ = 1;
  bool running_ = false;
  bool paused_ = false;
  int socket_fd_ = -1;
};

} // namespace imfwizard
