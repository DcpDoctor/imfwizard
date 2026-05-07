#include "imfwizard/rest_api.h"
#include "imfwizard/portable.h"
#include <spdlog/spdlog.h>

#include <array>
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

} // namespace imfwizard
