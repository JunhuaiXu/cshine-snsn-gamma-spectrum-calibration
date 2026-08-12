# Reproducing the CSHINE Sn+Sn gamma-spectrum calibration

This document records the implementation status of the CSHINE-Gamma
calibration, event-reconstruction, background-treatment, and detector-level
gamma-spectrum chain for the 25 MeV/u 124Sn+124Sn experiment.
A historical directory is not considered reproducible merely because its
source files have been archived. A directly migrated stage becomes a portable
reproduction entry after its inputs, ROOT objects, physical definitions,
command, outputs, build, and proportionate synthetic checks are documented.
Rerunning the published dataset is a separate numerical-validation activity
and is not required unless the implementation is substantially rewritten or a
result-level comparison is explicitly requested.

## Stage status

| Physical analysis stage | Historical source | Current status |
|---|---|---|
| Calibration, timing, event reconstruction, diagnostics, background, and measured spectrum | `DataPreprocessing/` | **Repository scope:** M0B is source-closed, M1--M12 are code-migrated within their stated boundaries, and M13 records are closed; the unavailable historical M5 batch-fit producer remains an explicit audited limit |

Transport-model interpretation, detector-response folding, physical-parameter
inference, and source-spectrum unfolding are outside this repository.

## Current analysis-migration workflow

For a complete ordered run, use
[`docs/CHAPTER3_END_TO_END.md`](docs/CHAPTER3_END_TO_END.md). It is the
authoritative copy-and-run sequence connecting the M2 and M3 outputs to M4,
running M6 separately for the 60 beam-on and 6 beam-off manifests, and passing
those exact outputs through M9--M12. The commands below document individual
stage behavior and are not a substitute for that ordered handoff.

Set `--analysis-root` to an authorized copy or mount of the historical
analysis directory. The required relative layout is:

```text
<analysis-root>/DataPreprocessing/...
```

Do not edit or reorganize the original `DataPreprocessing/` code. The
historical ROOT analysis and plotting sources remain immutable provenance.
Portable analysis code is added under `analysis/data_preprocessing/`, while
portable ROOT plotting and optional Python presentation code are kept under
`plotting/`. Run this project from a separate working directory, read inputs
through `--analysis-root`, and write generated files only below:

```text
results/
```

The portable input layout and ROOT-object definitions are listed in
[`docs/DATA_ACCESS.md`](docs/DATA_ACCESS.md). Exact server locations are kept
in private collaborator records and are intentionally absent here.

## Efficient remote figure execution

Short plotting jobs can be executed with one reused SSH connection rather
than a separate authentication for every upload, command, and download. Copy
the public-safe configuration example to the ignored local directory and fill
it with an authorized host and paths:

```bash
mkdir -p local
cp docs/remote-config.example.json local/remote.json
chmod 600 local/remote.json
```

Then validate and run a bounded job:

```bash
python3 tools/remote_figure.py validate trigger-monitoring-figures
python3 tools/remote_figure.py run trigger-monitoring-figures
```

The first command is local-only. The second opens or reuses one SSH master
connection, uploads the declared scripts in one transfer, writes into a unique
remote run directory, downloads the complete result directory, and verifies
every declared output against the remote SHA-256 value. It performs no rapid
retry loop. The idle connection expires automatically after the configured
`ControlPersist` interval.

This runner is only a transport and execution layer. Physics selections,
histogram definitions, numerical validation, author review, and thesis use
remain governed by the figure workflow and its individual records.

## Currently available core components

The recommended M0--M12 entry verifies the portable manifest, builds the
calibration and historical-output-audit ROOT libraries, and runs all
data-free tests. This includes the Python checks of the unified runner and eighteen ROOT/C++ tests of
the migrated analysis:

```bash
python3 tools/data_preprocessing.py check
```

If several ROOT installations are present, select one explicitly with
`--root-dir /path/to/root/cmake`. Add
`--snapshot-root /path/to/DataPreprocessing` when the immutable historical
sources are available for checksum verification. The check writes
`build/data_preprocessing/check_metadata.json` with the commands, environment,
manifest counts, status, and timestamps.

The equivalent manual build remains available:

```bash
mkdir -p build/data_preprocessing
cd build/data_preprocessing
cmake ../../analysis/data_preprocessing
cmake --build .
ctest --output-on-failure
cd ../..
```

Build details, installed files, and the optional historical-object inspector
are documented in
[`analysis/data_preprocessing/README.md`](analysis/data_preprocessing/README.md).

The M2 central source/background spectrum can be generated from an authorized
raw-data directory with:

```bash
python3 tools/data_preprocessing.py m2 \
  --input-dir /path/to/raw-root-files \
  --run-id central-0308
```

This entry is derived directly from the historical macro and has passed the
portable build and synthetic ROOT test. The complete historical 45-file
analysis is intentionally not rerun as part of code migration.

The M3 central low-/high-gain relation uses the same entry:

```bash
python3 tools/data_preprocessing.py m3 \
  --input-dir /path/to/raw-root-files \
  --run-id central-0308
```

This entry preserves the historical ROOT linear fit, selection, fit range,
parameter limits, 4-by-4 canvas, and `f_data` object. It has passed build,
installation, and synthetic ROOT tests; the complete historical 105-file fit
is intentionally not rerun as part of direct code migration.

The M4 central three-point calibration combines the M2 and M3 outputs:

```bash
python3 tools/data_preprocessing.py m4 \
  --source-spectra /path/to/20240308_ThnatCo60_NoBkg.root \
  --gain-relation /path/to/20240308_SnSn_GOAL_ALLCOIN.root \
  --run-id central-0308
```

It preserves the historical source-peak windows, fit models, Gaussian-width
x-error semantics, `f_data` embedding, ROOT keys, and 4-by-4 layout. The
complete M1--M4 five-test suite passed in the authorized server ROOT
environment; published calibration coefficients were not regenerated. The
M5 spectrum production, parameter application, and historical-output audit are
now implemented. The
surviving fit outputs can be checked without refitting with:

```bash
python3 tools/data_preprocessing.py m5-audit \
  --fits-dir /path/to/step3-time/timeFigs/fits \
  --run-id historical-central \
  --hash-inputs
```

This verifies the 15 ROOT/text pairs and their agreement with the production
parameters. The historical batch-fit producer is unavailable, so this audit
is deliberately not described as fit reproduction.

The reviewed raw time-amplitude spectra can be produced separately with:

```bash
python3 tools/data_preprocessing.py m5-spectra \
  --input-dir /path/to/raw-root-files \
  --mode original \
  --run-id central-0303-0310
```

The optional `historical-corrected` mode preserves the exact diagnostic macro
conversion and is not a substitute for formal event-level `GammaTime[15]`
production. Neither mode is run on the full historical data set by the test
suite.

M6 produces one calibrated event tree for every run group in the explicit
manifest:

```bash
python3 tools/data_preprocessing.py m6 \
  --input-dir /path/to/raw-root-files \
  --calibration /path/to/20240308_ThnatCo60_NoBkg.root \
  --run-id central-beam-on
```

The default manifest preserves all 60 historical beam-on groups. Its output
boundary is limited to event-level calibrated energy, corrected time, and the
retained raw gamma, veto, and T0 arrays. It does not run shower reconstruction
or veto selection. A complete run must invoke M6 a second time with
`analysis/data_preprocessing/config/central_beam_off_run_groups.tsv`; M9
requires the resulting separate beam-on and beam-off `events/` directories.
The exact two-command handoff is given in the end-to-end guide. The
implementation passed in an isolated ROOT 6.28 server
environment; the complete historical data set was not rerun.

M7 reduces those 60 calibrated trees to the six neighboring-time diagnostic
objects used in Sec. 3.3.2:

```bash
python3 tools/data_preprocessing.py m7 \
  --input-dir results/data_preprocessing/m6/central-beam-on/events \
  --run-id central-neighbor-time
```

The ROOT output stores `h2_all`, `h2_cut`, `h1`, `hh_diff`, `h3`, and `h4`.
The selected time-difference object is unscaled; the historical peak-height
ratio is a recorded display convention. The new M7 producer has synthetic
coverage but has not yet been run on the complete real-data set.

M8 applies the historical shower reconstruction to one or more explicit M6
trees:

```bash
python3 tools/data_preprocessing.py m8 \
  --input /path/to/calibrated_run_a.root \
  --input /path/to/calibrated_run_b.root \
  --run-id shower-check
```

The output retains the M6 branches and adds `recon_result`, `count_veto`, and
the per-crystal detector and reconstructed spectrum families consumed by M9.
The 1 MeV threshold, orthogonal and diagonal neighbors, 50 ns aggregation,
100 ns core separation, central/side core classes, and strict three-face veto
limits are fixed in the C++ definition and covered by synthetic boundary and
ROOT-streaming tests. M8 requires explicit input files.

M9 applies that per-run reconstruction to the reviewed beam-on and beam-off
M6 outputs and merges their spectra:

```bash
python3 tools/data_preprocessing.py m9 \
  --beam-on-input-dir /path/to/m6-beam-on/events \
  --beam-off-input-dir /path/to/m6-beam-off/events \
  --run-id central-spectra
```

The default manifests contain exactly 60 beam-on and six beam-off groups.
The output retains both the `all_notree*` per-crystal layer and the
`all_recon*` central/side/total layer. The complete real sample has not been
rerun as part of direct code migration.

The historical before/after reconstruction figure uses a distinct 59-group
March 5--10 subset, not the 60-group central-analysis manifest. If the M8
per-run files are already available, reproduce its merged ROOT input with:

```bash
python3 tools/data_preprocessing.py reconstruction-spectra \
  --reconstructed-run-dir /path/to/m9/beam-on/reconstructed_runs \
  --run-id historical-figure-sample

python3 plotting/plot_reconstruction_spectra.py \
  --per-crystal-root \
    results/data_preprocessing/reconstruction-spectra/historical-figure-sample/all_notree_figure.root \
  --output-dir results/figures/reconstruction_spectra
```

The first command reuses M9's checked histogram merger with the frozen
`reconstruction_spectra_figure_run_groups.tsv`; the second performs display
only. Neither command substitutes the 60-group central sample for the
historical 59-group figure sample.

M10 reads the stored M9 reconstruction candidates and produces the reviewed
beam-on and beam-off Chapter 3 diagnostics:

```bash
python3 tools/data_preprocessing.py m10 \
  --beam-on-input-dir /path/to/m9/beam-on/reconstructed_runs \
  --beam-off-input-dir /path/to/m9/beam-off/reconstructed_runs \
  --run-id central-diagnostics
```

This stage keeps the main central/side-veto definition separate from the
all-15-crystal, no-veto multiplicity diagnostic. It does not rerun energy or
time calibration in the plotting layer.

M10B produces the 15 trigger-monitor spectra and the five reviewed
trigger-conditioned time--energy object families:

```bash
python3 tools/data_preprocessing.py m10b \
  --beam-on-input-dir /path/to/m9/beam-on/reconstructed_runs \
  --run-id central-trigger-diagnostics
```

The frozen historical Fig. 15 veto definition and the author-reviewed main
analysis definition are written under distinct object names rather than being
silently combined.

M11 then forms the slow and fast detector-level observed spectra without
merging their distinct background definitions:

```bash
python3 tools/data_preprocessing.py m11 \
  --slow-signal /path/to/m9/beam-on/all_recon.root \
  --slow-background /path/to/m9/beam-off/all_recon_BKG.root \
  --beam-on-input-dir /path/to/m9/beam-on/reconstructed_runs \
  --run-id central-background-subtraction
```

The slow result preserves the 110--200 MeV beam-off normalization. The fast
result preserves the strict SSD M2 exclusion, two inclusive equal-width time
windows, the reviewed central/side veto policy, and unit-scale random-window
subtraction. See
[`analysis/data_preprocessing/M11_BACKGROUND_SUBTRACTION_MIGRATION_REVIEW.md`](analysis/data_preprocessing/M11_BACKGROUND_SUBTRACTION_MIGRATION_REVIEW.md)
for the complete physics and verification contract.

M12 validates the authoritative slow-route detector-level spectrum without
copying or transforming it:

```bash
python3 tools/data_preprocessing.py m12 \
  --observed-spectrum /path/to/m11/slow/spectrum_110.root \
  --run-id central-observed-spectrum-interface
```

The required object is `histDiff`, stored as a `TH1D` with 200 one-MeV bins
over 0--200 MeV in the laboratory frame and with per-bin uncertainties. The
inspector preserves negative and flow bins, writes only a report and run
metadata, and leaves all later fit-range or reference-frame choices to a
separate consumer.

The M2--M12 stage commands create
`results/data_preprocessing/<stage>/<run-id>/` and reject an existing run
directory. Each result contains `config_used.txt`, `input_manifest.tsv`,
`run_metadata.json`, `run.log`, the stage report, and the ROOT output; M3 also
contains `gain_parameters.txt`. The input manifest records path, size, and
modification time by default. Use `--hash-inputs` only when full raw-input
SHA-256 values are required and the associated I/O cost is acceptable.

Advanced users may still invoke `build_source_spectra`, `fit_gain_relation`,
`fit_energy_calibration`, `build_time_amplitude_spectra`,
`build_calibrated_event_tree`, `build_neighbor_time_diagnostics`, and
`build_reconstructed_event_tree`, `merge_reconstructed_spectra`,
`build_chapter3_diagnostics`, `build_trigger_diagnostics`,
`build_fast_coincidence_spectra`, `build_observed_spectrum`,
`inspect_observed_spectrum`, and `inspect_time_fit_outputs` directly as
documented in the component README. Their
parent output directories are created automatically, but direct invocation
does not add the unified environment and checksum metadata.

## Currently available supplementary plotting reproduction

Install the Python-only dependencies if needed:

```bash
python3 -m pip install -r plotting/requirements.txt
```

Generate one time-amplitude correction figure:

```bash
python3 plotting/plot_time_amplitude_correction.py \
  --analysis-root /path/to/gamma2024 \
  --crystal-index 5 \
  --output-dir results/figures
```

Expected output:

```text
results/figures/time_walk/individual/
└── cshine_gamma_time_amplitude_correction_CsI05_horizontal.pdf
```

Generate all 15 individual channel pairs, the two historical-layout
overviews, and the count-conservation summary:

```bash
python3 plotting/plot_time_amplitude_correction.py \
  --analysis-root /path/to/gamma2024 \
  --all-crystals \
  --output-dir results/figures
```

Expected output structure:

```text
results/figures/time_walk/
├── individual/                 15 PDF and 15 PNG channel pairs
├── overview/                   before/after PDF and PNG overviews
├── validation_summary.csv      parameters, hashes, and count checks
└── run_metadata.json           Python/ROOT/NumPy/Matplotlib and script record
```

The command checks that all requested input pairs exist before processing.
It exits before drawing new figures if either the 16 x 16 rebinning or the
corrected-time re-histogramming fails count conservation. Review both
`validation_passed` fields and retain the CSV/JSON files with any accepted
figure set.

Generate the horizontal CsI05--CsI06 corrected-time correlation, first
without an energy cut and then with
`GammaEnergy[5] + GammaEnergy[6] >= 30`:

```bash
python3 plotting/plot_neighbor_time_correlation.py \
  --analysis-root /path/to/gamma2024 \
  --output-dir results/figures
```

Expected output:

```text
results/figures/neighbor_time_correlation/
├── cshine_gamma_neighbor_time_correlation_csi05_csi06_horizontal.pdf
├── cshine_gamma_neighbor_time_correlation_csi05_csi06_horizontal.png
└── cshine_gamma_neighbor_time_correlation_csi05_csi06_horizontal_metadata.json
```

The default preserves the independently scaled logarithmic color bars used by
the two historical ROOT canvases. Run with `--shared-color-scale` only when a
common absolute count scale is explicitly required. The program verifies all
60 historical inputs and required branches before drawing, and rejects an
output if any energy-selected histogram bin exceeds its no-cut counterpart.

Generate the two one-dimensional source panels for analysis-note Fig. 6 with
the portable ROOT reference:

```bash
root -l -b -q \
  'plotting/root/draw_unit_time_and_neighbor_difference.C("/path/to/gamma2024","results/figures/unit_time_difference/root")'
```

Generate the horizontal figure and machine-readable record:

```bash
python3 plotting/plot_unit_time_and_neighbor_difference.py \
  --analysis-root /path/to/gamma2024 \
  --output-dir results/figures
```

Expected output:

```text
results/figures/unit_time_difference/
├── cshine_gamma_unit05_time_and_neighbor_difference_horizontal.pdf
├── cshine_gamma_unit05_time_and_neighbor_difference_horizontal.png
├── cshine_gamma_unit05_time_and_neighbor_difference_horizontal_metadata.json
└── root/
    ├── cshine_gamma_unit05_time_root.pdf
    └── cshine_gamma_neighbor_time_difference_root.pdf
```

ROOT performs the event selection and histogram filling in both entries. The
energy-selected time-difference curve is scaled by the ratio of the two peak
heights exactly as in the historical macro; the unscaled counts remain in the
JSON record. The real-input ROOT and Python runs agree on a scale factor of
52.00885. The author-approved PDF is used in thesis Sec. 3.3.2; any future
regeneration must still pass author review before replacing that fixed output.

After generating `core_multiplicity_all15_noveto.root` with
`analysis/data_preprocessing/validation/diagnose_core_subset_counts.cxx` as
documented in `validation/README.md`, generate the horizontal figure for
analysis-note Fig. 8:

```bash
python3 plotting/plot_reconstruction_multiplicity.py \
  --analysis-root /path/to/gamma2024 \
  --output-dir results/figures
```

Expected output:

```text
results/figures/reconstruction_multiplicity/
├── cshine_gamma_cluster_size_and_core_multiplicity_horizontal.pdf
├── cshine_gamma_cluster_size_and_core_multiplicity_horizontal.png
└── run_metadata.json
```

The command reads the three all-15-crystal, no-veto ROOT objects generated in
the same event loop. Panel (a), the blue curve, and the red curve therefore
share one crystal scope and selection. The red curve is read from
`all15_high_core_multiplicity` and gives 7218/21 in the first two bins; the
historical fixed 6731/19 values are retained only in metadata. See
`plotting/README.md` for the output hashes, thesis-use status, and historical
boundary.

Generate the horizontal redraw of analysis-note Fig. 10 from the two
historical spatial-correlation objects:

```bash
python3 plotting/plot_spatial_correlation_energy_intervals.py \
  --analysis-root /path/to/gamma2024 \
  --output-dir results/figures
```

Expected output:

```text
results/figures/spatial_correlation_energy_intervals/
├── cshine_gamma_spatial_correlation_energy_intervals_horizontal.pdf
├── cshine_gamma_spatial_correlation_energy_intervals_horizontal.png
└── cshine_gamma_spatial_correlation_energy_intervals_horizontal.json
```

The program reads `ALL_h2_ax_ay_10_100` and `ALL_h2_ax_ay_100_inf` from
`DataPreprocessing/step7-DeltaYrelated/h2_check.root`. It requires the
historical 70 x 70, 0--7 cm binning, performs no rebinning or normalization,
and keeps separate logarithmic count scales for the two source panels. The
JSON file records the input checksum, object count summaries, software
versions, and output checksums. This entry has passed local syntax and
synthetic-layout checks and a real-input run; the author-confirmed PDF is used
in thesis Sec. 3.4.1.

Generate the horizontal redraw of analysis-note Fig. 11 from the frozen total
energy and spatial-spread objects:

```bash
python3 plotting/plot_energy_spatial_spread_correlations.py \
  --analysis-root /path/to/gamma2024 \
  --output-dir results/figures
```

Expected output:

```text
results/figures/energy_spatial_spread_correlations/
├── cshine_gamma_energy_spatial_spread_correlations_horizontal.pdf
├── cshine_gamma_energy_spatial_spread_correlations_horizontal.png
└── cshine_gamma_energy_spatial_spread_correlations_horizontal.json
```

The program reads `ALL_h2_TotalE_DeltaY` and `ALL_h2_TotalE_Delta` from the
same `h2_check.root`. It preserves the historical 5--200 MeV energy range of
the first object and 0--200 MeV range of the second, together with the common
0--7 cm spread range. No rebinning or normalization is applied. Displayed
counts and underflow/overflow are recorded separately in the JSON metadata.
The author-confirmed PDF is used in thesis Sec. 3.4.1. Its SHA-256 is
`0521814a91ebaa9c2317f13e2d392ae8f226b20f2e1efe9adca2a45153064002`.
The archived validation record covers the two source-object entries and
displayed-bin counts; an enhanced axis-by-axis flow metadata file is not
claimed because it was not downloaded with the final PDF.

Generate the horizontal beam-off topology figure corresponding to
analysis-note Fig. 18:

```bash
python3 plotting/plot_cosmic_muon_topology.py \
  --analysis-root /path/to/gamma2024 \
  --output-dir results/figures
```

Expected output:

```text
results/figures/cosmic_muon_topology/
├── cshine_gamma_cosmic_muon_topology_horizontal.pdf
├── cshine_gamma_cosmic_muon_topology_horizontal.png
└── cshine_gamma_cosmic_muon_topology_horizontal.json
```

The program intentionally reads the dedicated long-paper
`step11-otherFigsLongPaper/Fig2-deltaxdeltayBKG/h2_check.root`. It requires
the frozen 70 x 70 high-energy spatial object and 200 x 80 core-versus-total
energy object. The later general beam-off output has different energy
binning and acceptance logic and is rejected by the schema check. The program
does not rebin, normalize, smooth, or fit either object. Its authorized
real-data candidate has passed object-schema, count, software-version, and
remote-to-local checksum checks; author review is still required before thesis
integration.

## Figure acceptance sequence

Every candidate figure follows the same sequence:

1. identify the historical code, server input, and ROOT objects;
2. write one standalone plotting program without changing the physics logic;
3. run it in the isolated server plotting directory;
4. compare its numerical content with the historical or published figure;
5. record the result in `plotting/README.md`;
6. wait for the author's explicit decision before copying the PDF into thesis
   assets or adding a LaTeX figure environment.

The first time-amplitude plotting version ran with the experimental ROOT
input, but its rendered isolated low-count-bin occupancy differed from the
verified historical `redrawNew` PDFs. PDF composition is not used as a
substitute for reproduction. The revised direct port fixes the historical
`imshow` interpolation and `dpi=300` output settings, and the author reports
that the revised CsI05 rendering appears consistent. The complete real-input
batch passed all count checks and its selected figures are now used in the
thesis. The neighboring-crystal program has also been run with the authorized
60-file input; its independent-scale PDF passed the built-in selected-sample
subset check and is now used in thesis Sec. 3.3.2. The one-dimensional Fig. 6
ROOT and Python entries have likewise been run with the same 60 files and
agree on the peak-scaling factor. The author-approved horizontal output is now
used in thesis Sec. 3.3.2.

The accepted Python outputs close their figure-generation tasks only. The upstream
calibration, timing, and event-level analysis is currently being migrated in
small validated stages, and the original ROOT plotting logic will be retained
as portable ROOT entries before Secs. 3.3.1 and 3.3.2 are considered complete
at the analysis-chain level.
