#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>

namespace imfwizard
{

struct RenderNode
{
  std::string hostname;
  uint16_t port = 9090;
  uint32_t max_threads = 0;   // 0 = auto-detect on node
  bool available = false;
  std::string status;         // "idle", "busy", "offline"
  uint32_t assigned_frames = 0;
};

struct FrameRange
{
  uint32_t start = 0;
  uint32_t end = 0;
  std::string assigned_node;
  std::string status; // "pending", "encoding", "done", "failed"
};

struct MultiNodeOptions
{
  std::filesystem::path input_dir;    // Source frames
  std::filesystem::path output_dir;   // Encoded output
  std::vector<std::string> nodes;     // "host:port" list
  uint32_t chunk_size = 100;          // Frames per chunk
  std::string codec = "j2k";          // "j2k", "prores", "dnxhr"
  uint32_t bitrate = 250;             // Mbps target
  uint8_t bit_depth = 12;
  bool verify = true;                 // Verify all chunks after encode
  uint16_t coordinator_port = 9090;   // Port for coordination server
};

struct MultiNodeResult
{
  bool success = false;
  std::string error;
  uint32_t total_frames = 0;
  uint32_t frames_encoded = 0;
  uint32_t nodes_used = 0;
  double elapsed_seconds = 0.0;
  double speedup_factor = 0.0; // vs single-node estimate
  std::vector<FrameRange> chunks;
};

// Discover available render nodes
std::vector<RenderNode> discover_nodes(const std::vector<std::string>& addresses);

// Distribute encoding across multiple nodes
MultiNodeResult distribute_encode(const MultiNodeOptions& opts);

// Start a render worker on this machine (called on each node)
int run_render_worker(uint16_t port, uint32_t max_threads);

} // namespace imfwizard
