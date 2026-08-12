#!/usr/bin/env python3
"""Plot per-crystal spectra before and after shower reconstruction.

The input is the M9 per-crystal ROOT output.  It contains the three historical
histogram families produced upstream by M8 and summed over the exact run list
by M9.  This script performs presentation only: it does not repeat event
calibration, shower reconstruction, veto selection, or run merging.
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


CRYSTAL_COUNT = 15
CENTRAL_CRYSTALS = (5, 6, 9, 10)
EDGE_CRYSTALS = (4, 7, 8, 11, 13, 14)
DETECTOR_REBIN = 10
RECONSTRUCTED_REBIN = 5
OUTPUT_STEM = "cshine_gamma_spectra_before_after_reconstruction"


def parse_arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--per-crystal-root",
        type=Path,
        required=True,
        help="M9 per-crystal ROOT file containing h_eDep_*, h_recon_*, and h_recon_veto_*.",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("results/figures/reconstruction_spectra"),
    )
    parser.add_argument("--force", action="store_true")
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


def write_json_atomic(path, value):
    temporary = Path(str(path) + ".tmp")
    with temporary.open("w", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True, ensure_ascii=False)
        stream.write("\n")
    temporary.replace(path)


def open_root_file(ROOT, path):
    root_file = ROOT.TFile.Open(str(path), "READ")
    if not root_file or root_file.IsZombie():
        raise OSError("Cannot open ROOT input: %s" % path)
    return root_file


def require_histogram(root_file, name, bins, x_max):
    histogram = root_file.Get(name)
    if not histogram:
        raise KeyError("Missing ROOT object %s" % name)
    if not histogram.InheritsFrom("TH1") or histogram.InheritsFrom("TH2"):
        raise TypeError("ROOT object %s is not a one-dimensional TH1." % name)
    axis = histogram.GetXaxis()
    if not (
        histogram.GetNbinsX() == bins
        and np.isclose(axis.GetXmin(), 0.0)
        and np.isclose(axis.GetXmax(), x_max)
    ):
        raise ValueError(
            "Unexpected schema for %s; expected %d bins over 0--%g MeV."
            % (name, bins, x_max)
        )
    return histogram


def rebin_arrays(histogram, factor):
    bins = histogram.GetNbinsX()
    if bins % factor != 0:
        raise ValueError("Histogram bin count is not divisible by rebin factor.")
    values = np.array(
        [histogram.GetBinContent(index) for index in range(1, bins + 1)],
        dtype=float,
    )
    values = values.reshape((-1, factor)).sum(axis=1)
    axis = histogram.GetXaxis()
    edges = np.array(
        [axis.GetBinLowEdge(index) for index in range(1, bins + 2, factor)],
        dtype=float,
    )
    if len(edges) == len(values):
        edges = np.append(edges, axis.GetXmax())
    return edges, values


def step_xy(edges, values):
    return edges, np.append(values, values[-1])


def panel_index(crystal):
    historical_panel = 16 - crystal if crystal != 12 else 1
    return historical_panel - 1


def draw_figure(detector, reconstructed, output_pdf, output_png):
    plt.rcParams.update(
        {
            "font.family": "DejaVu Sans",
            "axes.unicode_minus": False,
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
        }
    )
    colors = ("#0173B2", "#D55E00")
    fig, axes = plt.subplots(4, 4, figsize=(16.0, 12.0))
    axes = axes.ravel()

    for crystal in range(CRYSTAL_COUNT):
        axis = axes[panel_index(crystal)]
        edges, values = detector[crystal]
        x, y = step_xy(edges, values)
        axis.step(x, y, where="post", color=colors[0], linewidth=1.2, label="Individual")

        if crystal in reconstructed:
            edges, values = reconstructed[crystal]
            x, y = step_xy(edges, values)
            axis.step(
                x,
                y,
                where="post",
                color=colors[1],
                linewidth=1.2,
                label="Reconstructed",
            )
            axis.legend(loc="upper right", frameon=False, fontsize=7.5)

        axis.set_yscale("log")
        axis.set_xlim(0.0, 100.0)
        axis.set_ylim(1.0, 2.0e6)
        axis.tick_params(which="both", direction="in", top=True, right=True, labelsize=8)

    axes[3].axis("off")
    for row in range(4):
        for column in range(4):
            axis = axes[row * 4 + column]
            if not axis.axison:
                continue
            if row == 3:
                axis.set_xlabel(r"$E$ (MeV)")
            else:
                axis.tick_params(labelbottom=False)
            if column == 0:
                axis.set_ylabel("Counts")
            else:
                axis.tick_params(labelleft=False)

    fig.subplots_adjust(left=0.08, right=0.99, bottom=0.08, top=0.99, wspace=0.08, hspace=0.08)
    fig.savefig(str(output_pdf), bbox_inches="tight")
    fig.savefig(str(output_png), dpi=300, bbox_inches="tight")
    plt.close(fig)


def main():
    args = parse_arguments()
    input_path = args.per_crystal_root.expanduser().resolve()
    if not input_path.is_file():
        raise OSError("Input ROOT file does not exist: %s" % input_path)

    output_directory = args.output_dir.expanduser().resolve()
    output_directory.mkdir(parents=True, exist_ok=True)
    output_pdf = output_directory / (OUTPUT_STEM + ".pdf")
    output_png = output_directory / (OUTPUT_STEM + ".png")
    output_json = output_directory / (OUTPUT_STEM + ".json")
    for output in (output_pdf, output_png, output_json):
        if output.exists() and not args.force:
            raise RuntimeError("Output already exists; use --force intentionally: %s" % output)

    try:
        import ROOT
    except ImportError as error:
        raise RuntimeError("PyROOT is required to read the M9 ROOT output.") from error
    ROOT.gROOT.SetBatch(True)

    root_file = open_root_file(ROOT, input_path)
    detector = {}
    reconstructed = {}
    object_records = {}
    for crystal in range(CRYSTAL_COUNT):
        detector_name = "h_eDep_%d" % crystal
        detector_histogram = require_histogram(root_file, detector_name, 1000, 100.0)
        detector[crystal] = rebin_arrays(detector_histogram, DETECTOR_REBIN)
        object_records[detector_name] = {
            "entries": float(detector_histogram.GetEntries()),
            "regular_integral": float(detector_histogram.Integral(1, 1000)),
            "rebin_factor": DETECTOR_REBIN,
        }

        reconstructed_name = None
        if crystal in CENTRAL_CRYSTALS:
            reconstructed_name = "h_recon_%d" % crystal
        elif crystal in EDGE_CRYSTALS:
            reconstructed_name = "h_recon_veto_%d" % crystal
        if reconstructed_name is not None:
            reconstructed_histogram = require_histogram(
                root_file, reconstructed_name, 1000, 200.0
            )
            reconstructed[crystal] = rebin_arrays(
                reconstructed_histogram, RECONSTRUCTED_REBIN
            )
            object_records[reconstructed_name] = {
                "entries": float(reconstructed_histogram.GetEntries()),
                "regular_integral": float(reconstructed_histogram.Integral(1, 1000)),
                "rebin_factor": RECONSTRUCTED_REBIN,
            }
    root_file.Close()

    draw_figure(detector, reconstructed, output_pdf, output_png)
    metadata = {
        "schema_version": 1,
        "created_utc": datetime.utcnow().replace(microsecond=0).isoformat() + "Z",
        "input": {
            "path": str(input_path),
            "sha256": sha256_file(input_path),
            "objects": object_records,
        },
        "physics_contract": {
            "central_crystals": list(CENTRAL_CRYSTALS),
            "central_object_family": "h_recon_*",
            "edge_crystals": list(EDGE_CRYSTALS),
            "edge_object_family": "h_recon_veto_*",
            "detector_object_family": "h_eDep_*",
            "detector_rebin_factor": DETECTOR_REBIN,
            "reconstructed_rebin_factor": RECONSTRUCTED_REBIN,
            "normalization": "none",
        },
        "environment": {
            "python": platform.python_version(),
            "root": str(ROOT.gROOT.GetVersion()),
            "numpy": np.__version__,
            "matplotlib": matplotlib.__version__,
        },
        "outputs": {
            "pdf": {"path": str(output_pdf), "sha256": sha256_file(output_pdf)},
            "png": {"path": str(output_png), "sha256": sha256_file(output_png)},
        },
    }
    write_json_atomic(output_json, metadata)
    print("PDF saved to: %s" % output_pdf)
    print("PNG saved to: %s" % output_png)
    print("Metadata saved to: %s" % output_json)


if __name__ == "__main__":
    main()
