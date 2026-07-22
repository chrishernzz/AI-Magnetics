"""
requirement_parser.py

Natural-language -> InductorDesignRequest extraction, built so the LLM can
only ever translate, never invent:

1. The LM Studio call uses schema-constrained JSON output (every field
   nullable) - the model structurally cannot return prose, extra fields,
   or a value for something the sentence didn't state (rule: unstated
   fields are null).
2. Everything after that is deterministic Python: validate_extraction()
   applies the physics sanity checks (Irms <= Ipk, plausible ranges) and
   builds the clarifying questions for missing fields, with the reason each
   question matters - mirroring knowledge/input-interview-guide.md.
3. The caller (frontend) shows the extracted values in the normal form for
   the engineer to confirm/edit - the parse never triggers the design
   engine directly.

LM Studio only exists on the local machine (127.0.0.1:1234), so this whole
feature degrades gracefully: parse_requirements() raises
LmStudioUnavailable when the server isn't reachable, and the route turns
that into a 503 the frontend explains ("fill the form manually").
"""

from __future__ import annotations

import json
from typing import Optional

import requests

LM_STUDIO_BASE_URL = "http://127.0.0.1:1234/v1"
REQUEST_TIMEOUT_SECONDS = 900  # big models on CPU are slow; a timeout that kills a working parse is worse than waiting

# Every field nullable on purpose: null is the model's only honest way to say
# "the sentence didn't state this," and the schema makes that the default path.
EXTRACTION_SCHEMA = {
    "name": "inductor_requirements",
    "strict": True,
    "schema": {
        "type": "object",
        "properties": {
            "inductanceUH": {"type": ["number", "null"], "description": "Target inductance in microhenries (convert mH/nH/H to uH)"},
            "peakCurrentA": {"type": ["number", "null"], "description": "Peak current in amps"},
            "rmsCurrentA": {"type": ["number", "null"], "description": "RMS current in amps"},
            "averageCurrentA": {"type": ["number", "null"], "description": "Average/DC current in amps"},
            "rippleCurrentPeakToPeakA": {"type": ["number", "null"], "description": "Peak-to-peak ripple current in amps"},
            "switchingFreqKHz": {"type": ["number", "null"], "description": "Switching frequency in kHz (convert Hz/MHz to kHz)"},
            "ambientTemperatureC": {"type": ["number", "null"], "description": "Ambient temperature in Celsius"},
            "allowableTempRiseC": {"type": ["number", "null"], "description": "Allowed temperature rise in Celsius"},
            "inductanceTolerancePercent": {"type": ["number", "null"], "description": "Inductance tolerance in percent"},
        },
        "required": ["inductanceUH", "peakCurrentA", "rmsCurrentA", "averageCurrentA",
                      "rippleCurrentPeakToPeakA", "switchingFreqKHz", "ambientTemperatureC",
                      "allowableTempRiseC", "inductanceTolerancePercent"],
        "additionalProperties": False,
    },
}

SYSTEM_PROMPT = """You extract power-inductor design requirements from an engineer's sentence into JSON.

Rules:
- Extract ONLY what the sentence actually states. Any quantity not stated is null. Never estimate, never fill in typical values.
- Normalize units: inductance to microhenries (0.5 mH -> 500), frequency to kHz (100000 Hz -> 100, 1 MHz -> 1000), current to amps, temperature to Celsius.
- "peak" current -> peakCurrentA. "RMS" -> rmsCurrentA. "average"/"DC" current -> averageCurrentA. "ripple" (peak-to-peak) -> rippleCurrentPeakToPeakA. Do NOT assign a bare "current" with no qualifier to any of these - leave all null if unqualified.
- A bare inductance number with no unit in a power-converter context means microhenries.

Examples:
Sentence: "I need a 470 uH inductor, 1.5 A peak, 1 A RMS, ripple is 0.3 A, switching at 80 kHz"
JSON: {"inductanceUH": 470, "peakCurrentA": 1.5, "rmsCurrentA": 1.0, "averageCurrentA": null, "rippleCurrentPeakToPeakA": 0.3, "switchingFreqKHz": 80, "ambientTemperatureC": null, "allowableTempRiseC": null, "inductanceTolerancePercent": null}

Sentence: "0.25 mH choke for a 100k buck, about 3 amps average with half an amp of ripple, 40 degree rise max in a 50C box"
JSON: {"inductanceUH": 250, "peakCurrentA": null, "rmsCurrentA": null, "averageCurrentA": 3.0, "rippleCurrentPeakToPeakA": 0.5, "switchingFreqKHz": 100, "ambientTemperatureC": 50, "allowableTempRiseC": 40, "inductanceTolerancePercent": null}

Sentence: "Need an inductor around 10 amps for my converter"
JSON: {"inductanceUH": null, "peakCurrentA": null, "rmsCurrentA": null, "averageCurrentA": null, "rippleCurrentPeakToPeakA": null, "switchingFreqKHz": null, "ambientTemperatureC": null, "allowableTempRiseC": null, "inductanceTolerancePercent": null}"""


class LmStudioUnavailable(RuntimeError):
    pass


def extract_requirements(text: str, chat_model: str = "google/gemma-4-12b-qat",
                          base_url: str = LM_STUDIO_BASE_URL) -> dict:
    """Schema-constrained extraction. Returns the raw field dict (values or None)."""
    payload = {
        "model": chat_model,
        "messages": [
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": f"Sentence: {text}\nJSON:"},
        ],
        "temperature": 0.1,
        "max_tokens": 500,
        "response_format": {"type": "json_schema", "json_schema": EXTRACTION_SCHEMA},
    }
    try:
        response = requests.post(f"{base_url}/chat/completions", json=payload,
                                  timeout=REQUEST_TIMEOUT_SECONDS)
    except requests.exceptions.ConnectionError as exc:
        raise LmStudioUnavailable(
            "LM Studio is not reachable at 127.0.0.1:1234 - natural-language input "
            "only works when the app runs on a machine with LM Studio's local server running. "
            "Fill in the form manually instead."
        ) from exc
    response.raise_for_status()
    content = response.json()["choices"][0]["message"]["content"]
    return json.loads(content)


# ---------------------------------------------------------------------------
# Deterministic validation - no AI from here down.
# ---------------------------------------------------------------------------

# (field, question, why-it-matters) - the interview, mirroring
# knowledge/input-interview-guide.md. Only asked when the field is missing
# and not otherwise derivable.
REQUIRED_FIELD_QUESTIONS = [
    ("inductanceUH", "What inductance do you need (µH)?",
      "Everything starts here - energy storage, core size, and turns all derive from L."),
    ("peakCurrentA", "What is the worst-case peak current (A), including startup/transients?",
      "Peak current drives core size and the saturation check - flux follows the instantaneous current."),
    ("switchingFreqKHz", "What switching frequency (kHz)?",
      "Determines which core materials are usable and how big the core loss is."),
]


def validate_extraction(fields: dict) -> dict:
    """
    Pure-Python referee. Returns:
      errors    - physically impossible / blocking (do not run the engine)
      questions - missing information, each with the reason it's needed
      warnings  - suspicious but not blocking
      assumedDefaults - fields the form will fill with a labeled default
    """
    errors: list[str] = []
    questions: list[dict] = []
    warnings: list[str] = []
    assumed: list[str] = []

    def val(name) -> Optional[float]:
        v = fields.get(name)
        return float(v) if v is not None else None

    l_uh = val("inductanceUH")
    ipk = val("peakCurrentA")
    irms = val("rmsCurrentA")
    iavg = val("averageCurrentA")
    ripple = val("rippleCurrentPeakToPeakA")
    f_khz = val("switchingFreqKHz")
    tol = val("inductanceTolerancePercent")

    # Blocking impossibilities
    for name, v in [("inductance", l_uh), ("peak current", ipk), ("RMS current", irms),
                     ("average current", iavg), ("ripple current", ripple), ("frequency", f_khz)]:
        if v is not None and v <= 0:
            errors.append(f"{name} must be positive (got {v}).")
    if irms is not None and ipk is not None and irms > ipk:
        errors.append(
            f"RMS current ({irms} A) is greater than peak current ({ipk} A) - physically "
            "impossible; the average heating effect cannot exceed the instantaneous maximum. "
            "Probably a typo - please check both numbers.")

    # Missing required fields -> interview questions
    for field, question, why in REQUIRED_FIELD_QUESTIONS:
        if val(field) is None:
            questions.append({"field": field, "question": question, "why": why})

    # RMS: required, but derivable from average + ripple (triangular assumption)
    if irms is None:
        if iavg is not None and ripple is not None:
            warnings.append(
                "RMS current not stated - it will be derived from average current and ripple "
                "assuming a triangular ripple waveform (Irms = sqrt(Iavg² + ΔI²/12)).")
        else:
            questions.append({
                "field": "rmsCurrentA",
                "question": "What is the RMS current (A)? (Or give average current + ripple and it can be derived.)",
                "why": "RMS current sizes the wire and sets copper loss - it can never be safely guessed from peak current.",
            })

    # Ripple: optional but consequential
    if ripple is None:
        questions.append({
            "field": "rippleCurrentPeakToPeakA",
            "question": "What peak-to-peak ripple current does the converter design assume (A)? (Optional)",
            "why": "Core loss depends on the flux swing the ripple causes - without it, core loss is reported not-evaluated rather than guessed.",
        })

    # Plausibility warnings (non-blocking)
    if ripple is not None and ipk is not None and ripple > 2 * ipk:
        warnings.append(f"Ripple ({ripple} A p-p) exceeds twice the peak current ({ipk} A) - unusual; double-check.")
    if f_khz is not None and not (1 <= f_khz <= 5000):
        warnings.append(f"Switching frequency {f_khz} kHz is outside the typical 1-5000 kHz power-converter range - double-check units.")
    if l_uh is not None and not (0.01 <= l_uh <= 100000):
        warnings.append(f"Inductance {l_uh} µH is outside the typical power-inductor range - double-check units.")
    if tol is not None and not (0.5 <= tol <= 50):
        warnings.append(f"Tolerance {tol}% is unusual (typical 1-20%).")

    # Labeled defaults for the two temperature fields
    if val("ambientTemperatureC") is None:
        assumed.append("ambientTemperatureC: using the form default (25 °C) - adjust if your environment differs.")
    if val("allowableTempRiseC") is None:
        assumed.append("allowableTempRiseC: using the form default (40 °C) - adjust if your thermal budget differs.")

    return {"errors": errors, "questions": questions, "warnings": warnings, "assumedDefaults": assumed}
