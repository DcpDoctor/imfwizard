# IMF Wizard

Interoperable Master Format (IMF) package creator — CLI tool and desktop GUI.

## Overview

IMF Wizard creates valid IMF packages (Interoperable Master Packages) from
video sources, image sequences, and WAV audio, conforming to SMPTE ST 2067 (App#2E).

## Features

### Packaging & Wrapping
- **Original Version IMP creation** from J2K + WAV
- **Supplemental IMP** creation with segment replacement
- **Multi-CPL IMP** — multiple compositions sharing a track pool
- **TTML / IMSC subtitle** packaging as AS-02 timed text MXF
- **Closed captions** — SCC (CEA-608) and SRT → TTML conversion
- **Accessibility tracks** — Audio Description, Hearing Impaired, Sign Language, Commentary
- **Multi-language audio** with RFC 5646 language tags and MCA labels
- **AS-02 MXF wrapping** (SMPTE 2067-5), CPL/PKL/AssetMap generation
- **SHA-1 hashing** and optional **XML-DSIG signing**

### Encoding & Transcoding
- **Image encoding pipeline** — DPX, TIFF, EXR, PNG, BMP, JPEG → JPEG 2000 (via grok or OpenJPEG)
- **ProRes / DNxHR / H.264 / H.265 transcoding** — video files → image sequence → J2K (via ffmpeg)
- **ProRes IMF packaging** — create App#2E IMPs directly from ProRes .mov files
- **ACES (App#5) colour management** — ACES transforms during encode
- **Scale / crop / letterbox** options for resolution adaptation
- **Subtitle burn-in** — permanently render SRT/TTML/SCC into video frames (for festivals)

### HDR & Advanced
- **Dolby Vision 4.0** RPU metadata injection (via dovi_tool)
- **HDR10+ dynamic metadata** injection (via hdr10plus_tool)
- **HDR/WCG color metadata** — ST 2067-21 (PQ, HLG, BT.2020, P3-D65)
- **IAB / Dolby Atmos** immersive audio packaging

### Quality Control
- **Loudness analysis** — EBU R128 / ATSC A/85 measurement and normalization
- **Photon validation** — validate output IMPs using Netflix Photon
- **Frame-level QC** — per-frame bitrate analysis with over/under-budget detection
- **VMAF / PSNR / SSIM** quality metrics (via ffmpeg libvmaf)
- **Bitrate analytics** — per-second throughput, histogram, standard deviation (JSON output for dashboards)
- **QC HTML report** generation

### Workflow & Automation
- **Delivery presets** — Netflix, Disney+, Amazon, Apple TV+, Cinema 2K/4K, Broadcast, Archival
- **Batch delivery** — create IMPs for multiple platforms in a single pass
- **IMF-to-DCP conversion** — repackage IMF as Digital Cinema Package for theatrical playback
- **Job queue daemon** — background processing with Unix socket / Windows named pipe IPC
- **Watch folder** — auto-IMP creation when files appear
- **EDL/AAF conform** — import CMX3600 edit decisions to auto-build CPL timelines
- **S3 cloud upload** — push completed IMPs to AWS S3
- **Aspera FASP** — high-speed delivery via IBM Aspera
- **Partial restore** — extract tracks/segments from existing IMPs back to raw files

### Desktop GUI (Tauri 2)
- **Dark theme** by default with optional light mode toggle
- **Drag-and-drop** file import (WAV, TTML, image sequences)
- **Timeline editor** — visual segment arrangement for multi-track compositions
- **Supplemental IMP wizard** — guided workflow for versioned supplements
- **Loudness metering panel** — EBU R128 / ATSC A/85 compliance badges
- **IMP metadata editor** — edit CPL annotations, content versioning, locale info
- **Delivery preset selector** — one-click configuration for major platforms
- **Preview player** — J2K frame-by-frame playback with waveform display
- **Bitrate analytics dashboard** — per-second charts, histogram, statistics
- **Subtitle burn-in** — GUI for hardcoding subs into video
- **Batch delivery panel** — deliver to multiple platforms with checkboxes
- **IMF-to-DCP converter** — one-click conversion for cinema delivery
- **Job queue manager** — submit, monitor, cancel background jobs
- **Progress notifications** — system notifications when jobs complete
- **Recent projects** — quick access to previously created IMPs

## Building

### Prerequisites

- C++23 compiler (GCC 13+, Clang 16+, MSVC 2022)
- CMake 3.25+
- libxml2
- OpenSSL
- Xerces-C++

### Optional runtime dependencies

- **grok** or **OpenJPEG** — for image→J2K encoding (`grk_compress` / `opj_compress`)
- **ffmpeg** / **ffprobe** — for transcoding, loudness, channel remapping, quality metrics
- **dovi_tool** — for Dolby Vision RPU injection
- **hdr10plus_tool** — for HDR10+ dynamic metadata injection
- **Java** + **Netflix Photon** — for IMF validation
- **AWS CLI** — for S3 upload

### Build

```bash
git clone --recurse-submodules https://github.com/DcpDoctor/imfwizard.git
cd imfwizard
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Run tests

```bash
cd build
ctest
```

### GUI (Tauri 2)

```bash
cd gui
npm install
npm run tauri dev
```

To build a release binary:

```bash
cd gui
npm run tauri build
```

The built app will be in `gui/src-tauri/target/release/bundle/`.

## Usage

### Create an IMP from J2K + WAV

```bash
imfwizard create \
  --title "My Feature Film" \
  --video /path/to/j2k_frames/ \
  --audio /path/to/audio.wav \
  --output /path/to/output_imp/ \
  --fps-num 24 --fps-den 1
```

### Create an IMP with subtitles and HDR

```bash
imfwizard create \
  --title "My HDR Film" \
  --video /path/to/j2k_frames/ \
  --audio /path/to/audio.wav \
  --subtitle /path/to/subs.ttml \
  --color-space bt2020-pq \
  --output /path/to/output/
```

### Create an IMP from non-J2K images (auto-encode)

```bash
# Input can be DPX, TIFF, EXR, PNG — automatically encoded to J2K
imfwizard create \
  --title "My Film" \
  --video /path/to/dpx_frames/ \
  --audio /path/to/audio.wav \
  --output /path/to/output/
```

### Transcode ProRes to image sequence

```bash
imfwizard transcode \
  -i input.mov \
  -o /path/to/frames/ \
  -f tiff --bit-depth 16
```

### Encode image sequence to JPEG 2000

```bash
imfwizard encode \
  -i /path/to/tiff_frames/ \
  -o /path/to/j2k_output/ \
  --bitrate 250
```

### Create a Supplemental IMP

```bash
imfwizard supplement \
  --title "Version 2" \
  --ov /path/to/original_imp/ \
  --video /path/to/replacement_frames/ \
  --output /path/to/supplemental_imp/ \
  --entry-point 100 --duration 50
```

### Measure loudness

```bash
imfwizard loudness /path/to/audio.wav
```

### Validate an IMP

```bash
imfwizard validate /path/to/imp/
```

### Display IMP info

```bash
imfwizard info /path/to/existing_imp/
```

### Delivery presets

```bash
# Use a preset to auto-configure encoding parameters
imfwizard create \
  --title "Netflix Delivery" \
  --preset netflix \
  --video /path/to/dpx_frames/ \
  --audio /path/to/audio.wav \
  --output /path/to/output/
```

### Batch delivery to multiple platforms

```bash
imfwizard deliver \
  --video /path/to/j2k_frames/ \
  --audio /path/to/audio.wav \
  --title "My Feature" \
  --output /path/to/deliveries/ \
  --targets netflix disney amazon apple
```

### Create IMP from ProRes

```bash
imfwizard prores \
  -i /path/to/master.mov \
  -o /path/to/output_imp/ \
  -t "ProRes Master"
```

### Burn subtitles into video

```bash
imfwizard burn-in \
  -i /path/to/video.mp4 \
  -s /path/to/subs.srt \
  -o /path/to/output_burned.mp4 \
  --font-size 56
```

### Convert IMF to DCP

```bash
imfwizard to-dcp \
  -i /path/to/imp/ \
  -o /path/to/dcp_output/ \
  -t "Cinema Release"
```

### Bitrate analytics

```bash
# Human-readable summary
imfwizard analytics -d /path/to/j2k_frames/

# JSON output for dashboards
imfwizard analytics -d /path/to/j2k_frames/ --json -o report.json
```

### Generate preview thumbnails

```bash
# Single frame
imfwizard preview -d /path/to/j2k/ -o /tmp/thumbs/ -f 42

# Thumbnail strip (10 evenly spaced frames)
imfwizard preview -d /path/to/j2k/ -o /tmp/thumbs/ --strip
```

## Architecture

```
imfwizard/
├── include/imfwizard/   # Public headers
├── src/                 # Core library + CLI
├── tests/               # Unit + integration tests
├── gui/                 # Tauri 2 desktop application
│   ├── src/             # Frontend (Vite + vanilla JS)
│   └── src-tauri/       # Rust backend (plugin shell)
├── docs/                # GitHub Pages site
└── extern/              # Git submodules
    ├── asdcplib/        # AS-DCP + AS-02 MXF (BSD)
    ├── CLI11/           # CLI parsing (BSD)
    └── spdlog/          # Logging (MIT)
```

## License

GPL-3.0 — see [LICENSE](LICENSE)
