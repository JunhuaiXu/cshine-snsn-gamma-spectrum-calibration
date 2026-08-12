# Chapter 3 end-to-end reproduction guide

This is the single ordered runbook for the migrated CSHINE-Gamma Chapter 3
data-processing chain. It connects the radioactive-source calibration,
event-level calibration, shower reconstruction, diagnostic objects,
background subtraction, and final detector-level observed spectrum.

The commands below do not include raw data, historical fit artifacts, or
generated ROOT files. An authorized user must supply those inputs according
to [`DATA_ACCESS.md`](DATA_ACCESS.md). The historical time-walk batch fitter
is not available; M5 therefore audits the surviving 15 fit-result pairs and
does not claim to regenerate them.

## 1. Evidence level

The portable code is migrated through M12 and has passed the documented
synthetic ROOT/C++ and Python tests. The complete 60 beam-on plus 6 beam-off
experimental sample has not been rerun through this ordered entry. Following
this guide is the procedure for that full run; successful completion and
comparison with the frozen historical outputs would upgrade the corresponding
stages from **code migrated** to **numerically validated**.

Several adopted thesis figures have already been validated independently on
real historical ROOT objects. Their records do not substitute for an
end-to-end rerun, and an end-to-end rerun does not silently replace an
author-approved thesis artifact.

## 2. Prerequisites

Run from the repository root in a ROOT environment compatible with the
historical object dictionaries. The tested server environment used ROOT
6.28/04, GCC 9.4.0, CMake, and Python 3.

Set task-specific paths for the current shell:

```bash
CSHINE_REPOSITORY_ROOT="$PWD"
CSHINE_RAW_ROOT=/path/to/authorized/raw-root-files
CSHINE_TIME_FITS=/path/to/authorized/step3-time/timeFigs/fits
CSHINE_RESULTS_ROOT="$CSHINE_REPOSITORY_ROOT/results/data_preprocessing"
CSHINE_RUN_TAG=chapter3-full-YYYYMMDD
```

Do not point `CSHINE_RESULTS_ROOT` at the historical analysis directory. Each
stage creates a new protected run directory and rejects an existing run ID.

First verify the portable files, build the analysis, and run the synthetic
tests:

```bash
python3 tools/data_preprocessing.py verify
python3 tools/data_preprocessing.py check
```

Add `--snapshot-root /path/to/immutable/DataPreprocessing` to either command
when the frozen historical source snapshot is available and should also be
checked.

## 3. Calibration and time-correction evidence

### M2: radioactive-source background subtraction

```bash
python3 tools/data_preprocessing.py m2 \
  --input-dir "$CSHINE_RAW_ROOT" \
  --results-dir "$CSHINE_RESULTS_ROOT" \
  --run-id "$CSHINE_RUN_TAG-m2"
```

Output used by M4:

```text
results/data_preprocessing/m2/<tag>-m2/source_background.root
```

### M3: low-/high-gain relation

```bash
python3 tools/data_preprocessing.py m3 \
  --input-dir "$CSHINE_RAW_ROOT" \
  --results-dir "$CSHINE_RESULTS_ROOT" \
  --run-id "$CSHINE_RUN_TAG-m3"
```

Output used by M4:

```text
results/data_preprocessing/m3/<tag>-m3/gain_relation.root
```

### M4: three-point energy calibration

```bash
python3 tools/data_preprocessing.py m4 \
  --source-spectra \
    "$CSHINE_RESULTS_ROOT/m2/$CSHINE_RUN_TAG-m2/source_background.root" \
  --gain-relation \
    "$CSHINE_RESULTS_ROOT/m3/$CSHINE_RUN_TAG-m3/gain_relation.root" \
  --results-dir "$CSHINE_RESULTS_ROOT" \
  --run-id "$CSHINE_RUN_TAG-m4"
```

The authoritative M4 output for both beam roles is:

```text
results/data_preprocessing/m4/<tag>-m4/energy_calibration.root
└── cali_20240308
```

### M5: time-amplitude spectra and surviving fit-result audit

Build the original time-amplitude histograms:

```bash
python3 tools/data_preprocessing.py m5-spectra \
  --input-dir "$CSHINE_RAW_ROOT" \
  --mode original \
  --results-dir "$CSHINE_RESULTS_ROOT" \
  --run-id "$CSHINE_RUN_TAG-m5-original"
```

Audit the 15 surviving ROOT/text fit-result pairs against the production
parameter table:

```bash
python3 tools/data_preprocessing.py m5-audit \
  --fits-dir "$CSHINE_TIME_FITS" \
  --results-dir "$CSHINE_RESULTS_ROOT" \
  --run-id "$CSHINE_RUN_TAG-m5-audit" \
  --hash-inputs
```

The optional `historical-corrected` spectrum mode preserves a diagnostic
historical macro conversion. It is not the event-level `GammaTime[15]`
producer and is not an upstream dependency of M6.

## 4. Event-level production for both beam roles

M6 must be run twice. The two outputs have different exact manifests and must
not be discovered by directory wildcard.

### M6 beam-on: 60 run groups

```bash
python3 tools/data_preprocessing.py m6 \
  --input-dir "$CSHINE_RAW_ROOT" \
  --calibration \
    "$CSHINE_RESULTS_ROOT/m4/$CSHINE_RUN_TAG-m4/energy_calibration.root" \
  --run-manifest \
    analysis/data_preprocessing/config/central_beam_on_run_groups.tsv \
  --results-dir "$CSHINE_RESULTS_ROOT" \
  --run-id "$CSHINE_RUN_TAG-m6-beam-on"
```

### M6 beam-off: 6 run groups

```bash
python3 tools/data_preprocessing.py m6 \
  --input-dir "$CSHINE_RAW_ROOT" \
  --calibration \
    "$CSHINE_RESULTS_ROOT/m4/$CSHINE_RUN_TAG-m4/energy_calibration.root" \
  --run-manifest \
    analysis/data_preprocessing/config/central_beam_off_run_groups.tsv \
  --results-dir "$CSHINE_RESULTS_ROOT" \
  --run-id "$CSHINE_RUN_TAG-m6-beam-off"
```

Each run writes one `GammaCaliData` tree per manifest group below its
`events/` directory. M6 retains veto and trigger arrays but applies neither
shower reconstruction nor a veto selection.

### M7: neighboring-crystal time diagnostics

M7 is a beam-on diagnostic and consumes the 60 M6 beam-on trees:

```bash
python3 tools/data_preprocessing.py m7 \
  --input-dir \
    "$CSHINE_RESULTS_ROOT/m6/$CSHINE_RUN_TAG-m6-beam-on/events" \
  --run-manifest \
    analysis/data_preprocessing/config/central_beam_on_run_groups.tsv \
  --results-dir "$CSHINE_RESULTS_ROOT" \
  --run-id "$CSHINE_RUN_TAG-m7"
```

Its authoritative numerical output is
`neighbor_time_diagnostics.root`. Display-only scaling remains in the
plotting layer.

M8 is available as a standalone single-file or small-sample diagnostic. It is
not required as a separate command in the full production sequence because
M9 invokes the same M8 executable once for every manifest-defined run group.

## 5. Reconstruction, merging, and diagnostics

### M9: reconstruct and merge the 60 + 6 groups

```bash
python3 tools/data_preprocessing.py m9 \
  --beam-on-input-dir \
    "$CSHINE_RESULTS_ROOT/m6/$CSHINE_RUN_TAG-m6-beam-on/events" \
  --beam-off-input-dir \
    "$CSHINE_RESULTS_ROOT/m6/$CSHINE_RUN_TAG-m6-beam-off/events" \
  --beam-on-run-manifest \
    analysis/data_preprocessing/config/central_beam_on_run_groups.tsv \
  --beam-off-run-manifest \
    analysis/data_preprocessing/config/central_beam_off_run_groups.tsv \
  --results-dir "$CSHINE_RESULTS_ROOT" \
  --run-id "$CSHINE_RUN_TAG-m9"
```

M9 writes the per-run reconstructed trees and the merged per-crystal,
central, edge, and total spectra for each beam role. These exact output
directories feed M10, M10B, and M11.

### Figure-specific 59-group reconstruction spectra

The before/after reconstruction figure uses March 5--10 only: the frozen
59-group sample excludes the additional March 4 group present in the central
60-group analysis. The command rejects a substituted manifest.

```bash
python3 tools/data_preprocessing.py reconstruction-spectra \
  --reconstructed-run-dir \
    "$CSHINE_RESULTS_ROOT/m9/$CSHINE_RUN_TAG-m9/beam-on/reconstructed_runs" \
  --run-manifest \
    analysis/data_preprocessing/config/reconstruction_spectra_figure_run_groups.tsv \
  --results-dir "$CSHINE_RESULTS_ROOT" \
  --run-id "$CSHINE_RUN_TAG-reconstruction-spectra"
```

### M10A: Chapter 3 beam-on and beam-off diagnostics

```bash
python3 tools/data_preprocessing.py m10 \
  --beam-on-input-dir \
    "$CSHINE_RESULTS_ROOT/m9/$CSHINE_RUN_TAG-m9/beam-on/reconstructed_runs" \
  --beam-off-input-dir \
    "$CSHINE_RESULTS_ROOT/m9/$CSHINE_RUN_TAG-m9/beam-off/reconstructed_runs" \
  --beam-on-run-manifest \
    analysis/data_preprocessing/config/central_beam_on_run_groups.tsv \
  --beam-off-run-manifest \
    analysis/data_preprocessing/config/central_beam_off_run_groups.tsv \
  --results-dir "$CSHINE_RESULTS_ROOT" \
  --run-id "$CSHINE_RUN_TAG-m10"
```

The main diagnostic object family uses the reviewed central/edge-veto
selection. The reconstruction-multiplicity family separately uses all 15
crystals with no plastic-veto requirement.

### M10B: trigger diagnostics

```bash
python3 tools/data_preprocessing.py m10b \
  --beam-on-input-dir \
    "$CSHINE_RESULTS_ROOT/m9/$CSHINE_RUN_TAG-m9/beam-on/reconstructed_runs" \
  --beam-on-run-manifest \
    analysis/data_preprocessing/config/central_beam_on_run_groups.tsv \
  --results-dir "$CSHINE_RESULTS_ROOT" \
  --run-id "$CSHINE_RUN_TAG-m10b"
```

`trigger_diagnostics.root` contains both the frozen historical Fig. 15
selection and the separately named author-reviewed selection. A renderer must
choose the intended object family explicitly.

## 6. Background subtraction and final observed spectrum

### M11: slow main route and fast random-window cross-check

```bash
python3 tools/data_preprocessing.py m11 \
  --slow-signal \
    "$CSHINE_RESULTS_ROOT/m9/$CSHINE_RUN_TAG-m9/beam-on/all_recon.root" \
  --slow-background \
    "$CSHINE_RESULTS_ROOT/m9/$CSHINE_RUN_TAG-m9/beam-off/all_recon_BKG.root" \
  --beam-on-input-dir \
    "$CSHINE_RESULTS_ROOT/m9/$CSHINE_RUN_TAG-m9/beam-on/reconstructed_runs" \
  --beam-on-run-manifest \
    analysis/data_preprocessing/config/central_beam_on_run_groups.tsv \
  --results-dir "$CSHINE_RESULTS_ROOT" \
  --run-id "$CSHINE_RUN_TAG-m11"
```

The two outputs are deliberately separate:

```text
results/data_preprocessing/m11/<tag>-m11/slow/spectrum_110.root:histDiff
results/data_preprocessing/m11/<tag>-m11/fast/spectrum_110.root:histDiff
```

The slow file is the authoritative Chapter 3 detector-level observed
spectrum. The fast file is an independent background cross-check.

### M12: read-only observed-spectrum interface check

```bash
python3 tools/data_preprocessing.py m12 \
  --observed-spectrum \
    "$CSHINE_RESULTS_ROOT/m11/$CSHINE_RUN_TAG-m11/slow/spectrum_110.root" \
  --results-dir "$CSHINE_RESULTS_ROOT" \
  --run-id "$CSHINE_RUN_TAG-m12" \
  --hash-inputs
```

M12 validates `histDiff` as a `TH1D` with 200 one-MeV bins over 0--200 MeV,
stored uncertainties, negative-bin preservation, and flow-bin reporting. It
does not copy, rebin, transform, or fit the spectrum.

## 7. Required completion checks

Every stage directory must contain a completed `run_metadata.json`, the exact
`input_manifest.tsv`, the frozen `config_used.txt`, a `run.log`, its stage
report, and the declared ROOT output. For a final archival rerun, add
`--hash-inputs` to the large-data stages if the additional input I/O is
acceptable.

After the run, repeat the repository-level structural checks:

```bash
python3 tools/data_preprocessing.py verify
python3 tools/pipeline_workflow.py check
python3 tools/figure_workflow.py check
python3 tools/repository_closure.py check
```

For numerical validation, compare the new stage reports and ROOT outputs with
the frozen historical outputs using the tolerances appropriate to each
quantity. Do not promote a stage to **numerically validated** solely because
the process exits successfully.

## 8. Thesis figures and tables

The analysis chain above produces the numerical ROOT objects. Figure
rendering and thesis adoption are tracked separately:

- [`../plotting/README.md`](../plotting/README.md) maps each analysis figure to
  its historical ROOT source, portable ROOT/Python renderer, expected input,
  validation, and adopted artifact;
- `plotting/records/` contains machine-readable figure records;
- the private thesis workspace maintains the output-to-code map and exact
  thesis labels.

Source diagrams, apparatus drawings, and author-created geometry figures are
not regenerated by the analysis code. The M5 time-walk parameter table is
audited from surviving historical artifacts because its original batch fitter
is unavailable. These boundaries are part of the reproduction result, not
missing commands to be guessed or replaced.
