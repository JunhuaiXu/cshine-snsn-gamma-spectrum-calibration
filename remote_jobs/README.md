# Remote figure jobs

Each JSON file defines one bounded, public-safe server task for
`tools/remote_figure.py`. A job contains no account, host name, port, password,
or absolute server path. Those values come from the ignored
`local/remote.json` file.

Required fields are:

- `job_id`: stable identifier used in isolated run directories;
- `uploads`: repository-relative source files and their server-side names;
- `commands`: argument lists executed in order after one batch upload;
- `outputs`: paths relative to the unique result directory that must exist and
  pass remote-to-local SHA-256 verification.

Command arguments may use these placeholders:

```text
{analysis_root}     authorized historical-analysis root from private config
{remote_workdir}    portable server work directory from private config
{remote_run_dir}    unique directory for this job and run ID
{remote_code_dir}   unique upload directory for this run
{run_output_dir}    unique generated-output directory for this run
{job_id}
{run_id}
```

Start a new definition by copying the closest existing job. Keep physics
selections and histogram definitions in the plotting or analysis program, not
in this transport description. Validate locally before any connection:

```bash
python3 tools/remote_figure.py validate <job-id>
```

One job may group several closely related short plots when they use the same
analysis root and are intended to run together. Long simulations, training,
and full production analysis should use the server's batch system instead of
this short-job runner.

`event-display-source-audit` and `event-display-candidate-export` are the two
read-only provenance-audit tasks. `event-display-reproduction` is the bounded
production and validation task: it builds the frozen-event producer, writes
new outputs only below its isolated run directory, compares every selected
4 x 4 bin with the historical ROOT canvases, and renders a candidate. None of
the three jobs changes the historical analysis directory.

`slow-coincidence-background-subtraction` is the bounded Fig. 3.17 task. It
uploads one focused ROOT reference and one Python presentation script, reads
the existing March 8 beam-on, beam-off, and central-difference objects, and
writes all outputs below an isolated run directory. Both entries reproduce
the historical 1 MeV rebinning, 110--200 MeV beam-off normalization, and
subtraction. The Python entry must obtain zero mismatched content and error
bins relative to `spectrum_110.root:histDiff`; otherwise the job exits before
the candidate is accepted as a validated output.

`fast-coincidence-background-subtraction` is the bounded Fig. 3.18 task. It
reads the existing `RemoveSSDM2` fast-window, equal-width random-window, and
saved-difference histograms without changing the historical directories. The
focused ROOT and Python entries both apply the historical factor-five rebinning
and direct subtraction with scale one. The Python entry must match every
regular and flow-bin content and uncertainty in
`step4-convert.0308.data/spectrum_110.root:histDiff`; otherwise no validated
candidate is returned.

`central-edge-spectrum-consistency` is the bounded Fig. 3.19 task. It reads
the six central, edge, and total beam-on/beam-off objects used by the published
`SpecCompare.ipynb`, reproduces the independent 110--200 MeV beam-off
normalizations and the 35--100 MeV shape comparison, and writes a focused ROOT
reference plus a horizontal Python redraw. The Python entry compares every
regular and flow-bin content and uncertainty with the ROOT reference. It also
compares the published-figure `.0308.PreRun` inputs with the central `.0308`
inputs object by object, so the two directory versions are never assumed to be
equivalent from their names alone.
