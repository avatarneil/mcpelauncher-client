#!/usr/bin/env python3
"""Compile the production input methods against recording platform doubles.

Run: python3 tests/test_gamepad_neutral.py [--source path/to/window_callbacks.cpp]
Requires a C++17 compiler (CXX, or c++). Does not launch Minecraft. This focused
harness does not replace a complete runtime build or in-game integration tests.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile


def method(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=root / 'src/window_callbacks.cpp')
    args = parser.parse_args()
    source = args.source.read_text()
    methods = '\n'.join(method(source, signature) for signature in [
        'bool WindowCallbacks::hasInputMode(',
        'void WindowCallbacks::setCursorLocked(',
        'void WindowCallbacks::queueGamepadAxisInputIfNeeded(',
        'void WindowCallbacks::onGamepadAxis(',
        'void WindowCallbacks::onGamepadButton(',
    ])
    harness = (root / 'tests/gamepad_neutral_harness.cpp').read_text()
    with tempfile.TemporaryDirectory(prefix='gamepad-neutral-') as directory:
        cpp = Path(directory) / 'test.cpp'
        executable = Path(directory) / 'test'
        cpp.write_text(harness.replace('// PRODUCTION_METHODS', methods))
        subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) + [
            '-std=c++17', '-Wall', '-Wextra', '-Werror',
            '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
            str(cpp), '-o', str(executable),
        ], check=True)
        subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    main()
