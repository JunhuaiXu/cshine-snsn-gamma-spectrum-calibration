# Figure work records

This directory stores one public-safe JSON work record for each newly traced
or redrawn thesis figure. Records are created and checked with:

```bash
python3 tools/figure_workflow.py init --help
python3 tools/figure_workflow.py check
python3 tools/figure_workflow.py summary
```

A record captures staged provenance: historical source, frozen physics
contract, portable ROOT/Python entries, validation evidence, candidate
checksums, author decision, and thesis-use status. It must not contain server
accounts, credentials, private absolute paths, raw data, or generated binary
outputs.

These records supplement rather than replace the project documentation:

- `../../../THESIS_OUTPUT_CODE_MAP.md` remains the sole project-wide coverage
  table;
- `../README.md` remains the human-readable figure/code guide;
- exact collaborator paths remain in the private `../../../DATA_PATHS.md`;
- adopted figure provenance remains in the thesis asset manifest.

The status checker enforces ordering constraints but cannot decide whether a
physics definition or numerical comparison is scientifically correct.
