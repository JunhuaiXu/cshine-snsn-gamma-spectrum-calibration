# M10B trigger-diagnostics migration review

> Historical closure record. Current status is maintained in
> `../../docs/REPRODUCIBILITY_STATUS.md`; current commands are maintained in
> `../../docs/CHAPTER3_END_TO_END.md`.

## Scope

M10B closes the trigger-monitor and trigger-conditioned part of the Chapter 3
diagnostics. It consumes the exact manifest-defined beam-on event trees from
M9 and performs two operations in one pass:

1. fill all 15 hardware trigger-monitor TDC spectra;
2. fill the reconstructed-energy--core-time distributions for the five
   monitored trigger conditions used in analysis-note Fig. 15.

The experimental data and generated ROOT files are not distributed. The
historical source snapshot remains unchanged, and the complete experimental
sample was not rerun during this migration.

## Restored upstream branch

The frozen Fig. 14/15 branch retains the raw 32-element `TDC_Gamma_Trig` array
as `TDC_Gamma_Trig_list`. The earlier central PreRun source used to define M6
did not write this branch. M10B therefore required a bounded upstream repair:

- M6 now validates and copies `TDC_Gamma_Trig` to
  `TDC_Gamma_Trig_list[32]`;
- M8 propagates `TDC_Gamma_Trig_list[32]` unchanged with the reconstructed
  event tree;
- synthetic M6 and M8 tests verify the array size and values.

No trigger condition is applied in M6 or M8. The retained values are first
interpreted in M10B.

## Frozen trigger definitions

The monitor spectra use the strict historical window
`100 < TDC_Gamma_Trig_list[index] < 4000` and preserve 4096 bins over
0--4096. M10B writes `h1_TrigList0`--`h1_TrigList14`; the thesis Fig. 14
display order is 1, 2, 3, 4, 6, 0.

The five conditioned distributions use the inclusive window
`100 <= TDC_Gamma_Trig_list[index] <= 4000` at indices 17, 18, 19, 20, and
22. This is a valid monitor-signal condition, not an exclusive selection of
the narrow self-trigger peak.

## Historical and reviewed candidate policies

Static source review found an important distinction in the common historical
Fig. 15 `h2_check.C`:

- the frozen macro requires `count_veto == 0` for central cores 5, 6, 9, and
  10, and applies no veto requirement to side cores 4, 7, 8, 11, 13, and 14;
- the author-reviewed main-analysis definition keeps central cores without a
  veto requirement and requires `count_veto == 0` for the six side cores.

These definitions are not silently merged. M10B writes both under explicit
names:

```text
historical_h2_TOF_TotalE_Trig17,18,19,20,22
reviewed_h2_TOF_TotalE_Trig17,18,19,20,22
```

Each object has 100 x 200 bins over corrected core time -500--500 ns and
reconstructed total energy 0--200 MeV. The adopted thesis Fig. 15 remains an
unchanged record of the historical implementation. The reviewed family is
available for a future physics comparison and must not be substituted without
an explicit author decision.

## Portable files

```text
include/trigger_diagnostics.h
src/trigger_diagnostics.cxx
apps/build_trigger_diagnostics.cxx
tests/test_trigger_diagnostics.cxx
```

The unified `m10b` command resolves the exact beam-on reconstructed filenames
from the M9 manifest, writes a protected run directory, and records the input
manifest, configuration, ROOT output, TSV report, software environment, and
checksums. No wildcard or latest-file selection is used.

The two Python plotting entries retain their historical-file mode and also
accept the single M10B ROOT file. The Fig. 15 entry requires an explicit
`historical` or `reviewed` object policy; its metadata records the chosen
policy and its exact veto definition.

## Verification

- The complete local Python suite passes 36 tests, including exact M9 input
  resolution and overwrite protection for the M10B wrapper.
- The isolated server build used GCC 9.4.0 and ROOT 6.28/04. All 15 M1--M10B
  C++ synthetic tests passed.
- The M10B synthetic test checks strict versus inclusive TDC boundaries, all
  15 monitor-object schemas, the five trigger-index mappings, both candidate
  policies, nested output creation, report contents, and overwrite rejection.
- The server clock-skew messages reflect the known host-clock difference and
  do not change the test result.

M10B is code-migrated and synthetically verified. This is not a claim that
the complete historical data set or published event counts were rerun.
