#!/usr/bin/env python3
"""Build isolated Android size variants using an existing Gradle CMake configuration."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, required=True, help="Gradle ABI CMakeCache.txt")
    parser.add_argument("--output", type=Path, required=True, help="External experiment directory")
    parser.add_argument("--modes", nargs="+", choices=["off", "on", "ir", "light"], default=["off", "on"])
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    cache = {}
    for line in args.cache.read_text().splitlines():
        if line and not line.startswith(("#", "//")) and ":" in line and "=" in line:
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    keys = ["ANDROID_ABI", "ANDROID_NDK", "ANDROID_PLATFORM", "ANDROID_STL", "CMAKE_TOOLCHAIN_FILE",
            "CMAKE_FIND_ROOT_PATH", "CMAKE_C_FLAGS", "CMAKE_CXX_FLAGS", "CMAKE_C_FLAGS_RELEASE",
            "CMAKE_CXX_FLAGS_RELEASE", "CMAKE_MAKE_PROGRAM"]
    common = {key: cache[key] for key in keys}
    common.update(CMAKE_BUILD_TYPE="Release", DEXKIT_ENABLE_INTERNAL_METRICS="OFF",
                  DEXKIT_ENABLE_INTERNAL_METRICS_API="OFF")
    args.output.mkdir(parents=True, exist_ok=True)
    args.output = args.output.resolve()
    abi = cache["ANDROID_ABI"]
    ndk = Path(cache["ANDROID_NDK"])
    tool_bin = next((ndk / "toolchains/llvm/prebuilt").glob("*/bin"))
    result = {
        "commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
        "dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=root, text=True)),
        "abi": abi, "configuration": common, "variants": {},
    }
    for mode in args.modes:
        directory = args.output / f"build-{abi}-{mode}"
        settings = dict(common, DEXKIT_ENABLE_SMALI="ON" if mode == "on" else "OFF",
                        DEXKIT_SMALI_SIZE_PROBE=mode if mode in ("ir", "light") else "none")
        settings["CMAKE_SHARED_LINKER_FLAGS"] = f"-Wl,-Map,{args.output / (abi + '-' + mode + '.map')}"
        with (args.output / f"{abi}-{mode}.log").open("w") as log:
            subprocess.run(["cmake", "-S", str(root / "dexkit-android"), "-B", str(directory), "-G", "Ninja"] +
                           [f"-D{k}={v}" for k, v in settings.items()], check=True, stdout=log, stderr=log)
            subprocess.run(["cmake", "--build", str(directory), "--target", "dexkit", "-j", "4"],
                           check=True, stdout=log, stderr=log)
        library = args.output / f"{abi}-{mode}.so"
        shutil.copyfile(directory / "libdexkit.so", library)
        exports = subprocess.check_output([str(tool_bin / "llvm-nm"), "-D", "--defined-only", str(library)], text=True)
        has_probe = "DexKitSmaliSizeProbe" in exports or "DexKitSmaliControlProbe" in exports
        if has_probe != (mode in ("ir", "light")):
            raise RuntimeError(f"Unexpected probe export in {mode}")
        sections = subprocess.check_output([str(tool_bin / "llvm-size"), "--format=sysv", str(library)], text=True)
        result["variants"][mode] = {"bytes": library.stat().st_size, "sections": sections,
                                    "has_probe": has_probe, "has_smali_jni": "nativeGetSmali" in exports}
        print(abi, mode, library.stat().st_size, flush=True)
    (args.output / f"{abi}.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
