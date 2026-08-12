#!/usr/bin/env python3
"""Create and validate machine-readable thesis-figure work records.

The tool manages provenance metadata only.  It does not execute a physics
analysis, compare histograms, approve a candidate, or modify the thesis.
"""

from __future__ import print_function

import argparse
import datetime
import hashlib
import json
import os
import re
import sys
from pathlib import Path


TOOL_VERSION = "0.1.0"
SCRIPT_PATH = Path(__file__).resolve()
REPRODUCIBLE_ROOT = SCRIPT_PATH.parents[1]
DEFAULT_RECORD_DIRECTORY = REPRODUCIBLE_ROOT / "plotting" / "records"
ID_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]*$")
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")

FIGURE_CLASSES = {
    "reproducible_redraw",
    "direct_use",
    "static_diagram",
    "literature",
}
HISTORICAL_STATUSES = {"not_started", "source_located", "source_frozen"}
CONTRACT_STATUSES = {"draft", "frozen"}
PORTABLE_STATUSES = {"not_started", "implemented", "checked"}
VALIDATION_STATUSES = {
    "not_run",
    "synthetic_checked",
    "real_data_checked",
    "numerically_validated",
}
AUTHOR_STATUSES = {"pending", "confirmed", "rejected"}
THESIS_STATUSES = {"not_integrated", "candidate", "used", "final"}
PRIVATE_MARKERS = (
    "/nas/",
    "/Users/",
    "xjh23@",
    "bobby@",
    "token=",
    "password=",
)


def utc_now():
    return datetime.datetime.utcnow().replace(microsecond=0).isoformat() + "Z"


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
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("w", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True, ensure_ascii=False)
        stream.write("\n")
    os.replace(str(temporary), str(path))


def read_json(path):
    with Path(path).open("r", encoding="utf-8") as stream:
        return json.load(stream)


def new_record(args):
    return {
        "schema_version": 1,
        "stable_id": args.stable_id,
        "title": args.title,
        "figure_class": args.figure_class,
        "created_utc": utc_now(),
        "updated_utc": utc_now(),
        "thesis": {
            "section": args.thesis_section,
            "label": args.thesis_label,
            "status": "not_integrated",
            "adopted_artifacts": [],
        },
        "historical": {
            "status": "source_located",
            "publication_or_note_figure": args.source_figure or "",
            "source_files": [
                {"path": item, "sha256": "not_recorded"}
                for item in args.historical_source
            ],
            "input_objects": [],
            "historical_outputs": [],
        },
        "physics_contract": {
            "status": "draft",
            "sample": "",
            "inputs": [],
            "variables": [],
            "selections": [],
            "binning": [],
            "normalization": "",
            "fit_or_transform": "",
            "display_policy": [],
        },
        "portable": {
            "status": "not_started",
            "preserved_root_sources": [],
            "root_entries": [],
            "python_entries": [],
            "command": "",
            "minimum_python": "3.6",
        },
        "validation": {
            "status": "not_run",
            "checks": [],
            "records": [],
            "notes": "",
        },
        "candidate": {"status": "not_generated", "artifacts": []},
        "author_decision": {"status": "pending", "date": "", "notes": ""},
        "public_boundary": {
            "contains_private_paths": False,
            "internal_path_reference": "../../../DATA_PATHS.md",
            "notes": "Experimental data and generated binary outputs are not distributed.",
        },
    }


def iter_strings(value, prefix=""):
    if isinstance(value, dict):
        for key, item in value.items():
            child = key if not prefix else prefix + "." + key
            for result in iter_strings(item, child):
                yield result
    elif isinstance(value, list):
        for index, item in enumerate(value):
            child = "{0}[{1}]".format(prefix, index)
            for result in iter_strings(item, child):
                yield result
    elif isinstance(value, str):
        yield prefix, value


def get_mapping(record, key, errors):
    value = record.get(key)
    if not isinstance(value, dict):
        errors.append("{0} must be an object".format(key))
        return {}
    return value


def require_text(mapping, key, label, errors):
    value = mapping.get(key)
    if not isinstance(value, str) or not value.strip():
        errors.append("{0}.{1} must be non-empty".format(label, key))


def require_list(mapping, key, label, errors):
    value = mapping.get(key)
    if not isinstance(value, list) or not value:
        errors.append("{0}.{1} must contain at least one item".format(label, key))


def check_status(mapping, key, allowed, label, errors):
    value = mapping.get(key)
    if value not in allowed:
        errors.append(
            "{0}.{1} has invalid value {2!r}; allowed: {3}".format(
                label, key, value, ", ".join(sorted(allowed))
            )
        )
    return value


def check_portable_paths(portable, errors):
    if portable.get("status") not in {"implemented", "checked"}:
        return
    entries = list(portable.get("root_entries", [])) + list(
        portable.get("python_entries", [])
    )
    if not entries:
        errors.append("portable implementation has no ROOT or Python entry")
        return
    for relative in entries:
        if not isinstance(relative, str) or not relative:
            errors.append("portable entry must be a non-empty relative path")
            continue
        path = Path(relative)
        if path.is_absolute():
            errors.append("portable entry must not be absolute: {0}".format(relative))
            continue
        if not (REPRODUCIBLE_ROOT / path).is_file():
            errors.append("portable entry does not exist: {0}".format(relative))


def check_artifacts(items, label, errors):
    for index, item in enumerate(items):
        prefix = "{0}[{1}]".format(label, index)
        if not isinstance(item, dict):
            errors.append("{0} must be an object".format(prefix))
            continue
        require_text(item, "role", prefix, errors)
        require_text(item, "path", prefix, errors)
        size = item.get("size_bytes")
        if not isinstance(size, int) or size < 0:
            errors.append("{0}.size_bytes must be a non-negative integer".format(prefix))
        digest = item.get("sha256", "")
        if not isinstance(digest, str) or not SHA256_PATTERN.match(digest):
            errors.append("{0}.sha256 must be a lowercase SHA-256".format(prefix))


def validate_record(record):
    errors = []
    if record.get("schema_version") != 1:
        errors.append("schema_version must be 1")

    stable_id = record.get("stable_id", "")
    if not isinstance(stable_id, str) or not ID_PATTERN.match(stable_id):
        errors.append("stable_id must match {0}".format(ID_PATTERN.pattern))
    if record.get("figure_class") not in FIGURE_CLASSES:
        errors.append("figure_class is invalid")
    if not isinstance(record.get("title"), str) or not record.get("title", "").strip():
        errors.append("title must be non-empty")

    thesis = get_mapping(record, "thesis", errors)
    historical = get_mapping(record, "historical", errors)
    contract = get_mapping(record, "physics_contract", errors)
    portable = get_mapping(record, "portable", errors)
    validation = get_mapping(record, "validation", errors)
    candidate = get_mapping(record, "candidate", errors)
    author = get_mapping(record, "author_decision", errors)
    public = get_mapping(record, "public_boundary", errors)

    require_text(thesis, "section", "thesis", errors)
    require_text(thesis, "label", "thesis", errors)
    thesis_status = check_status(
        thesis, "status", THESIS_STATUSES, "thesis", errors
    )
    historical_status = check_status(
        historical, "status", HISTORICAL_STATUSES, "historical", errors
    )
    contract_status = check_status(
        contract, "status", CONTRACT_STATUSES, "physics_contract", errors
    )
    portable_status = check_status(
        portable, "status", PORTABLE_STATUSES, "portable", errors
    )
    validation_status = check_status(
        validation, "status", VALIDATION_STATUSES, "validation", errors
    )
    author_status = check_status(
        author, "status", AUTHOR_STATUSES, "author_decision", errors
    )

    if historical_status in {"source_located", "source_frozen"}:
        require_list(historical, "source_files", "historical", errors)
    if historical_status == "source_frozen":
        for item in historical.get("source_files", []):
            if not isinstance(item, dict) or item.get("sha256") in {None, "", "not_recorded"}:
                errors.append("source_frozen requires SHA-256 for every source file")

    if contract_status == "frozen":
        require_text(contract, "sample", "physics_contract", errors)
        require_list(contract, "inputs", "physics_contract", errors)
        require_list(contract, "variables", "physics_contract", errors)
        require_list(contract, "selections", "physics_contract", errors)
        require_list(contract, "binning", "physics_contract", errors)
        require_text(contract, "normalization", "physics_contract", errors)
        require_list(contract, "display_policy", "physics_contract", errors)

    check_portable_paths(portable, errors)
    if portable_status in {"implemented", "checked"} and contract_status != "frozen":
        errors.append("portable implementation requires a frozen physics contract")

    if validation_status != "not_run":
        if contract_status != "frozen":
            errors.append("validation requires a frozen physics contract")
        if portable_status not in {"implemented", "checked"}:
            errors.append("validation requires an implemented portable entry")
        require_list(validation, "checks", "validation", errors)
        require_list(validation, "records", "validation", errors)

    if candidate.get("status") not in {"not_generated", "generated"}:
        errors.append("candidate.status must be not_generated or generated")
    if candidate.get("status") == "generated":
        require_list(candidate, "artifacts", "candidate", errors)
        check_artifacts(candidate.get("artifacts", []), "candidate.artifacts", errors)
        if record.get("figure_class") == "reproducible_redraw":
            if contract_status != "frozen":
                errors.append("generated redraw candidate requires a frozen physics contract")
            if portable_status not in {"implemented", "checked"}:
                errors.append("generated redraw candidate requires an implemented portable entry")

    if record.get("figure_class") == "reproducible_redraw" and author_status == "confirmed":
        if validation_status not in {"real_data_checked", "numerically_validated"}:
            errors.append(
                "author-confirmed redraw requires real-data or numerical validation"
            )
        if candidate.get("status") != "generated":
            errors.append("author-confirmed redraw requires a generated candidate")

    if thesis_status in {"used", "final"}:
        if author_status != "confirmed":
            errors.append("thesis use requires author_decision.status=confirmed")
        require_list(thesis, "adopted_artifacts", "thesis", errors)
        check_artifacts(
            thesis.get("adopted_artifacts", []), "thesis.adopted_artifacts", errors
        )

    if public.get("contains_private_paths") is not False:
        errors.append("public_boundary.contains_private_paths must be false")
    for location, value in iter_strings(record):
        if value.startswith("/") or value.startswith("~"):
            errors.append("absolute or home-relative path in {0}".format(location))
        if any(marker in value for marker in PRIVATE_MARKERS):
            errors.append("private path or credential marker in {0}".format(location))

    return errors


def record_paths(arguments):
    if arguments:
        return [Path(item) for item in arguments]
    return sorted(DEFAULT_RECORD_DIRECTORY.glob("*.json"))


def command_init(args):
    if not ID_PATTERN.match(args.stable_id):
        raise ValueError("stable ID must match {0}".format(ID_PATTERN.pattern))
    output = Path(args.output) if args.output else (
        DEFAULT_RECORD_DIRECTORY / (args.stable_id + ".json")
    )
    if output.exists() and not args.force:
        raise RuntimeError("record already exists; use --force only intentionally: {0}".format(output))
    record = new_record(args)
    errors = validate_record(record)
    if errors:
        raise RuntimeError("new record is invalid:\n  " + "\n  ".join(errors))
    write_json_atomic(output, record)
    print("created figure record: {0}".format(output))
    return 0


def command_check(args):
    paths = record_paths(args.records)
    if not paths:
        print("no figure records found under {0}".format(DEFAULT_RECORD_DIRECTORY))
        return 0
    failed = False
    for path in paths:
        try:
            errors = validate_record(read_json(path))
        except Exception as error:
            errors = [str(error)]
        if errors:
            failed = True
            print("FAIL {0}".format(path))
            for error in errors:
                print("  - {0}".format(error))
        else:
            print("PASS {0}".format(path))
    return 1 if failed else 0


def logical_artifact_path(path, logical_path):
    path = Path(path).expanduser().resolve()
    if logical_path:
        if Path(logical_path).is_absolute():
            raise ValueError("--logical-path must be relative")
        return logical_path
    try:
        return str(path.relative_to(REPRODUCIBLE_ROOT))
    except ValueError:
        raise ValueError(
            "artifact lies outside reproducible/; provide a public-safe --logical-path"
        )


def command_artifact(args):
    record_path = Path(args.record)
    record = read_json(record_path)
    source = Path(args.path).expanduser().resolve()
    if not source.is_file():
        raise RuntimeError("artifact does not exist: {0}".format(source))
    artifact = {
        "role": args.role,
        "path": logical_artifact_path(source, args.logical_path),
        "size_bytes": source.stat().st_size,
        "sha256": sha256_file(source),
        "recorded_utc": utc_now(),
    }
    if args.stage == "candidate":
        target = record.setdefault("candidate", {})
        target["status"] = "generated"
        target.setdefault("artifacts", []).append(artifact)
    else:
        target = record.setdefault("thesis", {})
        target.setdefault("adopted_artifacts", []).append(artifact)
    record["updated_utc"] = utc_now()
    errors = validate_record(record)
    if errors:
        raise RuntimeError(
            "artifact would leave an invalid record:\n  " + "\n  ".join(errors)
        )
    write_json_atomic(record_path, record)
    print("recorded {0} artifact: {1}".format(args.stage, artifact["path"]))
    print("sha256: {0}".format(artifact["sha256"]))
    return 0


def command_summary(args):
    paths = record_paths(args.records)
    if not paths:
        print("no figure records found under {0}".format(DEFAULT_RECORD_DIRECTORY))
        return 0
    header = ("stable_id", "source", "contract", "portable", "validation", "author", "thesis")
    print("\t".join(header))
    failed = False
    for path in paths:
        try:
            record = read_json(path)
            errors = validate_record(record)
            failed = failed or bool(errors)
            row = (
                record.get("stable_id", "?"),
                record.get("historical", {}).get("status", "?"),
                record.get("physics_contract", {}).get("status", "?"),
                record.get("portable", {}).get("status", "?"),
                record.get("validation", {}).get("status", "?"),
                record.get("author_decision", {}).get("status", "?"),
                record.get("thesis", {}).get("status", "?"),
            )
            print("\t".join(row))
        except Exception as error:
            failed = True
            print("{0}\tERROR: {1}".format(path, error))
    return 1 if failed else 0


def build_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", action="version", version=TOOL_VERSION)
    subparsers = parser.add_subparsers(dest="command")

    init_parser = subparsers.add_parser("init", help="create one staged figure record")
    init_parser.add_argument("--stable-id", required=True)
    init_parser.add_argument("--title", required=True)
    init_parser.add_argument("--figure-class", choices=sorted(FIGURE_CLASSES), default="reproducible_redraw")
    init_parser.add_argument("--thesis-section", required=True)
    init_parser.add_argument("--thesis-label", required=True)
    init_parser.add_argument("--source-figure")
    init_parser.add_argument("--historical-source", action="append", required=True)
    init_parser.add_argument("--output")
    init_parser.add_argument("--force", action="store_true")
    init_parser.set_defaults(function=command_init)

    check_parser = subparsers.add_parser("check", help="validate staged figure records")
    check_parser.add_argument("records", nargs="*")
    check_parser.set_defaults(function=command_check)

    artifact_parser = subparsers.add_parser("artifact", help="record an output checksum")
    artifact_parser.add_argument("--record", required=True)
    artifact_parser.add_argument("--stage", choices=("candidate", "thesis"), required=True)
    artifact_parser.add_argument("--role", required=True)
    artifact_parser.add_argument("--path", required=True)
    artifact_parser.add_argument("--logical-path")
    artifact_parser.set_defaults(function=command_artifact)

    summary_parser = subparsers.add_parser("summary", help="print compact staged status")
    summary_parser.add_argument("records", nargs="*")
    summary_parser.set_defaults(function=command_summary)
    return parser


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)
    if not hasattr(args, "function"):
        parser.print_help()
        return 2
    try:
        return args.function(args)
    except (OSError, ValueError, RuntimeError) as error:
        print("error: {0}".format(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
