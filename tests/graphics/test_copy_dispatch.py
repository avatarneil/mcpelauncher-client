#!/usr/bin/env python3
"""Reproduce GLES 3.2 entry-point rejection on the lab's GLES 3.1 context.

Copy a changing depth/stencil image and sample every destination pixel. A stale
destination is deliberately initialized differently on every frame.
"""
import ctypes as C
import argparse
import json
from pathlib import Path
from probe_graphics import probe


def exercise(proc, bridge_library=None):
    u, i, p = C.c_uint, C.c_int, C.c_void_p
    w, h = 65, 33
    def gl(name, result, *args):
        address = proc(name.encode())
        if not address:
            raise RuntimeError('Missing ' + name)
        return C.CFUNCTYPE(result, *args)(address)
    error = gl('glGetError', u)
    def check(step):
        code = error()
        if code:
            raise RuntimeError(f'{step}: {code:#x}')
    textures = (u * 3)()
    fbos = (u * 3)()
    gl('glGenTextures', None, i, p)(3, textures)
    gl('glGenFramebuffers', None, i, p)(3, fbos)
    bind = gl('glBindFramebuffer', None, u, u)
    for index, texture in enumerate(textures):
        gl('glBindTexture', None, u, u)(0xDE1, texture)
        gl('glTexStorage2D', None, u, i, u, i, i)(0xDE1, 1, 0x88F0 if index < 2 else 0x8058, w, h)
        for param in (0x2800, 0x2801):
            gl('glTexParameteri', None, u, u, i)(0xDE1, param, 0x2600)
        bind(0x8D40, fbos[index])
        attachment = 0x821A if index < 2 else 0x8CE0
        gl('glFramebufferTexture2D', None, u, u, u, u, i)(0x8D40, attachment, 0xDE1, texture, 0)
        if index < 2:
            gl('glDrawBuffers', None, i, p)(1, (u * 1)(0))
            gl('glReadBuffer', None, u)(0)
        assert gl('glCheckFramebufferStatus', u, u)(0x8D40) == 0x8CD5
    program = gl('glCreateProgram', u)()
    sources = [(0x8B31, '#version 310 es\nvoid main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);gl_Position=vec4(p*2.0-1.0,0.0,1.0);}'),
               (0x8B30, '#version 310 es\nprecision highp float;uniform highp sampler2D depth;out vec4 color;void main(){float d=texelFetch(depth,ivec2(gl_FragCoord.xy),0).r;color=vec4(d,d,d,1.0);}')]
    for kind, source in sources:
        shader = gl('glCreateShader', u, u)(kind)
        text = C.c_char_p(source.encode())
        gl('glShaderSource', None, u, i, p, p)(shader, 1, C.byref(text), None)
        gl('glCompileShader', None, u)(shader)
        ok = i()
        gl('glGetShaderiv', None, u, u, p)(shader, 0x8B81, C.byref(ok))
        assert ok.value, 'Shader compilation failed'
        gl('glAttachShader', None, u, u)(program, shader)
    gl('glLinkProgram', None, u)(program)
    ok = i()
    gl('glGetProgramiv', None, u, u, p)(program, 0x8B82, C.byref(ok))
    assert ok.value, 'Program link failed'
    gl('glUseProgram', None, u)(program)
    gl('glViewport', None, i, i, i, i)(0, 0, w, h)
    copy_args = (u, u, i, i, i, i, u, u, i, i, i, i, i, i, i)
    copy_values = (textures[0], 0xDE1, 0, 0, 0, 0, textures[1], 0xDE1, 0, 0, 0, 0, w, h, 1)
    check('setup')
    gl('glCopyImageSubData', None, *copy_args)(*copy_values)
    core_error = error()
    assert core_error == 0x502, f'Expected GLES 3.2 core rejection, got {core_error:#x}'
    pixels = (C.c_ubyte * (w * h * 4))()
    results = []
    entries = [('glCopyImageSubDataEXT', gl('glCopyImageSubDataEXT', None, *copy_args), True),
               ('glCopyImageSubDataOES', gl('glCopyImageSubDataOES', None, *copy_args), True)]
    if bridge_library:
        shim = C.CDLL(str(bridge_library))
        shim.bridge.argtypes, shim.bridge.restype = (C.c_char_p, p), p
        address = shim.bridge(b'copy', proc(b'glCopyImageSubDataEXT'))
        android_call = C.CFUNCTYPE(None, *([C.c_uint64] * 15))
        entries.append(('Android ABI without bridge (negative control)', android_call(proc(b'glCopyImageSubDataEXT')), False))
        entries.append(('Android ABI through production bridge', android_call(address), True))
    for entry, copy, copies_expected in entries:
        for frame in range(4):
            value = (frame + 1) / 8
            for index, depth in [(0, value), (1, 0.875)]:
                bind(0x8D40, fbos[index])
                gl('glClearBufferfi', None, u, i, C.c_float, i)(0x84F9, 0, depth, frame + index)
            copy(*copy_values)
            check(entry)
            bind(0x8D40, fbos[2])
            gl('glBindTexture', None, u, u)(0xDE1, textures[1])
            gl('glDrawArrays', None, u, i, i)(4, 0, 3)
            gl('glReadPixels', None, i, i, i, i, u, u, p)(0, 0, w, h, 0x1908, 0x1401, pixels)
            check('sample copied depth')
            expected = round((value if copies_expected else 0.875) * 255)
            assert all(abs(pixels[k] - expected) <= 1 for k in range(0, len(pixels), 4)), entry
        results.append({'entry': entry, 'frames': 4, 'depth_pixels_checked': w * h * 4,
                        'result': 'current depth copied' if copies_expected else 'stale destination reproduced'})
    return {'core_error': hex(core_error), 'extensions': results, 'passed': True}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('frameworks', type=Path, help='Directory containing matching ANGLE libraries and MoltenVK_icd.json')
    parser.add_argument('--bridge-library', type=Path)
    args = parser.parse_args()
    frameworks = args.frameworks.resolve()
    print(json.dumps(probe(frameworks, lambda proc: exercise(proc, args.bridge_library)), indent=2))
