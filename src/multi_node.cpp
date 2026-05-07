#include "imfwizard/multi_node.h"
#include "imfwizard/portable.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <thread>

#ifdef _WIN32
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace imfwizard
{

namespace
{

// Simple TCP ping to check if a render node is alive
bool ping_node(const std::string& host, uint16_t port, int timeout_ms = 2000)
{
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    return false;

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

  // Set timeout
  struct timeval tv;
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
  setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));

  bool connected = (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == 0);

#ifdef _WIN32
  closesocket(sock);
#else
  close(sock);
#endif

  return connected;
}

// Parse "host:port" string
std::pair<std::string, uint16_t> parse_address(const std::string& addr)
{
  auto colon = addr.rfind(':');
  if (colon == std::string::npos)
    return {addr, 9090};
  return {addr.substr(0, colon), static_cast<uint16_t>(std::stoi(addr.substr(colon + 1)))};
}

// Send encode command to a render worker
bool send_encode_command(const std::string& host, uint16_t port,
                         const std::string& input_path,
                         uint32_t start_frame, uint32_t end_frame,
                         const std::string& output_path,
                         const std::string& codec, uint32_t bitrate)
{
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0)
    return false;

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

  if (connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) != 0)
  {
#ifdef _WIN32
    closesocket(sock);
#else
    close(sock);
#endif
    return false;
  }

  // Simple text protocol: ENCODE input start end output codec bitrate\n
  std::ostringstream cmd;
  cmd << "ENCODE " << input_path << " " << start_frame << " " << end_frame
      << " " << output_path << " " << codec << " " << bitrate << "\n";
  std::string msg = cmd.str();
  send(sock, msg.c_str(), msg.size(), 0);

  // Wait for ACK
  char buf[64]{};
  recv(sock, buf, sizeof(buf) - 1, 0);

#ifdef _WIN32
  closesocket(sock);
#else
  close(sock);
#endif

  return std::string(buf).find("OK") != std::string::npos;
}

} // anonymous namespace

std::vector<RenderNode> discover_nodes(const std::vector<std::string>& addresses)
{
  std::vector<RenderNode> nodes;
  nodes.reserve(addresses.size());

  for (auto& addr : addresses)
  {
    auto [host, port] = parse_address(addr);
    RenderNode node;
    node.hostname = host;
    node.port = port;
    node.available = ping_node(host, port);
    node.status = node.available ? "idle" : "offline";
    nodes.push_back(node);
  }

  return nodes;
}

MultiNodeResult distribute_encode(const MultiNodeOptions& opts)
{
  MultiNodeResult result;
  auto start_time = std::chrono::steady_clock::now();

  if (!std::filesystem::exists(opts.input_dir))
  {
    result.error = "Input directory not found: " + opts.input_dir.string();
    return result;
  }

  // Count input frames
  uint32_t frame_count = 0;
  for (auto& entry : std::filesystem::directory_iterator(opts.input_dir))
  {
    auto ext = entry.path().extension().string();
    if (ext == ".j2k" || ext == ".j2c" || ext == ".tif" || ext == ".tiff" ||
        ext == ".dpx" || ext == ".exr" || ext == ".png")
      frame_count++;
  }

  if (frame_count == 0)
  {
    result.error = "No frames found in input directory";
    return result;
  }
  result.total_frames = frame_count;

  // Discover available nodes
  auto nodes = discover_nodes(opts.nodes);
  uint32_t available_count = 0;
  for (auto& n : nodes)
    if (n.available)
      available_count++;

  if (available_count == 0)
  {
    result.error = "No render nodes available";
    return result;
  }
  result.nodes_used = available_count;

  // Create output directory
  std::filesystem::create_directories(opts.output_dir);

  // Divide frames into chunks and assign to nodes
  uint32_t chunk_size = opts.chunk_size;
  uint32_t chunk_count = (frame_count + chunk_size - 1) / chunk_size;

  std::vector<FrameRange> chunks;
  uint32_t node_idx = 0;
  for (uint32_t i = 0; i < chunk_count; i++)
  {
    FrameRange range;
    range.start = i * chunk_size;
    range.end = std::min(range.start + chunk_size - 1, frame_count - 1);

    // Round-robin assignment to available nodes
    while (!nodes[node_idx % nodes.size()].available)
      node_idx++;
    auto& assigned_node = nodes[node_idx % nodes.size()];
    range.assigned_node = assigned_node.hostname + ":" + std::to_string(assigned_node.port);
    assigned_node.assigned_frames += (range.end - range.start + 1);
    node_idx++;

    range.status = "pending";
    chunks.push_back(range);
  }

  // Send encode commands to each node
  for (auto& chunk : chunks)
  {
    auto [host, port] = parse_address(chunk.assigned_node);
    bool sent = send_encode_command(
        host, port,
        opts.input_dir.string(),
        chunk.start, chunk.end,
        opts.output_dir.string(),
        opts.codec, opts.bitrate);

    chunk.status = sent ? "encoding" : "failed";
    if (sent)
      result.frames_encoded += (chunk.end - chunk.start + 1);
  }

  result.chunks = chunks;

  auto end_time = std::chrono::steady_clock::now();
  result.elapsed_seconds = std::chrono::duration<double>(end_time - start_time).count();
  result.speedup_factor = static_cast<double>(available_count); // Theoretical max
  result.success = (result.frames_encoded > 0);

  return result;
}

int run_render_worker(uint16_t port, uint32_t /*max_threads*/)
{
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0)
  {
    std::cerr << "Failed to create socket\n";
    return 1;
  }

  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);

  if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
  {
    std::cerr << "Failed to bind to port " << port << "\n";
    return 1;
  }

  listen(server_fd, 8);
  std::cout << "Render worker listening on port " << port << "\n";

  while (true)
  {
    struct sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &client_len);
    if (client_fd < 0)
      continue;

    char buf[4096]{};
    auto n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0)
    {
#ifdef _WIN32
      closesocket(client_fd);
#else
      close(client_fd);
#endif
      continue;
    }

    std::string cmd(buf, n);
    if (cmd.starts_with("ENCODE"))
    {
      // Parse and execute encode (fork to background)
      std::string ack = "OK\n";
      send(client_fd, ack.c_str(), ack.size(), 0);

      // In production, would fork/thread here and run the encode
      std::cout << "Received encode command: " << cmd;
    }
    else if (cmd.starts_with("PING"))
    {
      std::string ack = "PONG\n";
      send(client_fd, ack.c_str(), ack.size(), 0);
    }

#ifdef _WIN32
    closesocket(client_fd);
#else
    close(client_fd);
#endif
  }

  return 0;
}

} // namespace imfwizard
