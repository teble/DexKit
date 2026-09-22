#!/usr/bin/env python3
"""Compare the opt-in S0 probe exports from two separately built host libraries."""

import argparse
import ctypes
import json
import time


def load(path):
    library = ctypes.CDLL(path)
    function = library.DexKitSmaliSizeProbe
    function.argtypes = [ctypes.POINTER(ctypes.c_uint16), ctypes.c_size_t,
                         ctypes.c_uint16, ctypes.c_void_p, ctypes.c_size_t]
    function.restype = ctypes.c_size_t
    return function


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ir", required=True, help="CodeIr probe library")
    parser.add_argument("--light", required=True, help="Decoder probe library")
    parser.add_argument("--iterations", type=int, default=200)
    args = parser.parse_args()
    if args.iterations < 1:
        parser.error("iterations must be positive")
    functions = {"ir": load(args.ir), "light": load(args.light)}
    output = ctypes.create_string_buffer(400000)
    fixtures = [
        [0x1012, 0x0101, 0x0138, 4, 0x0013, 0xffff, 0x000e],
        [0x0228, 0x0012, 0x000e],
        [0x000e],
    ]
    for fixture in fixtures:
        code = (ctypes.c_uint16 * len(fixture))(*fixture)
        texts = []
        for function in functions.values():
            size = function(code, len(code), 2, output, len(output))
            assert 0 < size <= len(output)
            texts.append(bytes(output[:size]))
        assert texts[0] == texts[1], texts
    for fixture in ([0x00ff], [0x0038], [0x0038, 1, 0x000e], [0xf012, 0x000e]):
        code = (ctypes.c_uint16 * len(fixture))(*fixture)
        for function in functions.values():
            assert function(code, len(code), 0, output, len(output)) == 0
    code = (ctypes.c_uint16 * 5001)(*([0x1012] * 5000 + [0x000e]))
    results = {}
    for name, function in functions.items():
        for _ in range(10):
            function(code, len(code), 2, output, len(output))
        start = time.perf_counter()
        for _ in range(args.iterations):
            size = function(code, len(code), 2, output, len(output))
        results[name] = {
            "microseconds_per_call": (time.perf_counter() - start) * 1e6 / args.iterations,
            "output_bytes": size,
        }
    print(json.dumps(results, indent=2))


if __name__ == "__main__":
    main()
