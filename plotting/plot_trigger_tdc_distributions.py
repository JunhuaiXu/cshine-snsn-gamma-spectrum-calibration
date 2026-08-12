#!/usr/bin/env python3
"""Redraw the monitored-trigger TDC-channel distributions.

The historical program allocates ``h1_TrigList0``--``14`` by reusing a
15-element container, while the trigger-monitoring drawing reads only its
first seven objects.  The analysis-note figure uses six of those seven in the
fixed order documented below; channel 5 is omitted because the independent
``SSD M1 & LS`` monitoring signal was not recorded.  This program only reads
the frozen TH1 objects and changes the presentation layer.  It does not
refill, rebin, normalize, smooth, or fit any distribution.
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
import numpy as np


DEFAULT_INPUT = Path(
    "DataPreprocessing/step8-TimeCheck/step6-TimeWalkPlot/"
    "step7-DeltaYrelated/h2_check.root"
)
OUTPUT_STEM = "cshine_gamma_trigger_tdc_distributions"

X_BINS = 4096
X_MIN = 0.0
X_MAX = 4096.0
Y_MIN = 1.0
Y_MAX = 1.0e8
LABEL_REGION_MAXIMUM = 1.0e6

# The order is fixed by analysis-note Fig. 14 and the PRC supplement.
# ``histogram_index`` i corresponds to TDC_Gamma_Trig_list[i + 16].
PANELS = (
    {
        "panel": "(a)",
        "histogram_index": 1,
        "object": "h1_TrigList1",
        "trigger": "SSD M1 & CsI M1",
        "label_x_min": 1700.0,
    },
    {
        "panel": "(b)",
        "histogram_index": 2,
        "object": "h1_TrigList2",
        "trigger": "SSD M2",
        "label_x_min": 2800.0,
    },
    {
        "panel": "(c)",
        "histogram_index": 3,
        "object": "h1_TrigList3",
        "trigger": "SSD M1 & NA M1",
        "label_x_min": 1700.0,
    },
    {
        "panel": "(d)",
        "histogram_index": 4,
        "object": "h1_TrigList4",
        "trigger": r"NA M1 & $T_0$",
        "label_x_min": 2300.0,
    },
    {
        "panel": "(e)",
        "histogram_index": 6,
        "object": "h1_TrigList6",
        "trigger": r"LS & $T_0$",
        "label_x_min": 2800.0,
    },
    {
        "panel": "(f)",
        "histogram_index": 0,
        "object": "h1_TrigList0",
        "trigger": "ALL_OR (global trigger)",
        "label_x_min": 1000.0,
    },
)


def parse_arguments():
    parser = argparse.ArgumentParser(
        description=(
            "Draw the six monitored-trigger TDC spectra used in "
            "analysis-note Fig. 14."
        )
    )
    parser.add_argument(
        "--analysis-root",
        type=Path,
        help="Root of the authorized gamma2024 analysis directory.",
    )
    parser.add_argument(
        "--portable-root",
        type=Path,
        help="M10B trigger_diagnostics.root; overrides the historical input.",
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


def resolve_input(analysis_root, input_root, portable_root=None):
    if portable_root is not None:
        return Path(portable_root).expanduser().resolve()
    if analysis_root is None:
        raise ValueError("Provide --analysis-root or --portable-root.")
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
    axis = histogram.GetXaxis()
    matches = (
        histogram.GetNbinsX() == X_BINS
        and np.isclose(axis.GetXmin(), X_MIN)
        and np.isclose(axis.GetXmax(), X_MAX)
    )
    if not matches:
        raise ValueError(
            "%s does not have the historical 4096-bin, 0--4096 TDC axis."
            % object_name
        )


def histogram_summary(histogram, counts):
    x_bins = histogram.GetNbinsX()
    in_range = float(np.sum(counts))
    total_with_flow = float(histogram.Integral(0, x_bins + 1))
    return {
        "entries": float(histogram.GetEntries()),
        "in_range_counts": in_range,
        "including_underflow_overflow": total_with_flow,
        "outside_displayed_range": float(total_with_flow - in_range),
        "underflow": float(histogram.GetBinContent(0)),
        "overflow": float(histogram.GetBinContent(x_bins + 1)),
        "maximum_bin_count": float(np.max(counts)),
        "nonzero_bins": int(np.count_nonzero(counts)),
        "maximum_bin_center": float(histogram.GetBinCenter(histogram.GetMaximumBin())),
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
        for panel in PANELS:
            histogram = root_file.Get(panel["object"])
            if not histogram:
                raise KeyError("Missing ROOT object: %s" % panel["object"])
            if not histogram.InheritsFrom("TH1") or histogram.InheritsFrom("TH2"):
                raise TypeError("ROOT object %s is not a TH1." % panel["object"])
            require_historical_binning(histogram, panel["object"])
            edges = axis_edges(histogram.GetXaxis())
            counts = np.array(
                [
                    histogram.GetBinContent(index)
                    for index in range(1, histogram.GetNbinsX() + 1)
                ],
                dtype=float,
            )
            if np.any(counts < 0.0):
                raise ValueError("ROOT object %s contains negative bins." % panel["object"])
            if not np.any(counts > 0.0):
                raise ValueError("ROOT object %s is empty." % panel["object"])
            centers = 0.5 * (edges[:-1] + edges[1:])
            label_region = counts[centers >= panel["label_x_min"]]
            label_region_maximum = float(np.max(label_region))
            if label_region_maximum > LABEL_REGION_MAXIMUM:
                raise ValueError(
                    "The predefined upper-right label region is no longer "
                    "empty for %s (maximum count %.0f > %.0f). Review the "
                    "annotation position instead of covering the spectrum."
                    % (
                        panel["object"],
                        label_region_maximum,
                        LABEL_REGION_MAXIMUM,
                    )
                )
            result.append(
                {
                    "panel": panel["panel"],
                    "histogram_index": panel["histogram_index"],
                    "object": panel["object"],
                    "trigger": panel["trigger"],
                    "label_x_min": panel["label_x_min"],
                    "label_region_maximum": label_region_maximum,
                    "edges": edges,
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
            "font.size": 10.0,
            "axes.labelsize": 12,
            "xtick.labelsize": 9.5,
            "ytick.labelsize": 9.5,
            "axes.linewidth": 1.1,
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
        }
    )


def draw_figure(histograms, output_pdf, output_png):
    configure_plot_style()
    figure, axes = plt.subplots(
        2,
        3,
        figsize=(8.8, 5.6),
        sharex=True,
        sharey=True,
    )
    figure.subplots_adjust(
        left=0.085,
        right=0.99,
        bottom=0.12,
        top=0.985,
        hspace=0.12,
        wspace=0.08,
    )

    for plot_index, (axis, histogram) in enumerate(
        zip(axes.flat, histograms)
    ):
        step_counts = np.append(histogram["counts"], histogram["counts"][-1])
        axis.step(
            histogram["edges"],
            step_counts,
            where="post",
            color="black",
            linewidth=0.85,
        )
        axis.set_xlim(X_MIN, X_MAX)
        axis.set_ylim(Y_MIN, Y_MAX)
        axis.set_yscale("log")
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

        # Panel letters stay at upper left.  All six trigger names are placed
        # at upper right, which is blank in the frozen distributions; this
        # keeps the annotations away from the self-trigger peaks at low TDC.
        axis.text(
            0.05,
            0.92,
            histogram["panel"],
            transform=axis.transAxes,
            ha="left",
            va="top",
            fontsize=12,
            fontweight="bold",
        )
        axis.text(
            0.96,
            0.88,
            histogram["trigger"],
            transform=axis.transAxes,
            ha="right",
            va="top",
            fontsize=10.2,
            fontweight="bold",
        )

        row = plot_index // 3
        column = plot_index % 3
        if column == 0:
            axis.set_ylabel("Counts")
        if row == 1:
            axis.set_xlabel("TDC channel")

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
            "sample": (
                "fixed March 4--10 GammaCaliData sample used by the "
                "historical trigger-monitoring analysis"
            ),
            "source_branch": "TDC_Gamma_Trig_list",
            "channel_mapping": [
                {
                    "panel": histogram["panel"],
                    "trigger": histogram["trigger"],
                    "root_object": histogram["object"],
                    "histogram_index": histogram["histogram_index"],
                    "source_array_index": histogram["histogram_index"] + 16,
                }
                for histogram in histograms
            ],
            "selection": "100 < TDC_Gamma_Trig_list[index] < 4000",
            "binning": "4096 bins over TDC channel 0--4096",
            "normalization": "none",
            "display": (
                "two rows by three columns; shared logarithmic y range "
                "1--1e8; trigger names fixed at the data-free upper-right "
                "region of each panel"
            ),
            "underflow_overflow": "recorded in metadata and not drawn",
        },
        "histograms": [
            {
                "panel": histogram["panel"],
                "trigger": histogram["trigger"],
                "object": histogram["object"],
                "summary": histogram["summary"],
                "label_region": {
                    "x_min": histogram["label_x_min"],
                    "maximum_bin_count": histogram["label_region_maximum"],
                    "allowed_maximum_bin_count": LABEL_REGION_MAXIMUM,
                },
            }
            for histogram in histograms
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
    input_path = resolve_input(
        args.analysis_root, args.input_root, args.portable_root
    )
    if not input_path.is_file():
        raise FileNotFoundError("ROOT input does not exist: %s" % input_path)

    output_directory = (
        args.output_dir.expanduser().resolve() / "trigger_tdc_distributions"
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
    draw_figure(histograms, output_pdf, output_png)
    outputs = [
        {
            "path": str(output),
            "size_bytes": output.stat().st_size,
            "sha256": sha256_file(output),
        }
        for output in (output_pdf, output_png)
    ]
    write_metadata(output_json, input_path, root_version, histograms, outputs)

    print("PDF saved to: %s" % output_pdf)
    print("PNG saved to: %s" % output_png)
    print("Metadata saved to: %s" % output_json)


if __name__ == "__main__":
    main()
