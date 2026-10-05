#!/usr/bin/env python3
"""Exercise Android stack layouts against native callbacks and the real bridge."""
import ctypes as C
import json
import platform
import sys

if platform.system() != "Darwin" or platform.machine() != "arm64":
    raise SystemExit("This ABI regression requires Apple Silicon macOS")

shim = C.CDLL(sys.argv[1])
shim.bridge.argtypes, shim.bridge.restype = (C.c_char_p, C.c_void_p), C.c_void_p
u, i, p = C.c_uint, C.c_int, C.c_void_p
signatures = {
    'copy': [u, u, i, i, i, i, u, u, i, i, i, i, i, i, i],
    'upload': [u, i, i, i, i, i, i, i, u, u, p],
    'compressed': [u, i, i, i, i, i, i, i, u, i, p],
    'blit': [i, i, i, i, i, i, i, i, u, u],
}
results = []
for name, signature in signatures.items():
    observed = []
    native = C.CFUNCTYPE(None, *signature)(lambda *args: observed.append(args))
    android = C.CFUNCTYPE(None, *(p if t is p else C.c_uint64 for t in signature))
    bridged = android(shim.bridge(name.encode(), C.cast(native, p)))
    direct = android(C.cast(native, p).value)
    for case in range(3):
        expected = tuple((0x1234567890 + index if t is p else
                          (-500 - index if t is i and case == 1 else 100 + case * 100 + index))
                         for index, t in enumerate(signature))
        # Poison upper bits: AAPCS64 does not promise useful high register bits
        # for narrow integer arguments, including the first eight arguments.
        args = tuple(value if t is p else (0xBAD0FEED00000000 | (value & 0xFFFFFFFF))
                     for t, value in zip(signature, expected))
        bridged(*args)
        assert observed[-1] == expected, (name, observed[-1], expected)
        direct(*args)
        assert observed[-1] != expected, (name, 'negative control unexpectedly matched')
    results.append({'signature': name, 'bridged_cases_passed': 3, 'unbridged_failures_reproduced': 3})
print(json.dumps({'passed': True, 'results': results}, indent=2))
