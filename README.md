# Basic Electrical Simulation Software - BESS

> This branch will include EnTT integration, ui refactoring and new time based simulation engine.

BESS is a software for simulating electrical circuits. It is designed to be simple to use and easy to understand. It is intended for educational purposes, but can also be used for professional work.

It is written in `C++` and uses its own renderer made with `OpenGL`.
It now uses its own `Time Based Simulation Engine`.


![Screenshot 1](screenshots/ss1.png)
![Screenshot 2](screenshots/ss2.png)

## Build Steps
> Run all script files from `CMake Source File Folder` (./scripts/script_name.sh)

- To run in debug mode `./scripts/build_run_debug.sh`
_Make sure to run `./scripts/clean.sh` or clean the `build` folder before running other build script._
- To build in release mode `./scripts/build_release.sh`.
    - Running this script will build the project in release mode and also copy the assets in the correct folder
    - Navigate to build path e.g. bin/Relase/x64/ and run ./Bess
