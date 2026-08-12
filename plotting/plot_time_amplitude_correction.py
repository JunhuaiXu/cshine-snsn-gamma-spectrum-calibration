#!/usr/bin/env python3
"""Reproduce the CSHINE-Gamma time-amplitude correction figures.

The numerical and rendering operations are a direct port of the historical
``DrawRebin.ipynb`` cells. Single-channel output places the before/after
panels next to each other; batch output reuses the same calculation for all
15 channels and writes validation records alongside the figures.  All panels
generated in one run use a common logarithmic count scale so that colors can
be compared directly between crystals and between the uncorrected and
corrected distributions.
"""

import argparse
import csv
import hashlib
import json
import platform
import re
from datetime import datetime, timezone
from pathlib import Path
from typing import NamedTuple, Sequence, Tuple

import matplotlib

matplotlib.use("Agg")

import matplotlib.pyplot as plt
import numpy as np
import ROOT
from matplotlib.colors import LogNorm


class FitParameters(NamedTuple):
    """Parameters of C0 / (E0 - ADC-E) + T0."""

    c0: float
    e0: float
    t0: float


class ChannelResult(NamedTuple):
    """Inputs and derived histograms for one CsI(Tl) channel."""

    crystal_index: int
    root_path: Path
    parameter_path: Path
    root_sha256: str
    parameter_sha256: str
    parameters: FitParameters
    rebin_factor: int
    input_y_bins: int
    input_x_bins: int
    input_count_sum: float
    raw_counts: np.ndarray
    raw_x_edges: np.ndarray
    raw_y_edges: np.ndarray
    corrected_counts: np.ndarray
    corrected_x_edges: np.ndarray
    corrected_y_edges: np.ndarray


CRYSTAL_INDICES = tuple(range(15))
COUNT_TOLERANCE = 1.0e-9

# Keep the 4 x 4 channel arrangement used by DrawRebin.ipynb.  ``None`` is
# the unused cell in the historical overview.
OVERVIEW_GRID = (
    (12, 14, 13, None),
    (11, 10, 9, 8),
    (7, 6, 5, 4),
    (3, 2, 1, 0),
)

_OVERVIEW_CHANNELS = tuple(
    channel
    for row in OVERVIEW_GRID
    for channel in row
    if channel is not None
)
if tuple(sorted(_OVERVIEW_CHANNELS)) != CRYSTAL_INDICES:
    raise RuntimeError("OVERVIEW_GRID must contain CsI00 through CsI14 exactly once")


FLOAT_PATTERN = r"[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[Ee][+-]?\d+)?"


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Generate the horizontal before/after time-walk correction figure "
            "from the historical ROOT histogram and fit parameters."
        )
    )
    parser.add_argument(
        "--analysis-root",
        type=Path,
        required=True,
        help="Root of the gamma2024 analysis directory.",
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        "--crystal-index",
        type=int,
        default=5,
        help="Index used by f_XX.root and f_XX.out (default: 5).",
    )
    mode.add_argument(
        "--all-crystals",
        action="store_true",
        help="Process CsI00 through CsI14 and generate overview figures.",
    )
    parser.add_argument(
        "--rebin-factor",
        type=int,
        default=16,
        help="Number of adjacent bins summed along each axis (default: 16).",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=Path("results/figures"),
        help=(
            "Directory for generated PDF and PNG files "
            "(default: results/figures)."
        ),
    )
    arguments = parser.parse_args()
    if not arguments.all_crystals and arguments.crystal_index not in CRYSTAL_INDICES:
        parser.error("--crystal-index must be between 0 and 14")
    return arguments


def resolve_inputs(analysis_root: Path, crystal_index: int) -> Tuple[Path, Path]:
    fit_dir = (
        analysis_root
        / "DataPreprocessing"
        / "step3-time"
        / "timeFigs"
        / "fits"
    )
    stem = f"f_{crystal_index:02d}"
    return fit_dir / f"{stem}.root", fit_dir / f"{stem}.out"


def require_file(path: Path) -> None:
    if not path.is_file():
        raise FileNotFoundError(f"Required input does not exist: {path}")


def preflight_inputs(analysis_root: Path, crystal_indices: Sequence[int]) -> None:
    missing = []
    for crystal_index in crystal_indices:
        root_path, parameter_path = resolve_inputs(analysis_root, crystal_index)
        for path in (root_path, parameter_path):
            if not path.is_file():
                missing.append(path)
    if missing:
        formatted = "\n".join(f"  - {path}" for path in missing)
        raise FileNotFoundError(f"Required inputs are missing:\n{formatted}")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as input_file:
        while True:
            block = input_file.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def load_last_th2_from_canvas(
    root_path: Path,
) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    """Return x edges, y edges, and bin contents from the last TH2 in Canvas_1."""

    root_file = ROOT.TFile.Open(str(root_path), "READ")
    if not root_file or root_file.IsZombie():
        raise OSError(f"Cannot open ROOT file: {root_path}")

    try:
        canvas = root_file.Get("Canvas_1")
        if not canvas:
            raise KeyError(f"Canvas_1 is absent from {root_path}")

        histogram = None
        for obj in canvas.GetListOfPrimitives():
            # Keep the same object-selection rule as DrawRebin.ipynb.
            if isinstance(obj, ROOT.TH2):
                histogram = obj
        if histogram is None:
            raise KeyError(f"No TH2 primitive is present in Canvas_1 of {root_path}")
        nx = histogram.GetNbinsX()
        ny = histogram.GetNbinsY()

        x_edges = np.array(
            [histogram.GetXaxis().GetBinLowEdge(index + 1) for index in range(nx + 1)],
            dtype=float,
        )
        y_edges = np.array(
            [histogram.GetYaxis().GetBinLowEdge(index + 1) for index in range(ny + 1)],
            dtype=float,
        )
        counts = np.array(
            [
                [
                    histogram.GetBinContent(x_index + 1, y_index + 1)
                    for x_index in range(nx)
                ]
                for y_index in range(ny)
            ],
            dtype=float,
        )
    finally:
        root_file.Close()

    return x_edges, y_edges, counts


def read_fit_parameters(parameter_path: Path) -> FitParameters:
    text = parameter_path.read_text(encoding="utf-8", errors="replace")

    def extract(name: str) -> float:
        match = re.search(rf"\b{name}\s*=\s*({FLOAT_PATTERN})", text)
        if match is None:
            raise ValueError(f"Parameter {name} was not found in {parameter_path}")
        return float(match.group(1))

    parameters = FitParameters(c0=extract("C0"), e0=extract("E0"), t0=extract("T0"))
    if not all(np.isfinite(value) for value in parameters):
        raise ValueError(f"Non-finite fit parameter in {parameter_path}")
    return parameters


def rebin_2d(
    counts: np.ndarray,
    x_edges: np.ndarray,
    y_edges: np.ndarray,
    factor: int,
) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    """Sum factor-by-factor bins and return rebinned and retained original grids."""

    if factor <= 0:
        raise ValueError("The rebin factor must be positive.")
    if counts.ndim != 2:
        raise ValueError("The input histogram must be two-dimensional.")
    if x_edges.size != counts.shape[1] + 1:
        raise ValueError("The x-edge array is inconsistent with the histogram.")
    if y_edges.size != counts.shape[0] + 1:
        raise ValueError("The y-edge array is inconsistent with the histogram.")
    if np.any(counts < 0):
        raise ValueError("The time-amplitude count histogram contains negative bins.")

    usable_ny = counts.shape[0] - counts.shape[0] % factor
    usable_nx = counts.shape[1] - counts.shape[1] % factor
    if usable_nx == 0 or usable_ny == 0:
        raise ValueError("The rebin factor is larger than a histogram dimension.")

    counts = counts[:usable_ny, :usable_nx]
    x_edges = x_edges[: usable_nx + 1]
    y_edges = y_edges[: usable_ny + 1]

    rebinned = counts.reshape(
        usable_ny // factor,
        factor,
        usable_nx // factor,
        factor,
    ).sum(axis=(1, 3))

    return (
        rebinned,
        x_edges[::factor],
        y_edges[::factor],
        x_edges,
        y_edges,
    )


def corrected_histogram(
    counts: np.ndarray,
    x_edges: np.ndarray,
    y_edges: np.ndarray,
    parameters: FitParameters,
    number_of_time_bins: int,
    corrected_y_edges: np.ndarray,
) -> Tuple[np.ndarray, np.ndarray, np.ndarray]:
    x_centers = 0.5 * (x_edges[:-1] + x_edges[1:])
    y_centers = 0.5 * (y_edges[:-1] + y_edges[1:])
    time_grid, amplitude_grid = np.meshgrid(x_centers, y_centers)

    with np.errstate(divide="ignore", invalid="ignore"):
        fitted_time = parameters.c0 / (parameters.e0 - amplitude_grid) + parameters.t0
        corrected_time = time_grid - fitted_time

    finite = np.isfinite(corrected_time)
    if not np.any(finite):
        raise ValueError("No finite corrected-time entries were obtained.")

    finite_corrected_time = corrected_time[finite]
    if finite_corrected_time.max() <= finite_corrected_time.min():
        raise ValueError("The corrected-time range has zero width.")
    corrected_time_edges = np.linspace(
        finite_corrected_time.min(),
        finite_corrected_time.max(),
        number_of_time_bins + 1,
    )

    # Zero-weight cells do not contribute to np.histogram2d. Excluding them
    # preserves the historical numerical result while substantially reducing
    # memory and runtime for the 15-channel batch.
    populated = finite & (counts != 0)
    if not np.any(populated):
        raise ValueError("The input histogram has no populated finite-time bins.")

    corrected_counts, corrected_time_edges, corrected_amplitude_edges = np.histogram2d(
        corrected_time[populated],
        amplitude_grid[populated],
        bins=[corrected_time_edges, corrected_y_edges],
        weights=counts[populated],
    )

    return corrected_counts.T, corrected_time_edges, corrected_amplitude_edges


def logarithmic_norm(counts: np.ndarray) -> LogNorm:
    positive = counts[counts > 0]
    if positive.size == 0:
        raise ValueError("The histogram contains no positive counts.")
    vmin = max(1.0, float(positive.min()))
    vmax = float(positive.max())
    if vmax <= vmin:
        vmax = vmin * 1.01
    return LogNorm(vmin=vmin, vmax=vmax)


def shared_logarithmic_norm(results: Sequence[ChannelResult]) -> LogNorm:
    """Return one count normalization for every raw and corrected panel."""

    maximum_counts = []
    for result in results:
        for counts in (result.raw_counts, result.corrected_counts):
            positive = counts[counts > 0]
            if positive.size:
                maximum_counts.append(float(positive.max()))
    if not maximum_counts:
        raise ValueError("No positive counts are available for plotting.")
    maximum = max(maximum_counts)
    if maximum <= 1.0:
        maximum = 1.01
    return LogNorm(vmin=1.0, vmax=maximum)


def configure_matplotlib() -> None:
    plt.rcParams.update(
        {
            "font.family": "DejaVu Sans",
            "font.size": 11,
            "axes.labelsize": 14,
            "xtick.labelsize": 12,
            "ytick.labelsize": 12,
            "axes.linewidth": 1.0,
            # DrawRebin.ipynb was produced with Matplotlib 3.3.1, whose
            # default imshow interpolation was ``antialiased``.
            "image.interpolation": "antialiased",
            "image.resample": True,
            "pdf.fonttype": 42,
            "ps.fonttype": 42,
        }
    )


def fit_curve(
    y_edges: np.ndarray,
    x_maximum: float,
    parameters: FitParameters,
) -> Tuple[np.ndarray, np.ndarray]:
    fit_amplitudes = np.linspace(150.0, y_edges[-1], 500)
    with np.errstate(divide="ignore", invalid="ignore"):
        fit_times = parameters.c0 / (parameters.e0 - fit_amplitudes) + parameters.t0
    fit_mask = (
        np.isfinite(fit_times)
        & (fit_times > 0.0)
        & (fit_times < x_maximum)
    )
    return fit_times[fit_mask], fit_amplitudes[fit_mask]


def draw_raw_panel(
    axis: plt.Axes,
    result: ChannelResult,
    count_norm: LogNorm = None,
):
    image = axis.imshow(
        result.raw_counts,
        extent=[
            result.raw_x_edges[0],
            result.raw_x_edges[-1],
            result.raw_y_edges[0],
            result.raw_y_edges[-1],
        ],
        origin="lower",
        aspect="auto",
        interpolation="antialiased",
        norm=(
            count_norm
            if count_norm is not None
            else logarithmic_norm(result.raw_counts)
        ),
    )
    fit_times, fit_amplitudes = fit_curve(
        result.raw_y_edges,
        result.raw_x_edges[-1],
        result.parameters,
    )
    axis.plot(
        fit_times,
        fit_amplitudes,
        color="red",
        linewidth=1.0,
    )
    return image


def draw_corrected_panel(
    axis: plt.Axes,
    result: ChannelResult,
    count_norm: LogNorm = None,
):
    return axis.imshow(
        result.corrected_counts,
        extent=[
            result.corrected_x_edges[0],
            result.corrected_x_edges[-1],
            result.corrected_y_edges[0],
            result.corrected_y_edges[-1],
        ],
        origin="lower",
        aspect="auto",
        interpolation="antialiased",
        norm=(
            count_norm
            if count_norm is not None
            else logarithmic_norm(result.corrected_counts)
        ),
    )


def save_figure(
    fig,
    output_pdf: Path,
    output_png: Path,
    apply_tight_layout: bool = True,
) -> None:
    output_pdf.parent.mkdir(parents=True, exist_ok=True)
    if apply_tight_layout:
        fig.tight_layout()
    # The historical notebook explicitly used dpi=300 for both PDF and PNG.
    fig.savefig(output_pdf, dpi=300, bbox_inches="tight", pad_inches=0.04)
    fig.savefig(output_png, dpi=300, bbox_inches="tight", pad_inches=0.04)
    plt.close(fig)


def draw_single_channel(
    result: ChannelResult,
    output_directory: Path,
    count_norm: LogNorm,
) -> None:
    configure_matplotlib()

    fig, axes = plt.subplots(
        1,
        2,
        figsize=(10.0, 4.0),
        sharey=True,
    )

    raw_image = draw_raw_panel(axes[0], result, count_norm=count_norm)
    draw_corrected_panel(axes[1], result, count_norm=count_norm)

    axes[0].set_xlabel(r"$t'_{\mathrm{det}}$ [ns]")
    axes[1].set_xlabel(r"$t'_{\gamma}$ [ns]")
    axes[0].set_ylabel(r"$\mathrm{CH}_{E}$")
    axes[1].tick_params(labelleft=False)

    for axis, panel_label in zip(axes, ("(a)", "(b)")):
        axis.tick_params(axis="both")
        axis.text(
            0.95,
            0.94,
            panel_label,
            transform=axis.transAxes,
            ha="right",
            va="top",
            fontsize=15,
            fontweight="bold",
        )

    fig.subplots_adjust(
        left=0.085,
        right=0.890,
        bottom=0.180,
        top=0.965,
        wspace=0.090,
    )
    colorbar_axis = fig.add_axes([0.915, 0.180, 0.018, 0.785])
    colorbar = fig.colorbar(raw_image, cax=colorbar_axis)
    colorbar.set_label("Counts", fontsize=12)
    colorbar.ax.tick_params(labelsize=10)

    output_stem = (
        "cshine_gamma_time_amplitude_correction_"
        f"CsI{result.crystal_index:02d}_horizontal"
    )
    save_figure(
        fig,
        output_directory / f"{output_stem}.pdf",
        output_directory / f"{output_stem}.png",
        apply_tight_layout=False,
    )


def draw_overview(
    results: Sequence[ChannelResult],
    corrected: bool,
    output_directory: Path,
    count_norm: LogNorm,
) -> None:
    configure_matplotlib()
    result_by_index = {result.crystal_index: result for result in results}
    fig, axes = plt.subplots(
        4,
        4,
        figsize=(12.0, 11.0),
        sharex=True,
        sharey=True,
    )

    if corrected:
        x_minimum = min(result.corrected_x_edges[0] for result in results)
        x_maximum = max(result.corrected_x_edges[-1] for result in results)
        y_minimum = min(result.corrected_y_edges[0] for result in results)
        y_maximum = max(result.corrected_y_edges[-1] for result in results)
        common_x_label = r"$t'_{\gamma}$ [ns]"
    else:
        x_minimum = min(result.raw_x_edges[0] for result in results)
        x_maximum = max(result.raw_x_edges[-1] for result in results)
        y_minimum = min(result.raw_y_edges[0] for result in results)
        y_maximum = max(result.raw_y_edges[-1] for result in results)
        common_x_label = r"$t'_{\mathrm{det}}$ [ns]"

    color_image = None

    for row_index, grid_row in enumerate(OVERVIEW_GRID):
        for column_index, crystal_index in enumerate(grid_row):
            axis = axes[row_index, column_index]
            if crystal_index is None:
                axis.axis("off")
                continue

            result = result_by_index[crystal_index]
            if corrected:
                color_image = draw_corrected_panel(
                    axis,
                    result,
                    count_norm=count_norm,
                )
            else:
                color_image = draw_raw_panel(
                    axis,
                    result,
                    count_norm=count_norm,
                )

            axis.set_title(f"CsI{crystal_index:02d}", fontsize=11)
            axis.set_xlim(x_minimum, x_maximum)
            axis.set_ylim(y_minimum, y_maximum)
            axis.tick_params(
                axis="both",
                labelsize=8,
                labelbottom=(row_index == len(OVERVIEW_GRID) - 1),
                labelleft=(column_index == 0),
            )

    if color_image is None:
        raise RuntimeError("No populated crystal panel was drawn.")

    fig.subplots_adjust(
        left=0.075,
        right=0.895,
        bottom=0.075,
        top=0.965,
        wspace=0.100,
        hspace=0.220,
    )
    fig.text(
        0.485,
        0.018,
        common_x_label,
        ha="center",
        va="center",
        fontsize=14,
    )
    fig.text(
        0.020,
        0.520,
        r"$\mathrm{CH}_{E}$",
        ha="center",
        va="center",
        rotation="vertical",
        fontsize=14,
    )
    colorbar_axis = fig.add_axes([0.920, 0.075, 0.018, 0.890])
    colorbar = fig.colorbar(color_image, cax=colorbar_axis)
    colorbar.set_label("Counts", fontsize=12)
    colorbar.ax.tick_params(labelsize=10)

    state = "after_correction" if corrected else "before_correction"
    save_figure(
        fig,
        output_directory / f"all_crystals_{state}.pdf",
        output_directory / f"all_crystals_{state}.png",
        apply_tight_layout=False,
    )


def process_channel(
    analysis_root: Path,
    crystal_index: int,
    rebin_factor: int,
) -> ChannelResult:
    root_path, parameter_path = resolve_inputs(analysis_root, crystal_index)
    require_file(root_path)
    require_file(parameter_path)

    raw_x_edges, raw_y_edges, counts = load_last_th2_from_canvas(root_path)
    parameters = read_fit_parameters(parameter_path)

    (
        rebinned_counts,
        rebinned_x_edges,
        rebinned_y_edges,
        original_x_edges,
        original_y_edges,
    ) = rebin_2d(
        counts,
        raw_x_edges,
        raw_y_edges,
        rebin_factor,
    )

    usable_counts = counts[
        : original_y_edges.size - 1,
        : original_x_edges.size - 1,
    ]
    corrected_counts, corrected_x_edges, corrected_y_edges = corrected_histogram(
        usable_counts,
        original_x_edges,
        original_y_edges,
        parameters,
        number_of_time_bins=rebinned_counts.shape[1],
        corrected_y_edges=rebinned_y_edges,
    )

    return ChannelResult(
        crystal_index=crystal_index,
        root_path=root_path,
        parameter_path=parameter_path,
        root_sha256=sha256_file(root_path),
        parameter_sha256=sha256_file(parameter_path),
        parameters=parameters,
        rebin_factor=rebin_factor,
        input_y_bins=usable_counts.shape[0],
        input_x_bins=usable_counts.shape[1],
        input_count_sum=float(usable_counts.sum()),
        raw_counts=rebinned_counts,
        raw_x_edges=rebinned_x_edges,
        raw_y_edges=rebinned_y_edges,
        corrected_counts=corrected_counts,
        corrected_x_edges=corrected_x_edges,
        corrected_y_edges=corrected_y_edges,
    )


def relative_path(path: Path, analysis_root: Path) -> str:
    try:
        return str(path.relative_to(analysis_root))
    except ValueError:
        return path.name


def count_differences(result: ChannelResult) -> Tuple[float, float]:
    rebinned_sum = float(result.raw_counts.sum())
    corrected_sum = float(result.corrected_counts.sum())
    return (
        rebinned_sum - result.input_count_sum,
        corrected_sum - result.input_count_sum,
    )


def validate_count_conservation(
    results: Sequence[ChannelResult],
    tolerance: float = COUNT_TOLERANCE,
) -> None:
    failures = []
    for result in results:
        rebinned_difference, corrected_difference = count_differences(result)
        if (
            abs(rebinned_difference) > tolerance
            or abs(corrected_difference) > tolerance
        ):
            failures.append(
                f"CsI{result.crystal_index:02d}: "
                f"rebinned-input={rebinned_difference:.17g}, "
                f"corrected-input={corrected_difference:.17g}"
            )
    if failures:
        detail = "\n".join(f"  - {failure}" for failure in failures)
        raise RuntimeError(
            "Count-conservation validation failed; figures were not generated:\n"
            f"{detail}"
        )


def write_validation_summary(
    results: Sequence[ChannelResult],
    analysis_root: Path,
    output_path: Path,
) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = [
        "crystal",
        "root_input",
        "parameter_input",
        "root_sha256",
        "parameter_sha256",
        "c0",
        "e0",
        "t0",
        "rebin_factor",
        "input_y_bins",
        "input_x_bins",
        "rebinned_y_bins",
        "rebinned_x_bins",
        "input_count_sum",
        "rebinned_count_sum",
        "corrected_count_sum",
        "rebinned_minus_input",
        "corrected_minus_input",
        "count_tolerance",
        "validation_passed",
    ]
    with output_path.open("w", newline="", encoding="utf-8") as output_file:
        writer = csv.DictWriter(output_file, fieldnames=fieldnames)
        writer.writeheader()
        for result in results:
            input_sum = result.input_count_sum
            rebinned_sum = float(result.raw_counts.sum())
            corrected_sum = float(result.corrected_counts.sum())
            rebinned_difference, corrected_difference = count_differences(result)
            validation_passed = (
                abs(rebinned_difference) <= COUNT_TOLERANCE
                and abs(corrected_difference) <= COUNT_TOLERANCE
            )
            writer.writerow(
                {
                    "crystal": f"CsI{result.crystal_index:02d}",
                    "root_input": relative_path(result.root_path, analysis_root),
                    "parameter_input": relative_path(
                        result.parameter_path,
                        analysis_root,
                    ),
                    "root_sha256": result.root_sha256,
                    "parameter_sha256": result.parameter_sha256,
                    "c0": f"{result.parameters.c0:.17g}",
                    "e0": f"{result.parameters.e0:.17g}",
                    "t0": f"{result.parameters.t0:.17g}",
                    "rebin_factor": result.rebin_factor,
                    "input_y_bins": result.input_y_bins,
                    "input_x_bins": result.input_x_bins,
                    "rebinned_y_bins": result.raw_counts.shape[0],
                    "rebinned_x_bins": result.raw_counts.shape[1],
                    "input_count_sum": f"{input_sum:.17g}",
                    "rebinned_count_sum": f"{rebinned_sum:.17g}",
                    "corrected_count_sum": f"{corrected_sum:.17g}",
                    "rebinned_minus_input": f"{rebinned_difference:.17g}",
                    "corrected_minus_input": f"{corrected_difference:.17g}",
                    "count_tolerance": f"{COUNT_TOLERANCE:.17g}",
                    "validation_passed": validation_passed,
                }
            )


def write_run_metadata(
    results: Sequence[ChannelResult],
    rebin_factor: int,
    count_norm: LogNorm,
    output_path: Path,
) -> None:
    validation_passed = all(
        abs(rebinned_difference) <= COUNT_TOLERANCE
        and abs(corrected_difference) <= COUNT_TOLERANCE
        for rebinned_difference, corrected_difference in (
            count_differences(result) for result in results
        )
    )
    metadata = {
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "script": Path(__file__).name,
        "script_sha256": sha256_file(Path(__file__).resolve()),
        "python_version": platform.python_version(),
        "root_version": ROOT.gROOT.GetVersion(),
        "numpy_version": np.__version__,
        "matplotlib_version": matplotlib.__version__,
        "rebin_factor": rebin_factor,
        "count_tolerance": COUNT_TOLERANCE,
        "validation_passed": validation_passed,
        "plot_count_normalization": {
            "type": "LogNorm",
            "vmin": float(count_norm.vmin),
            "vmax": float(count_norm.vmax),
            "scope": "all raw and corrected panels generated in this run",
        },
        "crystals": [f"CsI{result.crystal_index:02d}" for result in results],
        "validation_summary": "validation_summary.csv",
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(
        json.dumps(metadata, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def main() -> None:
    arguments = parse_arguments()
    analysis_root = arguments.analysis_root.resolve()
    crystal_indices = (
        CRYSTAL_INDICES if arguments.all_crystals else (arguments.crystal_index,)
    )
    preflight_inputs(analysis_root, crystal_indices)

    results = []
    total_channels = len(crystal_indices)
    for position, index in enumerate(crystal_indices, start=1):
        print(
            f"[{position}/{total_channels}] Processing CsI{index:02d}...",
            flush=True,
        )
        result = process_channel(analysis_root, index, arguments.rebin_factor)
        results.append(result)
        rebinned_difference, corrected_difference = count_differences(result)
        print(
            f"    input={result.input_count_sum:g}, "
            f"rebinned-input={rebinned_difference:g}, "
            f"corrected-input={corrected_difference:g}",
            flush=True,
        )

    time_walk_directory = arguments.output_dir / "time_walk"
    individual_directory = time_walk_directory / "individual"
    overview_directory = time_walk_directory / "overview"
    summary_path = time_walk_directory / "validation_summary.csv"
    write_validation_summary(results, analysis_root, summary_path)
    count_norm = shared_logarithmic_norm(results)
    metadata_path = time_walk_directory / "run_metadata.json"
    write_run_metadata(
        results,
        arguments.rebin_factor,
        count_norm,
        metadata_path,
    )
    validate_count_conservation(results)

    for result in results:
        draw_single_channel(
            result,
            individual_directory,
            count_norm=count_norm,
        )

    if arguments.all_crystals:
        draw_overview(
            results,
            corrected=False,
            output_directory=overview_directory,
            count_norm=count_norm,
        )
        draw_overview(
            results,
            corrected=True,
            output_directory=overview_directory,
            count_norm=count_norm,
        )

    print(f"Figures written under: {time_walk_directory.resolve()}")
    print(f"Validation summary: {summary_path.resolve()}")
    print(f"Run metadata: {metadata_path.resolve()}")


if __name__ == "__main__":
    ROOT.gROOT.SetBatch(True)
    main()
