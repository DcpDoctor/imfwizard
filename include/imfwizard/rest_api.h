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

} // namespace imfwizard
