# ReshadeEffectCounterToggler

ReShade addon that automatically hides all ReShade effects when UI screens (menu, map, dialog) are open, using a smart counter model.

Unlike a plain global toggle (like F12), this addon tracks each screen independently. Effects stay hidden as long as **any** screen is open, and are restored the moment all screens close.

## How it works

- **Groups** — each group represents one screen (e.g. "Menu", "Map"). A group is *open* when its screen is on screen.
- **Keys** — each group has a list of keys. All keys in the same group are **equivalent**: pressing any of them toggles the group's open state (each press flips it, so every second press closes it).
- **Effects hidden** — derived state: effects are hidden if **any** group is open (or manual hide is active).
- **Snapshot/Restore** — when effects are hidden, the addon snapshots every ReShade technique's state, disables them all, and restores the snapshot when shown again. This is automatic and safe — only the techniques that were actually active are re-enabled.

## Fallback keys

- **Global fallback** — one key for all groups. It closes **all** open groups, but only if at least one group is open. If no group is open, it does nothing (it is **not** a persistent toggle).
- **Per-group fallback** — optional key per group. It closes just that group, but only if that group is currently open.

Both are optional and configured in the UI.

## Persistence

Groups and fallback keys are saved to the ReShade INI config via the **Save all groups** button and are loaded automatically on startup. Nothing is hardcoded — you create groups and assign keys entirely in the UI.

## Installation

1. Build the addon (see below) to produce `ReshadeEffectCounterToggler.addon64`.
2. Copy `ReshadeEffectCounterToggler.addon64` into the ReShade addons folder of the game:
   - `%AppData%\ReShade\Addons\` (global) or the game's local `reshade-shaders\Addons\` folder.
3. Launch the game. Open the ReShade overlay (default `Home`) and find the **Effect Counter Toggler** tab.

## Usage

In the **Effect Counter Toggler** overlay tab:

- **Hide effects (manual) / Show effects (manual)** — manual test toggle.
- **Snapshot / Restore** — manually snapshot or restore all technique states.
- **Add group** — create a new group.
- **Save all groups** — persist all groups and fallback keys to the config.
- **Global fallback key** — click the field, then press the key to bind (or leave as None).
- Per group (expand its header):
  - **Fallback key** — optional per-group fallback.
  - **Keys** — add keys, click a key field then press the key to bind, `X` removes it.
  - **Delete group** — remove the group.

Keys support keyboard shortcuts (including Ctrl/Shift/Alt combos) and gamepad buttons (XInput).

## Building

Requires CMake and a C++17 compiler (MSVC recommended).

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The output is `build\Release\ReshadeEffectCounterToggler.addon64`.

## Project layout

```
ReshadeEffectCounterToggler/
├── CMakeLists.txt
├── README.md
├── include/          # ReShade SDK headers + imgui.h
└── src/
    ├── addon.cpp     # entry point: DllMain, present logic, overlay UI, shared state
    ├── constants.h   # shared constants (GP_FLAG, XInput flags, config section)
    ├── types.h       # ToggleGroup model
    ├── state.h       # shared addon state (extern globals)
    ├── input.h/.cpp  # unified input: keyboard + gamepad (XInput), key capture, key names
    ├── effects.h/.cpp# EffectGate: snapshot/restore all ReShade techniques
    └── config.h/.cpp # persistence: save/load groups to the ReShade INI
```

## Notes

- Effects are only hidden while a group is open; closing all screens restores the previous effect states automatically.
- If ReShade reloads effects while hidden, the snapshot is refreshed so restore stays correct.
