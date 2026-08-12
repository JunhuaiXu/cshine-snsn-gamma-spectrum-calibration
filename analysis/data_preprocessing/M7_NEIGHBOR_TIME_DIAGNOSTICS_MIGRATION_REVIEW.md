# M7 neighboring-crystal timing diagnostics migration review

> Historical closure record. Current status is maintained in
> `../../docs/REPRODUCIBILITY_STATUS.md`; current commands are maintained in
> `../../docs/CHAPTER3_END_TO_END.md`.

## Scope

M7 covers only the CsI05--CsI06 timing diagnostics used in thesis Sec. 3.3.2.
It consumes the calibrated `GammaCaliData` trees produced by M6 and does not
perform energy calibration, time correction, shower reconstruction, veto
selection, or gamma-spectrum construction.

The immutable historical source is source ID `DP-S504`:

```text
DataPreprocessing/step4-convert.0308.PreRun/draw_GammaTimeDiff.C
SHA-256: 6ad88f3440b6b79356f13701769d2d65eb48027f61fafcd050fc904cedcfd2c6
```

## Numerical contract

The producer reads the exact 60 M6 files listed in
`config/central_beam_on_run_groups.tsv` and writes one protected ROOT file:

| Object | ROOT type | Quantity | Selection | Binning |
|---|---|---|---|---|
| `h2_all` | `TH2F` | `GammaTime[6]:GammaTime[5]` | none | 100 x 100, both axes -500--500 ns |
| `h2_cut` | `TH2F` | `GammaTime[6]:GammaTime[5]` | `GammaEnergy[5]+GammaEnergy[6]>=30` | same as `h2_all` |
| `h1` | `TH1F` | `GammaTime[5]-GammaTime[6]` | none | 100 bins, -200--200 ns |
| `hh_diff` | `TH1F` | `GammaTime[5]-GammaTime[6]` | `GammaEnergy[5]+GammaEnergy[6]>=30` | same as `h1` |
| `h3` | `TH1F` | `GammaTime[5]` | none | 100 bins, -500--500 ns |
| `h4` | `TH1F` | `GammaTime[6]` | none | 100 bins, -500--500 ns |

`hh_diff` is stored without scaling. The historical comparison plot multiplies
it by `maximum(h1)/maximum(hh_diff)` only at display time. The producer records
that factor in its TSV report so a rendering program cannot silently convert a
display convention into a numerical analysis step.

## Portable implementation

The numerical layer is:

```text
include/neighbor_time_diagnostics.h
src/neighbor_time_diagnostics.cxx
apps/build_neighbor_time_diagnostics.cxx
tests/test_neighbor_time_diagnostics.cxx
```

The unified command `python3 tools/data_preprocessing.py m7` resolves the exact
M6 output names from the run manifest, records all inputs and software
metadata, and creates:

```text
results/data_preprocessing/m7/<run-id>/
├── neighbor_time_diagnostics.root
├── run_report.tsv
├── input_manifest.tsv
├── config_used.txt
├── run.log
└── run_metadata.json
```

The display layer reads this ROOT file:

```text
plotting/root/draw_neighbor_time_diagnostics.C
plotting/plot_neighbor_time_correlation.py
plotting/plot_unit_time_and_neighbor_difference.py
```

The two Python programs retain `--analysis-root` only as a legacy direct-tree
provenance check. New production uses `--diagnostics-root`, so the event
selection and histogram filling exist in one authoritative numerical layer.

## Validation status

- The historical source hash, expressions, selections, object names, types,
  binning, and ranges are frozen.
- A synthetic ROOT test checks all six objects, selected/no-cut counts,
  selected-bin subset behavior, the unscaled `hh_diff`, the historical peak
  ratio, reports, nested output creation, overwrite protection, and missing
  branch rejection.
- The isolated ROOT 6.28/04 and GCC 9.4.0 server build passed all ten M1--M7
  C++ tests. Both Python `--diagnostics-root` routes also read the synthetic M7
  file and produced their declared outputs.
- The already adopted thesis figures were previously generated and checked
  from the same 60 real-data trees through the retained direct-tree route. The
  time-difference peak-scale factor was `52.00885` in both ROOT and Python.
- The new M7 producer has not yet been run over the authorized 60-file
  real-data set. Therefore M7 is `code_migrated`, not `real_data_reproduced`.

## Next boundary

M8 begins shower reconstruction, crystal adjacency, core selection, timing
conditions, and plastic-veto treatment. None of those definitions belongs in
M7.
