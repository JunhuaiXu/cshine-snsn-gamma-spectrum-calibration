# Data-preprocessing validation utilities

## Event-display records

`build_event_display_records.cxx` freezes the two representative events used
by analysis-note Fig. 17 and the PRC supplementary event display. It writes
only two 4 x 4 `TH2F` energy-deposition histograms plus selection metadata; it
does not alter the historical analysis directory.

The gamma-ray panel is the 16th accepted candidate from the fixed 60-file
March 4--10 `step4-convert.0308.PreRun` sample. Candidates are reconstructed
with `jiugong_recon`; the allowed core units are 5, 6, 9, 10 and 4, 7, 8, 11,
13, 14, with `count_veto == 0` required for a side core, and the reconstructed
energy is restricted to 110--200 MeV. At most one accepted candidate is
counted per tree entry. The selected record is zero-based source entry
1,678,036, core CsI05, with reconstructed energy 155.4430569 MeV.

The cosmic-muon panel is the second accepted event from
`step4-convert.0308/a20240306_SnSn_GOAL_ALLCOIN.007.root`. It requires
`count_veto == 0`, at least five crystals with finite corrected times, and a
raw displayed energy of at least 100 MeV. The selected record is zero-based
source entry 1,224,015, with six finite-time crystals.

Build and run on a ROOT-enabled analysis host:

```bash
g++ -std=c++11 -O2 -Wall -Wextra -Wpedantic \
  -I/path/to/gamma2024/DataPreprocessing/t_gamma_cali \
  build_event_display_records.cxx -o build_event_display_records \
  $(root-config --cflags --libs)

./build_event_display_records \
  /path/to/gamma2024 \
  /path/to/output/event_display_records.root \
  /path/to/output/event_display_selection.tsv
```

`validate_event_display_records.cxx` compares every in-range bin against the
historical `EventALL/lego_0.root` and `EventDisplay/lego_5.root` canvases. The
validated run reported zero mismatched bins and zero maximum absolute
difference for both panels. The validated ROOT record has SHA-256
`4d325a8b8c8ddb9ae0d39979141e2f1eb2352226d8d9694a7828ab5eb47bd5ae`.

`plotting/root/draw_event_display_horizontal.C` is the adopted presentation
layer. It reads the validated record, preserves the ROOT `LEGO2` rendering,
places the two panels horizontally, labels both crystal-grid axes from 1 to 4,
and fixes both energy axes to 0--100 MeV. It does not repeat the event
selection or change a histogram bin.

## Core-multiplicity validation

`validate_core_multiplicity.cxx` preserves the earlier ten-core comparison.
`diagnose_core_subset_counts.cxx` supplies the current all-15-crystal,
no-veto plotting objects. Both are focused validation utilities, not
replacements for the historical event-processing chain.

## Physics definition

Only ten possible shower cores enter the count:

- central units: CsI05, CsI06, CsI09, and CsI10;
- left/right edge units: CsI04, CsI07, CsI08, CsI11, CsI13, and CsI14.

No plastic-veto requirement is applied in this core-multiplicity diagnostic.
CsI00, CsI01, CsI02, CsI03, and CsI12 are excluded. For each trigger, the
program records:

- `all_core_multiplicity`: the number of accepted reconstructed candidates;
- `high_energy_core_multiplicity`: the number of those candidates with
  reconstructed total energy strictly above 35 MeV;
- `all_cores_high_multiplicity`: the historical diagnostic in which a trigger
  is retained only when every accepted candidate exceeds 35 MeV.

Thus a trigger containing one candidate above 35 MeV and one below 35 MeV
contributes to bin one of `high_energy_core_multiplicity`, but does not enter
`all_cores_high_multiplicity`.

## Build and run

The program requires ROOT with `TTreeProcessorMT` and the historical
`jiugong_recon.h` header. The input root must contain the fixed 60-file
`GammaCaliData` sample under
`DataPreprocessing/step4-convert.0308.PreRun/`.

```bash
g++ -std=c++11 -O2 -Wall -Wextra -Wpedantic -pthread \
  -I/path/to/gamma2024/DataPreprocessing/t_gamma_cali \
  validate_core_multiplicity.cxx -o validate_core_multiplicity \
  $(root-config --cflags --libs)

./validate_core_multiplicity \
  /path/to/gamma2024 \
  /path/to/new-output.root \
  22
```

The output file is opened with ROOT `CREATE` mode and therefore does not
overwrite an existing result.

## Validation status

The utility was run on the authorized 60-file sample containing 160,947,782
tree entries. Without a veto requirement, the all-candidate distribution is
15724944, 326463, 2807, and 16 in the first four multiplicity bins. The
individual-candidate high-energy definition gives 4736 and 12 triggers in the
one- and two-candidate bins; applying the all-candidates-high condition to the
same ten-core sample gives 4499 and 11.

The older drawing macro contains fixed values 6731 and 19. Their production
source has not been recovered, so the historical figure values are not yet
numerically reproducible from the currently retained event sample. The
retained `ALL_h1_EventMulti` and 3353/11 diagnostic objects were produced with
the separate edge-veto selection and are therefore retained only as a
historical implementation comparison. This
distinction must remain explicit in downstream documentation.

## Per-crystal and subset diagnostic

`diagnose_core_subset_counts.cxx` records the 15-bit pattern of reconstructed
cores above 35 MeV for every trigger, without applying a plastic-veto
condition. `summarize_core_subset_masks.py` then evaluates crystal subsets
from that saved pattern frequency; it does not rerun event reconstruction.
This event-mask method preserves the fact that removing one crystal from a
selection can change a two-core trigger into a one-core trigger.

```bash
g++ -std=c++11 -O2 -Wall -Wextra -Wpedantic -pthread \
  -I/path/to/gamma2024/DataPreprocessing/t_gamma_cali \
  diagnose_core_subset_counts.cxx -o diagnose_core_subset_counts \
  $(root-config --cflags --libs)

./diagnose_core_subset_counts \
  /path/to/gamma2024 \
  /path/to/core_multiplicity_all15_noveto.root \
  22

python3 summarize_core_subset_masks.py \
  /path/to/core_multiplicity_all15_noveto.root \
  --limit 10

python3 summarize_core_subset_masks.py \
  /path/to/core_multiplicity_all15_noveto.root \
  --object-name high_core_mask_frequency_veto \
  --limit 10
```

For the confirmed ten-core scope, the per-crystal decomposition is:

| Core | One-core triggers | Participation in two-core triggers |
|---:|---:|---:|
| CsI04 | 441 | 3 |
| CsI05 | 408 | 2 |
| CsI06 | 482 | 3 |
| CsI07 | 502 | 0 |
| CsI08 | 673 | 5 |
| CsI09 | 380 | 2 |
| CsI10 | 455 | 3 |
| CsI11 | 353 | 1 |
| CsI13 | 503 | 1 |
| CsI14 | 539 | 4 |

The first numerical column sums to 4736. The second sums to 24 because each
of the 12 two-core triggers contributes to two crystal rows.

Including all 15 cores gives 7218/21. Exhaustive evaluation of all 32767
nonempty crystal subsets finds no subset with exactly 6731/19. The closest
subset with exactly 19 two-core triggers contains every crystal except CsI13
and gives 6718/19. Replacing `E_tot > 35 MeV` by `E_tot >= 35 MeV` changes none
of these counts, so the remaining difference of 13 is not a threshold-equality
effect.

The final diagnostic sources and isolated ROOT output have SHA-256 values:

- `diagnose_core_subset_counts.cxx`:
  `d617b0b43662d9acf74bb3e89b8736af3cf97a34ba2980c1491eba7d2a349965`;
- `summarize_core_subset_masks.py`:
  `ca4d762b9fe6b2923fa2963cbb4a6c9d7f9160479295a99aeba2d37daa4d0419`;
- `core_multiplicity_all15_noveto.root`:
  `0a182512a3da37fabb1e5ff10813a4fbf85775ac879cfe0c82604380c50aaa1f`.

## All-crystal global-veto comparison

As a separate diagnostic, all 15 CsI(Tl) units were admitted as possible
cores and an entire trigger was retained only when `count_veto == 0`. This is
a global-veto comparison, not the historical central/edge mixed selection.
The result is 4683/16 in the one- and two-core bins. Its per-crystal
decomposition is:

| Core | One-core triggers | Participation in two-core triggers |
|---:|---:|---:|
| CsI00 | 496 | 4 |
| CsI01 | 288 | 0 |
| CsI02 | 337 | 1 |
| CsI03 | 312 | 0 |
| CsI04 | 319 | 5 |
| CsI05 | 283 | 1 |
| CsI06 | 357 | 4 |
| CsI07 | 286 | 0 |
| CsI08 | 403 | 4 |
| CsI09 | 259 | 2 |
| CsI10 | 313 | 4 |
| CsI11 | 205 | 1 |
| CsI12 | 251 | 1 |
| CsI13 | 273 | 2 |
| CsI14 | 301 | 3 |

The one-core column sums to 4683; the two-core participation column sums to
32, corresponding to 16 two-core triggers. Exhaustive subset evaluation under
the same global-veto condition finds no subset with even 19 two-core triggers;
the closest result is the complete 15-crystal set itself, 4683/16.

## Plotting objects for the adopted all-crystal no-veto figure

The same event loop writes three mutually consistent objects:

- `all15_cluster_size_vs_total_energy`;
- `all15_core_multiplicity`;
- `all15_high_core_multiplicity`.

The blue multiplicity histogram contains
22479729/635143/7882/61/1 in its first five bins. The high-energy histogram is
7218/21 with zero entries above multiplicity two. The isolated ROOT file
`core_multiplicity_all15_noveto.root` has SHA-256
`0a182512a3da37fabb1e5ff10813a4fbf85775ac879cfe0c82604380c50aaa1f`
and is the default input of `plot_reconstruction_multiplicity.py`.
