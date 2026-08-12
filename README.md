# ReshadeEffectCounterToggler

A ReShade addon that hides all ReShade effects while UI screens (menu, map, dialog) are open, and restores them automatically when the screens close.

Unlike a plain global toggle, it tracks each screen independently: effects stay hidden while **any** screen is open and are restored the moment all screens close.

## Features

- **Groups** — each group represents one screen (e.g. "Menu", "Map"). A group is *open* when its screen is on screen.
- **Keys** — each group has keys. Pressing any key in a group toggles that group's open state.
- **Fallback keys** — a global fallback closes all open groups; an optional per-group fallback closes just that group. Fallbacks only act when the group is open (not a persistent toggle).
- **Smart snapshot/restore** — when effects are hidden, the addon snapshots every technique's state, disables them, and restores the exact previous state when shown again.
- **Persistence** — groups and keys are saved to the ReShade INI and loaded automatically on startup.
- **Keyboard + gamepad** — supports keyboard shortcuts (Ctrl/Shift/Alt combos) and gamepad buttons. Gamepad toggles on a quick click (press + release), not on hold.

## Installation

1. Copy `ReshadeEffectCounterToggler.addon64` into the game folder (next to `dxgi.dll` / `d3d11.dll`).
2. Launch the game, open the ReShade overlay (default `Home`), and go to the **Effect Counter Toggler** tab.
3. Create groups, assign keys, and click **Save all groups**.

## Usage example (Elder Scrolls Online)

| Group | Keys | Purpose |
|-------|------|---------|
| **Map** | `M` | Toggle the map (effects hide while open). |
| **Menu** | `Esc` | Toggle the menu (effects hide while open). |
| **Global fallback** | `Alt` | Universal "back" — closes all open groups, restoring effects. |

Setup: add a `Map` group with key `M`, a `Menu` group with key `Esc`, set the global fallback key to `Alt`, then **Save all groups**.

## Building

Requires CMake and a C++17 compiler (MSVC recommended).

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The output is `build\Release\ReshadeEffectCounterToggler.addon64`.
