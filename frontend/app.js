// Served two ways: by the FastAPI app itself (same origin - relative path
// works) or as static files on GitHub Pages (different origin than the
// Vercel-hosted API, so it needs the full URL). Update API_BASE if the
// Vercel deployment moves to a different domain.
const API_BASE = window.location.hostname.endsWith("github.io")
    ? "https://ai-magnetics-1jg2zwwq7-christian-hernandezs-projects-56a515d7.vercel.app"
    : "";
const ENDPOINT = `${API_BASE}/inductor-design`;

let lastResult = null;
let currentFilter = "all"; // all | passing | rejected
let sortKey = "status";
let sortAscending = true;

function buildPayload() {
    const toleranceRaw = document.getElementById("tolerance").value;

    const payload = {
        inductanceUH: Number(document.getElementById("inductance").value),
        peakCurrentA: Number(document.getElementById("current").value),
        rmsCurrentA: Number(document.getElementById("rmsCurrent").value),
        switchingFreqKHz: Number(document.getElementById("frequency").value),
        ambientTemperatureC: Number(document.getElementById("ambientTemp").value),
        allowableTempRiseC: Number(document.getElementById("tempRise").value),
    };

    if (toleranceRaw !== "") {
        payload.inductanceTolerancePercent = Number(toleranceRaw);
    }

    return payload;
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
                </tr>
                <tr class="detail-row" data-detail-index="${index}" hidden>
                    <td colspan="9">${renderCandidateDetail(c)}</td>
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
});
