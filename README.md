# BESS: Basic Electrical Simulation Software

> **Branch focus:** EnTT integration, UI refactoring, and a new time‑based simulation engine.

BESS is a software for simulating electrical circuits. It is designed to be simple to use and easy to understand. It is intended for educational purposes, but can also be used for professional work.

It is written in `C++` and uses its own renderer made with `OpenGL`.
It now uses its own `Time Based Simulation Engine`.

## 📸 Screenshots

<div align="center">
  <img src="screenshots/ss1.png" alt="BESS SS1" width="45%" />
  <img src="screenshots/ss2.png" alt="BESS SS2" width="45%" />
</div>

## ⚙️ Build & Run

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
