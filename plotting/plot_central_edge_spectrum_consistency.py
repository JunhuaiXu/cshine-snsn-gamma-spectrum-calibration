#!/usr/bin/env python3
"""Redraw and validate the central/edge reconstructed-spectrum comparison.

The historical notebook reads the central, edge, and combined reconstructed
energy spectra from the March 8 beam-on and beam-off outputs.  Each pair is
rebinned from 0.2 to 1 MeV, the beam-off spectrum is scaled independently to
the corresponding beam-on spectrum in the ROOT ``FindBin(110)`` through
``FindBin(200)`` interval, and the scaled beam-off spectrum is subtracted.
For the shape comparison, all three differences are then scaled to the total
spectrum integral between ``FindBin(35)`` and ``FindBin(100)``.

PyROOT performs all histogram arithmetic so the historical ROOT bin-boundary
and uncertainty semantics are retained.  Matplotlib changes only the panel
arrangement from vertical to horizontal.  A focused ROOT output is required
and every content and uncertainty bin, including flow bins, is compared before
the candidate figure is written.
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


DEFAULT_INPUT_DIRECTORY = Path(
    "DataPreprocessing/step4-convert.0308.PreRun"
)
DEFAULT_COMPARISON_DIRECTORY = Path("DataPreprocessing/step4-convert.0308")
INPUT_FILES = ("all_recon.root", "all_recon_BKG.root")
OBJECT_NAMES = ("h_central_E_M1", "h_side_E_M1", "h_total_E_M1")
LABELS = ("Central", "Edge", "Total")
COLORS = ("#1f77b4", "#2ca02c", "#d62728")
REBIN_FACTOR = 5
COSMIC_RANGE_MEV = (110.0, 200.0)
SHAPE_RANGE_MEV = (35.0, 100.0)
OUTPUT_STEM = "cshine_gamma_central_edge_spectrum_consistency_horizontal"
REFERENCE_STEM = "cshine_gamma_central_edge_spectrum_consistency_root_reference"


def parse_arguments():
    parser = argparse.ArgumentParser(
        description=(
            "Redraw the central, edge, and combined background-subtracted "
            "spectra and validate them against a focused ROOT reference."
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
        help="Historical input directory relative to --analysis-root.",
    )
    parser.add_argument(
        "--comparison-directory",
        type=Path,
        default=DEFAULT_COMPARISON_DIRECTORY,
        help=(
            "Optional central-analysis directory compared object by object "
            "with the published-figure input directory."
        ),
    )
    parser.add_argument(
        "--reference-root",
        type=Path,
        required=True,
        help="Focused ROOT-reference file created by the companion macro.",
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
        help="Replace existing PDF, PNG, or metadata output.",
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
    temporary = Path(str(path) + ".tmp")
    with temporary.open("w", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True, ensure_ascii=False)
        stream.write("\n")
    temporary.replace(path)


def resolve_directory(analysis_root, directory):
    directory = Path(directory).expanduser()
    if directory.is_absolute():
        return directory.resolve()
    return (Path(analysis_root).expanduser().resolve() / directory).resolve()


def open_root_file(ROOT, path):
    root_file = ROOT.TFile.Open(str(path), "READ")
    if not root_file or root_file.IsZombie():
        raise OSError("Cannot open ROOT input: %s" % path)
    return root_file


def require_histogram(root_file, object_name, path, source_schema=True):
    histogram = root_file.Get(object_name)
    if not histogram:
        raise KeyError("Missing ROOT object %s in %s" % (object_name, path))
    if not histogram.InheritsFrom("TH1") or histogram.InheritsFrom("TH2"):
        raise TypeError("ROOT object %s is not a one-dimensional TH1." % object_name)
    if source_schema:
        axis = histogram.GetXaxis()
        if not (
            histogram.GetNbinsX() == 1000
            and np.isclose(axis.GetXmin(), 0.0)
            and np.isclose(axis.GetXmax(), 200.0)
        ):
            raise ValueError(
                "%s must have 1000 bins over 0--200 MeV." % object_name
            )
    return histogram


def copy_and_rebin(ROOT, source, name):
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


def subtract_background(ROOT, signal_source, background_source, name):
    signal = copy_and_rebin(ROOT, signal_source, name + "_beam_on")
    background = copy_and_rebin(ROOT, background_source, name + "_beam_off")
    first_bin = signal.FindBin(COSMIC_RANGE_MEV[0])
    last_bin = signal.FindBin(COSMIC_RANGE_MEV[1])
    signal_integral = signal.Integral(first_bin, last_bin)
    background_integral = background.Integral(first_bin, last_bin)
    if background_integral == 0.0:
        raise ZeroDivisionError("Beam-off cosmic-region integral is zero.")
    scale = signal_integral / background_integral
    background.Scale(scale)
    difference = signal.Clone(name + "_subtracted")
    difference.SetDirectory(0)
    difference.Add(background, -1.0)
    return difference, {
        "first_bin": int(first_bin),
        "last_bin": int(last_bin),
        "beam_on_integral": float(signal_integral),
        "beam_off_integral": float(background_integral),
        "beam_off_scale": float(scale),
    }


def normalize_to_total(source, total_integral, name):
    result = source.Clone(name + "_normalized_to_total")
    result.SetDirectory(0)
    first_bin = result.FindBin(SHAPE_RANGE_MEV[0])
    last_bin = result.FindBin(SHAPE_RANGE_MEV[1])
    integral = result.Integral(first_bin, last_bin)
    if integral == 0.0:
        raise ZeroDivisionError("Subtracted-spectrum shape integral is zero.")
    scale = total_integral / integral
    result.Scale(scale)
    return result, {
        "first_bin": int(first_bin),
        "last_bin": int(last_bin),
        "integral_before_scaling": float(integral),
        "scale_to_total": float(scale),
    }


def compare_histograms(candidate, reference, tolerance=1.0e-9):
    if candidate.GetNbinsX() != reference.GetNbinsX():
        raise ValueError("Candidate and reference bin counts differ.")
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


def compare_input_versions(ROOT, published_directory, comparison_directory):
    result = {
        "comparison_directory_exists": comparison_directory.is_dir(),
        "files": {},
        "all_objects_identical": True,
    }
    if not comparison_directory.is_dir():
        result["all_objects_identical"] = False
        return result
    for file_name in INPUT_FILES:
        published_path = published_directory / file_name
        comparison_path = comparison_directory / file_name
        file_record = {
            "published_sha256": sha256_file(published_path),
            "comparison_exists": comparison_path.is_file(),
            "objects": {},
        }
        if not comparison_path.is_file():
            result["all_objects_identical"] = False
            result["files"][file_name] = file_record
            continue
        file_record["comparison_sha256"] = sha256_file(comparison_path)
        published_file = open_root_file(ROOT, published_path)
        comparison_file = open_root_file(ROOT, comparison_path)
        for object_name in OBJECT_NAMES:
            comparison = compare_histograms(
                require_histogram(published_file, object_name, published_path),
                require_histogram(comparison_file, object_name, comparison_path),
                tolerance=0.0,
            )
            file_record["objects"][object_name] = comparison
            if (
                comparison["mismatched_content_bins"] != 0
                or comparison["mismatched_error_bins"] != 0
            ):
                result["all_objects_identical"] = False
        published_file.Close()
        comparison_file.Close()
        result["files"][file_name] = file_record
    return result


def histogram_arrays(histogram):
    axis = histogram.GetXaxis()
    edges = np.array(
        [axis.GetBinLowEdge(index) for index in range(1, histogram.GetNbinsX() + 2)],
        dtype=float,
    )
    values = np.array(
        [histogram.GetBinContent(index) for index in range(1, histogram.GetNbinsX() + 1)],
        dtype=float,
    )
    return edges, values


def histogram_summary(histogram):
    return {
        "bins": int(histogram.GetNbinsX()),
        "x_min_mev": float(histogram.GetXaxis().GetXmin()),
        "x_max_mev": float(histogram.GetXaxis().GetXmax()),
        "entries": float(histogram.GetEntries()),
        "regular_integral": float(histogram.Integral(1, histogram.GetNbinsX())),
        "underflow": float(histogram.GetBinContent(0)),
        "overflow": float(histogram.GetBinContent(histogram.GetNbinsX() + 1)),
    }


def positive_limits(histograms):
    values = []
    for histogram in histograms:
        _, current = histogram_arrays(histogram)
        values.extend(current[current > 0.0])
    if not values:
        return 1.0e-1, 1.0
    values = np.asarray(values, dtype=float)
    lower = max(1.0e-1, float(np.min(values)) * 0.7)
    upper = float(np.max(values)) * 4.0
    return lower, upper


def draw_figure(subtracted, normalized, output_pdf, output_png):
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
    figure, axes = plt.subplots(1, 2, figsize=(8.6, 3.75))
    figure.subplots_adjust(
        left=0.085,
        right=0.985,
        bottom=0.17,
        top=0.97,
        wspace=0.25,
    )

    for axis, spectra, y_label, panel in (
        (axes[0], subtracted, "Counts", "(a)"),
        (axes[1], normalized, "Counts normalized to total", "(b)"),
    ):
        for histogram, label, color in zip(spectra, LABELS, COLORS):
            edges, values = histogram_arrays(histogram)
            axis.step(
                edges,
                np.append(values, values[-1]),
                where="post",
                color=color,
                linewidth=1.7,
                label=label,
            )
        axis.set_xlim(0.0, 100.0)
        axis.set_yscale("log")
        axis.set_ylim(*positive_limits(spectra))
        axis.set_xlabel(r"$E_{\mathrm{tot}}$ (MeV)")
        axis.set_ylabel(y_label)
        axis.minorticks_on()
        axis.tick_params(
            which="major", direction="in", top=True, right=True, length=5, width=1.0
        )
        axis.tick_params(
            which="minor", direction="in", top=True, right=True, length=2.5, width=0.8
        )
        axis.legend(loc="upper right", frameon=False)
        axis.text(
            0.04,
            0.94,
            panel,
            transform=axis.transAxes,
            ha="left",
            va="top",
            fontsize=12,
            fontweight="bold",
        )
    figure.savefig(output_pdf, bbox_inches="tight")
    figure.savefig(output_png, dpi=300, bbox_inches="tight")
    plt.close(figure)


def main():
    args = parse_arguments()
    analysis_root = args.analysis_root.expanduser().resolve()
    input_directory = resolve_directory(analysis_root, args.input_directory)
    comparison_directory = resolve_directory(
        analysis_root, args.comparison_directory
    )
    output_directory = (
        args.output_dir.expanduser().resolve()
        / "central_edge_spectrum_consistency"
    )
    output_directory.mkdir(parents=True, exist_ok=True)
    output_pdf = output_directory / (OUTPUT_STEM + ".pdf")
    output_png = output_directory / (OUTPUT_STEM + ".png")
    output_json = output_directory / (OUTPUT_STEM + ".json")
    for path in (output_pdf, output_png, output_json):
        if path.exists() and not args.force:
            raise FileExistsError("Output exists; pass --force: %s" % path)

    import ROOT

    ROOT.gROOT.SetBatch(True)
    signal_path = input_directory / INPUT_FILES[0]
    background_path = input_directory / INPUT_FILES[1]
    reference_path = args.reference_root.expanduser().resolve()
    signal_file = open_root_file(ROOT, signal_path)
    background_file = open_root_file(ROOT, background_path)
    reference_file = open_root_file(ROOT, reference_path)

    subtracted = []
    normalized = []
    normalization_records = {}
    validation_records = {}
    total_integral = None
    for object_name, label in zip(OBJECT_NAMES, LABELS):
        difference, background_record = subtract_background(
            ROOT,
            require_histogram(signal_file, object_name, signal_path),
            require_histogram(background_file, object_name, background_path),
            object_name,
        )
        subtracted.append(difference)
        normalization_records[label] = {"background": background_record}

    total_first_bin = subtracted[2].FindBin(SHAPE_RANGE_MEV[0])
    total_last_bin = subtracted[2].FindBin(SHAPE_RANGE_MEV[1])
    total_integral = subtracted[2].Integral(total_first_bin, total_last_bin)
    for object_name, label, difference in zip(OBJECT_NAMES, LABELS, subtracted):
        shape, shape_record = normalize_to_total(
            difference, total_integral, object_name
        )
        normalized.append(shape)
        normalization_records[label]["shape"] = shape_record

        subtracted_reference = require_histogram(
            reference_file,
            object_name + "_subtracted",
            reference_path,
            source_schema=False,
        )
        normalized_reference = require_histogram(
            reference_file,
            object_name + "_normalized_to_total",
            reference_path,
            source_schema=False,
        )
        validation_records[label] = {
            "subtracted": compare_histograms(difference, subtracted_reference),
            "normalized": compare_histograms(shape, normalized_reference),
        }

    for label in LABELS:
        for stage in ("subtracted", "normalized"):
            comparison = validation_records[label][stage]
            if (
                comparison["mismatched_content_bins"] != 0
                or comparison["mismatched_error_bins"] != 0
            ):
                raise RuntimeError(
                    "%s %s spectrum differs from ROOT reference." % (label, stage)
                )

    draw_figure(subtracted, normalized, output_pdf, output_png)

    input_version_comparison = compare_input_versions(
        ROOT, input_directory, comparison_directory
    )
    metadata = {
        "created_utc": datetime.utcnow().replace(microsecond=0).isoformat() + "Z",
        "python": platform.python_version(),
        "root": str(ROOT.gROOT.GetVersion()),
        "matplotlib": matplotlib.__version__,
        "numpy": np.__version__,
        "historical_input_directory": str(args.input_directory),
        "comparison_directory": str(args.comparison_directory),
        "input_files": {
            path.name: {
                "size_bytes": path.stat().st_size,
                "sha256": sha256_file(path),
            }
            for path in (signal_path, background_path)
        },
        "input_version_comparison": input_version_comparison,
        "objects": list(OBJECT_NAMES),
        "rebin_factor": REBIN_FACTOR,
        "cosmic_normalization_range_mev": list(COSMIC_RANGE_MEV),
        "shape_normalization_range_mev": list(SHAPE_RANGE_MEV),
        "root_findbin_policy": (
            "Inclusive bin indices returned by ROOT FindBin are preserved, "
            "including the overflow bin at the 200 MeV upper axis edge."
        ),
        "normalization": normalization_records,
        "histograms": {
            label: {
                "subtracted": histogram_summary(difference),
                "normalized": histogram_summary(shape),
            }
            for label, difference, shape in zip(LABELS, subtracted, normalized)
        },
        "reference_root": {
            "size_bytes": reference_path.stat().st_size,
            "sha256": sha256_file(reference_path),
        },
        "root_reference_validation": validation_records,
        "outputs": {
            "pdf": {"size_bytes": output_pdf.stat().st_size, "sha256": sha256_file(output_pdf)},
            "png": {"size_bytes": output_png.stat().st_size, "sha256": sha256_file(output_png)},
        },
    }
    write_json_atomic(output_json, metadata)
    print("PDF saved to: %s" % output_pdf)
    print("PNG saved to: %s" % output_png)
    print("Metadata saved to: %s" % output_json)
    print("Published and central input objects identical: %s" % input_version_comparison["all_objects_identical"])


if __name__ == "__main__":
    main()
