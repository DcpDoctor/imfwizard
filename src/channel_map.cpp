#include "imfwizard/channel_map.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <sstream>

namespace imfwizard {

uint32_t channel_count(ChannelLayout layout)
{
    switch (layout) {
        case ChannelLayout::Mono: return 1;
        case ChannelLayout::Stereo: return 2;
        case ChannelLayout::Surround_51: return 6;
        case ChannelLayout::Surround_71: return 8;
        case ChannelLayout::Atmos: return 12; // 7.1.4 bed
    }
    return 2;
}

const char* layout_to_ffmpeg(ChannelLayout layout)
{
    switch (layout) {
        case ChannelLayout::Mono: return "mono";
        case ChannelLayout::Stereo: return "stereo";
        case ChannelLayout::Surround_51: return "5.1";
        case ChannelLayout::Surround_71: return "7.1";
        case ChannelLayout::Atmos: return "7.1.4";
    }
    return "stereo";
}

ChannelMapResult remap_channels(const ChannelMapOptions& opts)
{
    ChannelMapResult result;

    if (!std::filesystem::exists(opts.input_file)) {
        result.error = "Input file not found";
        return result;
    }

    result.input_channels = channel_count(opts.source_layout);
    result.output_channels = channel_count(opts.target_layout);

    std::ostringstream cmd;
    cmd << "ffmpeg -y -i " << opts.input_file.string();

    // Build downmix filter
    if (opts.target_layout == ChannelLayout::Stereo &&
        (opts.source_layout == ChannelLayout::Surround_51 ||
         opts.source_layout == ChannelLayout::Surround_71)) {
        // Standard downmix with configurable levels
        cmd << " -af \"pan=stereo|"
            << "FL=FC*" << std::pow(10, opts.center_mix_level / 20) << "+FL+"
            << "BL*" << std::pow(10, opts.surround_mix_level / 20) << "+LFE*"
            << std::pow(10, opts.lfe_mix_level / 20) << "|"
            << "FR=FC*" << std::pow(10, opts.center_mix_level / 20) << "+FR+"
            << "BR*" << std::pow(10, opts.surround_mix_level / 20) << "+LFE*"
            << std::pow(10, opts.lfe_mix_level / 20) << "\"";
    } else {
        // Use ffmpeg's built-in layout conversion
        cmd << " -ac " << result.output_channels;
    }

    cmd << " " << opts.output_file.string() << " 2>/dev/null";

    spdlog::info("Remapping: {} -> {}", layout_to_ffmpeg(opts.source_layout),
                 layout_to_ffmpeg(opts.target_layout));

    int ret = system(cmd.str().c_str());
    if (ret != 0) {
        result.error = "ffmpeg channel remap failed with code " + std::to_string(ret);
        return result;
    }

    result.output_file = opts.output_file;
    result.success = true;
    return result;
}

} // namespace imfwizard
