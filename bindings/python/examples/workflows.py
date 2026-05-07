#!/usr/bin/env python3
"""Example: Batch delivery workflow — create IMPs for multiple platforms."""

import imfwizard


def batch_workflow():
    """Create delivery packages for Netflix, Disney+, Amazon, and Apple TV+."""
    opts = imfwizard.BatchDeliverOptions()
    opts.imp_dir = "/path/to/source_imp"
    opts.output_dir = "/tmp/deliveries"
    opts.targets = imfwizard.StringVector()
    opts.targets.append("netflix")
    opts.targets.append("disney")
    opts.targets.append("amazon")
    opts.targets.append("apple")

    result = imfwizard.batch_deliver(opts)
    if result.success:
        print(f"Created {len(result.outputs)} delivery packages:")
        for output in result.outputs:
            print(f"  {output}")
    else:
        print(f"Error: {result.error}")


def analytics_workflow():
    """Analyze J2K bitrate distribution."""
    opts = imfwizard.AnalyticsOptions()
    opts.input_dir = "/path/to/j2k_frames"
    opts.output_json = "/tmp/analytics.json"

    result = imfwizard.analyze_bitrate(opts)
    if result.success:
        print(f"Frames analyzed: {result.frame_count}")
        print(f"Average bitrate: {result.avg_bitrate_mbps:.1f} Mbps")
        print(f"Peak bitrate: {result.peak_bitrate_mbps:.1f} Mbps")
        print(f"JSON exported to: {opts.output_json}")
    else:
        print(f"Error: {result.error}")


def burnin_workflow():
    """Burn subtitles into video frames."""
    opts = imfwizard.BurnInOptions()
    opts.input_file = "/path/to/video.mxf"
    opts.subtitle_file = "/path/to/subtitles.srt"
    opts.output_file = "/tmp/burned_in.mxf"
    opts.font_size = 48

    result = imfwizard.burn_subtitles(opts)
    if result.success:
        print(f"Subtitles burned in: {result.output_file}")
    else:
        print(f"Error: {result.error}")


def dcp_convert_workflow():
    """Convert IMF package to DCP for theatrical."""
    opts = imfwizard.DcpConvertOptions()
    opts.imp_dir = "/path/to/imp"
    opts.output_dir = "/tmp/dcp_output"
    opts.content_title = "My Film"

    result = imfwizard.convert_to_dcp(opts)
    if result.success:
        print(f"DCP created: {result.output_dir}")
        print(f"CPL UUID: {result.cpl_uuid}")
    else:
        print(f"Error: {result.error}")


if __name__ == "__main__":
    batch_workflow()
    analytics_workflow()
    burnin_workflow()
    dcp_convert_workflow()
