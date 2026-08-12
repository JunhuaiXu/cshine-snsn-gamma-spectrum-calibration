# Reproducing the CSHINE Sn+Sn gamma-spectrum calibration

This page provides the shortest route from a clean checkout to a verified
analysis environment. The complete data-bearing M2--M12 sequence is maintained
only in [`docs/CHAPTER3_END_TO_END.md`](docs/CHAPTER3_END_TO_END.md).

## Evidence statement

M0B is source-closed, M1--M12 are code-migrated within their documented
boundaries, and M13 closes repository records and the public-data boundary.
The current claim is **code migrated, published result not rerun**. Exact
limits and evidence levels are maintained in
[`docs/REPRODUCIBILITY_STATUS.md`](docs/REPRODUCIBILITY_STATUS.md).

## Requirements

- CMake 3.10 or newer;
- a C++11 compiler;
- ROOT 6.22 or newer with Core, Gpad, Graf, Hist, Imt, RIO, Thread, Tree, and
  TreePlayer;
- Python 3.6 or newer;
- PyROOT, NumPy, and Matplotlib for the optional Python figure renderers.

Install only the Python plotting dependencies, if required, with:

```bash
python3 -m pip install -r plotting/requirements.txt
```

## Repository verification

From the repository root:

```bash
python3 tools/data_preprocessing.py verify
python3 tools/pipeline_workflow.py check
python3 tools/figure_workflow.py check
python3 tools/repository_closure.py check
```

`verify` checks every portable file in the migration manifest. Supply
`--snapshot-root /path/to/DataPreprocessing` only when an authorized read-only
historical snapshot is available and its frozen checksums should also be
checked.

In a compatible ROOT environment, run the full data-free build and test suite:

```bash
python3 tools/data_preprocessing.py check
```

This builds the migrated ROOT/C++ component and runs the Python orchestration
tests and eighteen ROOT/C++ synthetic tests. Passing these checks establishes
code portability and object contracts; it is not evidence that the complete
experimental sample has been rerun.

## Running the analysis

Use [`docs/CHAPTER3_END_TO_END.md`](docs/CHAPTER3_END_TO_END.md) for the only
complete ordered sequence. It specifies:

- the handoff from M2 and M3 into M4;
- separate M6 runs for the 60-group beam-on and 6-group beam-off manifests;
- M7 timing diagnostics;
- M9 reconstruction and merging;
- M10A and M10B diagnostic products;
- slow and fast M11 background routes;
- the read-only M12 observed-spectrum contract check;
- the distinct 59-group sample used by the reconstruction-spectrum figure.

The compact stage crosswalk is
[`docs/ANALYSIS_IO_MAP.md`](docs/ANALYSIS_IO_MAP.md). Exact file, tree,
branch, ROOT-object, selection, binning, and unit requirements are in
[`docs/DATA_ACCESS.md`](docs/DATA_ACCESS.md).

Every data-bearing stage writes to a new protected run directory under
`results/data_preprocessing/<stage>/<run-id>/`. Its record includes the frozen
configuration, exact command, software versions, input manifest, executable
checksum, output checksums, and a stage report. Existing run directories and
output files are rejected rather than overwritten silently. Use
`--hash-inputs` when full input SHA-256 calculation is desired and the added
I/O cost is acceptable.

## Direct component development

The repository-level runner is recommended for analysis execution. For
component development, the ROOT/C++ code can also be built directly:

```bash
cmake -S analysis/data_preprocessing -B build/data_preprocessing
cmake --build build/data_preprocessing
ctest --test-dir build/data_preprocessing --output-on-failure
```

Direct executable names, installed products, and stage-specific physical
contracts are documented in
[`analysis/data_preprocessing/README.md`](analysis/data_preprocessing/README.md).

## Figure reproduction

Figure generation is an output-layer task and does not change the upstream
analysis evidence level. The canonical figure map in
[`plotting/README.md`](plotting/README.md) records, for every stable figure ID:

- historical source and ROOT objects;
- portable ROOT and/or Python entry;
- real-data validation evidence;
- author decision and thesis location.

Public-safe staged records are stored in `plotting/records/`. Generated PDF,
PNG, JSON, CSV, and ROOT files remain below `results/` and are ignored by Git.

For a locally mounted authorized analysis directory, obtain the exact command
from the relevant plotting program with `--help`, for example:

```bash
python3 plotting/plot_cosmic_muon_topology.py --help
```

The adopted cosmic-muon topology redraw uses the final-selection
`DataPreprocessing/step7-DeltaYrelated/h2_check_BKG.root`, not the historical
long-paper branch. The selection matches the analysis-note Fig. 10 producer:
central cores have no plastic-veto requirement and side cores require
`count_veto == 0`. The long-paper implementation is retained only as historical
provenance.

## Bounded remote figure jobs

Short authorized jobs can reuse one SSH connection through
`tools/remote_figure.py`. Copy the public-safe example to the ignored local
directory and add the private host and path information there:

```bash
mkdir -p local
cp docs/remote-config.example.json local/remote.json
chmod 600 local/remote.json
```

Then validate and run one declared job:

```bash
python3 tools/remote_figure.py validate <job-id>
python3 tools/remote_figure.py run <job-id>
```

Job definitions and output contracts are documented in
[`remote_jobs/README.md`](remote_jobs/README.md). The runner transports and
executes declared files only; it does not decide physics selections,
validation status, or thesis adoption.

## Public-data boundary

Raw experimental data, generated ROOT files, spectra, figures, private hosts,
accounts, mounts, and credentials are not part of this repository. Authorized
users supply inputs that satisfy `docs/DATA_ACCESS.md`; all public commands use
replaceable paths.
