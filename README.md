# IMF Wizard

Interoperable Master Format (IMF) package creator — CLI tool and desktop GUI.

## Overview

IMF Wizard creates valid IMF packages (Interoperable Master Packages) from
JPEG 2000 codestreams and WAV audio, conforming to SMPTE ST 2067 (App#2E).

## Features

- **Phase 1 (current):** Create Original Version IMPs from J2K + WAV
- Wrap essence into AS-02 MXF track files (SMPTE 2067-5)
- Generate CPL (ST 2067-3), PKL (ST 429-8), AssetMap (ST 429-9)
- SHA-1 hash computation for all assets
- Optional XML-DSIG signing

## Building

### Prerequisites

- C++23 compiler (GCC 13+, Clang 16+, MSVC 2022)
- CMake 3.25+
- libxml2
- OpenSSL
- Xerces-C++

### Build

```bash
git clone --recurse-submodules https://github.com/youruser/imfwizard.git
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

### Create an Original Version IMP

```bash
imfwizard create \
  --title "My Feature Film" \
  --video /path/to/j2k_frames/ \
  --audio /path/to/audio.wav \
  --output /path/to/output_imp/ \
  --fps-num 24 --fps-den 1
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

## Roadmap

- **Phase 2:** Supplemental IMP / Versioning
- **Phase 3:** Encoding pipeline (raw images → J2K via grok)
- **Phase 4:** Full GUI with timeline editor

## License

GPL-3.0 — see [LICENSE](LICENSE)
