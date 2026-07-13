AIMagnetics Test Case: Heavy-Duty Powder Iron Design
Test 3: Heavy-Duty Design (Primary Validation Test)
This is the most important test. It validates that:

✅ Material selection picks the right material for frequency
✅ Area Product calculation is correct
✅ Core filtering respects material recommendations
✅ Energy calculation works (not 0.00 mJ)
Input Values
Enter these exact values in the browser form:

Inductance (L): 470 µH
Peak Current (Ipk): 10 A
Switching Frequency: 80 kHz
Temperature Rise: 50 °C

Expected Output
After clicking "Generate Recommendation", you should see:

Material Recommendation: Powder Iron
Core Recommendation: 0055500F or 0077443
μ: 90 or 26
AL: 40 or 59 nH/T²
Ae: 249 or 319 mm²
Wa: 399 or 599 mm²
Le: 161 or 135 mm

Stored Energy: 23.5 mJ (NOT 0.00!)
Area Product (Ap): 9.79 cm⁴ (approx)

Why These Results? (The Complete Workflow)
STAGE 1: Material Selection
Question: Which material is best for 80 kHz?

Your Material Database:

Powder Iron: 1 kHz to 100 kHz ← 80 kHz IS HERE ✅
Kool Mu: 50 kHz to 250 kHz ← 80 kHz also here, but less optimal
Ferrite 3C90: 50 kHz to 250 kHz ← Not ideal at 80 kHz
High Frequency: 250 kHz to 10 MHz ← Too high

Decision: At 80 kHz, Powder Iron is the best choice (center of its range)

Why? Lower core loss than Kool Mu at this frequency
More saturation margin than Ferrite
Perfect for heavy-duty applications
System Output: Material Recommendation: Powder Iron ✅

STAGE 2: Area Product Calculation
Question: How physically large must the core be to safely handle 10A?

The Math:

Step 1: Calculate stored energy

E = 0.5 × L × Ipk²
E = 0.5 × 470×10⁻⁶ H × (10 A)²
E = 0.5 × 470×10⁻⁶ × 100
E = 23.5 × 10⁻³ J = 23.5 mJ

Meaning: Your inductor will store 23.5 millijoules of energy. This is SIGNIFICANT power (not a tiny signal)

Step 2: Calculate Area Product requirement

Ap = (2 × E_max × 10⁴) / (Ku × Bmax × J)

Where:
Ku = 0.4 (window utilization: 40% of core window has copper)
Bmax = 0.30 T (max flux density, your tool's default)
J = 400 A/cm² (current density, your tool's default)

Ap = (2 × 23.5×10⁻³ × 10⁴) / (0.4 × 0.30 × 400)
Ap = (470) / (48)
Ap = 9.79 cm⁴

Meaning: The core must have an area product of at least 9.79 cm⁴ to safely store this energy without overheating.

Safety margin: Multiply by 0.95 to add margin: 9.79 × 0.95 = 9.30 cm⁴ minimum

System Output:
Stored Energy: 23.5 mJ ✅
Area Product (Ap): 9.79 cm⁴ ✅

STAGE 3: Core Database Filtering
Question: Which cores in the database meet the Ap requirement?

Your Core Database (simplified):

Core Material Ae(mm²) Wa(mm²) Ap(cm⁴) Status
0077440A7 Kool Mu 199 427 8.50 ❌ TOO SMALL (8.50 < 9.30)
0054035 Kool Mu 46 33 0.15 ❌ TOO SMALL
0077438 Kool Mu 119 256 3.05 ❌ TOO SMALL
0055500 Powder Iron 74 58 0.43 ❌ TOO SMALL
0077439 Kool Mu 159 344 5.47 ❌ TOO SMALL
0077437 Kool Mu 79 171 1.35 ❌ TOO SMALL
0054044 Kool Mu 57 68 0.39 ❌ TOO SMALL
0055610 Powder Iron 159 299 4.75 ❌ TOO SMALL (4.75 < 9.30)
0077441 Kool Mu 279 600 16.74 ✅ PASS (but wrong material!)
0054035L Kool Mu 76 62 0.47 ❌ TOO SMALL
0055601 Powder Iron 249 316 7.87 ❌ TOO SMALL (7.87 < 9.30)
0077440 Kool Mu 159 271 4.31 ❌ TOO SMALL
0054003 Kool Mu 34 23 0.08 ❌ TOO SMALL
0055500F Powder Iron 249 399 9.95 ✅ PASS ✅ Powder Iron!
0077442 Kool Mu 359 800 28.72 ✅ PASS (but wrong material!)
0077443 Powder Iron 319 599 19.11 ✅ PASS ✅ Powder Iron!

Step 1: Filter by Ap requirement (≥ 9.30 cm⁴)

Candidates: 0077441 (Kool Mu), 0055500F (Powder Iron), 0077442 (Kool Mu), 0077443 (Powder Iron)
Step 2: Filter by recommended material (Powder Iron only!)

This is KEY: Material selection said "Powder Iron"
Only keep cores that match: 0055500F and 0077443
Remove 0077441 and 0077442 (they're Kool Mu, not Powder Iron)
Step 3: Pick best by minimizing loss

Loss Heuristic: loss = I² / (Ae × Wa × 0.01)

0055500F: loss = 100 / (249 × 399 × 0.01) = 100 / 993 = 0.1007
0077443: loss = 100 / (319 × 599 × 0.01) = 100 / 1910 = 0.0523 ← LOWER!

Winner: 0077443 (lower loss = cooler = safer design)

System Output:
Core Recommendation: 0077443 (or 0055500F is acceptable)
Material: Powder Iron ✅
Ae: 319 mm²
Wa: 599 mm²

What This Means in Plain English
Your Design:

You're building a 470 µH inductor that will carry 10 Amps
It switches at 80 kHz (moderate frequency)
You allow up to 50°C temperature rise (lots of headroom)
What The Tool Decided:

"Use Powder Iron" → Best material for 80 kHz heavy-duty applications
"You need a core with Ap ≥ 9.79 cm⁴" → Core must be reasonably large (10A is a lot!)
"Best core is 0077443" → Large Powder Iron core that:
Is physically big (Ae=319, Wa=599)
Handles the power without overheating
Has lower loss than alternatives
Respects the material selection
Real-World Analogy:
"You're hauling heavy cargo (10A) over a medium-distance road (80 kHz). You need a Diesel truck (Powder Iron), and the largest one (0077443) that's efficient enough for this job."

How to Verify Results
In Browser:
✅ Material shows: "Powder Iron" (not Kool Mu!)
✅ Core shows: 0055500F or 0077443 (both are Powder Iron and viable)
✅ Energy shows: ~23.5 mJ (NOT 0.00!)
✅ Ap shows: ~9.79 cm⁴ (NOT 0 or tiny value)
In Terminal (Debug Output):
You should see:

=== CORE SELECTION DEBUG ===
Input Ap requirement: 9.79167 cm⁴
Recommended material: Powder Iron

Core 0077440A7 (Kool Mu): Ap=8.4973 → FAIL
...
Core 0055500F (Powder Iron): Ap=9.95 → PASS
...
Core 0077443 (Powder Iron): Ap=19.1081 → PASS

Total candidates: 2
Filtered by material "Powder Iron": 2 cores

Loss comparison:
0055500F: loss=0.100653
0077443: loss=0.0523338 ← NEW BEST

Selected: 0077443 (Powder Iron)
Troubleshooting
Issue	Cause	Fix
Material is "Kool Mu" (wrong!)	Frequency range in materials.csv is wrong	Check: Powder Iron should be 1000-100000 Hz
Energy shows 0.00 mJ	app.js dividing instead of multiplying	Change / to * on line 75
Core is always 0077441 (Kool Mu)	Core not filtering by material	Check CoreSelection.cpp has material filtering logic
Ap shows 0 or wrong value	Energy calculation failed	Check AreaProduct.cpp formula
Core doesn't change between tests	Database not reloading	Recompile and restart server
Summary
Test Input: 470µH, 10A, 80kHz, 50°C

Expected Output:

Material: Powder Iron ✅
Core: 0055500F or 0077443 ✅
Energy: 23.5 mJ ✅
Ap: 9.79 cm⁴ ✅
Validation: ✅ If you get these results, your design engine is working perfectly!

Technical Definitions (Quick Reference)
Term	Means	Units
L (Inductance)	Energy storage capacity	µH (microhenries)
Ipk (Peak Current)	Maximum current through inductor	A (amperes)
Frequency	How fast circuit switches	kHz (kilohertz)
ΔT (Temp Rise)	Allowed temperature increase	°C (degrees Celsius)
Ae	Core cross-section area	mm²
Wa	Window area for copper wire	mm²
Ap (Area Product)	Size requirement (Ae × Wa)	cm⁴
E (Stored Energy)	Power being handled	mJ (millijoules)
Powder Iron	Material optimized for 1-100 kHz	Material type
Kool Mu	Material optimized for 50-250 kHz	Material type
