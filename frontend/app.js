const ENDPOINT = "/inductor-design";
const TOPOLOGY_ENDPOINT = "/topology-design/buck";

let lastResult = null;
let currentShapeFilter = "all"; // all | Toroid | TwoPieceSet
let currentMaterialTypeFilter = "all"; // all | ferrite | powder

// Set when Mode 1 (Buck converter) has derived requirements - holds the
// averageCurrentA/rippleCurrentPeakToPeakA pair so buildPayload() sends
// those instead of a directly-entered rmsCurrentA. null in Mode 2.
let topologyDerived = null;

function buildPayload() {
    const toleranceRaw = document.getElementById("tolerance").value;
    const rippleRaw = document.getElementById("rippleCurrent").value;
    const peakRaw = document.getElementById("current").value;

    const payload = {
        inductanceUH: Number(document.getElementById("inductance").value),
        switchingFreqKHz: Number(document.getElementById("frequency").value),
        ambientTemperatureC: Number(document.getElementById("ambientTemp").value),
        allowableTempRiseC: Number(document.getElementById("tempRise").value),
    };

    if (peakRaw !== "") {
        // Optional - omitted entirely (not sent as 0) when left blank, so the
        // backend skips the area-product pre-filter and reports
        // PeakFluxValidation/SaturationValidation as not-evaluated instead of
        // ever inferring a peak from RMS.
        payload.peakCurrentA = Number(peakRaw);
    }
    if (toleranceRaw !== "") {
        payload.inductanceTolerancePercent = Number(toleranceRaw);
    }
    if (rippleRaw !== "") {
        payload.rippleCurrentPeakToPeakA = Number(rippleRaw);
    }

    if (topologyDerived) {
        // Derived from a Buck converter (Mode 1) - averageCurrentA + ripple
        // feed the same triangular-ripple RMS derivation Mode 2 already
        // relies on, rather than a separately-entered RMS value.
        payload.averageCurrentA = topologyDerived.averageCurrentA;
    } else {
        payload.rmsCurrentA = Number(document.getElementById("rmsCurrent").value);
    }

    return payload;
}

function buildTopologyPayload() {
    const toleranceRaw = document.getElementById("buckTolerance").value;

    const payload = {
        vinMinV: Number(document.getElementById("buckVinMin").value),
        vinMaxV: Number(document.getElementById("buckVinMax").value),
        voutV: Number(document.getElementById("buckVout").value),
        ioutA: Number(document.getElementById("buckIout").value),
        switchingFreqKHz: Number(document.getElementById("buckFrequency").value),
        rippleCurrentPercent: Number(document.getElementById("buckRipplePercent").value),
        ambientTemperatureC: Number(document.getElementById("buckAmbientTemp").value),
        allowableTempRiseC: Number(document.getElementById("buckTempRise").value),
    };

    if (toleranceRaw !== "") {
        payload.inductanceTolerancePercent = Number(toleranceRaw);
    }

    return payload;
}

function switchMode(mode) {
    const buckSection = document.getElementById("buckModeSection");
    const directSection = document.getElementById("directModeSection");
    const buckBtn = document.getElementById("modeBuckBtn");
    const directBtn = document.getElementById("modeDirectBtn");
    if (!buckSection || !directSection || !buckBtn || !directBtn) return;

    const showBuck = mode === "buck";
    buckSection.hidden = !showBuck;
    directSection.hidden = showBuck;
    buckBtn.classList.toggle("active", showBuck);
    buckBtn.setAttribute("aria-selected", String(showBuck));
    directBtn.classList.toggle("active", !showBuck);
    directBtn.setAttribute("aria-selected", String(!showBuck));

    const buckDiagnostics = document.getElementById("buckDiagnostics");
    const directDiagnostics = document.getElementById("directDiagnostics");
    if (buckDiagnostics) buckDiagnostics.hidden = !showBuck;
    if (directDiagnostics) directDiagnostics.hidden = showBuck;

    if (showBuck) {
        updateBuckDiagnosticsLive();
    } else {
        updateDirectDiagnosticsLive();
    }
}

// Splits a mode's fields into sub-tabs (e.g. "Buck Converter" vs. "Thermal
// & Tolerance") so only one group's inputs are visible at a time instead of
// every field stacked in one long column - purely a display split, every
// field keeps its id and still submits normally regardless of which tab is
// showing.
function initFieldTabs(container) {
    const tabs = container.querySelectorAll(".field-tab-btn");
    const panels = container.parentElement.querySelectorAll(".field-tab-panel");
    tabs.forEach((btn) => {
        btn.addEventListener("click", () => {
            const targetId = btn.dataset.tabTarget;
            tabs.forEach((other) => {
                const isActive = other === btn;
                other.classList.toggle("active", isActive);
                other.setAttribute("aria-selected", String(isActive));
            });
            panels.forEach((panel) => {
                panel.hidden = panel.id !== targetId;
            });
        });
    });
}

// Simple debounce - live diagnostics call the real backend on every
// keystroke, so this keeps that to one request per pause in typing
// instead of one per character.
function debounce(fn, delayMs) {
    let timer = null;
    return (...args) => {
        clearTimeout(timer);
        timer = setTimeout(() => fn(...args), delayMs);
    };
}

function setDiagnosticsRowPending(rowId, isPending) {
    const row = document.getElementById(rowId)?.closest(".diagnostics-row");
    if (row) row.classList.toggle("is-pending", isPending);
}

// Briefly highlights a diagnostics value when it actually changes, so a
// live recalculation reads as "this updated" instead of a number silently
// jumping. A no-op (no flash) when the new text matches the old one.
function setDiagValue(id, text) {
    const el = document.getElementById(id);
    if (!el) return;
    if (el.textContent === text) return;
    el.textContent = text;
    el.classList.remove("value-flash");
    void el.offsetWidth; // restart the CSS animation
    el.classList.add("value-flash");
}

// Draws the inductor current over two switching periods as a real triangle
// wave - rises for the duty-cycle fraction of the period, falls for the
// rest - from the exact same peak/ripple/duty-cycle numbers already shown
// as text above it. Not decorative: change any Buck input and this redraws
// from the live derived values, same as the text rows next to it.
// Real chart, not an image: the axis frame (the two axis lines, all
// ticks, and the "0 / T / 2T" time labels) is fixed geometry set once in
// the HTML, since the plot is always auto-scaled to fill it - peak always
// lands on the top axis line, min always on the bottom one. Only the
// trace, its filled area, the peak marker, and the two amplitude label
// strings are computed here from the live derived values and written
// onto the same persistent elements (not rebuilt), so the shape morphs
// via the CSS transition on `d` instead of snapping between two states.
function updateRippleWaveform(derived) {
    const svg = document.getElementById("rippleWaveform");
    if (!svg) return;

    const GUTTER = 42; // left margin reserved for the y-axis + its labels
    const PLOT_RIGHT = 302;
    const Y_TOP = 14; // peak axis line - always where ipk is plotted
    const Y_BOTTOM = 72; // min axis line / x-axis baseline - always where iMin is plotted
    const ipk = derived.peakCurrentA;
    const ripple = Math.max(derived.rippleCurrentPeakToPeakA, 1e-9);
    const iMin = ipk - ripple;
    const duty = Math.min(Math.max(derived.dutyCycle, 0.02), 0.98);
    const periods = 2;
    const periodW = (PLOT_RIGHT - GUTTER) / periods;

    let points = [[GUTTER, Y_BOTTOM]];
    for (let p = 0; p < periods; p++) {
        const x0 = GUTTER + p * periodW;
        points.push([x0 + duty * periodW, Y_TOP]);
        points.push([x0 + periodW, Y_BOTTOM]);
    }
    const pathD = "M " + points.map(([x, y]) => `${x.toFixed(1)},${y.toFixed(1)}`).join(" L ");
    // Path already starts and ends on the baseline, so closing it directly
    // encloses the area under the trace with no extra corner points needed.
    const fillD = `${pathD} Z`;
    const lastPeak = points[points.length - 2];

    setWaveformAttrs("waveformFill", { d: fillD });
    setWaveformAttrs("waveformPath", { d: pathD });
    setWaveformAttrs("waveformMarker", { cx: lastPeak[0].toFixed(1), cy: lastPeak[1].toFixed(1) });

    const topLabel = document.getElementById("waveformLabelTop");
    if (topLabel) topLabel.textContent = `${ipk.toFixed(2)} A`;
    const bottomLabel = document.getElementById("waveformLabelBottom");
    if (bottomLabel) bottomLabel.textContent = `${iMin.toFixed(2)} A`;
}

function setWaveformAttrs(id, attrs) {
    const el = document.getElementById(id);
    if (!el) return;
    Object.entries(attrs).forEach(([key, value]) => el.setAttribute(key, value));
}

async function updateBuckDiagnosticsLive() {
    const note = document.getElementById("diagBuckNote");
    const rowIds = ["diagDutyCycle", "diagRippleA", "diagPeakCurrent", "diagAvgCurrent", "diagInductance"];

    let payload;
    try {
        payload = buildTopologyPayload();
    } catch (error) {
        return; // fields not all numeric yet mid-typing - leave last known values showing
    }

    try {
        const derived = await postRequest(TOPOLOGY_ENDPOINT, payload);
        // Sized at Vin Maximum - the worst case for ripple current (see
        // BuckElectricalSolver.cpp), so that's the operating point shown here.
        setDiagValue("diagDutyCycle", `${(derived.atVinMax.dutyCycle * 100).toFixed(1)}%`);
        setDiagValue("diagRippleA", `${derived.atVinMax.rippleCurrentPeakToPeakA.toFixed(3)} A`);
        setDiagValue("diagPeakCurrent", `${derived.atVinMax.peakCurrentA.toFixed(3)} A`);
        setDiagValue("diagAvgCurrent", `${derived.request.averageCurrentA.toFixed(3)} A`);
        setDiagValue("diagInductance", `${derived.request.inductanceUH.toFixed(3)} µH`);

        rowIds.forEach((id) => setDiagnosticsRowPending(id, false));
        updateRippleWaveform(derived.atVinMax);

        if (note) {
            note.textContent = derived.warnings && derived.warnings.length ? derived.warnings.join(" ") : "";
            note.className = derived.warnings && derived.warnings.length ? "diagnostics-note diagnostics-note-warn" : "diagnostics-note";
        }
    } catch (error) {
        // Expected constantly while typing (e.g. Vout momentarily blank, or
        // temporarily >= Vin Max) - a quiet pending state, not a red error,
        // since this fires on every keystroke rather than an explicit submit.
        rowIds.forEach((id) => setDiagnosticsRowPending(id, true));
        if (note) {
            note.textContent = "Waiting for valid values - " + error.message;
            note.className = "diagnostics-note";
        }
    }
}

function updateDirectDiagnosticsLive() {
    const inductanceUH = Number(document.getElementById("inductance").value);
    const peakA = Number(document.getElementById("current").value);
    const rmsA = Number(document.getElementById("rmsCurrent").value);
    const rippleRaw = document.getElementById("rippleCurrent").value;

    const storedEnergyNote = document.getElementById("diagStoredEnergyNote");
    if (document.getElementById("diagStoredEnergy")) {
        if (inductanceUH > 0 && peakA > 0) {
            // E = 0.5 x L x I^2 - the same headline formula already shown in
            // the Inductance field's hint tooltip, just kept live here too.
            const energyMJ = 0.5 * (inductanceUH * 1e-6) * peakA * peakA * 1e3;
            setDiagValue("diagStoredEnergy", `${energyMJ.toFixed(4)} mJ`);
            if (storedEnergyNote) {
                storedEnergyNote.textContent = "";
                storedEnergyNote.className = "diagnostics-note";
            }
        } else if (inductanceUH > 0 && rmsA > 0) {
            // No real peak current yet - rather than leave this row blank (the
            // Diagnostics panel reading as empty for the RMS-only case is exactly
            // the gap this exists to close), show a labeled ESTIMATE using RMS
            // current as a proxy. This is never presented as the real figure: RMS
            // is always <= real peak for any unidirectional inductor-current
            // waveform, so E=0.5*L*Irms^2 is a genuine LOWER BOUND, not a guess -
            // same "guaranteed floor, never a substitute" logic as the backend's
            // RMS-floor saturation check (DesignValidation.cpp), applied here to
            // this one live, client-side, non-blocking sanity number.
            const energyMJ = 0.5 * (inductanceUH * 1e-6) * rmsA * rmsA * 1e3;
            setDiagValue("diagStoredEnergy", `${energyMJ.toFixed(4)} mJ (est.)`);
            if (storedEnergyNote) {
                storedEnergyNote.textContent = `Peak current not supplied - this is a lower-bound ESTIMATE using RMS current (${rmsA} A) as a proxy for peak, not the real figure. Actual stored energy is likely higher (real peak current is always >= RMS) - recompute once peak current is known.`;
                storedEnergyNote.className = "diagnostics-note diagnostics-note-warn";
            }
        } else {
            setDiagValue("diagStoredEnergy", "–");
            if (storedEnergyNote) {
                storedEnergyNote.textContent = "";
                storedEnergyNote.className = "diagnostics-note";
            }
        }
    }

    const currentNote = document.getElementById("diagCurrentNote");
    if (currentNote) {
        const ripple = rippleRaw === "" ? null : Number(rippleRaw);
        const minCurrentA = ripple !== null ? peakA - ripple : null;
        const kZeroEpsilonA = 1e-6;
        const rmsOutOfEnvelope =
            ripple !== null &&
            peakA > 0 &&
            rmsA > 0 &&
            (rmsA > peakA + kZeroEpsilonA || rmsA < minCurrentA - kZeroEpsilonA);

        if (peakA > 0 && rmsA > 0 && rmsA > peakA) {
            currentNote.textContent = "RMS current is higher than peak current - that can't happen physically, double check these values. This blocks Generate.";
            currentNote.className = "diagnostics-note diagnostics-note-warn";
        } else if (ripple === 0 && rmsOutOfEnvelope) {
            // The single most common way to trip the RMS/peak/ripple consistency
            // check: typing 0 into Ripple Current meaning "I don't have this data,"
            // when 0 actually asserts "current is perfectly constant" - a much
            // stronger claim that forces RMS to exactly equal peak. Non-blocking:
            // the design still generates, with CurrentConsistencyValidation
            // reporting not-evaluated for this candidate.
            currentNote.textContent = `Ripple Current is set to 0 (constant/DC current) - that requires RMS to exactly equal Peak (${peakA} A), but RMS is ${rmsA} A. This won't block Generate (the consistency check will report not-evaluated), but if you don't have real ripple data, leave Ripple Current blank instead - blank skips the check, 0 asserts zero ripple.`;
            currentNote.className = "diagnostics-note diagnostics-note-warn";
        } else if (rmsOutOfEnvelope) {
            currentNote.textContent = `Supplied RMS current (${rmsA} A) is physically inconsistent with peak current and ripple - RMS can never fall outside [${minCurrentA.toFixed(3)}, ${peakA}] A for this combination. This won't block Generate; CurrentConsistencyValidation will report not-evaluated for this candidate.`;
            currentNote.className = "diagnostics-note diagnostics-note-warn";
        } else {
            currentNote.textContent = "";
            currentNote.className = "diagnostics-note";
        }
    }

    const coreLossNote = document.getElementById("diagCoreLossNote");
    if (coreLossNote) {
        if (rippleRaw === "") {
            coreLossNote.textContent = "No ripple current entered - core loss will report not evaluated for every candidate.";
            coreLossNote.className = "diagnostics-note diagnostics-note-warn";
        } else {
            coreLossNote.textContent = "Ripple current supplied - core loss will be computed for candidates whose material has Steinmetz data at this frequency.";
            coreLossNote.className = "diagnostics-note diagnostics-note-ok";
        }
    }

    // Minimum inductor current / conduction mode - a live preview of exactly
    // what the backend's CurrentConsistencyValidation computes
    // (RequirementDerivationService::derive(), same triangular-ripple
    // assumption and same 1e-6 A zero-epsilon convention as
    // BuckElectricalSolver's identical DCM check), so a genuine contradiction
    // is visible here before you ever click Generate - never blocking, since
    // the backend degrades this specific check to not-evaluated rather than
    // rejecting the whole design. Blank until both peak and ripple are
    // entered - there's no minimum-current relationship to compute without both.
    const conductionNote = document.getElementById("diagConductionNote");
    if (rippleRaw === "" || !(peakA > 0)) {
        setDiagValue("diagMinCurrent", "–");
        setDiagValue("diagConductionMode", "–");
        if (conductionNote) {
            conductionNote.textContent = "";
            conductionNote.className = "diagnostics-note";
        }
    } else {
        const ripple = Number(rippleRaw);
        const minCurrentA = peakA - ripple;
        const kZeroEpsilonA = 1e-6;
        setDiagValue("diagMinCurrent", `${minCurrentA.toFixed(3)} A`);

        if (minCurrentA < -kZeroEpsilonA) {
            setDiagValue("diagConductionMode", "DCM (unsupported)");
            if (conductionNote) {
                conductionNote.textContent =
                    "Ripple exceeds peak current - minimum inductor current would be negative (discontinuous conduction mode). Phase 1 only supports CCM, so this won't block Generate, but CurrentConsistencyValidation will report not-evaluated and conduction mode will show DCM (unsupported) for this candidate.";
                conductionNote.className = "diagnostics-note diagnostics-note-warn";
            }
        } else if (minCurrentA <= kZeroEpsilonA) {
            setDiagValue("diagConductionMode", "CCM Boundary");
            if (conductionNote) {
                conductionNote.textContent =
                    "Minimum inductor current is at or near zero - this sits right at the CCM/DCM boundary; small load or line variation may push it into DCM.";
                conductionNote.className = "diagnostics-note diagnostics-note-warn";
            }
        } else {
            setDiagValue("diagConductionMode", "CCM");
            if (conductionNote) {
                conductionNote.textContent = "";
                conductionNote.className = "diagnostics-note";
            }
        }
    }
}

function setTopologyStatus(message, isError = false) {
    const status = document.getElementById("topologyStatus");
    if (!status) return;
    status.textContent = message;
    status.className = isError ? "status-message error" : "status-message";
}

function clearTopologyDerived() {
    topologyDerived = null;

    const rmsInput = document.getElementById("rmsCurrent");
    if (rmsInput) {
        rmsInput.disabled = false;
        rmsInput.placeholder = "";
        rmsInput.value = "1.4";
    }

    const banner = document.getElementById("topologyDerivedBanner");
    if (banner) {
        banner.hidden = true;
        banner.innerHTML = "";
    }
    renderInputValidation();
}

function applyTopologyDerivedRequest(result, vinUsedV) {
    const derived = result.request;
    document.getElementById("inductance").value = derived.inductanceUH.toFixed(3);
    document.getElementById("current").value = derived.peakCurrentA.toFixed(3);
    document.getElementById("frequency").value = derived.switchingFreqKHz;
    document.getElementById("rippleCurrent").value = derived.rippleCurrentPeakToPeakA.toFixed(3);
    document.getElementById("ambientTemp").value = derived.ambientTemperatureC;
    document.getElementById("tempRise").value = derived.allowableTempRiseC;
    if (derived.inductanceTolerancePercent !== null && derived.inductanceTolerancePercent !== undefined) {
        document.getElementById("tolerance").value = derived.inductanceTolerancePercent;
    }

    topologyDerived = {
        averageCurrentA: derived.averageCurrentA,
        rippleCurrentPeakToPeakA: derived.rippleCurrentPeakToPeakA,
    };

    const rmsInput = document.getElementById("rmsCurrent");
    if (rmsInput) {
        rmsInput.value = "";
        rmsInput.disabled = true;
        rmsInput.placeholder = "from Buck";
    }

    const banner = document.getElementById("topologyDerivedBanner");
    if (banner) {
        const warningsHtml = result.warnings.length
            ? `<p class="topology-derived-warning">${result.warnings.join(" ")}</p>`
            : "";
        banner.hidden = false;
        banner.innerHTML = `
            <p><strong>Derived from your Buck converter requirements</strong> at the worst-case input voltage, Vin = ${vinUsedV} V.</p>
            <p>L = ${derived.inductanceUH.toFixed(3)} µH · Ipeak = ${derived.peakCurrentA.toFixed(3)} A ·
               Iavg = ${derived.averageCurrentA.toFixed(3)} A (= Iout) · Ripple = ${derived.rippleCurrentPeakToPeakA.toFixed(3)} A p-p ·
               fsw = ${derived.switchingFreqKHz} kHz</p>
            <p>RMS current is derived downstream from Iavg + ripple (triangular-ripple assumption), the same as if entered directly.</p>
            ${warningsHtml}
            <button type="button" id="clearTopologyDerivedButton">Clear and enter inductor requirements directly</button>
        `;
        const clearButton = document.getElementById("clearTopologyDerivedButton");
        if (clearButton) clearButton.addEventListener("click", clearTopologyDerived);
    }

    switchMode("direct");
    renderInputValidation();
}

async function calculateTopology() {
    setTopologyStatus("Calculating...");

    const button = document.getElementById("calculateTopologyButton");
    button.disabled = true;
    button.textContent = "Calculating...";

    try {
        const payload = buildTopologyPayload();
        const derived = await postRequest(TOPOLOGY_ENDPOINT, payload);
        applyTopologyDerivedRequest(derived, payload.vinMaxV);
        setTopologyStatus("Derived - review below and Generate Recommendation when ready.");
    } catch (error) {
        console.error(error);
        setTopologyStatus(error.message, true);
    } finally {
        button.disabled = false;
        button.textContent = "Calculate Magnetic Requirements";
    }
}

function checkCurrentSanity() {
    const warningElement = document.getElementById("currentWarning");
    if (!warningElement) return;

    const peak = Number(document.getElementById("current").value);
    const rms = Number(document.getElementById("rmsCurrent").value);

    if (peak > 0 && rms > 0 && rms > peak) {
        warningElement.textContent = "RMS current is higher than peak current - double check these values.";
    } else {
        warningElement.textContent = "";
    }
}

// Real client-side checks on the direct-mode fields (the ones Generate
// actually submits, regardless of whether they were typed directly or
// auto-filled by a Buck calculation). Only format-only problems (missing,
// negative, non-numeric) hard-block Generate here - the button stays
// disabled while any of those is present. Physical contradictions between
// peak/ripple/RMS (implied DCM, an out-of-envelope RMS) are NOT blocked
// here anymore: RequirementDerivationService::derive() degrades a genuine
// contradiction to NotEvaluated server-side rather than rejecting the whole
// design, so a candidate can still be generated from whatever data is
// actually usable. Those cases are surfaced as informational notes in the
// Diagnostics panel instead - see updateDirectDiagnosticsLive(). The one
// exception is RMS exceeding peak current, kept as a hard block since it's
// an unambiguous contradiction between two directly-entered numbers, not an
// assumption-dependent one - mirrors the one check RequirementDerivationService
// still throws on.
function validateDirectInputs() {
    const errors = [];
    const inductanceUH = Number(document.getElementById("inductance").value);
    const peakRaw = document.getElementById("current").value;
    const peakA = Number(peakRaw);
    const rmsA = Number(document.getElementById("rmsCurrent").value);
    const freqKHz = Number(document.getElementById("frequency").value);
    const ambientC = Number(document.getElementById("ambientTemp").value);
    const tempRiseC = Number(document.getElementById("tempRise").value);
    const rippleRaw = document.getElementById("rippleCurrent").value;

    if (!(inductanceUH > 0)) errors.push("Inductance must be a positive number.");
    if (peakRaw !== "" && !(peakA > 0)) {
        errors.push("Peak current must be a positive number, or left blank if unknown.");
    }
    if (!topologyDerived) {
        if (!(rmsA > 0)) errors.push("RMS current must be a positive number.");
        if (peakRaw !== "" && peakA > 0 && rmsA > 0 && rmsA > peakA) {
            errors.push("RMS current cannot exceed peak current - a real waveform's RMS value never exceeds its own peak.");
        }
    }
    if (!(freqKHz > 0)) errors.push("Switching frequency must be a positive number.");
    if (!Number.isFinite(ambientC)) errors.push("Ambient temperature must be a number.");
    if (!(tempRiseC > 0)) errors.push("Allowable temperature rise must be a positive number.");
    if (rippleRaw !== "" && !(Number(rippleRaw) >= 0)) {
        errors.push("Ripple current must be zero or a positive number, or left blank if unknown.");
    }

    return { valid: errors.length === 0, errors };
}

function renderInputValidation() {
    const card = document.getElementById("inputValidationCard");
    const generateButton = document.getElementById("generateButton");
    const { valid, errors } = validateDirectInputs();

    if (card) {
        if (valid) {
            card.hidden = true;
            card.innerHTML = "";
        } else {
            card.hidden = false;
            card.innerHTML = `
                <p class="input-validation-title">Fix before generating:</p>
                <ul>${errors.map((e) => `<li>${e}</li>`).join("")}</ul>
            `;
        }
    }
    if (generateButton) generateButton.disabled = !valid;
    return valid;
}

function setStatus(message, isError = false) {
    const status = document.getElementById("statusMessage");
    if (!status) return;
    status.textContent = message;
    status.className = isError ? "status-message error" : "status-message";
}

function clearResults() {
    ["feasibility", "triageStrip", "assistant"].forEach((id) => {
        const element = document.getElementById(id);
        if (element) element.innerHTML = "";
    });
    const feasibility = document.getElementById("feasibility");
    if (feasibility) feasibility.hidden = true;
    const recommended = document.getElementById("recommendedCandidateCard");
    if (recommended) {
        recommended.hidden = false;
        recommended.innerHTML = '<div class="loading-skeleton" aria-hidden="true"></div>';
    }
    const table = document.getElementById("candidateTable");
    if (table) table.innerHTML = "";
    const emptyState = document.getElementById("candidatesEmptyState");
    if (emptyState) {
        emptyState.hidden = false;
        emptyState.innerHTML = '<div class="loading-skeleton" aria-hidden="true"></div><div class="loading-skeleton" aria-hidden="true"></div>';
    }
    const passingSection = document.getElementById("passingSection");
    if (passingSection) passingSection.open = true;
    const rejectedSection = document.getElementById("rejectedSection");
    if (rejectedSection) {
        rejectedSection.hidden = true;
        rejectedSection.open = false;
    }
    const rankingNote = document.getElementById("rankingNote");
    if (rankingNote) rankingNote.hidden = true;
    const rulesSummaryLine = document.getElementById("rulesSummaryLine");
    if (rulesSummaryLine) rulesSummaryLine.textContent = "Generating...";
}

async function postRequest(endpoint, payload) {
    const response = await fetch(endpoint, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
    });

    if (!response.ok) {
        const body = await response.json().catch(() => ({}));
        throw new Error(body.detail || `Server error ${response.status}`);
    }

    return await response.json();
}

function renderRules(rules) {
    const element = document.getElementById("assistant");
    if (!element) return;

    element.innerHTML = `
        <p>Ku (window utilization): <strong>${rules.windowUtilization}</strong></p>
        <p>Default Bmax limit: <strong>${rules.defaultFluxDensityLimitT} T</strong> (Phase 1 default, not a material fact unless a candidate says otherwise)</p>
        <p>Allowable current density: <strong>${rules.allowableCurrentDensityAperCm2} A/cm²</strong></p>
        <p>Minimum saturation margin: <strong>${rules.minimumSaturationMarginPercent}%</strong></p>
        <p>Maximum fill factor: <strong>${rules.maximumFillFactor}</strong></p>
        <p>Default inductance tolerance: <strong>${rules.defaultInductanceTolerancePercent}%</strong></p>
    `;

    // Collapsed-by-default one-liner shown next to the summary triangle, so
    // the full breakdown above only has to be opened when actually wanted.
    const summaryLine = document.getElementById("rulesSummaryLine");
    if (summaryLine) {
        summaryLine.textContent =
            `Ku ${rules.windowUtilization} · Bmax ${rules.defaultFluxDensityLimitT}T · ` +
            `J ${rules.allowableCurrentDensityAperCm2}A/cm² · Sat margin ${rules.minimumSaturationMarginPercent}% · ` +
            `Fill ${rules.maximumFillFactor} · Tol ${rules.defaultInductanceTolerancePercent}%`;
    }
}

// Only rendered for the failure case - a passing run's counts are already
// shown once, prominently, in the Candidates triage strip immediately
// below. Repeating "N passed, M rejected" in a second boxed panel above
// it was the same sentence twice in two different cards.
function renderFeasibility(result) {
    const element = document.getElementById("feasibility");
    if (!element) return;

    if (result.status === "no_feasible_design") {
        let extra = "";
        if (result.requiredAreaProductCm4) {
            extra = `
                <p>Required area product: <strong>${result.requiredAreaProductCm4.toExponential(2)} cm⁴</strong></p>
                <p>Largest available area product: <strong>${result.largestAvailableAreaProductCm4.toExponential(2)} cm⁴</strong></p>
            `;
        }
        element.hidden = false;
        element.innerHTML = `
            <div class="result-block">
                <h3>No Feasible Design</h3>
                <p>${result.message}</p>
                ${extra}
            </div>
        `;
    } else {
        element.hidden = true;
        element.innerHTML = "";
    }
}

// Tallies why candidates were rejected so an engineer can triage a batch
// of failures at a glance instead of opening each one individually.
// Three headline counts only - not a rejection-reason tally, since that same
// information (each check, its actual/limit values, and its real explanation)
// already lives in every rejected candidate's own side panel. Repeating a
// summarized version here was the same fact shown twice.
function renderTriageStrip(result) {
    const element = document.getElementById("triageStrip");
    if (!element) return;

    const total = result.candidates.length + result.rejectedCandidates.length;
    if (total === 0) {
        element.innerHTML = "";
        return;
    }

    element.innerHTML = `
        <div class="triage-stats">
            <div class="triage-stat">
                <span class="triage-stat-value">${total}</span>
                <span class="triage-stat-label">Evaluated</span>
            </div>
            <div class="triage-stat triage-stat-pass">
                <span class="triage-stat-value">${result.candidates.length}</span>
                <span class="triage-stat-label">Passing</span>
            </div>
            <div class="triage-stat triage-stat-fail">
                <span class="triage-stat-value">${result.rejectedCandidates.length}</span>
                <span class="triage-stat-label">Rejected</span>
            </div>
        </div>
    `;
}

// How many of a candidate's MANDATORY checks actually ran and produced a
// real result - the same "mandatory" flag determineRecommendationStatus()
// gates the tier on (RecommendationStatus.cpp), read back out here so the
// UI's completeness chip can never disagree with the tier it sits next to.
function completeness(candidate) {
    const mandatory = candidate.validations.filter((v) => v.mandatory);
    const evaluated = mandatory.filter((v) => v.status === "Evaluated").length;
    return { evaluated, total: mandatory.length };
}

function completenessChip(candidate) {
    const c = completeness(candidate);
    const complete = c.evaluated === c.total;
    return `<span class="chip chip-completeness${complete ? " chip-completeness-full" : ""}" title="${c.evaluated} of ${c.total} mandatory checks evaluated">${c.evaluated}/${c.total} evaluated</span>`;
}

// Surfaces the one candidate result.candidates[0] already is (the table's
// own rank order, backend-computed by candidateRanksAhead() - see
// InductorDesignService.cpp) as a prominent card instead of leaving "which
// one is recommended" to be discovered by reading table position. Every
// number here is read from the same candidate object the table/detail panel
// use - nothing here is a second calculation of anything.
function renderRecommendedCandidate(result) {
    const element = document.getElementById("recommendedCandidateCard");
    if (!element) return;

    if (!result.candidates || result.candidates.length === 0) {
        element.hidden = true;
        element.innerHTML = "";
        return;
    }

    const candidate = result.candidates[0];
    const c = completeness(candidate);
    const tier = candidate.recommendation.tier;
    const tierLabel = tier === "Pass" ? "PASS" : tier === "ConditionalPass" ? "CONDITIONAL PASS" : "REJECT";
    const tierClass = tier === "Pass" ? "chip-pass" : tier === "ConditionalPass" ? "chip-warn" : "chip-fail";

    const notEvaluated = candidate.validations.filter((v) => v.status === "NotEvaluated");
    const stillNotEvaluated = notEvaluated.length
        ? `
        <div class="recommended-block">
            <h4>Still not evaluated</h4>
            <ul class="recommended-list">
                ${notEvaluated.map((v) => `<li><strong>${v.checkName}:</strong> ${v.explanation}</li>`).join("")}
            </ul>
        </div>`
        : "";

    const nextSteps = notEvaluated.length
        ? "See “Still not evaluated” above for exactly why each check couldn't run - missing input or a contradiction in what was entered - before treating this as final."
        : "Every mandatory check ran on real data - review the full validation list before committing to this part.";

    element.hidden = false;
    element.innerHTML = `
        <div class="card-head">
            <span class="card-icon" aria-hidden="true">
                <svg viewBox="0 0 18 18" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round">
                    <path d="M9 2 L11 6.5 L16 7.2 L12.5 10.6 L13.3 15.5 L9 13.2 L4.7 15.5 L5.5 10.6 L2 7.2 L7 6.5 Z"/>
                </svg>
            </span>
            <div class="card-head-titles">
                <h2>Recommended Candidate</h2>
                <span class="card-subtitle">Top-ranked of ${result.candidates.length} passing candidate${result.candidates.length === 1 ? "" : "s"}</span>
            </div>
        </div>
        <div class="recommended-top">
            <div class="recommended-identity">
                <span class="recommended-part">${candidate.core.partNumber}</span>
                ${shapeCellLabel(candidate.core)}
                <span class="chip ${tierClass}">${tierLabel}</span>
                ${completenessChip(candidate)}
            </div>
            <button type="button" class="recommended-view-detail" id="recommendedViewDetail">View full detail</button>
        </div>
        <div class="detail-kpis recommended-kpis">
            <div class="detail-kpi">
                <span class="detail-kpi-label">Material</span>
                <span class="detail-kpi-value">${candidate.material.materialFamily}</span>
            </div>
            <div class="detail-kpi">
                <span class="detail-kpi-label">Known Partial Loss</span>
                <span class="detail-kpi-value">${formatTotalLossCell(candidate)}</span>
            </div>
            <div class="detail-kpi">
                <span class="detail-kpi-label">Physical Fill %</span>
                <span class="detail-kpi-value">${(candidate.winding.physicalWindowFillFactor * 100).toFixed(1)}%</span>
            </div>
            <div class="detail-kpi">
                <span class="detail-kpi-label">Turns / Gap</span>
                <span class="detail-kpi-value">${candidate.turnsAndGap.turns}t, ${candidate.turnsAndGap.gapMethod === "Distributed" ? "distributed gap" : candidate.turnsAndGap.gapMm.toFixed(2) + "mm"}</span>
            </div>
        </div>
        <div class="recommended-block">
            <h4>Why this one</h4>
            <p class="recommended-explanation">${candidate.recommendation.explanation}</p>
        </div>
        ${stillNotEvaluated}
        <div class="recommended-block">
            <h4>Next steps</h4>
            <p class="recommended-explanation">${nextSteps}</p>
        </div>
    `;

    const viewDetailButton = document.getElementById("recommendedViewDetail");
    if (viewDetailButton) {
        viewDetailButton.addEventListener("click", () => openCandidateSidePanel(candidate, true));
    }
}

// Same plain "not evaluated" treatment as the Known Partial Loss tile right
// next to it in the same detail-kpi row (formatTotalLossCell below) - a loud
// chip-warn pill here read as visually inconsistent with its neighbor, and
// wrapped oddly inside the narrow stat tile.
function formatLoss(status, watts) {
    if (status !== "Evaluated") return '<span class="cell-muted">not evaluated</span>';
    return `${watts.toFixed(3)} W`;
}

// Same not_evaluated fact as formatLoss, but as quiet inline text rather
// than a loud chip - in the dense table this status is the common case
// (most/all rows), so a chip repeated on every row is noise rather than
// a useful signal. The chip form is kept for one-off mentions (detail view).
function formatLossCell(status, watts) {
    if (status !== "Evaluated") return '<span class="cell-muted">not evaluated</span>';
    return `${watts.toFixed(3)} W`;
}

// candidate.lossSummary.knownPartialLossW is the exact number the backend ranks
// passing candidates by (InductorDesignService.cpp's candidateRanksAhead()) - read
// directly from the API response rather than re-deriving it here, so the table's
// "Known Partial Loss" column can never silently drift from the real ranking.
function formatTotalLossCell(candidate) {
    const summary = candidate.lossSummary;
    if (!summary || (candidate.losses.copperLossStatus !== "Evaluated" && candidate.losses.coreLossStatus !== "Evaluated")) {
        return '<span class="cell-muted">not evaluated</span>';
    }
    return `${summary.knownPartialLossW.toFixed(3)} W`;
}

// 3-tier PASS/CONDITIONAL_PASS/REJECT recommendation status - real backend
// classification (RecommendationStatus.h), not the old "first row in a
// loss-sorted list" UI sugar. Only PASS gets a chip anywhere in the UI -
// CONDITIONAL_PASS is currently true of every passing candidate (PASS is
// structurally unreachable until real per-core thermal data exists - see
// FORMULAS.md section 12), so a "Conditional" label on every single row/panel
// would be constant noise, not a signal. The real facts behind it (which
// checks rest on a default assumption or preliminary estimate rather than
// measured data) are still spelled out in the status line and validation rows
// themselves - the tier NAME just isn't repeated as a label on every row.
function recommendationTierChip(tier) {
    if (tier === "Pass") return '<span class="chip chip-recommended">PASS</span>';
    return "";
}

// The single place a candidate's checks are shown - no separate "why
// rejected" banner duplicating this. Every check appears exactly once:
// status chip, name, "actual X · limit Y" (labeled, not a bare ratio), and
// its one real explanation line - a pass gets the same sentence a fail
// would, since "actual 0.449 vs limit 0.501" is a claim, and the sentence
// is what shows the claim actually checks out, not just a fail's excuse.
function renderValidationList(validations) {
    return validations
        .map((v) => {
            const notEvaluated = v.status === "NotEvaluated";
            const rowClass = notEvaluated ? "validation-row-warn" : v.passed ? "validation-row-pass" : "validation-row-fail";
            const chip = notEvaluated
                ? '<span class="chip chip-warn">NOT EVAL</span>'
                : v.passed
                ? '<span class="chip chip-pass">PASS</span>'
                : '<span class="chip chip-fail">FAIL</span>';
            // isRmsLowerBoundRejection: real peak current was never supplied, but flux
            // at RMS current alone (a guaranteed lower bound on the real peak - see
            // DesignValidation.cpp) already exceeded the limit, so failure is CERTAIN
            // regardless of the unknown real peak/ripple. A different kind of FAIL
            // than a check that ran against a real supplied peak - worth flagging so
            // an engineer doesn't read this as "less certain" than it actually is.
            const rmsFloorTag = v.isRmsLowerBoundRejection
                ? '<span class="chip chip-tag" title="Real peak current was never supplied - this fails even in the best case (RMS current alone, zero ripple), so the real peak (always >= RMS) is guaranteed to fail too">certain failure · RMS floor</span>'
                : "";
            return `
        <li class="validation-row ${rowClass}">
            <div class="validation-row-main">
                ${chip}
                <span class="validation-item-name">${v.checkName}</span>
                ${rmsFloorTag}
            </div>
            <div class="validation-item-value">actual <strong>${v.calculatedValue.toFixed(3)}</strong> · limit <strong>${v.limitValue.toFixed(3)}</strong> ${v.unit}${v.usesDefaultAssumption ? " *" : ""}</div>
            <div class="validation-row-explain">${v.explanation}</div>
        </li>
    `;
        })
        .join("");
}

// Real shape classification (Toroid/TwoPieceSet) from PyOpenMagnetics geometry -
// see scripts/export_real_data.py's _core_shape_and_family(). Empty string means
// no shape data was recorded for this core (never guessed from the part number).
// Its own table column (not a badge crammed next to the part number), same
// treatment as the Material column next to it.
// Real powder toroid materials achieve their working permeability through gapping
// distributed at the powder-particle level, baked into the catalog AL - there is
// no discrete machined air gap to report (see TurnsAndGapDesign.cpp's
// isDistributedGapCore branch). Showing "0.00 mm" for these would misleadingly
// imply a machinable dimension that does not physically exist on the part.
function gapCellLabel(turnsAndGap) {
    if (turnsAndGap.gapMethod === "Distributed") {
        return '<span class="cell-muted" title="Distributed-gap toroid - no discrete machined air gap exists on this part">Distributed gap</span>';
    }
    return turnsAndGap.gapMm.toFixed(2);
}

// Surfaces TurnsAndGapDesign.cpp's flux-aware seed decision - previously computed
// by the backend (turnsRaisedForSaturationMargin/Reason) but never shown anywhere
// in this UI. Distinguishes a raise based on a real supplied peak current from one
// based on turnsRaisedUsingRmsFloor (RMS current used as a guaranteed lower bound
// on the real, unsupplied peak - see DesignValidation.cpp) - the latter is a
// weaker guarantee (real peak, still unknown, could need more margin than the
// floor proves) and engineers reading this panel should be able to tell the two
// apart, not just see "turns were raised" with no context for how confident that is.
function turnsRaisedSubLine(turnsAndGap) {
    if (!turnsAndGap.turnsRaisedForSaturationMargin) return "";
    const basis = turnsAndGap.turnsRaisedUsingRmsFloor
        ? '<span class="chip chip-tag" title="Raised using RMS current as a guaranteed lower bound on peak - real (unsupplied) peak current could still require more margin than this floor guarantees">RMS floor</span>'
        : '<span class="chip chip-tag" title="Raised against a real, directly supplied peak current">peak-based</span>';
    return `<div class="detail-kpi-sub" title="${turnsAndGap.turnsRaisedForSaturationMarginReason}">raised for saturation margin ${basis}</div>`;
}

function shapeCellLabel(core) {
    if (!core.coreShape) return '<span class="cell-muted">—</span>';
    const label = core.coreShape === "TwoPieceSet" ? "Two-Piece Set" : core.coreShape;
    return `<span class="chip chip-shape" title="Shape family: ${core.shapeFamily || "unknown"}">${label}</span>`;
}


// Sources row: manufacturer/confidence/note for the material and core, collapsed
// by default - real provenance where it exists (core Vendor column), honestly
// blank where it doesn't (see Provenance.h - datasheet revision/URL/date-accessed
// are never fabricated).
function renderSourcesDetail(candidate) {
    const rows = [candidate.material.source, candidate.core.source]
        .map((s, i) => {
            const label = i === 0 ? "Material" : "Core";
            const manufacturer = s.manufacturer || "not sourced";
            const confidence = s.confidence || "Estimated";
            const note = s.note ? `<span class="detail-group-list-note"> — ${s.note}</span>` : "";
            return `<li><span class="detail-group-list-label">${label}</span> ${manufacturer} <span class="source-confidence-chip">${confidence}</span>${note}</li>`;
        })
        .join("");
    return `<details class="detail-group"><summary>Sources</summary><ul class="detail-group-list">${rows}</ul></details>`;
}

// AC-loss risk chip (spec section 8) - qualitative skin-depth heuristic, never a
// watts figure (SkinDepthRisk.h). The full reason (what was/wasn't evaluated) is
// the tooltip rather than inline text, to keep the KPI strip scannable.
function acLossRiskChip(acLossRisk) {
    const level = acLossRisk.riskLevel;
    const chipClass = level === "Low" ? "chip-pass" : level === "Moderate" ? "chip-warn" : "chip-fail";
    return `<span class="chip ${chipClass}" title="${acLossRisk.reason}">AC risk: ${level}</span>`;
}

// Which real, user-suppliable input would let a NotEvaluated check actually
// run - only for the 5 checks that can ever report NotEvaluated (confirmed
// against DesignValidation.cpp). ThermalValidation/BundleFitValidation are
// deliberately absent: their real missing-data reason varies per candidate
// (a database gap, a winding-geometry fact) and isn't a single fixed input
// the user can type in, so no additional-input hint is fabricated for them -
// their own explanation text (already shown per-check below) is the honest
// answer instead.
const ADDITIONAL_INPUT_NEEDED = {
    CurrentConsistencyValidation: "Peak current and ripple current (both, mutually consistent)",
    PeakFluxValidation: "Peak current",
    SaturationValidation: "Peak current",
};

// Compact Evaluated/Not-Evaluated summary, positioned right under the status
// chips - complements rather than duplicates the full Validation Checks list
// below (which has each check's calculated value and full explanation).
function renderValidationCoverage(candidate) {
    const evaluated = candidate.validations.filter((v) => v.status === "Evaluated");
    const notEvaluated = candidate.validations.filter((v) => v.status === "NotEvaluated");
    if (notEvaluated.length === 0) {
        return `<div class="coverage-block coverage-block-full">Validation coverage: every mandatory check ran (${evaluated.length}/${evaluated.length}).</div>`;
    }
    return `
        <div class="coverage-block">
            <div class="coverage-block-title">Validation Coverage</div>
            <div class="coverage-columns">
                <div class="coverage-col">
                    <div class="coverage-col-label">Evaluated (${evaluated.length})</div>
                    <ul>${evaluated.map((v) => `<li>${v.checkName}</li>`).join("")}</ul>
                </div>
                <div class="coverage-col coverage-col-not-eval">
                    <div class="coverage-col-label">Not Evaluated (${notEvaluated.length})</div>
                    <ul>${notEvaluated
                        .map((v) => {
                            const needed = ADDITIONAL_INPUT_NEEDED[v.checkName];
                            return `<li><strong>${v.checkName}</strong>${needed ? `<br><span class="coverage-needs">needs: ${needed}</span>` : ""}</li>`;
                        })
                        .join("")}</ul>
                </div>
            </div>
        </div>
    `;
}

function renderCandidateDetail(candidate) {
    const warnings = candidate.material.missingDataWarnings
        .concat(candidate.winding.missingData || [])
        .concat(candidate.losses.missingData || []);

    const failCount = candidate.validations.filter((v) => v.status !== "NotEvaluated" && !v.passed).length;
    const notEvalCount = candidate.validations.filter((v) => v.status === "NotEvaluated").length;
    const passCount = candidate.validations.length - failCount - notEvalCount;
    const usesDefaultAssumption = candidate.validations.some((v) => v.usesDefaultAssumption);

    // KPIs first, always - the numbers an engineer actually judges a
    // candidate by, in one scannable strip, before any narrative text.
    const kpis = `
        <div class="detail-kpis">
            <div class="detail-kpi">
                <span class="detail-kpi-label">${candidate.lossSummary.label.startsWith("Known") ? "Known Partial Loss" : "Loss"}</span>
                <span class="detail-kpi-value">${formatTotalLossCell(candidate)}</span>
            </div>
            <div class="detail-kpi">
                <span class="detail-kpi-label">Core Loss</span>
                <span class="detail-kpi-value">${formatLoss(candidate.losses.coreLossStatus, candidate.losses.coreLossW)}</span>
            </div>
            <div class="detail-kpi">
                <span class="detail-kpi-label" title="Physical window fill - includes insulation build-up, packing density, and margin/lead-exit allowance. This is the number WindingFitValidation gates on.">Physical Fill %</span>
                <span class="detail-kpi-value">${(candidate.winding.physicalWindowFillFactor * 100).toFixed(1)}%</span>
                <span class="detail-kpi-sub" title="Raw copper cross-section over window area, before any insulation/packing/margin derating - informational only, not the gate.">raw copper fill ${(candidate.winding.fillFactor * 100).toFixed(1)}%</span>
            </div>
            <div class="detail-kpi">
                <span class="detail-kpi-label">Current Density</span>
                <span class="detail-kpi-value">${candidate.winding.currentDensityAperMm2.toFixed(2)} A/mm²</span>
            </div>
            <div class="detail-kpi">
                <span class="detail-kpi-label">Turns / Gap</span>
                <span class="detail-kpi-value">${candidate.turnsAndGap.turns}t, ${candidate.turnsAndGap.gapMethod === "Distributed" ? "distributed gap" : candidate.turnsAndGap.gapMm.toFixed(2) + "mm"}</span>
                ${turnsRaisedSubLine(candidate.turnsAndGap)}
            </div>
        </div>
        <p class="detail-winding-line">Winding: <strong>${candidate.winding.wireDescription}</strong> &nbsp; ${acLossRiskChip(candidate.acLossRisk)}</p>
    `;

    // One-line status for a rejection only - the pass/conditional-pass case
    // used to restate "N of M checks passed, K not evaluated" here too, but
    // that's now the Validation Coverage block right below (same facts, more
    // detail); repeating it here was the same sentence twice. Tier comes
    // from the real backend classification - only "Recommended" ever
    // renders a chip here (see recommendationTierChip), so this stays empty
    // whenever tier isn't Pass (currently always, since Pass is
    // structurally unreachable - see RecommendationStatus.h).
    const tierChip = recommendationTierChip(candidate.recommendation.tier);
    const statusLine = candidate.rejectionReasons.length
        ? `<div class="detail-status detail-status-fail">
            <div class="detail-status-chips"><span class="chip chip-fail">REJECTED</span></div>
            <div class="detail-status-sub">${candidate.rejectionReasons.length} of ${candidate.validations.length} checks failed</div>
        </div>`
        : tierChip
        ? `<div class="detail-status detail-status-pass"><div class="detail-status-chips">${tierChip}</div></div>`
        : "";

    // No rankingLine/recommendation.explanation paragraph here - that exact
    // sentence already appears once, in the Recommended Candidate card's
    // "Why this one" section (see renderRecommendedCandidate()), and the
    // statusLine directly above already states the same passed/not-evaluated/
    // preliminary facts in the panel's own words. Repeating the backend's
    // sentence a second time here said the same thing three times over for
    // the top candidate (card, then twice in its own panel).

    // Overview (KPIs + status) is always visible, never collapsed - it's the
    // numbers a candidate is actually judged by. Everything below is grouped
    // into its own collapsible section so the panel doesn't read as one long
    // undifferentiated scroll - Validations stays open by default since it's
    // still primary content, Sources and Warnings default closed.
    return `
        ${kpis}
        ${statusLine}
        ${renderValidationCoverage(candidate)}
        ${candidate.designNarrative ? `<p class="detail-next-step"><strong>Suggested next step:</strong> ${candidate.designNarrative}</p>` : ""}
        <details class="detail-group" open>
            <summary>Validation Checks <span class="detail-group-count">(${passCount} passed, ${failCount} failed, ${notEvalCount} not evaluated)</span></summary>
            <ul class="validation-list">${renderValidationList(candidate.validations)}</ul>
            ${usesDefaultAssumption ? '<p class="validation-footnote">* Phase 1 default limit, not a material-specific value</p>' : ""}
        </details>
        ${renderSourcesDetail(candidate)}
        ${
            warnings.length
                ? `<details class="detail-group"><summary>Missing-data warnings <span class="detail-group-count">(${warnings.length})</span></summary><ul class="detail-group-list detail-group-list-warn">${warnings.map((w) => `<li>${w}</li>`).join("")}</ul></details>`
                : ""
        }
    `;
}

const CANDIDATE_TABLE_HEADERS = [
    { label: "Status" },
    { label: "Core" },
    { label: "Shape" },
    { label: "Material" },
    { label: "Turns", numeric: true },
    { label: "Gap (mm)", numeric: true },
    // µ (micro sign) has no real uppercase form - the table header's CSS
    // text-transform:uppercase (.candidate-table thead th) was mapping it to
    // Greek capital Mu, which most fonts render indistinguishably from Latin
    // "M", so this column was displaying as "CALC L (MH)" instead of "µH" -
    // a real user report caught this. Wrapping just the unit in
    // .unit-literal (text-transform:none) keeps the rest of the header
    // uppercase as designed while leaving µH untouched.
    { label: 'Calc L (<span class="unit-literal">µH</span>)', numeric: true },
    { label: "Error %", numeric: true },
    { label: "Physical Fill %", numeric: true },
    { label: "DC Copper Loss", numeric: true },
    { label: "Core Loss", numeric: true },
    { label: "Known Partial Loss", numeric: true },
];

function candidateRowHtml(c, passed, index) {
    const tier = c.recommendation.tier;
    const badge = passed
        ? `<span class="chip chip-pass">PASS</span>${recommendationTierChip(tier)}`
        : '<span class="chip chip-fail">REJECT</span>';
    const rowClass = ["candidate-row", passed ? "row-pass" : "row-reject", tier === "Pass" ? "row-recommended" : ""]
        .filter(Boolean)
        .join(" ");
    return `
        <tr class="${rowClass}" data-row-index="${index}">
            <td>${badge} ${completenessChip(c)}</td>
            <td>${c.core.partNumber}</td>
            <td>${shapeCellLabel(c.core)}</td>
            <td>${c.material.materialFamily}</td>
            <td class="numeric">${c.turnsAndGap.turns}</td>
            <td class="numeric">${gapCellLabel(c.turnsAndGap)}</td>
            <td class="numeric">${c.turnsAndGap.calculatedInductanceUH.toFixed(2)}</td>
            <td class="numeric">${c.turnsAndGap.inductanceErrorPercent.toFixed(2)}</td>
            <td class="numeric">${(c.winding.physicalWindowFillFactor * 100).toFixed(1)}</td>
            <td class="numeric">${formatLossCell(c.losses.copperLossStatus, c.losses.copperLossW)}</td>
            <td class="numeric">${formatLossCell(c.losses.coreLossStatus, c.losses.coreLossW)}</td>
            <td class="numeric">${formatTotalLossCell(c)}</td>
        </tr>
    `;
}

function wireCandidateRowClicks(table, rows, passed) {
    table.querySelectorAll(".candidate-row").forEach((tr) => {
        tr.addEventListener("click", () => {
            const candidate = rows[Number(tr.dataset.rowIndex)];
            if (candidate) openCandidateSidePanel(candidate, passed);
        });
    });
}

// Passing candidates get their own always-visible table (result.candidates
// is already ranked by the backend - candidateRanksAhead() in
// InductorDesignService.cpp - lowest known-partial-loss first within a tier),
// separated structurally from Rejected rather than a filter toggle a reader
// has to remember to use, so a first-time look at this card never opens on
// a wall of failures.
function renderPassingTable(result) {
    const table = document.getElementById("candidateTable");
    const emptyState = document.getElementById("candidatesEmptyState");
    const tabCount = document.getElementById("passingTabCount");
    if (!table) return;

    if (tabCount) tabCount.textContent = String(result.candidates.length);

    let rows = result.candidates;
    if (currentShapeFilter !== "all") rows = rows.filter((c) => c.core.coreShape === currentShapeFilter);
    if (currentMaterialTypeFilter !== "all") rows = rows.filter((c) => c.core.materialType === currentMaterialTypeFilter);

    if (rows.length === 0) {
        table.innerHTML = "";
        if (emptyState) {
            emptyState.hidden = false;
            emptyState.innerHTML =
                result.candidates.length === 0
                    ? '<p class="candidates-empty-title">No candidate passed every mandatory check.</p><p>See Rejected Candidates below for why.</p>'
                    : '<p class="candidates-empty-title">No passing candidate matches the selected filters.</p>';
        }
        return;
    }
    if (emptyState) {
        emptyState.hidden = true;
        emptyState.innerHTML = "";
    }

    const headerHtml = CANDIDATE_TABLE_HEADERS.map((h) => `<th${h.numeric ? ' class="numeric"' : ""}>${h.label}</th>`).join("");
    const bodyHtml = rows.map((c, index) => candidateRowHtml(c, true, index)).join("");
    table.innerHTML = `<thead><tr>${headerHtml}</tr></thead><tbody>${bodyHtml}</tbody>`;
    wireCandidateRowClicks(table, rows, true);
}

// Rejected candidates live behind their own tab, not stacked under the
// passing table - de-emphasized by default (the Rejected tab isn't shown at
// all unless there's at least one, and switching to it is never the default
// after a run - see switchCandidatesTab()) since they're the "why not"
// record, not the primary answer, but never hidden entirely - every
// evaluated candidate is still one click away.
function renderRejectedSection(result) {
    const rejectedBtn = document.getElementById("rejectedTabBtn");
    const tabCount = document.getElementById("rejectedTabCount");
    const table = document.getElementById("rejectedTable");
    if (!table) return;

    if (result.rejectedCandidates.length === 0) {
        if (rejectedBtn) rejectedBtn.hidden = true;
        table.innerHTML = "";
        // This run has no rejected candidates at all - if a previous run left the Rejected tab
        // active, fall back to Passing so the user is never looking at a hidden, unreachable tab.
        if (currentCandidatesTab === "rejected") switchCandidatesTab("passing");
        return;
    }
    if (rejectedBtn) rejectedBtn.hidden = false;
    if (tabCount) tabCount.textContent = String(result.rejectedCandidates.length);

    let rows = result.rejectedCandidates;
    if (currentShapeFilter !== "all") rows = rows.filter((c) => c.core.coreShape === currentShapeFilter);
    if (currentMaterialTypeFilter !== "all") rows = rows.filter((c) => c.core.materialType === currentMaterialTypeFilter);

    if (rows.length === 0) {
        table.innerHTML = `<tbody><tr><td class="table-empty">No rejected candidates match the selected filters.</td></tr></tbody>`;
        return;
    }

    const headerHtml = CANDIDATE_TABLE_HEADERS.map((h) => `<th${h.numeric ? ' class="numeric"' : ""}>${h.label}</th>`).join("");
    const bodyHtml = rows.map((c, index) => candidateRowHtml(c, false, index)).join("");
    table.innerHTML = `<thead><tr>${headerHtml}</tr></thead><tbody>${bodyHtml}</tbody>`;
    wireCandidateRowClicks(table, rows, false);
}

// Passing/Rejected as real tabs (one panel visible at a time) instead of the
// old stacked <details> accordion - a real user report: switching back and
// forth meant scrolling to find the collapsed section's <summary> bar each
// time. Wired once against the static tab buttons/panels in the page (their
// contents get replaced on every render, but the elements themselves never
// do) - clicking a tab just toggles which panel is hidden, no scrolling
// required to reach the other one.
let currentCandidatesTab = "passing"; // passing | rejected

function switchCandidatesTab(tab) {
    currentCandidatesTab = tab;
    const passingBtn = document.getElementById("passingTabBtn");
    const rejectedBtn = document.getElementById("rejectedTabBtn");
    const passingPanel = document.getElementById("passingSection");
    const rejectedPanel = document.getElementById("rejectedSection");
    if (!passingBtn || !rejectedBtn || !passingPanel || !rejectedPanel) return;

    const showPassing = tab === "passing";
    passingBtn.classList.toggle("active", showPassing);
    passingBtn.setAttribute("aria-selected", String(showPassing));
    passingPanel.hidden = !showPassing;

    rejectedBtn.classList.toggle("active", !showPassing);
    rejectedBtn.setAttribute("aria-selected", String(!showPassing));
    rejectedPanel.hidden = showPassing;
}

function wireCandidatesTabs() {
    const passingBtn = document.getElementById("passingTabBtn");
    const rejectedBtn = document.getElementById("rejectedTabBtn");
    if (!passingBtn || !rejectedBtn) return;

    passingBtn.addEventListener("click", () => switchCandidatesTab("passing"));
    rejectedBtn.addEventListener("click", () => switchCandidatesTab("rejected"));
}

// Slide-in side panel for candidate detail (replaces the old inline accordion
// row, which crammed KPIs, status, every validation check, sources, and
// warnings into one long stacked block under the table row). The table stays
// visible/scrollable behind it - clicking a different row just re-renders
// the same panel's contents rather than opening a second one.
function openCandidateSidePanel(candidate, passed) {
    const panel = document.getElementById("candidateSidePanel");
    const title = document.getElementById("sidePanelTitle");
    const subtitle = document.getElementById("sidePanelSubtitle");
    const body = document.getElementById("sidePanelBody");
    if (!panel || !title || !subtitle || !body) return;

    title.textContent = candidate.core.partNumber;
    subtitle.innerHTML = `${shapeCellLabel(candidate.core)} <span>${candidate.material.materialFamily}</span> <span>·</span> <span>${passed ? "Passing" : "Rejected"}</span>`;
    body.innerHTML = renderCandidateDetail(candidate);

    panel.hidden = false;
    panel.setAttribute("aria-hidden", "false");
    // Next frame, so the hidden->visible change and the slide-in transition
    // don't collapse into a single instant jump.
    requestAnimationFrame(() => panel.classList.add("open"));
}

function closeCandidateSidePanel() {
    const panel = document.getElementById("candidateSidePanel");
    if (!panel || panel.hidden) return;
    panel.classList.remove("open");
    panel.setAttribute("aria-hidden", "true");
    // Wait for the slide-out transition to finish before actually hiding -
    // matches the CSS transition duration (see .side-panel-content).
    window.setTimeout(() => {
        if (!panel.classList.contains("open")) panel.hidden = true;
    }, 220);
}

// Shape filter (Toroid / Two-Piece Set) - added directly in response to a real
// user report that the tool had no way to search or filter by core shape. Only
// offers shapes actually present in this result, so an empty snapshot can't
// show a dead dropdown option.
function renderShapeFilter(result) {
    const element = document.getElementById("shapeFilter");
    if (!element) return;

    const allCandidates = result.candidates.concat(result.rejectedCandidates);
    const shapesPresent = new Set(allCandidates.map((c) => c.core.coreShape).filter(Boolean));

    if (shapesPresent.size === 0) {
        element.hidden = true;
        return;
    }
    element.hidden = false;

    const shapeLabels = { all: "All shapes", Toroid: "Toroid", TwoPieceSet: "Two-Piece Set" };
    const options = ["all", ...Array.from(shapesPresent)]
        .map((key) => `<option value="${key}"${key === currentShapeFilter ? " selected" : ""}>${shapeLabels[key] || key}</option>`)
        .join("");
    element.innerHTML = options;

    element.onchange = () => {
        currentShapeFilter = element.value;
        renderPassingTable(lastResult);
        renderRejectedSection(lastResult);
    };
}

// Same pattern as renderShapeFilter() above, one filter dimension over -
// material FAMILY (ferrite vs powder), not the specific grade (e.g. "MPP 60"
// vs "N87"). A real user report: with 150+ candidates now that the core
// database covers real MPP/Kool Mu/High Flux permeability grades (not just
// the handful that used to survive the old sampling bug), ferrite still
// crowds out powder in the ranked list - shape alone doesn't let an engineer
// isolate "just show me the powder options." Filtering by the specific grade
// name instead of family would mean 30+ options (one per real MPP/Kool
// Mu/... permeability grade) - too granular to be a useful dropdown; the
// grade itself is still visible in the Material column per row.
function renderMaterialTypeFilter(result) {
    const element = document.getElementById("materialTypeFilter");
    if (!element) return;

    const allCandidates = result.candidates.concat(result.rejectedCandidates);
    const typesPresent = new Set(allCandidates.map((c) => c.core.materialType).filter(Boolean));

    if (typesPresent.size === 0) {
        element.hidden = true;
        return;
    }
    element.hidden = false;

    const typeLabels = { all: "All types", ferrite: "Ferrite", powder: "Powder" };
    const options = ["all", ...Array.from(typesPresent)]
        .map((key) => `<option value="${key}"${key === currentMaterialTypeFilter ? " selected" : ""}>${typeLabels[key] || key}</option>`)
        .join("");
    element.innerHTML = options;

    element.onchange = () => {
        currentMaterialTypeFilter = element.value;
        renderPassingTable(lastResult);
        renderRejectedSection(lastResult);
    };
}

async function generateRecommendation() {
    // Hard block - re-check even though the button is already disabled when
    // invalid, since this function is the single entry point regardless of
    // how it's triggered.
    if (!renderInputValidation()) {
        setStatus("Fix the highlighted inputs before generating.", true);
        return;
    }

    clearResults();
    setStatus("Generating recommendation...");

    const button = document.getElementById("generateButton");
    button.disabled = true;
    button.textContent = "Generating...";

    try {
        const payload = buildPayload();
        const result = await postRequest(ENDPOINT, payload);
        lastResult = result;
        currentShapeFilter = "all";
        currentMaterialTypeFilter = "all";

        console.log("DesignRecommendation", result);

        renderRules(result.activeRules);
        renderFeasibility(result);
        renderTriageStrip(result);
        renderRecommendedCandidate(result);
        renderShapeFilter(result);
        renderMaterialTypeFilter(result);
        renderPassingTable(result);
        renderRejectedSection(result);
        // Every fresh run opens back on the Passing tab, even if a previous run left
        // Rejected active - same reset policy as the shape/material-type filters above.
        switchCandidatesTab("passing");

        // The ranking policy only means anything once there's something to
        // rank - showing it before any run just reads as unexplained noise.
        const rankingNote = document.getElementById("rankingNote");
        if (rankingNote) rankingNote.hidden = result.candidates.length + result.rejectedCandidates.length === 0;

        // Jump straight to the results card instead of leaving the user to scroll past
        // the (often tall) requirements form and diagnostics panel to find the table -
        // a real complaint once candidate counts grew into the hundreds (see
        // candidatesByTechnology/material-type-filter work above for why).
        const candidatesCard = document.querySelector(".candidates-card");
        if (candidatesCard) candidatesCard.scrollIntoView({ behavior: "smooth", block: "start" });

        setStatus(
            result.status === "ok"
                ? "Recommendation generated successfully."
                : "No feasible design - see feasibility panel."
        );
    } catch (error) {
        console.error(error);
        setStatus(error.message, true);
        const recommended = document.getElementById("recommendedCandidateCard");
        if (recommended) {
            recommended.hidden = true;
            recommended.innerHTML = "";
        }
        const emptyState = document.getElementById("candidatesEmptyState");
        if (emptyState) {
            emptyState.hidden = false;
            emptyState.innerHTML = `<p class="candidates-empty-title">Could not generate a recommendation.</p><p>${error.message}</p>`;
        }
    } finally {
        button.disabled = false;
        button.textContent = "Generate Recommendation";
    }
}

window.addEventListener("DOMContentLoaded", () => {
    document.getElementById("generateButton").addEventListener("click", generateRecommendation);
    ["current", "rmsCurrent"].forEach((id) => {
        document.getElementById(id).addEventListener("input", checkCurrentSanity);
    });

    document.getElementById("modeDirectBtn").addEventListener("click", () => switchMode("direct"));
    document.getElementById("modeBuckBtn").addEventListener("click", () => switchMode("buck"));
    document.getElementById("calculateTopologyButton").addEventListener("click", calculateTopology);

    const debouncedBuckDiagnostics = debounce(updateBuckDiagnosticsLive, 350);
    ["buckVinMin", "buckVinMax", "buckVout", "buckIout", "buckFrequency", "buckRipplePercent"].forEach((id) => {
        document.getElementById(id).addEventListener("input", debouncedBuckDiagnostics);
    });

    ["inductance", "current", "rmsCurrent", "rippleCurrent"].forEach((id) => {
        document.getElementById(id).addEventListener("input", updateDirectDiagnosticsLive);
    });

    ["inductance", "current", "rmsCurrent", "rippleCurrent", "frequency", "ambientTemp", "tempRise"].forEach((id) => {
        document.getElementById(id).addEventListener("input", renderInputValidation);
    });
    renderInputValidation();

    // Before any run: distinct from "generating" (skeleton) and "no results
    // matched a filter" (see renderPassingTable) - a plain first-look state
    // pointing at what to do next.
    const emptyState = document.getElementById("candidatesEmptyState");
    if (emptyState) {
        emptyState.hidden = false;
        emptyState.innerHTML = '<p class="candidates-empty-title">No recommendation generated yet.</p><p>Enter your requirements above and click Generate Recommendation.</p>';
    }

    document.querySelectorAll(".field-tabs").forEach(initFieldTabs);
    wireCandidatesTabs();

    document.getElementById("sidePanelClose").addEventListener("click", closeCandidateSidePanel);
    document.getElementById("sidePanelOverlay").addEventListener("click", closeCandidateSidePanel);
    document.addEventListener("keydown", (event) => {
        if (event.key === "Escape") closeCandidateSidePanel();
    });

    switchMode("direct");
});
