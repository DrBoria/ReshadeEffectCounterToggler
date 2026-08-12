#pragma once

// EffectGate: snapshot and restore the state of all ReShade techniques.
// Effects are hidden while any group is open (or manual hide is active).

void snapshot_effects();
void apply_effects(bool enabled);
void hide_effects();
void show_effects();
void update_effects_hidden();
