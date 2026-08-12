# M11 background subtraction and observed-spectrum migration review

Date: 2026-08-11

## Scope

M11 closes the portable code boundary from the M9 reconstructed spectra to the
detector-level observed spectrum used at the end of Chapter 3.  Two background
definitions are retained as separate analysis routes:

1. the central slow-coincidence route uses a beam-off sample and normalizes it
   to the beam-on spectrum over 110--200 MeV;
2. the fast-coincidence cross-check excludes the SSD M2 trigger peak, forms
   equal-width signal and random timing windows, and subtracts the random
   spectrum with unit scale.

The migration does not rerun the complete experimental sample and does not
change any thesis figure or text.

## Historical evidence

| Source ID | Historical source | Preserved role |
|---|---|---|
| `DP-S604` | `step4-convert.0308/EnergySpecGen.C` | 1 MeV rebinning, 110--200 MeV beam-off normalization, `histDiff` output |
| `DP-S721` | `step10-DifferentTriggerMode/RemoveSSDM2/README` | fast/random-window contract |
| `DP-S722` | `.../step4-convert.0308.data/aa_example.C` | strict SSD M2 exclusion and inclusive signal window |
| `DP-S723` | `.../step4-convert.0308.bkg/aa_example.C` | identical exclusion and inclusive random window |
| `DP-S724` | `.../step4-convert.0308.data/all_recon.C` | central plus veto-silent side spectrum aggregation |
| `DP-S725` | `.../step4-convert.0308.data/EnergySpecGen.C` | unit-scale direct subtraction of equal-width windows |

The data and random-window `all_recon.C` files are byte-identical.  The two
`aa_example.C` files differ in the timing window.  The fast-branch output name
`spectrum_110.root` is historical; no 110 MeV normalization is applied in that
branch.

## Portable analysis contract

### Slow-coincidence route

- signal input: M9 beam-on `all_recon.root:h_total_E_M1`;
- background input: M9 beam-off `all_recon_BKG.root:h_total_E_M1`;
- input schema: 1000 bins over 0--200 MeV;
- both spectra are rebinned by five to 1 MeV bins;
- scale factor:
  \[
  \alpha=\frac{N_{\mathrm{beam\ on}}(110\text{--}200~\mathrm{MeV})}
                  {N_{\mathrm{beam\ off}}(110\text{--}200~\mathrm{MeV})};
  \]
- output: `spectrum_110.root:histDiff`, equal to signal minus
  \(\alpha\) times background.  The full 0--200 MeV histogram is retained even
  when a figure displays only 0--100 MeV.

### Fast-coincidence cross-check

- input: the exact 60 M9 beam-on reconstructed trees selected by the central
  run manifest;
- discard an event only when
  `835 < TDC_Gamma_Trig_list[18] < 850`;
- signal window: \(-350\leq t_{\mathrm{core}}\leq-50\) ns;
- random window: \(50\leq t_{\mathrm{core}}\leq350\) ns;
- central cores 5, 6, 9, and 10 enter without a veto requirement;
- side cores 4, 7, 8, 11, 13, and 14 require `count_veto == 0`, which means all
  three plastic-scintillator veto faces are silent;
- each window writes `h_central_E_M1`, `h_side_E_M1`, and `h_total_E_M1` with
  1000 bins over 0--200 MeV;
- equal window widths imply a fixed random-background scale of one;
- the final output is again `spectrum_110.root:histDiff` after 1 MeV rebinning.

The portable fast producer reads the M8 `recon_result`, `GammaTime`,
`count_veto`, and retained trigger-monitor array.  It therefore preserves the
historical selection without repeating energy calibration or shower
reconstruction inside M11.

## Recommended entry

```bash
python3 tools/data_preprocessing.py m11 \
  --slow-signal /path/to/m9/beam-on/all_recon.root \
  --slow-background /path/to/m9/beam-off/all_recon_BKG.root \
  --beam-on-input-dir /path/to/m9/beam-on/reconstructed_runs \
  --run-id central-background-subtraction
```

The command creates separate `slow/` and `fast/` directories.  It records the
exact input manifest, printed configurations, executable hashes, commands,
software environment, reports, output checksums, and completion status.  It
will not overwrite an existing run directory.

## Verification

- All 37 local Python tests passed, including the three-command M11
  orchestration and protected-output checks.
- The isolated server build used GCC 9.4.0 and ROOT 6.28/04.
- All 17 registered ROOT/C++ synthetic tests passed.
- The M11 tests explicitly cover strict SSD M2 boundaries, inclusive timing
  boundaries, central/side veto behavior, object schemas, 1 MeV rebinning,
  slow normalization, fast unit scaling, reporting, and overwrite rejection.
- The complete experimental sample was not rerun.  Earlier output-level checks
  of the adopted slow and fast figures remain separate evidence and are not
  represented as a new end-to-end real-data reproduction.

## Closure and next stage

M11 is code-migrated. M12 freezes only the detector-level `histDiff` input
interface; the already adopted central/side comparison remains an output-level
figure closure and later physics analyses remain outside this stage. Subsequent
source review located and froze the historical before/after-reconstruction
spectrum producer, merger, final notebook, and dedicated 59-group sample, so
M0B is now source-closed. The complete experimental sample was not rerun.
