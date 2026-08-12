# ReshadeEffectCounterToggler

ReShade addon that automatically hides all ReShade effects when UI screens (menu, map, dialog) are open, using a smart counter model.

Unlike a plain global toggle (like F12), this addon tracks each screen independently. Effects stay hidden as long as **any** screen is open, and are restored the moment all screens close.

## How it works

- **Groups** — each group represents one screen (e.g. "Menu", "Map"). A group is *open* when its screen is on screen.
- **Keys** — each group has a list of keys. Pressing a key toggles that key's bit.
- **Effects hidden** — derived state: effects are hidden if **any** group is open (or manual hide is active).
- **Snapshot/Restore** — when effects are hidden, the addon snapshots every ReShade technique's state, disables them all, and restores the snapshot when shown again. This is automatic and safe.

## Group modes

| Mode | Behavior |
|------|----------|
| **Switch** | Any key in the group toggles the whole group. Each second press closes it. |
| **Screens** | Each key is its own bit-screen. The group is open if **any** bit is open. (M opens map, ESC opens menu, M again closes map but menu still open → effects stay hidden.) |

## Fallback keys

- **Global fallback** — one key for all groups; toggles every group open/closed at once.
- **Per-group fallback** — optional key per group; toggles just that group.

Both are optional and configured in the UI.

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
- **Global fallback key** — click the button, then press the key to bind (or leave as None).
- Per group:
  - **Mode** — Switch or Screens.
  - **Fallback key** — optional per-group fallback.
  - **Keys** — add keys, click a key button then press the key to bind, `X` removes it.
  - **Delete group** — remove the group.

Keys are fully dynamic — nothing is hardcoded. You create groups and assign keys entirely in the UI.

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
    └── addon.cpp     # addon implementation
```

## Notes

- Effects are only hidden while a group is open; closing all screens restores the previous effect states automatically.
- If ReShade reloads effects while hidden, the snapshot is refreshed so restore stays correct.
