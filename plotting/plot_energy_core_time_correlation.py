#!/usr/bin/env python3
"""Redraw the reconstructed-energy--core-time correlation.

The historical analysis fills ``ALL_h2_TOF_TotalE`` after gamma-cluster
reconstruction.  This program does not reconstruct events, alter the core or
veto selections, rebin, normalize, or fit the distribution.  It reads the
frozen ROOT object and produces a single publication-quality panel together
with machine-readable run metadata.
"""

from __future__ import print_function

import argparse
import hashlib
import json
import platform
from datetime import datetime
from pathlib import Path

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
from matplotlib.colors import LogNorm
import numpy as np


DEFAULT_INPUT = Path("DataPreprocessing/step7-DeltaYrelated/h2_check.root")
OBJECT_NAME = "ALL_h2_TOF_TotalE"
OUTPUT_STEM = "cshine_gamma_energy_core_time_correlation"

X_BINS = 100
X_MIN = -500.0
X_MAX = 500.0
Y_BINS = 200
Y_MIN = 0.0
Y_MAX = 200.0


def parse_arguments():
    parser = argparse.ArgumentParser(
        description=(
            "Draw reconstructed total energy versus corrected core time "
            "from the historical h2_check ROOT output."
        )
    )
    parser.add_argument(
        "--analysis-root",
        type=Path,
        required=True,
        help="Root of the authorized gamma2024 analysis directory.",
    )
    parser.add_argument(
        "--input-root",
        type=Path,
        default=DEFAULT_INPUT,
        help=(
            "ROOT input relative to --analysis-root unless absolute "
            "(default: %s)." % DEFAULT_INPUT
        ),
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("results/figures"),
        help="Directory below which the figure and metadata are written.",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Replace an existing PDF, PNG, or metadata file.",
    )
    return parser.parse_args()


def sha256_file(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def resolve_input(analysis_root, input_root):
    input_root = Path(input_root).expanduser()
    if input_root.is_absolute():
        return input_root.resolve()
    return (Path(analysis_root).expanduser().resolve() / input_root).resolve()


def axis_edges(axis):
    return np.array(
        [axis.GetBinLowEdge(index) for index in range(1, axis.GetNbins() + 2)],
        dtype=float,
    )


def require_published_binning(histogram):
    x_axis = histogram.GetXaxis()
    y_axis = histogram.GetYaxis()
    matches = (
        histogram.GetNbinsX() == X_BINS
        and histogram.GetNbinsY() == Y_BINS
        and np.isclose(x_axis.GetXmin(), X_MIN)
        and np.isclose(x_axis.GetXmax(), X_MAX)
        and np.isclose(y_axis.GetXmin(), Y_MIN)
        and np.isclose(y_axis.GetXmax(), Y_MAX)
    )
    if not matches:
        raise ValueError(
            "%s does not have the published 100 x 200 "
            "T_core--E_tot binning." % OBJECT_NAME
        )


def histogram_to_numpy(histogram):
    x_edges = axis_edges(histogram.GetXaxis())
    y_edges = axis_edges(histogram.GetYaxis())
    counts = np.empty((histogram.GetNbinsY(), histogram.GetNbinsX()), dtype=float)
    for y_index in range(1, histogram.GetNbinsY() + 1):
        for x_index in range(1, histogram.GetNbinsX() + 1):
            counts[y_index - 1, x_index - 1] = histogram.GetBinContent(
                x_index, y_index
            )
    return x_edges, y_edges, counts


def histogram_summary(histogram, counts):
    x_bins = histogram.GetNbinsX()
    y_bins = histogram.GetNbinsY()
    total_with_flow = histogram.Integral(0, x_bins + 1, 0, y_bins + 1)
    in_range = float(np.sum(counts))
    return {
        "entries": float(histogram.GetEntries()),
        "in_range_counts": in_range,
        "including_underflow_overflow": float(total_with_flow),
        "outside_displayed_range": float(total_with_flow - in_range),
        "x_underflow": float(histogram.Integral(0, 0, 0, y_bins + 1)),
        "x_overflow": float(
            histogram.Integral(x_bins + 1, x_bins + 1, 0, y_bins + 1)
        ),
        "y_underflow_with_x_in_range": float(
            histogram.Integral(1, x_bins, 0, 0)
        ),
        "y_overflow_with_x_in_range": float(
            histogram.Integral(1, x_bins, y_bins + 1, y_bins + 1)
        ),
        "maximum_bin_count": float(np.max(counts)),
        "nonzero_bins": int(np.count_nonzero(counts)),
    }


def read_source_histogram(input_path):
    try:
        import ROOT
    except ImportError as error:
        raise RuntimeError(
            "PyROOT is required to read the historical ROOT output."
        ) from error

    root_file = ROOT.TFile.Open(str(input_path), "READ")
    if not root_file or root_file.IsZombie():
        raise OSError("Cannot open ROOT input: %s" % input_path)

    try:
        histogram = root_file.Get(OBJECT_NAME)
        if not histogram:
            raise KeyError("Missing ROOT object: %s" % OBJECT_NAME)
        if not histogram.InheritsFrom("TH2"):
            raise TypeError("ROOT object %s is not a TH2." % OBJECT_NAME)
        require_published_binning(histogram)
        x_edges, y_edges, counts = histogram_to_numpy(histogram)
        if np.any(counts < 0.0):
            raise ValueError("ROOT object %s contains negative bins." % OBJECT_NAME)
        if not np.any(counts > 0.0):
            raise ValueError("ROOT object %s is empty." % OBJECT_NAME)
        return {
            "object": OBJECT_NAME,
            "x_edges": x_edges,
            "y_edges": y_edges,
            "counts": counts,
            "summary": histogram_summary(histogram, counts),
        }, str(ROOT.gROOT.GetVersion())
    finally:
        root_file.Close()


def configure_plot_style():
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


def draw_figure(histogram, output_pdf, output_png):
    configure_plot_style()
    figure = plt.figure(figsize=(6.5, 4.8))
    grid = figure.add_gridspec(
        1,
        2,
        width_ratios=[1.0, 0.045],
        left=0.13,
        right=0.94,
        bottom=0.15,
        top=0.97,
        wspace=0.08,
    )
    axis = figure.add_subplot(grid[0, 0])
    color_axis = figure.add_subplot(grid[0, 1])

    counts = histogram["counts"]
    masked = np.ma.masked_less_equal(counts, 0.0)
    image = axis.pcolormesh(
        histogram["x_edges"],
        histogram["y_edges"],
        masked,
        cmap="viridis",
        norm=LogNorm(vmin=1.0, vmax=float(np.max(counts))),
        shading="flat",
        rasterized=True,
    )
    colorbar = figure.colorbar(image, cax=color_axis)
    colorbar.set_label("Counts")

    axis.set_xlim(X_MIN, X_MAX)
    axis.set_ylim(Y_MIN, Y_MAX)
    axis.set_xlabel(r"$T_{\mathrm{core}}$ (ns)")
    axis.set_ylabel(r"$E_{\mathrm{tot}}$ (MeV)")
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

    figure.savefig(str(output_pdf), bbox_inches="tight")
    figure.savefig(str(output_png), dpi=300, bbox_inches="tight")
    plt.close(figure)


def write_metadata(path, input_path, root_version, histogram, outputs):
    metadata = {
        "created_utc": datetime.utcnow().replace(microsecond=0).isoformat() + "Z",
        "input": {
            "path": str(input_path),
            "size_bytes": input_path.stat().st_size,
            "modified_utc": datetime.utcfromtimestamp(
                input_path.stat().st_mtime
            ).replace(microsecond=0).isoformat()
            + "Z",
            "sha256": sha256_file(input_path),
        },
        "physics_contract": {
            "sample": (
                "accepted reconstructed candidates with central cores 5, 6, "
                "9, 10 or side cores 4, 7, 8, 11, 13, 14; side cores require "
                "count_veto == 0"
            ),
            "variables": [
                "T_core = corrected time of the reconstructed core crystal [ns]",
                "E_tot = reconstructed cluster energy [MeV]",
            ],
            "binning": (
                "100 x 200 bins over T_core=-500--500 ns and E_tot=0--200 MeV"
            ),
            "normalization": "none",
            "color_scale": "logarithmic count scale",
            "underflow_overflow": "recorded in metadata and not drawn",
        },
        "histogram": {
            "object": histogram["object"],
            "summary": histogram["summary"],
        },
        "software": {
            "python": platform.python_version(),
            "numpy": np.__version__,
            "matplotlib": matplotlib.__version__,
            "root": root_version,
        },
        "outputs": outputs,
    }
    with path.open("w", encoding="utf-8") as stream:
        json.dump(metadata, stream, indent=2, sort_keys=True)
        stream.write("\n")


def main():
    args = parse_arguments()
    input_path = resolve_input(args.analysis_root, args.input_root)
    if not input_path.is_file():
        raise FileNotFoundError("ROOT input does not exist: %s" % input_path)

    output_directory = (
        args.output_dir.expanduser().resolve() / "energy_core_time_correlation"
    )
    output_directory.mkdir(parents=True, exist_ok=True)
    output_pdf = output_directory / (OUTPUT_STEM + ".pdf")
    output_png = output_directory / (OUTPUT_STEM + ".png")
    output_json = output_directory / (OUTPUT_STEM + ".json")

    existing = [path for path in (output_pdf, output_png, output_json) if path.exists()]
    if existing and not args.force:
        raise FileExistsError(
            "Output already exists; use --force to replace: %s"
            % ", ".join(str(path) for path in existing)
        )

    histogram, root_version = read_source_histogram(input_path)
    draw_figure(histogram, output_pdf, output_png)
    outputs = [
        {
            "path": str(path),
            "size_bytes": path.stat().st_size,
            "sha256": sha256_file(path),
        }
        for path in (output_pdf, output_png)
    ]
    write_metadata(output_json, input_path, root_version, histogram, outputs)

    print("PDF saved to: %s" % output_pdf)
    print("PNG saved to: %s" % output_png)
    print("Metadata saved to: %s" % output_json)


if __name__ == "__main__":
    main()
