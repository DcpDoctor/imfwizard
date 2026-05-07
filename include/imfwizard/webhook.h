#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace imfwizard
{

struct WebhookConfig
{
  std::string url; // POST endpoint
  std::string secret; // HMAC-SHA256 signing secret (optional)
  std::string event_filter; // Comma-separated event types (empty = all)
  uint32_t timeout_seconds = 10;
  uint32_t max_retries = 3;
  bool verify_ssl = true;
};

struct WebhookEvent
{
  std::string type; // job.completed, job.failed, validate.done, encode.progress, etc.
  std::string job_id;
  std::string payload_json; // JSON payload body
  std::string timestamp; // ISO 8601
};

struct WebhookResult
{
  bool success = false;
  int http_status = 0;
  std::string error;
  uint32_t attempts = 0;
};

// Send a webhook notification
WebhookResult send_webhook(const WebhookConfig& config, const WebhookEvent& event);

// Build a JSON payload for common events
std::string build_job_completed_payload(const std::string& job_id,
                                        const std::filesystem::path& output_dir,
                                        double duration_seconds);
std::string build_job_failed_payload(const std::string& job_id, const std::string& error);
std::string build_validation_payload(const std::filesystem::path& imp_dir, bool valid,
                                     uint32_t errors, uint32_t warnings);

// Test a webhook endpoint with a ping event
WebhookResult test_webhook(const WebhookConfig& config);

} // namespace imfwizard
