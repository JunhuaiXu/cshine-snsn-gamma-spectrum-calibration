#!/usr/bin/env python3
"""Unified M0--M12 verification, build, and execution entry.

This program is intentionally limited to the migrated DataPreprocessing stages.
It does not encode or alter any physics selection, fit, or ROOT object schema.
"""

from __future__ import print_function

import argparse
import csv
import datetime
import hashlib
import json
import os
import platform
import re
import subprocess
import sys
from pathlib import Path


TOOL_VERSION = "0.12.2"
SCRIPT_PATH = Path(__file__).resolve()
REPOSITORY_ROOT = SCRIPT_PATH.parents[1]
ANALYSIS_SOURCE = REPOSITORY_ROOT / "analysis" / "data_preprocessing"
PROVENANCE_DIRECTORY = ANALYSIS_SOURCE / "provenance"
DEFAULT_BUILD_DIRECTORY = REPOSITORY_ROOT / "build" / "data_preprocessing"
DEFAULT_RESULTS_DIRECTORY = REPOSITORY_ROOT / "results" / "data_preprocessing"
DEFAULT_M6_RUN_MANIFEST = (
    ANALYSIS_SOURCE / "config" / "central_beam_on_run_groups.tsv"
)
DEFAULT_M9_BEAM_OFF_RUN_MANIFEST = (
    ANALYSIS_SOURCE / "config" / "central_beam_off_run_groups.tsv"
)
DEFAULT_RECONSTRUCTION_SPECTRA_FIGURE_MANIFEST = (
    ANALYSIS_SOURCE / "config" / "reconstruction_spectra_figure_run_groups.tsv"
)
TOOLS_TEST_DIRECTORY = REPOSITORY_ROOT / "tools" / "tests"
RUN_ID_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
M5_TIME_INPUT_PATTERNS = (
    "a20240303_SnSn_ALLCOIN.*.root",
    "a20240303_SnSn_GOAL_ALLCOIN.*.root",
    "a20240304_SnSn_GOAL_ALLCOIN.*.root",
    "a20240305_SnSn_GOAL_ALLCOIN.*.root",
    "a20240306_SnSn_GOAL_ALLCOIN.*.root",
    "a20240307_SnSn_GOAL_ALLCOIN.*.root",
    "a20240308_SnSn_GOAL_ALLCOIN.*.root",
    "a20240309_SnSn_GOAL_ALLCOIN.*.root",
    "a20240310_SnSn_GOAL_ALLCOIN.*.root",
)


class CommandFailure(RuntimeError):
    def __init__(self, command, return_code, output):
        RuntimeError.__init__(
            self,
            "command failed with exit code {0}: {1}".format(
                return_code, " ".join(command)
            ),
        )
        self.command = command
        self.return_code = return_code
        self.output = output


def utc_now():
    return datetime.datetime.utcnow().replace(microsecond=0).isoformat() + "Z"


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def run_command(command, cwd=None, check=True, echo=True):
    process = subprocess.Popen(
        [str(item) for item in command],
        cwd=str(cwd) if cwd is not None else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    )
    output, _ = process.communicate()
    if echo and output:
        print(output, end="" if output.endswith("\n") else "\n")
    if check and process.returncode != 0:
        raise CommandFailure(
            [str(item) for item in command], process.returncode, output or ""
        )
    return process.returncode, output or ""


def command_version(command):
    try:
        return_code, output = run_command(command, check=False, echo=False)
    except OSError:
        return None
    if return_code != 0:
        return None
    lines = [line.strip() for line in output.splitlines() if line.strip()]
    return lines[0] if lines else None


def git_revision():
    try:
        return_code, output = run_command(
            ["git", "-C", str(REPOSITORY_ROOT), "rev-parse", "HEAD"],
            check=False,
            echo=False,
        )
    except OSError:
        return None
    if return_code != 0:
        return None
    return output.strip() or None


def environment_record():
    return {
        "platform": platform.platform(),
        "python": sys.version.replace("\n", " "),
        "root": command_version(["root-config", "--version"]),
        "cmake": command_version(["cmake", "--version"]),
        "compiler": command_version(["c++", "--version"]),
        "git_revision": git_revision(),
    }


def provenance_record():
    source_manifest = PROVENANCE_DIRECTORY / "source_manifest.tsv"
    migration_manifest = PROVENANCE_DIRECTORY / "migration_manifest.tsv"
    return {
        "entrypoint_sha256": sha256_file(SCRIPT_PATH),
        "source_manifest_sha256": sha256_file(source_manifest),
        "migration_manifest_sha256": sha256_file(migration_manifest),
    }


def write_text_atomic(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("w", encoding="utf-8") as stream:
        stream.write(content)
    os.replace(str(temporary), str(path))


def write_json_atomic(path, value):
    write_text_atomic(
        path,
        json.dumps(value, indent=2, sort_keys=True, ensure_ascii=False) + "\n",
    )


def read_tsv(path):
    with path.open("r", encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def require_unique(rows, field, label):
    values = [row.get(field, "") for row in rows]
    duplicates = sorted({value for value in values if values.count(value) > 1})
    if duplicates:
        raise RuntimeError(
            "duplicate {0} values in {1}: {2}".format(
                field, label, ", ".join(duplicates)
            )
        )


def verify_reconstruction_spectra_figure_manifest(manifest_path=None):
    _, central_rows = read_m6_run_groups(DEFAULT_M6_RUN_MANIFEST)
    reference_manifest, reference_rows = read_m6_run_groups(
        DEFAULT_RECONSTRUCTION_SPECTRA_FIGURE_MANIFEST
    )
    central_ids = [row["run_id"] for row in central_rows]
    reference_ids = [row["run_id"] for row in reference_rows]
    if len(reference_rows) != 59:
        raise RuntimeError(
            "reconstruction-spectrum figure manifest must contain 59 run groups"
        )
    if reference_ids != central_ids[1:]:
        raise RuntimeError(
            "reconstruction-spectrum figure manifest must equal the reviewed "
            "60-group central sample without its March 4 run group"
        )

    if manifest_path is None:
        figure_manifest = reference_manifest
        figure_rows = reference_rows
    else:
        figure_manifest, figure_rows = read_m6_run_groups(manifest_path)
        if figure_rows != reference_rows:
            raise RuntimeError(
                "reconstruction-spectrum run manifest must exactly match the "
                "frozen 59-group historical figure manifest"
            )
    return {
        "path": str(figure_manifest),
        "run_group_count": len(figure_rows),
        "excluded_central_run_groups": [central_ids[0]],
    }


def verify_manifests(snapshot_root=None):
    source_manifest = PROVENANCE_DIRECTORY / "source_manifest.tsv"
    migration_manifest = PROVENANCE_DIRECTORY / "migration_manifest.tsv"
    source_rows = read_tsv(source_manifest)
    migration_rows = read_tsv(migration_manifest)
    require_unique(source_rows, "source_id", "source manifest")
    require_unique(migration_rows, "portable_path", "migration manifest")
    figure_manifest_summary = verify_reconstruction_spectra_figure_manifest()

    portable_checked = 0
    portable_generated = 0
    errors = []
    for row in migration_rows:
        expected = row.get("portable_sha256", "")
        relative_path = row.get("portable_path", "")
        if expected == "generated":
            portable_generated += 1
            continue
        if not relative_path or not expected:
            errors.append("incomplete migration record: {0}".format(relative_path))
            continue
        path = REPOSITORY_ROOT / relative_path
        if not path.is_file():
            errors.append("missing portable file: {0}".format(relative_path))
            continue
        actual = sha256_file(path)
        if actual != expected:
            errors.append(
                "portable SHA-256 mismatch: {0} expected={1} actual={2}".format(
                    relative_path, expected, actual
                )
            )
            continue
        portable_checked += 1

    source_checked = 0
    if snapshot_root is not None:
        snapshot_root = Path(snapshot_root).expanduser().resolve()
        for row in source_rows:
            relative_path = row.get("historical_path", "")
            expected = row.get("sha256", "")
            path = snapshot_root / relative_path
            if not path.is_file():
                errors.append("missing frozen source: {0}".format(relative_path))
                continue
            actual = sha256_file(path)
            if actual != expected:
                errors.append(
                    "source SHA-256 mismatch: {0} expected={1} actual={2}".format(
                        relative_path, expected, actual
                    )
                )
                continue
            source_checked += 1

    if errors:
        raise RuntimeError("manifest verification failed:\n  " + "\n  ".join(errors))

    summary = {
        "source_records": len(source_rows),
        "source_files_checked": source_checked,
        "source_snapshot_checked": snapshot_root is not None,
        "portable_records": len(migration_rows),
        "portable_files_checked": portable_checked,
        "generated_artifacts_skipped": portable_generated,
        "reconstruction_spectra_figure_manifest": figure_manifest_summary,
    }
    print(
        "manifest verification passed: {0} portable files, {1} generated "
        "artifacts, {2} frozen source files checked".format(
            portable_checked, portable_generated, source_checked
        )
    )
    return summary


def create_run_directory(results_directory, stage, run_id):
    if run_id is None:
        run_id = datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ")
    if not RUN_ID_PATTERN.match(run_id):
        raise ValueError(
            "run ID must contain only letters, numbers, '.', '_' or '-'"
        )
    run_directory = Path(results_directory).expanduser().resolve() / stage / run_id
    try:
        run_directory.mkdir(parents=True, exist_ok=False)
    except FileExistsError:
        raise RuntimeError(
            "run directory already exists; choose a new --run-id: {0}".format(
                run_directory
            )
        )
    return run_directory


def parse_input_paths(report_path, stage):
    paths = []
    with report_path.open("r", encoding="utf-8") as stream:
        for raw_line in stream:
            fields = raw_line.rstrip("\n").split("\t")
            if not fields or fields[0] != "input":
                continue
            path_index = 3 if stage == "m2" else 2
            if len(fields) <= path_index or not fields[path_index]:
                raise RuntimeError(
                    "cannot parse input record in {0}: {1}".format(
                        report_path, raw_line.rstrip()
                    )
                )
            if stage in ("m2", "m4"):
                role = fields[1]
            else:
                role = "gain_relation"
            paths.append((role, Path(fields[path_index])))
    if not paths:
        raise RuntimeError("run report contains no input records: {0}".format(report_path))
    return paths


def write_input_manifest(path, input_paths, hash_inputs):
    lines = ["index\trole\tpath\tsize_bytes\tmtime_ns\tsha256"]
    records = []
    for index, (role, input_path) in enumerate(input_paths):
        stat = input_path.stat()
        digest = sha256_file(input_path) if hash_inputs else "not_computed"
        mtime_ns = getattr(stat, "st_mtime_ns", int(stat.st_mtime * 1000000000))
        lines.append(
            "{0}\t{1}\t{2}\t{3}\t{4}\t{5}".format(
                index, role, input_path, stat.st_size, mtime_ns, digest
            )
        )
        records.append(
            {
                "role": role,
                "path": str(input_path),
                "size_bytes": stat.st_size,
                "mtime_ns": mtime_ns,
                "sha256": digest,
            }
        )
    write_text_atomic(path, "\n".join(lines) + "\n")
    return records


def output_records(paths):
    records = []
    for path in paths:
        if path.is_file():
            records.append(
                {
                    "path": str(path),
                    "size_bytes": path.stat().st_size,
                    "sha256": sha256_file(path),
                }
            )
    return records


def run_stage(args, stage):
    verify_summary = verify_manifests()
    build_directory = Path(args.build_dir).expanduser().resolve()
    executable_names = {
        "m2": "build_source_spectra",
        "m3": "fit_gain_relation",
        "m4": "fit_energy_calibration",
    }
    executable_name = executable_names[stage]
    executable = build_directory / executable_name
    if not executable.is_file():
        raise RuntimeError(
            "missing executable {0}; run the 'check' command first".format(executable)
        )

    run_directory = create_run_directory(args.results_dir, stage, args.run_id)
    output_names = {
        "m2": "source_background.root",
        "m3": "gain_relation.root",
        "m4": "energy_calibration.root",
    }
    output_root = run_directory / output_names[stage]
    run_report = run_directory / "run_report.tsv"
    config_path = run_directory / "config_used.txt"
    log_path = run_directory / "run.log"
    metadata_path = run_directory / "run_metadata.json"
    parameter_path = run_directory / "gain_parameters.txt"

    started = utc_now()
    metadata = {
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "stage": stage,
        "status": "running",
        "started_utc": started,
        "repository_root": str(REPOSITORY_ROOT),
        "run_directory": str(run_directory),
        "input_hash_mode": "sha256" if args.hash_inputs else "size-and-mtime",
        "environment": environment_record(),
        "provenance": provenance_record(),
        "manifest_verification": verify_summary,
        "executable": {
            "path": str(executable),
            "sha256": sha256_file(executable),
        },
    }
    write_json_atomic(metadata_path, metadata)

    try:
        _, config_output = run_command([str(executable), "--print-config"])
        write_text_atomic(config_path, config_output)
        if stage == "m4":
            command = [
                str(executable),
                "--source-spectra",
                str(Path(args.source_spectra).expanduser().resolve()),
                "--gain-relation",
                str(Path(args.gain_relation).expanduser().resolve()),
                "--output",
                str(output_root),
                "--report",
                str(run_report),
            ]
        else:
            command = [
                str(executable),
                "--input-dir",
                str(Path(args.input_dir).expanduser().resolve()),
                "--output",
                str(output_root),
                "--report",
                str(run_report),
            ]
        if stage == "m3":
            command.extend(["--parameters", str(parameter_path)])
        metadata["command"] = command
        return_code, output = run_command(command, check=False)
        write_text_atomic(log_path, output)
        metadata["return_code"] = return_code
        if return_code != 0:
            raise CommandFailure(command, return_code, output)

        input_paths = parse_input_paths(run_report, stage)
        inputs = write_input_manifest(
            run_directory / "input_manifest.tsv", input_paths, args.hash_inputs
        )
        expected_outputs = [output_root, run_report, config_path, log_path]
        if stage == "m3":
            expected_outputs.append(parameter_path)
        metadata["inputs"] = inputs
        metadata["outputs"] = output_records(expected_outputs)
        metadata["status"] = "completed"
        metadata["completed_utc"] = utc_now()
        write_json_atomic(metadata_path, metadata)
        print("completed {0} run: {1}".format(stage.upper(), run_directory))
    except Exception as error:
        metadata["status"] = "failed"
        metadata["completed_utc"] = utc_now()
        metadata["error"] = str(error)
        write_json_atomic(metadata_path, metadata)
        raise


def historical_time_fit_inputs(fits_directory):
    fits_directory = Path(fits_directory).expanduser().resolve()
    paths = []
    for crystal in range(15):
        stem = "f_{0:02d}".format(crystal)
        for role, suffix in (("fit_root", ".root"), ("fit_text", ".out")):
            path = fits_directory / (stem + suffix)
            if not path.is_file():
                raise RuntimeError("missing M5 historical fit input: {0}".format(path))
            paths.append(("{0}_{1:02d}".format(role, crystal), path))
    return paths


def run_m5_audit(args):
    verify_summary = verify_manifests()
    build_directory = Path(args.build_dir).expanduser().resolve()
    executable = build_directory / "inspect_time_fit_outputs"
    if not executable.is_file():
        raise RuntimeError(
            "missing executable {0}; run the 'check' command first".format(executable)
        )

    input_paths = historical_time_fit_inputs(args.fits_dir)
    run_directory = create_run_directory(
        args.results_dir, "m5-audit", args.run_id
    )
    report_path = run_directory / "time_fit_audit.tsv"
    config_path = run_directory / "config_used.txt"
    log_path = run_directory / "run.log"
    metadata_path = run_directory / "run_metadata.json"
    metadata = {
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "stage": "m5-audit",
        "status": "running",
        "started_utc": utc_now(),
        "repository_root": str(REPOSITORY_ROOT),
        "run_directory": str(run_directory),
        "input_hash_mode": "sha256" if args.hash_inputs else "size-and-mtime",
        "environment": environment_record(),
        "provenance": provenance_record(),
        "manifest_verification": verify_summary,
        "executable": {
            "path": str(executable),
            "sha256": sha256_file(executable),
        },
    }
    write_json_atomic(metadata_path, metadata)

    try:
        _, config_output = run_command([str(executable), "--print-config"])
        write_text_atomic(config_path, config_output)
        metadata["inputs"] = write_input_manifest(
            run_directory / "input_manifest.tsv", input_paths, args.hash_inputs
        )
        command = [
            str(executable),
            "--fits-dir",
            str(Path(args.fits_dir).expanduser().resolve()),
            "--report",
            str(report_path),
            "--tolerance",
            str(args.tolerance),
        ]
        metadata["command"] = command
        return_code, output = run_command(command, check=False)
        write_text_atomic(log_path, output)
        metadata["return_code"] = return_code
        if return_code != 0:
            raise CommandFailure(command, return_code, output)

        metadata["outputs"] = output_records(
            [report_path, config_path, log_path, run_directory / "input_manifest.tsv"]
        )
        metadata["status"] = "completed"
        metadata["completed_utc"] = utc_now()
        write_json_atomic(metadata_path, metadata)
        print("completed M5 historical-fit audit: {0}".format(run_directory))
    except Exception as error:
        metadata["status"] = "failed"
        metadata["completed_utc"] = utc_now()
        metadata["error"] = str(error)
        write_json_atomic(metadata_path, metadata)
        raise


def historical_time_spectrum_inputs(input_directory):
    input_directory = Path(input_directory).expanduser().resolve()
    paths = []
    seen = set()
    for pattern in M5_TIME_INPUT_PATTERNS:
        matched = sorted(input_directory.glob(pattern))
        if not matched:
            raise RuntimeError(
                "M5 time-spectrum input pattern matched no files: {0}".format(
                    pattern
                )
            )
        for path in matched:
            resolved = path.resolve()
            if resolved in seen:
                continue
            seen.add(resolved)
            paths.append(("raw_event_tree", resolved))
    return paths


def run_m5_spectra(args):
    verify_summary = verify_manifests()
    build_directory = Path(args.build_dir).expanduser().resolve()
    executable = build_directory / "build_time_amplitude_spectra"
    if not executable.is_file():
        raise RuntimeError(
            "missing executable {0}; run the 'check' command first".format(executable)
        )

    input_paths = historical_time_spectrum_inputs(args.input_dir)
    run_directory = create_run_directory(
        args.results_dir, "m5-spectra", args.run_id
    )
    output_name = (
        "time_orig.root"
        if args.mode == "original"
        else "time_cali_historical_diagnostic.root"
    )
    output_root = run_directory / output_name
    report_path = run_directory / "run_report.tsv"
    config_path = run_directory / "config_used.txt"
    log_path = run_directory / "run.log"
    metadata_path = run_directory / "run_metadata.json"
    metadata = {
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "stage": "m5-spectra",
        "mode": args.mode,
        "status": "running",
        "started_utc": utc_now(),
        "repository_root": str(REPOSITORY_ROOT),
        "run_directory": str(run_directory),
        "input_hash_mode": "sha256" if args.hash_inputs else "size-and-mtime",
        "environment": environment_record(),
        "provenance": provenance_record(),
        "manifest_verification": verify_summary,
        "executable": {
            "path": str(executable),
            "sha256": sha256_file(executable),
        },
    }
    write_json_atomic(metadata_path, metadata)

    try:
        metadata["inputs"] = write_input_manifest(
            run_directory / "input_manifest.tsv", input_paths, args.hash_inputs
        )
        config_command = [
            str(executable),
            "--print-config",
            "--mode",
            args.mode,
            "--threads",
            str(args.threads),
        ]
        _, config_output = run_command(config_command)
        write_text_atomic(config_path, config_output)
        command = [
            str(executable),
            "--input-dir",
            str(Path(args.input_dir).expanduser().resolve()),
            "--output",
            str(output_root),
            "--report",
            str(report_path),
            "--mode",
            args.mode,
            "--threads",
            str(args.threads),
        ]
        metadata["command"] = command
        return_code, output = run_command(command, check=False)
        write_text_atomic(log_path, output)
        metadata["return_code"] = return_code
        if return_code != 0:
            raise CommandFailure(command, return_code, output)

        metadata["outputs"] = output_records(
            [
                output_root,
                report_path,
                config_path,
                log_path,
                run_directory / "input_manifest.tsv",
            ]
        )
        metadata["status"] = "completed"
        metadata["completed_utc"] = utc_now()
        write_json_atomic(metadata_path, metadata)
        print("completed M5 time-spectrum run: {0}".format(run_directory))
    except Exception as error:
        metadata["status"] = "failed"
        metadata["completed_utc"] = utc_now()
        metadata["error"] = str(error)
        write_json_atomic(metadata_path, metadata)
        raise


def read_m6_run_groups(manifest_path):
    manifest_path = Path(manifest_path).expanduser().resolve()
    rows = read_tsv(manifest_path)
    if not rows:
        raise RuntimeError("M6 run manifest is empty: {0}".format(manifest_path))
    required = ("index", "run_id", "raw_pattern", "output_name")
    for row_number, row in enumerate(rows, 1):
        missing = [field for field in required if not row.get(field)]
        if missing:
            raise RuntimeError(
                "M6 run manifest row {0} is missing: {1}".format(
                    row_number, ", ".join(missing)
                )
            )
        if int(row["index"]) != row_number - 1:
            raise RuntimeError("M6 run manifest indices must be contiguous from zero")
        if not RUN_ID_PATTERN.match(row["run_id"]):
            raise RuntimeError("invalid M6 run ID: {0}".format(row["run_id"]))
        pattern = Path(row["raw_pattern"])
        output_name = Path(row["output_name"])
        if pattern.is_absolute() or ".." in pattern.parts:
            raise RuntimeError("unsafe M6 raw pattern: {0}".format(pattern))
        if output_name.is_absolute() or len(output_name.parts) != 1:
            raise RuntimeError("unsafe M6 output name: {0}".format(output_name))
    require_unique(rows, "run_id", "M6 run manifest")
    require_unique(rows, "raw_pattern", "M6 run manifest")
    require_unique(rows, "output_name", "M6 run manifest")
    return manifest_path, rows


def resolve_m6_inputs(input_directory, rows):
    input_directory = Path(input_directory).expanduser().resolve()
    resolved = []
    for row in rows:
        paths = sorted(path.resolve() for path in input_directory.glob(row["raw_pattern"]))
        if not paths:
            raise RuntimeError(
                "M6 input pattern matched no files: {0}".format(row["raw_pattern"])
            )
        resolved.append((row, paths))
    return resolved


def resolve_m7_inputs(input_directory, rows):
    """Resolve the exact M6 event-tree names required by the M7 contract."""
    input_directory = Path(input_directory).expanduser().resolve()
    resolved = []
    for row in rows:
        path = (input_directory / row["output_name"]).resolve()
        if not path.is_file():
            raise RuntimeError("missing M7 calibrated event tree: {0}".format(path))
        resolved.append(("calibrated_event_tree:" + row["run_id"], path))
    return resolved


def resolve_manifest_outputs(input_directory, rows, role_prefix, stage):
    """Resolve exact manifest-defined outputs without wildcard discovery."""
    input_directory = Path(input_directory).expanduser().resolve()
    resolved = []
    for row in rows:
        path = (input_directory / row["output_name"]).resolve()
        if not path.is_file():
            raise RuntimeError(
                "missing {0} manifest output: {1}".format(stage, path)
            )
        resolved.append((role_prefix + ":" + row["run_id"], path))
    return resolved


def run_m6(args):
    verify_summary = verify_manifests()
    build_directory = Path(args.build_dir).expanduser().resolve()
    executable = build_directory / "build_calibrated_event_tree"
    if not executable.is_file():
        raise RuntimeError(
            "missing executable {0}; run the 'check' command first".format(executable)
        )
    calibration = Path(args.calibration).expanduser().resolve()
    if not calibration.is_file():
        raise RuntimeError("missing M6 calibration ROOT file: {0}".format(calibration))
    manifest_path, rows = read_m6_run_groups(args.run_manifest)
    resolved_groups = resolve_m6_inputs(args.input_dir, rows)
    run_directory = create_run_directory(args.results_dir, "m6", args.run_id)
    event_directory = run_directory / "events"
    report_directory = run_directory / "reports"
    event_directory.mkdir()
    report_directory.mkdir()
    config_path = run_directory / "config_used.txt"
    log_path = run_directory / "run.log"
    metadata_path = run_directory / "run_metadata.json"
    metadata = {
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "stage": "m6",
        "status": "running",
        "started_utc": utc_now(),
        "repository_root": str(REPOSITORY_ROOT),
        "run_directory": str(run_directory),
        "run_group_count": len(rows),
        "input_hash_mode": "sha256" if args.hash_inputs else "size-and-mtime",
        "environment": environment_record(),
        "provenance": provenance_record(),
        "manifest_verification": verify_summary,
        "executable": {"path": str(executable), "sha256": sha256_file(executable)},
        "commands": [],
    }
    write_json_atomic(metadata_path, metadata)
    try:
        _, config_output = run_command([str(executable), "--print-config"])
        write_text_atomic(
            config_path,
            config_output + "run_manifest=" + str(manifest_path) + "\n",
        )
        input_records = [("calibration", calibration), ("run_manifest", manifest_path)]
        for row, paths in resolved_groups:
            input_records.extend(("raw:" + row["run_id"], path) for path in paths)
        metadata["inputs"] = write_input_manifest(
            run_directory / "input_manifest.tsv", input_records, args.hash_inputs
        )
        log_sections = []
        expected_outputs = [config_path, run_directory / "input_manifest.tsv"]
        for row, paths in resolved_groups:
            output_root = event_directory / row["output_name"]
            report_path = report_directory / (row["run_id"] + ".tsv")
            command = [str(executable)]
            for path in paths:
                command.extend(["--input", str(path)])
            command.extend(
                [
                    "--calibration", str(calibration),
                    "--output", str(output_root),
                    "--report", str(report_path),
                ]
            )
            metadata["commands"].append(command)
            return_code, output = run_command(command, check=False)
            log_sections.append("===== {0} =====\n{1}".format(row["run_id"], output))
            if return_code != 0:
                write_text_atomic(log_path, "\n".join(log_sections))
                raise CommandFailure(command, return_code, output)
            expected_outputs.extend([output_root, report_path])
        write_text_atomic(log_path, "\n".join(log_sections))
        expected_outputs.append(log_path)
        metadata["outputs"] = output_records(expected_outputs)
        metadata["status"] = "completed"
        metadata["completed_utc"] = utc_now()
        write_json_atomic(metadata_path, metadata)
        print("completed M6 calibrated-event run: {0}".format(run_directory))
    except Exception as error:
        metadata["status"] = "failed"
        metadata["completed_utc"] = utc_now()
        metadata["error"] = str(error)
        write_json_atomic(metadata_path, metadata)
        raise


def run_m7(args):
    verify_summary = verify_manifests()
    build_directory = Path(args.build_dir).expanduser().resolve()
    executable = build_directory / "build_neighbor_time_diagnostics"
    if not executable.is_file():
        raise RuntimeError(
            "missing executable {0}; run the 'check' command first".format(executable)
        )
    manifest_path, rows = read_m6_run_groups(args.run_manifest)
    input_paths = resolve_m7_inputs(args.input_dir, rows)
    run_directory = create_run_directory(args.results_dir, "m7", args.run_id)
    output_root = run_directory / "neighbor_time_diagnostics.root"
    report_path = run_directory / "run_report.tsv"
    config_path = run_directory / "config_used.txt"
    log_path = run_directory / "run.log"
    metadata_path = run_directory / "run_metadata.json"
    metadata = {
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "stage": "m7",
        "status": "running",
        "started_utc": utc_now(),
        "repository_root": str(REPOSITORY_ROOT),
        "run_directory": str(run_directory),
        "input_file_count": len(input_paths),
        "input_hash_mode": "sha256" if args.hash_inputs else "size-and-mtime",
        "environment": environment_record(),
        "provenance": provenance_record(),
        "manifest_verification": verify_summary,
        "executable": {"path": str(executable), "sha256": sha256_file(executable)},
    }
    write_json_atomic(metadata_path, metadata)
    try:
        _, config_output = run_command([str(executable), "--print-config"])
        write_text_atomic(
            config_path,
            config_output + "run_manifest=" + str(manifest_path) + "\n",
        )
        recorded_inputs = [("run_manifest", manifest_path)] + input_paths
        metadata["inputs"] = write_input_manifest(
            run_directory / "input_manifest.tsv", recorded_inputs, args.hash_inputs
        )
        command = [str(executable)]
        for _, path in input_paths:
            command.extend(["--input", str(path)])
        command.extend(
            [
                "--output", str(output_root),
                "--report", str(report_path),
            ]
        )
        metadata["command"] = command
        return_code, output = run_command(command, check=False)
        write_text_atomic(log_path, output)
        metadata["return_code"] = return_code
        if return_code != 0:
            raise CommandFailure(command, return_code, output)
        metadata["outputs"] = output_records(
            [
                output_root,
                report_path,
                config_path,
                log_path,
                run_directory / "input_manifest.tsv",
            ]
        )
        metadata["status"] = "completed"
        metadata["completed_utc"] = utc_now()
        write_json_atomic(metadata_path, metadata)
        print("completed M7 neighboring-time diagnostic run: {0}".format(run_directory))
    except Exception as error:
        metadata["status"] = "failed"
        metadata["completed_utc"] = utc_now()
        metadata["error"] = str(error)
        write_json_atomic(metadata_path, metadata)
        raise


def resolve_explicit_inputs(paths, stage):
    """Resolve an ordered list of explicit ROOT files without wildcard expansion."""
    resolved = []
    seen = set()
    for item in paths:
        path = Path(item).expanduser().resolve()
        if not path.is_file():
            raise RuntimeError("missing {0} input ROOT file: {1}".format(stage, path))
        if path in seen:
            raise RuntimeError("duplicate {0} input ROOT file: {1}".format(stage, path))
        seen.add(path)
        resolved.append(("calibrated_event_tree", path))
    return resolved


def run_m8(args):
    """Apply the migrated shower reconstruction to explicit M6 event trees.

    Run-group discovery and beam-on/beam-off orchestration remain an M9 concern;
    this entry deliberately accepts only explicit files so every recorded input is
    unambiguous.
    """
    verify_summary = verify_manifests()
    build_directory = Path(args.build_dir).expanduser().resolve()
    executable = build_directory / "build_reconstructed_event_tree"
    if not executable.is_file():
        raise RuntimeError(
            "missing executable {0}; run the 'check' command first".format(executable)
        )
    input_paths = resolve_explicit_inputs(args.input, "M8")
    run_directory = create_run_directory(args.results_dir, "m8", args.run_id)
    output_root = run_directory / "reconstructed_events.root"
    report_path = run_directory / "run_report.tsv"
    config_path = run_directory / "config_used.txt"
    log_path = run_directory / "run.log"
    metadata_path = run_directory / "run_metadata.json"
    metadata = {
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "stage": "m8",
        "status": "running",
        "started_utc": utc_now(),
        "repository_root": str(REPOSITORY_ROOT),
        "run_directory": str(run_directory),
        "input_file_count": len(input_paths),
        "input_hash_mode": "sha256" if args.hash_inputs else "size-and-mtime",
        "environment": environment_record(),
        "provenance": provenance_record(),
        "manifest_verification": verify_summary,
        "executable": {"path": str(executable), "sha256": sha256_file(executable)},
    }
    write_json_atomic(metadata_path, metadata)
    try:
        _, config_output = run_command([str(executable), "--print-config"])
        write_text_atomic(config_path, config_output)
        metadata["inputs"] = write_input_manifest(
            run_directory / "input_manifest.tsv", input_paths, args.hash_inputs
        )
        command = [str(executable)]
        for _, path in input_paths:
            command.extend(["--input", str(path)])
        command.extend(
            ["--output", str(output_root), "--report", str(report_path)]
        )
        metadata["command"] = command
        return_code, output = run_command(command, check=False)
        write_text_atomic(log_path, output)
        metadata["return_code"] = return_code
        if return_code != 0:
            raise CommandFailure(command, return_code, output)
        metadata["outputs"] = output_records(
            [
                output_root,
                report_path,
                config_path,
                log_path,
                run_directory / "input_manifest.tsv",
            ]
        )
        metadata["status"] = "completed"
        metadata["completed_utc"] = utc_now()
        write_json_atomic(metadata_path, metadata)
        print("completed M8 shower-reconstruction run: {0}".format(run_directory))
    except Exception as error:
        metadata["status"] = "failed"
        metadata["completed_utc"] = utc_now()
        metadata["error"] = str(error)
        write_json_atomic(metadata_path, metadata)
        raise


def run_m9(args):
    """Reconstruct and merge the reviewed beam-on and beam-off run groups."""
    verify_summary = verify_manifests()
    build_directory = Path(args.build_dir).expanduser().resolve()
    reconstruction_executable = build_directory / "build_reconstructed_event_tree"
    merge_executable = build_directory / "merge_reconstructed_spectra"
    for executable in (reconstruction_executable, merge_executable):
        if not executable.is_file():
            raise RuntimeError(
                "missing executable {0}; run the 'check' command first".format(
                    executable
                )
            )

    beam_on_manifest, beam_on_rows = read_m6_run_groups(
        args.beam_on_run_manifest
    )
    beam_off_manifest, beam_off_rows = read_m6_run_groups(
        args.beam_off_run_manifest
    )
    beam_on_inputs = resolve_manifest_outputs(
        args.beam_on_input_dir, beam_on_rows, "calibrated_event_tree:beam-on", "M9"
    )
    beam_off_inputs = resolve_manifest_outputs(
        args.beam_off_input_dir,
        beam_off_rows,
        "calibrated_event_tree:beam-off",
        "M9",
    )
    samples = (
        ("beam-on", beam_on_manifest, beam_on_rows, beam_on_inputs,
         "all_notree.root", "all_recon.root"),
        ("beam-off", beam_off_manifest, beam_off_rows, beam_off_inputs,
         "all_notree_BKG.root", "all_recon_BKG.root"),
    )

    run_directory = create_run_directory(args.results_dir, "m9", args.run_id)
    config_path = run_directory / "config_used.txt"
    log_path = run_directory / "run.log"
    metadata_path = run_directory / "run_metadata.json"
    metadata = {
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "stage": "m9",
        "status": "running",
        "started_utc": utc_now(),
        "repository_root": str(REPOSITORY_ROOT),
        "run_directory": str(run_directory),
        "beam_on_run_group_count": len(beam_on_rows),
        "beam_off_run_group_count": len(beam_off_rows),
        "input_hash_mode": "sha256" if args.hash_inputs else "size-and-mtime",
        "environment": environment_record(),
        "provenance": provenance_record(),
        "manifest_verification": verify_summary,
        "executables": {
            "reconstruction": {
                "path": str(reconstruction_executable),
                "sha256": sha256_file(reconstruction_executable),
            },
            "merge": {
                "path": str(merge_executable),
                "sha256": sha256_file(merge_executable),
            },
        },
        "commands": [],
    }
    write_json_atomic(metadata_path, metadata)
    try:
        _, reconstruction_config = run_command(
            [str(reconstruction_executable), "--print-config"]
        )
        _, merge_config = run_command([str(merge_executable), "--print-config"])
        write_text_atomic(
            config_path,
            "[M8 per-run reconstruction]\n"
            + reconstruction_config
            + "[M9 spectrum merge]\n"
            + merge_config
            + "beam_on_run_manifest="
            + str(beam_on_manifest)
            + "\nbeam_off_run_manifest="
            + str(beam_off_manifest)
            + "\n",
        )
        recorded_inputs = [
            ("run_manifest:beam-on", beam_on_manifest),
            ("run_manifest:beam-off", beam_off_manifest),
        ] + beam_on_inputs + beam_off_inputs
        metadata["inputs"] = write_input_manifest(
            run_directory / "input_manifest.tsv", recorded_inputs, args.hash_inputs
        )

        log_sections = []
        expected_outputs = [config_path, run_directory / "input_manifest.tsv"]
        for (sample_role, manifest_path, rows, input_paths,
             per_crystal_name, merged_name) in samples:
            sample_directory = run_directory / sample_role
            event_directory = sample_directory / "reconstructed_runs"
            report_directory = sample_directory / "run_reports"
            event_directory.mkdir(parents=True)
            report_directory.mkdir(parents=True)
            reconstructed_paths = []
            for row, (_, input_path) in zip(rows, input_paths):
                output_path = event_directory / row["output_name"]
                report_path = report_directory / (row["run_id"] + ".tsv")
                command = [
                    str(reconstruction_executable),
                    "--input", str(input_path),
                    "--output", str(output_path),
                    "--report", str(report_path),
                ]
                metadata["commands"].append(command)
                return_code, output = run_command(command, check=False)
                log_sections.append(
                    "===== {0}:{1} =====\n{2}".format(
                        sample_role, row["run_id"], output
                    )
                )
                if return_code != 0:
                    write_text_atomic(log_path, "\n".join(log_sections))
                    raise CommandFailure(command, return_code, output)
                reconstructed_paths.append(output_path)
                expected_outputs.extend([output_path, report_path])

            per_crystal_output = sample_directory / per_crystal_name
            merged_output = sample_directory / merged_name
            merge_report = sample_directory / "merge_report.tsv"
            merge_command = [str(merge_executable)]
            for path in reconstructed_paths:
                merge_command.extend(["--input", str(path)])
            merge_command.extend(
                [
                    "--per-crystal-output", str(per_crystal_output),
                    "--output", str(merged_output),
                    "--sample-role", sample_role,
                    "--report", str(merge_report),
                ]
            )
            metadata["commands"].append(merge_command)
            return_code, output = run_command(merge_command, check=False)
            log_sections.append(
                "===== {0}:merge =====\n{1}".format(sample_role, output)
            )
            if return_code != 0:
                write_text_atomic(log_path, "\n".join(log_sections))
                raise CommandFailure(merge_command, return_code, output)
            expected_outputs.extend(
                [per_crystal_output, merged_output, merge_report]
            )

        write_text_atomic(log_path, "\n".join(log_sections))
        expected_outputs.append(log_path)
        metadata["outputs"] = output_records(expected_outputs)
        metadata["status"] = "completed"
        metadata["completed_utc"] = utc_now()
        write_json_atomic(metadata_path, metadata)
        print("completed M9 run-group reconstruction and merge: {0}".format(
            run_directory
        ))
    except Exception as error:
        metadata["status"] = "failed"
        metadata["completed_utc"] = utc_now()
        metadata["error"] = str(error)
        write_json_atomic(metadata_path, metadata)
        raise


def run_reconstruction_spectra(args):
    """Merge the exact historical 59-group M8 sample for the spectrum figure."""
    verify_summary = verify_manifests()
    build_directory = Path(args.build_dir).expanduser().resolve()
    merge_executable = build_directory / "merge_reconstructed_spectra"
    if not merge_executable.is_file():
        raise RuntimeError(
            "missing executable {0}; run the 'check' command first".format(
                merge_executable
            )
        )

    figure_manifest_summary = verify_reconstruction_spectra_figure_manifest(
        args.run_manifest
    )
    run_manifest, rows = read_m6_run_groups(args.run_manifest)
    input_paths = resolve_manifest_outputs(
        args.reconstructed_run_dir,
        rows,
        "reconstructed_event_tree:figure",
        "reconstruction-spectra",
    )
    run_directory = create_run_directory(
        args.results_dir, "reconstruction-spectra", args.run_id
    )
    config_path = run_directory / "config_used.txt"
    log_path = run_directory / "run.log"
    metadata_path = run_directory / "run_metadata.json"
    per_crystal_output = run_directory / "all_notree_figure.root"
    merged_output = run_directory / "all_recon_figure.root"
    report_path = run_directory / "merge_report.tsv"
    metadata = {
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "stage": "reconstruction-spectra",
        "status": "running",
        "started_utc": utc_now(),
        "repository_root": str(REPOSITORY_ROOT),
        "run_directory": str(run_directory),
        "run_group_count": len(rows),
        "input_hash_mode": "sha256" if args.hash_inputs else "size-and-mtime",
        "environment": environment_record(),
        "provenance": provenance_record(),
        "manifest_verification": verify_summary,
        "figure_manifest_verification": figure_manifest_summary,
        "executable": {
            "path": str(merge_executable),
            "sha256": sha256_file(merge_executable),
        },
    }
    write_json_atomic(metadata_path, metadata)
    try:
        _, merge_config = run_command([str(merge_executable), "--print-config"])
        write_text_atomic(
            config_path,
            merge_config
            + "run_manifest="
            + str(run_manifest)
            + "\nfigure_sample=59 reviewed March 5--10 run groups\n",
        )
        recorded_inputs = [("run_manifest", run_manifest)] + input_paths
        metadata["inputs"] = write_input_manifest(
            run_directory / "input_manifest.tsv",
            recorded_inputs,
            args.hash_inputs,
        )
        command = [str(merge_executable)]
        for _, path in input_paths:
            command.extend(["--input", str(path)])
        command.extend(
            [
                "--per-crystal-output",
                str(per_crystal_output),
                "--output",
                str(merged_output),
                "--sample-role",
                "beam-on",
                "--report",
                str(report_path),
            ]
        )
        metadata["command"] = command
        return_code, output = run_command(command, check=False)
        write_text_atomic(log_path, output)
        metadata["return_code"] = return_code
        if return_code != 0:
            raise CommandFailure(command, return_code, output)
        metadata["outputs"] = output_records(
            [
                per_crystal_output,
                merged_output,
                report_path,
                config_path,
                log_path,
                run_directory / "input_manifest.tsv",
            ]
        )
        metadata["status"] = "completed"
        metadata["completed_utc"] = utc_now()
        write_json_atomic(metadata_path, metadata)
        print(
            "completed historical reconstruction-spectrum merge: {0}".format(
                run_directory
            )
        )
    except Exception as error:
        metadata["status"] = "failed"
        metadata["completed_utc"] = utc_now()
        metadata["error"] = str(error)
        write_json_atomic(metadata_path, metadata)
        raise


def run_m10(args):
    """Build reviewed beam-on/off Chapter 3 diagnostic ROOT objects."""
    verify_summary = verify_manifests()
    build_directory = Path(args.build_dir).expanduser().resolve()
    executable = build_directory / "build_chapter3_diagnostics"
    if not executable.is_file():
        raise RuntimeError(
            "missing executable {0}; run the 'check' command first".format(
                executable
            )
        )

    beam_on_manifest, beam_on_rows = read_m6_run_groups(
        args.beam_on_run_manifest
    )
    beam_off_manifest, beam_off_rows = read_m6_run_groups(
        args.beam_off_run_manifest
    )
    beam_on_inputs = resolve_manifest_outputs(
        args.beam_on_input_dir,
        beam_on_rows,
        "reconstructed_event_tree:beam-on",
        "M10",
    )
    beam_off_inputs = resolve_manifest_outputs(
        args.beam_off_input_dir,
        beam_off_rows,
        "reconstructed_event_tree:beam-off",
        "M10",
    )
    samples = (
        ("beam-on", beam_on_inputs, "h2_check.root", "h2_check.run.tsv"),
        ("beam-off", beam_off_inputs, "h2_check_BKG.root", "h2_check_BKG.run.tsv"),
    )

    run_directory = create_run_directory(args.results_dir, "m10", args.run_id)
    config_path = run_directory / "config_used.txt"
    log_path = run_directory / "run.log"
    metadata_path = run_directory / "run_metadata.json"
    metadata = {
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "stage": "m10a",
        "status": "running",
        "started_utc": utc_now(),
        "repository_root": str(REPOSITORY_ROOT),
        "run_directory": str(run_directory),
        "beam_on_run_group_count": len(beam_on_rows),
        "beam_off_run_group_count": len(beam_off_rows),
        "input_hash_mode": "sha256" if args.hash_inputs else "size-and-mtime",
        "environment": environment_record(),
        "provenance": provenance_record(),
        "manifest_verification": verify_summary,
        "executable": {
            "path": str(executable),
            "sha256": sha256_file(executable),
        },
        "commands": [],
    }
    write_json_atomic(metadata_path, metadata)
    try:
        _, configuration = run_command([str(executable), "--print-config"])
        write_text_atomic(
            config_path,
            configuration
            + "beam_on_run_manifest=" + str(beam_on_manifest) + "\n"
            + "beam_off_run_manifest=" + str(beam_off_manifest) + "\n",
        )
        recorded_inputs = [
            ("run_manifest:beam-on", beam_on_manifest),
            ("run_manifest:beam-off", beam_off_manifest),
        ] + beam_on_inputs + beam_off_inputs
        metadata["inputs"] = write_input_manifest(
            run_directory / "input_manifest.tsv", recorded_inputs,
            args.hash_inputs
        )

        log_sections = []
        expected_outputs = [config_path, run_directory / "input_manifest.tsv"]
        for sample_role, input_paths, output_name, report_name in samples:
            sample_directory = run_directory / sample_role
            sample_directory.mkdir(parents=True)
            output_path = sample_directory / output_name
            report_path = sample_directory / report_name
            command = [str(executable)]
            for _, path in input_paths:
                command.extend(["--input", str(path)])
            command.extend(
                [
                    "--output", str(output_path),
                    "--report", str(report_path),
                    "--sample-role", sample_role,
                ]
            )
            metadata["commands"].append(command)
            return_code, output = run_command(command, check=False)
            log_sections.append(
                "===== {0} =====\n{1}".format(sample_role, output)
            )
            if return_code != 0:
                write_text_atomic(log_path, "\n".join(log_sections))
                raise CommandFailure(command, return_code, output)
            expected_outputs.extend([output_path, report_path])

        write_text_atomic(log_path, "\n".join(log_sections))
        expected_outputs.append(log_path)
        metadata["outputs"] = output_records(expected_outputs)
        metadata["status"] = "completed"
        metadata["completed_utc"] = utc_now()
        write_json_atomic(metadata_path, metadata)
        print("completed M10A Chapter 3 diagnostics: {0}".format(run_directory))
    except Exception as error:
        metadata["status"] = "failed"
        metadata["completed_utc"] = utc_now()
        metadata["error"] = str(error)
        write_json_atomic(metadata_path, metadata)
        raise


def run_m10b(args):
    """Build trigger-monitor and trigger-conditioned diagnostic objects."""
    verify_summary = verify_manifests()
    build_directory = Path(args.build_dir).expanduser().resolve()
    executable = build_directory / "build_trigger_diagnostics"
    if not executable.is_file():
        raise RuntimeError(
            "missing executable {0}; run the 'check' command first".format(
                executable
            )
        )

    run_manifest, rows = read_m6_run_groups(args.beam_on_run_manifest)
    inputs = resolve_manifest_outputs(
        args.beam_on_input_dir, rows, "reconstructed_event_tree:beam-on", "M10B"
    )
    run_directory = create_run_directory(args.results_dir, "m10b", args.run_id)
    config_path = run_directory / "config_used.txt"
    log_path = run_directory / "run.log"
    metadata_path = run_directory / "run_metadata.json"
    output_path = run_directory / "trigger_diagnostics.root"
    report_path = run_directory / "trigger_diagnostics.run.tsv"
    metadata = {
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "stage": "m10b",
        "status": "running",
        "started_utc": utc_now(),
        "repository_root": str(REPOSITORY_ROOT),
        "run_directory": str(run_directory),
        "beam_on_run_group_count": len(rows),
        "input_hash_mode": "sha256" if args.hash_inputs else "size-and-mtime",
        "environment": environment_record(),
        "provenance": provenance_record(),
        "manifest_verification": verify_summary,
        "executable": {"path": str(executable), "sha256": sha256_file(executable)},
        "commands": [],
    }
    write_json_atomic(metadata_path, metadata)
    try:
        _, configuration = run_command([str(executable), "--print-config"])
        write_text_atomic(
            config_path,
            configuration + "beam_on_run_manifest=" + str(run_manifest) + "\n",
        )
        recorded_inputs = [("run_manifest:beam-on", run_manifest)] + inputs
        metadata["inputs"] = write_input_manifest(
            run_directory / "input_manifest.tsv", recorded_inputs,
            args.hash_inputs
        )
        command = [str(executable)]
        for _, path in inputs:
            command.extend(["--input", str(path)])
        command.extend(
            ["--output", str(output_path), "--report", str(report_path)]
        )
        metadata["commands"].append(command)
        return_code, output = run_command(command, check=False)
        write_text_atomic(log_path, output)
        if return_code != 0:
            raise CommandFailure(command, return_code, output)
        expected_outputs = [
            config_path, run_directory / "input_manifest.tsv", output_path,
            report_path, log_path
        ]
        metadata["outputs"] = output_records(expected_outputs)
        metadata["status"] = "completed"
        metadata["completed_utc"] = utc_now()
        write_json_atomic(metadata_path, metadata)
        print("completed M10B trigger diagnostics: {0}".format(run_directory))
    except Exception as error:
        metadata["status"] = "failed"
        metadata["completed_utc"] = utc_now()
        metadata["error"] = str(error)
        write_json_atomic(metadata_path, metadata)
        raise


def run_m11(args):
    """Build slow-beam-off and fast/random-window observed spectra."""
    verify_summary = verify_manifests()
    build_directory = Path(args.build_dir).expanduser().resolve()
    fast_executable = build_directory / "build_fast_coincidence_spectra"
    spectrum_executable = build_directory / "build_observed_spectrum"
    for executable in (fast_executable, spectrum_executable):
        if not executable.is_file():
            raise RuntimeError(
                "missing executable {0}; run the 'check' command first".format(
                    executable
                )
            )

    slow_signal = Path(args.slow_signal).expanduser().resolve()
    slow_background = Path(args.slow_background).expanduser().resolve()
    for label, path in (("slow signal", slow_signal),
                        ("slow background", slow_background)):
        if not path.is_file():
            raise RuntimeError("missing {0} ROOT file: {1}".format(label, path))
    run_manifest, rows = read_m6_run_groups(args.beam_on_run_manifest)
    fast_inputs = resolve_manifest_outputs(
        args.beam_on_input_dir, rows, "reconstructed_event_tree:beam-on", "M11"
    )

    run_directory = create_run_directory(args.results_dir, "m11", args.run_id)
    slow_directory = run_directory / "slow"
    fast_directory = run_directory / "fast"
    slow_directory.mkdir()
    fast_directory.mkdir()
    config_path = run_directory / "config_used.txt"
    log_path = run_directory / "run.log"
    metadata_path = run_directory / "run_metadata.json"
    fast_signal = fast_directory / "fast_window.root"
    fast_random = fast_directory / "random_window.root"
    fast_report = fast_directory / "window_spectra.run.tsv"
    slow_spectrum = slow_directory / "spectrum_110.root"
    slow_report = slow_directory / "spectrum_110.run.tsv"
    fast_spectrum = fast_directory / "spectrum_110.root"
    fast_spectrum_report = fast_directory / "spectrum_110.run.tsv"
    metadata = {
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "stage": "m11",
        "status": "running",
        "started_utc": utc_now(),
        "repository_root": str(REPOSITORY_ROOT),
        "run_directory": str(run_directory),
        "beam_on_run_group_count": len(rows),
        "input_hash_mode": "sha256" if args.hash_inputs else "size-and-mtime",
        "environment": environment_record(),
        "provenance": provenance_record(),
        "manifest_verification": verify_summary,
        "executables": [
            {"path": str(fast_executable), "sha256": sha256_file(fast_executable)},
            {"path": str(spectrum_executable),
             "sha256": sha256_file(spectrum_executable)},
        ],
        "commands": [],
    }
    write_json_atomic(metadata_path, metadata)
    try:
        _, fast_configuration = run_command(
            [str(fast_executable), "--print-config"]
        )
        _, spectrum_configuration = run_command(
            [str(spectrum_executable), "--print-config"]
        )
        write_text_atomic(
            config_path,
            fast_configuration + spectrum_configuration
            + "beam_on_run_manifest=" + str(run_manifest) + "\n",
        )
        recorded_inputs = [
            ("run_manifest:beam-on", run_manifest),
            ("merged-spectrum:slow-signal", slow_signal),
            ("merged-spectrum:slow-background", slow_background),
        ] + fast_inputs
        metadata["inputs"] = write_input_manifest(
            run_directory / "input_manifest.tsv", recorded_inputs,
            args.hash_inputs
        )

        fast_command = [str(fast_executable)]
        for _, path in fast_inputs:
            fast_command.extend(["--input", str(path)])
        fast_command.extend([
            "--signal-output", str(fast_signal),
            "--random-output", str(fast_random),
            "--report", str(fast_report),
        ])
        slow_command = [
            str(spectrum_executable), "--signal", str(slow_signal),
            "--background", str(slow_background), "--mode", "slow",
            "--output", str(slow_spectrum), "--report", str(slow_report),
        ]
        fast_spectrum_command = [
            str(spectrum_executable), "--signal", str(fast_signal),
            "--background", str(fast_random), "--mode", "fast",
            "--output", str(fast_spectrum), "--report",
            str(fast_spectrum_report),
        ]
        log_sections = []
        for label, command in (("fast-windows", fast_command),
                               ("slow-spectrum", slow_command),
                               ("fast-spectrum", fast_spectrum_command)):
            metadata["commands"].append(command)
            return_code, output = run_command(command, check=False)
            log_sections.append("===== {0} =====\n{1}".format(label, output))
            if return_code != 0:
                write_text_atomic(log_path, "\n".join(log_sections))
                raise CommandFailure(command, return_code, output)
        write_text_atomic(log_path, "\n".join(log_sections))
        expected_outputs = [
            config_path, run_directory / "input_manifest.tsv", fast_signal,
            fast_random, fast_report, slow_spectrum, slow_report,
            fast_spectrum, fast_spectrum_report, log_path,
        ]
        metadata["outputs"] = output_records(expected_outputs)
        metadata["status"] = "completed"
        metadata["completed_utc"] = utc_now()
        write_json_atomic(metadata_path, metadata)
        print("completed M11 observed spectra: {0}".format(run_directory))
    except Exception as error:
        metadata["status"] = "failed"
        metadata["completed_utc"] = utc_now()
        metadata["error"] = str(error)
        write_json_atomic(metadata_path, metadata)
        raise


def run_m12(args):
    """Validate and record the detector-level observed-spectrum interface."""
    verify_summary = verify_manifests()
    build_directory = Path(args.build_dir).expanduser().resolve()
    executable = build_directory / "inspect_observed_spectrum"
    if not executable.is_file():
        raise RuntimeError(
            "missing executable {0}; run the 'check' command first".format(
                executable
            )
        )
    observed_spectrum = Path(args.observed_spectrum).expanduser().resolve()
    if not observed_spectrum.is_file():
        raise RuntimeError(
            "missing observed-spectrum ROOT file: {0}".format(observed_spectrum)
        )

    run_directory = create_run_directory(args.results_dir, "m12", args.run_id)
    config_path = run_directory / "config_used.txt"
    input_manifest = run_directory / "input_manifest.tsv"
    report_path = run_directory / "observed_spectrum_interface.tsv"
    log_path = run_directory / "run.log"
    metadata_path = run_directory / "run_metadata.json"
    metadata = {
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "stage": "m12",
        "status": "running",
        "started_utc": utc_now(),
        "repository_root": str(REPOSITORY_ROOT),
        "run_directory": str(run_directory),
        "input_hash_mode": "sha256" if args.hash_inputs else "size-and-mtime",
        "environment": environment_record(),
        "provenance": provenance_record(),
        "manifest_verification": verify_summary,
        "executable": {"path": str(executable), "sha256": sha256_file(executable)},
        "commands": [],
    }
    write_json_atomic(metadata_path, metadata)
    try:
        _, configuration = run_command([str(executable), "--print-config"])
        write_text_atomic(config_path, configuration)
        metadata["inputs"] = write_input_manifest(
            input_manifest,
            [("detector-level-observed-spectrum", observed_spectrum)],
            args.hash_inputs,
        )
        command = [
            str(executable), "--input", str(observed_spectrum),
            "--report", str(report_path),
        ]
        metadata["commands"].append(command)
        return_code, output = run_command(command, check=False)
        write_text_atomic(log_path, output)
        if return_code != 0:
            raise CommandFailure(command, return_code, output)
        metadata["outputs"] = output_records(
            [config_path, input_manifest, report_path, log_path]
        )
        metadata["status"] = "completed"
        metadata["completed_utc"] = utc_now()
        write_json_atomic(metadata_path, metadata)
        print("completed M12 observed-spectrum interface: {0}".format(run_directory))
    except Exception as error:
        metadata["status"] = "failed"
        metadata["completed_utc"] = utc_now()
        metadata["error"] = str(error)
        write_json_atomic(metadata_path, metadata)
        raise


def run_check(args):
    snapshot_root = Path(args.snapshot_root) if args.snapshot_root else None
    verify_summary = verify_manifests(snapshot_root)
    build_directory = Path(args.build_dir).expanduser().resolve()
    build_directory.mkdir(parents=True, exist_ok=True)
    metadata_path = build_directory / "check_metadata.json"
    metadata = {
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "stage": "m0-m12-check",
        "status": "running",
        "started_utc": utc_now(),
        "environment": environment_record(),
        "provenance": provenance_record(),
        "manifest_verification": verify_summary,
        "commands": [],
    }
    write_json_atomic(metadata_path, metadata)
    try:
        configure = ["cmake", str(ANALYSIS_SOURCE)]
        if args.root_dir:
            configure.append("-DROOT_DIR={0}".format(Path(args.root_dir).resolve()))
        commands = [
            [
                sys.executable,
                "-m",
                "unittest",
                "discover",
                "-s",
                str(TOOLS_TEST_DIRECTORY),
            ],
            configure,
            ["cmake", "--build", "."],
            ["ctest", "--output-on-failure"],
        ]
        for command in commands:
            metadata["commands"].append([str(item) for item in command])
            run_command(command, cwd=build_directory)
        metadata["status"] = "completed"
        metadata["completed_utc"] = utc_now()
        write_json_atomic(metadata_path, metadata)
        print("M0--M12 check completed: {0}".format(build_directory))
    except Exception as error:
        metadata["status"] = "failed"
        metadata["completed_utc"] = utc_now()
        metadata["error"] = str(error)
        write_json_atomic(metadata_path, metadata)
        raise


def build_parser():
    parser = argparse.ArgumentParser(
        description="Verify, build, and run migrated CSHINE-Gamma M0--M12 stages."
    )
    parser.add_argument("--version", action="version", version=TOOL_VERSION)
    subparsers = parser.add_subparsers(dest="command")

    verify_parser = subparsers.add_parser(
        "verify", help="verify portable hashes and optionally the frozen snapshot"
    )
    verify_parser.add_argument(
        "--snapshot-root",
        help="path to the frozen historical DataPreprocessing directory",
    )

    check_parser = subparsers.add_parser(
        "check", help="verify manifests, configure, build, and run synthetic tests"
    )
    check_parser.add_argument(
        "--build-dir", default=str(DEFAULT_BUILD_DIRECTORY)
    )
    check_parser.add_argument("--root-dir", help="ROOT CMake package directory")
    check_parser.add_argument(
        "--snapshot-root",
        help="also verify the frozen historical DataPreprocessing directory",
    )

    for stage in ("m2", "m3"):
        stage_parser = subparsers.add_parser(
            stage, help="run the migrated {0} analysis".format(stage.upper())
        )
        stage_parser.add_argument("--input-dir", required=True)
        stage_parser.add_argument(
            "--build-dir", default=str(DEFAULT_BUILD_DIRECTORY)
        )
        stage_parser.add_argument(
            "--results-dir", default=str(DEFAULT_RESULTS_DIRECTORY)
        )
        stage_parser.add_argument("--run-id")
        stage_parser.add_argument(
            "--hash-inputs",
            action="store_true",
            help="compute full SHA-256 values for all raw ROOT inputs",
        )

    m4_parser = subparsers.add_parser(
        "m4", help="run the migrated M4 three-point energy calibration"
    )
    m4_parser.add_argument("--source-spectra", required=True)
    m4_parser.add_argument("--gain-relation", required=True)
    m4_parser.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIRECTORY))
    m4_parser.add_argument("--results-dir", default=str(DEFAULT_RESULTS_DIRECTORY))
    m4_parser.add_argument("--run-id")
    m4_parser.add_argument(
        "--hash-inputs",
        action="store_true",
        help="compute full SHA-256 values for both ROOT inputs",
    )

    m5_parser = subparsers.add_parser(
        "m5-audit",
        help="audit historical M5 time-fit artifacts without refitting",
    )
    m5_parser.add_argument("--fits-dir", required=True)
    m5_parser.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIRECTORY))
    m5_parser.add_argument("--results-dir", default=str(DEFAULT_RESULTS_DIRECTORY))
    m5_parser.add_argument("--run-id")
    m5_parser.add_argument("--tolerance", type=float, default=1.0e-9)
    m5_parser.add_argument(
        "--hash-inputs",
        action="store_true",
        help="compute SHA-256 values for the 30 small historical fit files",
    )

    m5_spectra_parser = subparsers.add_parser(
        "m5-spectra",
        help="build M5 original or historical-diagnostic time-amplitude spectra",
    )
    m5_spectra_parser.add_argument("--input-dir", required=True)
    m5_spectra_parser.add_argument(
        "--mode",
        choices=("original", "historical-corrected"),
        default="original",
    )
    m5_spectra_parser.add_argument("--threads", type=int, default=12)
    m5_spectra_parser.add_argument(
        "--build-dir", default=str(DEFAULT_BUILD_DIRECTORY)
    )
    m5_spectra_parser.add_argument(
        "--results-dir", default=str(DEFAULT_RESULTS_DIRECTORY)
    )
    m5_spectra_parser.add_argument("--run-id")
    m5_spectra_parser.add_argument(
        "--hash-inputs",
        action="store_true",
        help="compute SHA-256 values for all matched raw ROOT inputs",
    )

    m6_parser = subparsers.add_parser(
        "m6", help="produce calibrated event trees for manifest-defined run groups"
    )
    m6_parser.add_argument("--input-dir", required=True)
    m6_parser.add_argument("--calibration", required=True)
    m6_parser.add_argument(
        "--run-manifest", default=str(DEFAULT_M6_RUN_MANIFEST)
    )
    m6_parser.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIRECTORY))
    m6_parser.add_argument("--results-dir", default=str(DEFAULT_RESULTS_DIRECTORY))
    m6_parser.add_argument("--run-id")
    m6_parser.add_argument(
        "--hash-inputs",
        action="store_true",
        help="compute SHA-256 values for calibration, manifest, and all raw inputs",
    )

    m7_parser = subparsers.add_parser(
        "m7", help="produce CsI05--CsI06 neighboring-time diagnostics"
    )
    m7_parser.add_argument(
        "--input-dir",
        required=True,
        help="directory containing the manifest-defined M6 calibrated event trees",
    )
    m7_parser.add_argument(
        "--run-manifest", default=str(DEFAULT_M6_RUN_MANIFEST)
    )
    m7_parser.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIRECTORY))
    m7_parser.add_argument("--results-dir", default=str(DEFAULT_RESULTS_DIRECTORY))
    m7_parser.add_argument("--run-id")
    m7_parser.add_argument(
        "--hash-inputs",
        action="store_true",
        help="compute SHA-256 values for the manifest and all calibrated trees",
    )

    m8_parser = subparsers.add_parser(
        "m8", help="apply shower reconstruction to explicit calibrated event trees"
    )
    m8_parser.add_argument(
        "--input",
        action="append",
        required=True,
        help="explicit M6 calibrated event tree; repeat for additional files",
    )

    m9_parser = subparsers.add_parser(
        "m9", help="reconstruct and merge reviewed beam-on and beam-off run groups"
    )
    m9_parser.add_argument(
        "--beam-on-input-dir",
        required=True,
        help="directory containing the 60 manifest-defined M6 beam-on trees",
    )
    m9_parser.add_argument(
        "--beam-off-input-dir",
        required=True,
        help="directory containing the six manifest-defined M6 beam-off trees",
    )
    m9_parser.add_argument(
        "--beam-on-run-manifest", default=str(DEFAULT_M6_RUN_MANIFEST)
    )
    m9_parser.add_argument(
        "--beam-off-run-manifest", default=str(DEFAULT_M9_BEAM_OFF_RUN_MANIFEST)
    )
    m9_parser.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIRECTORY))
    m9_parser.add_argument("--results-dir", default=str(DEFAULT_RESULTS_DIRECTORY))
    m9_parser.add_argument("--run-id")
    m9_parser.add_argument(
        "--hash-inputs",
        action="store_true",
        help="compute SHA-256 values for manifests and all M6 input trees",
    )

    reconstruction_spectra_parser = subparsers.add_parser(
        "reconstruction-spectra",
        help="merge the exact 59-group M8 sample used by the historical per-crystal spectrum figure",
    )
    reconstruction_spectra_parser.add_argument(
        "--reconstructed-run-dir",
        required=True,
        help="directory containing manifest-defined per-run M8 ROOT outputs",
    )
    reconstruction_spectra_parser.add_argument(
        "--run-manifest",
        default=str(DEFAULT_RECONSTRUCTION_SPECTRA_FIGURE_MANIFEST),
    )
    reconstruction_spectra_parser.add_argument(
        "--build-dir", default=str(DEFAULT_BUILD_DIRECTORY)
    )
    reconstruction_spectra_parser.add_argument(
        "--results-dir", default=str(DEFAULT_RESULTS_DIRECTORY)
    )
    reconstruction_spectra_parser.add_argument("--run-id")
    reconstruction_spectra_parser.add_argument(
        "--hash-inputs",
        action="store_true",
        help="compute SHA-256 values for the manifest and all M8 input files",
    )

    m10_parser = subparsers.add_parser(
        "m10", help="produce reviewed beam-on and beam-off Chapter 3 diagnostics"
    )
    m10_parser.add_argument(
        "--beam-on-input-dir",
        required=True,
        help="M9 beam-on reconstructed_runs directory",
    )
    m10_parser.add_argument(
        "--beam-off-input-dir",
        required=True,
        help="M9 beam-off reconstructed_runs directory",
    )
    m10_parser.add_argument(
        "--beam-on-run-manifest", default=str(DEFAULT_M6_RUN_MANIFEST)
    )
    m10_parser.add_argument(
        "--beam-off-run-manifest", default=str(DEFAULT_M9_BEAM_OFF_RUN_MANIFEST)
    )
    m10_parser.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIRECTORY))
    m10_parser.add_argument("--results-dir", default=str(DEFAULT_RESULTS_DIRECTORY))
    m10_parser.add_argument("--run-id")
    m10_parser.add_argument(
        "--hash-inputs",
        action="store_true",
        help="compute SHA-256 values for manifests and all M8 input trees",
    )
    m10b_parser = subparsers.add_parser(
        "m10b", help="produce trigger-monitor and trigger-conditioned diagnostics"
    )
    m10b_parser.add_argument(
        "--beam-on-input-dir", required=True,
        help="M9 beam-on reconstructed_runs directory",
    )
    m10b_parser.add_argument(
        "--beam-on-run-manifest", default=str(DEFAULT_M6_RUN_MANIFEST)
    )
    m10b_parser.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIRECTORY))
    m10b_parser.add_argument("--results-dir", default=str(DEFAULT_RESULTS_DIRECTORY))
    m10b_parser.add_argument("--run-id")
    m10b_parser.add_argument(
        "--hash-inputs", action="store_true",
        help="compute SHA-256 values for the manifest and all M8 input trees",
    )

    m11_parser = subparsers.add_parser(
        "m11", help="build slow and fast-cross-check observed spectra"
    )
    m11_parser.add_argument(
        "--slow-signal", required=True,
        help="M9 beam-on all_recon.root containing h_total_E_M1",
    )
    m11_parser.add_argument(
        "--slow-background", required=True,
        help="M9 beam-off all_recon_BKG.root containing h_total_E_M1",
    )
    m11_parser.add_argument(
        "--beam-on-input-dir", required=True,
        help="M9 beam-on reconstructed_runs directory for the fast cross-check",
    )
    m11_parser.add_argument(
        "--beam-on-run-manifest", default=str(DEFAULT_M6_RUN_MANIFEST)
    )
    m11_parser.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIRECTORY))
    m11_parser.add_argument("--results-dir", default=str(DEFAULT_RESULTS_DIRECTORY))
    m11_parser.add_argument("--run-id")
    m11_parser.add_argument(
        "--hash-inputs", action="store_true",
        help="compute SHA-256 values for the manifest and all ROOT inputs",
    )
    m12_parser = subparsers.add_parser(
        "m12", help="validate the detector-level observed-spectrum interface"
    )
    m12_parser.add_argument(
        "--observed-spectrum", required=True,
        help="M11 slow-route spectrum_110.root containing TH1D histDiff",
    )
    m12_parser.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIRECTORY))
    m12_parser.add_argument("--results-dir", default=str(DEFAULT_RESULTS_DIRECTORY))
    m12_parser.add_argument("--run-id")
    m12_parser.add_argument(
        "--hash-inputs", action="store_true",
        help="compute the full SHA-256 value of the observed-spectrum ROOT file",
    )
    m8_parser.add_argument("--build-dir", default=str(DEFAULT_BUILD_DIRECTORY))
    m8_parser.add_argument("--results-dir", default=str(DEFAULT_RESULTS_DIRECTORY))
    m8_parser.add_argument("--run-id")
    m8_parser.add_argument(
        "--hash-inputs",
        action="store_true",
        help="compute SHA-256 values for all calibrated event trees",
    )

    return parser


def main():
    parser = build_parser()
    args = parser.parse_args()
    if args.command is None:
        parser.print_help()
        return 2
    try:
        if args.command == "verify":
            verify_manifests(args.snapshot_root)
        elif args.command == "check":
            run_check(args)
        elif args.command in ("m2", "m3", "m4"):
            run_stage(args, args.command)
        elif args.command == "m5-audit":
            run_m5_audit(args)
        elif args.command == "m5-spectra":
            if args.threads <= 0:
                raise ValueError("--threads must be positive")
            run_m5_spectra(args)
        elif args.command == "m6":
            run_m6(args)
        elif args.command == "m7":
            run_m7(args)
        elif args.command == "m8":
            run_m8(args)
        elif args.command == "m9":
            run_m9(args)
        elif args.command == "reconstruction-spectra":
            run_reconstruction_spectra(args)
        elif args.command == "m10":
            run_m10(args)
        elif args.command == "m10b":
            run_m10b(args)
        elif args.command == "m11":
            run_m11(args)
        elif args.command == "m12":
            run_m12(args)
        else:
            parser.error("unknown command")
    except (OSError, RuntimeError, ValueError) as error:
        print("data_preprocessing.py: {0}".format(error), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
