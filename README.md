<h1>
  <img alt="BESS:" style="height: 2em;" src="assets/images/logo/BessLogo.png"/> 
  Basic Electrical Simulation Software
</h1>


BESS is a software for simulating electrical circuits (for now digital components). It is designed to be simple to use and easy to understand. It is intended for educational purposes, but can also be used for professional work.

It is written in `C++` and uses its own renderer made with `OpenGL`.
It now uses its own `Time Based Simulation Engine`.

## Screenshots

<div align="center">
  <img src="screenshots/ss1.png" alt="BESS SS1" width="45%" />
  <img src="screenshots/ss2.png" alt="BESS SS2" width="45%" />
</div>

## Build & Run
Tested Build On:
- [x] Linux (Arch)
- [x] Windows
- [ ] MacOS

Following commands are only valid for Linux, as build scripts for windows have not been written yet.
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
## TODO
- [ ] Command System
- [ ] Test Cases for simulation engine
- [ ] Undo, Redo functionality
- [ ] Using emscripten to host on web
- [ ] Move to vulkan
