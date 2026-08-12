# M13 repository-closure review

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
