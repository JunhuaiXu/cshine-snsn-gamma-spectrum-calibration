# Reproducibility status

This repository preserves and migrates the CSHINE-Gamma calibration and
reconstruction analysis for the 25 MeV/u 124Sn+124Sn experiment, from detector
calibration through the detector-level observed spectrum. It
does not include raw experimental data, generated ROOT/PDF/PNG outputs, or the
later physical interpretation and source-spectrum analyses.

## Evidence levels

The current status is **code migrated, published result not rerun**.  M1--M12
have reviewed source mappings, portable entries, protected output directories,
run metadata, and eighteen ROOT/C++ synthetic tests that passed in ROOT
6.28/04 with GCC 9.4.0.  The local Python orchestration and provenance tests
also pass.  These checks establish the migrated code and object contracts;
they do not claim that the complete experimental sample has been rerun.

The complete stage-to-stage command sequence is now fixed in
[`CHAPTER3_END_TO_END.md`](CHAPTER3_END_TO_END.md). It closes the previous
documentation gap between M4 and the two role-specific M6 runs, and between
those outputs and M9--M12. This makes the migrated chain executable by an
authorized user, but it does not by itself upgrade the evidence level to a
full numerical reproduction.

Several thesis figures have separate real-data figure records because their
historical ROOT objects or approved redraws were checked before the complete
upstream chain was migrated.  Each such record stores the physics contract,
code entry, validation level, author decision, and artifact checksum.

## Known provenance boundary

M0B is source-closed. The historical before/after reconstruction spectrum
comparison is traced to the per-run producer, the exact 59-group March 5--10
sample, the three per-crystal ROOT-object families, and the final plotting
notebook. The portable M8/M9 plus Python path has not been rerun over those 59
experimental groups, so the repository still states **code migrated,
published result not rerun**. The M5 batch producer for the surviving 15
time-fit outputs is unavailable; the repository therefore audits those
historical artifacts without claiming to reproduce the fit.

The remaining result-level boundaries are:

- the complete 60 beam-on plus 6 beam-off sample has not been rerun through
  the unified portable entry;
- the unavailable M5 batch fitter prevents regeneration of the 15 historical
  time-fit ROOT/text pairs from the raw time-amplitude histograms;
- the exact 59-group reconstruction-spectrum figure path is source-closed and
  command-complete, but has not been rerun on the full real sample;
- run-quality, bad-channel, trigger-prescale, dead-time, and effective-exposure
  records are separate experimental-provenance items and are not inferred from
  the analysis code.

## Data access

Users with authorized access must supply the inputs described in
[`DATA_ACCESS.md`](DATA_ACCESS.md).  Commands use public-safe placeholders and
parameterized roots.  Private hosts, accounts, mounts, credentials, and exact
collaborator paths are intentionally excluded.

## Validation commands

```bash
python3 tools/data_preprocessing.py verify
python3 tools/pipeline_workflow.py check
python3 tools/figure_workflow.py check
python3 tools/repository_closure.py check
```

Use `python3 tools/data_preprocessing.py check` in a compatible ROOT build
environment to run the Python and ROOT/C++ synthetic test suites.

## Public-release boundary

Project-level citation and repository metadata are fixed in `CITATION.cff`.
Users are asked to cite both
the 2026 Physical Review C experiment/analysis article and the 2025 Physical
Review Research precision-result article. The README separately identifies the
detector and high-energy calibration component citations without bringing any
out-of-scope code into this repository. The author has approved the BSD
3-Clause License for the public analysis code and documentation. Experimental
data, generated calibration parameters, spectra, figures, and other analysis
products remain outside both the repository and this software license. The
strict repository audit requires both `LICENSE` and the matching
`license: BSD-3-Clause` entry in `CITATION.cff`.
