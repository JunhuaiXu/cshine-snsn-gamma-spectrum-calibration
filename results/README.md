# Generated results

Programs write generated ROOT files, tables, and figures below this directory.
All generated contents are ignored by Git; this README only documents the
output location.

Candidate thesis figures should be written to `results/figures/` unless a
program states otherwise. Presence in this directory does not mean that a
figure has been validated or accepted for the thesis; those states are
recorded in `../plotting/README.md`.

The unified M2 and M3 analysis entry writes one immutable run directory per
stage and run identifier:

```text
results/data_preprocessing/<m2-or-m3>/<run-id>/
├── config_used.txt
├── input_manifest.tsv
├── run.log
├── run_metadata.json
├── run_report.tsv
├── source_background.root     M2
├── gain_relation.root         M3
└── gain_parameters.txt        M3
```

An existing run directory is never reused. Raw-input SHA-256 values are only
computed when `--hash-inputs` is requested; the default manifest records path,
size, and modification time.

The time-amplitude correction program writes its outputs under:

```text
results/figures/time_walk/
├── individual/
├── overview/
├── validation_summary.csv
└── run_metadata.json
```

The neighboring-crystal time-correlation program writes:

```text
results/figures/neighbor_time_correlation/
├── cshine_gamma_neighbor_time_correlation_csi05_csi06_horizontal.pdf
├── cshine_gamma_neighbor_time_correlation_csi05_csi06_horizontal.png
└── cshine_gamma_neighbor_time_correlation_csi05_csi06_horizontal_metadata.json
```

The one-dimensional unit-time and neighboring-crystal time-difference
programs write:

```text
results/figures/unit_time_difference/
├── cshine_gamma_unit05_time_and_neighbor_difference_horizontal.pdf
├── cshine_gamma_unit05_time_and_neighbor_difference_horizontal.png
├── cshine_gamma_unit05_time_and_neighbor_difference_horizontal_metadata.json
└── root/
    ├── cshine_gamma_unit05_time_root.pdf
    └── cshine_gamma_neighbor_time_difference_root.pdf
```

The reconstruction-multiplicity plotting program writes:

```text
results/figures/reconstruction_multiplicity/
├── cshine_gamma_cluster_size_and_core_multiplicity_horizontal.pdf
├── cshine_gamma_cluster_size_and_core_multiplicity_horizontal.png
└── run_metadata.json
```

The all-15-crystal, no-veto source output is written separately under:

```text
results/candidates/all15_noveto/reconstruction_multiplicity/
├── cshine_gamma_cluster_size_and_core_multiplicity_horizontal.pdf
├── cshine_gamma_cluster_size_and_core_multiplicity_horizontal.png
└── run_metadata.json
```

Its high-energy curve is read from the validated ROOT histogram and gives
7218/21. The historical 6731/19 values remain only in metadata. The PDF was
accepted by the author and copied unchanged into thesis Sec. 3.3.5. See
`../plotting/README.md` for hashes and thesis-use status.
