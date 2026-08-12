# M9 run and spectrum-merge migration review

> Historical closure record. Current status is maintained in
> `../../docs/REPRODUCIBILITY_STATUS.md`; current commands are maintained in
> `../../docs/CHAPTER3_END_TO_END.md`.

## Scope

M9 closes the run-group and spectrum-merging stage between the M8 per-event
reconstruction and the later Chapter 3 diagnostic and background-subtraction
stages. It performs four operations without changing the M8 event-level
physics definition:

1. resolve exactly 60 beam-on and 6 beam-off run groups from reviewed TSV
   manifests;
2. run the M8 reconstruction once for every group and retain its per-crystal
   histograms;
3. merge the per-run histograms separately for beam-on and beam-off samples;
4. construct the central, side, and total reconstructed-energy spectra used
   by the historical central analysis.

Trigger-specific branches, Chapter 3 diagnostic ROOT objects, slow/fast
background subtraction, and the final `histDiff` spectrum remain outside M9.

## Frozen sources and run lists

| Source ID | Historical path | Role |
|---|---|---|
| `DP-S600` | `step4-convert.0308/aa_example.C` | per-run tree and per-crystal histograms |
| `DP-S601` | `step4-convert.0308/Makefile` | generic target-to-macro expansion rule |
| `DP-S602` | `step4-convert.0308/README` | explicit 60 beam-on and 6 beam-off run-group commands |
| `DP-S603` | `step4-convert.0308/all_recon.C` | beam-on central, side, and total spectra |
| `DP-S604` | `step4-convert.0308/all_recon_BKG.C` | beam-off central, side, and total spectra |

The Makefile does not enumerate the physical sample. It copies the generic
macro, substitutes one explicit target name, and invokes ROOT. The authoritative
run-group membership is therefore frozen in
`config/central_beam_on_run_groups.tsv` and
`config/central_beam_off_run_groups.tsv` from the historical README command.
The six beam-off groups are independently corroborated by the downstream
background diagnostic source `step7-DeltaYrelated/h2_check_BKG.C`.

Manifest rows are resolved to exact M6 output names. M9 does not search an
output directory with a wildcard and rejects duplicate inputs, missing files,
and existing output directories.

## Preserved ROOT-object contract

Each reconstructed run contains:

- `GammaCaliData`, retaining the M6 branches and adding `recon_result` and
  `count_veto`;
- `h_eDep_0`--`h_eDep_14`, filled only when the corresponding corrected time
  is finite;
- `h_recon_0`--`h_recon_14`, filled for every historical reconstruction-map
  entry, including zero-energy invalid-center placeholders;
- `h_recon_veto_0`--`h_recon_veto_14`, filled when all three plastic-veto
  faces are silent;
- `h_recon_vetoed_0`--`h_recon_vetoed_14`, filled when at least one veto face
  fires.

The merged `all_notree` layer contains the sum of all 60 per-crystal
histograms and no event tree. The following spectra are then built with the
historical 1000-bin, 0--200 MeV definition:

- `h_central_E_M1`: sum of `h_recon_5`, `6`, `9`, and `10`;
- `h_side_E_M1`: sum of `h_recon_veto_4`, `7`, `8`, `11`, `13`, and `14`;
- `h_total_E_M1`: central plus side spectra.

The beam-on outputs retain the historical names `all_notree.root` and
`all_recon.root`; the beam-off outputs use `all_notree_BKG.root` and
`all_recon_BKG.root`. The diagnostic canvas `c1` and 10 MeV rate histogram
`h_rate` are also retained. Canvas-only clones use distinct in-memory names so
the persisted top-level spectrum keys remain unambiguous; this display-layer
distinction does not change any histogram content used downstream.

The exponential fits attached to historical `h_eDep_*` objects and the
auxiliary `aa_example.out` text file are not consumed by `all_recon.C` or any
downstream central-spectrum step. They are recorded as historical diagnostics,
not silently treated as inputs to the physical spectrum merge.

## Portable entry

The recommended entry is:

```bash
python3 tools/data_preprocessing.py m9 \
  --beam-on-input-dir /path/to/m6/beam_on/events \
  --beam-off-input-dir /path/to/m6/beam_off/events \
  --run-id central-0308
```

The wrapper records the selected manifests, exact resolved input files,
per-run M8 reports, sample roles, software environment, executable and output
checksums, and final status. `merge_reconstructed_spectra` is also installed
as a lower-level entry for an explicitly prepared set of reconstructed files.

## Validation

The complete M1--M9 C++ suite was configured, compiled, and run in an isolated
server directory with GCC 9.4.0 and ROOT 6.28/04. All 13 registered synthetic
tests passed. The M9 test checks all 60 per-crystal histogram objects, central
and side crystal membership, veto-dependent side selection, total-spectrum
addition, historical binning, beam-on/beam-off sample roles, missing-object
failure, duplicate-input rejection, and overwrite protection. Python tests
cover the exact 60/6 manifests and wrapper orchestration.

The full experimental dataset was not rerun. M9 is therefore closed as a
source-preserving code migration with synthetic validation; it is not claimed
as a new numerical reproduction of the published spectra.

## Next boundary

M10 will migrate the Chapter 3 diagnostic ROOT-object production from the M9
merged and reconstructed inputs and connect those objects to the already
reviewed plotting entries. It must not redefine the run groups, crystal roles,
veto condition, or merged-spectrum objects closed in M9.
