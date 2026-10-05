# Apple ARM64 GLES argument regression

Run on Apple Silicon macOS with Python 3 and Clang. This exercises the production
bridge without launching Minecraft or accessing game files or accounts.

From the repository root:

```sh
clang++ -std=c++17 -O2 -Wall -Wextra -Werror -dynamiclib -I src \
  tests/graphics/arm64_gles_bridge_probe.cpp -o /tmp/gles-arm64-bridge.dylib
python3 tests/graphics/test_arm64_bridge.py /tmp/gles-arm64-bridge.dylib
```

The test checks copy, blit, ordinary 3D upload and compressed 3D upload signatures.
It sends AAPCS64-shaped arguments to native recording callbacks, including signed
values and poisoned upper bits. All 12 bridge cases must preserve the arguments;
all 12 direct native calls must reproduce a mismatch. It does not dereference the
intentionally invalid pointer values in the negative controls.

For an actual GPU depth-copy/readback regression, supply a directory containing
matching `libEGL.dylib`, `libGLESv2.dylib`, `libMoltenVK.dylib` and
`MoltenVK_icd.json` from an ANGLE Vulkan build:

```sh
python3 tests/graphics/test_copy_dispatch.py /path/to/mvk-angle \
  --bridge-library /tmp/gles-arm64-bridge.dylib
```

This uses a headless GLES context and four changing 65 x 33 depth images. Native
EXT/OES calls and the bridged Android call must copy current depth. The unbridged
negative control must retain stale destination depth. It checks depth through
sampling, not stencil pixels. The tested ANGLE exposes GLES 3.1: core image-copy
rejection is expected; the game uses the EXT entry point.

The bridge is deliberately restricted to integer/pointer signatures. Floating
point, aggregates, variadics and callbacks in the other direction need separate
ABI treatment. These tests are not a general ARM64 ABI conformance suite.
