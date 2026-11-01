#pragma once

// Guard chip experiments. Final design = GuardRollback: the guard takes its
// vanilla path (clang + 35-37 reaction + chip) and the HP box is pressed back
// to its pre-hit snapshot while the weapon FSM is inside {35,36,37}. The
// threshold hook stays trace-only (forcing it zeroes the damage but the
// engine queues NO reaction for a zero-damage hit - it just plays an emitter).
void InstallChipGuardHook();
void LoadChipGuardConfig();
void PollChipGuardHotkey();  // probe only
void PollGuardRollback();
