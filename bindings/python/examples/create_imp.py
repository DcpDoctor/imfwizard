#!/usr/bin/env python3
"""Example: Create an IMF package from Python."""

import imfwizard


def main():
    # Create a basic OV IMP
    opts = imfwizard.ImpOptions()
    opts.title = "My Feature Film"
    opts.issuer = "My Studio"
    opts.video_dir = "/path/to/j2k_frames"
    opts.audio_file = "/path/to/audio.wav"
    opts.output_dir = "/tmp/my_imp"
    opts.edit_rate_num = 24
    opts.edit_rate_den = 1

    result = imfwizard.create_ov_imp(opts)
    if result.success:
        print(f"IMP created: {result.output_dir}")
        print(f"CPL UUID: {result.cpl_uuid}")
        print(f"PKL UUID: {result.pkl_uuid}")
        print(f"Track files: {len(result.track_files)}")
    else:
        print(f"Error: {result.error}")


if __name__ == "__main__":
    main()
