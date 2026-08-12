#!/usr/bin/env python3
"""Redraw the slow-coincidence beam-off background subtraction.

This program preserves the numerical operations in the published central
``EnergySpecGen.C`` analysis:

* read ``h_total_E_M1`` from the beam-on and beam-off merged ROOT files;
* copy both inputs to double-precision histograms and rebin by five;
* normalize the beam-off histogram to the beam-on counts from the 110 MeV
  bin through the final 200 MeV bin;
* subtract the scaled beam-off spectrum from the beam-on spectrum;
* compare the result bin-by-bin with ``spectrum_110.root:histDiff``.

Only the presentation changes: the two historically separate ROOT panels are
placed side by side.  No event selection, bin content, error, normalization,
or subtraction rule is changed.
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


DEFAULT_INPUT_DIRECTORY = Path("DataPreprocessing/step4-convert.0308")
BEAM_ON_FILE = "all_recon.root"
BEAM_OFF_FILE = "all_recon_BKG.root"
REFERENCE_FILE = "spectrum_110.root"
INPUT_OBJECT = "h_total_E_M1"
REFERENCE_OBJECT = "histDiff"
NORMALIZATION_LOW_MEV = 110.0
REBIN_FACTOR = 5
OUTPUT_STEM = "cshine_gamma_slow_coincidence_background_subtraction_horizontal"


def parse_arguments():
    parser = argparse.ArgumentParser(
        description=(
            "Draw the beam-on, scaled beam-off, and background-subtracted "
            "slow-coincidence gamma-ray spectra."
        )
    )
    parser.add_argument(
        "--analysis-root",
        type=Path,
        required=True,
        help="Root of the authorized gamma2024 analysis directory.",
    )
    parser.add_argument(
        "--input-directory",
        type=Path,
        default=DEFAULT_INPUT_DIRECTORY,
        help=(
            "Directory containing the three ROOT files, relative to "
            "--analysis-root unless absolute."
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


def write_json_atomic(path, value):
    path = Path(path)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("w", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True, ensure_ascii=False)
        stream.write("\n")
    temporary.replace(path)


def resolve_input_directory(analysis_root, input_directory):
    input_directory = Path(input_directory).expanduser()
    if input_directory.is_absolute():
        return input_directory.resolve()
    return (Path(analysis_root).expanduser().resolve() / input_directory).resolve()


def open_root_file(ROOT, path):
    root_file = ROOT.TFile.Open(str(path), "READ")
    if not root_file or root_file.IsZombie():
        raise OSError("Cannot open ROOT input: %s" % path)
    return root_file


def require_histogram(root_file, object_name, path):
    histogram = root_file.Get(object_name)
    if not histogram:
        raise KeyError("Missing ROOT object %s in %s" % (object_name, path))
    if not histogram.InheritsFrom("TH1") or histogram.InheritsFrom("TH2"):
        raise TypeError("ROOT object %s in %s is not a one-dimensional TH1." % (
            object_name,
            path,
        ))
    return histogram


def require_source_schema(histogram, label):
    axis = histogram.GetXaxis()
    matches = (
        histogram.GetNbinsX() == 1000
        and np.isclose(axis.GetXmin(), 0.0)
        and np.isclose(axis.GetXmax(), 200.0)
    )
    if not matches:
        raise ValueError(
            "%s must have 1000 bins over 0--200 MeV; found %d bins over "
            "%g--%g MeV."
            % (
                label,
                histogram.GetNbinsX(),
                axis.GetXmin(),
                axis.GetXmax(),
            )
        )


def copy_as_double(ROOT, source, name):
    axis = source.GetXaxis()
    result = ROOT.TH1D(
        name,
        name,
        source.GetNbinsX(),
        axis.GetXmin(),
        axis.GetXmax(),
    )
    result.SetDirectory(0)
    result.Add(source)
    result.Sumw2()
    rebinned = result.Rebin(REBIN_FACTOR, name + "_rebin5")
    rebinned.SetDirectory(0)
    return rebinned


def histogram_arrays(histogram):
    axis = histogram.GetXaxis()
    edges = np.array(
        [axis.GetBinLowEdge(index) for index in range(1, histogram.GetNbinsX() + 2)],
        dtype=float,
    )
    centers = np.array(
        [axis.GetBinCenter(index) for index in range(1, histogram.GetNbinsX() + 1)],
        dtype=float,
    )
    values = np.array(
        [histogram.GetBinContent(index) for index in range(1, histogram.GetNbinsX() + 1)],
        dtype=float,
    )
    errors = np.array(
        [histogram.GetBinError(index) for index in range(1, histogram.GetNbinsX() + 1)],
        dtype=float,
    )
    return edges, centers, values, errors


def compare_histograms(candidate, reference, tolerance=1.0e-9):
    if candidate.GetNbinsX() != reference.GetNbinsX():
        raise ValueError(
            "Reference histogram has %d bins, candidate has %d."
            % (reference.GetNbinsX(), candidate.GetNbinsX())
        )
    candidate_axis = candidate.GetXaxis()
    reference_axis = reference.GetXaxis()
    if not (
        np.isclose(candidate_axis.GetXmin(), reference_axis.GetXmin())
        and np.isclose(candidate_axis.GetXmax(), reference_axis.GetXmax())
    ):
        raise ValueError("Candidate and reference histogram axes differ.")

    mismatched_contents = 0
    mismatched_errors = 0
    maximum_content_difference = 0.0
    maximum_error_difference = 0.0
    for index in range(0, candidate.GetNbinsX() + 2):
        content_difference = abs(
            candidate.GetBinContent(index) - reference.GetBinContent(index)
        )
        error_difference = abs(
            candidate.GetBinError(index) - reference.GetBinError(index)
        )
        maximum_content_difference = max(
            maximum_content_difference, content_difference
        )
        maximum_error_difference = max(maximum_error_difference, error_difference)
        if content_difference > tolerance:
            mismatched_contents += 1
        if error_difference > tolerance:
            mismatched_errors += 1

    return {
        "tolerance": tolerance,
        "mismatched_content_bins": mismatched_contents,
        "mismatched_error_bins": mismatched_errors,
        "maximum_absolute_content_difference": maximum_content_difference,
        "maximum_absolute_error_difference": maximum_error_difference,
    }


def histogram_summary(histogram):
    return {
        "bins": int(histogram.GetNbinsX()),
        "x_min_mev": float(histogram.GetXaxis().GetXmin()),
        "x_max_mev": float(histogram.GetXaxis().GetXmax()),
        "entries": float(histogram.GetEntries()),
        "in_range_integral": float(histogram.Integral(1, histogram.GetNbinsX())),
        "underflow": float(histogram.GetBinContent(0)),
        "overflow": float(histogram.GetBinContent(histogram.GetNbinsX() + 1)),
    }


def step_values(values):
    return np.append(values, values[-1])


def draw_figure(beam_on, beam_off_scaled, difference, output_pdf, output_png):
    on_edges, _, on_values, _ = histogram_arrays(beam_on)
    off_edges, _, off_values, _ = histogram_arrays(beam_off_scaled)
    _, diff_centers, diff_values, diff_errors = histogram_arrays(difference)

    plt.rcParams.update(
        {
            "font.family": "serif",
            "font.serif": ["DejaVu Serif"],
            "mathtext.fontset": "dejavuserif",
            "font.size": 10.5,
            "axes.labelsize": 12,
            "xtick.labelsize": 10,
            "ytick.labelsize": 10,
            "legend.fontsize": 10,
            "axes.linewidth": 1.1,
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
        }
    )

    figure, axes = plt.subplots(1, 2, figsize=(9.2, 3.85))
    figure.subplots_adjust(
        left=0.085,
        right=0.985,
        bottom=0.17,
        top=0.97,
        wspace=0.27,
    )

    axes[0].step(
        on_edges,
        step_values(on_values),
        where="post",
        color="#222222",
        linewidth=1.25,
        label="Beam on",
    )
    axes[0].step(
        off_edges,
        step_values(off_values),
        where="post",
        color="#D1495B",
        linewidth=1.25,
        label="Scaled beam off",
    )
    axes[0].set_xlim(0.0, 200.0)
    axes[0].set_ylim(5.0e-1, 1.0e8)
    axes[0].legend(loc="upper center", frameon=False, handlelength=2.5)

    positive = diff_values > 0.0
    axes[1].errorbar(
        diff_centers[positive],
        diff_values[positive],
        yerr=diff_errors[positive],
        fmt=".",
        markersize=3.0,
        color="#D1495B",
        ecolor="#D1495B",
        elinewidth=0.8,
        capsize=0,
    )
    axes[1].set_xlim(0.0, 100.0)
    axes[1].set_ylim(1.0e-1, 1.0e8)

    for index, axis in enumerate(axes):
        axis.set_yscale("log")
        axis.set_xlabel(r"$E_{\mathrm{tot}}$ (MeV)")
        axis.set_ylabel("Counts")
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


def main():
    args = parse_arguments()
    input_directory = resolve_input_directory(
        args.analysis_root, args.input_directory
    )
    beam_on_path = input_directory / BEAM_ON_FILE
    beam_off_path = input_directory / BEAM_OFF_FILE
    reference_path = input_directory / REFERENCE_FILE
    for path in (beam_on_path, beam_off_path, reference_path):
        if not path.is_file():
            raise FileNotFoundError("ROOT input not found: %s" % path)

    output_directory = (
        Path(args.output_dir).expanduser().resolve()
        / "slow_coincidence_background_subtraction"
    )
    output_directory.mkdir(parents=True, exist_ok=True)
    output_pdf = output_directory / (OUTPUT_STEM + ".pdf")
    output_png = output_directory / (OUTPUT_STEM + ".png")
    output_json = output_directory / (OUTPUT_STEM + ".json")
    existing = [path for path in (output_pdf, output_png, output_json) if path.exists()]
    if existing and not args.force:
        raise FileExistsError(
            "Output already exists; use --force to replace it: %s" % existing[0]
        )

    try:
        import ROOT
    except ImportError as error:
        raise RuntimeError("PyROOT is required to read the ROOT inputs.") from error

    ROOT.gROOT.SetBatch(True)
    ROOT.TH1.AddDirectory(False)
    beam_on_file = open_root_file(ROOT, beam_on_path)
    beam_off_file = open_root_file(ROOT, beam_off_path)
    reference_file = open_root_file(ROOT, reference_path)
    try:
        beam_on_source = require_histogram(
            beam_on_file, INPUT_OBJECT, beam_on_path
        )
        beam_off_source = require_histogram(
            beam_off_file, INPUT_OBJECT, beam_off_path
        )
        reference = require_histogram(
            reference_file, REFERENCE_OBJECT, reference_path
        )
        require_source_schema(beam_on_source, "beam-on histogram")
        require_source_schema(beam_off_source, "beam-off histogram")

        beam_on = copy_as_double(ROOT, beam_on_source, "beam_on")
        beam_off_scaled = copy_as_double(ROOT, beam_off_source, "beam_off")
        normalization_start_bin = beam_on.FindBin(NORMALIZATION_LOW_MEV)
        normalization_end_bin = beam_on.GetNbinsX()
        beam_on_normalization_counts = beam_on.Integral(
            normalization_start_bin, normalization_end_bin
        )
        beam_off_normalization_counts = beam_off_scaled.Integral(
            normalization_start_bin, normalization_end_bin
        )
        if beam_on_normalization_counts <= 0.0:
            raise ValueError("Beam-on normalization integral is not positive.")
        if beam_off_normalization_counts <= 0.0:
            raise ValueError("Beam-off normalization integral is not positive.")
        background_scale = (
            beam_on_normalization_counts / beam_off_normalization_counts
        )
        beam_off_scaled.Scale(background_scale)

        difference = beam_on.Clone("histDiff_candidate")
        difference.SetDirectory(0)
        difference.Add(beam_off_scaled, -1.0)
        comparison = compare_histograms(difference, reference)
        if (
            comparison["mismatched_content_bins"] != 0
            or comparison["mismatched_error_bins"] != 0
        ):
            raise ValueError(
                "Reconstructed histDiff does not match spectrum_110.root; "
                "see the reported maximum differences."
            )

        draw_figure(
            beam_on,
            beam_off_scaled,
            difference,
            output_pdf,
            output_png,
        )

        metadata = {
            "schema_version": 1,
            "created_utc": datetime.utcnow().replace(microsecond=0).isoformat() + "Z",
            "stable_id": "cshine-slow-background-subtraction-figure",
            "inputs": {
                "beam_on": {
                    "path": str(beam_on_path),
                    "object": INPUT_OBJECT,
                    "sha256": sha256_file(beam_on_path),
                },
                "beam_off": {
                    "path": str(beam_off_path),
                    "object": INPUT_OBJECT,
                    "sha256": sha256_file(beam_off_path),
                },
                "reference": {
                    "path": str(reference_path),
                    "object": REFERENCE_OBJECT,
                    "sha256": sha256_file(reference_path),
                },
            },
            "physics_contract": {
                "rebin_factor": REBIN_FACTOR,
                "rebinned_bin_width_mev": float(beam_on.GetBinWidth(1)),
                "normalization_low_mev": NORMALIZATION_LOW_MEV,
                "normalization_high_mev_exclusive": float(
                    beam_on.GetXaxis().GetXmax()
                ),
                "normalization_start_bin": int(normalization_start_bin),
                "normalization_end_bin": int(normalization_end_bin),
                "beam_on_normalization_counts": float(
                    beam_on_normalization_counts
                ),
                "beam_off_normalization_counts_before_scaling": float(
                    beam_off_normalization_counts
                ),
                "beam_off_scale": float(background_scale),
                "normalization": "scaled beam-off integral equals beam-on integral",
                "subtraction": "beam on minus scaled beam off",
            },
            "histograms": {
                "beam_on_rebinned": histogram_summary(beam_on),
                "beam_off_rebinned_scaled": histogram_summary(beam_off_scaled),
                "difference": histogram_summary(difference),
                "reference_histDiff": histogram_summary(reference),
            },
            "validation": comparison,
            "display": {
                "layout": "one row by two columns",
                "panel_a_x_mev": [0.0, 200.0],
                "panel_b_x_mev": [0.0, 100.0],
                "y_scale": "logarithmic",
                "normalization_shading": False,
            },
            "software": {
                "python": platform.python_version(),
                "root": str(ROOT.gROOT.GetVersion()),
                "numpy": np.__version__,
                "matplotlib": matplotlib.__version__,
            },
        }
        write_json_atomic(output_json, metadata)
    finally:
        beam_on_file.Close()
        beam_off_file.Close()
        reference_file.Close()

    print("PDF saved to: %s" % output_pdf)
    print("PNG saved to: %s" % output_png)
    print("Metadata saved to: %s" % output_json)
    print("Beam-off scale: %.12g" % background_scale)
    print("histDiff comparison: zero mismatched content and error bins")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
