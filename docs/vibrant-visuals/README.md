# Opt-in Vibrant Visuals on Apple Silicon

This fork contains the client changes validated with Android Bedrock 1.26.52.3,
ANGLE 2.1.44625 (`f32fc54c74d9`), and a patched MoltenVK 1.4.1 on an Apple M5 Pro.
The normal macOS fallback to Fancy remains the default.

Build this client through the usual mcpelauncher manifest/dependencies. Preserve
`USE_OWN_CURL=ON` and `XAL_WEBVIEW_USE_QT=ON` when using the tested networking path.
Use a separate launcher profile and test world for initial validation.

## Required pieces

1. Use this client branch. The Android/Darwin ARM64 bridge corrects spilled
   arguments for image copy, framebuffer blit and 3D texture uploads.
2. Use the matching graphics library from
   [avatarneil/MoltenVK, fix/vibrant-visuals-compat](https://github.com/avatarneil/MoltenVK/tree/fix/vibrant-visuals-compat).
   The ABI bridge does not replace its independent shader-output compatibility fixes.
3. Set these options in the environment used to start the client:

   ```sh
   export MCPELAUNCHER_VIBRANT_VISUALS=1
   export MCPELAUNCHER_VIBRANT_ARM64_BRIDGE=1
   export MCPELAUNCHER_VIBRANT_PRESERVE_UINT=1
   ```

4. Register the **actual** `glGetString(GL_RENDERER)` value at integer GPU tier 4
   in a separate game installation's base64-encoded JSON `assets/assets/tiers.bin`.
   Match the installed driver instead of copying a renderer identifier from another
   machine or build. The observed local renderer changed between driver builds.
5. Select Vibrant Visuals in the game. The stock tier-4 upscaling configuration
   was retained; no forced ANGLE feature overrides or modified game shaders are needed.

The repository contains no Minecraft binaries, game assets, saves, credentials or
prebuilt driver. The opt-in flag is not a claim that stock MoltenVK or every game
version/GPU is supported. Full driver packaging, game eligibility and the selected
graphics mode must agree.

## Controller integration

`fix/macos-vibrant-visuals` contains only graphics work on the upstream client base.
`integration/macos-vibrant-controller` combines it with the previously tested
neutral/button-release fixes and opt-in captured mixed input. For that integration,
set `MCPELAUNCHER_CLIENT_MIXED_INPUT=1` and leave `MCPELAUNCHER_CLIENT_RAW_INPUT`
unset. Controller mappings are outside this change.

## Validation

Run the [headless ABI/depth regressions](../../tests/graphics/README.md).
They reproduce corrupt arguments/stale depth without the bridge, and pass through
the same bridge template used by the client. On the tested installation, pale
camera-following patches disappeared and water rendered after full process restarts,
movement, window resizing and world save/reload. All tests used a disposable local world.

Long play sessions, intermittent flicker, physical controller integration and Realms
still need hands-on acceptance. The observed nominal frame-rate cap was not enforced.
The local runtime had these graphics source changes alongside the controller fixes;
the graphics-only branch has not separately undergone full-game acceptance.
