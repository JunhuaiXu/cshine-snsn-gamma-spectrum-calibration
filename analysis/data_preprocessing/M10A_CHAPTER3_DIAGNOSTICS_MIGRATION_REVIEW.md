# M10A Chapter 3 diagnostics migration review

## Scope

M10 is split into two bounded parts.  M10A covers the central beam-on and
beam-off diagnostic-object producers derived from DP-S700--DP-S704.  M10B will
cover the trigger monitor and the five trigger-conditioned branches.  The
split prevents the central all-trigger sample from being conflated with the
independently selected trigger samples.

M10A consumes the manifest-defined per-run `GammaCaliData` trees written by
M8 and orchestrated by M9.  It reads the stored `recon_result` and
`count_veto` branches.  Unlike DP-S700 and DP-S701, it does not run
`jiugong_recon` a second time.  This removes duplicated computation without
changing the reconstruction result or its dictionary-backed ROOT schema.

## Frozen physical definitions

The main diagnostic selection is the same selection used for the reviewed
Chapter 3 topology, spread, core-energy, and core-time figures:

- central centres: CsI05, CsI06, CsI09, and CsI10, without a veto condition;
- side centres: CsI04, CsI07, CsI08, CsI11, CsI13, and CsI14, retained only
  when `count_veto == 0`;
- lower-edge centres and corner crystals are excluded from this main set;
- the transverse coordinates are the historical relative 3 x 3 crystal map;
- `delta_x` and `delta_y` are energy-weighted mean absolute deviations in
  crystal-pitch units and are multiplied by the 7 cm pitch;
- `delta_r = sqrt(delta_x^2 + delta_y^2)`;
- the lower spatial interval is inclusive, `10 <= E_tot <= 100 MeV`, and the
  upper interval is `E_tot > 100 MeV`.

The multiplicity figure has a different, author-confirmed definition.  It
uses every valid reconstructed centre among all 15 CsI(Tl) units, applies no
plastic-veto condition, and counts candidates with `E_tot > 35 MeV`
independently within each total trigger.  Its three ROOT objects therefore
carry the `all15_` prefix and are not aliases for the main-selection objects.

The beam-on core-time object uses `GammaTime[centre]`.  The beam-off producer
retains the historical DP-S701 sign convention, `-GammaTime[centre]`, although
the currently adopted beam-off topology figure does not read that object.

## ROOT outputs

The M10A output contains the objects currently required by the reviewed
Chapter 3 figures:

| Object | Definition |
|---|---|
| `ALL_h2_TotalE_DeltaY` | `E_tot` versus `delta_y`, 50 x 70 bins over 5--200 MeV and 0--7 cm |
| `ALL_h2_TotalE_Delta` | `E_tot` versus `delta_r`, 50 x 70 bins over 0--200 MeV and 0--7 cm |
| `ALL_h2_ax_ay_10_100` | `delta_y` versus `delta_x` for 10--100 MeV, 70 x 70 bins over 0--7 cm |
| `ALL_h2_ax_ay_100_inf` | `delta_y` versus `delta_x` above 100 MeV, same binning |
| `central_h2_TotalE_CenterE` | central-core `E_core` versus `E_tot` |
| `side_h2_TotalE_CenterE` | accepted side-core `E_core` versus `E_tot` |
| `ALL_h2_TotalE_CenterE` | sum of the two reviewed core classes |
| `ALL_h2_TOF_TotalE` | `E_tot` versus corrected core time, 100 x 200 bins over -500--500 ns and 0--200 MeV |
| `all15_cluster_size_vs_total_energy` | cluster size versus `E_tot` for all valid centres |
| `all15_core_multiplicity` | number of valid all-15 candidates per trigger |
| `all15_high_core_multiplicity` | number of all-15 candidates above 35 MeV per trigger |

The historical DP-S700 code constructs some intermediate merged core-time
histograms with a 0--7 vertical range even though the per-crystal objects and
published final object use 0--200 MeV.  M10A constructs the final published
100 x 200 schema directly.  It records the discrepancy rather than relying on
ROOT `Merge` to replace an incompatible target schema.

Historical auxiliary objects not used by the current thesis figures remain
available in the immutable snapshot.  Their omission from this bounded M10A
producer is not a claim that the historical file contained only the objects
listed above.

## Portable files

```text
include/gamma_position.hpp
include/spatial_spread.hpp
include/chapter3_diagnostics.h
src/gamma_position.cxx
src/spatial_spread.cxx
src/chapter3_diagnostics.cxx
apps/build_chapter3_diagnostics.cxx
tests/test_chapter3_diagnostics.cxx
```

The command-line entry accepts repeated explicit M8 files, one sample role,
one protected ROOT output, and one TSV report.  The unified `m10` command
resolves the exact 60 beam-on and six beam-off filenames from the two M9
manifests.  No wildcard discovery or implicit latest-run selection is used.

## Verification

- Local C++11 syntax checking passed for all M10A headers, sources, command
  entry, and synthetic test.  ROOT itself emitted only warnings from the
  locally installed ROOT 6.24 headers.
- The local full Python orchestration suite passed all 35 tests, including the
  exact-manifest M10 wrapper, isolated output-directory creation, metadata,
  and overwrite protection.
- An isolated server build used GCC 9.4.0 and ROOT 6.28/04.  All 14 registered
  M1--M10A C++ synthetic tests passed, including the new Chapter 3 diagnostic
  test.  The server clock-skew warning is the previously documented host-clock
  difference and does not change the test result.
- The synthetic M10A test checks stored reconstruction consumption, relative
  geometry, spatial spreads, main side-veto selection, all-15 no-veto
  multiplicity, energy interval boundaries, published core-time binning,
  the historical beam-off time-sign convention, reports, and output creation.

No complete experimental beam-on or beam-off sample was run.  M10A is
therefore code-migrated and synthetically verified; it is not claimed as a
new numerical reproduction of the published event counts.  M10 remains in
progress until M10B trigger branches are migrated.
