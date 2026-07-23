const ENDPOINT = "/inductor-design";
const TOPOLOGY_ENDPOINT = "/topology-design/buck";

let lastResult = null;
let currentFilter = "all"; // all | passing | rejected
let sortKey = "status";
let sortAscending = true;

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

function applyTopologyDerivedRequest(derived, vinUsedV) {
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
        rmsInput.placeholder = "derived from Buck converter";
    }

    const banner = document.getElementById("topologyDerivedBanner");
    if (banner) {
        banner.hidden = false;
        banner.innerHTML = `
            <p><strong>Derived from your Buck converter requirements</strong> at the worst-case input voltage, Vin = ${vinUsedV} V.</p>
            <p>L = ${derived.inductanceUH.toFixed(3)} µH · Ipeak = ${derived.peakCurrentA.toFixed(3)} A ·
               Iavg = ${derived.averageCurrentA.toFixed(3)} A (= Iout) · Ripple = ${derived.rippleCurrentPeakToPeakA.toFixed(3)} A p-p ·
               fsw = ${derived.switchingFreqKHz} kHz</p>
            <p>RMS current is derived downstream from Iavg + ripple (triangular-ripple assumption), the same as if entered directly.</p>
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
    ["feasibility", "triageStrip", "designSummary", "assistant"].forEach((id) => {
        const element = document.getElementById(id);
        if (element) element.innerHTML = "";
    });
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
        element.innerHTML = `
            <div class="result-block">
                <h3>No Feasible Design</h3>
                <p>${result.message}</p>
                ${extra}
            </div>
        `;
    } else {
        element.innerHTML = `
            <div class="result-block">
                <h3>Status: OK</h3>
                <p>${result.message}</p>
            </div>
        `;
    }
}

// Tallies why candidates were rejected so an engineer can triage a batch
// of failures at a glance instead of opening each one individually.
function renderTriageStrip(result) {
    const element = document.getElementById("triageStrip");
    if (!element) return;

    const total = result.candidates.length + result.rejectedCandidates.length;
    if (total === 0) {
        element.innerHTML = "";
        return;
    }

    const tally = new Map();
    result.rejectedCandidates.forEach((candidate) => {
        candidate.rejectionReasons.forEach((reason) => {
            tally.set(reason.checkName, (tally.get(reason.checkName) || 0) + 1);
        });
    });

    const chips = [...tally.entries()]
        .sort((a, b) => b[1] - a[1])
        .map(([name, count]) => `<span class="tally-chip">${name} × ${count}</span>`)
        .join("");

    element.innerHTML = `
        <div class="triage-counts">
            <span class="triage-count triage-total"><strong>${total}</strong> evaluated</span>
            <span class="triage-count triage-pass"><strong>${result.candidates.length}</strong> passing</span>
            <span class="triage-count triage-fail"><strong>${result.rejectedCandidates.length}</strong> rejected</span>
        </div>
        ${chips ? `<div class="triage-tally"><span class="triage-tally-label">Why they were rejected:</span> ${chips}</div>` : ""}
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

// Mirrors InductorDesignService.cpp's totalKnownLossW()/hasAnyLossData() exactly -
// this is the same number the backend actually ranks passing candidates by, so the
// table's "Total Loss" column has to compute it the same way or the numbers and the
// row order would silently disagree.
function hasAnyLossData(candidate) {
    return candidate.losses.copperLossStatus === "Evaluated" || candidate.losses.coreLossStatus === "Evaluated";
}

function totalKnownLossW(candidate) {
    let loss = 0;
    if (candidate.losses.copperLossStatus === "Evaluated") loss += candidate.losses.copperLossW;
    if (candidate.losses.coreLossStatus === "Evaluated") loss += candidate.losses.coreLossW;
    return loss;
}

function formatTotalLossCell(candidate) {
    if (!hasAnyLossData(candidate)) return '<span class="cell-muted">not evaluated</span>';
    return `${totalKnownLossW(candidate).toFixed(3)} W`;
}

function renderValidationList(validations) {
    return validations
        .map((v) => {
            const notEvaluated = v.status === "NotEvaluated";
            const label = notEvaluated ? "NOT EVALUATED" : v.passed ? "PASS" : "FAIL";
            const cssClass = notEvaluated ? "check-warn" : v.passed ? "check-pass" : "check-fail";
            return `
        <li class="${cssClass}">
            ${label} - ${v.checkName}: ${v.calculatedValue.toFixed(3)} ${v.unit}
            (limit ${v.limitValue.toFixed(3)} ${v.unit})${v.usedDefaultLimit ? " [Phase 1 default limit]" : ""}
            <div class="check-explanation">${v.explanation}</div>
        </li>
    `;
        })
        .join("");
}

function candidateRows(result) {
    const passing = result.candidates.map((c) => ({ candidate: c, passed: true }));
    const rejected = result.rejectedCandidates.map((c) => ({ candidate: c, passed: false }));
    return passing.concat(rejected);
}

function sortValue(row, key) {
    const c = row.candidate;
    switch (key) {
        case "status":
            return row.passed ? 0 : 1;
        case "core":
            return c.core.partNumber;
        case "material":
            return c.material.materialFamily;
        case "turns":
            return c.turnsAndGap.turns;
        case "gap":
            return c.turnsAndGap.gapMm;
        case "calcL":
            return c.turnsAndGap.calculatedInductanceUH;
        case "error":
            return Math.abs(c.turnsAndGap.inductanceErrorPercent);
        case "fill":
            return c.winding.fillFactor;
        case "cuLoss":
            return c.losses.copperLossStatus === "Evaluated" ? c.losses.copperLossW : -1;
        case "coreLoss":
            return c.losses.coreLossStatus === "Evaluated" ? c.losses.coreLossW : -1;
        case "totalLoss":
            // matches the backend's own ranking metric - see totalKnownLossW() above
            return hasAnyLossData(c) ? totalKnownLossW(c) : -1;
        default:
            return 0;
    }
}

function renderCandidateDetail(candidate) {
    const warnings = candidate.material.missingDataWarnings
        .concat(candidate.winding.missingData || [])
        .concat(candidate.losses.missingData || []);

    return `
        <p>Winding: <strong>${candidate.winding.wireDescription}</strong>,
           fill factor ${(candidate.winding.fillFactor * 100).toFixed(1)}%,
           current density ${candidate.winding.currentDensityAperMm2.toFixed(2)} A/mm²</p>
        <p>Core loss: <strong>${formatLoss(candidate.losses.coreLossStatus, candidate.losses.coreLossW)}</strong></p>
        <details>
            <summary>Validation checks (${candidate.validations.length})</summary>
            <ul class="check-list">${renderValidationList(candidate.validations)}</ul>
        </details>
        ${
            candidate.rejectionReasons.length
                ? `<details open><summary>Rejection reasons</summary><ul class="check-list">${candidate.rejectionReasons
                      .map((r) => `<li class="check-fail">${r.checkName}: ${r.explanation}</li>`)
                      .join("")}</ul></details>`
                : ""
        }
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

    let rows = candidateRows(result);
    if (currentFilter === "passing") rows = rows.filter((r) => r.passed);
    if (currentFilter === "rejected") rows = rows.filter((r) => !r.passed);

    rows.sort((a, b) => {
        const av = sortValue(a, sortKey);
        const bv = sortValue(b, sortKey);
        const cmp = typeof av === "string" ? av.localeCompare(bv) : av - bv;
        return sortAscending ? cmp : -cmp;
    });

    if (rows.length === 0) {
        table.innerHTML = `<tbody><tr><td class="table-empty">No candidates match this filter.</td></tr></tbody>`;
        return;
    }

    const headers = [
        { key: "status", label: "Status" },
        { key: "core", label: "Core" },
        { key: "material", label: "Material" },
        { key: "turns", label: "Turns", numeric: true },
        { key: "gap", label: "Gap (mm)", numeric: true },
        { key: "calcL", label: "Calc L (µH)", numeric: true },
        { key: "error", label: "Error %", numeric: true },
        { key: "fill", label: "Fill %", numeric: true },
        { key: "cuLoss", label: "Cu Loss", numeric: true },
        { key: "coreLoss", label: "Core Loss", numeric: true },
        { key: "totalLoss", label: "Total Loss", numeric: true },
    ];

    const headerHtml = headers
        .map((h) => {
            const active = h.key === sortKey ? (sortAscending ? " ▲" : " ▼") : "";
            return `<th data-sort-key="${h.key}"${h.numeric ? ' class="numeric"' : ""}>${h.label}${active}</th>`;
        })
        .join("");

    const bodyHtml = rows
        .map((row, index) => {
            const c = row.candidate;
            const badge = row.passed
                ? '<span class="chip chip-pass">PASS</span>'
                : '<span class="chip chip-fail">REJECT</span>';
            return `
                <tr class="candidate-row ${row.passed ? "row-pass" : "row-reject"}" data-row-index="${index}">
                    <td>${badge}</td>
                    <td>${c.core.partNumber}</td>
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
                <tr class="detail-row" data-detail-index="${index}" hidden>
                    <td colspan="11">${renderCandidateDetail(c)}</td>
                </tr>
            `;
        })
        .join("");

    table.innerHTML = `<thead><tr>${headerHtml}</tr></thead><tbody>${bodyHtml}</tbody>`;

    table.querySelectorAll("th[data-sort-key]").forEach((th) => {
        th.addEventListener("click", () => {
            const key = th.dataset.sortKey;
            if (sortKey === key) {
                sortAscending = !sortAscending;
            } else {
                sortKey = key;
                sortAscending = true;
            }
            renderCandidateTable(lastResult);
        });
    });

    table.querySelectorAll(".candidate-row").forEach((tr) => {
        tr.addEventListener("click", () => {
            const detailRow = table.querySelector(`.detail-row[data-detail-index="${tr.dataset.rowIndex}"]`);
            if (detailRow) detailRow.hidden = !detailRow.hidden;
        });
    });
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
}

function renderDesignSummary(result) {
    const element = document.getElementById("designSummary");
    if (!element) return;

    if (result.candidates.length === 0) {
        element.innerHTML = `<p>No passing candidate - see the triage panel and candidate table for why.</p>`;
        return;
    }

    const best = result.candidates[0];
    const rankingBasis = hasAnyLossData(best)
        ? "lowest real total loss among passing candidates"
        : "smallest area product - no candidate in this run had real loss data to rank by";
    element.innerHTML = `
        <div class="summary-metric">
            <div class="summary-label">Recommended Material</div>
            <div class="summary-value">${best.material.materialFamily}</div>
        </div>
        <div class="summary-metric">
            <div class="summary-label">Recommended Core</div>
            <div class="summary-value">${best.core.partNumber}</div>
        </div>
        <div class="summary-metric">
            <div class="summary-label">Turns / Gap</div>
            <div class="summary-value">${best.turnsAndGap.turns} turns, ${best.turnsAndGap.gapMm.toFixed(2)} mm</div>
        </div>
        <div class="summary-metric">
            <div class="summary-label">Total Loss (Cu + Core)</div>
            <div class="summary-value">${formatTotalLossCell(best)}</div>
        </div>
        <p class="ranking-note">Ranked #1 by: ${rankingBasis}.</p>
    `;
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
        sortKey = "status";
        sortAscending = true;

        console.log("DesignRecommendation", result);

        renderRules(result.activeRules);
        renderFeasibility(result);
        renderTriageStrip(result);
        renderFilterChips(result);
        renderCandidateTable(result);
        renderDesignSummary(result);

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
    switchMode("direct");
});
