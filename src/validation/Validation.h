#pragma once

// KEPT — same purpose as before (checking results against physical
// limits), just re-pointed at buck-inductor quantities instead of
// flyback ones: flux density vs. Bsat, window fill factor, and now
// also the allowable temperature rise your boss specified as an input.

struct ValidationResult {
    bool passed;
    // TODO: list of violated checks, if any
};
