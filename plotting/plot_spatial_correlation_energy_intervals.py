#!/usr/bin/env python3
"""Redraw analysis-note Fig. 10 from its two historical ROOT objects.

The upstream ROOT analysis defines the spatial spreads ``delta_x`` and
``delta_y`` for each accepted reconstructed candidate and fills:

* ``ALL_h2_ax_ay_10_100`` for 10 <= E_tot <= 100 MeV;
* ``ALL_h2_ax_ay_100_inf`` for E_tot > 100 MeV.

Both histograms use 70 x 70 bins over 0--7 cm on each axis.  This program does
not reconstruct events, change selections, rebin, or normalize.  It only reads
the two source histograms and arranges them in a horizontal Matplotlib figure
with separate logarithmic count scales, matching the historical ROOT panels.
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
LOW_ENERGY_OBJECT = "ALL_h2_ax_ay_10_100"
HIGH_ENERGY_OBJECT = "ALL_h2_ax_ay_100_inf"
OUTPUT_STEM = "cshine_gamma_spatial_correlation_energy_intervals_horizontal"


def parse_arguments():
    parser = argparse.ArgumentParser(
        description=(
            "Draw delta_y versus delta_x for 10 <= E_tot <= 100 MeV and "
            "E_tot > 100 MeV from the historical h2_check ROOT output."
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


def require_historical_binning(histogram, object_name):
    x_axis = histogram.GetXaxis()
    y_axis = histogram.GetYaxis()
    matches = (
        histogram.GetNbinsX() == 70
        and histogram.GetNbinsY() == 70
        and np.isclose(x_axis.GetXmin(), 0.0)
        and np.isclose(x_axis.GetXmax(), 7.0)
        and np.isclose(y_axis.GetXmin(), 0.0)
        and np.isclose(y_axis.GetXmax(), 7.0)
    )
    if not matches:
        raise ValueError(
            "%s does not have the historical 70 x 70, [0, 7] cm binning."
            % object_name
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
    all_bins = histogram.Integral(0, x_bins + 1, 0, y_bins + 1)
    return {
        "entries": float(histogram.GetEntries()),
        "in_range_counts": float(np.sum(counts)),
        "including_underflow_overflow": float(all_bins),
        "maximum_bin_count": float(np.max(counts)),
        "nonzero_bins": int(np.count_nonzero(counts)),
    }


def read_source_histograms(input_path):
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
        result = []
        for object_name in (LOW_ENERGY_OBJECT, HIGH_ENERGY_OBJECT):
            histogram = root_file.Get(object_name)
            if not histogram:
                raise KeyError("Missing ROOT object: %s" % object_name)
            if not histogram.InheritsFrom("TH2"):
                raise TypeError("ROOT object %s is not a TH2." % object_name)
            require_historical_binning(histogram, object_name)
            x_edges, y_edges, counts = histogram_to_numpy(histogram)
            if np.any(counts < 0.0):
                raise ValueError("ROOT object %s contains negative bins." % object_name)
            if not np.any(counts > 0.0):
                raise ValueError("ROOT object %s is empty." % object_name)
            result.append(
                {
                    "object": object_name,
                    "x_edges": x_edges,
                    "y_edges": y_edges,
                    "counts": counts,
                    "summary": histogram_summary(histogram, counts),
                }
            )
        return result, str(ROOT.gROOT.GetVersion())
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


def draw_horizontal_figure(histograms, output_pdf, output_png):
    configure_plot_style()
    figure = plt.figure(figsize=(9.8, 4.25))
    grid = figure.add_gridspec(
        1,
        5,
        width_ratios=[1.0, 0.045, 0.20, 1.0, 0.045],
        left=0.075,
        right=0.985,
        bottom=0.16,
        top=0.96,
        wspace=0.12,
    )
    axes = [figure.add_subplot(grid[0, 0]), figure.add_subplot(grid[0, 3])]
    color_axes = [figure.add_subplot(grid[0, 1]), figure.add_subplot(grid[0, 4])]

    for index, (axis, color_axis, item) in enumerate(
        zip(axes, color_axes, histograms)
    ):
        counts = item["counts"]
        positive = counts[counts > 0.0]
        masked = np.ma.masked_less_equal(counts, 0.0)
        image = axis.pcolormesh(
            item["x_edges"],
            item["y_edges"],
            masked,
            cmap="viridis",
            norm=LogNorm(vmin=1.0, vmax=float(np.max(positive))),
            shading="flat",
            rasterized=True,
        )
        colorbar = figure.colorbar(image, cax=color_axis)
        colorbar.set_label("Counts")

        axis.set_xlim(0.0, 7.0)
        axis.set_ylim(0.0, 7.0)
        axis.set_xlabel(r"$\delta_x$ (cm)")
        axis.set_ylabel(r"$\delta_y$ (cm)")
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
            0.045,
            0.94,
            "(%s)" % ("a" if index == 0 else "b"),
            transform=axis.transAxes,
            ha="left",
            va="top",
            fontsize=12,
            fontweight="bold",
        )

    figure.savefig(str(output_pdf), bbox_inches="tight")
    figure.savefig(str(output_png), dpi=300, bbox_inches="tight")
    plt.close(figure)


def write_metadata(path, input_path, root_version, histograms, outputs):
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
            "low_energy_selection": "10 <= E_tot <= 100 MeV",
            "high_energy_selection": "E_tot > 100 MeV",
            "variables": ["delta_x [cm]", "delta_y [cm]"],
            "binning": "70 x 70 bins over [0, 7] cm on both axes",
            "normalization": "none",
            "color_scale": "independent logarithmic count scale for each panel",
        },
        "histograms": [
            {"object": item["object"], "summary": item["summary"]}
            for item in histograms
        ],
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
        args.output_dir.expanduser().resolve()
        / "spatial_correlation_energy_intervals"
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

    histograms, root_version = read_source_histograms(input_path)
    draw_horizontal_figure(histograms, output_pdf, output_png)
    outputs = [
        {
            "path": str(path),
            "size_bytes": path.stat().st_size,
            "sha256": sha256_file(path),
        }
        for path in (output_pdf, output_png)
    ]
    write_metadata(
        output_json,
        input_path,
        root_version,
        histograms,
        outputs,
    )

    print("PDF saved to: %s" % output_pdf)
    print("PNG saved to: %s" % output_png)
    print("Metadata saved to: %s" % output_json)


if __name__ == "__main__":
    main()
