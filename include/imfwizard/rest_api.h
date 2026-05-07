#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace imfwizard
{

struct RestApiOptions
{
  uint16_t port = 8080;
  std::string bind_address = "127.0.0.1";
  std::string api_key; // optional auth token
  uint32_t max_concurrent_jobs = 4;
  std::filesystem::path working_dir = "/tmp/imfwizard-api";
};

struct RestApiResult
{
  bool success = false;
  std::string error;
};

// Start HTTP REST API server (blocking)
RestApiResult start_rest_api(const RestApiOptions& opts);

// === Prometheus Metrics ===
// Exposed at GET /metrics in OpenMetrics/Prometheus exposition format

struct MetricsSnapshot
{
  // Job counters
  uint64_t jobs_total = 0;
  uint64_t jobs_completed = 0;
  uint64_t jobs_failed = 0;
  uint64_t jobs_active = 0;
  uint64_t jobs_queued = 0;

  // Performance
  double avg_encode_seconds = 0.0;
  double avg_wrap_seconds = 0.0;
  uint64_t frames_encoded_total = 0;
  uint64_t bytes_written_total = 0;

  // System
  double uptime_seconds = 0.0;
  uint64_t api_requests_total = 0;
  uint64_t api_errors_total = 0;
};

// Get current metrics snapshot
MetricsSnapshot get_metrics();

// Format metrics in Prometheus exposition format
std::string format_prometheus_metrics(const MetricsSnapshot& m);

} // namespace imfwizard
