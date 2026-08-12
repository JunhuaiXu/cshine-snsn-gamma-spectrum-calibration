# M12 observed-spectrum interface review

## Scope

M12 freezes only the detector-level observed spectrum delivered by the M11
slow beam-off route. It does not migrate the central/side comparison, select a
later fit range, transform energies to another reference frame, or run any
inference or unfolding code.

## Authoritative input

```text
spectrum_110.root
└── histDiff  (TH1D)
```

The `110` records the lower edge of the beam-off normalization interval. It is
not an upper energy limit on the stored spectrum. The fast/random-window
`histDiff` is a cross-check and is not the authoritative interface input.

## Frozen contract

| Field | Definition |
|---|---|
| ROOT object | `histDiff` |
| ROOT class | `TH1D` |
| regular bins | 200 |
| energy range | 0--200 MeV |
| bin width | 1 MeV |
| energy frame | laboratory frame |
| bin content | background-subtracted event counts |
| bin uncertainty | stored per-bin standard error |
| flow bins | retained and reported |
| negative bins | retained and counted, never clipped by M12 |

The exact `TH1D` class is part of the contract. Historical consumer code that
casts this object directly to `TH1F` relies on an unsafe type assumption.
Future readers must retrieve it as `TH1D` or through the `TH1` base class.

M12 does not define a consumer's fit interval, rebinning, normalization,
negative-bin treatment, laboratory-to-center-of-mass conversion, response
matrix, or statistical method.

## Portable implementation and verification

```text
include/observed_spectrum.h
src/observed_spectrum.cxx
apps/inspect_observed_spectrum.cxx
tests/test_observed_spectrum_interface.cxx
```

The inspector opens the input read-only, verifies the exact object class and
uniform axis, requires stored variances, checks finite regular and flow bins,
and writes a tab-separated report. It never creates a replacement spectrum.
The unified `m12` command records input metadata, optional SHA-256, executable
checksum, configuration, environment, and report checksum in a protected run
directory.

The synthetic ROOT test covers exact class and axis, stored uncertainties,
negative-bin preservation, flow-bin reporting, nested report-directory
creation, overwrite rejection, and rejection of a `TH1F` object with the same
name and binning. The isolated server build used GCC 9.4.0 and ROOT 6.28/04;
all 18 registered ROOT/C++ synthetic tests passed. The server filesystem
reported clock-skew warnings during the build, but every target compiled and
linked and every test executable ran successfully. This is a code-migrated
interface; the complete experimental spectrum has not been rerun by M12.
