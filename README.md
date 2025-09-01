# Guitar-PlusPlus

[![Build Status](https://github.com/Fabio3rs/Guitar-PlusPlus/workflows/Build%20and%20test%20Guitar%2B%2B/badge.svg)](https://github.com/Fabio3rs/Guitar-PlusPlus/actions)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](#license)
[![C++ Standard](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/std/the-standard)

A Brazilian indie rhythm game inspired by Guitar Hero, built with modern C++17 and OpenGL. Guitar++ features a complete guitar simulation experience with custom song charts, multiplayer support, and Lua scripting capabilities.

> **Development Status**: This is an active open-source project. While functional, it's in ongoing development and may have some rough edges.

## 🎸 Demo

🎬 **[Watch Gameplay Video](https://www.youtube.com/watch?v=p2zfNnjcJ2M)**

## ✨ Features

- **🎵 Rhythm Gameplay**: Full 5-fret guitar simulation with notes, chords, and slides
- **🎮 Multiple Game Modes**: 
  - Campaign mode with progression
  - Multiplayer support (LAN) - *Incomplete, development testing only*
  - Charter mode for creating custom songs
- **🎨 Modern Graphics**: OpenGL-based 3D rendering with particle effects
- **🔧 Lua Scripting**: Extensible gameplay through Lua scripts
- **📊 Advanced Features**:
  - BPM synchronization and timing
  - HOPO (Hammer-on/Pull-off) detection
  - Slide notes and tap notes
  - Score tracking and save system
  - Multiple difficulty levels
- **🌐 Localization**: Multi-language support
- **🎵 Custom Songs**: Support for custom charts and audio files

## 📋 System Requirements

### Minimum Requirements
- **OS**: Linux (Debian/Ubuntu recommended), Windows (via build tools)
- **CPU**: x64 processor with SSE2 support
- **GPU**: OpenGL 2.1 compatible graphics card
- **RAM**: 512MB available memory
- **Storage**: 100MB free space (plus space for songs/assets)

### Dependencies
- **Audio**: BASS & BASS_FX libraries (from [un4seen.com](http://www.un4seen.com/))
- **Graphics**: OpenGL 2.1+, GLEW
- **Window Management**: GLFW (included as submodule)
- **Math**: GLM (included as submodule) 
- **Serialization**: Cereal (included as submodule)
- **Scripting**: Lua 5.x (embedded)

## 🚀 Installation

### Prerequisites

#### Linux (Debian/Ubuntu)
```bash
sudo apt update
sudo apt install xorg-dev extra-cmake-modules cmake build-essential gcc g++ libgl-dev libglew-dev
```

#### Other Linux Distributions
- **Fedora/RHEL**: `sudo dnf install cmake gcc-c++ mesa-libGL-devel glew-devel libX11-devel`
- **Arch**: `sudo pacman -S cmake gcc mesa glew libx11`

### BASS Audio Libraries Setup

1. Download BASS and BASS_FX from [un4seen.com](http://www.un4seen.com/)
2. Extract the following files to the `bass/` directory:
   - `bass.h`
   - `bass_fx.h` 
   - `libbass.so` (Linux) or `bass.dll` (Windows)
   - `libbass_fx.so` (Linux) or `bass_fx.dll` (Windows)

### Building from Source

```bash
# Clone with submodules
git clone --recurse-submodules https://github.com/Fabio3rs/Guitar-PlusPlus.git
cd Guitar-PlusPlus/

# If you forgot --recurse-submodules, initialize them:
git submodule update --init --recursive

# Create build directory
mkdir build
cd build

# Configure and build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Binary will be in build/bin/
```

> **Note**: The `--recurse-submodules` flag is crucial as it downloads required dependencies (GLFW, GLM, Cereal)

### Running the Game

```bash
# From the build directory
cd bin/
./GuitarPP
```

## 🎵 Adding Songs

1. Create a `data/songs/` directory in your build folder
2. Place song folders with the following structure:
   ```
   data/songs/YourSong/
   ├── song.ogg          # Audio file
   ├── chart.chart       # Chart file (Guitar Hero format)
   └── song.ini          # Song metadata (optional)
   ```

3. Charts can be created using tools like:
   - [Clone Hero Chart Editor](https://github.com/TheNathannator/GuitarGame_ChartFormats/blob/main/doc/FileFormats/.chart.md)
   - [Moonscraper Chart Editor](https://github.com/FireFox2000000/Moonscraper-Chart-Editor)

## 🎮 Controls

Default keyboard controls:
- **Frets**: A, S, D, F, G (Green, Red, Yellow, Blue, Orange)
- **Strum**: Up/Down arrow keys
- **Menu Navigation**: Arrow keys, Enter, Escape

*Note: Guitar controller support available - check in-game settings*

## 🔧 Development

### Building Tests
```bash
# In build directory
cmake .. -DCMAKE_BUILD_TYPE=Debug
make
ctest
```

### Code Style
- C++17 standard
- Follow existing code conventions
- Use clang-format for formatting

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📝 License

This software is provided "AS IS", without warranty of any kind, express or implied, including but not limited to the warranties of merchantability, fitness for a particular purpose and noninfringement. In no event shall the authors or copyright holders be liable for any claim, damages or other liability, whether in an action of contract, tort or otherwise, arising from, out of or in connection with the software or the use or other dealings in the software.

## 🔗 Links

- **Community Forum**: [BRModStudio](https://brmodstudio.forumeiros.com/t1590-guitar-topico-oficial)
- **Demo Video**: [YouTube](https://www.youtube.com/watch?v=p2zfNnjcJ2M)
- **BASS Audio**: [un4seen.com](http://www.un4seen.com/)

## 🛠️ Troubleshooting

### Common Issues

**Build fails with OpenGL errors**
- Ensure you have OpenGL development libraries installed
- Update your graphics drivers

**Build fails with missing submodules (GLM/GLFW/Cereal)**
- Make sure to clone with `--recurse-submodules` flag
- Or run `git submodule update --init --recursive` in an existing clone

**CMake cannot find BASS libraries**
- Verify BASS and BASS_FX files are in the `bass/` directory
- Check that library files match your platform (.so for Linux, .dll for Windows)

**Audio not working**
- Check that BASS libraries are correctly placed in `bass/` directory
- Ensure audio files are in OGG Vorbis format

**Game crashes on startup**
- Verify all dependencies are installed
- Run from the correct directory (where assets are located)
- Check the console output for error messages

For more help, visit the [community forum](https://brmodstudio.forumeiros.com/t1590-guitar-topico-oficial).

