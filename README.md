# <img alt="BESS: Basic Electrical Simulation Software" src="assets/images/logo/NameLogo.png"/>
[![GitHub stars](https://img.shields.io/github/stars/shivang51/bess?style=social)](https://github.com/shivang51/bess)
[![GitHub forks](https://img.shields.io/github/forks/shivang51/bess?style=social)](https://github.com/shivang51/bess/network/members)
[![GitHub issues](https://img.shields.io/github/issues/shivang51/bess)](https://github.com/shivang51/bess/issues)
[![GitHub license](https://img.shields.io/github/license/shivang51/bess)](https://github.com/shivang51/bess/blob/main/LICENSE)

[![Discord](https://img.shields.io/discord/1475852976357773312?logo=discord&label=Discord)](https://discord.gg/cuB7c9q275)

BESS (Basic Electrical Simulation Software) is an open-source circuit simulator designed to be accessible, modern, and cross-platform.

The project began as an attempt to remove the barriers students often face when working with existing tools. Many popular circuit simulators are proprietary, restricted to a single operating system, or rely on outdated interfaces. BESS was created to provide a free, user-friendly, and modern alternative that works consistently across platforms. Its goal is to make learning and experimenting with circuits simpler and more approachable for everyone.

### Some of the nice things:
1. Very extensible:
   - One can write own plugins in Python to add new components, ui elements, and more.
   - One can write own simulation drivers to add more capablities. Currently I have digital components simulation driver and maths simulation driver.
2. Python plugins support hot reloading, for now only while in debug build.
3. Maintains good fps ~60fps for large components.
4. Verilog script import also works when working with digital components.
5. Cross platform support for windows and linux.

### Web Link -  [Check Here](https://bess-orpin.vercel.app/Bess)
> Web Support is currently limited as plugins are not supported for now.

## Screenshots - [More](screenshots/README.md)

<div align="center" height="100px">
  <img src="screenshots/ss1.png" alt="BESS SS1" width="45%"/>
  <img src="screenshots/ss2.png" alt="BESS SS2" width="45%"/>
</div>

## Build & Run
Tested Build On:
- [x] Linux (Arch Linux: Wayland)
- [x] Windows (Running in Github Actions)
- [x] macOS (Apple Silicon, macOS 26+)

Following commands are valid for Linux and macOS (see [macOS](#macos) for the extra one-time steps).
All build scripts live in the **scripts/** folder inside the CMake source directory.

1. **Clean previous builds** (if you are building after another build)
   ```bash
   ./scripts/clean.sh
   ```
2. **Debug build & run**  
   ```bash
   ./scripts/build_run_debug.sh
   ```
3. **Release build**  
   ```bash
   ./scripts/build_release.sh
   ```
   - Copies assets automatically.
   - Binaries will be in `bin/Release/x64/`.
   - **Execute**  
       ```bash
       cd bin/Release/x64/
       ./Bess
       ```

### macOS
Apple Silicon, macOS 26 or newer, with Xcode Command Line Tools and Homebrew.

1. **Prerequisites**
   ```bash
   brew install cmake ninja freetype python
   brew install yosys   # optional, only needed for Verilog import
   ```
2. **Dawn (WebGPU)** - `external/dawn/macos/lib/libwebgpu_dawn.dylib` is committed. To rebuild it
   (fetches the pinned Dawn commit into `.dawn-build/`, takes a while):
   ```bash
   ./scripts/build_dawn_macos.sh
   ```
3. **Build & run** with the same scripts as Linux. Binaries go to `bin/<Debug|Release>/arm64/`.
   Python stub generation is off by default on macOS; pass `BSTUBS=ON` if `pybind11-stubgen` is
   installed for the Python CMake finds.
4. **App bundle** - after a release build:
   ```bash
   ./scripts/package_macos_app.sh   # creates bin/Release/arm64/Bess.app
   open bin/Release/arm64/Bess.app
   ```
   The bundle is ad-hoc signed and self-contained: it embeds the Python framework it was built
   with (CMake picks the newest one installed) and the Homebrew dylibs it links, such as freetype.
   When launched from Finder, `yosys` is not on the PATH, so Verilog import only works when
   started from a terminal.
   The app writes its log to `~/Library/Logs/Bess/bess.log` and its UI layout to
   `~/Library/Application Support/Bess/bess.ini`, so it also runs from a read-only disk image.

## TODO
- [x] Architecture Overhaul and Usability Improvements - [Details](https://github.com/shivang51/bess/pull/25)
- [x] Web Support - Kind of done - [Check Here](https://bess-orpin.vercel.app/Bess)
- [ ] Architectural Changes
- [ ] Good Test Harness
- [ ] Stability and UX Improvements
