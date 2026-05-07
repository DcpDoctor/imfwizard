#include "imfwizard/transcode.h"
#include <spdlog/spdlog.h>
#include <array>
#include <cstdio>
#include <sstream>
#include <algorithm>

namespace imfwizard {

bool ffmpeg_available()
{
#ifdef _WIN32
    return system("where ffmpeg >NUL 2>&1") == 0;
#else
    return system("which ffmpeg >/dev/null 2>&1") == 0;
#endif
}

bool ffprobe_available()
{
#ifdef _WIN32
    return system("where ffprobe >NUL 2>&1") == 0;
#else
    return system("which ffprobe >/dev/null 2>&1") == 0;
#endif
}

static std::string exec_cmd(const std::string& cmd)
{
    std::array<char, 4096> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return {};
    while (fgets(buffer.data(), buffer.size(), pipe))
        result += buffer.data();
    pclose(pipe);
    // Trim trailing whitespace
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();
    return result;
}

VideoCodec detect_codec(const std::filesystem::path& file)
{
    if (!ffprobe_available()) return VideoCodec::Unknown;

    std::string cmd = "ffprobe -v error -select_streams v:0 "
                      "-show_entries stream=codec_name "
                      "-of default=noprint_wrappers=1:nokey=1 " +
                      file.string() + " 2>/dev/null";
    auto codec = exec_cmd(cmd);

    if (codec.find("prores") != std::string::npos) return VideoCodec::ProRes;
    if (codec.find("dnxh") != std::string::npos) return VideoCodec::DNxHR;
    if (codec.find("h264") != std::string::npos || codec.find("avc") != std::string::npos)
        return VideoCodec::H264;
    if (codec.find("hevc") != std::string::npos || codec.find("h265") != std::string::npos)
        return VideoCodec::H265;
    if (codec.find("av1") != std::string::npos) return VideoCodec::AV1;

    return VideoCodec::Unknown;
}

TranscodeResult transcode_to_sequence(const TranscodeOptions& opts)
{
    TranscodeResult result;

    if (!ffmpeg_available()) {
        result.error = "ffmpeg not found in PATH";
        return result;
    }

    std::filesystem::create_directories(opts.output_dir);

    // Get source info via ffprobe
    if (ffprobe_available()) {
        auto w = exec_cmd("ffprobe -v error -select_streams v:0 "
                          "-show_entries stream=width -of default=noprint_wrappers=1:nokey=1 " +
                          opts.input_file.string() + " 2>/dev/null");
        auto h = exec_cmd("ffprobe -v error -select_streams v:0 "
                          "-show_entries stream=height -of default=noprint_wrappers=1:nokey=1 " +
                          opts.input_file.string() + " 2>/dev/null");
        auto fps_str = exec_cmd("ffprobe -v error -select_streams v:0 "
                                "-show_entries stream=r_frame_rate -of default=noprint_wrappers=1:nokey=1 " +
                                opts.input_file.string() + " 2>/dev/null");
        try {
            result.width = static_cast<uint32_t>(std::stoul(w));
            result.height = static_cast<uint32_t>(std::stoul(h));
        } catch (...) {}

        // Parse fps like "24000/1001"
        auto slash = fps_str.find('/');
        if (slash != std::string::npos) {
            try {
                double num = std::stod(fps_str.substr(0, slash));
                double den = std::stod(fps_str.substr(slash + 1));
                if (den > 0) result.fps = num / den;
            } catch (...) {}
        }
    }

    // Build ffmpeg command
    std::string pix_fmt;
    std::string ext = opts.output_format;
    if (opts.output_format == "tiff") {
        pix_fmt = (opts.bit_depth > 8) ? "rgb48le" : "rgb24";
        ext = "tiff";
    } else if (opts.output_format == "dpx") {
        pix_fmt = (opts.bit_depth > 8) ? "rgb48le" : "rgb24";
        ext = "dpx";
    } else if (opts.output_format == "exr") {
        pix_fmt = "gbrpf32le";
        ext = "exr";
    } else if (opts.output_format == "png") {
        pix_fmt = (opts.bit_depth > 8) ? "rgb48be" : "rgb24";
        ext = "png";
    }

    std::ostringstream cmd;
    cmd << "ffmpeg -y -i " << opts.input_file.string();

    if (opts.start_frame > 0 && result.fps > 0)
        cmd << " -ss " << (opts.start_frame / result.fps);
    if (opts.end_frame > 0 && result.fps > 0)
        cmd << " -to " << (opts.end_frame / result.fps);

    if (!pix_fmt.empty())
        cmd << " -pix_fmt " << pix_fmt;

    if (opts.threads > 0)
        cmd << " -threads " << opts.threads;

    auto output_pattern = opts.output_dir / ("frame_%06d." + ext);
    cmd << " " << output_pattern.string() << " 2>&1";

    spdlog::info("Transcoding: {}", cmd.str());

    int ret = system(cmd.str().c_str());
    if (ret != 0) {
        result.error = "ffmpeg exited with code " + std::to_string(ret);
        return result;
    }

    // Count output frames
    uint32_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(opts.output_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ("." + ext))
            ++count;
    }

    result.output_dir = opts.output_dir;
    result.frame_count = count;
    result.success = true;
    spdlog::info("Transcoded {} frames to {}", count, opts.output_dir.string());

    return result;
}

} // namespace imfwizard
