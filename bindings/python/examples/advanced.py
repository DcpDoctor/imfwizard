#!/usr/bin/env python3
"""Example: Validate an IMP and transcode ProRes to IMF."""

import imfwizard


def validate_example():
    """Validate an existing IMP with Netflix Photon."""
    if not imfwizard.photon_available():
        print("Photon not found — install Java and Photon JAR")
        return

    result = imfwizard.validate_with_photon("/path/to/imp")
    print(f"Valid: {result.valid}")
    print(f"Validator: {result.validator_version}")
    for note in result.notes:
        severity = "ERROR" if note.severity == imfwizard.ValidationNote.Severity_error else "WARN"
        print(f"  [{severity}] {note.message}")


def transcode_example():
    """Transcode ProRes MOV to TIFF sequence."""
    opts = imfwizard.TranscodeOptions()
    opts.input_file = "/path/to/movie.mov"
    opts.output_dir = "/tmp/tiff_sequence"
    opts.output_format = "tiff"
    opts.bit_depth = 16

    result = imfwizard.transcode_to_sequence(opts)
    if result.success:
        print(f"Transcoded {result.frame_count} frames ({result.width}x{result.height} @ {result.fps}fps)")
    else:
        print(f"Error: {result.error}")


def encode_example():
    """Encode TIFF sequence to JPEG 2000."""
    opts = imfwizard.EncodeOptions()
    opts.input_dir = "/tmp/tiff_sequence"
    opts.output_dir = "/tmp/j2k_output"
    opts.target_bitrate_mbps = 250.0
    opts.color_space = imfwizard.ColorSpace_BT2020
    opts.bit_depth = 12
    opts.cinema_profile = True

    result = imfwizard.encode_to_j2k(opts)
    if result.success:
        print(f"Encoded {result.frame_count} frames to J2K")
    else:
        print(f"Error: {result.error}")


def loudness_example():
    """Measure and normalize audio loudness."""
    # Measure
    loud = imfwizard.measure_loudness("/path/to/audio.wav")
    if loud.success:
        print(f"Integrated: {loud.integrated_lufs:.1f} LUFS")
        print(f"True Peak: {loud.true_peak_dbtp:.1f} dBTP")
        print(f"R128 compliant: {loud.compliant_r128}")

    # Normalize
    norm_opts = imfwizard.NormalizeOptions()
    norm_opts.input_file = "/path/to/audio.wav"
    norm_opts.output_file = "/tmp/normalized.wav"
    norm_opts.target_lufs = -23.0

    norm = imfwizard.normalize_audio(norm_opts)
    if norm.success:
        print(f"Normalized to {norm.measured.integrated_lufs:.1f} LUFS")


def info_example():
    """Read IMP info."""
    info = imfwizard.read_imp_info("/path/to/imp")
    if info.valid:
        print(f"Title: {info.cpl_title}")
        print(f"CPL: {info.cpl_uuid}")
        print(f"Tracks: {len(info.tracks)}")
        for t in info.tracks:
            print(f"  {t.type}: {t.filename} ({t.size} bytes)")
    else:
        print(f"Error: {info.error}")


if __name__ == "__main__":
    info_example()
    validate_example()
    transcode_example()
    encode_example()
    loudness_example()
