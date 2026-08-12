# CSHINE Sn+Sn gamma-spectrum calibration and reconstruction

This repository preserves the calibration and reconstruction chain used to
obtain the detector-level gamma-ray spectrum measured with CSHINE-Gamma in the
25 MeV/u 124Sn+124Sn experiment. Its scope is the experimental data-processing
sequence:

```text
calibration and time correction
  -> gamma-event reconstruction and background subtraction
  -> measured detector-level gamma-ray spectrum
```

The high-energy part of this spectrum is the experimental observable used by
later physics analyses. IBUU transport calculations, detector-response
folding, high-momentum-tail inference, and source-spectrum unfolding are not
part of this repository; they are maintained as separate projects.

## Operating model

The historical production code, data, and outputs remain unchanged under the
authorized server analysis root. The local read-only source snapshot is kept
outside this directory in `../gamma2024_code_snapshot/` for provenance.

This project has three distinct code layers:

- the immutable historical snapshot outside this directory, which preserves
  the original ROOT analysis and plotting sources;
- `analysis/`: the migrated calibration and spectrum-reconstruction analysis;
- `plotting/`: portable ROOT plotting entries plus optional Python programs
  for thesis-oriented figure layouts.

The migrated code under `DataPreprocessing/` defines the complete repository
scope. Later physics analyses are maintained, if released, as separate
projects rather than future subdirectories of this repository.

The planned companion projects are:

- `cshine-snsn-src-inference`: IBUU inputs, detector-response production and
  folding, high-momentum-tail inference, and inference systematics;
- `hard-photon-spectrum-unfolding`: source-spectrum unfolding, uncertainty
  propagation, and refolding checks.

The response-matrix implementation and version record belong to the inference
project. The unfolding project consumes the same version through a documented
matrix schema and checksum rather than copying the matrix-production code.

Generating a figure does not mean that its upstream analysis has been
migrated, and it does not approve the figure for the thesis. Every output remains
a candidate until its source, numerical content, and presentation have been
reviewed and the author explicitly decides to include it.

## Repository layout

```text
.
├── analysis/
│   └── data_preprocessing/    current active analysis stage
├── plotting/                  ROOT figure entries and Python presentation helpers
│   └── records/               public-safe staged figure work records
├── remote_jobs/               public-safe bounded remote job definitions
├── tools/                     analysis and figure-workflow checks
├── docs/                      public-safe data-access documentation
├── local/                     ignored private connection configuration
├── results/                   generated outputs; ignored by Git
├── README.md
└── REPRODUCE.md
```

## Start here

1. [`REPRODUCE.md`](REPRODUCE.md): active stage and reproduction status;
2. [`docs/CHAPTER3_END_TO_END.md`](docs/CHAPTER3_END_TO_END.md): the single
   ordered M2--M12 runbook, including the separate beam-on and beam-off M6
   productions and their exact handoff to M9--M12;
3. [`analysis/data_preprocessing/README.md`](analysis/data_preprocessing/README.md):
   current historical code and data-processing scope;
4. [`plotting/README.md`](plotting/README.md): one-to-one figure/code map and
   author-review status;
5. [`tools/README.md`](tools/README.md): M0--M12 orchestration, source indexing,
   pipeline-registry checks, figure records, and remote jobs;
6. [`remote_jobs/README.md`](remote_jobs/README.md): bounded remote-job schema;
7. [`docs/DATA_ACCESS.md`](docs/DATA_ACCESS.md): portable input layout and data
   boundary.

For short server-side figure jobs, `tools/remote_figure.py` reuses one SSH
master connection for upload, execution, and download. Public job definitions
live in `remote_jobs/`; private host and absolute-path settings remain in the
ignored `local/remote.json`. This avoids repeated authentication while keeping
server details outside this public repository.

For each newly traced or redrawn figure, initialize and validate one staged
record before writing a new plotting program:

```bash
python3 tools/figure_workflow.py init --help
python3 tools/figure_workflow.py check
python3 tools/figure_workflow.py summary
```

The records capture provenance and status dependencies only. They do not
replace the project-wide thesis-output map, numerical validation, or the
author's figure-acceptance decision.

## Current status

The server-side `DataPreprocessing/` structure and the central measured
spectrum chain have been traced without modifying or rerunning the historical
code. The migration target now covers the complete Chapter 3 central chain.
The original M0 source freeze records 32 exact historical paths and verified local
snapshot checksums. M1 provides the minimal calibration-only ROOT library and
dictionary for the 2024-03-08 chain. Its data-free logic and ROOT streaming
tests pass in a compatible ROOT 6.28 environment, and the library reads the
historical `cali_20240308` class-version-2 object. M2 adds a configurable entry
for the 2024-03-08 radioactive-source background treatment and a synthetic
ROOT test of the time selection, live-time normalization, object schema, and
error propagation. M2 is complete as a source-based migration; the published
45-file analysis has intentionally not been rerun. M3 provides the configurable 2024-03-08
low-/high-gain relation fit, preserves the historical ROOT fit and `f_data`
object semantics, and passes build, installation, and synthetic ROOT tests.
M4 now provides the configurable 2024-03-08 three-point energy calibration.
It preserves the historical channel-specific source-peak windows, Co-60 and
Th-nat fit models, Gaussian-width x-error semantics, `f_data` embedding,
`cali_20240308` object, and 4-by-4 canvas layout. The M1--M4 suite of five
synthetic ROOT/C++ tests passed in the authorized server environment on
2026-08-11. M5 is now code-migrated to the limit supported by the surviving
evidence. It includes the original and historical-diagnostic time-amplitude
histogram producers, preserves the published 15-channel production formula,
and keeps the diagnostic expression separate. The batch
fit producer for the 15 ROOT/text parameter outputs is absent from both the
archived source and a bounded read-only inventory of the surviving server
directory. A dedicated M5 audit now checks the surviving ROOT objects, fit
summaries, and their equality to the production parameter table without
inventing a replacement fit. No experimental input or published calibration
result was rerun. M6 now preserves the exact event-level energy and corrected
time production, the retained raw ADC/TDC arrays, and all 60 beam-on run
groups. M7 adds the six neighboring-time diagnostic objects. M8 preserves the
historical shower algorithm, central/side/corner roles, 1 MeV and 50/100 ns
boundaries, three-face veto rule, and reconstructed ROOT storage. M9 freezes
the 60 beam-on and 6 beam-off groups, runs M8 per group, and merges the
per-crystal, central, side, and total spectra. All 13 registered M1--M9 tests
pass in the compatible server environment. The
complete historical production was not rerun.

M0B now closes the source freeze through the complete Chapter 3 chain, while
M5 remains closed at its evidence-supported code boundary.
`tools/source_index.py` provides a read-only candidate index
of the immutable snapshot, while `tools/pipeline_workflow.py` validates the
reviewed M0B--M13 dependency, artifact, ROOT-object, source-ID, and thesis-output
registry. The registry and all currently declared figure-workflow records pass
structural checks. The reviewed central M9--M12 chain now has frozen source IDs
for the run list, merged spectra, diagnostic objects, trigger variants, slow
background subtraction, and central/edge comparison. M11 now freezes and
migrates the fast-background upstream chain separately from the slow beam-off
route. The before/after reconstruction spectrum chain is frozen to the
historical per-run producer, final notebook, exact 59-group March 5--10 sample,
and M8/M9 histogram families. This figure sample remains distinct from the
reviewed 60-group central-analysis sample.

The completed M0--M12 code boundary has one recommended entry:

```bash
python3 tools/data_preprocessing.py verify
python3 tools/data_preprocessing.py check
```

The first command validates every portable file recorded in the migration
manifest. Add `--snapshot-root /path/to/DataPreprocessing` to validate all
frozen historical sources as well. The second command repeats the portable
manifest check, runs the Python orchestration suite, builds the ROOT/C++
code, and runs the eighteen ROOT/C++ synthetic tests.

M13 adds a public-boundary and documentation audit:

```bash
python3 tools/repository_closure.py check
```

Its exact evidence level, known provenance gaps, and release requirements are
summarized in [`docs/REPRODUCIBILITY_STATUS.md`](docs/REPRODUCIBILITY_STATUS.md).

The same entry runs M2, M3, and M4 into new, non-overwriting result directories:

```bash
python3 tools/data_preprocessing.py m2 \
  --input-dir /path/to/raw-root-files \
  --run-id central-0308

python3 tools/data_preprocessing.py m3 \
  --input-dir /path/to/raw-root-files \
  --run-id central-0308

python3 tools/data_preprocessing.py m4 \
  --source-spectra /path/to/20240308_ThnatCo60_NoBkg.root \
  --gain-relation /path/to/20240308_SnSn_GOAL_ALLCOIN.root \
  --run-id central-0308

python3 tools/data_preprocessing.py m5-spectra \
  --input-dir /path/to/raw-root-files \
  --mode original \
  --run-id central-0303-0310

python3 tools/data_preprocessing.py m5-audit \
  --fits-dir /path/to/step3-time/timeFigs/fits \
  --run-id historical-central \
  --hash-inputs

python3 tools/data_preprocessing.py m6 \
  --input-dir /path/to/raw-root-files \
  --calibration /path/to/energy_calibration.root \
  --run-id central-beam-on

python3 tools/data_preprocessing.py m7 \
  --input-dir results/data_preprocessing/m6/central-beam-on/events \
  --run-id central-neighbor-time

python3 tools/data_preprocessing.py m8 \
  --input /path/to/calibrated_run.root \
  --run-id shower-check

python3 tools/data_preprocessing.py m9 \
  --beam-on-input-dir /path/to/m6-beam-on/events \
  --beam-off-input-dir /path/to/m6-beam-off/events \
  --run-id central-spectra

python3 tools/data_preprocessing.py m10 \
  --beam-on-input-dir /path/to/m9/beam-on/reconstructed_runs \
  --beam-off-input-dir /path/to/m9/beam-off/reconstructed_runs \
  --run-id central-chapter3-diagnostics
```

Each run records the frozen configuration, exact command, software versions,
executable checksum, input paths and file metadata, output checksums, and the
analysis program's own ROOT/fit report. Full input SHA-256 calculation is
available with `--hash-inputs`; it is not enabled by default because the raw
ROOT dataset may be large. Parent output directories are created
automatically, while existing run directories and output files remain
protected from overwrite.

The `m5-spectra` entry writes the reviewed original time-amplitude ROOT
objects from the nine historical file patterns. Its optional
`historical-corrected` mode preserves the diagnostic macro semantics and is
not formal event-level time production. The complete historical raw data set
has not been rerun as part of migration.

The first standalone Python plotting program implements the historical
time-amplitude correction from existing ROOT histograms and fit parameters.
It supports one selected channel or all 15 channels. The complete mode writes
15 individual before/after pairs, two 4 x 4 overviews, and a validation table
containing the fit parameters, input hashes, and count-conservation checks;
an accompanying JSON records the script and software environment. The final
all-channel server run passed every count check, and the author-approved
shared-scale CsI05 pair and overview figures are used in the thesis.

The second plotting program corresponds to the CsI05--CsI06 corrected-time
correlation in Sec. 3.3.2. It preserves the historical 60-file list, ROOT
expressions, 30 MeV energy selection, binning, and range, while replacing the
external presentation composition with one direct horizontal output. The
program passed syntax, exact-file-list, and synthetic horizontal-layout checks,
then ran with the authorized server input. Its built-in per-bin subset check
passed, and the author-approved independent-scale PDF is used in thesis
Sec. 3.3.2.

The third plotting program reproduces the one-dimensional distributions used
in analysis-note Fig. 6 and thesis Sec. 3.3.2: the CsI05 corrected time and the
CsI05--CsI06 corrected-time difference before and after the 30 MeV selection.
The portable ROOT reference and Python entry share the exact 60-file list,
tree expressions, selections, bins, ranges, and peak-height scaling. Both ran
with the authorized input and independently obtained the same scale factor;
the author-approved horizontal PDF is used in the thesis.

The fourth plotting program is a bounded entry for analysis-note Fig. 8. Its
current mode reads three ROOT objects generated together by the focused C++
diagnostic: reconstructed cluster size, all-core multiplicity, and high-energy
core multiplicity. All 15 CsI(Tl) units are admitted as possible cores and no
plastic-veto requirement is applied. The high-energy curve counts individual
candidates above 35 MeV and gives 7218/21. The historical `Drawhistos.C`
constants 6731/19 remain in metadata only. The numerically checked PDF has
been accepted by the author and copied unchanged into thesis Sec. 3.3.5.

The fifth plotting program traces analysis-note Fig. 10 to the two
`h2_check.root` spatial-correlation objects for 10--100 MeV and above 100 MeV.
The focused ROOT reference preserves the historical object-level drawing, and
the Python entry arranges those unchanged counts horizontally with independent
logarithmic color scales. The physics contract, source hashes, and portable
input structure are frozen. The entry ran on the authorized ROOT output with
83,807 low-energy and 1,574 high-energy candidates; all counts lie inside the
historical axis ranges. The author-confirmed PDF is used in thesis Sec. 3.4.1.

The beam-off topology plotting entry retains analysis-note Fig. 18 and its
dedicated long-paper producer as historical provenance, but uses the later
`step7-DeltaYrelated/h2_check_BKG.root` for the thesis figure. The author
confirmed that its central/side-core and plastic-veto definition must match
analysis-note Fig. 10: central candidates have no veto requirement and side
candidates require `count_veto == 0`. The output layer reads the high-energy
spatial-spread and core-versus-total-energy objects without rebinning or
normalization and arranges them horizontally with independent logarithmic
count scales. The real-data run checked both object schemas, entries,
displayed counts, flow counts, and transfer hashes. The result remains a
candidate generated from the author-confirmed physics selection. The PDF was
copied unchanged into thesis Sec. 3.5.1 after author approval.

Historical PDF or slide composition is never accepted as a replacement for
numerical reproduction. The portable ROOT reference for the third figure is
available under `plotting/root/`; upstream time-amplitude ROOT-object
production is now covered by M5, while the
two-dimensional neighboring-crystal objects remain part of M7. These
figure-level results do not imply that the
upstream event-processing chain has already been migrated.

## Citation

Please identify the exact software commit used and cite **both** associated
articles below. The Physical Review C article documents the complete experiment,
calibration, reconstruction, background treatment, and spectrum analysis. The
Physical Review Research article reports the corresponding precision
short-range-correlation result. Machine-readable metadata for both articles is
provided in [`CITATION.cff`](CITATION.cff).

```bibtex
@article{Xu2026SnSnGamma,
  author  = {Xu, Junhuai and Niu, Qinglin and Qin, Yuhao and Si, Dawei and
             Wang, Yijie and Xiao, Sheng and Tian, Baiting and Qin, Zhi and
             Zhang, Haojie and Zhang, Boyuan and Guo, Dong and Fu, Minxue and
             Wei, Xiaobao and Hao, Yibo and Wang, Zengxiang and Zhuo, Tianren and
             Ma, Chunwang and Yang, Yuansheng and Wei, Xianglun and Yang, Herun and
             Ma, Peng and Duan, Limin and Duan, Fangfang and Wang, Kang and
             Ma, Junbing and Xu, Shiwei and Bai, Zhen and Yang, Guo and
             Yang, Yanyun and Xu, Mengke and Chen, Kaijie and Hao, Zirui and
             Fan, Gongtao and Wang, Hongwei and Xu, Chang and Xiao, Zhigang},
  title   = {Experimental Study of Bremsstrahlung Gamma-Ray Emission and
             Short-Range Correlations in {$^{124}$Sn+$^{124}$Sn} Collisions at
             25 {MeV}/Nucleon},
  journal = {Physical Review C},
  volume  = {113},
  number  = {4},
  pages   = {044613},
  year    = {2026},
  doi     = {10.1103/dhz2-nl56},
  url     = {https://doi.org/10.1103/dhz2-nl56}
}
```

```bibtex
@article{Xu2025SRC,
  author  = {Xu, Junhuai and others},
  title   = {Precise Measurement of Short-Range Correlations in Nuclei from
             Bremsstrahlung Gamma-Ray Emission in Low-Energy Heavy-Ion Collisions},
  journal = {Physical Review Research},
  volume  = {7},
  number  = {4},
  pages   = {043174},
  year    = {2025},
  doi     = {10.1103/jw1p-36pb},
  url     = {https://doi.org/10.1103/jw1p-36pb}
}
```

The following component citations should also be included when the corresponding
detector description or high-energy calibration is used:

```bibtex
@article{Qin2023CSHINEGamma,
  author  = {Qin, Yuhao and others},
  title   = {A {CsI(Tl)} Hodoscope on {CSHINE} for Bremsstrahlung Gamma Rays in
             Heavy-Ion Reactions},
  journal = {Nuclear Instruments and Methods in Physics Research Section A},
  volume  = {1053},
  pages   = {168330},
  year    = {2023},
  doi     = {10.1016/j.nima.2023.168330},
  url     = {https://doi.org/10.1016/j.nima.2023.168330}
}

@article{Xu2025CsIResponse,
  author  = {Xu, Junhuai and Si, Dawei and Qin, Yuhao and Xu, Mengke and
             Chen, Kaijie and Hao, Zirui and Fan, Gongtao and Wang, Hongwei and
             Wang, Yijie and Xiao, Zhigang},
  title   = {Linear Response of {CsI(Tl)} Crystal to Energetic Photons below
             20 {MeV}},
  journal = {Nuclear Instruments and Methods in Physics Research Section A},
  volume  = {1080},
  pages   = {170787},
  year    = {2025},
  doi     = {10.1016/j.nima.2025.170787},
  url     = {https://doi.org/10.1016/j.nima.2025.170787}
}
```

The requirement to cite both PRC and PRR does not change the repository scope:
IBUU calculations, detector-response folding, high-momentum-tail inference,
and source-spectrum unfolding remain in separate projects.

## License

The analysis source code and documentation in this repository are released
under the [BSD 3-Clause License](LICENSE). The license does not grant access to,
or redistribute, experimental ROOT files, generated calibration parameters,
generated spectra, figures, or other analysis products. Those materials are
outside the public repository and must be obtained through their applicable
data-access and collaboration procedures.

## Data and publication boundary

Experimental ROOT files, generated spectra, PDFs, and PNGs are not
distributed. Generated numerical reference outputs are likewise excluded;
the corresponding programs and output contracts remain available so that an
authorized user can regenerate them from the required inputs. Source code
published in this GitHub
repository must remain configurable and must not contain credentials or embed
internal server paths in code or public documentation. Exact collaborator
paths remain in private workspace records outside this directory.
