#!/usr/bin/env python3
"""Load bessplug with its native deps, then run pybind11-stubgen.

On Windows, Python 3.8+ does not search PATH for extension-module DLL
dependencies. BessRuntime.dll and friends live in the app runtime dir, so we
add that directory via os.add_dll_directory and also copy the DLLs next to the
.pyd (which is always searched).
"""

from __future__ import annotations

import os
import shutil
import sys


def _stage_runtime_dlls(runtime_dir: str, module_dir: str) -> list[str]:
    """Copy runtime DLLs beside the extension module. Returns staged names."""
    staged: list[str] = []
    if sys.platform != "win32":
        return staged

    for name in os.listdir(runtime_dir):
        if not name.lower().endswith(".dll"):
            continue
        src = os.path.join(runtime_dir, name)
        if not os.path.isfile(src):
            continue
        dst = os.path.join(module_dir, name)
        try:
            if (not os.path.exists(dst)) or (
                os.path.getmtime(src) > os.path.getmtime(dst)
            ):
                shutil.copy2(src, dst)
            staged.append(name)
        except OSError as exc:
            print(f"Warning: could not stage {name}: {exc}", file=sys.stderr)
    return staged


def main() -> int:
    if len(sys.argv) != 4:
        print(
            "Usage: run_stubgen.py <runtime_dir> <module_dir> <out_dir>",
            file=sys.stderr,
        )
        return 2

    runtime_dir = os.path.abspath(sys.argv[1])
    module_dir = os.path.abspath(sys.argv[2])
    out_dir = os.path.abspath(sys.argv[3])

    if not os.path.isdir(runtime_dir):
        print(f"Runtime dir does not exist: {runtime_dir}", file=sys.stderr)
        return 1
    if not os.path.isdir(module_dir):
        print(f"Module dir does not exist: {module_dir}", file=sys.stderr)
        return 1

    staged = _stage_runtime_dlls(runtime_dir, module_dir)

    if sys.platform == "win32":
        os.add_dll_directory(runtime_dir)
        os.add_dll_directory(module_dir)
        os.environ["PATH"] = (
            runtime_dir
            + os.pathsep
            + module_dir
            + os.pathsep
            + os.environ.get("PATH", "")
        )
    else:
        os.environ["LD_LIBRARY_PATH"] = (
            runtime_dir + os.pathsep + os.environ.get("LD_LIBRARY_PATH", "")
        )

    sys.path.insert(0, module_dir)
    existing = os.environ.get("PYTHONPATH", "")
    os.environ["PYTHONPATH"] = (
        module_dir if not existing else module_dir + os.pathsep + existing
    )

    try:
        import importlib

        importlib.invalidate_caches()
        importlib.import_module("bessplug")
    except Exception as exc:
        print(f"Failed to import bessplug: {exc}", file=sys.stderr)
        print(f"  runtime_dir={runtime_dir}", file=sys.stderr)
        print(f"  module_dir={module_dir}", file=sys.stderr)
        print(f"  staged_dlls={len(staged)}: {', '.join(staged[:30])}", file=sys.stderr)
        return 1

    from pybind11_stubgen import main as stubgen_main

    sys.argv = ["pybind11-stubgen", "bessplug", "-o", out_dir]
    stubgen_main()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
