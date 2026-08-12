# Reproduction tools

`data_preprocessing.py` is the single orchestration entry for the completed
M0--M12 code-migration boundary, including the evidence-supported M5 spectrum and
artifact audit and the manifest-defined M6 event-tree production. It does not implement any detector calibration,
selection, normalization, or fit; those definitions remain in the migrated
ROOT/C++ analysis under `analysis/data_preprocessing/`.

Run all commands from the repository root. The list below is a command-surface
reference, not an ordered production recipe.

For the ordered M2--M12 production sequence, including the required separate
beam-on and beam-off M6 invocations and all exact output paths, follow
[`../docs/CHAPTER3_END_TO_END.md`](../docs/CHAPTER3_END_TO_END.md). This file
documents the individual command surface; it is not a second end-to-end
workflow definition.

```bash
python3 tools/data_preprocessing.py verify
python3 tools/data_preprocessing.py check
python3 tools/data_preprocessing.py m2 --input-dir /path/to/raw-root-files
python3 tools/data_preprocessing.py m3 --input-dir /path/to/raw-root-files
python3 tools/data_preprocessing.py m4 \
  --source-spectra /path/to/source_background.root \
  --gain-relation /path/to/gain_relation.root
python3 tools/data_preprocessing.py m5-spectra \
  --input-dir /path/to/raw-root-files \
  --mode original
python3 tools/data_preprocessing.py m5-audit \
  --fits-dir /path/to/step3-time/timeFigs/fits \
  --hash-inputs
python3 tools/data_preprocessing.py m6 \
  --input-dir /path/to/raw-root-files \
  --calibration /path/to/energy_calibration.root
python3 tools/data_preprocessing.py m7 \
  --input-dir results/data_preprocessing/m6/RUN/events
python3 tools/data_preprocessing.py m8 \
  --input /path/to/calibrated_run.root
python3 tools/data_preprocessing.py m9 \
  --beam-on-input-dir /path/to/m6/beam_on/events \
  --beam-off-input-dir /path/to/m6/beam_off/events
python3 tools/data_preprocessing.py m10 \
  --beam-on-input-dir /path/to/m9/beam-on/reconstructed_runs \
  --beam-off-input-dir /path/to/m9/beam-off/reconstructed_runs
python3 tools/data_preprocessing.py m10b \
  --beam-on-input-dir /path/to/m9/beam-on/reconstructed_runs
python3 tools/data_preprocessing.py m11 \
  --slow-signal /path/to/m9/beam-on/all_recon.root \
  --slow-background /path/to/m9/beam-off/all_recon_BKG.root \
  --beam-on-input-dir /path/to/m9/beam-on/reconstructed_runs
python3 tools/data_preprocessing.py m12 \
  --observed-spectrum /path/to/m11/slow/spectrum_110.root \
  --hash-inputs
```

- `verify` checks the SHA-256 values of all non-generated portable artifacts.
  Add `--snapshot-root /path/to/DataPreprocessing` to check the frozen M0
  sources.
- `check` runs the tool tests, configures and builds the ROOT/C++ component,
  and runs its eighteen synthetic tests. Use `--root-dir` to select a ROOT CMake
  package explicitly.
- `m2`, `m3`, and `m4` create a new
  `results/data_preprocessing/<stage>/<run-id>/` directory and execute the
  corresponding compiled analysis. Existing run directories are rejected.
- `m5-spectra` resolves the nine reviewed raw-file patterns and runs either the
  original time-amplitude histogram producer or the explicitly diagnostic
  historical-corrected producer in a protected result directory.
- `m5-audit` checks the 15 surviving ROOT/text fit-output pairs and compares
  their parameters with the production time-correction table. It does not
  refit the timing distributions.
- `m6` validates and expands the run-group manifest, then writes one protected
  `GammaCaliData` ROOT file and one report per group. The default manifest has
  60 beam-on groups; shower reconstruction and veto selection are not run.
- `m7` resolves those exact 60 M6 outputs and writes one protected ROOT file
  containing `h2_all`, `h2_cut`, `h1`, `hh_diff`, `h3`, and `h4`. It performs
  no shower reconstruction or veto selection and stores the selected
  time-difference histogram before the display-only peak scaling.
- `m8` accepts one or more explicit M6 trees, retains their event-level
  branches, and adds the historical shower-reconstruction and three-face veto
  results and per-crystal reconstructed histograms.
- `m9` resolves the exact 60 beam-on and 6 beam-off M6 outputs from separate
  manifests, runs M8 once per group, merges the per-crystal histograms, and
  constructs the historical central, side, and total spectra for each sample.
- `m10` resolves the exact M9 per-run reconstructed trees and writes the
  reviewed central beam-on and beam-off Chapter 3 diagnostics. It reads the
  stored reconstruction and keeps the main central/side-veto definition
  separate from the all-15 no-veto multiplicity definition.
- `m10b` resolves the exact beam-on reconstructed trees, writes all 15
  trigger-monitor spectra, and writes five trigger-conditioned time-energy
  objects for both the frozen historical and author-reviewed veto policies.
- `m11` keeps the slow beam-off and fast random-window routes separate. It
  normalizes the slow background over 110--200 MeV, forms equal-width fast and
  random timing-window spectra after strict SSD M2 exclusion, and writes both
  `histDiff` outputs with complete run metadata.
- `m12` validates the authoritative slow-route `histDiff` as a laboratory-frame
  `TH1D` with 200 one-MeV bins from 0 to 200 MeV and stored per-bin errors. It
  writes only an interface report and run metadata; it does not copy, rebin,
  transform, or otherwise alter the observed spectrum.

Run metadata contain the exact command, frozen configuration, software
versions, executable and output checksums, input paths, file sizes, and
modification times. Add `--hash-inputs` to compute full raw-input SHA-256
values; this is optional because the authorized ROOT dataset may be large.

The tests under `tests/` use temporary synthetic files. They validate the
orchestration and metadata layer only and do not replace the ROOT/C++ physics
tests or a separately authorized real-data comparison.

## Historical source index

`source_index.py` builds a read-only discovery index from an immutable
`DataPreprocessing/` source snapshot. It records only relative source paths,
file sizes, SHA-256 values, includes, candidate symbols, ROOT branch names,
ROOT object names, and ROOT-file string literals. Exact duplicate groups are
defined by full-file SHA-256.

Build and query the index from the repository root:

```bash
python3 tools/source_index.py build \
  --snapshot-root /path/to/immutable/DataPreprocessing
python3 tools/source_index.py check
python3 tools/source_index.py query gamma_time_cali --field includes
python3 tools/source_index.py query ALL_h2_TOF_TotalE --field root_objects --paths-only
python3 tools/source_index.py duplicates --path-contains step3-time
```

The generated index is written under ignored `results/source_index/` by
default. Historical ROOT-file literals may contain internal absolute paths,
so the generated index is an internal artifact and must not be committed. A
match is a candidate source only. The tool never labels a file as the central
production version and never edits the snapshot.

## Chapter 3 pipeline registry

`pipeline_workflow.py` validates and queries the reviewed stage registry at
`analysis/data_preprocessing/provenance/pipeline_stages.json`. The registry
connects M0B--M13 dependencies, historical source IDs, portable entries,
external input contracts, produced artifacts, ROOT objects, thesis outputs,
and unresolved questions.

```bash
python3 tools/pipeline_workflow.py check
python3 tools/pipeline_workflow.py status
python3 tools/pipeline_workflow.py next
python3 tools/pipeline_workflow.py show M5
python3 tools/pipeline_workflow.py trace histDiff
python3 tools/pipeline_workflow.py graph
```

Structural validation checks dependency cycles, missing artifacts, unknown
source IDs, missing portable files, unknown thesis-output IDs, and private
path markers. It also reports thesis figures that lack a machine-readable
figure-workflow record. Passing validation means that the reviewed registry
is internally consistent; it does not mean that planned stages or published
results have been rerun.

## Reused SSH figure jobs

`remote_figure.py` reduces one remote figure task to a controlled sequence:
validate local sources, open or reuse one SSH master connection, upload all
declared files together, execute one isolated job, download one result
directory, and compare remote and local SHA-256 values. It has no automatic
retry loop and never modifies the historical analysis tree.

Private host names, accounts, ports, and absolute paths belong in the ignored
file `local/remote.json`. Start from the public-safe example:

```bash
mkdir -p local
cp docs/remote-config.example.json local/remote.json
chmod 600 local/remote.json
```

If non-interactive sessions require a site-specific software initialization,
place each setup command in the private `environment_setup` list. The tool
runs those commands once at the start of the isolated remote job. Installation
paths therefore remain outside public job definitions, while all uploaded
plotting programs can use the same configured ROOT environment.

List and validate public-safe job definitions without contacting a server:

```bash
python3 tools/remote_figure.py list
python3 tools/remote_figure.py validate trigger-monitoring-figures
```

Run one job and let the connection expire after the configured idle period:

```bash
python3 tools/remote_figure.py run trigger-monitoring-figures
```

Each run uses unique local and remote directories of the form
`results/remote_runs/<job-id>/<run-id>/`. Existing local run directories are
rejected. The downloaded directory contains the declared figure outputs and
`remote_workflow.json`, which records job and script checksums and confirms
the remote-to-local artifact checks. A generated figure remains a candidate;
this tool does not perform scientific validation or thesis integration.

Inspect or explicitly close the shared connection with:

```bash
python3 tools/remote_figure.py status
python3 tools/remote_figure.py close
```

`ControlPersist` normally removes the idle connection automatically, so
routine runs do not need the explicit close command.

The event-display provenance audit uses two read-only helpers through this
runner:

- `audit_event_display_sources.py` inventories files below the historical
  `EventDisplay/` directory and records sizes, times, and SHA-256 values;
- `export_event_display_candidates.py` copies only small PDF candidates into
  an isolated result directory and writes a checksum manifest.

They do not open raw ROOT data or infer event selections. Their purpose is to
establish the historical inventory before the separately defined
`event-display-reproduction` task runs the frozen selection and exact-bin
validator. The latter task is documented under `remote_jobs/` and writes only
to an isolated run directory.

## Figure-workflow records

`figure_workflow.py` creates and validates one public-safe JSON record for each
newly traced or redrawn thesis figure. It prevents common bookkeeping errors:

- a portable plotting entry cannot be marked implemented before the physics
  contract is frozen;
- validation cannot be recorded before a portable entry exists;
- a reproducible redraw cannot be marked author-confirmed without real-data or
  numerical validation;
- thesis use requires an author-confirmed artifact and checksum;
- internal server paths and common credential markers are rejected.

Create a staged record from the repository root:

```bash
python3 tools/figure_workflow.py init \
  --stable-id example-figure \
  --title "Example figure" \
  --thesis-section "Sec. 3.3" \
  --thesis-label fig:example-figure \
  --historical-source DataPreprocessing/path/to/source.C
```

Check one record or all records:

```bash
python3 tools/figure_workflow.py check plotting/records/example-figure.json
python3 tools/figure_workflow.py check
python3 tools/figure_workflow.py summary
```

Record a generated artifact without storing a private absolute path:

```bash
python3 tools/figure_workflow.py artifact \
  --record plotting/records/example-figure.json \
  --stage candidate \
  --role pdf \
  --path results/figures/example/candidate.pdf
```

The tool records file size and SHA-256 but never executes the physics analysis,
compares scientific results, approves a figure, or edits the thesis. The full
human workflow is documented in `../../FIGURE_REPRODUCTION_WORKFLOW.md` outside
the public-safe reproduction directory.

## Repository-closure audit

`repository_closure.py` combines the non-data M13 checks without running the
physics analysis:

```bash
python3 tools/repository_closure.py check
python3 tools/repository_closure.py check \
  --output results/repository_closure/report.json
```

It verifies the migration manifest, stage registry, figure records, required
navigation documents, project citation metadata, M2--M12 command coverage,
and the absence of private path markers in public source files. Ignored
`local/`, `results/`, `build/`, and cache directories are outside the
public-source scan.

The approved `CITATION.cff` and BSD 3-Clause `LICENSE` are required by the
release audit. The citation metadata must also declare
`license: BSD-3-Clause`. Before creating a public repository, run:

```bash
python3 tools/repository_closure.py check --strict-release
```

The strict form fails if the author-approved license or matching citation
metadata is absent. Passing either form does not claim that the full
experimental data set or published result has been rerun, and the license does
not apply to excluded experimental data or generated analysis products.
