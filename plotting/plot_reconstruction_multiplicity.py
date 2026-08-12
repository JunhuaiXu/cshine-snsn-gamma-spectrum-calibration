#!/usr/bin/env python3
"""Draw the two analysis-note Fig. 8 panels for all 15 CsI(Tl) units.

The input is the focused real-event validation output. All 15 CsI(Tl) units
are admitted as possible reconstructed cores and no plastic-veto requirement
is applied. Panel (a) shows reconstructed cluster size versus reconstructed
total energy. Panel (b) compares the number of reconstructed cores per trigger
before and after counting only candidates with ``E_tot > 35 MeV``.

The historical drawing macro supplied the high-energy multiplicity curve as
fixed bin contents (6731 and 19). Those values are retained only in metadata;
the plotted high-energy curve is read from the validated all-crystal,
no-veto ROOT histogram and has 7218 and 21 entries in its first two bins.
"""

import argparse
import hashlib
import json
import platform
from datetime import datetime, timezone
from pathlib import Path
from typing import Tuple

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm
import numpy as np
import ROOT


DEFAULT_INPUT = Path(
    "reproducible/validation/core_multiplicity/"
    "core_multiplicity_all15_noveto.root"
)
CLUSTER_OBJECT = "all15_cluster_size_vs_total_energy"
CORE_COUNT_OBJECT = "all15_core_multiplicity"
HIGH_ENERGY_OBJECT = "all15_high_core_multiplicity"

# Values drawn by the historical Drawhistos.C macro. They are recorded only as
# a provenance comparison and are not plotted by the current all-crystal mode.
HISTORICAL_HIGH_ENERGY_COUNTS = np.array(
    [6731.0, 19.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0],
    dtype=float,
)


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Draw cluster size versus reconstructed energy and the number of "
            "reconstructed cores per trigger for all 15 CsI(Tl) units without "
            "a plastic-veto requirement."
        )
    )
    parser.add_argument(
        "--analysis-root",
        type=Path,
        required=True,
        help="Root of the gamma2024 analysis directory.",
    )
    parser.add_argument(
        "--input-root",
        type=Path,
        default=DEFAULT_INPUT,
        help=(
            "ROOT input relative to --analysis-root "
            "(default: %s)." % DEFAULT_INPUT
        ),
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("results/figures"),
        help="Directory below which the figure and metadata are written.",
    )
    return parser.parse_args()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def require_histogram(root_file: ROOT.TFile, name: str, dimension: int):
    histogram = root_file.Get(name)
    if not histogram:
        raise KeyError("ROOT object %s is absent from %s" % (name, root_file.GetName()))
    if dimension == 1 and not histogram.InheritsFrom("TH1"):
        raise TypeError("ROOT object %s is not a TH1 histogram." % name)
    if dimension == 2 and not histogram.InheritsFrom("TH2"):
        raise TypeError("ROOT object %s is not a TH2 histogram." % name)
    return histogram


def axis_edges(axis) -> np.ndarray:
    return np.array(
        [axis.GetBinLowEdge(index) for index in range(1, axis.GetNbins() + 2)],
        dtype=float,
    )


def th1_to_numpy(histogram) -> Tuple[np.ndarray, np.ndarray]:
    edges = axis_edges(histogram.GetXaxis())
    counts = np.array(
        [
            histogram.GetBinContent(index)
            for index in range(1, histogram.GetNbinsX() + 1)
        ],
        dtype=float,
    )
    return edges, counts


def th2_to_numpy(histogram) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    x_edges = axis_edges(histogram.GetXaxis())
    y_edges = axis_edges(histogram.GetYaxis())
    counts = np.empty((histogram.GetNbinsY(), histogram.GetNbinsX()), dtype=float)
    for y_index in range(1, histogram.GetNbinsY() + 1):
        for x_index in range(1, histogram.GetNbinsX() + 1):
            counts[y_index - 1, x_index - 1] = histogram.GetBinContent(
                x_index, y_index
            )
    return x_edges, y_edges, counts


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


def draw_horizontal_figure(
    cluster_x_edges: np.ndarray,
    cluster_y_edges: np.ndarray,
    cluster_counts: np.ndarray,
    core_edges: np.ndarray,
    all_core_counts: np.ndarray,
    high_energy_counts: np.ndarray,
    output_pdf: Path,
    output_png: Path,
) -> None:
    positive_cluster_counts = cluster_counts[cluster_counts > 0.0]
    if positive_cluster_counts.size == 0:
        raise ValueError("The cluster-size histogram has no positive in-range bins.")
    if np.max(all_core_counts) <= 0.0:
        raise ValueError("The all-trigger N_core histogram is empty.")

    configure_plot_style()
    figure = plt.figure(figsize=(10.3, 4.0))
    grid = figure.add_gridspec(
        1,
        4,
        width_ratios=[1.0, 0.045, 0.24, 1.0],
        left=0.075,
        right=0.985,
        bottom=0.16,
        top=0.96,
        wspace=0.10,
    )
    axis_cluster = figure.add_subplot(grid[0, 0])
    color_axis = figure.add_subplot(grid[0, 1])
    axis_core = figure.add_subplot(grid[0, 3])

    masked_cluster_counts = np.ma.masked_less_equal(cluster_counts, 0.0)
    image = axis_cluster.pcolormesh(
        cluster_x_edges,
        cluster_y_edges,
        masked_cluster_counts,
        cmap="viridis",
        norm=LogNorm(vmin=1.0, vmax=float(np.max(positive_cluster_counts))),
        shading="flat",
        rasterized=True,
    )
    colorbar = figure.colorbar(image, cax=color_axis)
    colorbar.set_label("Counts")

    axis_cluster.set_xlim(cluster_x_edges[0], cluster_x_edges[-1])
    axis_cluster.set_ylim(cluster_y_edges[0], cluster_y_edges[-1])
    axis_cluster.set_xlabel(r"$E_{\mathrm{tot}}$ (MeV)")
    axis_cluster.set_ylabel("Cluster size")
    axis_cluster.set_yticks(np.arange(1, 10, dtype=int))

    all_step = np.append(all_core_counts, all_core_counts[-1])
    high_step = np.append(high_energy_counts, high_energy_counts[-1])
    axis_core.step(
        core_edges,
        all_step,
        where="post",
        color="#4477AA",
        linewidth=1.8,
        label="All",
    )
    axis_core.step(
        core_edges,
        high_step,
        where="post",
        color="#CC6677",
        linewidth=1.8,
        label=r"$E_{\mathrm{tot}}>35\,\mathrm{MeV}$",
    )
    axis_core.set_yscale("log")
    axis_core.set_xlim(core_edges[0], core_edges[-1])
    axis_core.set_ylim(0.8, 2.0 * float(np.max(all_core_counts)))
    axis_core.set_xticks(np.arange(1, 10, dtype=int))
    axis_core.set_xlabel(r"$N_{\mathrm{core}}$")
    axis_core.set_ylabel("Counts")
    axis_core.legend(loc="upper right", frameon=False)

    for label, axis in (("(a)", axis_cluster), ("(b)", axis_core)):
        axis.text(
            0.04,
            0.94,
            label,
            transform=axis.transAxes,
            ha="left",
            va="top",
            fontsize=12,
            fontweight="bold",
        )
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

    output_pdf.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(str(output_pdf), bbox_inches="tight")
    figure.savefig(str(output_png), dpi=300, bbox_inches="tight")
    plt.close(figure)


def main() -> None:
    arguments = parse_arguments()
    input_root = (
        arguments.input_root
        if arguments.input_root.is_absolute()
        else arguments.analysis_root / arguments.input_root
    )
    if not input_root.is_file():
        raise FileNotFoundError("Validated ROOT output is missing: %s" % input_root)

    root_file = ROOT.TFile.Open(str(input_root), "READ")
    if not root_file or root_file.IsZombie():
        raise OSError("Cannot open ROOT input: %s" % input_root)

    try:
        cluster_histogram = require_histogram(root_file, CLUSTER_OBJECT, 2)
        core_histogram = require_histogram(root_file, CORE_COUNT_OBJECT, 1)
        high_energy_histogram = require_histogram(
            root_file, HIGH_ENERGY_OBJECT, 1
        )
        cluster_x_edges, cluster_y_edges, cluster_counts = th2_to_numpy(
            cluster_histogram
        )
        core_edges, all_core_counts = th1_to_numpy(core_histogram)
        high_energy_edges, high_energy_counts = th1_to_numpy(
            high_energy_histogram
        )
    finally:
        root_file.Close()

    if len(all_core_counts) != 9:
        raise ValueError(
            "%s must contain nine N_core bins; found %d."
            % (CORE_COUNT_OBJECT, len(all_core_counts))
        )
    if not np.array_equal(core_edges, high_energy_edges):
        raise ValueError(
            "%s and %s use different N_core bin edges."
            % (CORE_COUNT_OBJECT, HIGH_ENERGY_OBJECT)
        )
    if np.any(high_energy_counts > all_core_counts):
        raise ValueError(
            "An energy-selected N_core count exceeds the corresponding all-trigger count."
        )

    if high_energy_counts[0] != 7218.0 or high_energy_counts[1] != 21.0:
        raise ValueError(
            "%s does not reproduce the validated N_core=1,2 counts 7218/21: %s"
            % (HIGH_ENERGY_OBJECT, high_energy_counts.astype(int).tolist())
        )

    figure_directory = arguments.output_dir / "reconstruction_multiplicity"
    output_pdf = (
        figure_directory
        / "cshine_gamma_cluster_size_and_core_multiplicity_horizontal.pdf"
    )
    output_png = output_pdf.with_suffix(".png")
    metadata_path = figure_directory / "run_metadata.json"

    draw_horizontal_figure(
        cluster_x_edges,
        cluster_y_edges,
        cluster_counts,
        core_edges,
        all_core_counts,
        high_energy_counts,
        output_pdf,
        output_png,
    )

    metadata = {
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "input": {
            "path": str(input_root.resolve()),
            "size_bytes": input_root.stat().st_size,
            "sha256": sha256_file(input_root),
        },
        "root_objects": {
            "cluster_size": CLUSTER_OBJECT,
            "all_core_multiplicity": CORE_COUNT_OBJECT,
            "high_energy_multiplicity": HIGH_ENERGY_OBJECT,
        },
        "selection": {
            "threshold_mev": 35.0,
            "core_scope": "All 15 CsI(Tl) units",
            "veto_requirement": "None",
            "high_energy_definition": (
                "Count reconstructed candidates with E_tot > 35 MeV within "
                "each trigger."
            ),
        },
        "counts": {
            "all_triggers": all_core_counts.astype(int).tolist(),
            "high_energy_plotted": high_energy_counts.astype(int).tolist(),
            "historical_macro_comparison": (
                HISTORICAL_HIGH_ENERGY_COUNTS.astype(int).tolist()
            ),
        },
        "software": {
            "python": platform.python_version(),
            "root": str(ROOT.gROOT.GetVersion()),
            "numpy": np.__version__,
            "matplotlib": matplotlib.__version__,
        },
        "outputs": {
            "pdf": str(output_pdf.resolve()),
            "png": str(output_png.resolve()),
        },
    }
    metadata_path.write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )

    print("PDF saved to: %s" % output_pdf.resolve())
    print("PNG saved to: %s" % output_png.resolve())
    print("Metadata saved to: %s" % metadata_path.resolve())


if __name__ == "__main__":
    main()
