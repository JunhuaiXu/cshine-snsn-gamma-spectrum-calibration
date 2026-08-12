# M13 repository-closure review

> Historical closure record. Current status is maintained in
> `../../docs/REPRODUCIBILITY_STATUS.md`; current commands are maintained in
> `../../docs/CHAPTER3_END_TO_END.md`.

## Scope

M13 closes the documentation, record, and public-boundary tooling for the
CSHINE Sn+Sn gamma-spectrum calibration and reconstruction repository. It does
not change detector physics, rerun the experimental sample, or include later
physical interpretation and source-spectrum analyses.

## Added closure evidence

- `REPRODUCE.md` now lists M2--M12 in physical processing order, including the
  distinct M10 diagnostic, M10B trigger-monitoring, and M12 read-only
  observed-spectrum entries.
- `time-amplitude-csi05.json` and `time-amplitude-all.json` close the two
  previously missing figure records with frozen source hashes, contracts,
  real-data validation, author decisions, and adopted-artifact checksums.
- `repository_closure.py` checks the migration manifest, stage graph, figure
  records, required navigation documents, stage coverage, and public-source
  privacy boundary in one local command.
- `REPRODUCIBILITY_STATUS.md` states the exact evidence level and the historical
  time-fit-producer limit. M0B was closed separately after this review by
  locating the final before/after reconstruction spectrum chain.
- ignored cache and desktop metadata files are removed from the source tree.

## Verification boundary

The closure command must pass without private-path findings in public source
files. Repository-level citation metadata is now fixed in `CITATION.cff`; both
the PRC experiment/analysis article and the PRR precision-result article are
required associated citations, while the README separately identifies
component citations. The author approved the BSD 3-Clause License for the
analysis code and documentation on 2026-08-12. Experimental data, generated
calibration parameters, spectra, figures, and other analysis products are not
distributed and are not covered by this software release. The strict audit
requires the license text and matching `CITATION.cff` declaration before a
public GitHub repository is created.

M13 is a closed repository-documentation stage. The overall analysis remains
**code migrated, published result not rerun**. M0B is now source-closed, but
that later closure is likewise not a claim that the full experimental sample
or publication result has been rerun.

## Post-release documentation consolidation

The public documentation was consolidated after release without changing any
physics implementation, selection, ROOT object, or evidence claim. The
documentation authorities are now separated explicitly:

- `README.md` provides project scope and navigation;
- `REPRODUCE.md` provides requirements and verification entry points;
- `docs/CHAPTER3_END_TO_END.md` is the only complete M2--M12 command source;
- `docs/ANALYSIS_IO_MAP.md` is the compact stage crosswalk;
- `docs/DATA_ACCESS.md` defines data and ROOT-object contracts;
- `docs/REPRODUCIBILITY_STATUS.md` defines the current evidence level;
- `plotting/README.md` provides one figure registry rather than separate,
  overlapping mapping and status tables.

The repository-closure audit was updated to verify the authoritative
end-to-end guide instead of requiring duplicate stage commands in
`REPRODUCE.md`. Manifest verification, the pipeline and figure-record checks,
the public-boundary audit, local links, and the complete Python test suite all
pass after this consolidation.
