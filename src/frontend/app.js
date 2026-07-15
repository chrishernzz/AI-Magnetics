const endpoints = {
    material: "/material-selection",
    areaProduct: "/calculate",
    coreSelection: "/core-selection",
    turns: "/turns-calculation"
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

    const status =
        document.getElementById("statusMessage");

    if (!status) return;

    status.textContent = message;

    status.className =
        isError
            ? "status-message error"
            : "status-message";
}

function clearResults() {

    [
        "material",
        "core",
        "turns",
        "details",
        "assistant",
        "designSummary"
    ]
    .forEach(id => {

        const element =
            document.getElementById(id);

        if (element) {
            element.innerHTML = "";
        }
    });
}

async function postRequest(endpoint, payload) {

    const response =
        await fetch(endpoint, {
            method: "POST",
            headers: {
                "Content-Type":
                    "application/json"
            },
            body:
                JSON.stringify(payload)
        });

    if (!response.ok) {
        throw new Error(
            `Server error ${response.status}`
        );
    }

    return await response.json();
}

function renderMaterial(result) {

    const element =
        document.getElementById("material");

    if (!element) return;

    element.innerHTML = `
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

    const element =
        document.getElementById("core");

    if (!element) return;

    element.innerHTML = `
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
                Material: ${result.material}
            </p>

            <p>
                μ: ${result.mu}
            </p>

            <p>
                AL: ${result.al} nH/T²
            </p>

            <p>
                Ae: ${result.ae} mm²
            </p>

            <p>
                Wa: ${result.wa} mm²
            </p>

            <p>
                Le: ${result.le} mm
            </p>

        </div>
    `;
}

function renderTurns(result) {

    const element =
        document.getElementById("turns");

    if (!element) {
        console.error(
            "Turns element not found"
        );
        return;
    }

    element.innerHTML = `
        <div class="result-block">

            <h3>
                Turns Recommendation
            </h3>

            <p>
                <strong>
                    ${result.turns} Turns
                </strong>
            </p>

            <p>
                Target Inductance:
                ${result.inductanceUH} µH
            </p>

            <p>
                Core AL:
                ${result.al} nH/T²
            </p>

        </div>
    `;
}

function renderDetails(areaProductResult) {

    const element =
        document.getElementById("details");

    if (!element) return;

    const energy =
        areaProductResult.energy * 1000.0;

    element.innerHTML = `
        <div class="result-block">

            <h3>
                Design Details
            </h3>

            <p>

                <strong>
                    Stored Energy:
                </strong>

                ${energy.toFixed(2)}
                mJ

            </p>

            <p>

                <strong>
                    Area Product:
                </strong>

                ${Number(
                    areaProductResult.areaProduct
                ).toExponential(2)}
                cm⁴

            </p>

        </div>
    `;
}

function renderDesignSummary(
    material,
    core,
    turns
) {

    const element =
        document.getElementById(
            "designSummary"
        );

    if (!element) return;

    element.innerHTML = `
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
                Turns
            </div>

            <div class="summary-value">
                ${turns.turns}
            </div>

        </div>

    `;
}

async function generateRecommendation() {

    clearResults();

    setStatus(
        "Generating recommendation..."
    );

    try {

        const payload =
            buildPayload();

        const material =
            await postRequest(
                endpoints.material,
                payload
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

        const turns =
            await postRequest(
                endpoints.turns,
                payload
            );

        console.log("Material", material);
        console.log("Core", core);
        console.log("Turns", turns);

        renderMaterial(material);
        renderCore(core);
        renderTurns(turns);
        renderDetails(areaProduct);

        renderDesignSummary(
            material,
            core,
            turns
        );

        const assistant =
            document.getElementById(
                "assistant"
            );

        if (assistant) {

            assistant.innerHTML = `
                <p>

                    Material:

                    <strong>
                        ${material.materialFamily}
                    </strong>

                </p>

                <p>

                    Core:

                    <strong>
                        ${core.partNumber}
                    </strong>

                </p>

                <p>

                    Estimated Turns:

                    <strong>
                        ${turns.turns}
                    </strong>

                </p>

            `;
        }

        setStatus(
            "Recommendation generated successfully."
        );
    }
    catch (error) {

        console.error(error);

        setStatus(
            error.message,
            true
        );
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