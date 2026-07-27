const ENDPOINT = "/inductor-design";
const TOPOLOGY_ENDPOINT = "/topology-design/buck";

let lastResult = null;
let currentFilter = "all"; // all | passing | rejected
let currentShapeFilter = "all"; // all | Toroid | TwoPieceSet

// Set when Mode 1 (Buck converter) has derived requirements - holds the
// averageCurrentA/rippleCurrentPeakToPeakA pair so buildPayload() sends
// those instead of a directly-entered rmsCurrentA. null in Mode 2.
let topologyDerived = null;

function buildPayload() {
    const toleranceRaw = document.getElementById("tolerance").value;
    const rippleRaw = document.getElementById("rippleCurrent").value;

    const payload = {
        inductanceUH: Number(document.getElementById("inductance").value),
        peakCurrentA: Number(document.getElementById("current").value),
        switchingFreqKHz: Number(document.getElementById("frequency").value),
        ambientTemperatureC: Number(document.getElementById("ambientTemp").value),
        allowableTempRiseC: Number(document.getElementById("tempRise").value),
    };

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

    if (document.getElementById("diagStoredEnergy")) {
        if (inductanceUH > 0 && peakA > 0) {
            // E = 0.5 x L x I^2 - the same headline formula already shown in
            // the Inductance field's hint tooltip, just kept live here too.
            const energyMJ = 0.5 * (inductanceUH * 1e-6) * peakA * peakA * 1e3;
            setDiagValue("diagStoredEnergy", `${energyMJ.toFixed(4)} mJ`);
        } else {
            setDiagValue("diagStoredEnergy", "–");
        }
    }

    const currentNote = document.getElementById("diagCurrentNote");
    if (currentNote) {
        if (peakA > 0 && rmsA > 0 && rmsA > peakA) {
            currentNote.textContent = "RMS current is higher than peak current - that can't happen physically, double check these values.";
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
    const table = document.getElementById("candidateTable");
    if (table) table.innerHTML = "";
    const chips = document.getElementById("filterChips");
    if (chips) chips.innerHTML = "";
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

// Reproducibility info - real content hashes of the loaded CSV data plus literal
// engine/rules version strings (EngineVersions.h), never an invented upstream semver.
// Folded into the existing "Active Rules & Assumptions" disclosure instead of its
// own always-visible line at the page bottom - same collapsed-by-default pattern
// as everything else reference-only on this page.
function renderVersions(versions) {
    const element = document.getElementById("assistant");
    if (!element || !versions) return;
    const line = document.createElement("p");
    line.className = "rules-versions-line";
    line.textContent =
        `Engine ${versions.calculationEngineVersion} · Rules ${versions.designRulesVersion} · ` +
        `Core DB ${versions.coreDatabaseVersion} · Material DB ${versions.materialDatabaseVersion}`;
    element.appendChild(line);
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

function statusChip(status) {
    if (status === "Evaluated") return "";
    if (status === "NotEvaluated") return '<span class="chip chip-warn">not evaluated</span>';
    return `<span class="chip chip-fail">${status}</span>`;
}

function formatLoss(status, watts) {
    if (status !== "Evaluated") return statusChip(status);
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

// candidate.lossSummary.knownEvaluatedLossW is the exact number the backend ranks
// passing candidates by (InductorDesignService.cpp's candidateRanksAhead()) - read
// directly from the API response rather than re-deriving it here, so the table's
// "Known Evaluated Loss" column can never silently drift from the real ranking.
function formatTotalLossCell(candidate) {
    const summary = candidate.lossSummary;
    if (!summary || (candidate.losses.copperLossStatus !== "Evaluated" && candidate.losses.coreLossStatus !== "Evaluated")) {
        return '<span class="cell-muted">not evaluated</span>';
    }
    return `${summary.knownEvaluatedLossW.toFixed(3)} W`;
}

// 3-tier recommendation status (spec section 10) - real backend classification,
// not the old "first row in a loss-sorted list" UI sugar. Only "Recommended"
// gets a chip anywhere in the UI - "PreliminaryCandidate" is currently true of
// every passing candidate (Phase1Recommended is structurally unreachable until
// real per-core thermal data exists - see FORMULAS.md section 12), so a
// "Preliminary" label on every single row/panel would be constant noise, not a
// signal. The real facts behind it (which checks rest on a default assumption
// rather than measured data) are still spelled out in the status line and
// validation rows themselves - the tier NAME just isn't repeated as a label.
function recommendationTierChip(tier) {
    if (tier === "Phase1Recommended") return '<span class="chip chip-recommended">Recommended</span>';
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
            return `
        <li class="validation-row ${rowClass}">
            <div class="validation-row-main">
                ${chip}
                <span class="validation-item-name">${v.checkName}</span>
                <span class="validation-item-value">actual <strong>${v.calculatedValue.toFixed(3)}</strong> · limit <strong>${v.limitValue.toFixed(3)}</strong> ${v.unit}${v.usedDefaultLimit ? " *" : ""}</span>
            </div>
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
function shapeCellLabel(core) {
    if (!core.coreShape) return '<span class="cell-muted">—</span>';
    const label = core.coreShape === "TwoPieceSet" ? "Two-Piece Set" : core.coreShape;
    return `<span class="chip chip-shape" title="Shape family: ${core.shapeFamily || "unknown"}">${label}</span>`;
}

function candidateRows(result) {
    // result.candidates is already ranked (see InductorDesignService.cpp's
    // candidateRanksAhead()) - the passed flag drives which bucket a row sorts
    // into; the 3-tier chip itself comes from the real backend classification
    // (candidate.recommendation.tier), not from table position.
    const passing = result.candidates.map((c) => ({ candidate: c, passed: true }));
    const rejected = result.rejectedCandidates.map((c) => ({ candidate: c, passed: false }));
    return passing.concat(rejected);
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
            const note = s.note ? ` — ${s.note}` : "";
            return `<li><strong>${label}:</strong> ${manufacturer} (${confidence})${note}</li>`;
        })
        .join("");
    return `<details><summary>Sources</summary><ul>${rows}</ul></details>`;
}

// AC-loss risk chip (spec section 8) - qualitative skin-depth heuristic, never a
// watts figure (SkinDepthRisk.h). The full reason (what was/wasn't evaluated) is
// the tooltip rather than inline text, to keep the KPI strip scannable.
function acLossRiskChip(acLossRisk) {
    const level = acLossRisk.riskLevel;
    const chipClass = level === "Low" ? "chip-pass" : level === "Moderate" ? "chip-warn" : "chip-fail";
    return `<span class="chip ${chipClass}" title="${acLossRisk.reason}">AC risk: ${level}</span>`;
}

function renderCandidateDetail(candidate) {
    const warnings = candidate.material.missingDataWarnings
        .concat(candidate.winding.missingData || [])
        .concat(candidate.losses.missingData || []);

    const failCount = candidate.validations.filter((v) => v.status !== "NotEvaluated" && !v.passed).length;
    const notEvalCount = candidate.validations.filter((v) => v.status === "NotEvaluated").length;
    const passCount = candidate.validations.length - failCount - notEvalCount;
    const usedDefaultLimit = candidate.validations.some((v) => v.usedDefaultLimit);
    const preliminaryChecks = candidate.validations.filter((v) => v.isPreliminaryEstimate);

    // KPIs first, always - the numbers an engineer actually judges a
    // candidate by, in one scannable strip, before any narrative text.
    const kpis = `
        <div class="detail-kpis">
            <div class="detail-kpi">
                <span class="detail-kpi-label">${candidate.lossSummary.label.startsWith("Known") ? "Known Evaluated Loss" : "Loss"}</span>
                <span class="detail-kpi-value">${formatTotalLossCell(candidate)}</span>
            </div>
            <div class="detail-kpi">
                <span class="detail-kpi-label">Core Loss</span>
                <span class="detail-kpi-value">${formatLoss(candidate.losses.coreLossStatus, candidate.losses.coreLossW)}</span>
            </div>
            <div class="detail-kpi">
                <span class="detail-kpi-label">Fill Factor</span>
                <span class="detail-kpi-value">${(candidate.winding.fillFactor * 100).toFixed(1)}%</span>
            </div>
            <div class="detail-kpi">
                <span class="detail-kpi-label">Current Density</span>
                <span class="detail-kpi-value">${candidate.winding.currentDensityAperMm2.toFixed(2)} A/mm²</span>
            </div>
            <div class="detail-kpi">
                <span class="detail-kpi-label">Turns / Gap</span>
                <span class="detail-kpi-value">${candidate.turnsAndGap.turns}t, ${candidate.turnsAndGap.gapMm.toFixed(2)}mm</span>
            </div>
        </div>
        <p class="detail-winding-line">Winding: <strong>${candidate.winding.wireDescription}</strong> &nbsp; ${acLossRiskChip(candidate.acLossRisk)}</p>
    `;

    // One-line status, not a restatement of any check - the list below is
    // the single place every check (and, for a failure, its real reason)
    // is actually explained. Tier comes from the real backend classification -
    // only "Recommended" ever renders a chip here (see recommendationTierChip);
    // the real fact behind an unshown tier (some checks rest on a Phase 1
    // default assumption rather than measured data) is spelled out in words
    // instead, without naming the tier.
    const tierChip = recommendationTierChip(candidate.recommendation.tier);
    const statusLine = candidate.rejectionReasons.length
        ? `<div class="detail-status detail-status-fail">Rejected — ${candidate.rejectionReasons.length} of ${candidate.validations.length} checks failed</div>`
        : `<div class="detail-status detail-status-pass">${tierChip ? tierChip + " — " : ""}${passCount} of ${candidate.validations.length} applicable checks passed${notEvalCount ? `, ${notEvalCount} not evaluated` : ""}${preliminaryChecks.length ? `, ${preliminaryChecks.length} check${preliminaryChecks.length > 1 ? "s" : ""} based on a Phase 1 default assumption` : ""}</div>`;

    const rankingLine = candidate.rankingExplanation
        ? `<p class="detail-ranking-explanation">${candidate.recommendation.explanation}</p>`
        : "";

    return `
        ${kpis}
        ${statusLine}
        ${rankingLine}
        <ul class="validation-list">${renderValidationList(candidate.validations)}</ul>
        ${usedDefaultLimit ? '<p class="validation-footnote">* Phase 1 default limit, not a material-specific value</p>' : ""}
        ${renderSourcesDetail(candidate)}
        ${
            warnings.length
                ? `<details><summary>Missing-data warnings</summary><ul>${warnings.map((w) => `<li>${w}</li>`).join("")}</ul></details>`
                : ""
        }
    `;
}

function renderCandidateTable(result) {
    const table = document.getElementById("candidateTable");
    if (!table) return;

    // Fixed order - candidateRows() already returns passing candidates
    // ranked by the backend (lowest total loss first), then rejected. Since
    // the tool always names one specific recommended candidate, letting a
    // reader re-sort by another column can't change which one that is -
    // it would just be a different view of the same fixed recommendation,
    // so there's no sort control here.
    let rows = candidateRows(result);
    if (currentFilter === "passing") rows = rows.filter((r) => r.passed);
    if (currentFilter === "rejected") rows = rows.filter((r) => !r.passed);
    if (currentShapeFilter !== "all") rows = rows.filter((r) => r.candidate.core.coreShape === currentShapeFilter);

    if (rows.length === 0) {
        table.innerHTML = `<tbody><tr><td class="table-empty">No candidates match this filter.</td></tr></tbody>`;
        return;
    }

    const headers = [
        { label: "Status" },
        { label: "Core" },
        { label: "Shape" },
        { label: "Material" },
        { label: "Turns", numeric: true },
        { label: "Gap (mm)", numeric: true },
        { label: "Calc L (µH)", numeric: true },
        { label: "Error %", numeric: true },
        { label: "Fill %", numeric: true },
        { label: "DC Copper Loss", numeric: true },
        { label: "Core Loss", numeric: true },
        { label: "Known Evaluated Loss", numeric: true },
    ];

    const headerHtml = headers.map((h) => `<th${h.numeric ? ' class="numeric"' : ""}>${h.label}</th>`).join("");

    const bodyHtml = rows
        .map((row, index) => {
            const c = row.candidate;
            const tier = c.recommendation.tier;
            const badge = row.passed
                ? `<span class="chip chip-pass">PASS</span>${recommendationTierChip(tier)}`
                : '<span class="chip chip-fail">REJECT</span>';
            const rowClass = ["candidate-row", row.passed ? "row-pass" : "row-reject", tier === "Phase1Recommended" ? "row-recommended" : ""]
                .filter(Boolean)
                .join(" ");
            return `
                <tr class="${rowClass}" data-row-index="${index}">
                    <td>${badge}</td>
                    <td>${c.core.partNumber}</td>
                    <td>${shapeCellLabel(c.core)}</td>
                    <td>${c.material.materialFamily}</td>
                    <td class="numeric">${c.turnsAndGap.turns}</td>
                    <td class="numeric">${c.turnsAndGap.gapMm.toFixed(2)}</td>
                    <td class="numeric">${c.turnsAndGap.calculatedInductanceUH.toFixed(2)}</td>
                    <td class="numeric">${c.turnsAndGap.inductanceErrorPercent.toFixed(2)}</td>
                    <td class="numeric">${(c.winding.fillFactor * 100).toFixed(1)}</td>
                    <td class="numeric">${formatLossCell(c.losses.copperLossStatus, c.losses.copperLossW)}</td>
                    <td class="numeric">${formatLossCell(c.losses.coreLossStatus, c.losses.coreLossW)}</td>
                    <td class="numeric">${formatTotalLossCell(c)}</td>
                </tr>
            `;
        })
        .join("");

    table.innerHTML = `<thead><tr>${headerHtml}</tr></thead><tbody>${bodyHtml}</tbody>`;

    table.querySelectorAll(".candidate-row").forEach((tr) => {
        tr.addEventListener("click", () => {
            const row = rows[Number(tr.dataset.rowIndex)];
            if (row) openCandidateSidePanel(row.candidate, row.passed);
        });
    });
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

function renderFilterChips(result) {
    const element = document.getElementById("filterChips");
    if (!element) return;

    const counts = {
        all: result.candidates.length + result.rejectedCandidates.length,
        passing: result.candidates.length,
        rejected: result.rejectedCandidates.length,
    };
    const labels = { all: "All", passing: "Passing", rejected: "Rejected" };

    element.innerHTML = Object.keys(labels)
        .map(
            (key) =>
                `<button type="button" class="filter-chip${key === currentFilter ? " active" : ""}" data-filter="${key}">${labels[key]} (${counts[key]})</button>`
        )
        .join("");

    element.querySelectorAll(".filter-chip").forEach((button) => {
        button.addEventListener("click", () => {
            currentFilter = button.dataset.filter;
            renderFilterChips(lastResult);
            renderCandidateTable(lastResult);
        });
    });

    renderShapeFilter(result);
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
        renderCandidateTable(lastResult);
    };
}

async function generateRecommendation() {
    clearResults();
    setStatus("Generating recommendation...");

    const button = document.getElementById("generateButton");
    button.disabled = true;
    button.textContent = "Generating...";

    try {
        const payload = buildPayload();
        const result = await postRequest(ENDPOINT, payload);
        lastResult = result;
        currentFilter = "all";

        console.log("DesignRecommendation", result);

        renderRules(result.activeRules);
        renderVersions(result.versions);
        renderFeasibility(result);
        renderTriageStrip(result);
        renderFilterChips(result);
        renderCandidateTable(result);

        // The ranking policy only means anything once there's something to
        // rank - showing it before any run just reads as unexplained noise.
        const rankingNote = document.getElementById("rankingNote");
        if (rankingNote) rankingNote.hidden = result.candidates.length + result.rejectedCandidates.length === 0;

        setStatus(
            result.status === "ok"
                ? "Recommendation generated successfully."
                : "No feasible design - see feasibility panel."
        );
    } catch (error) {
        console.error(error);
        setStatus(error.message, true);
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

    document.querySelectorAll(".field-tabs").forEach(initFieldTabs);

    document.getElementById("sidePanelClose").addEventListener("click", closeCandidateSidePanel);
    document.getElementById("sidePanelOverlay").addEventListener("click", closeCandidateSidePanel);
    document.addEventListener("keydown", (event) => {
        if (event.key === "Escape") closeCandidateSidePanel();
    });

    switchMode("direct");
});
