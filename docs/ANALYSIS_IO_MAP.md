# Analysis input/output map

This page is the compact crosswalk for the migrated CSHINE-Gamma calibration
and reconstruction chain. It answers four questions for each stage: what the
stage consumes, what it produces, which ROOT objects form the handoff, and
what consumes the result next.

This table is navigation, not a second stage registry. The machine-readable
authority is
[`pipeline_stages.json`](../analysis/data_preprocessing/provenance/pipeline_stages.json),
the exact commands are in
[`CHAPTER3_END_TO_END.md`](CHAPTER3_END_TO_END.md), and branch/file contracts
are in [`DATA_ACCESS.md`](DATA_ACCESS.md). Exact collaborator hosts and
absolute data paths are intentionally excluded.

## Stage crosswalk

| Stage | Physical role | Required input | Portable output or record | Principal ROOT handoff | Next consumer | Evidence status |
|---|---|---|---|---|---|---|
| M0B | Freeze the reviewed Chapter 3 source and data-flow boundary | Immutable historical source snapshot and reviewed thesis-output map | Source and migration manifests; stage registry | Not applicable | All migrated stages and figure records | Source boundary closed; M5 batch fitter remains absent |
| M1 | Provide the minimum ROOT-streaming classes used by calibration objects | Historical class layouts | Calibration library and ROOT dictionary | `t_2d_fit`, `t_gamma_cali` | M3 and M4 | Code migrated; schema and streaming tested |
| M2 | Subtract live-time-normalized radioactive-source background | 21 source files and 24 background files, tree `tree` | `m2/<run-id>/source_background.root` | `h_src_XE_00--14`, `h_bkg_XE_00--14`, `h_nobkg_XE_00--14` | M4 | Code migrated; complete experimental input not rerun |
| M3 | Determine the low-/high-gain channel relation | 105 central-date collision files, tree `tree`; M1 library | `m3/<run-id>/gain_relation.root` | `f_data`, `c` | M4 | Code migrated; complete experimental input not rerun |
| M4 | Fit the three-point energy calibration for 15 crystals | M2 net source spectra and M3 gain relation | `m4/<run-id>/energy_calibration.root` | `cali_20240308`, `g_0--14`, `f_0--14` | M6 | Code migrated; published coefficients not regenerated |
| M5 | Produce time-amplitude evidence and audit the surviving per-crystal fits | Reviewed raw timing files; 15 historical ROOT/text fit-result pairs | Protected M5 spectrum and audit run directories | `h_TOF_one_*`, `h_TOF_move_*`, `h_TOF_move_cali_*`; audited `f_00--14` artifacts | M6 time correction and thesis appendix | Code migrated to surviving evidence boundary; historical batch fitter unavailable |
| M6 | Produce calibrated per-event energy and corrected-time arrays | Raw event trees, M4 calibration, fixed M5 parameters, one explicit run manifest | `m6/<run-id>/events/*.root` | `GammaCaliData`; `GammaEnergy[15]`, `GammaTime[15]`, retained raw veto/trigger arrays | M7 and M9 | Code migrated; 60+6 full production not rerun |
| M7 | Form neighboring-crystal timing diagnostics | The 60 manifest-defined M6 beam-on trees | `m7/<run-id>/neighbor_time_diagnostics.root` | `h2_all`, `h2_cut`, `h1`, `hh_diff`, `h3`, `h4` | Timing-figure renderers | Code migrated; adopted figures have separate real-object checks |
| M8 | Reconstruct clusters and candidate cores | Explicit M6 event trees and reviewed geometry/veto mapping | Reconstructed `GammaCaliData` trees and per-crystal spectra | `recon_result`, `count_veto`, `h_eDep_*`, `h_recon_*`, `h_recon_veto_*`, `h_recon_vetoed_*` | M9 | Code migrated; full experimental input not rerun |
| M9 | Run M8 over exact beam-on/off groups and merge reconstructed spectra | M6 beam-on and beam-off event directories; 60-group and 6-group manifests | `m9/<run-id>/beam-{on,off}/` reconstructed trees and merged ROOT files | `h_central_E_M1`, `h_side_E_M1`, `h_total_E_M1` plus per-crystal families | M10, M10B, and M11 | Code migrated; complete 60+6 chain not rerun |
| M10A | Produce central Chapter 3 topology, energy, timing, and multiplicity diagnostics | Manifest-defined M9 reconstructed trees for both beam roles | `m10/<run-id>/beam-on/h2_check.root` and `beam-off/h2_check_BKG.root` | Spatial-spread, core-energy, core-time, and all-15 multiplicity object families | Figure renderers and thesis diagnostics | Code migrated; complete sample not rerun |
| M10B | Produce trigger-monitor and trigger-conditioned diagnostics | Manifest-defined M9 beam-on reconstructed trees | `m10b/<run-id>/trigger_diagnostics.root` | `h1_TrigList0--14`; separately named historical and reviewed trigger-conditioned objects | Trigger-figure renderers | Code migrated; object family must be selected explicitly |
| M11 | Form the measured detector-level spectrum by two background routes | M9 beam-on/off merged spectra and beam-on reconstructed trees | `m11/<run-id>/slow/spectrum_110.root` and `fast/spectrum_110.root` | `histDiff` in each route | M12; slow route is the authoritative Chapter 3 spectrum | Code migrated; histogram arithmetic tested, full sample not rerun |
| M12 | Validate the final observed-spectrum contract without rewriting it | M11 slow-route `spectrum_110.root` | Read-only schema and metadata report | `histDiff` (`TH1D`, 200 bins, 0--200 MeV laboratory frame) | Later physics projects through an explicit data contract | Code migrated; complete historical spectrum not inspected by this stage run |
| M13 | Audit records, documentation, licensing, and public boundaries | M12 contract plus all manifests and figure records | Repository closure record | Not applicable | Public release and later maintenance | Documentation and public boundary closed |

## Sample manifests

The analysis never discovers production samples by an unrestricted directory
wildcard. The reviewed sample roles are frozen separately:

| Manifest | Role | Number of groups | Used by |
|---|---|---:|---|
| `central_beam_on_run_groups.tsv` | Central beam-on production | 60 | M6, M7, M9, M10, M10B, M11 |
| `central_beam_off_run_groups.tsv` | Beam-off background production | 6 | M6, M9, M10, M11 |
| `reconstruction_spectra_figure_run_groups.tsv` | March 5--10 before/after-reconstruction figure sample | 59 | Figure-specific reconstruction-spectrum path only |

The 59-group figure sample is not a substitute for the 60-group central
analysis sample.

## Run records and generated outputs

Each migrated stage writes into a protected run directory. Depending on the
stage, the directory contains the frozen configuration, exact input manifest,
file size and modification-time records, optional input SHA-256 values,
environment and executable metadata, the stage report, output checksums, and
the declared ROOT products. Generated ROOT, PDF, PNG, and run directories are
ignored by Git.

The repository therefore exposes the complete portable data contract without
publishing experimental files, historical generated products, private mounts,
accounts, or credentials.
