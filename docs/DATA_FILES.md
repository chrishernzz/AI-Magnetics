# Data Files Guide
This document explains the structure of CSV files in `data/` and how to add new entries.

---
## cores.csv
**Purpose:** Database of available inductor cores (part numbers, geometry, materials).
**Location:** `data/cores.csv`
**Used by:** `src/data/CoreDatabase.cpp`
### Fields
| Field | Unit | Example | Description |
|-------|------|---------|-------------|
| PartNumber | — | 0077440A7 | Supplier part number (unique key) |
| Material | — | Kool Mu | Material type (must match materials.csv) |
| Ac | cm² | 1.2 | Cross-sectional area of core |
| Wa | cm² | 2.8 | Window area available for copper winding |
| Le | cm | 8.5 | Magnetic path length |
| MuR | — | 26 | Relative permeability (µᵣ) |
| AL | nH/100T | 125 | Inductance index (nH per 100 turns) |
| VolumeCore | cm³ | 10.2 | Core volume (for loss calculation) |
| CostUSD | $ | 2.50 | Unit cost (optional; for cost optimization) |
| Supplier | — | IntelliPower | Vendor name |
| Link | URL | https://... | Datasheet or product link |
### How to Add a New Core
1. **Get datasheet from supplier** — Contains Ac, Wa, Le, AL, material
2. **Open data/cores.csv** in a spreadsheet or text editor
3. **Add new row:**
0077441A7,Kool Mu,1.5,3.2,9.0,26,195,12.0,3.00,IntelliPower,https://...

4. **Verify:**
- Material name matches exactly (case-sensitive) with materials.csv
- AL value (nH/100T) is from datasheet
- Ac × Wa product is reasonable (typical: 3–6 cm⁴ for buck inductors)
5. **Save and rebuild** — `magnetics_server` will load new data on next run

### Calculation Tips
If datasheet only provides **AL value** (most common):
- Ap = Ac × Wa (core size product)
- Calculate from datasheet: Ac, Wa, Le are usually provided
- AL = inductance per 100-turn winding (e.g., 125 nH/100T)
If datasheet provides **µ instead of AL**:
- AL ≈ 0.4π × µ₀ × µᵣ × (Ac / Le) × 10⁹ (nH/100T)
- Where: µ₀ = 4π × 10⁻⁷, Ac in cm², Le in cm

---
## materials.csv
**Purpose:** Database of magnetic materials and their properties.
**Location:** `data/materials.csv`
**Used by:** `src/data/Materials.cpp`, `MaterialSelection.cpp`, loss calculators
### Fields
| Field | Unit | Example | Description |
|-------|------|---------|-------------|
| Name | — | Kool Mu | Material identifier (unique; must match cores.csv) |
| MuOpt | — | 26 | Optimal permeability for frequency range |
| MinFrequencyHz | Hz | 50000 | Minimum operating frequency (50 kHz = 50000 Hz) |
| MaxFrequencyHz | Hz | 250000 | Maximum operating frequency |
| Reason | — | Balanced performance... | Why this material is good in its range |
| Alternatives | — | Ferrite\|Powder Iron | Comma-separated alternatives (pipe-separated list) |
| BmaxT | T | 1.0 | Maximum flux density (saturation limit) |
| CuLossFactor | — | 1.15 | Multiplier for copper loss (accounts for skin effect, proximity) |
### Frequency Ranges (Key Rules)
- **Powder Iron:** 1–100 kHz (great for low freq, high current)
- **Kool Mu:** 50–250 kHz (balanced; low loss)
- **Ferrite 3C90:** 50–250 kHz (low loss but expensive)
- **High Frequency Ferrite:** 250 kHz–10 MHz (for high-speed switching)
**Selection Logic:**
- Tool picks material whose frequency range contains the requested frequency
- Prefers material with lowest loss in that range
### How to Add a New Material
1. **Get datasheet from supplier** — Contains µ, Bmax, loss curves
2. **Open data/materials.csv**
3. **Add new row:**
Powder Iron M,60,1000,100000,Affordable mid-frequency core,Kool Mu|Ferrite,1.6,1.0

4. **Verify:**
- Frequency range covers your use case
- BmaxT is from datasheet (saturation flux density)
- CuLossFactor accounts for skin effect at max frequency (typically 1.0–1.5)
5. **Save and rebuild**
### CuLossFactor Explanation
At higher frequencies, AC resistance increases due to **skin effect**:
- Skin depth: δ = 1 / √(πfμσ)
- At 1 MHz in copper: δ ≈ 0.066 mm
- Thin wires (< 0.1 mm) only conduct on surface → higher resistance
**CuLossFactor** multiplier accounts for this:
- 1.0 = No skin effect (DC-like resistance)
- 1.15 = 15% higher loss than DC due to AC effects
- 1.3 = 30% higher loss (common for very high frequency)

---
## reference_designs.csv
**Purpose:** Example designs for testing and documentation.
**Location:** `data/reference_designs.csv`
**Used by:** Documentation, manual testing
### Fields
| Field | Example | Description |
|-------|---------|-------------|
| DesignName | USB_Charger_5W | Descriptive name |
| L_uH | 10 | Inductance (µH) |
| Ipk_A | 1.5 | Peak current (A) |
| Frequency_kHz | 500 | Switching frequency (kHz) |
| TempRise_C | 40 | Allowable temperature rise (°C) |
| ExpectedMaterial | Ferrite 3C90 | Predicted best material |
| ExpectedCore | 0077440A7 | Expected core part number |
| ExpectedLoss_W | 0.05 | Expected total loss (W) |
| Notes | Low-power example | Design context |
### How to Use
- **Validation:** Add your successful designs here for regression testing
- **Documentation:** Share example designs with users
- **Regression:** Compare future results against known-good designs

---
## test_scenarios.csv
**Purpose:** Test cases for validating the design algorithm.
**Location:** `data/test_scenarios.csv`
**Used by:** Automated tests, manual verification
### Fields
| Field | Example | Description |
|-------|---------|-------------|
| TestName | MaterialSelectionOK | Scenario identifier |
| Frequency_kHz | 100 | Input: switching frequency |
| L_uH | 250 | Input: inductance |
| Ipk_A | 2 | Input: peak current |
| TempRise_C | 40 | Input: allowable temperature rise |
| ExpectedStatus | PASS | Expected outcome (PASS/FAIL/WARN) |
| ExpectedMaterial | Kool Mu | Expected material selected |
| ExpectedAp_cm4 | 3.2 | Expected Ap value ±5% |
| ExpectedTurns | 18 | Expected turns count ±2 |
| Reason | Validates Ap formula | Why this test matters |
### How to Run Tests
1. **Manual:** Enter each scenario into the web UI and verify results match expected output
2. **Automated:** (Future) Parse test_scenarios.csv and run batch validation
### Adding Test Cases
When you discover a bug or edge case:
1. Add a row to test_scenarios.csv with the failure scenario
2. Fix the code
3. Re-run test; verify it now shows PASS
4. Keep the row in test_scenarios.csv for regression prevention

---
## CSV Format Rules
1. **Headers on row 1** — Required; column names are case-sensitive
2. **Pipe (|) for lists** — Use `Ferrite|Kool Mu|Powder Iron` for alternatives
3. **No extra spaces** — Trim whitespace around values
4. **Numbers without units** — Store `250` not `250µH`
5. **URLs in links** — Must start with `http://` or `https://`
6. **Encoding:** UTF-8 (save as UTF-8 in Excel/Google Sheets)

---
## Where Data Comes From
### cores.csv
- **Source:** Supplier datasheets (IntelliPower, Vishay, etc.)
- **Frequency:** Updated quarterly as new cores release
- **Validation:** Cross-check AL against calculated L = AL × (N/100)²
### materials.csv
- **Source:** Material vendor datasheets (Powder Iron, Ferrite, etc.)
- **Frequency:** Updated annually or when new materials adopted
- **Validation:** Measure core loss experimentally at key frequencies
### reference_designs.csv
- **Source:** AMETEK product specifications
- **Frequency:** Updated per new AMETEK products
- **Use:** Regression testing
### test_scenarios.csv
- **Source:** Manual edge-case discovery + AMETEK QA
- **Frequency:** Updated per bug fix
- **Use:** Prevent regression