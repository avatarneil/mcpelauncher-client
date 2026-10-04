# Gamepad release regressions

Run `python3 tests/test_gamepad_neutral.py` with Python 3 and a C++17 compiler.
Set `CXX` to select a compiler. AddressSanitizer and UndefinedBehaviorSanitizer
must be supported.

The runner extracts `hasInputMode`, `onGamepadAxis`, `onGamepadButton`, `setCursorLocked`, and
`queueGamepadAxisInputIfNeeded` directly from `src/window_callbacks.cpp` and
compiles them with the recording platform doubles in
`gamepad_neutral_harness.cpp`. It tests production method bodies without
requiring the manifest dependencies or launching Minecraft. The doubles are
not an Android/JNI integration test, and the harness does not compile the
complete production translation unit.

Check the regression against an earlier source file with:

```sh
python3 tests/test_gamepad_neutral.py --source /path/to/old/window_callbacks.cpp
```

The pre-fix source fails when the cached axis remains nonzero after mouse
mode takes over and the controller sends zero. The fixed source covers all
six axes, both input delivery paths, queued versus consumed legacy events,
zero/default switch delays, repeated neutral events, startup and forced-mode
filtering, raw-input behavior, and disconnected-device queue reads.

Button-release coverage checks that a delivered A/B button press or D-pad state
is released after mouse takeover in both delivery paths, without reclaiming
gamepad mode. Redundant releases and startup/forced-mode restrictions remain
filtered. The harness currently passes 40 release/guard scenarios, plus its
filtering and invalid-device checks.

The neutral-release portion forwards an exact zero for previously nonzero axes rejected
by normal mode filtering. It does not forward new nonzero gamepad input while
mouse mode owns input, treat small nonzero values as zero, or enable mixed
controller/mouse input by itself. Controller deadzone processing and in-game
cursor/menu behavior still need integration testing.

## Experimental captured mixed input

Set `MCPELAUNCHER_CLIENT_MIXED_INPUT=1` on the patched runtime and leave
`MCPELAUNCHER_CLIENT_RAW_INPUT` unset. The option defaults off. While Minecraft
requests cursor capture, mouse and gamepad events can pass without the mode
switch delay or activation threshold. Normal mode/cursor bookkeeping still
runs. Releasing capture restores mouse mode if the last input was gamepad;
normal menu filtering resumes. Startup guards, forced modes, and touch priority
remain in effect. Raw input retains its existing precedence and behavior.

The harness checks low-amplitude analog input, small mouse motion eligibility,
repeated gameplay/menu capture transitions, cursor restoration, menu filtering,
and forced-mode/touch guards. A successful harness and full client build do not
prove that Minecraft's capture requests accurately identify every gameplay/menu
transition. In-game inventory, menus, button releases across transitions, and
UI input-mode behavior still require manual validation before recommending this
option. This candidate must not replace the installed runtime without a separate,
reversible test setup and a normal Save & Quit.
