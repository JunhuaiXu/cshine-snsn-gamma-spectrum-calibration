# Data access and input layout

Experimental data and historical ROOT outputs are not distributed with this
project. Obtain authorized access separately and provide the analysis root at
runtime through `--analysis-root`. Do not copy credentials or internal server
locations into source code or public documentation.

For migrated M2--M12 runs, prefer `tools/data_preprocessing.py`. It writes a
new run directory containing the frozen configuration, exact input list, file
sizes and modification times, environment and executable metadata, and output
checksums. Add `--hash-inputs` only when full SHA-256 calculation over the raw
ROOT inputs is required.

The exact ordered M2--M12 commands and stage output paths are given in
[`CHAPTER3_END_TO_END.md`](CHAPTER3_END_TO_END.md). In particular, M6 must be
run once with the 60-group beam-on manifest and once with the 6-group beam-off
manifest before M9 can reconstruct and merge both roles.

## Radioactive-source background input contract

`build_source_spectra` accepts a directory containing the fixed central
2024-03-08 raw ROOT inputs:

```text
<raw-root-directory>/
├── a20240308_ThnatCo60.0000.root ... .0020.root
└── a20240308_BKG_ALLOR.0000.root ... .0023.root
```

All 45 files must contain a tree named `tree`. Each tree must provide the
15 high-gain ADC branches `GAMMA1_HIGH_E`--`GAMMA15_HIGH_E` and the 15 timing
branches `GAMMA1_T`--`GAMMA15_T`. The command performs a complete file and
branch preflight before creating its ROOT output. The input files are read
only; generated ROOT spectra and the tab-separated run report must be written
to a separate result directory.

## Low-/high-gain relation input contract

`fit_gain_relation` accepts a directory containing the fixed central
2024-03-08 collision inputs:

```text
<raw-root-directory>/
└── a20240308_SnSn_GOAL_ALLCOIN.0000.root ... .0104.root
```

All 105 files must contain a tree named `tree`. Each tree must provide the
15 low-gain branches `GAMMA1_LOW_E`--`GAMMA15_LOW_E` and the 15 high-gain
branches `GAMMA1_HIGH_E`--`GAMMA15_HIGH_E`. The command performs a complete
file and branch preflight before fitting. Inputs are read only; the `f_data`
ROOT object, stored 4-by-4 canvas, parameter text, run report, and any optional
canvas exports must be written to a separate result directory.

## Three-point energy-calibration input contract

`fit_energy_calibration` reads two independent ROOT inputs:

1. an M2 source-spectrum file containing `h_nobkg_XE_00` through
   `h_nobkg_XE_14`;
2. an M3 gain-relation file containing an object named `f_data` of type
   `t_2d_fit`.

The command reads both inputs without modification and writes a new ROOT file
containing the historical 15 calibration graphs and fits, three canvases, and
the `t_gamma_cali` object `cali_20240308`. A tab-separated run report records
the inputs, fixed reference energies, fitted peak positions and widths, fit
status, and linear calibration parameters. Optional canvas exports and all
metadata must be written to the same separate result directory. Existing
outputs are rejected by default.

## Calibrated and reconstructed event-tree contracts

M6 reads the central raw-event branches and the M4 `cali_20240308` object. Its
`GammaCaliData` output contains:

- `GammaEnergy[15]` and `GammaTime[15]`;
- `ADC_Gamma[32]` and `TDC_Gamma[32]`;
- `ADC_Veto[3]` and `TDC_Veto[3]`;
- `TDC_T0[4]`.

The veto arrays are retained unchanged; M6 applies no veto decision. M8 reads
one or more explicit M6 ROOT files and writes the same branches plus:

- `recon_result`, a map from candidate crystal index to the historical
  `jiugong_recon_result_t` cluster result;
- `count_veto`, the number of the three veto faces satisfying the strict TDC
  interval.

Explicit M8 input files are required. Directory wildcards, sample roles,
beam-on/beam-off discovery, and spectrum merging are not part of the M8
contract. M9 supplies these through exact 60-group beam-on and 6-group
beam-off manifests, runs M8 once per group, and writes protected merged
outputs, reports, manifests, and metadata below the selected results directory.

M10A then reads those exact M9 per-run reconstruction outputs and writes
`beam-on/h2_check.root` and `beam-off/h2_check_BKG.root`. M10B reads the same
beam-on outputs and writes `trigger_diagnostics.root`, keeping the historical
and author-reviewed trigger-conditioned selections under different object
names. M11 reads the M9 merged spectra for the slow beam-off route and the M9
beam-on reconstructed trees for the fast/random-window route. It writes
separate `slow/spectrum_110.root` and `fast/spectrum_110.root` files; the slow
route is the authoritative input to M12.

## Detector-level observed-spectrum interface

M12 reads, without modifying or copying, the slow-route M11 output:

```text
<m11-run-directory>/slow/spectrum_110.root
└── histDiff  (TH1D, 200 bins, 0--200 MeV)
```

The axis is the laboratory-frame reconstructed gamma-ray energy in MeV. Each
regular bin stores background-subtracted event counts and its stored standard
error. Underflow, overflow, and negative regular-bin contents are reported
without clipping. The filename suffix `110` identifies the lower edge of the
historical background-normalization interval; it does not truncate the ROOT
object's energy axis.

The interface does not prescribe a later fit range, rebinning, normalization,
energy-frame conversion, response model, or negative-bin treatment. Read the
object as `TH1D` or through the `TH1` base class; do not cast it to `TH1F`.

## Time-amplitude correction input contract

The time-amplitude correction program expects one selected pair or, in
`--all-crystals` mode, all 15 pairs:

```text
<analysis-root>/
└── DataPreprocessing/
    └── step3-time/
        └── timeFigs/
            └── fits/
                ├── f_00.root ... f_14.root
                └── f_00.out  ... f_14.out
```

Required contents:

- `f_XX.root`: ROOT canvas `Canvas_1`; the final object satisfying the
  historical `isinstance(obj, ROOT.TH2)` selection is the time-amplitude count
  distribution;
- `f_XX.out`: fit parameters `C0`, `E0`, and `T0` for
  `C0 / (E0 - ADC-E) + T0`.

The program validates that both files and the required ROOT objects exist
before drawing. It reads the inputs without modifying them and writes the new
PDF, PNG, validation CSV, and run-metadata JSON files only below the directory
specified by `--output-dir`. The CSV records the SHA-256 of every ROOT and
parameter input without copying those files into the project.

## Corrected-time correlation and time-difference input contract

`plot_neighbor_time_correlation.py`,
`plot_unit_time_and_neighbor_difference.py`, and the corresponding portable
ROOT macro expect the historical reconstructed-event files below the analysis
root:

```text
<analysis-root>/
└── DataPreprocessing/
    └── step4-convert.0308.PreRun/
        ├── a20240304_SnSn_GOAL_ALLCOIN.006.root
        ├── a20240305_SnSn_GOAL_ALLCOIN.000.root ... .007.root
        ├── a20240306_SnSn_GOAL_ALLCOIN.000.root ... .014.root
        ├── a20240307_SnSn_GOAL_ALLCOIN.000.root ... .013.root
        ├── a20240308_SnSn_GOAL_ALLCOIN.000.root ... .010.root
        ├── a20240309_SnSn_GOAL_ALLCOIN.000.root ... .003.root
        └── a20240310_SnSn_GOAL_ALLCOIN.000.root ... .006.root
```

The exact list contains 60 ROOT files. Every file must contain tree
`GammaCaliData` with the following 15-element event arrays:

- `GammaTime`: corrected CsI(Tl) times in ns;
- `GammaEnergy`: calibrated CsI(Tl) energies in MeV.

The two-dimensional neighboring-crystal figure uses `GammaTime[5]` and
`GammaTime[6]`. Its second panel alone applies
`GammaEnergy[5] + GammaEnergy[6] >= 30`. Both two-dimensional histograms use
100 bins per axis from -500 to 500 ns.

The one-dimensional analysis-note Fig. 6 reproduction uses three histograms:

- `GammaTime[5]`, 100 bins from -500 to 500 ns;
- `GammaTime[5] - GammaTime[6]`, 100 bins from -200 to 200 ns, without an
  energy selection;
- the same time difference and bins with
  `GammaEnergy[5] + GammaEnergy[6] >= 30`.

For the one-dimensional overlay only, the selected histogram is multiplied by
the ratio of the two histogram peak heights, following the historical ROOT
macro. The unscaled count totals and scale factor are preserved in the run
metadata. Inputs are read without modification; generated figures and
metadata are written only below `--output-dir`.

## Reconstruction-multiplicity ROOT-output contract

`plot_reconstruction_multiplicity.py` reads the existing analysis output:

```text
<analysis-root>/
└── DataPreprocessing/
    └── step7-DeltaYrelated/
        └── h2_check.root
```

The current plotting input is generated by the focused validator and must
contain three objects filled in the same event loop:

- `all15_cluster_size_vs_total_energy`: reconstructed total energy versus the
  number of CsI(Tl) units assigned to one reconstructed cluster, with 200
  energy bins from 0 to 200 MeV and nine size bins from 0.5 to 9.5;
- `all15_core_multiplicity`: number of reconstructed cores per trigger;
- `all15_high_core_multiplicity`: number, within each trigger, of reconstructed
  candidates whose individual total energy is strictly above 35 MeV.

All 15 CsI(Tl) units are admitted as possible cores and no plastic-veto
condition is applied. On the retained 60-file sample the high-energy histogram
has 7218 and 21 entries in its first two bins. The historical drawing macro's
fixed 6731/19 values are not plotted and are retained only in JSON metadata as
a provenance comparison. The source of those historical constants is not part
of the current data contract.

## Spatial-correlation ROOT-output contract

`plot_spatial_correlation_energy_intervals.py` and its focused ROOT reference
read the existing reconstructed-candidate diagnostic:

```text
<analysis-root>/
└── DataPreprocessing/
    └── step7-DeltaYrelated/
        └── h2_check.root
```

The file must provide two `TH2` objects:

- `ALL_h2_ax_ay_10_100`: \(10\leq E_{\rm tot}\leq100\) MeV;
- `ALL_h2_ax_ay_100_inf`: \(E_{\rm tot}>100\) MeV.

Each object has 70 x 70 bins over 0--7 cm on both axes. The horizontal axis is
the energy-weighted horizontal spread `delta_x`, and the vertical axis is the
corresponding vertical spread `delta_y`. The upstream analysis admits the four
central and six side cores used by the main reconstructed sample; side cores
satisfy `count_veto == 0`. The two plotted objects have no additional
local-multiplicity requirement.

The plotting layer does not rebin or normalize the counts. It writes PDF, PNG,
and JSON metadata only below `--output-dir`; existing outputs are protected
unless `--force` is supplied.

## Total-energy/spatial-spread ROOT-output contract

`plot_energy_spatial_spread_correlations.py` and its focused ROOT reference
read the same `h2_check.root` and require:

- `ALL_h2_TotalE_DeltaY`: 50 x 70 bins over 5--200 MeV and 0--7 cm;
- `ALL_h2_TotalE_Delta`: 50 x 70 bins over 0--200 MeV and 0--7 cm.

The horizontal coordinate is `E_tot`. The vertical coordinates are `delta_y`
and `delta_r = sqrt(delta_x^2 + delta_y^2)`, respectively. Both objects use
the accepted central/side-core sample defined above, including the side-core
veto requirement, and neither imposes an additional local-multiplicity cut.
The plotting layer preserves each object's own energy range and logarithmic
count scale. Counts outside the fixed axes are retained in the ROOT object and
reported in metadata but are not drawn.

## Core-energy/total-energy ROOT-output contract

`plot_core_total_energy_correlations.py` and its focused ROOT reference read
the same `h2_check.root` and require:

- `central_h2_TotalE_CenterE`: accepted candidates with central core units
  5, 6, 9, or 10;
- `side_h2_TotalE_CenterE`: accepted candidates with side core units 4, 7, 8,
  11, 13, or 14 and the upstream `count_veto == 0` condition.

Both are 50 x 80 `TH2` objects spanning 5--200 MeV in `E_tot` and 0--80 MeV
in `E_core`. Here `E_tot` is the reconstructed cluster energy and `E_core` is
the calibrated energy of its reconstructed core crystal. Neither object has
an additional local-multiplicity selection. The plotting layer performs no
rebinning, normalization, fitting, or event reconstruction and keeps an
independent logarithmic count scale for each panel.

## Reconstructed-energy/core-time ROOT-output contract

`plot_energy_core_time_correlation.py` and its focused ROOT reference read the
same `h2_check.root` and require the merged object
`ALL_h2_TOF_TotalE`. Its horizontal coordinate is the corrected time
`GammaTime[core]` of the reconstructed core crystal, and its vertical
coordinate is the reconstructed cluster energy `E_tot`.

The object contains the accepted central-core candidates from units 5, 6, 9,
and 10 and the accepted side-core candidates from units 4, 7, 8, 11, 13, and
14 after the upstream `count_veto == 0` side-core requirement. It applies no
additional energy, time-window, or local-multiplicity selection. The published
merged schema is 100 x 200 bins over -500--500 ns and 0--200 MeV.

The historical source initializes merge targets with a 0--7 vertical range
before ROOT `Merge`, although the per-crystal histograms and the resulting
published object span 0--200 MeV. The immutable source is retained exactly;
the portable readers validate the resulting published object schema. The
plotting layer performs no event reconstruction, rebinning, normalization, or
fitting and writes all new artifacts only below `--output-dir`.

## Event-display input and selection contract

The PRC/analysis-note panels have been traced to
`EventDisplay/EventALL/lego_0.root` for the gamma-ray candidate and
`EventDisplay/lego_5.root` for the cosmic-muon candidate. A dedicated producer
now freezes both events in `event_display_records.root`, and an independent
validator compares every displayed bin with the two historical ROOT panels.
The accepted validation has zero mismatched bins and zero maximum absolute
difference for both histograms.

The gamma-ray record is the 16th accepted candidate in the fixed 60-file
March 4--10 `step4-convert.0308.PreRun` sample: zero-based source entry
1,678,036, core CsI05, and reconstructed energy 155.4430569 MeV. The selection
uses `jiugong_recon`, the central/side core set, side-core veto, a 110--200 MeV
reconstructed-energy interval, and at most one accepted candidate per source
entry. The cosmic record is the second accepted event in
`step4-convert.0308/a20240306_SnSn_GOAL_ALLCOIN.007.root`: zero-based entry
1,224,015, `count_veto == 0`, at least five finite corrected-time crystals,
and at least 100 MeV raw displayed energy.

The thesis renderer reads only this validated record and uses ROOT `LEGO2` in
a horizontal layout. PRR used different representative events and conditions
and remains outside this PRC/analysis-note reproduction contract.

## Beam-off cosmic-muon topology ROOT-output contract

`plot_cosmic_muon_topology.py` and its focused ROOT reference read the
author-confirmed final-selection beam-off output:

```text
<analysis-root>/
└── DataPreprocessing/
    └── step7-DeltaYrelated/
        └── h2_check_BKG.root
```

The file must provide:

- `ALL_h2_ax_ay_100_inf`: 70 x 70 bins over 0--7 cm on both spatial-spread
  axes, filled for reconstructed beam-off candidates with `E_tot > 100 MeV`;
- `ALL_h2_TotalE_CenterE`: 50 x 80 bins over `E_tot=5--200 MeV` and
  `E_core=0--80 MeV`, with no additional energy selection.

Both merged objects contain central core units 5, 6, 9, and 10 and side core
units 4, 7, 8, 11, 13, and 14. Central candidates have no plastic-veto
requirement; side candidates require `count_veto == 0`, meaning that all three
plastic-veto detectors have no signal. This is the same selection used by the
analysis-note Fig. 10 producer and was explicitly selected by the author for
the thesis redraw. The dedicated long-paper output and its reversed veto
condition remain historical provenance but are not accepted by this input
contract.

The plotting layer performs no event reconstruction, rebinning,
normalization, smoothing, or fitting. It records entries, displayed counts,
flow counts, input and output checksums, and software versions in JSON. New
outputs are written only below `--output-dir` and remain candidates until
author review.

## Trigger-monitoring TDC ROOT-output contract

`plot_trigger_tdc_distributions.py` and its focused ROOT reference read the
historical trigger-monitoring output:

```text
<analysis-root>/
└── DataPreprocessing/
    └── step8-TimeCheck/
        └── step6-TimeWalkPlot/
            └── step7-DeltaYrelated/
                └── h2_check.root
```

The portable chain copies the 32-element hardware trigger TDC array into the
M6 event tree as `TDC_Gamma_Trig_list` and propagates it unchanged through M8.
M10B allocates `h1_TrigList0`--`14` through a
15-element generic container, but the historical trigger-TDC drawing reads
only the first seven objects, corresponding to source-array elements 16--22.
Each object has 4096 bins over TDC channels 0--4096. A value is filled only when
`100 < TDC_Gamma_Trig_list[index] < 4000`. The six published panels use this
fixed mapping:

| Panel | ROOT object | Source-array element | Trigger monitor |
|---|---|---:|---|
| (a) | `h1_TrigList1` | 17 | `SSD M1 & CsI M1` |
| (b) | `h1_TrigList2` | 18 | `SSD M2` |
| (c) | `h1_TrigList3` | 19 | `SSD M1 & NA M1` |
| (d) | `h1_TrigList4` | 20 | `NA M1 & T0` |
| (e) | `h1_TrigList6` | 22 | `LS & T0` |
| (f) | `h1_TrigList0` | 16 | `ALL_OR` global trigger |

Within these first seven objects, `h1_TrigList5` is not displayed because the independent `SSD M1 & LS`
monitoring TDC signal was not recorded. The plotting layer does not rebin,
normalize, smooth, or fit the histograms. It preserves the common logarithmic
count range used by the analysis-note figure and places the panel letters at
upper left and the trigger labels in the data-free upper-right region. Before
drawing, it checks the maximum count in each predefined label-side horizontal
region and stops if a changed input would make the annotation overlap a large
spectral structure. The current thesis-oriented presentation uses two rows and
three columns; this changes only the arrangement of the same six histograms.
The revised authorized run used the same input SHA-256 as the earlier
validated run and reproduced all six object entries, in-range counts, and
zero flow counts. Its PDF, PNG, and JSON outputs were downloaded and
checksum-verified; the author-approved PDF is used in thesis Sec. 3.3.4.

## Trigger-conditioned energy/core-time ROOT-output contract

`plot_trigger_energy_time_correlations.py` and its focused ROOT reference read
five existing analysis outputs under `<analysis-root>/DataPreprocessing/
step8-TimeCheck/`:

| Panel | Branch | Trigger-monitor element |
|---|---|---:|
| (a) | `step5-onlygamma` | 17 |
| (b) | `step5-SSDM2` | 18 |
| (c) | `step5-NAandSSD` | 19 |
| (d) | `step5-T0andNA` | 20 |
| (e) | `step5-T0LS` | 22 |

For every branch, the required input is
`step7-DeltaYrelated/h2_check.root:ALL_h2_TOF_TotalE`. Upstream,
`step4-convert.0308/aa_example.C` retains events when the corresponding
`TDC_Gamma_Trig_array[index]` is between 100 and 4000 inclusive. This means
that the trigger monitor is valid; it is not an exclusive selection of the
narrow self-trigger peak.

Each object contains corrected `GammaTime[core]` on the horizontal axis and
reconstructed `E_tot` on the vertical axis. Source review found that the
historical Fig. 15 producer requires `count_veto == 0` for central cores
5, 6, 9, 10 and does not impose it on side cores 4, 7, 8, 11, 13, 14. This is
opposite to the author-reviewed main-analysis rule. M10B therefore stores
both `historical_h2_TOF_TotalE_TrigXX` and
`reviewed_h2_TOF_TotalE_TrigXX` families rather than silently replacing one
definition with the other. The portable reader requires the published 100 x 200 bins
over -500--500 ns and 0--200 MeV. It performs no rebinning, normalization,
fitting, event reconstruction, or trigger reclassification. The
thesis-oriented presentation uses two rows and three columns, with the
lower-right pad intentionally empty because analysis-note Fig. 15 contains
five trigger-conditioned panels. Each panel keeps an independent logarithmic
count scale; repeated axis and color-bar titles are suppressed without
changing the displayed values.

The authorized real-data run recorded all five input checksums and verified
the required 100 x 200 object schema. Panel entries are 15,140,779,
4,658,621, 50,384, 193,610, and 3,925; displayed-bin counts are 15,096,166,
4,511,595, 48,813, 186,990, and 3,721. The generated PDF, PNG, and JSON
outputs were downloaded and checksum-verified. They remain candidate
artifacts in the local archive; the author-approved PDF is used in thesis
Sec. 3.3.4. That adopted artifact remains the historical-policy result.

For portable M10B outputs, use:

```bash
python3 plotting/plot_trigger_tdc_distributions.py \
  --portable-root /path/to/trigger_diagnostics.root \
  --output-dir results/figures

python3 plotting/plot_trigger_energy_time_correlations.py \
  --portable-root /path/to/trigger_diagnostics.root \
  --selection-policy historical \
  --output-dir results/figures
```

Use `--selection-policy reviewed` only for an explicitly labelled comparison
with the author-reviewed central/side veto definition.

## M11 observed-spectrum input contract

M11 keeps the slow-coincidence and fast-coincidence background routes in
separate result directories.

The slow route requires two M9 merged ROOT files:

```text
beam-on/all_recon.root:h_total_E_M1
beam-off/all_recon_BKG.root:h_total_E_M1
```

Both input histograms must have 1000 bins over 0--200 MeV. They are rebinned
by five, the beam-off spectrum is normalized to the beam-on spectrum over
110--200 MeV, and the output is
`slow/spectrum_110.root:histDiff`.

The fast route requires the exact beam-on run manifest and its M8
`recon_result` trees. It excludes the open interval
`835 < TDC_Gamma_Trig_list[18] < 850`, fills the inclusive signal window
`-350 <= GammaTime[core] <= -50 ns` and random window
`50 <= GammaTime[core] <= 350 ns`, and uses the reviewed central/side veto
policy. Equal window widths give a fixed subtraction scale of one. The final
output is `fast/spectrum_110.root:histDiff`; despite the historical filename,
no 110 MeV normalization is used in this route.

The public entry is:

```bash
python3 tools/data_preprocessing.py m11 \
  --slow-signal /path/to/m9/beam-on/all_recon.root \
  --slow-background /path/to/m9/beam-off/all_recon_BKG.root \
  --beam-on-input-dir /path/to/m9/beam-on/reconstructed_runs \
  --run-id central-background-subtraction
```

The command records the exact input manifest, configurations, commands,
software environment, executable hashes, reports, output checksums, and
completion state without embedding a private server path.

Later physical-interpretation inputs are outside this repository and are not
part of this data-access contract.
