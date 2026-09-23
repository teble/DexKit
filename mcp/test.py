#!/usr/bin/env python3
"""Build fixtures, run independent interop and MCP HTTP/pipe/schema tests."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parent.parent
MANIFEST = ROOT / "mcp/Cargo.toml"


def run(*args):
    subprocess.run(list(map(str, args)), cwd=ROOT, check=True)


run(sys.executable, ROOT / "tests/interop/planus/run.py")
run("cargo", "test", "--manifest-path", MANIFEST, "--workspace", "--all-targets", "--locked")
run("cargo", "build", "--manifest-path", MANIFEST, "-p", "dexkit-mcp", "--locked")
run("cargo", "build", "--manifest-path", MANIFEST, "-p", "dexkit-mcp", "--locked", "--example", "http_smoke")
run(sys.executable, ROOT / "mcp/tests/test_stdio.py")
run(sys.executable, ROOT / "mcp/tests/test_http.py")
run("cargo", "run", "--manifest-path", MANIFEST, "-p", "dexkit-mcp", "--locked",
    "--example", "validate_contract", "--", ROOT / "mcp/target/contract-cases.json")
run("cargo", "run", "--manifest-path", MANIFEST, "-p", "dexkit-mcp", "--locked",
    "--example", "validate_contract", "--", ROOT / "mcp/target/http-contract-cases.json")
