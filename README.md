# CSHINE Sn+Sn gamma-spectrum calibration and reconstruction

This repository preserves the calibration and reconstruction chain used to
obtain the detector-level gamma-ray spectrum measured with CSHINE-Gamma in the
25 MeV/u 124Sn+124Sn experiment:

```text
calibration and time correction
  -> gamma-event reconstruction and background subtraction
  -> measured detector-level gamma-ray spectrum
```

IBUU transport calculations, detector-response production and folding,
high-momentum-tail inference, and source-spectrum unfolding are outside this
repository and are maintained as separate projects.

## Analysis and provenance model

The historical production code, data, and outputs remain unchanged in the
authorized analysis environment. A read-only source snapshot is maintained
outside this public repository. This repository contains two complementary
layers:

- `analysis/`: migrated ROOT/C++ calibration and spectrum-reconstruction code;
- `plotting/`: focused ROOT renderers and optional Python presentation helpers.

Original ROOT implementations are retained as provenance. Python plotting
does not replace the physics analysis. Source tracing, code migration,
numerical validation, and thesis adoption are recorded as separate evidence
states.

## Repository layout

```text
.
├── analysis/data_preprocessing/  migrated calibration and reconstruction chain
├── docs/                         runbook, data contracts, and evidence status
├── plotting/                     figure provenance and rendering programs
├── remote_jobs/                  public-safe bounded remote-job definitions
├── tools/                        orchestration and repository checks
├── local/                        ignored private connection configuration
└── results/                      ignored generated outputs
```

## Documentation map

Each document has one primary responsibility:

1. [`REPRODUCE.md`](REPRODUCE.md): requirements, verification, and entry points;
2. [`docs/CHAPTER3_END_TO_END.md`](docs/CHAPTER3_END_TO_END.md): the only
   complete ordered M2--M12 command sequence;
3. [`docs/ANALYSIS_IO_MAP.md`](docs/ANALYSIS_IO_MAP.md): compact stage input,
   output, ROOT-object, consumer, and evidence crosswalk;
4. [`docs/DATA_ACCESS.md`](docs/DATA_ACCESS.md): file, tree, branch, object,
   selection, binning, and unit contracts;
5. [`docs/REPRODUCIBILITY_STATUS.md`](docs/REPRODUCIBILITY_STATUS.md): current
   evidence level and unresolved provenance limits;
6. [`analysis/data_preprocessing/README.md`](analysis/data_preprocessing/README.md):
   source layout, physical contracts, build products, and direct executables;
7. [`plotting/README.md`](plotting/README.md): figure-to-code map, validation,
   and thesis-use status;
8. [`tools/README.md`](tools/README.md) and
   [`remote_jobs/README.md`](remote_jobs/README.md): orchestration and bounded
   remote execution.

Machine-readable authorities are
`analysis/data_preprocessing/provenance/pipeline_stages.json` for the analysis
graph and `plotting/records/` for figure work records.

## Quick verification

In a compatible ROOT environment, run:

```bash
python3 tools/data_preprocessing.py verify
python3 tools/data_preprocessing.py check
python3 tools/repository_closure.py check
```

Use the end-to-end guide for any data-bearing run. Generated outputs are
written under protected run directories in `results/`; existing outputs are
not overwritten silently.

## Reproducibility status

The reviewed source boundary is closed through M13. M1--M12 have portable
entries, protected outputs, run metadata, and eighteen ROOT/C++ synthetic
tests that pass in ROOT 6.28/04 with GCC 9.4.0.

The current claim is **code migrated, published result not rerun**. The full
60-group beam-on plus 6-group beam-off chain and the separate 59-group
reconstruction-spectrum figure sample have not been rerun through the unified
entry. The original M5 batch fitter is unavailable; its surviving per-crystal
outputs are audited rather than regenerated. See
[`docs/REPRODUCIBILITY_STATUS.md`](docs/REPRODUCIBILITY_STATUS.md) for the
authoritative evidence statement.

## Citation

Please identify the software commit used and cite **both** associated
articles. The Physical Review C article documents the experiment,
calibration, reconstruction, background treatment, and spectrum analysis; the
Physical Review Research article reports the corresponding short-range-
correlation result. Machine-readable metadata are provided in
[`CITATION.cff`](CITATION.cff).

```bibtex
@article{Xu2026SnSnGamma,
  author  = {Xu, Junhuai and Niu, Qinglin and Qin, Yuhao and Si, Dawei and
             Wang, Yijie and Xiao, Sheng and Tian, Baiting and Qin, Zhi and
             Zhang, Haojie and Zhang, Boyuan and Guo, Dong and Fu, Minxue and
             Wei, Xiaobao and Hao, Yibo and Wang, Zengxiang and Zhuo, Tianren and
             Ma, Chunwang and Yang, Yuansheng and Wei, Xianglun and Yang, Herun and
             Ma, Peng and Duan, Limin and Duan, Fangfang and Wang, Kang and
             Ma, Junbing and Xu, Shiwei and Bai, Zhen and Yang, Guo and
             Yang, Yanyun and Xu, Mengke and Chen, Kaijie and Hao, Zirui and
             Fan, Gongtao and Wang, Hongwei and Xu, Chang and Xiao, Zhigang},
  title   = {Experimental Study of Bremsstrahlung Gamma-Ray Emission and
             Short-Range Correlations in {$^{124}$Sn+$^{124}$Sn} Collisions at
             25 {MeV}/Nucleon},
  journal = {Physical Review C},
  volume  = {113},
  number  = {4},
  pages   = {044613},
  year    = {2026},
  doi     = {10.1103/dhz2-nl56},
  url     = {https://doi.org/10.1103/dhz2-nl56}
}

@article{Xu2025SRC,
  author  = {Xu, Junhuai and others},
  title   = {Precise Measurement of Short-Range Correlations in Nuclei from
             Bremsstrahlung Gamma-Ray Emission in Low-Energy Heavy-Ion Collisions},
  journal = {Physical Review Research},
  volume  = {7},
  number  = {4},
  pages   = {043174},
  year    = {2025},
  doi     = {10.1103/jw1p-36pb},
  url     = {https://doi.org/10.1103/jw1p-36pb}
}
```

When the corresponding detector description or high-energy calibration is
used, cite the relevant component article as well:

```bibtex
@article{Qin2023CSHINEGamma,
  author  = {Qin, Yuhao and others},
  title   = {A {CsI(Tl)} Hodoscope on {CSHINE} for Bremsstrahlung Gamma Rays in
             Heavy-Ion Reactions},
  journal = {Nuclear Instruments and Methods in Physics Research Section A},
  volume  = {1053},
  pages   = {168330},
  year    = {2023},
  doi     = {10.1016/j.nima.2023.168330},
  url     = {https://doi.org/10.1016/j.nima.2023.168330}
}

@article{Xu2025CsIResponse,
  author  = {Xu, Junhuai and Si, Dawei and Qin, Yuhao and Xu, Mengke and
             Chen, Kaijie and Hao, Zirui and Fan, Gongtao and Wang, Hongwei and
             Wang, Yijie and Xiao, Zhigang},
  title   = {Linear Response of {CsI(Tl)} Crystal to Energetic Photons below
             20 {MeV}},
  journal = {Nuclear Instruments and Methods in Physics Research Section A},
  volume  = {1080},
  pages   = {170787},
  year    = {2025},
  doi     = {10.1016/j.nima.2025.170787},
  url     = {https://doi.org/10.1016/j.nima.2025.170787}
}
```

## License and data boundary

The analysis source code and documentation are released under the
[BSD 3-Clause License](LICENSE). Experimental ROOT files, generated
calibration parameters, spectra, figures, and other analysis products are not
distributed and are not covered by this software license.

Public code and documentation use parameterized paths. Credentials, private
hosts, accounts, mounts, and exact collaborator paths must remain outside this
repository.
