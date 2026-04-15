#!/usr/bin/env bash
set -euo pipefail

if [ "${BESS_SKIP_BOOTSTRAP:-0}" = "1" ]; then
    echo "Skipping dependency bootstrap because BESS_SKIP_BOOTSTRAP=1"
    exit 0
fi

# In non-interactive environments (e.g. CI), allow opting in without a prompt.
if [ "${BESS_AUTO_BOOTSTRAP:-0}" = "1" ]; then
    BESS_BOOTSTRAP_CONFIRMED=1
else
    BESS_BOOTSTRAP_CONFIRMED=0
fi

require_cmd() {
    local cmd="$1"
    if command -v "$cmd" >/dev/null 2>&1; then
        return 0
    fi
    return 1
}

confirm_bootstrap() {
    if [ "${BESS_BOOTSTRAP_CONFIRMED}" = "1" ]; then
        return 0
    fi

    # No tty available: skip prompt and bootstrap unless explicitly opted in.
    if [ ! -t 0 ]; then
        echo "Skipping dependency bootstrap (non-interactive shell). Set BESS_AUTO_BOOTSTRAP=1 to allow automatic install." >&2
        return 1
    fi

    echo "Dependency bootstrap may install system packages using sudo."
    printf "Continue? [y/N]: "
    read -r answer
    case "${answer}" in
        [yY]|[yY][eE][sS])
            BESS_BOOTSTRAP_CONFIRMED=1
            return 0
            ;;
        *)
            echo "Skipping dependency bootstrap by user choice."
            return 1
            ;;
    esac
}

install_linux_apt() {
    sudo apt-get update
    sudo apt-get install -y \
        build-essential \
        cmake \
        git \
        ninja-build \
        pkg-config \
        python3 \
        python3-dev \
        python3-pip \
        pybind11-dev \
        libx11-dev \
        libxrandr-dev \
        libxinerama-dev \
        libxcursor-dev \
        libxi-dev \
        libxext-dev \
        libxrender-dev \
        vulkan-tools \
        libvulkan-dev
}

install_linux_pacman() {
    sudo pacman -Sy --needed --noconfirm \
        base-devel \
        cmake \
        git \
        ninja \
        pkgconf \
        python \
        python-pip \
        pybind11 \
        libx11 \
        libxrandr \
        libxinerama \
        libxcursor \
        libxi \
        vulkan-tools \
        vulkan-headers \
        vulkan-icd-loader
}

install_linux_dnf() {
    sudo dnf install -y \
        @development-tools \
        cmake \
        git \
        ninja-build \
        pkgconf-pkg-config \
        python3 \
        python3-devel \
        python3-pip \
        pybind11-devel \
        libX11-devel \
        libXrandr-devel \
        libXinerama-devel \
        libXcursor-devel \
        libXi-devel \
        vulkan-tools \
        vulkan-loader-devel
}

install_python_tools() {
    if ! require_cmd python3; then
        echo "python3 not found; skipping pybind11 Python tooling install." >&2
        return 0
    fi

    if ! python3 -m pip --version >/dev/null 2>&1; then
        echo "python3 pip not available; skipping pybind11 Python tooling install." >&2
        return 0
    fi

    if ! python3 -m pip show pybind11 >/dev/null 2>&1; then
        python3 -m pip install --user pybind11
    fi

    if ! python3 -m pip show pybind11-stubgen >/dev/null 2>&1; then
        python3 -m pip install --user pybind11-stubgen
    fi
}

install_linux() {
    if ! confirm_bootstrap; then
        return 0
    fi

    if require_cmd apt-get; then
        install_linux_apt
    elif require_cmd pacman; then
        install_linux_pacman
    elif require_cmd dnf; then
        install_linux_dnf
    else
        echo "Unsupported Linux package manager. Install cmake, git, ninja, X11 dev packages, and Vulkan loader manually." >&2
        return 0
    fi

    install_python_tools
}

install_macos() {
    if ! confirm_bootstrap; then
        return 0
    fi

    if ! require_cmd brew; then
        echo "Homebrew not found. Installing Homebrew..."
        /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
    fi

    brew update
    brew install cmake git ninja pkg-config python pybind11 molten-vk vulkan-loader vulkan-headers

    install_python_tools
}

OS="$(uname -s)"
case "${OS}" in
    Linux)
        install_linux
        ;;
    Darwin)
        install_macos
        ;;
    *)
        echo "bootstrap_deps.sh is for Linux/macOS only. Use scripts/bootstrap_deps.ps1 on Windows."
        ;;
esac

echo "Dependency bootstrap finished."
