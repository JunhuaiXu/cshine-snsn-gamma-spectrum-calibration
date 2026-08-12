# CSHINE-Gamma data preprocessing

This directory contains the migrated ROOT/C++ implementation of the analysis
from detector calibration and time correction through the measured
detector-level gamma-ray spectrum. It preserves the reviewed historical
physics definitions while replacing embedded paths, source-file mutation, and
unprotected outputs with parameterized inputs, explicit manifests, and run
metadata.

The complete ordered run is documented in
[`../../docs/CHAPTER3_END_TO_END.md`](../../docs/CHAPTER3_END_TO_END.md).
This page describes the component structure and physical contracts; it does
not duplicate the full runbook.

## Historical-source crosswalk

| Historical path | Physical role | Portable stage |
|---|---|---|
| `DataPreprocessing/step0-nobkg/` | radioactive-source background subtraction | M2 |
| `DataPreprocessing/step1-2D/` | low-/high-gain relation | M3 |
| `DataPreprocessing/step2-fit/` | three-point crystal energy calibration | M4 |
| `DataPreprocessing/step3-time/` | time-amplitude spectra, fit artifacts, and correction | M5 |
| `DataPreprocessing/step4-convert*` | calibrated event trees and reconstructed spectra | M6--M9 |
| `DataPreprocessing/step7-DeltaYrelated/` and `step8-TimeCheck/` | topology, timing, and trigger diagnostics | M10A--M10B |
| `DataPreprocessing/step10-DifferentTriggerMode/` and central spectrum macros | fast and slow background routes | M11 |
| central `spectrum_110.root:histDiff` | detector-level observed spectrum | M12 |

The immutable source inventory and checksums are in `provenance/`. Historical
paths identify provenance only; public commands use authorized, replaceable
input roots.

## Source layout

```text
analysis/data_preprocessing/
├── apps/          one direct executable per analysis operation
├── config/        reviewed 60-group, 6-group, and 59-group manifests
├── include/       public physical and ROOT-streaming contracts
├── provenance/    source manifest, migration manifest, and stage registry
├── src/           migrated implementations
├── tests/         ROOT/C++ synthetic and schema tests
└── validation/    bounded diagnostics for historical ambiguities
```

`tools/data_preprocessing.py` is the recommended repository-level entry. It
orchestrates these executables, creates protected run directories, and records
metadata; physics calculations remain in the C++ component.

## Stage contracts

The compact file/object handoffs are maintained in
[`../../docs/ANALYSIS_IO_MAP.md`](../../docs/ANALYSIS_IO_MAP.md), and the exact
input schemas are in [`../../docs/DATA_ACCESS.md`](../../docs/DATA_ACCESS.md).
The following points define the component-level physics boundary.

### M1: calibration ROOT classes

`t_2d_fit` and `t_gamma_cali` retain their historical class names, array
layouts, gain-transition behavior, and ROOT class version 2. CMake regenerates
the minimal dictionary required to stream `cali_20240308`. A separate
reconstruction dictionary preserves `recon_result` and its nested STL types.

### M2: radioactive-source background subtraction

M2 preserves 21 source files, 24 background files, the strict
`100 < GAMMA*_T < 4000` selection, 4096-bin high-gain ADC spectra, historical
live times, ROOT object names, statistical errors, and live-time-normalized
subtraction. Inputs and output paths are configurable.

### M3: low-/high-gain relation

M3 preserves the 105 reviewed files, all 15 crystals, the strict low-gain
selection `150 < GAMMA*_LOW_E < 600`, the model `[a0]+[a1]*x`, the 1000--3500
high-gain fit range, slope limits 0.099--0.101, the `f_data` object, and the
historical 4-by-4 channel canvas.

### M4: three-point energy calibration

M4 reads the M2 net source spectra and M3 `f_data`. It preserves the
channel-specific fit windows, the Co-60 quadratic-background plus two-Gaussian
model, the Th-nat Gaussian model, and the 1.173, 1.332, and 2.614 MeV reference
energies. The fitted Gaussian widths remain the graph x uncertainties, matching
the historical implementation. The main output is `cali_20240308`.

### M5: time-amplitude evidence and fit audit

M5 preserves the original time-amplitude histogram producer, the distinct
historical diagnostic expression, the production correction parameters, and
the strict event-level gamma-TDC validity window. The original batch fitter
for the 15 per-crystal ROOT/text pairs is unavailable. The surviving
`f_00`--`f_14` artifacts are therefore immutable inputs to
`inspect_time_fit_outputs`; the repository does not invent a replacement fit.

The historical input-histogram test `100 <= TDC <= 4000` is intentionally
distinct from the event-production condition `100 < TDC < 4000`.

### M6: calibrated event trees

M6 reads raw `tree` branches, applies the dual-gain energy calibration and
time correction, and writes `GammaCaliData`. The output retains
`GammaEnergy[15]`, `GammaTime[15]`, `ADC_Gamma[32]`, `TDC_Gamma[32]`,
`ADC_Veto[3]`, `TDC_Veto[3]`, and `TDC_T0[4]`. M6 retains veto and trigger
information but applies neither shower reconstruction nor veto selection.

### M7: neighboring-crystal timing diagnostics

M7 consumes the 60 manifest-defined beam-on M6 trees and writes `h2_all`,
`h2_cut`, `h1`, `hh_diff`, `h3`, and `h4`. The selected objects use
`GammaEnergy[5]+GammaEnergy[6] >= 30 MeV`; no shower or veto condition is
introduced. `hh_diff` remains unscaled in the ROOT file, and any display
scaling is applied only by the renderer.

### M8: shower reconstruction

M8 appends `recon_result` and `count_veto` to explicit M6 trees. It preserves
the inclusive 1 MeV unit threshold, orthogonal and diagonal neighbors, 50 ns
aggregation, 100 ns core separation, descending-energy processing, and the
historical nonmerged placeholder behavior. Main candidates use central cores
5, 6, 9, and 10 without a veto requirement and side cores 4, 7, 8, 11, 13,
and 14 only when all three veto faces are silent. Each veto face fires for
`100 < TDC_Veto < 4000`.

### M9: run orchestration and spectrum merging

M9 invokes the M8 executable for the exact 60-group beam-on and 6-group
beam-off manifests. It then merges per-crystal objects and produces central,
side, and total reconstructed spectra. The separate 59-group March 5--10
manifest is reserved for the before/after-reconstruction figure and cannot be
substituted for the central 60-group sample.

### M10A and M10B: Chapter 3 diagnostics

M10A reads stored M8 candidates and writes topology, spatial-spread,
core-energy, core-time, and multiplicity objects for both beam roles. The main
diagnostic family uses four central cores plus six veto-silent side cores;
the author-reviewed multiplicity family uses all 15 cores without a veto.

M10B writes 15 trigger-monitor spectra and five trigger-conditioned
energy--time distributions. Historical and author-reviewed candidate policies
are separately named because their central/side veto assignments differ. A
renderer must choose the intended family explicitly.

### M11: background subtraction

The slow route rebins the beam-on and beam-off spectra to 1 MeV and normalizes
the beam-off spectrum over 110--200 MeV. The fast cross-check rejects the SSD
M2 interval, uses equal-width -350 to -50 ns and 50 to 350 ns windows, and
subtracts at unit scale. The two outputs remain separately named.

### M12: observed-spectrum contract

M12 validates, but does not rewrite, the slow-route
`spectrum_110.root:histDiff`. The fixed handoff is a laboratory-frame `TH1D`
with 200 uniform 1 MeV bins over 0--200 MeV, stored uncertainties, and retained
negative and flow bins. Frame transformations, rebinning, inference, and
unfolding are outside this repository.

## Build and test

Requirements are CMake 3.10 or newer, a C++11 compiler, and ROOT 6.22 or
newer with Core, Gpad, Graf, Hist, Imt, RIO, Thread, Tree, and TreePlayer.

The recommended repository-level check is:

```bash
python3 tools/data_preprocessing.py check
```

For direct component development:

```bash
cmake -S analysis/data_preprocessing -B build/data_preprocessing
cmake --build build/data_preprocessing
ctest --test-dir build/data_preprocessing --output-on-failure
```

Installation is optional:

```bash
cmake --install build/data_preprocessing --prefix /desired/install/prefix
```

The build installs the calibration and reconstruction libraries, ROOT
dictionaries, public headers, and stage executables declared by CMake.

## Direct executables

Direct executables are intended for single-stage development and debugging;
the repository-level runner should be used for recorded analysis runs.

| Operation | Direct executable |
|---|---|
| M2 source/background spectra | `build_source_spectra` |
| M3 gain relation | `fit_gain_relation` |
| M4 energy calibration | `fit_energy_calibration` |
| M5 time-amplitude spectra | `build_time_amplitude_spectra` |
| M5 surviving-fit audit | `inspect_time_fit_outputs` |
| M6 calibrated event tree | `build_calibrated_event_tree` |
| M7 neighboring-time diagnostics | `build_neighbor_time_diagnostics` |
| M8 reconstructed event tree | `build_reconstructed_event_tree` |
| M9 spectrum merge | `merge_reconstructed_spectra` |
| M10A Chapter 3 diagnostics | `build_chapter3_diagnostics` |
| M10B trigger diagnostics | `build_trigger_diagnostics` |
| M11 fast-window spectra | `build_fast_coincidence_spectra` |
| M11 observed spectrum | `build_observed_spectrum` |
| M12 observed-spectrum inspection | `inspect_observed_spectrum` |

Use `<executable> --help` for its exact parameters. Run records, sample
manifests, and output-directory conventions are defined by
`tools/data_preprocessing.py` and the end-to-end guide.

## Detailed migration records

`M7_..._REVIEW.md` through `M13_..._REVIEW.md` are historical migration
records. They preserve the evidence and decisions made when each stage was
closed; they are not current-status or command authorities. Current status is
maintained only in `docs/REPRODUCIBILITY_STATUS.md`.
