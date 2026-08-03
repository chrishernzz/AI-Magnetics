#pragma once

//CCM: minimum inductor current stays comfortably above zero across the
//switching cycle - the standard operating mode this engine's buck
//formulas assume.
//CCMBoundary: minimum inductor current is at or near zero - still
//technically continuous, but small load or line variation could push
//the converter into DCM. Surfaced as a warning, not a rejection.
//DCMUnsupported: minimum inductor current is negative under the ideal
//CCM formula, meaning the real converter would already be in
//discontinuous conduction mode - a different (unimplemented) set of
//equations would be needed, so this is rejected rather than silently
//sized with formulas that no longer apply.
enum class ConductionMode { CCM, CCMBoundary, DCMUnsupported };
