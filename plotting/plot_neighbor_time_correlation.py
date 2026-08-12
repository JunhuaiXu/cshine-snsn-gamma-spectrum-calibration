#!/usr/bin/env python3
"""Draw the CsI05--CsI06 time-correlation panels used in thesis Sec. 3.3.2.

The event list, ROOT tree, expressions, energy selection, binning, and axis
range follow the two-dimensional blocks in the historical
``DataPreprocessing/step4-convert.0308.PreRun/draw_GammaTimeDiff.C`` macro.
The preferred input is the authoritative M7 ROOT file, in which ROOT has
already performed the historical event selection and histogram filling.
Matplotlib only places the two stored histograms side by side. Direct reading
of the historical event trees remains available for provenance checks.
"""

import argparse
import hashlib
import json
import platform
from datetime import datetime, timezone
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
import ROOT
from matplotlib.colors import LogNorm


TREE_NAME = "GammaCaliData"
TIME_BRANCH = "GammaTime"
ENERGY_BRANCH = "GammaEnergy"
CRYSTAL_5 = 5
CRYSTAL_6 = 6
ENERGY_THRESHOLD_MEV = 30.0
NUMBER_OF_BINS = 100
TIME_MIN_NS = -500.0
TIME_MAX_NS = 500.0


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Draw the CsI05--CsI06 time correlation without an energy cut "
            "and with E5 + E6 >= 30 MeV."
        )
    )
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument(
        "--analysis-root",
        type=Path,
        help="Legacy provenance mode: root of the gamma2024 analysis directory.",
    )
    source.add_argument(
        "--diagnostics-root",
        type=Path,
        help="Preferred mode: M7 neighbor_time_diagnostics.root file.",
    )
    parser.add_argument(
        "--input-subdir",
        type=Path,
        default=Path("DataPreprocessing/step4-convert.0308.PreRun"),
        help=(
            "Input directory relative to --analysis-root "
            "(default: DataPreprocessing/step4-convert.0308.PreRun)."
        ),
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("results/figures"),
        help="Directory below which the figure and metadata are written.",
    )
    parser.add_argument(
        "--shared-color-scale",
        action="store_true",
        help=(
            "Use one logarithmic count range and one color bar for both "
            "panels. By default each panel preserves the historical "
            "independent logarithmic range."
        ),
    )
    return parser.parse_args()


def historical_run_files(input_dir: Path) -> List[Path]:
    """Return the exact 60-file list encoded in the historical ROOT macro."""

    relative_names = ["a20240304_SnSn_GOAL_ALLCOIN.006.root"]
    relative_names.extend(
        "a20240305_SnSn_GOAL_ALLCOIN.00%d.root" % index
        for index in range(8)
    )
    relative_names.extend(
        "a20240306_SnSn_GOAL_ALLCOIN.0%02d.root" % index
        for index in range(15)
    )
    relative_names.extend(
        "a20240307_SnSn_GOAL_ALLCOIN.0%02d.root" % index
        for index in range(14)
    )
    relative_names.extend(
        "a20240308_SnSn_GOAL_ALLCOIN.0%02d.root" % index
        for index in range(11)
    )
    relative_names.extend(
        "a20240309_SnSn_GOAL_ALLCOIN.0%02d.root" % index
        for index in range(4)
    )
    relative_names.extend(
        "a20240310_SnSn_GOAL_ALLCOIN.0%02d.root" % index
        for index in range(7)
    )
    paths = [input_dir / name for name in relative_names]
    if len(paths) != 60:
        raise RuntimeError("The historical input list must contain 60 ROOT files.")
    return paths


def require_inputs(paths: Sequence[Path]) -> None:
    missing = [path for path in paths if not path.is_file()]
    if missing:
        formatted = "\n".join("  - %s" % path for path in missing)
        raise FileNotFoundError("Required historical inputs are missing:\n%s" % formatted)


def validate_tree_layout(first_path: Path) -> None:
    root_file = ROOT.TFile.Open(str(first_path), "READ")
    if not root_file or root_file.IsZombie():
        raise OSError("Cannot open ROOT input: %s" % first_path)
    try:
        tree = root_file.Get(TREE_NAME)
        if not tree:
            raise KeyError("Tree %s is absent from %s" % (TREE_NAME, first_path))
        for branch_name in (TIME_BRANCH, ENERGY_BRANCH):
            if not tree.GetBranch(branch_name):
                raise KeyError(
                    "Branch %s is absent from %s:%s"
                    % (branch_name, first_path, TREE_NAME)
                )
    finally:
        root_file.Close()


def fill_historical_histograms(
    input_paths: Sequence[Path],
) -> Tuple[ROOT.TH2, ROOT.TH2, int, int, int]:
    """Fill the two TH2 objects through the historical TChain.Draw expressions."""

    chain = ROOT.TChain(TREE_NAME)
    for path in input_paths:
        if chain.Add(str(path)) != 1:
            raise OSError("ROOT.TChain failed to add %s" % path)

    total_entries = int(chain.GetEntries())
    if total_entries <= 0:
        raise ValueError("The historical TChain contains no entries.")

    expression = "%s[%d]:%s[%d]" % (
        TIME_BRANCH,
        CRYSTAL_6,
        TIME_BRANCH,
        CRYSTAL_5,
    )
    selection = "%s[%d]+%s[%d]>=%g" % (
        ENERGY_BRANCH,
        CRYSTAL_5,
        ENERGY_BRANCH,
        CRYSTAL_6,
        ENERGY_THRESHOLD_MEV,
    )

    histogram_specification = "(%d,%g,%g,%d,%g,%g)" % (
        NUMBER_OF_BINS,
        TIME_MIN_NS,
        TIME_MAX_NS,
        NUMBER_OF_BINS,
        TIME_MIN_NS,
        TIME_MAX_NS,
    )

    # Ensure Draw-created histograms belong to ROOT's memory directory rather
    # than to an input file that will later be closed.
    ROOT.gROOT.cd()
    selected_all = int(
        chain.Draw(
            "%s>>h_neighbor_time_all%s" % (expression, histogram_specification),
            "",
            "goff",
        )
    )
    if selected_all < 0:
        raise RuntimeError("ROOT failed to fill the no-cut time-correlation histogram.")
    histogram_all_source = ROOT.gDirectory.Get("h_neighbor_time_all")
    if not histogram_all_source:
        raise KeyError("ROOT did not create h_neighbor_time_all.")
    histogram_all = histogram_all_source.Clone("h_neighbor_time_all_detached")
    histogram_all.SetDirectory(0)

    selected_cut = int(
        chain.Draw(
            "%s>>h_neighbor_time_cut%s" % (expression, histogram_specification),
            selection,
            "goff",
        )
    )
    if selected_cut < 0:
        raise RuntimeError("ROOT failed to fill the energy-selected histogram.")
    histogram_cut_source = ROOT.gDirectory.Get("h_neighbor_time_cut")
    if not histogram_cut_source:
        raise KeyError("ROOT did not create h_neighbor_time_cut.")
    histogram_cut = histogram_cut_source.Clone("h_neighbor_time_cut_detached")
    histogram_cut.SetDirectory(0)

    return histogram_all, histogram_cut, total_entries, selected_all, selected_cut


def read_m7_histograms(
    diagnostics_path: Path,
) -> Tuple[ROOT.TH2, ROOT.TH2, int, int, int]:
    root_file = ROOT.TFile.Open(str(diagnostics_path), "READ")
    if not root_file or root_file.IsZombie():
        raise OSError("Cannot open M7 diagnostics: %s" % diagnostics_path)
    try:
        source_all = root_file.Get("h2_all")
        source_cut = root_file.Get("h2_cut")
        if not source_all or not source_cut:
            raise KeyError("M7 diagnostics must contain h2_all and h2_cut.")
        histogram_all = source_all.Clone("h2_all_m7_detached")
        histogram_cut = source_cut.Clone("h2_cut_m7_detached")
        histogram_all.SetDirectory(0)
        histogram_cut.SetDirectory(0)
        return (
            histogram_all,
            histogram_cut,
            int(round(histogram_all.GetEntries())),
            int(round(histogram_all.GetEntries())),
            int(round(histogram_cut.GetEntries())),
        )
    finally:
        root_file.Close()


def th2_to_numpy(histogram: ROOT.TH2) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Return x edges, y edges, and a y-by-x count array from a ROOT TH2."""

    number_x = histogram.GetNbinsX()
    number_y = histogram.GetNbinsY()
    x_axis = histogram.GetXaxis()
    y_axis = histogram.GetYaxis()
    x_edges = np.array(
        [x_axis.GetBinLowEdge(index) for index in range(1, number_x + 2)],
        dtype=float,
    )
    y_edges = np.array(
        [y_axis.GetBinLowEdge(index) for index in range(1, number_y + 2)],
        dtype=float,
    )
    counts = np.array(
        [
            [
                histogram.GetBinContent(x_index, y_index)
                for x_index in range(1, number_x + 1)
            ]
            for y_index in range(1, number_y + 1)
        ],
        dtype=float,
    )
    return x_edges, y_edges, counts


def positive_maximum(counts: np.ndarray, label: str) -> float:
    populated = counts[counts > 0]
    if populated.size == 0:
        raise ValueError("The %s histogram has no populated in-range bins." % label)
    return float(np.max(populated))


def configure_plot_style() -> None:
    plt.rcParams.update(
        {
            "font.family": "serif",
            "font.serif": ["DejaVu Serif"],
            "mathtext.fontset": "dejavuserif",
            "font.size": 10.5,
            "axes.labelsize": 12,
            "xtick.labelsize": 10,
            "ytick.labelsize": 10,
            "axes.linewidth": 1.1,
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
        }
    )


def draw_horizontal_figure(
    x_edges: np.ndarray,
    y_edges: np.ndarray,
    counts_all: np.ndarray,
    counts_cut: np.ndarray,
    output_pdf: Path,
    output_png: Path,
    shared_color_scale: bool,
) -> Dict[str, object]:
    configure_plot_style()

    maxima = [
        positive_maximum(counts_all, "no-cut"),
        positive_maximum(counts_cut, "energy-selected"),
    ]
    if shared_color_scale:
        norms = [LogNorm(vmin=1.0, vmax=max(maxima))] * 2
    else:
        norms = [LogNorm(vmin=1.0, vmax=maximum) for maximum in maxima]

    fig = plt.figure(figsize=(11.2, 4.15))
    grid = fig.add_gridspec(
        nrows=1,
        ncols=5,
        width_ratios=(1.0, 0.05, 0.34, 1.0, 0.05),
        left=0.065,
        right=0.975,
        bottom=0.15,
        top=0.96,
        wspace=0.08,
    )
    axes = (fig.add_subplot(grid[0, 0]), fig.add_subplot(grid[0, 3]))
    colorbar_axes = (
        fig.add_subplot(grid[0, 1]),
        fig.add_subplot(grid[0, 4]),
    )

    panel_data = ((counts_all, "(a)"), (counts_cut, "(b)"))
    meshes = []
    for axis, (counts, panel_label), norm in zip(axes, panel_data, norms):
        mesh = axis.pcolormesh(
            x_edges,
            y_edges,
            np.ma.masked_less_equal(counts, 0.0),
            cmap="viridis",
            norm=norm,
            shading="flat",
            rasterized=True,
        )
        meshes.append(mesh)
        axis.set_xlim(TIME_MIN_NS, TIME_MAX_NS)
        axis.set_ylim(TIME_MIN_NS, TIME_MAX_NS)
        axis.set_xlabel(r"$t_{5}$ (ns)")
        axis.set_ylabel(r"$t_{6}$ (ns)")
        axis.minorticks_on()
        axis.tick_params(
            which="major",
            direction="in",
            top=True,
            right=True,
            length=5,
            width=1.0,
        )
        axis.tick_params(
            which="minor",
            direction="in",
            top=True,
            right=True,
            length=2.5,
            width=0.8,
        )
        axis.text(
            0.04,
            0.95,
            panel_label,
            transform=axis.transAxes,
            ha="left",
            va="top",
            fontsize=12,
            fontweight="bold",
            color="black",
            zorder=5,
        )
    for colorbar_axis, mesh in zip(colorbar_axes, meshes):
        colorbar = fig.colorbar(mesh, cax=colorbar_axis)
        colorbar.set_label("Counts")

    output_pdf.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output_pdf, bbox_inches="tight")
    fig.savefig(output_png, dpi=300, bbox_inches="tight")
    plt.close(fig)

    return {
        "shared_color_scale": shared_color_scale,
        "no_cut_vmin": 1.0,
        "no_cut_vmax": float(norms[0].vmax),
        "energy_cut_vmin": 1.0,
        "energy_cut_vmax": float(norms[1].vmax),
    }


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as input_file:
        while True:
            block = input_file.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def write_metadata(
    metadata_path: Path,
    analysis_root: Path,
    input_dir: Path,
    input_paths: Sequence[Path],
    total_entries: int,
    selected_all: int,
    selected_cut: int,
    counts_all: np.ndarray,
    counts_cut: np.ndarray,
    color_scale: Dict[str, object],
    output_pdf: Path,
    output_png: Path,
    input_mode: str,
) -> None:
    input_records = []
    for path in input_paths:
        stat_result = path.stat()
        input_records.append(
            {
                "path_relative_to_analysis_root": str(
                    path.relative_to(analysis_root)
                ),
                "size_bytes": stat_result.st_size,
                "mtime_unix": stat_result.st_mtime,
            }
        )

    script_path = Path(__file__).resolve()
    metadata = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "script": script_path.name,
        "script_sha256": sha256_file(script_path),
        "python_version": platform.python_version(),
        "root_version": str(ROOT.gROOT.GetVersion()),
        "numpy_version": np.__version__,
        "matplotlib_version": matplotlib.__version__,
        "analysis_root": str(analysis_root),
        "input_mode": input_mode,
        "input_directory_relative_to_analysis_root": str(
            input_dir.relative_to(analysis_root)
        ),
        "tree": TREE_NAME,
        "input_files": input_records,
        "input_file_count": len(input_paths),
        "total_chain_entries": total_entries,
        "selected_entries_no_cut": selected_all,
        "selected_entries_energy_cut": selected_cut,
        "in_range_counts_no_cut": float(np.sum(counts_all)),
        "in_range_counts_energy_cut": float(np.sum(counts_cut)),
        "selection": "GammaEnergy[5] + GammaEnergy[6] >= 30 MeV",
        "x_quantity": "GammaTime[5]",
        "y_quantity": "GammaTime[6]",
        "bins_per_axis": NUMBER_OF_BINS,
        "time_range_ns": [TIME_MIN_NS, TIME_MAX_NS],
        "color_scale": color_scale,
        "outputs": {
            "pdf": str(output_pdf),
            "png": str(output_png),
        },
    }
    metadata_path.write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def main() -> None:
    arguments = parse_arguments()
    ROOT.gROOT.SetBatch(True)

    if arguments.diagnostics_root is not None:
        diagnostics_path = arguments.diagnostics_root.expanduser().resolve()
        if not diagnostics_path.is_file():
            raise FileNotFoundError("M7 diagnostics are missing: %s" % diagnostics_path)
        analysis_root = diagnostics_path.parent
        input_dir = diagnostics_path.parent
        input_paths = [diagnostics_path]
        (
            histogram_all,
            histogram_cut,
            total_entries,
            selected_all,
            selected_cut,
        ) = read_m7_histograms(diagnostics_path)
        input_mode = "m7-diagnostics-root"
    else:
        analysis_root = arguments.analysis_root.expanduser().resolve()
        if arguments.input_subdir.is_absolute():
            raise ValueError("--input-subdir must be relative to --analysis-root.")
        input_dir = analysis_root / arguments.input_subdir
        input_paths = historical_run_files(input_dir)
        require_inputs(input_paths)
        validate_tree_layout(input_paths[0])
        (
            histogram_all,
            histogram_cut,
            total_entries,
            selected_all,
            selected_cut,
        ) = fill_historical_histograms(input_paths)
        input_mode = "legacy-event-tree-provenance"

    x_edges_all, y_edges_all, counts_all = th2_to_numpy(histogram_all)
    x_edges_cut, y_edges_cut, counts_cut = th2_to_numpy(histogram_cut)
    if not np.array_equal(x_edges_all, x_edges_cut) or not np.array_equal(
        y_edges_all, y_edges_cut
    ):
        raise RuntimeError("The two historical histograms do not share bin edges.")
    if np.any(counts_cut > counts_all):
        raise RuntimeError(
            "An energy-selected bin exceeds the corresponding no-cut bin."
        )

    output_root = (
        arguments.output_dir.expanduser().resolve()
        / "neighbor_time_correlation"
    )
    suffix = "_shared_scale" if arguments.shared_color_scale else ""
    output_stem = (
        "cshine_gamma_neighbor_time_correlation_csi05_csi06_horizontal" + suffix
    )
    output_pdf = output_root / (output_stem + ".pdf")
    output_png = output_root / (output_stem + ".png")
    metadata_path = output_root / (output_stem + "_metadata.json")

    color_scale = draw_horizontal_figure(
        x_edges_all,
        y_edges_all,
        counts_all,
        counts_cut,
        output_pdf,
        output_png,
        arguments.shared_color_scale,
    )
    write_metadata(
        metadata_path,
        analysis_root,
        input_dir,
        input_paths,
        total_entries,
        selected_all,
        selected_cut,
        counts_all,
        counts_cut,
        color_scale,
        output_pdf,
        output_png,
        input_mode,
    )

    print("PDF saved to: %s" % output_pdf)
    print("PNG saved to: %s" % output_png)
    print("Metadata saved to: %s" % metadata_path)


if __name__ == "__main__":
    main()
