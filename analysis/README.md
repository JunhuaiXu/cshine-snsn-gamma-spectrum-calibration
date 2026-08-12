# Gamma-spectrum calibration and reconstruction code

This directory contains the portable CSHINE-Gamma calibration and
detector-level spectrum-reconstruction analysis for the 25 MeV/u
124Sn+124Sn experiment.

```text
analysis/
└── data_preprocessing/     calibration, timing, reconstruction, background,
                            and detector-level observed spectrum
```

`data_preprocessing/` is the complete scope of this repository. Later physical
interpretation and source-spectrum reconstruction are separate projects.

The historical server code is not modified. Core analysis is ported one
confirmed physical stage at a time. The implemented M1--M12 components under
`data_preprocessing/` cover the minimal calibration ROOT library, source
background treatment, low-/high-gain relation, three-point calibration,
time-correction evidence boundary, calibrated event trees, neighboring-time
diagnostics, per-event shower reconstruction, exact beam-on/beam-off run
manifests, central/side/total spectrum merging, diagnostic objects, two
background treatments, and the final observed-spectrum interface. They are
synthetic-tested and complete as direct code migrations without rerunning the
published dataset.
Standalone plotting programs under `../plotting/` continue to be supplementary
outputs rather than replacements for the upstream ROOT analysis.
