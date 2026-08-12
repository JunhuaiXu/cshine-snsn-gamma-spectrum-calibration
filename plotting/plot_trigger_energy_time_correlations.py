#!/usr/bin/env python3
"""Redraw reconstructed-energy--core-time correlations by trigger condition.

Analysis-note Fig. 15 contains five physical panels.  Each panel reads the
same ``ALL_h2_TOF_TotalE`` object name from a different historical trigger
branch.  The upstream branches differ only in the monitored-trigger TDC
element used to retain events.  This program reads those frozen TH2 objects
and changes only their presentation: it does not reconstruct events, alter
the trigger/core/veto selections, rebin, normalize, smooth, or fit counts.
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
import matplotlib.patheffects as path_effects
import numpy as np


OUTPUT_STEM = "cshine_gamma_trigger_energy_time_correlations"
OBJECT_NAME = "ALL_h2_TOF_TotalE"

X_BINS = 100
X_MIN = -500.0
X_MAX = 500.0
Y_BINS = 200
Y_MIN = 0.0
Y_MAX = 200.0

# Analysis-note Fig. 15 has five panels.  The historical source used three
# rows by two columns; the thesis-oriented presentation uses two rows by
# three columns and leaves its sixth pad empty.  ALL_OR is shown separately
# in Fig. 13 and is not an additional trigger-conditioned panel here.
PANELS = (
    {
        "panel": "(a)",
        "branch": "step5-onlygamma",
        "trigger_index": 17,
        "trigger": "SSD M1 & CsI M1",
    },
    {
        "panel": "(b)",
        "branch": "step5-SSDM2",
        "trigger_index": 18,
        "trigger": "SSD M2",
    },
    {
        "panel": "(c)",
        "branch": "step5-NAandSSD",
        "trigger_index": 19,
        "trigger": "SSD M1 & NA M1",
    },
    {
        "panel": "(d)",
        "branch": "step5-T0andNA",
        "trigger_index": 20,
        "trigger": r"NA M1 & $T_0$",
    },
    {
        "panel": "(e)",
        "branch": "step5-T0LS",
        "trigger_index": 22,
        "trigger": r"LS & $T_0$",
    },
)


def parse_arguments():
    parser = argparse.ArgumentParser(
        description=(
            "Draw the five trigger-conditioned reconstructed-energy--"
            "core-time panels used in analysis-note Fig. 15."
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
        help="M10B trigger_diagnostics.root; overrides the five historical files.",
    )
    parser.add_argument(
        "--selection-policy",
        choices=("historical", "reviewed"),
        default="historical",
        help="Object policy when --portable-root is used (default: historical).",
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


def input_path(analysis_root, branch):
    return (
        Path(analysis_root).expanduser().resolve()
        / "DataPreprocessing"
        / "step8-TimeCheck"
        / branch
        / "step7-DeltaYrelated"
        / "h2_check.root"
    )


def axis_edges(axis):
    return np.array(
        [axis.GetBinLowEdge(index) for index in range(1, axis.GetNbins() + 2)],
        dtype=float,
    )


def require_published_binning(histogram, source_path, object_name):
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
            "%s:%s does not have the published 100 x 200 "
            "T_core--E_tot binning." % (source_path, object_name)
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
    total_with_flow = float(
        histogram.Integral(0, x_bins + 1, 0, y_bins + 1)
    )
    in_range = float(np.sum(counts))
    return {
        "entries": float(histogram.GetEntries()),
        "in_range_counts": in_range,
        "including_underflow_overflow": total_with_flow,
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


def read_source_histograms(analysis_root, portable_root=None,
                           selection_policy="historical"):
    try:
        import ROOT
    except ImportError as error:
        raise RuntimeError(
            "PyROOT is required to read the historical ROOT outputs."
        ) from error

    histograms = []
    for panel in PANELS:
        if portable_root is not None:
            source_path = Path(portable_root).expanduser().resolve()
            object_name = "%s_h2_TOF_TotalE_Trig%d" % (
                selection_policy, panel["trigger_index"]
            )
        else:
            if analysis_root is None:
                raise ValueError("Provide --analysis-root or --portable-root.")
            source_path = input_path(analysis_root, panel["branch"])
            object_name = OBJECT_NAME
        if not source_path.is_file():
            raise FileNotFoundError("ROOT input does not exist: %s" % source_path)

        root_file = ROOT.TFile.Open(str(source_path), "READ")
        if not root_file or root_file.IsZombie():
            raise OSError("Cannot open ROOT input: %s" % source_path)

        try:
            histogram = root_file.Get(object_name)
            if not histogram:
                raise KeyError("Missing ROOT object: %s:%s" % (source_path, object_name))
            if not histogram.InheritsFrom("TH2"):
                raise TypeError("ROOT object %s:%s is not a TH2." % (source_path, object_name))
            require_published_binning(histogram, source_path, object_name)
            x_edges, y_edges, counts = histogram_to_numpy(histogram)
            if np.any(counts < 0.0):
                raise ValueError("ROOT object %s contains negative bins." % source_path)
            if not np.any(counts > 0.0):
                raise ValueError("ROOT object %s is empty." % source_path)

            item = dict(panel)
            item.update(
                {
                    "object": object_name,
                    "path": source_path,
                    "x_edges": x_edges,
                    "y_edges": y_edges,
                    "counts": counts,
                    "summary": histogram_summary(histogram, counts),
                }
            )
            histograms.append(item)
        finally:
            root_file.Close()

    return histograms, str(ROOT.gROOT.GetVersion())


def configure_plot_style():
    plt.rcParams.update(
        {
            "font.family": "serif",
            "font.serif": ["DejaVu Serif"],
            "mathtext.fontset": "dejavuserif",
            "font.size": 9.5,
            "axes.labelsize": 11.5,
            "xtick.labelsize": 9.0,
            "ytick.labelsize": 9.0,
            "axes.linewidth": 1.05,
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
        }
    )


def draw_figure(histograms, output_pdf, output_png):
    configure_plot_style()
    figure = plt.figure(figsize=(9.0, 5.9))
    outer = figure.add_gridspec(
        2,
        3,
        left=0.070,
        right=0.990,
        bottom=0.105,
        top=0.985,
        hspace=0.20,
        wspace=0.20,
    )

    for index, histogram in enumerate(histograms):
        row, column = divmod(index, 3)
        panel_grid = outer[row, column].subgridspec(
            1, 2, width_ratios=[1.0, 0.042], wspace=0.050
        )
        axis = figure.add_subplot(panel_grid[0, 0])
        color_axis = figure.add_subplot(panel_grid[0, 1])

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
        colorbar.ax.tick_params(labelsize=8.0)

        axis.set_xlim(X_MIN, X_MAX)
        axis.set_ylim(Y_MIN, Y_MAX)
        axis.tick_params(labelleft=(column == 0))
        axis.minorticks_on()
        axis.tick_params(
            which="major",
            direction="in",
            top=True,
            right=True,
            length=4.5,
            width=0.95,
        )
        axis.tick_params(
            which="minor",
            direction="in",
            top=True,
            right=True,
            length=2.3,
            width=0.75,
        )

        # The author requested the trigger mode in the upper-left corner.
        # A white outline preserves readability without covering the counts
        # with an opaque annotation box.
        annotation = axis.text(
            0.035,
            0.965,
            "%s %s" % (histogram["panel"], histogram["trigger"]),
            transform=axis.transAxes,
            ha="left",
            va="top",
            fontsize=8.5,
            fontweight="bold",
            color="black",
        )
        annotation.set_path_effects(
            [path_effects.withStroke(linewidth=2.5, foreground="white")]
        )

    # Fig. 15 contains five trigger-conditioned panels.  Keep the sixth pad
    # empty rather than introducing the ALL_OR distribution from Fig. 13.
    empty_axis = figure.add_subplot(outer[1, 2])
    empty_axis.axis("off")

    figure.text(
        0.50,
        0.025,
        r"$T_{\mathrm{core}}$ (ns)",
        ha="center",
        va="center",
        fontsize=11.5,
    )
    figure.text(
        0.012,
        0.54,
        r"$E_{\mathrm{tot}}$ (MeV)",
        ha="center",
        va="center",
        rotation="vertical",
        fontsize=11.5,
    )

    figure.savefig(str(output_pdf), bbox_inches="tight")
    figure.savefig(str(output_png), dpi=300, bbox_inches="tight")
    plt.close(figure)


def sample_definition(selection_policy):
    if selection_policy == "historical":
        return (
            "accepted reconstructed candidates with central cores 5, 6, 9, "
            "10 requiring count_veto == 0 and side cores 4, 7, 8, 11, 13, "
            "14 without a veto requirement; this reproduces the frozen "
            "analysis-note Fig. 15 implementation"
        )
    return (
        "accepted reconstructed candidates with central cores 5, 6, 9, 10 "
        "without a veto requirement and side cores 4, 7, 8, 11, 13, 14 "
        "requiring count_veto == 0; this is the author-reviewed main-analysis "
        "definition"
    )


def write_metadata(path, root_version, histograms, outputs, selection_policy):
    inputs = []
    histogram_records = []
    for histogram in histograms:
        source_path = histogram["path"]
        inputs.append(
            {
                "panel": histogram["panel"],
                "branch": histogram["branch"],
                "trigger": histogram["trigger"],
                "trigger_index": histogram["trigger_index"],
                "path": str(source_path),
                "size_bytes": source_path.stat().st_size,
                "modified_utc": datetime.utcfromtimestamp(
                    source_path.stat().st_mtime
                ).replace(microsecond=0).isoformat()
                + "Z",
                "sha256": sha256_file(source_path),
            }
        )
        histogram_records.append(
            {
                "panel": histogram["panel"],
                "branch": histogram["branch"],
                "trigger": histogram["trigger"],
                "trigger_index": histogram["trigger_index"],
                "object": histogram["object"],
                "summary": histogram["summary"],
            }
        )

    metadata = {
        "created_utc": datetime.utcnow().replace(microsecond=0).isoformat() + "Z",
        "inputs": inputs,
        "physics_contract": {
            "trigger_condition": (
                "each branch retains events with 100 <= "
                "TDC_Gamma_Trig_array[index] <= 4000 for its recorded "
                "trigger monitor; this is a valid-monitor condition, not "
                "an exclusive self-trigger-peak selection"
            ),
            "selection_policy": selection_policy,
            "sample": sample_definition(selection_policy),
            "variables": [
                "T_core = corrected time of the reconstructed core crystal [ns]",
                "E_tot = reconstructed cluster energy [MeV]",
            ],
            "binning": (
                "each panel has 100 x 200 bins over T_core=-500--500 ns "
                "and E_tot=0--200 MeV"
            ),
            "normalization": "none",
            "color_scale": "independent logarithmic count scale in each panel",
            "underflow_overflow": "recorded in metadata and not drawn",
            "panel_layout": "five panels in two rows and three columns; lower-right pad empty",
        },
        "histograms": histogram_records,
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
    output_directory = (
        args.output_dir.expanduser().resolve()
        / "trigger_energy_time_correlations"
    )
    output_directory.mkdir(parents=True, exist_ok=True)
    output_pdf = output_directory / (OUTPUT_STEM + ".pdf")
    output_png = output_directory / (OUTPUT_STEM + ".png")
    output_json = output_directory / (OUTPUT_STEM + ".json")

    existing = [
        path for path in (output_pdf, output_png, output_json) if path.exists()
    ]
    if existing and not args.force:
        raise FileExistsError(
            "Output already exists; use --force to replace: %s"
            % ", ".join(str(path) for path in existing)
        )

    histograms, root_version = read_source_histograms(
        args.analysis_root, args.portable_root, args.selection_policy
    )
    draw_figure(histograms, output_pdf, output_png)
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
        root_version,
        histograms,
        outputs,
        args.selection_policy,
    )

    print("PDF saved to: %s" % output_pdf)
    print("PNG saved to: %s" % output_png)
    print("Metadata saved to: %s" % output_json)


if __name__ == "__main__":
    main()
