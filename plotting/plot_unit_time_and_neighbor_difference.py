#!/usr/bin/env python3
"""Draw analysis-note Fig. 6 from the historical CSHINE-Gamma event trees.

Panel (a) is the corrected-time distribution of CsI05. Panel (b) compares
the CsI05--CsI06 corrected-time difference before and after applying
``GammaEnergy[5] + GammaEnergy[6] >= 30``. The preferred input is the
authoritative M7 ROOT file; Matplotlib only arranges its stored one-dimensional
objects horizontally. Direct event-tree reading remains available for
provenance checks.

The energy-selected time-difference histogram is multiplied by the ratio of
the no-cut and selected peak heights, exactly as in the historical ROOT macro.
The unscaled counts and the scale factor are retained in the metadata output.
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


TREE_NAME = "GammaCaliData"
TIME_BRANCH = "GammaTime"
ENERGY_BRANCH = "GammaEnergy"
CRYSTAL_5 = 5
CRYSTAL_6 = 6
ENERGY_THRESHOLD_MEV = 30.0

TIME_NUMBER_OF_BINS = 100
TIME_MIN_NS = -500.0
TIME_MAX_NS = 500.0

DIFFERENCE_NUMBER_OF_BINS = 100
DIFFERENCE_MIN_NS = -200.0
DIFFERENCE_MAX_NS = 200.0


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Draw the CsI05 time distribution and the CsI05--CsI06 time-"
            "difference comparison used in analysis-note Fig. 6."
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
    return parser.parse_args()


def historical_run_files(input_dir: Path) -> List[Path]:
    """Return the exact 60-file input list encoded in the historical macro."""

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


def draw_histogram(
    chain: ROOT.TChain,
    expression: str,
    selection: str,
    name: str,
    number_of_bins: int,
    lower_edge: float,
    upper_edge: float,
) -> Tuple[ROOT.TH1, int]:
    specification = "%s>>%s(%d,%g,%g)" % (
        expression,
        name,
        number_of_bins,
        lower_edge,
        upper_edge,
    )
    ROOT.gROOT.cd()
    selected_entries = int(chain.Draw(specification, selection, "goff"))
    if selected_entries < 0:
        raise RuntimeError("ROOT failed to fill histogram %s." % name)
    source = ROOT.gDirectory.Get(name)
    if not source:
        raise KeyError("ROOT did not create histogram %s." % name)
    histogram = source.Clone(name + "_detached")
    histogram.SetDirectory(0)
    return histogram, selected_entries


def fill_historical_histograms(
    input_paths: Sequence[Path],
) -> Tuple[ROOT.TH1, ROOT.TH1, ROOT.TH1, int, Dict[str, int]]:
    """Fill the three historical TH1 objects through TChain.Draw."""

    chain = ROOT.TChain(TREE_NAME)
    for path in input_paths:
        if chain.Add(str(path)) != 1:
            raise OSError("ROOT.TChain failed to add %s" % path)

    total_entries = int(chain.GetEntries())
    if total_entries <= 0:
        raise ValueError("The historical TChain contains no entries.")

    time_expression = "%s[%d]" % (TIME_BRANCH, CRYSTAL_5)
    difference_expression = "%s[%d]-%s[%d]" % (
        TIME_BRANCH,
        CRYSTAL_5,
        TIME_BRANCH,
        CRYSTAL_6,
    )
    energy_selection = "%s[%d]+%s[%d]>=%g" % (
        ENERGY_BRANCH,
        CRYSTAL_5,
        ENERGY_BRANCH,
        CRYSTAL_6,
        ENERGY_THRESHOLD_MEV,
    )

    time_5, selected_time_5 = draw_histogram(
        chain,
        time_expression,
        "",
        "h_unit05_time",
        TIME_NUMBER_OF_BINS,
        TIME_MIN_NS,
        TIME_MAX_NS,
    )
    difference_all, selected_difference_all = draw_histogram(
        chain,
        difference_expression,
        "",
        "h_neighbor_difference_all",
        DIFFERENCE_NUMBER_OF_BINS,
        DIFFERENCE_MIN_NS,
        DIFFERENCE_MAX_NS,
    )
    difference_cut, selected_difference_cut = draw_histogram(
        chain,
        difference_expression,
        energy_selection,
        "h_neighbor_difference_cut",
        DIFFERENCE_NUMBER_OF_BINS,
        DIFFERENCE_MIN_NS,
        DIFFERENCE_MAX_NS,
    )
    selected_entries = {
        "unit05_time_no_cut": selected_time_5,
        "difference_no_cut": selected_difference_all,
        "difference_energy_cut": selected_difference_cut,
    }
    return time_5, difference_all, difference_cut, total_entries, selected_entries


def read_m7_histograms(
    diagnostics_path: Path,
) -> Tuple[ROOT.TH1, ROOT.TH1, ROOT.TH1, int, Dict[str, int]]:
    root_file = ROOT.TFile.Open(str(diagnostics_path), "READ")
    if not root_file or root_file.IsZombie():
        raise OSError("Cannot open M7 diagnostics: %s" % diagnostics_path)
    try:
        sources = [root_file.Get(name) for name in ("h3", "h1", "hh_diff")]
        if any(not histogram for histogram in sources):
            raise KeyError("M7 diagnostics must contain h3, h1, and hh_diff.")
        time_5 = sources[0].Clone("h3_m7_detached")
        difference_all = sources[1].Clone("h1_m7_detached")
        difference_cut = sources[2].Clone("hh_diff_m7_detached")
        for histogram in (time_5, difference_all, difference_cut):
            histogram.SetDirectory(0)
        selected_entries = {
            "unit05_time_no_cut": int(round(time_5.GetEntries())),
            "difference_no_cut": int(round(difference_all.GetEntries())),
            "difference_energy_cut": int(round(difference_cut.GetEntries())),
        }
        return (
            time_5,
            difference_all,
            difference_cut,
            int(round(time_5.GetEntries())),
            selected_entries,
        )
    finally:
        root_file.Close()


def th1_to_numpy(histogram: ROOT.TH1) -> Tuple[np.ndarray, np.ndarray]:
    number_of_bins = histogram.GetNbinsX()
    axis = histogram.GetXaxis()
    edges = np.array(
        [axis.GetBinLowEdge(index) for index in range(1, number_of_bins + 2)],
        dtype=float,
    )
    counts = np.array(
        [histogram.GetBinContent(index) for index in range(1, number_of_bins + 1)],
        dtype=float,
    )
    return edges, counts


def positive_maximum(counts: np.ndarray, label: str) -> float:
    maximum = float(np.max(counts))
    if maximum <= 0.0:
        raise ValueError("The %s histogram has no positive in-range count." % label)
    return maximum


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
            "legend.fontsize": 9.5,
            "axes.linewidth": 1.1,
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
        }
    )


def plot_step(axis, edges: np.ndarray, counts: np.ndarray, **kwargs) -> None:
    values = np.append(counts, counts[-1])
    axis.step(edges, values, where="post", **kwargs)


def style_axis(axis) -> None:
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


def draw_horizontal_figure(
    time_edges: np.ndarray,
    time_counts: np.ndarray,
    difference_edges: np.ndarray,
    difference_counts_all: np.ndarray,
    difference_counts_cut: np.ndarray,
    peak_scale_factor: float,
    output_pdf: Path,
    output_png: Path,
) -> None:
    configure_plot_style()
    scaled_difference_cut = difference_counts_cut * peak_scale_factor

    figure, axes = plt.subplots(1, 2, figsize=(9.4, 3.75))
    figure.subplots_adjust(
        left=0.085,
        right=0.985,
        bottom=0.18,
        top=0.965,
        wspace=0.24,
    )

    plot_step(
        axes[0],
        time_edges,
        time_counts,
        color="#4477AA",
        linewidth=1.7,
    )
    axes[0].set_xlim(TIME_MIN_NS, TIME_MAX_NS)
    axes[0].set_ylim(0.0, 1.08 * positive_maximum(time_counts, "CsI05 time"))
    axes[0].set_xlabel(r"$t_{5}$ (ns)")
    axes[0].set_ylabel("Counts")

    plot_step(
        axes[1],
        difference_edges,
        difference_counts_all,
        color="#4477AA",
        linewidth=1.7,
        label="No energy cut",
    )
    plot_step(
        axes[1],
        difference_edges,
        scaled_difference_cut,
        color="#CC3311",
        linewidth=1.7,
        label=r"$E_{5}+E_{6}\geq 30$ MeV",
    )
    axes[1].set_xlim(DIFFERENCE_MIN_NS, DIFFERENCE_MAX_NS)
    axes[1].set_ylim(
        0.0,
        1.08
        * max(
            positive_maximum(difference_counts_all, "no-cut time difference"),
            positive_maximum(scaled_difference_cut, "scaled selected time difference"),
        ),
    )
    axes[1].set_xlabel(r"$t_{5}-t_{6}$ (ns)")
    axes[1].set_ylabel("Counts")
    axes[1].legend(loc="upper left", frameon=False, handlelength=2.4)

    for axis, panel_label in zip(axes, ("(a)", "(b)")):
        style_axis(axis)
        axis.text(
            0.96,
            0.94,
            panel_label,
            transform=axis.transAxes,
            ha="right",
            va="top",
            fontsize=12,
            fontweight="bold",
        )

    output_pdf.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(output_pdf, bbox_inches="tight")
    figure.savefig(output_png, dpi=300, bbox_inches="tight")
    plt.close(figure)


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
    selected_entries: Dict[str, int],
    time_counts: np.ndarray,
    difference_counts_all: np.ndarray,
    difference_counts_cut: np.ndarray,
    peak_scale_factor: float,
    output_pdf: Path,
    output_png: Path,
    input_mode: str,
) -> None:
    input_records = []
    for path in input_paths:
        stat_result = path.stat()
        input_records.append(
            {
                "path_relative_to_analysis_root": str(path.relative_to(analysis_root)),
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
        "selected_entries": selected_entries,
        "in_range_counts": {
            "unit05_time_no_cut": float(np.sum(time_counts)),
            "difference_no_cut": float(np.sum(difference_counts_all)),
            "difference_energy_cut_unscaled": float(np.sum(difference_counts_cut)),
        },
        "histograms": {
            "unit05_time": {
                "expression": "GammaTime[5]",
                "selection": "",
                "number_of_bins": TIME_NUMBER_OF_BINS,
                "range_ns": [TIME_MIN_NS, TIME_MAX_NS],
            },
            "neighbor_time_difference_no_cut": {
                "expression": "GammaTime[5] - GammaTime[6]",
                "selection": "",
                "number_of_bins": DIFFERENCE_NUMBER_OF_BINS,
                "range_ns": [DIFFERENCE_MIN_NS, DIFFERENCE_MAX_NS],
            },
            "neighbor_time_difference_energy_cut": {
                "expression": "GammaTime[5] - GammaTime[6]",
                "selection": "GammaEnergy[5] + GammaEnergy[6] >= 30 MeV",
                "number_of_bins": DIFFERENCE_NUMBER_OF_BINS,
                "range_ns": [DIFFERENCE_MIN_NS, DIFFERENCE_MAX_NS],
            },
        },
        "energy_selected_peak_scale_factor": peak_scale_factor,
        "outputs": {"pdf": str(output_pdf), "png": str(output_png)},
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
            histogram_time_5,
            histogram_difference_all,
            histogram_difference_cut,
            total_entries,
            selected_entries,
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
            histogram_time_5,
            histogram_difference_all,
            histogram_difference_cut,
            total_entries,
            selected_entries,
        ) = fill_historical_histograms(input_paths)
        input_mode = "legacy-event-tree-provenance"

    time_edges, time_counts = th1_to_numpy(histogram_time_5)
    difference_edges_all, difference_counts_all = th1_to_numpy(
        histogram_difference_all
    )
    difference_edges_cut, difference_counts_cut = th1_to_numpy(
        histogram_difference_cut
    )
    if not np.array_equal(difference_edges_all, difference_edges_cut):
        raise RuntimeError("The two time-difference histograms do not share bin edges.")
    if np.any(difference_counts_cut > difference_counts_all):
        raise RuntimeError(
            "An energy-selected time-difference bin exceeds its no-cut bin."
        )

    peak_scale_factor = positive_maximum(
        difference_counts_all, "no-cut time difference"
    ) / positive_maximum(difference_counts_cut, "selected time difference")

    output_root = arguments.output_dir.expanduser().resolve() / "unit_time_difference"
    output_stem = "cshine_gamma_unit05_time_and_neighbor_difference_horizontal"
    output_pdf = output_root / (output_stem + ".pdf")
    output_png = output_root / (output_stem + ".png")
    metadata_path = output_root / (output_stem + "_metadata.json")

    draw_horizontal_figure(
        time_edges,
        time_counts,
        difference_edges_all,
        difference_counts_all,
        difference_counts_cut,
        peak_scale_factor,
        output_pdf,
        output_png,
    )
    write_metadata(
        metadata_path,
        analysis_root,
        input_dir,
        input_paths,
        total_entries,
        selected_entries,
        time_counts,
        difference_counts_all,
        difference_counts_cut,
        peak_scale_factor,
        output_pdf,
        output_png,
        input_mode,
    )

    print("Energy-selected peak scale factor: %.12g" % peak_scale_factor)
    print("PDF saved to: %s" % output_pdf)
    print("PNG saved to: %s" % output_png)
    print("Metadata saved to: %s" % metadata_path)


if __name__ == "__main__":
    main()
