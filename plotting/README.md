# Thesis plotting code

This directory contains lightweight programs that redraw selected figures
from the CSHINE Sn+Sn gamma-spectrum calibration and reconstruction outputs.
Plotting is a small output layer, not an
alternative implementation of the physics analysis under `../analysis/`.
The directory can be copied to the server without moving, editing, or
duplicating the historical workflow.

Final figure numbers may change while the thesis is edited. The stable key is
the semantic figure ID in the tables below. Generating an output does not
approve it for thesis use: every candidate figure must be reviewed by the
author before it is copied to the chapter assets or referenced by LaTeX.

New figure work follows the internal
[`FIGURE_REPRODUCTION_WORKFLOW.md`](../../FIGURE_REPRODUCTION_WORKFLOW.md).
Create one public-safe staged JSON record under [`records/`](records/) with
`python3 tools/figure_workflow.py init` before adding a new plotting entry.
The JSON record makes state-order and checksum checks machine-readable; this
README remains the human-readable figure/code and validation guide.

## Figure registry

The registry below is the human-readable authority for figure provenance and
adoption. Detailed input-object and selection contracts are maintained in
[`../docs/DATA_ACCESS.md`](../docs/DATA_ACCESS.md); machine-readable work
records are maintained in [`records/`](records/). Output paths are declared by
the corresponding record and plotting program rather than repeated here.

| Stable ID | Historical trace | Portable entry | Real-data validation | Thesis use |
|---|---|---|---|---|
| `time-amplitude-csi05` | Traced | Shared-scale rendering implemented; local synthetic check passed | Revised rendering regenerated with real inputs; count checks passed | Shared-scale figure used in Sec. 3.3.1 |
| `time-amplitude-all` | Traced | Shared-scale rendering implemented; local synthetic check passed | Revised rendering regenerated for all 15 real channels; count checks passed | Shared-scale figures used in Appendix A |
| `neighbor-time-csi05-csi06` | Traced to the two historical TH2 blocks and exact 60-file list | Implemented with independent axes, individual logarithmic color scales, and separated panel layout | Run with the 60 authorized server inputs; the per-bin selected-sample subset check passed | Author-approved PDF used in Sec. 3.3.2 |
| `unit05-time-neighbor-difference` | Traced to the three TH1 blocks in one historical ROOT macro and the manually assembled analysis-note PDF | Portable ROOT reference and horizontal Python presentation entry implemented | Both entries ran with the 60 authorized inputs; the peak-scale factor agrees at 52.00885 | Author-approved PDF used in Sec. 3.3.2 |
| `reconstruction-multiplicity` | Historical mixed-selection panels and fixed 6731/19 values traced | Python entry reads a focused all-15-crystal, no-veto ROOT output for both panels | Current 60-file sample gives 7218/21; the two-dimensional distribution, blue curve, and red curve share the same all-crystal selection | Author-confirmed PDF used in thesis Sec. 3.3.5 |
| `cshine-spatial-correlation-energy-intervals` | Traced to the two exact `h2_check.root` objects and their `get_ax_ay` definition | Focused ROOT reference and horizontal Python entry implemented | Real-data run checked 83,807 and 1,574 in-range counts with no rebinning or normalization | Author-confirmed PDF used in Sec. 3.4.1 |
| `cshine-energy-spatial-spread-correlations` | Traced to the two exact `h2_check.root` objects and the common spatial-spread definition | Focused ROOT reference and horizontal Python entry implemented | Real-data object entries and displayed-bin counts checked; no rebinning or normalization | Author-confirmed PDF used in Sec. 3.4.1 |
| `cshine-core-total-energy-correlations` | Traced to the exact central/side-core objects and their upstream selections | Focused ROOT reference and horizontal Python entry implemented; syntax and record checks passed | Real-data run checked both objects, their binning, and central-plus-side count consistency | Author-confirmed PDF used in Sec. 3.4.1 |
| `cshine-cosmic-muon-topology` | Historical long-paper producer retained; author selected the later Fig. 10-consistent beam-off producer for the thesis figure | Focused ROOT reference and horizontal Python entry switched to the final central/side selection; syntax, record, and remote-job checks passed | Final-selection objects checked: 21,911 spatial entries and 2,087,393 energy-sharing entries; transfer hashes agree | Author-approved PDF copied unchanged into thesis Sec. 3.5.1 |
| `cshine-slow-background-subtraction-figure` | Traced to the central `EnergySpecGen.C`, its two reconstructed spectra, and the published `histDiff` object | Focused ROOT reference and horizontal Python entry implemented; syntax and remote-job checks passed | Recomputed beam-off scale is 0.0664146554231; every content and error bin, including flow bins, agrees exactly with `spectrum_110.root:histDiff` | Author-approved horizontal PDF used in Sec. 3.5.1 |
| `cshine-fast-background-subtraction-figure` | Traced to the `RemoveSSDM2` signal/random branches, their common merge macro, and the saved `histDiff` object | Focused ROOT reference and horizontal Python entry implemented; syntax and remote-job checks passed | Equal-width background scale is 1; every content and error bin, including flow bins, agrees exactly with `step4-convert.0308.data/spectrum_110.root:histDiff` | Author-requested horizontal PDF used in Sec. 3.5.3 |
| `cshine-central-edge-spectrum-consistency` | Traced to the published `SpecCompare.ipynb`, the six input ROOT objects, and the central/edge producer macros | Focused ROOT reference, horizontal Python entry, staged record, and bounded remote job implemented and checked | `.0308.PreRun` and central `.0308` objects are identical; Python and ROOT agree in every regular and flow-bin content and uncertainty for all six derived spectra | Author-approved horizontal PDF used in Sec. 3.6.2 |
| `cshine-energy-core-time` | Traced to the exact merged ROOT object, upstream core selection, and historical drawing block | Focused ROOT reference and single-panel Python entry implemented; syntax and record checks passed | Real-data run checked the published binning, entries, displayed counts, flow counts, and output checksum | Author-confirmed redraw replaces the historical direct-copy figure in Sec. 3.3.4 |
| `cshine-trigger-tdc-distributions` | Traced to the seven trigger-monitor histograms, six-panel index order, and historical notebook assembly | Focused ROOT reference and Python two-row, three-column presentation entry implemented; revised-layout syntax and synthetic checks passed | Revised layout rerun on the authorized server with the previously validated input; all six object entries, in-range counts, and zero flow counts agree | Author-approved revised PDF used in Sec. 3.3.4 |
| `cshine-trigger-energy-time-correlations` | Traced to five trigger-specific `aa_example.C` branches and five `ALL_h2_TOF_TotalE` outputs; analysis-note Fig. 15 confirmed to contain five panels | Focused ROOT reference and Python two-row, three-column presentation entry implemented; Python syntax, ROOT-reference syntax, staged-record, and synthetic-layout checks passed | Authorized run recorded all five input hashes, object schemas, entries, displayed counts, flow counts, software versions, and output hashes | Author-approved PDF used in Sec. 3.3.4 |
| `cshine-spectra-reconstruction` | Traced to the per-run producer, two merge/display notebooks, exact 59-group March 5--10 sample, and three histogram families | M8/M9 provide the unchanged object production and merge; a parameterized Python display entry is implemented | Static, schema, and manifest checks passed; the portable path has not been rerun on the complete 59-group real sample | Author-approved historical PDF copied unchanged into Sec. 3.3.5 |
| `cshine-event-display` | Final PRC source panels traced to `EventALL/lego_0.root` and `EventDisplay/lego_5.root`; PRR remains a separate selection | Frozen-event producer, exact-bin validator, and ROOT `LEGO2` horizontal renderer implemented | Both 4 x 4 histograms match the historical ROOT panels exactly in all bins; the renderer reads only the validated record | Author-approved ROOT PDF replaces the published composite in Sec. 3.4.1 |

For the two trigger figures, the preferred reproducible input is the portable
M10B `trigger_diagnostics.root`. The TDC figure reads `h1_TrigList0`--`14`.
The adopted time--energy figure deliberately reads the `historical_` family so
that it remains identical in meaning to analysis-note Fig. 15; the distinct
`reviewed_` family retains the author-reviewed main-analysis veto definition
and is not substituted into the published figure without a new physics review.

The four status fields are independent:

- **Historical trace:** the source code, input objects, and physical settings
  have been identified;
- **Portable script:** the standalone program is present and has passed the
  stated local checks;
- **Real-data validation:** the program has been run against the authorized
  analysis output and compared with the historical result;
- **Thesis use:** the author has explicitly accepted or rejected the figure.

### Per-crystal spectra before and after reconstruction

The Sec. 3.3.5 figure is fully traced to the historical per-run producer and
the final `draw_merge_h_recon.ipynb` notebook. The notebook reads exactly 59
March 5--10 run-group outputs. This is a figure-specific historical sample;
the reviewed central analysis manifest additionally contains one March 4 run
group and must not be substituted silently.

The displayed object contract is:

- `h_eDep_0`--`h_eDep_14` for the individual detector spectra;
- `h_recon_5`, `6`, `9`, and `10` for the four central-core spectra;
- `h_recon_veto_4`, `7`, `8`, `11`, `13`, and `14` for the six edge-core
  spectra after the three-sided plastic-veto requirement;
- factor-ten rebinning for `h_eDep_*`, factor-five rebinning for reconstructed
  spectra, no normalization, and the historical 4 x 4 panel order.

M8 already produces these histogram families and M9 already merges them. The
dedicated `reconstruction_spectra_figure_run_groups.tsv` supplies only the
exact 59-group sample. `plot_reconstruction_spectra.py` reads the resulting
M9 per-crystal ROOT file and performs display only. The adopted thesis PDF is
the unchanged analysis-note source artifact; its source and thesis checksums
both equal
`d0ce788da5168a657eca8cc0de3058a02c903ed0c08542a86162bc63060b7292`.
The new portable path has not been run over the complete 59-group real sample,
so it is recorded as implemented rather than numerically validated.

The portable input layout and ROOT-object definitions are also recorded in
[`../docs/DATA_ACCESS.md`](../docs/DATA_ACCESS.md). Exact collaborator paths
remain outside this public-safe project.

### Current validation finding

The two historical `redrawNew` source PDFs were generated by Matplotlib 3.3.1
from the recorded CsI05 inputs and were subsequently combined into the
vertical analysis-note figure. Their SHA-256 values are:

```text
d9a95d60a29212d03132d70f4d5dbea86c485da96bea74afeda01be3b15b38b8  CsI05_fit2D_rebin16x16.pdf
fa2dd835556f82297108e421c06b947d648eb2087cabdef7f16428cab4e5a399  CsI05_corrected2D_rebin16x16.pdf
```

The first standalone ROOT-based redraw showed more isolated one-count bins
than these source PDFs. PDF composition was rejected as a substitute for
reproducibility. A source-to-source comparison found that the numerical
operations were equivalent, but the first portable version did not explicitly
preserve every historical rendering choice: ROOT-object type selection,
Matplotlib's `imshow` interpolation default, the PDF `dpi=300` argument, and
the layout call differed. These choices are now fixed explicitly. The author
reports that the revised CsI05 rendering is consistent with the historical
result. The final server run processed all 15 real channel inputs; every row
in `validation_summary.csv` passed, with zero count difference after both
rebinning and corrected-time re-histogramming. The CsI05 pair and the two
all-channel overviews are used in thesis Sec. 3.3.1; the remaining individual
pairs are retained for channel-by-channel inspection.

### Neighboring-crystal time-correlation trace

Analysis-note Fig. 5 was assembled from two source panels, but the composition
step is not part of the numerical provenance. Both panels originate from one
historical macro:

```text
DataPreprocessing/step4-convert.0308.PreRun/draw_GammaTimeDiff.C
```

The macro reads the `GammaCaliData` tree from the exact 60-file March 4--10
run list. Its no-cut block fills
`GammaTime[6]:GammaTime[5]` into 100 x 100 bins over -500 to 500 ns and writes
`GammaTime2D_all.pdf`. Its selected block uses the same expression and bins,
applies `GammaEnergy[5]+GammaEnergy[6]>=30`, and writes
`GammaTime2D_cut.pdf`.

M7 retains these ROOT expressions, the event list, selection, binning, and
range in the authoritative `neighbor_time_diagnostics.root` objects `h2_all`
and `h2_cut`. New runs call `plot_neighbor_time_correlation.py` with
`--diagnostics-root`; the direct-tree `--analysis-root` route is retained only
for provenance checks of the already adopted output. Python converts the two
stored TH2 objects only for a horizontal Matplotlib layout. The two panels retain complete, independent coordinate axes and
individual color bars. Panel labels `(a)` and `(b)` remain at the upper-left
of the respective axes, while the selection descriptions are left to the
caption rather than being drawn over the histograms. A dedicated spacer separates the left color bar
from the right panel's vertical-axis title, rather than relying on automatic
subplot spacing. The default keeps the historical independent
logarithmic count range for each panel. `--shared-color-scale` is an explicit
presentation-only alternative and is never enabled implicitly. The script also checks that no
energy-selected bin exceeds the corresponding no-cut bin and records input
file sizes, modification times, event counts, software versions, and output
settings in JSON. The 60-file construction and both independent/shared-scale
horizontal layouts passed local synthetic checks. The final independent-scale
layout was subsequently run against the authorized 60-file input on the
server; the completed output passed the program's selected-sample subset check
and was approved for thesis Sec. 3.3.2. The adopted PDF has SHA-256
`d8a831c68224678346cdc6da202606cda7f9dfd4e77c012b64a500ebc9b78ad0`.

### Unit-time and neighboring-crystal time-difference trace

Analysis-note Fig. 6 was assembled manually from two one-dimensional source
panels. Both panels, including the two curves in panel (b), come from one
historical ROOT macro:

```text
DataPreprocessing/step4-convert.0308.PreRun/draw_GammaTimeDiff.C
```

The macro has SHA-256
`6ad88f3440b6b79356f13701769d2d65eb48027f61fafcd050fc904cedcfd2c6`.
The analysis-note and PRC-supplement copies of the manually assembled PDF are
identical, with SHA-256
`583171dc8804ad37058b9b5290b7cea452b73b659bf59bd2f285aaf8a92dc05c`.
The macro predates the composite PDF by about eleven minutes, consistent with
the recorded manual layout step.

The numerical definitions are:

- panel (a): `GammaTime[5]`, 100 bins from -500 to 500 ns;
- panel (b), blue: `GammaTime[5]-GammaTime[6]`, 100 bins from -200 to
  200 ns, without an energy selection;
- panel (b), red: the same expression and bins with
  `GammaEnergy[5]+GammaEnergy[6]>=30`;
- before overlaying panel (b), the selected histogram is multiplied by the
  ratio of the two peak heights. This is a display scaling, not an event
  normalization or a change to the selection.

M7 stores the three relevant unscaled objects as `h3`, `h1`, and `hh_diff`.
`root/draw_neighbor_time_diagnostics.C` reads all six M7 objects and writes the
historical source panels. `plot_unit_time_and_neighbor_difference.py` reads
the same M7 file with `--diagnostics-root` and arranges the panels
horizontally. The previous direct-tree ROOT macro and the Python
`--analysis-root` mode remain available as provenance routes for the already
adopted real-data output, not as a second numerical production path.

Both entries were run against the authorized 60-file input. ROOT and Python
independently obtained a peak-scale factor of 52.00885. The Python record
contains 1,505,885 in-range entries for panel (a), 97,323 for the unselected
time-difference histogram, and 516 for the energy-selected histogram before
display scaling. The horizontal output has SHA-256
`47a30e1380cfdcfbfc3ff51bcc768a6a9c50c910727bb8c319abcc20010dac1a`.
The author approved this output, and the unchanged PDF is used in thesis
Sec. 3.3.2 as `cshine_gamma_unit05_time_neighbor_difference.pdf`.

### Reconstruction-multiplicity trace

Analysis-note Fig. 8 was manually assembled from two source panels. The
composition step is not part of the numerical provenance. Both source panels
are produced from:

```text
DataPreprocessing/step7-DeltaYrelated/h2_check.C
DataPreprocessing/step7-DeltaYrelated/Drawhistos.C
```

Their SHA-256 values are, respectively,
`d2870f286774c24f878b9eccdb7e9c2da1f3beecc1379a29aab91f321286096e`
and `9302e43a298451a55d7f5964c3909349a7d9573c3d694bf79ded27cc2d54444e`.
The analysis-note/PRC-supplement composite PDF has SHA-256
`035ce406d16983c257b66c88dc9976ccef1d40e1f1e3ed4e92bd0cc31d08f46c`.

The analysis macro reads the fixed 60-file March 4--10 reconstructed-event
sample and writes `h2_check.root`. Panel (a) reads
`ALL_h2_TotalE_LocalMulti`, a 200 x 9 histogram spanning 0--200 MeV and
cluster sizes 1--9. One entry is filled for each accepted reconstructed core;
the ordinate is the number of CsI(Tl) units assigned to that reconstructed
cluster. The accepted core must be one of the four central or six edge units;
an edge core is retained only when the three-sided veto count is zero.

Panel (b) reads `ALL_h1_EventMulti`, a nine-bin histogram of the number of
accepted reconstructed cores per trigger. The historical drawing macro then
constructed the red histogram by writing 6731 and 19 directly into the
`N_core=1` and `N_core=2` bins and setting the remaining bins to zero. The
portable plotting program preserves these fixed historical drawing values. It also reads
`ALL_h1_TwoHighEnergy_M1` through `M6` and `Mo` as a diagnostic of the available
ROOT-output version. Those objects receive one entry only when every accepted
core in that trigger has reconstructed total energy at least 35 MeV.

The adopted figure uses all 15 CsI(Tl) units as possible reconstructed
cores and applies no plastic-veto condition. The focused C++ diagnostic writes
three plotting objects from the same event loop:

- `all15_cluster_size_vs_total_energy`;
- `all15_core_multiplicity`;
- `all15_high_core_multiplicity`.

The last object counts, within each trigger, candidates that individually
satisfy `E_tot > 35 MeV`; its first two bins are 7218/21 and the remaining
bins are zero. The blue multiplicity curve is 22479729/635143/7882/61/1 in
its first five bins. No panel reads the older veto-selected `h2_check.root`.

`plot_reconstruction_multiplicity.py` preserves the ROOT binning, performs no
normalization, and uses logarithmic count scales. The historical 6731/19
constants are retained only in run metadata. The current script SHA-256 is
`edfd501149488901cce8aa4bcef7190511a104333faa4bdc93562ed4b5e3adef`;
the ROOT input SHA-256 is
`0a182512a3da37fabb1e5ff10813a4fbf85775ac879cfe0c82604380c50aaa1f`.

The server-generated source output is stored under
`results/candidates/all15_noveto/reconstruction_multiplicity/`. Its PDF,
PNG, and metadata SHA-256 values are respectively
`25ed70668ebf0850cfa3e654640a839407e614a3ad0e7e76da0c8b16b47f7fe5`,
`466bd030cbd8a8df9b130d30d6f266291f438ffd3c0d8fdd8a73fd950a9b155d`,
and `c045b8550490b73c4ff94bf4af8d79e7ced13cc4c3c8f5d02325460d527fde6b`.
It is numerically checked, author-confirmed, and copied unchanged into thesis
Sec. 3.3.5.

### Event-display trace and adopted reproducible thesis figure

Analysis-note Fig. 17 and the PRC supplementary material use the same
`EventDisplay.pdf`; its SHA-256 is
`0678af7166afc9c9487587db06e6fcfb5297bf2fafc46367299417ae1c65c909`.
The PRR figure is a distinct composition with different representative events
and selection conditions and must not be substituted for the PRC figure.

The historical output timestamps and source comments identify the cosmic panel
as `EventDisplay/lego_5.root`, produced with the retained `(N_Fire,
EnergyCut, FireIndex)=(5,100,2)` configuration. The gamma panel is
`EventDisplay/EventALL/lego_0.root`, identical to accepted page 16 of the
historical candidate browser. Static source inspection fixes its 60-file
sample, `jiugong_recon` definition, core range, side-veto condition,
110--200 MeV reconstructed-energy interval, and one-candidate-per-entry rule.

`build_event_display_records.cxx` reselects these two events and writes their
4 x 4 `TH2F` records with source entries and selection metadata.
`validate_event_display_records.cxx` then compares every bin against the two
historical ROOT canvases. The authorized run found zero mismatched bins and
zero maximum absolute difference for both panels. This closes the PRC and
analysis-note event-display numerical provenance; the distinct PRR event
display is not covered by this contract.

The adopted ROOT drawing entry consumes only the validated record. It uses the
historical `LEGO2` style and viewing angles, places the panels horizontally,
labels the four row and four column positions explicitly, and applies the same
0--100 MeV displayed-energy range to both panels. It neither repeats event
selection nor modifies bin contents. The author-approved PDF has SHA-256
`0691863f0a87f4e6e55dcf348d20930eab5a6682c607f28931e7f2736a287799`
and is used in thesis Sec. 3.4.1.

### Spatial-correlation energy-interval trace

Analysis-note Fig. 10 and the matching PRC supplementary figure use two source
panels from the main reconstructed-candidate diagnostic:

```text
DataPreprocessing/step7-DeltaYrelated/h2_check.C
DataPreprocessing/step7-DeltaYrelated/get_ax_ay.hh
DataPreprocessing/step7-DeltaYrelated/Drawhistos.C
```

The analysis macro reads the fixed 60-file March 4--10 `GammaCaliData` sample
and reconstructs candidates with `jiugong_recon`. The accepted core is one of
central units 5, 6, 9, 10 or side units 4, 7, 8, 11, 13, 14. Side-core
candidates additionally require `count_veto == 0`. No local-multiplicity cut
is applied to the two `ALL_h2` objects used here.

For every accepted candidate, `get_ax_ay.hh` computes the energy-weighted mean
absolute displacement from the energy centroid in the horizontal and vertical
crystal-grid directions. `h2_check.C` multiplies the two dimensionless grid
components by the 7 cm crystal pitch and fills:

- `ALL_h2_ax_ay_10_100` for `10 <= E_tot <= 100 MeV`;
- `ALL_h2_ax_ay_100_inf` for `E_tot > 100 MeV`.

Both are 70 x 70 `TH2I` histograms over 0--7 cm on each axis. The historical
`Drawhistos.C` draws the two objects separately with `COLZ` and `SetLogz`, with
no rebinning or normalization. The analysis-note and supplementary copies of
the manually assembled figure are identical; their SHA-256 is
`c0114c197e32f2d5216634c2e0ff343e2c2818410f37eb10397694c36c764696`.

`root/draw_spatial_correlation_energy_intervals.C` is a focused extraction of
the historical ROOT output layer. `plot_spatial_correlation_energy_intervals.py`
reads the same two objects and only changes the presentation to a horizontal
layout. Each panel retains a complete pair of axes and its own logarithmic
count color scale, because the historical source canvases scale the two
distributions independently. The script fixes the object names and binning,
rejects missing, empty, negative, or nonhistorically binned inputs, performs no
normalization, and records input and output checksums plus count summaries in
JSON. The real-data run used Python 3.6.9, ROOT 6.28/04, NumPy 1.19.5, and
Matplotlib 3.3.4. The two panels contain 83,807 and 1,574 counts respectively,
with no underflow or overflow. The resulting PDF was accepted by the author
and copied unchanged into thesis Sec. 3.4.1.

### Total-energy and spatial-spread trace

Analysis-note Fig. 11 and the identical PRC supplementary figure were
assembled from two source panels written by the same reconstructed-candidate
analysis used for Fig. 10:

```text
DataPreprocessing/step7-DeltaYrelated/h2_check.C
DataPreprocessing/step7-DeltaYrelated/get_ax_ay.hh
DataPreprocessing/step7-DeltaYrelated/Drawhistos.C
```

`h2_check.C` fills `ALL_h2_TotalE_DeltaY` with \(E_{\rm tot}\) versus the
vertical spread \(\delta_y\), and `ALL_h2_TotalE_Delta` with
\(E_{\rm tot}\) versus
\(\delta_r=\sqrt{\delta_x^2+\delta_y^2}\). The two objects use the same
accepted-candidate sample as the preceding spatial-correlation figure: four
central cores and six side cores, with `count_veto == 0` required for side
cores. Neither object adds a local-multiplicity requirement.

The historical axes are deliberately different. `ALL_h2_TotalE_DeltaY` has
50 x 70 bins over 5--200 MeV and 0--7 cm; `ALL_h2_TotalE_Delta` has 50 x 70
bins over 0--200 MeV and 0--7 cm. The portable entries preserve those ranges,
perform no rebinning or normalization, keep an independent logarithmic count
scale in each panel, and record all underflow and overflow separately from the
displayed bins. The analysis-note and supplementary composite PDFs are
identical, with SHA-256
`0e70234bc53355d3607a41a2014deae38fe1e808d27de4b43088e7621a38750e`.

The real-data pass confirmed 16,093,168 entries in each source object. The
displayed \(\delta_y\) histogram contains 2,199,688 entries because its
historical energy axis begins at 5 MeV; the displayed \(\delta_r\) histogram
contains 16,092,897 entries. The author-confirmed server PDF was copied
unchanged into thesis Sec. 3.4.1 with SHA-256
`0521814a91ebaa9c2317f13e2d392ae8f226b20f2e1efe9adca2a45153064002`.
The enhanced axis-by-axis flow metadata is not archived locally and is not
claimed as part of the completed validation record.

### Core-energy and total-energy trace

Analysis-note Fig. 12 and the identical PRC supplementary figure were
assembled from two canvases produced by:

```text
DataPreprocessing/step7-DeltaYrelated/h2_check.C
DataPreprocessing/step7-DeltaYrelated/Drawhistos.C
```

For each accepted reconstructed candidate, `h2_check.C` fills the
reconstructed cluster energy `E_tot` against the calibrated energy of the
core crystal `E_core`. The source objects are:

- `central_h2_TotalE_CenterE`, merging central cores 5, 6, 9, and 10;
- `side_h2_TotalE_CenterE`, merging side cores 4, 7, 8, 11, 13, and 14 after
  the historical `count_veto == 0` requirement.

Neither object applies an additional local-multiplicity requirement. Both are
50 x 80 `TH2I` histograms over 5--200 MeV in `E_tot` and 0--80 MeV in
`E_core`. `Drawhistos.C` draws them separately with `COLZ` and `SetLogz`,
without rebinning, normalization, or fitting. The analysis-note and
supplementary composite PDFs are identical; their SHA-256 is
`80bc2e54efdc71c22377292299075da205e9c1d5a8a894fe9b62b213dc4bb9fc`.

`root/draw_core_total_energy_correlations.C` preserves the two historical ROOT
panels. `plot_core_total_energy_correlations.py` reads the same two objects and
only changes the layout to a horizontal pair. Each panel retains complete
axes and an independent logarithmic count scale because the historical
canvases were scaled separately and the two core classes have different
acceptance and statistics. The program rejects missing, empty, negative, or
nonhistorically binned inputs and records input, output, in-range, underflow,
and overflow information in JSON.

The authorized run used Python 3.6.9, ROOT 6.28/04, NumPy 1.19.5, and
Matplotlib 3.3.4. The central and side objects contain 6,136,451 and 9,956,717
entries, with 928,635 and 1,270,716 counts inside the displayed ranges. Their
entry sum is 16,093,168, exactly matching the accepted-candidate total in the
spatial-spread objects from the same ROOT file. The author-confirmed PDF was
copied unchanged into thesis Sec. 3.4.1 with SHA-256
`64eb3f515be9e1beec46fd1f6730d30ec412ae6694a204d8d134936f18442ef2`.

### Reconstructed-energy and core-time trace

Analysis-note Fig. 13 and the identical PRC supplementary figure originate
from:

```text
DataPreprocessing/step7-DeltaYrelated/h2_check.C
DataPreprocessing/step7-DeltaYrelated/Drawhistos.C
```

For every accepted reconstructed candidate, `h2_check.C` fills the corrected
time `GammaTime[core]` on the horizontal axis and the reconstructed cluster
energy `E_tot` on the vertical axis. Accepted cores are central units 5, 6,
9, and 10, plus side units 4, 7, 8, 11, 13, and 14 after the historical
`count_veto == 0` requirement. No additional energy, time-window, or local-
multiplicity selection is applied. The merged source object is
`ALL_h2_TOF_TotalE`.

The per-crystal source histograms are created with 100 x 200 bins over
-500--500 ns and 0--200 MeV. The historical code initializes the intermediate
and all-core merge targets with a 0--7 vertical range before invoking ROOT
`Merge`; the published merged object and published PDF have the final
100 x 200, 0--200 MeV schema. The portable code therefore rejects any input
that does not match this actual published object schema. This records the
historical constructor discrepancy without changing the immutable source.

`root/draw_energy_core_time_correlation.C` preserves the historical single
ROOT panel. `plot_energy_core_time_correlation.py` reads the same merged
object, masks only empty bins, uses one logarithmic count scale, and writes
PDF, PNG, and JSON metadata. It does not reconstruct events, rebin, normalize,
or fit the distribution. The historical analysis-note and PRC PDFs are
byte-identical with SHA-256
`6e1aecec8a47d8b982de54a5e31d27278a39265ff4b05e71573cb6d45b7c60b7`.
The authorized real-data run used Python 3.6.9, ROOT 6.28/04, NumPy 1.19.5,
and Matplotlib 3.3.4. The source object contains 16,093,168 entries, of which
15,864,261 lie inside the displayed range; the 228,907 remaining counts are
recorded as time underflow/overflow and a small energy overflow. The
author-confirmed PDF was copied unchanged into thesis Sec. 3.3.4 with SHA-256
`b50b2f0a0ec348f93a8830aa317801675117d6b1a5d78242af3dad6e9b46caab`.

### Trigger-conditioned reconstructed-energy/core-time trace

Analysis-note Fig. 15 is a five-panel figure, not a six-panel trigger set. Its
historical three-row, two-column composition leaves the lower-right pad empty.
The five source panels come from independent
historical branches below `DataPreprocessing/step8-TimeCheck/`:

| Panel | Historical branch | Trigger-monitor element | Trigger condition |
|---|---|---:|---|
| (a) | `step5-onlygamma` | 17 | `SSD M1 & CsI M1` |
| (b) | `step5-SSDM2` | 18 | `SSD M2` |
| (c) | `step5-NAandSSD` | 19 | `SSD M1 & NA M1` |
| (d) | `step5-T0andNA` | 20 | `NA M1 & T0` |
| (e) | `step5-T0LS` | 22 | `LS & T0` |

Within every branch, `step4-convert.0308/aa_example.C` retains events when
the corresponding `TDC_Gamma_Trig_array[index]` lies inclusively between 100
and 4000. This is a valid-monitor condition; it does not select only the
narrow self-trigger peak. The five `aa_example.C` files differ in the
monitored index. Their downstream `step7-DeltaYrelated/h2_check.C` files are
byte-identical, as are the five `Drawhistos.C` files.

Each `h2_check.C` fills the corrected core time `GammaTime[core]` and total
reconstructed cluster energy `E_tot` into `ALL_h2_TOF_TotalE`. The central
and side-core definitions, including `count_veto == 0` for side cores, are
the same as in the all-trigger Fig. 13 analysis. Each historical
`Drawhistos.C` draws that object independently with a logarithmic count scale
and no rebinning or normalization. The original five panels were combined
manually into `DiffTrig.pdf`; the composition step does not define the
histogram content.

`root/draw_trigger_energy_time_correlations.C` is a portable ROOT reference
that reads the five existing ROOT outputs. The Python entry performs the same
read-only operation and creates two rows by three columns with an
intentionally empty lower-right pad. Each panel keeps its independent
logarithmic count scale; one common horizontal-axis title and one common
vertical-axis title replace repeated titles, and only the left column keeps
vertical tick labels. The individual color bars retain their own logarithmic
scales but omit repeated `Counts` titles.
Panel letters and trigger modes are placed together at the upper-left, using
a white outline rather than an opaque box. `ALL_OR` is not added: it appears
in the global all-trigger energy--time distribution and in Fig. 14's TDC
monitoring panel, but it is not a sixth trigger-conditioned panel in Fig. 15.

The authorized run used Python 3.6.9, ROOT 6.28/04, NumPy 1.19.5, and
Matplotlib 3.3.4. Panel entries are 15,140,779, 4,658,621, 50,384, 193,610,
and 3,925; corresponding displayed-bin counts are 15,096,166, 4,511,595,
48,813, 186,990, and 3,721. The PDF has SHA-256
`f2f4499a759d47666270409e3982dcaa0f15d35095e00c186c016e05147cd210`.
The PDF, PNG, and JSON record were downloaded and checksum-verified. The
author-approved PDF was copied unchanged into thesis Sec. 3.3.4.

## Figure workflow

For each requested figure:

1. trace the historical code, input objects, and physical selections;
2. add one standalone, semantically named plotting program here;
3. copy only that program to the server-side plotting directory;
4. write its output under `reproducible/results/` and compare it with the
   historical figure or analysis note;
5. update the status registry above with the validation result;
6. wait for the author's explicit thesis-use decision.

Do not batch-convert unrelated figures, modify the server production code, or
insert a generated output into the thesis automatically.

## Running a renderer

Each Python renderer accepts `--help`, an authorized analysis root, and an
output directory. For example:

```bash
python3 plotting/plot_time_amplitude_correction.py --help
python3 plotting/plot_time_amplitude_correction.py \
  --analysis-root /path/to/gamma2024 \
  --crystal-index 5 \
  --output-dir results/figures
```

Use the stable ID in the registry to locate the corresponding Python program,
ROOT reference under `plotting/root/`, staged record, and bounded remote job.
This avoids maintaining a second command catalog in this document.

The script directly ports the `redrawNew` operations: 16 x 16 two-dimensional
bin summation, reciprocal time-walk fit, cell-by-cell time correction,
corrected-time re-histogramming, and `imshow` display. Counts are not
normalized. Every raw and corrected panel generated in one run now uses one
common logarithmic count normalization, with `vmin = 1` and `vmax` equal to
the largest populated bin among all processed channels and both correction
states. The historical `antialiased`
interpolation and `dpi=300` PDF output are set explicitly rather than being
left to version-dependent defaults. The presentation layer uses
`CH_E` for the low-gain ADC-E address, one shared color bar for each output
figure, and non-repeated coordinate labels in the 4 x 4 overviews. Saving uses
a tight outer bounding box to remove page whitespace without changing axes
limits or histogram content.

The all-channel mode reuses the same single-channel calculation and produces:

```text
results/figures/time_walk/
├── individual/                 15 before/after channel pairs
├── overview/
│   ├── all_crystals_before_correction.pdf
│   └── all_crystals_after_correction.pdf
├── validation_summary.csv      parameters, input hashes, and count checks
└── run_metadata.json           code and software-environment record
```

The overview preserves the historical 4 x 4 channel arrangement. Channels in
each overview share the displayed time and ADC-address ranges; only the
leftmost column and bottom row retain numerical tick labels. The single-
channel pairs and both overviews use the same logarithmic count range in an
all-channel run, so equal colors represent equal bin counts across channels
and correction states.

Before processing begins, the batch mode verifies that all 15 ROOT/parameter
pairs exist. It stores only the cropped input count sum after each channel is
processed, rather than retaining all 15 full input arrays. Figures are written
only after both rebinning and corrected-time re-histogramming conserve counts
within `1e-9`; a failed check produces the CSV/JSON diagnostics and exits with
an error before new figures are drawn.

The current script passes a Python syntax check and a synthetic execution of
the shared-scale single-channel output and both 4 x 4 overview outputs. The
numerical implementation previously also passed a synthetic execution of the
15-row validation table.
The optimized nonzero-weight re-histogram was also compared directly with the
historical full-grid calculation and produced identical bins and edges in the
synthetic numerical test. The final all-channel run with real server inputs
also passed the recorded validation. The shared-scale revision was then run
against all 15 real channel inputs with Python 3.6.9, ROOT 6.28/04, NumPy
1.17.4, and Matplotlib 3.3.1. Every rebinning and corrected-time count
difference was zero; the common logarithmic range was 1--13621 counts. The
new figures were downloaded, approved by the author, and copied directly into
the thesis final-figure directory without image processing.

Requirements are Python 3.6 or newer, ROOT with PyROOT, NumPy, and Matplotlib.
The scripts intentionally avoid newer annotation and `dataclasses` syntax so
they can run in the historical server environment. Generated PDF, PNG, and
JSON files remain under `results/` and are not tracked.

Install the Python-only dependencies, if needed, with:

```bash
python3 -m pip install -r plotting/requirements.txt
```

### Cosmic-muon beam-off topology trace

Analysis-note Fig. 18 and the matching PRC supplementary figure are identical
PDFs. Their two historical source panels were traced to the dedicated
long-paper branch:

```text
DataPreprocessing/step11-otherFigsLongPaper/Fig2-deltaxdeltayBKG/
├── h2_check.C
├── get_ax_ay.hh
└── Drawhistos.C
```

Source inspection found that the dedicated producer reverses the later
central/side plastic-veto condition. The author therefore decided that the
thesis redraw must use
`DataPreprocessing/step7-DeltaYrelated/h2_check_BKG.root`, whose producer uses
the same selection as analysis-note Fig. 10: only central units 5, 6, 9, and
10 and side units 4, 7, 8, 11, 13, and 14 are retained; central candidates
have no plastic-veto requirement, whereas side candidates require
`count_veto == 0`. The dedicated long-paper output remains preserved as
historical provenance and is not used as the thesis candidate.

Panel (a) reads `ALL_h2_ax_ay_100_inf`, which has 70 x 70 bins over 0--7 cm
and requires `E_tot > 100 MeV`. Panel (b) reads
`ALL_h2_TotalE_CenterE`, which has 50 x 80 bins over `E_tot=5--200 MeV` and
`E_core=0--80 MeV` and has no additional energy selection. Neither panel is
rebinned, normalized, smoothed, or fitted by the drawing layer.

The author-confirmed-selection run used input SHA-256
`5d800ae93559528d395b4e296135640281e26c48ea593d6db203f5f9abb22145`.
The spatial object contains 21,911 entries, all inside the displayed range.
The energy-sharing object contains 2,087,393 entries, of which 32,056 lie
inside the displayed axes; the remainder is recorded in underflow and overflow
bins rather than silently discarded. The horizontal PDF, PNG, and JSON passed
remote-to-local SHA-256 verification. The PDF SHA-256 is
`8364dee60c284600147d1bdac0c05275cb5c32efb089a04af36fdc90e0741361`;
the author-approved PDF was copied unchanged into thesis Sec. 3.5.1 as
`cshine_gamma_cosmic_muon_topology.pdf`.

### Slow-coincidence beam-off subtraction trace

Thesis Fig. 3.17 and the published `SlowWinSpec.pdf` originate from the
central March 8 preprocessing branch:

```text
DataPreprocessing/step4-convert.0308/EnergySpecGen.C
```

The historical macro reads `h_total_E_M1` from `all_recon.root` and
`all_recon_BKG.root`. Both source histograms contain 1000 bins over
0--200 MeV. It enables statistical uncertainties, rebins by a factor of five
to 1 MeV per bin, scales the beam-off spectrum so that its integral over
110--200 MeV equals the beam-on integral, and forms
`histDiff = beam on - scaled beam off`. The resulting 200-bin histogram is
stored in `spectrum_110.root`. The drawing layer introduces no additional
event selection, smoothing, fitting, or normalization.

`root/draw_slow_coincidence_background_subtraction.C` preserves this ROOT
operation as a focused reference. `plot_slow_coincidence_background_subtraction.py`
repeats the same histogram arithmetic through PyROOT and only changes the
presentation to a one-row, two-column layout. Panel (a) shows the beam-on and
scaled beam-off spectra over 0--200 MeV; panel (b) shows the subtracted
spectrum over 0--100 MeV with propagated statistical uncertainties. Both
panels use logarithmic count axes.

The authorized real-data run obtained 1142 beam-on and 17195 beam-off counts
in the normalization interval, giving a beam-off scale of
0.0664146554231. The recomputed difference agrees with
`spectrum_110.root:histDiff` in every regular and flow bin: both the maximum
absolute content difference and the maximum absolute uncertainty difference
are zero. The horizontal candidate PDF has SHA-256
`4c5515771704a62704cae0dc0fd537740f8179f78b1821530094c5d567bd4809`.
The author approved the horizontal candidate. The validated PDF was copied
unchanged into thesis Sec. 3.5.1 as
`cshine_gamma_slow_coincidence_background_subtraction.pdf`; its SHA-256 remains
`4c5515771704a62704cae0dc0fd537740f8179f78b1821530094c5d567bd4809`.

### Fast-coincidence random-window subtraction trace

Thesis Fig. 3.18 and the published `FastWinSpec.pdf` originate from the
historical branch:

```text
DataPreprocessing/step10-DifferentTriggerMode/RemoveSSDM2/
```

Both signal and random-window producers reject events whose SSD-M2 trigger
monitor lies at `835 < TDC < 850`. The `step4-convert.0308.data/aa_example.C`
producer retains reconstructed cores in the -350-- -50 ns fast-coincidence
window, whereas `step4-convert.0308.bkg/aa_example.C` retains cores in the
equal-width 50--350 ns random window. Their identical `all_recon.C` macros
form `h_total_E_M1` from four central cores and six veto-qualified side cores.

`step4-convert.0308.data/EnergySpecGen.C` copies both 1000-bin, 0--200 MeV
histograms, enables statistical uncertainties, rebins by five to 1 MeV per
bin, and forms `histDiff = fast window - random window`. Because both time
windows are 300 ns wide, no additional scale is applied. The result is stored
as `step4-convert.0308.data/spectrum_110.root:histDiff`; the `110` file suffix
is inherited from the shared historical macro and does not introduce a
high-energy normalization in this branch.

`root/draw_fast_coincidence_background_subtraction.C` preserves the ROOT
operation as a focused reference. `plot_fast_coincidence_background_subtraction.py`
repeats the same arithmetic through PyROOT and only changes the layout to one
row by two columns. The authorized real-data run found zero mismatched content
or uncertainty bins, including underflow and overflow, relative to the saved
`histDiff` object at tolerance 1e-9. The validated PDF has SHA-256
`7f81b0c08bbf7b280784d8a3bc8de6b4e46491bd865b63f0c461357a504505df`
and was copied unchanged into thesis Sec. 3.5.3.

Run the bounded server task from the reproducible repository root:

```bash
python3 tools/remote_figure.py validate fast-coincidence-background-subtraction
python3 tools/remote_figure.py run fast-coincidence-background-subtraction
```

For an authorized environment in which the historical ROOT inputs are already
mounted locally, the Python presentation entry is:

```bash
python3 plotting/plot_fast_coincidence_background_subtraction.py \
  --analysis-root /path/to/gamma2024 \
  --output-dir results/figures
```
