# IMF Wizard

Interoperable Master Format (IMF) package creator — CLI tool and desktop GUI.

## Overview

IMF Wizard creates valid IMF packages (Interoperable Master Packages) from
video sources, image sequences, and WAV audio, conforming to SMPTE ST 2067 (App#2E).

## Features

- **Original Version IMP creation** from J2K + WAV
- **Supplemental IMP** creation with segment replacement
- **Multi-CPL IMP** — multiple compositions sharing a track pool
- **Image encoding pipeline** — DPX, TIFF, EXR, PNG, BMP, JPEG → JPEG 2000 (via grok or OpenJPEG)
- **ProRes / DNxHR / H.264 / H.265 transcoding** — video files → image sequence → J2K (via ffmpeg)
- **TTML / IMSC subtitle** packaging as AS-02 timed text MXF
- **Dolby Vision 4.0** RPU metadata injection (via dovi_tool)
- **Photon validation** — validate output IMPs using Netflix Photon
- **Scale / crop / letterbox** options for resolution adaptation
- AS-02 MXF track file wrapping (SMPTE 2067-5)
- CPL (ST 2067-3), PKL (ST 429-8), AssetMap (ST 429-9) generation
- SHA-1 hash computation for all assets
- Optional XML-DSIG signing
- Desktop GUI (Tauri 2)

## Building

### Prerequisites

- C++23 compiler (GCC 13+, Clang 16+, MSVC 2022)
- CMake 3.25+
- libxml2
- OpenSSL
- Xerces-C++

### Optional runtime dependencies

- **grok** or **OpenJPEG** — for image→J2K encoding (`grk_compress` / `opj_compress`)
- **ffmpeg** / **ffprobe** — for ProRes/DNxHR/H.264/H.265 transcoding
- **dovi_tool** — for Dolby Vision RPU injection
- **Java** + **Netflix Photon** — for IMF validation

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

### Validate an IMP

```bash
imfwizard validate /path/to/imp/
```

### Display IMP info

```bash
imfwizard info /path/to/existing_imp/
```

## GUI

The desktop GUI is built with Tauri 2. See `gui/` directory.

```bash
cd gui
npm install
npm run tauri dev
```

## Architecture

```
imfwizard/
├── include/imfwizard/   # Public headers
├── src/                 # Core library + CLI
├── tests/               # Unit tests
├── gui/                 # Tauri desktop application
│   ├── src/             # Frontend (Vite)
│   └── src-tauri/       # Rust backend
└── extern/              # Git submodules
    ├── asdcplib/        # AS-DCP + AS-02 MXF (BSD)
    ├── CLI11/           # CLI parsing (BSD)
    └── spdlog/          # Logging (MIT)
```

## License

GPL-3.0 — see [LICENSE](LICENSE)
