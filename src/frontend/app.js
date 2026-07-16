const ENDPOINT = "/inductor-design";

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

function setStatus(message, isError = false) {
    const status = document.getElementById("statusMessage");
    if (!status) return;
    status.textContent = message;
    status.className = isError ? "status-message error" : "status-message";
}

function clearResults() {
    ["feasibility", "candidates", "rejected", "designSummary", "assistant"].forEach((id) => {
        const element = document.getElementById(id);
        if (element) element.innerHTML = "";
    });
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

function renderCandidate(candidate) {
    const warnings = candidate.material.missingDataWarnings.concat(candidate.winding.missingData, candidate.losses.missingData);

    return `
        <div class="result-block candidate-block">
            <h3>${candidate.core.partNumber} <span class="muted">(${candidate.material.materialFamily})</span></h3>
            <p>Turns: <strong>${candidate.turnsAndGap.turns}</strong>, Gap: <strong>${candidate.turnsAndGap.gapMm.toFixed(2)} mm</strong></p>
            <p>Calculated inductance: <strong>${candidate.turnsAndGap.calculatedInductanceUH.toFixed(2)} µH</strong>
               (error ${candidate.turnsAndGap.inductanceErrorPercent.toFixed(2)}%)</p>
            <p>Winding: <strong>${candidate.winding.wireDescription}</strong>,
               fill factor ${(candidate.winding.fillFactor * 100).toFixed(1)}%,
               current density ${candidate.winding.currentDensityAperMm2.toFixed(2)} A/mm²</p>
            <p>DC copper loss: <strong>${
                candidate.losses.copperLossStatus === "Evaluated"
                    ? candidate.losses.copperLossW.toFixed(3) + " W"
                    : "not evaluated"
            }</strong>,
               core loss: <strong>${candidate.losses.coreLossStatus === "Evaluated" ? candidate.losses.coreLossW.toFixed(3) + " W" : "not evaluated"}</strong></p>
            <details>
                <summary>Validation checks</summary>
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
        </div>
    `;
}

function renderCandidates(candidates) {
    const element = document.getElementById("candidates");
    if (!element) return;

    if (candidates.length === 0) {
        element.innerHTML = `<div class="result-block"><h3>Passing Candidates</h3><p>None.</p></div>`;
        return;
    }

    element.innerHTML = `<h3>Passing Candidates (${candidates.length})</h3>` + candidates.map(renderCandidate).join("");
}

function renderRejected(rejectedCandidates) {
    const element = document.getElementById("rejected");
    if (!element) return;

    if (rejectedCandidates.length === 0) {
        element.innerHTML = "";
        return;
    }

    element.innerHTML =
        `<h3>Rejected Candidates (${rejectedCandidates.length})</h3>` + rejectedCandidates.map(renderCandidate).join("");
}

function renderDesignSummary(result) {
    const element = document.getElementById("designSummary");
    if (!element) return;

    if (result.candidates.length === 0) {
        element.innerHTML = `<p>No passing candidate - see the feasibility panel and rejected candidates for why.</p>`;
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

    try {
        const payload = buildPayload();
        const result = await postRequest(ENDPOINT, payload);

        console.log("DesignRecommendation", result);

        renderRules(result.activeRules);
        renderFeasibility(result);
        renderCandidates(result.candidates);
        renderRejected(result.rejectedCandidates);
        renderDesignSummary(result);

        setStatus(
            result.status === "ok"
                ? "Recommendation generated successfully."
                : "No feasible design - see feasibility panel."
        );
    } catch (error) {
        console.error(error);
        setStatus(error.message, true);
    }
}

window.addEventListener("DOMContentLoaded", () => {
    document.getElementById("generateButton").addEventListener("click", generateRecommendation);
});
