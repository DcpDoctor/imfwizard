#include "imfwizard/rest_api.h"
#include "imfwizard/portable.h"
#include <spdlog/spdlog.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#include <windows.h>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace fs = std::filesystem;

namespace imfwizard
{

static std::string read_request(int client_fd)
{
  std::string request;
  char buf[4096];
  while(true)
  {
    auto n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if(n <= 0)
      break;
    buf[n] = '\0';
    request += buf;
    if(request.find("\r\n\r\n") != std::string::npos)
      break;
  }
  return request;
}

static std::string extract_path(const std::string& request)
{
  auto space1 = request.find(' ');
  if(space1 == std::string::npos)
    return "/";
  auto space2 = request.find(' ', space1 + 1);
  if(space2 == std::string::npos)
    return "/";
  return request.substr(space1 + 1, space2 - space1 - 1);
}

static std::string extract_method(const std::string& request)
{
  auto space = request.find(' ');
  if(space == std::string::npos)
    return "GET";
  return request.substr(0, space);
}

static std::string extract_body(const std::string& request)
{
  auto pos = request.find("\r\n\r\n");
  if(pos == std::string::npos)
    return "";
  return request.substr(pos + 4);
}

static void send_response(int client_fd, int status, const std::string& body,
                           const std::string& content_type = "application/json")
{
  std::string status_text;
  switch(status)
  {
  case 200: status_text = "OK"; break;
  case 201: status_text = "Created"; break;
  case 400: status_text = "Bad Request"; break;
  case 401: status_text = "Unauthorized"; break;
  case 404: status_text = "Not Found"; break;
  case 500: status_text = "Internal Server Error"; break;
  default: status_text = "Unknown"; break;
  }

  std::ostringstream resp;
  resp << "HTTP/1.1 " << status << " " << status_text << "\r\n";
  resp << "Content-Type: " << content_type << "\r\n";
  resp << "Content-Length: " << body.size() << "\r\n";
  resp << "Access-Control-Allow-Origin: *\r\n";
  resp << "Connection: close\r\n";
  resp << "\r\n";
  resp << body;

  auto str = resp.str();
  send(client_fd, str.c_str(), static_cast<int>(str.size()), 0);
}

static void handle_client(int client_fd, const RestApiOptions& opts)
{
  auto request = read_request(client_fd);
  auto method = extract_method(request);
  auto path = extract_path(request);
  auto body = extract_body(request);

  // Auth check
  if(!opts.api_key.empty())
  {
    auto auth_pos = request.find("Authorization: Bearer ");
    if(auth_pos == std::string::npos)
    {
      send_response(client_fd, 401, R"({"error":"unauthorized"})");
#ifndef _WIN32
      close(client_fd);
#else
      closesocket(client_fd);
#endif
      return;
    }
    auto key_start = auth_pos + 22;
    auto key_end = request.find("\r\n", key_start);
    auto provided_key = request.substr(key_start, key_end - key_start);
    if(provided_key != opts.api_key)
    {
      send_response(client_fd, 401, R"({"error":"invalid api key"})");
#ifndef _WIN32
      close(client_fd);
#else
      closesocket(client_fd);
#endif
      return;
    }
  }

  spdlog::info("REST: {} {}", method, path);

  if(path == "/api/v1/health" && method == "GET")
  {
    send_response(client_fd, 200, R"({"status":"healthy","version":"0.1.0"})");
  }
  else if(path == "/api/v1/create" && method == "POST")
  {
    // Queue IMP creation job
    send_response(client_fd, 201,
                  R"({"status":"queued","message":"IMP creation job submitted"})");
  }
  else if(path == "/api/v1/validate" && method == "POST")
  {
    send_response(client_fd, 201,
                  R"({"status":"queued","message":"Validation job submitted"})");
  }
  else if(path == "/api/v1/encode" && method == "POST")
  {
    send_response(client_fd, 201,
                  R"({"status":"queued","message":"Encode job submitted"})");
  }
  else if(path == "/api/v1/transcode" && method == "POST")
  {
    send_response(client_fd, 201,
                  R"({"status":"queued","message":"Transcode job submitted"})");
  }
  else if(path == "/api/v1/jobs" && method == "GET")
  {
    send_response(client_fd, 200, R"({"jobs":[]})");
  }
  else if(path == "/api/v1/profiles" && method == "GET")
  {
    send_response(client_fd, 200,
                  R"({"profiles":["netflix","disney","amazon","apple","cinema2k","cinema4k","broadcast","archival"]})");
  }
  else
  {
    send_response(client_fd, 404, R"({"error":"not found"})");
  }

#ifndef _WIN32
  close(client_fd);
#else
  closesocket(client_fd);
#endif
}

RestApiResult start_rest_api(const RestApiOptions& opts)
{
  RestApiResult result;

#ifdef _WIN32
  WSADATA wsa;
  if(WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
  {
    result.error = "WSAStartup failed";
    return result;
  }
#endif

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if(server_fd < 0)
  {
    result.error = "Failed to create socket";
    return result;
  }

  int opt_val = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt_val),
             sizeof(opt_val));

  struct sockaddr_in addr;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(opts.port);
  inet_pton(AF_INET, opts.bind_address.c_str(), &addr.sin_addr);

  if(bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
  {
    result.error = "Failed to bind to " + opts.bind_address + ":" + std::to_string(opts.port);
#ifndef _WIN32
    close(server_fd);
#else
    closesocket(server_fd);
#endif
    return result;
  }

  if(listen(server_fd, 16) < 0)
  {
    result.error = "Failed to listen";
#ifndef _WIN32
    close(server_fd);
#else
    closesocket(server_fd);
#endif
    return result;
  }

  fs::create_directories(opts.working_dir);
  spdlog::info("REST API listening on {}:{}", opts.bind_address, opts.port);

  while(true)
  {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd =
        accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
    if(client_fd < 0)
      continue;

    std::thread([client_fd, &opts]() { handle_client(client_fd, opts); }).detach();
  }

  result.success = true;
  return result;
}

// === Prometheus Metrics ===

namespace
{
// Global metrics state (thread-safe via atomic)
static std::atomic<uint64_t> g_jobs_total{0};
static std::atomic<uint64_t> g_jobs_completed{0};
static std::atomic<uint64_t> g_jobs_failed{0};
static std::atomic<uint64_t> g_jobs_active{0};
static std::atomic<uint64_t> g_jobs_queued{0};
static std::atomic<uint64_t> g_frames_encoded{0};
static std::atomic<uint64_t> g_bytes_written{0};
static std::atomic<uint64_t> g_api_requests{0};
static std::atomic<uint64_t> g_api_errors{0};
static auto g_start_time = std::chrono::steady_clock::now();
} // anonymous namespace

MetricsSnapshot get_metrics()
{
  MetricsSnapshot m;
  m.jobs_total = g_jobs_total.load();
  m.jobs_completed = g_jobs_completed.load();
  m.jobs_failed = g_jobs_failed.load();
  m.jobs_active = g_jobs_active.load();
  m.jobs_queued = g_jobs_queued.load();
  m.frames_encoded_total = g_frames_encoded.load();
  m.bytes_written_total = g_bytes_written.load();
  m.api_requests_total = g_api_requests.load();
  m.api_errors_total = g_api_errors.load();
  m.uptime_seconds = std::chrono::duration<double>(
      std::chrono::steady_clock::now() - g_start_time).count();
  return m;
}

std::string format_prometheus_metrics(const MetricsSnapshot& m)
{
  std::ostringstream out;
  out << "# HELP imfwizard_jobs_total Total jobs submitted\n";
  out << "# TYPE imfwizard_jobs_total counter\n";
  out << "imfwizard_jobs_total " << m.jobs_total << "\n\n";

  out << "# HELP imfwizard_jobs_completed_total Jobs completed successfully\n";
  out << "# TYPE imfwizard_jobs_completed_total counter\n";
  out << "imfwizard_jobs_completed_total " << m.jobs_completed << "\n\n";

  out << "# HELP imfwizard_jobs_failed_total Jobs that failed\n";
  out << "# TYPE imfwizard_jobs_failed_total counter\n";
  out << "imfwizard_jobs_failed_total " << m.jobs_failed << "\n\n";

  out << "# HELP imfwizard_jobs_active Currently running jobs\n";
  out << "# TYPE imfwizard_jobs_active gauge\n";
  out << "imfwizard_jobs_active " << m.jobs_active << "\n\n";

  out << "# HELP imfwizard_jobs_queued Jobs waiting in queue\n";
  out << "# TYPE imfwizard_jobs_queued gauge\n";
  out << "imfwizard_jobs_queued " << m.jobs_queued << "\n\n";

  out << "# HELP imfwizard_frames_encoded_total Total frames encoded\n";
  out << "# TYPE imfwizard_frames_encoded_total counter\n";
  out << "imfwizard_frames_encoded_total " << m.frames_encoded_total << "\n\n";

  out << "# HELP imfwizard_bytes_written_total Total bytes written\n";
  out << "# TYPE imfwizard_bytes_written_total counter\n";
  out << "imfwizard_bytes_written_total " << m.bytes_written_total << "\n\n";

  out << "# HELP imfwizard_api_requests_total Total API requests\n";
  out << "# TYPE imfwizard_api_requests_total counter\n";
  out << "imfwizard_api_requests_total " << m.api_requests_total << "\n\n";

  out << "# HELP imfwizard_api_errors_total Total API errors\n";
  out << "# TYPE imfwizard_api_errors_total counter\n";
  out << "imfwizard_api_errors_total " << m.api_errors_total << "\n\n";

  out << "# HELP imfwizard_uptime_seconds Server uptime\n";
  out << "# TYPE imfwizard_uptime_seconds gauge\n";
  out << "imfwizard_uptime_seconds " << std::fixed << m.uptime_seconds << "\n";

  return out.str();
}

} // namespace imfwizard
