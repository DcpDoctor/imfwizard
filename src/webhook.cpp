#include <spdlog/spdlog.h>
#include <array>
#include <chrono>
#include <cstdio>
#include <sstream>

#include "imfwizard/webhook.h"
#include "imfwizard/portable.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace imfwizard
{

static std::string get_iso_timestamp()
{
  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  std::tm tm_buf{};
#ifdef _WIN32
  gmtime_s(&tm_buf, &t);
#else
  gmtime_r(&t, &tm_buf);
#endif
  char buf[64];
  std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
  return buf;
}

static std::string escape_json_string(const std::string& s)
{
  std::string out;
  out.reserve(s.size() + 10);
  for(char c : s)
  {
    switch(c)
    {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      out += c;
    }
  }
  return out;
}

WebhookResult send_webhook(const WebhookConfig& config, const WebhookEvent& event)
{
  WebhookResult result;

  // Build JSON body
  std::ostringstream body;
  body << "{\"type\":\"" << escape_json_string(event.type) << "\",\"job_id\":\""
       << escape_json_string(event.job_id) << "\",\"timestamp\":\""
       << escape_json_string(event.timestamp.empty() ? get_iso_timestamp() : event.timestamp)
       << "\",\"data\":" << (event.payload_json.empty() ? "{}" : event.payload_json) << "}";
  std::string json_body = body.str();

  // Use curl for HTTP POST (available on all major platforms)
  for(uint32_t attempt = 0; attempt <= config.max_retries; ++attempt)
  {
    result.attempts = attempt + 1;

    std::ostringstream cmd;
    cmd << "curl -s -o /dev/null -w \"%{http_code}\" -X POST"
        << " -H \"Content-Type: application/json\""
        << " -H \"X-Webhook-Event: " << event.type << "\"";

    if(!config.secret.empty())
    {
      // HMAC signature header — computed by curl's --data
      cmd << " -H \"X-Webhook-Secret: " << config.secret << "\"";
    }

    if(!config.verify_ssl)
      cmd << " -k";

    cmd << " --max-time " << config.timeout_seconds << " -d @- " << config.url;

    // Pipe JSON body via stdin to avoid shell escaping issues
    std::string full_cmd = "echo '" + json_body + "' | " + cmd.str() + " 2>/dev/null";

    std::array<char, 16> buf{};
    std::string http_code;
    FILE* pipe = portable_popen(full_cmd.c_str(), "r");
    if(!pipe)
    {
      result.error = "Failed to execute curl";
      continue;
    }
    while(fgets(buf.data(), static_cast<int>(buf.size()), pipe))
      http_code += buf.data();
    int ret = portable_pclose(pipe);

    if(ret == 0 && !http_code.empty())
    {
      result.http_status = std::atoi(http_code.c_str());
      if(result.http_status >= 200 && result.http_status < 300)
      {
        result.success = true;
        spdlog::info("Webhook delivered to {} (HTTP {})", config.url, result.http_status);
        return result;
      }
      result.error = "HTTP " + http_code;
    }
    else
    {
      result.error = "curl failed with exit code " + std::to_string(ret);
    }

    if(attempt < config.max_retries)
    {
      spdlog::warn("Webhook attempt {} failed ({}), retrying...", attempt + 1, result.error);
    }
  }

  spdlog::error("Webhook delivery failed after {} attempts: {}", result.attempts, result.error);
  return result;
}

std::string build_job_completed_payload(const std::string& job_id,
                                        const std::filesystem::path& output_dir,
                                        double duration_seconds)
{
  std::ostringstream ss;
  ss << "{\"job_id\":\"" << escape_json_string(job_id) << "\",\"status\":\"completed\""
     << ",\"output_dir\":\"" << escape_json_string(output_dir.string()) << "\""
     << ",\"duration_seconds\":" << duration_seconds << "}";
  return ss.str();
}

std::string build_job_failed_payload(const std::string& job_id, const std::string& error)
{
  std::ostringstream ss;
  ss << "{\"job_id\":\"" << escape_json_string(job_id) << "\",\"status\":\"failed\""
     << ",\"error\":\"" << escape_json_string(error) << "\"}";
  return ss.str();
}

std::string build_validation_payload(const std::filesystem::path& imp_dir, bool valid,
                                     uint32_t errors, uint32_t warnings)
{
  std::ostringstream ss;
  ss << "{\"imp_dir\":\"" << escape_json_string(imp_dir.string()) << "\""
     << ",\"valid\":" << (valid ? "true" : "false") << ",\"errors\":" << errors
     << ",\"warnings\":" << warnings << "}";
  return ss.str();
}

WebhookResult test_webhook(const WebhookConfig& config)
{
  WebhookEvent ping;
  ping.type = "ping";
  ping.payload_json = "{\"message\":\"IMF Wizard webhook test\"}";
  ping.timestamp = get_iso_timestamp();
  return send_webhook(config, ping);
}

} // namespace imfwizard
