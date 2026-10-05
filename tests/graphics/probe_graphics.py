#!/usr/bin/env python3
"""Query the bundled ANGLE backend without launching Minecraft or touching saves."""
import argparse
import ctypes as C
import json
import os
from pathlib import Path


def probe(frameworks, exercise=None):
    os.environ['ANGLE_DEFAULT_PLATFORM'] = 'vulkan'
    os.environ['VK_ICD_FILENAMES'] = str(frameworks / 'MoltenVK_icd.json')
    egl = C.CDLL(str(frameworks / 'libEGL.dylib'))
    def fn(name, result, *args):
        f = getattr(egl, name)
        f.restype, f.argtypes = result, args
        return f
    ptr, integer = C.c_void_p, C.c_int
    get_error = fn('eglGetError', integer)
    def check(value, operation):
        if not value:
            raise RuntimeError(f'{operation}: EGL error {get_error():#x}')
        return value
    display = check(fn('eglGetDisplay', ptr, ptr)(None), 'get display')
    major, minor = integer(), integer()
    check(fn('eglInitialize', integer, ptr, ptr, ptr)(display, C.byref(major), C.byref(minor)), 'initialize')
    check(fn('eglBindAPI', integer, integer)(0x30A0), 'bind GLES')
    attrs = (integer * 13)(0x3033, 1, 0x3040, 0x40, 0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8, 0x3038)
    config, count = ptr(), integer()
    check(fn('eglChooseConfig', integer, ptr, ptr, ptr, integer, ptr)(display, attrs, C.byref(config), 1, C.byref(count)), 'choose config')
    check(count.value, 'config count')
    surface = check(fn('eglCreatePbufferSurface', ptr, ptr, ptr, ptr)(display, config, (integer * 5)(0x3057, 1, 0x3056, 1, 0x3038)), 'pbuffer')
    context = check(fn('eglCreateContext', ptr, ptr, ptr, ptr, ptr)(display, config, None, (integer * 3)(0x3098, 3, 0x3038)), 'context')
    check(fn('eglMakeCurrent', integer, ptr, ptr, ptr, ptr)(display, surface, surface, context), 'make current')
    proc = fn('eglGetProcAddress', ptr, C.c_char_p)
    get_string = C.CFUNCTYPE(C.c_char_p, C.c_uint)(proc(b'glGetString'))
    result = {name: get_string(enum).decode() for name, enum in [('vendor', 0x1F00), ('renderer', 0x1F01), ('version', 0x1F02), ('shading_language', 0x8B8C)]}
    result['egl'] = f'{major.value}.{minor.value}'
    if exercise:
        result['test'] = exercise(proc)
    fn('eglMakeCurrent', integer, ptr, ptr, ptr, ptr)(display, None, None, None)
    fn('eglDestroyContext', integer, ptr, ptr)(display, context)
    fn('eglDestroySurface', integer, ptr, ptr)(display, surface)
    fn('eglTerminate', integer, ptr)(display)
    return result


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('frameworks', type=Path)
    args = parser.parse_args()
    print(json.dumps(probe(args.frameworks.resolve()), indent=2))
