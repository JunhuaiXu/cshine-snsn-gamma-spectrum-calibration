# M8 shower-reconstruction migration review

## Scope

M8 migrates the per-event CSHINE-Gamma shower reconstruction used in thesis
Sec. 3.3.3. It consumes calibrated `GammaCaliData` trees produced by M6 and
writes the same tree schema with two additional branches:

- `recon_result`: `std::map<unsigned short, jiugong_recon_result_t>`;
- `count_veto`: number of the three plastic-veto faces with a valid TDC signal.

Run-group discovery, beam-on/beam-off roles, and histogram merging are outside
the M8 boundary and are closed separately in M9. M8 accepts explicit input
files so its input record cannot silently change with a directory glob.

## Frozen sources

| Source ID | Historical path | SHA-256 | Role |
|---|---|---|---|
| `DP-S307` | `t_gamma_cali/jiugong_recon.h` | `784ee46f9965074234bbe263dd3a7b65b6f5f2ebff764b9982c6aab5352de83a` | result type, crystal adjacency, ordering, and cluster construction |
| `DP-S308` | `t_gamma_cali/jiugong_recon.C` | `57ae24452f3d21c080f513400e11742fca93bfe4ced33f95658e1ed95eb00ded` | ROOT class implementation |
| `DP-S600` | `step4-convert.0308/aa_example.C` | `42ab302005e6e29a4f5ea7e2f7b7c1c7fb38bebdf02adcb4eed4d10ea2ad8d79` | central event producer, veto count, and candidate-core use |

The source files remain unchanged in the immutable snapshot. Generated ROOT
dictionary code is regenerated from portable headers and is not copied.

## Preserved physical definition

- The 15 crystal energies are inspected in descending order.
- A crystal is eligible when its reconstructed energy is greater than or
  equal to 1 MeV and its corrected time is finite.
- Both side-sharing and corner-sharing crystal neighbors are included.
- A neighboring deposit is included in one shower when the absolute time
  difference from its core is less than or equal to 50 ns.
- A nonmerged deposit more than 100 ns from prior cores starts another core.
- A deposit within 100 ns of a stronger core but outside the 50 ns merge
  window retains the historical invalid-center placeholder entry. Downstream
  candidate selection explicitly rejects that entry.
- Main-spectrum cores are the four central crystals 5, 6, 9, and 10 and the
  six left/right side crystals 4, 7, 8, 11, 13, and 14.
- Central cores do not require veto silence. A side core is accepted only when
  all three plastic-veto faces are silent.
- A veto face counts as firing only for the strict interval
  `100 < TDC_Veto < 4000`. The three faces are counted independently.
- Crystals 1 and 2 and corners 0, 3, and 12 remain outside the main-spectrum
  core set.

The historical unused `Energy_HighCut` declaration is not promoted into a new
selection. Equal-energy ordering retains the historical `std::sort`
comparator and is not assigned a new tie-breaking rule.

## Portable implementation

The historical algorithm and ROOT result type are in `include/jiugong_recon.h`
and `src/jiugong_recon.cxx`. The reviewed crystal-role and veto helpers are in
`include/shower_reconstruction.h` and `src/shower_reconstruction.cxx`. ROOT
streaming declarations are isolated in `include/ReconstructionLinkDef.h`.

`build_reconstructed_event_tree` validates every input file, reads the M6
energy, time, gamma ADC/TDC, veto ADC/TDC, and T0 arrays, and preserves these
branches in the output. The M6 contract was therefore extended only to retain
the three raw veto ADC and TDC channels; no M8 selection is applied during M6.

The unified wrapper records explicit input files, file metadata or optional
SHA-256 values, configuration, software environment, executable checksum,
run report, outputs, and status:

```bash
python3 tools/data_preprocessing.py m8 \
  --input /path/to/calibrated_run_a.root \
  --input /path/to/calibrated_run_b.root \
  --run-id shower-check
```

## Validation

The isolated server build used GCC 9.4.0 and ROOT 6.28/04. Configuration,
compilation, installation, and all 12 registered M1--M8 C++ tests passed. The
two M8 tests cover threshold and time boundaries, orthogonal and diagonal
neighbors, multiple cores, placeholders, crystal roles, strict veto limits,
candidate acceptance, ROOT dictionaries, streamed nested containers, input
schema, retained branches, run summaries, overwrite protection, nested output
creation, and missing-branch failure.

The complete historical experimental sample was not rerun, consistent with
the direct-migration acceptance boundary. This closes the M8 code migration;
it does not claim a new numerical reproduction of the published spectrum.

## Downstream boundary

M9 now defines the exact run manifests and sample roles, executes reconstruction
per run, and merges central, side, and combined spectra without changing the
per-event reconstruction contract closed here. Its evidence and validation are
recorded in `M9_RUN_AND_SPECTRUM_MERGE_MIGRATION_REVIEW.md`.
