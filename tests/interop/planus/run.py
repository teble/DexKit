#!/usr/bin/env python3
"""Assemble the test DEX and run the standalone Rust/native probe."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[3]
EXPERIMENT = Path(__file__).resolve().parent

subprocess.run([
    "bash", str(ROOT / "gradlew"), "-I", str(EXPERIMENT / "fixtures.gradle"),
    ":dexkit:planusInteropFixture",
], cwd=ROOT, check=True)
environment = os.environ.copy()
environment["DEXKIT_PLANUS_FIXTURE"] = str(ROOT / "tests/interop/planus/target/fixture/fixture.dex")
subprocess.run([
    "cargo", "test", "--manifest-path", str(EXPERIMENT / "Cargo.toml"),
    "-p", "dexkit-planus-interop", "--locked", "--", "--nocapture",
], cwd=ROOT, env=environment, check=True)
