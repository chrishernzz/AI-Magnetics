const endpoints = {
    material: "/material-selection",
    areaProduct: "/calculate",
    coreSelection: "/core-selection"
};

function buildPayload() {
    return {
        inductanceUH: Number(document.getElementById("inductance").value),
        peakCurrentA: Number(document.getElementById("current").value),
        switchingFreqKHz: Number(document.getElementById("frequency").value),
        allowableTempRiseC: Number(document.getElementById("tempRise").value)
    };
}

function setStatus(message, isError = false) {
    const status = document.getElementById("statusMessage");
    status.textContent = message;
    status.className = isError ? "status-message error" : "status-message";
}

function clearResults() {
    document.getElementById("material").innerHTML = "";
    document.getElementById("core").innerHTML = "";
    document.getElementById("details").innerHTML = "";
    document.getElementById("assistant").innerHTML = "";
    document.getElementById("designSummary").innerHTML = "";
}

async function postRequest(endpoint, payload) {
    const response = await fetch(endpoint, {
        method: "POST",
        headers: {
            "Content-Type": "application/json"
        },
        body: JSON.stringify(payload)
    });

    if (!response.ok) {
        throw new Error(
            `Server error ${response.status}`
        );
    }

    return await response.json();
}

function renderMaterial(result) {
    document.getElementById("material").innerHTML = `
        <div class="result-block">
            <h3>
                Material Recommendation
            </h3>
            <p>
                <strong>
                    ${result.materialFamily}
                </strong>
            </p>
            <p>
                Reference μ:
                ${result.muOpt}
            </p>
            <p>
                ${result.reason}
            </p>
            <p>
                <em>
                    Alternatives:
                </em>
                ${result.alternatives}
            </p>

        </div>
    `;
}

function renderCore(result) {

    document.getElementById("core").innerHTML = `
        <div class="result-block">

            <h3>
                Core Recommendation
            </h3>

            <p>
                <strong>
                    ${result.partNumber}
                </strong>
            </p>

            <p>
                Material:
                ${result.material}
            </p>

            <p>
                μ:
                ${result.mu}
            </p>

            <p>
                AL:
                ${result.al} nH/T²
            </p>

            <p>
                Ae:
                ${result.ae} mm²
            </p>

            <p>
                Wa:
                ${result.wa} mm²
            </p>

            <p>
                Le:
                ${result.le} mm
            </p>

        </div>
    `;
}

function renderDetails(areaProductResult) {

    const energy =
        areaProductResult.energy * 1000.0;

    document.getElementById("details").innerHTML = `
        <div class="result-block">

            <h3>
                Design Details
            </h3>

            <p>
                <strong>
                    Stored Energy:
                </strong>

                ${energy.toFixed(2)} mJ
            </p>

            <p>
                <strong>
                    Area Product (Ap):
                </strong>

                ${Number(
                    areaProductResult.areaProduct
                ).toExponential(2)}
                cm⁴
            </p>

            <p>
                <strong>
                    Allowed Temp Rise:
                </strong>

                ${buildPayload().allowableTempRiseC}
                °C
            </p>

        </div>
    `;
}

function renderDesignSummary(
    material,
    core
) {
    document.getElementById(
        "designSummary"
    ).innerHTML = `
        <div class="summary-metric">

            <div class="summary-label">
                Recommended Material
            </div>

            <div class="summary-value">
                ${material.materialFamily}
            </div>

        </div>

        <div class="summary-metric">

            <div class="summary-label">
                Recommended Core
            </div>

            <div class="summary-value">
                ${core.partNumber}
            </div>

        </div>

        <div class="summary-metric">

            <div class="summary-label">
                Next Step
            </div>

            <div class="summary-value">
                Turns & Loss Design
            </div>

        </div>
    `;
}

async function generateRecommendation() {

    clearResults();

    setStatus(
        "Generating recommendation...",
        false
    );

    try {

        const payload =
            buildPayload();

        const material =
            await postRequest(
                endpoints.material,
                payload
            );

        console.log(
            "MATERIAL:",
            material
        );

        const areaProduct =
            await postRequest(
                endpoints.areaProduct,
                payload
            );

        const core =
            await postRequest(
                endpoints.coreSelection,
                payload
            );

        renderMaterial(
            material
        );

        renderCore(
            core
        );

        renderDetails(
            areaProduct
        );

        renderDesignSummary(
            material,
            core
        );

        document.getElementById(
            "assistant"
        ).innerHTML = `
            <p>
                AIMagnetics selected
                <strong>
                    ${material.materialFamily}
                </strong>
                as the preferred material.
            </p>

            <p>
                Core
                <strong>
                    ${core.partNumber}
                </strong>
                was recommended from the
                core database.
            </p>

            <p>
                Next stage:
                turns calculation,
                wire selection,
                and loss analysis.
            </p>
        `;

        setStatus(
            "Recommendation generated successfully."
        );
    }
    catch (error) {

        console.error(error);

        setStatus(
            `Error: ${error.message}`,
            true
        );

        document.getElementById(
            "assistant"
        ).innerHTML = `
            <div class="error-block">

                <p>
                    Unable to generate recommendation.
                </p>

                <p>
                    ${error.message}
                </p>

            </div>
        `;
    }
}

window.addEventListener(
    "DOMContentLoaded",
    () => {

        document
            .getElementById(
                "generateButton"
            )
            .addEventListener(
                "click",
                generateRecommendation
            );
    }
);