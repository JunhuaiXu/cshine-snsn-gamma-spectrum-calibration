#!/usr/bin/env python3
"""Build and query a read-only index of the historical source snapshot.

The index is a discovery aid.  It reports candidate files and exact textual
evidence but never assigns a physics role or selects a production version.
"""

from __future__ import print_function

import argparse
import hashlib
import json
import os
import re
import sys
from pathlib import Path


TOOL_VERSION = "0.1.0"
SCRIPT_PATH = Path(__file__).resolve()
REPRODUCIBLE_ROOT = SCRIPT_PATH.parents[1]
PROJECT_ROOT = REPRODUCIBLE_ROOT.parent
DEFAULT_SNAPSHOT_ROOT = PROJECT_ROOT / "gamma2024_code_snapshot" / "DataPreprocessing"
DEFAULT_INDEX_PATH = REPRODUCIBLE_ROOT / "results" / "source_index" / "source_index.json"

EXCLUDED_DIRECTORY_NAMES = {
    ".git",
    "__pycache__",
    ".ipynb_checkpoints",
    "build",
    "results",
}
TEXT_SUFFIXES = {
    "",
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".C",
    ".h",
    ".hh",
    ".hpp",
    ".H",
    ".py",
    ".sh",
    ".md",
    ".txt",
    ".tsv",
    ".csv",
    ".json",
    ".ipynb",
    ".cmake",
}

INCLUDE_PATTERN = re.compile(r"^\s*#\s*include\s*[<\"]([^>\"]+)[>\"]", re.MULTILINE)
BRANCH_PATTERN = re.compile(
    r"\b(?:SetBranchAddress|Branch|SetBranchStatus)\s*\(\s*[\"']([^\"']+)[\"']"
)
ROOT_OBJECT_PATTERN = re.compile(
    r"\b(?:Get|FindObject|Write|Clone|SetName)\s*\(\s*[\"']([^\"']+)[\"']"
)
HISTOGRAM_NAME_PATTERN = re.compile(
    r"\b(?:new\s+)?(?:TH[123][A-Za-z0-9_]*|TProfile[A-Za-z0-9_]*)"
    r"(?:\s+[A-Za-z_][A-Za-z0-9_]*)?\s*\(\s*[\"']([^\"']+)[\"']"
)
ROOT_FILE_PATTERN = re.compile(r"[\"']([^\"']+\.root)[\"']", re.IGNORECASE)
CLASS_PATTERN = re.compile(r"\b(?:class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)")
FUNCTION_PATTERN = re.compile(
    r"(?:^|[;{}]\s*)"
    r"(?:[A-Za-z_][A-Za-z0-9_:<>*&\s]+\s+)?"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{}]*\)\s*\{",
    re.MULTILINE,
)
CONTROL_WORDS = {"if", "for", "while", "switch", "catch"}


class IndexErrorMessage(RuntimeError):
    pass


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def read_text_file(path):
    data = path.read_bytes()
    if b"\x00" in data:
        return None, data
    try:
        text = data.decode("utf-8")
    except UnicodeDecodeError:
        text = data.decode("utf-8", errors="replace")
    return text, data


def unique_matches(pattern, text):
    return sorted({item.strip() for item in pattern.findall(text) if item.strip()})


def extract_symbols(text):
    symbols = set(CLASS_PATTERN.findall(text))
    for match in FUNCTION_PATTERN.findall(text):
        if match not in CONTROL_WORDS:
            symbols.add(match)
    return sorted(symbols)


def should_index(path):
    if any(part in EXCLUDED_DIRECTORY_NAMES for part in path.parts):
        return False
    if path.name.startswith("."):
        return False
    if path.name in {"Makefile", "GNUmakefile"}:
        return True
    return path.suffix in TEXT_SUFFIXES


def index_one_file(path, root):
    text, data = read_text_file(path)
    if text is None:
        return None
    relative = path.relative_to(root).as_posix()
    root_objects = set(unique_matches(ROOT_OBJECT_PATTERN, text))
    root_objects.update(unique_matches(HISTOGRAM_NAME_PATTERN, text))
    return {
        "path": relative,
        "size": len(data),
        "sha256": sha256_bytes(data),
        "includes": unique_matches(INCLUDE_PATTERN, text),
        "symbols": extract_symbols(text),
        "branches": unique_matches(BRANCH_PATTERN, text),
        "root_objects": sorted(root_objects),
        "root_files": unique_matches(ROOT_FILE_PATTERN, text),
    }


def build_index(snapshot_root):
    root = Path(snapshot_root).expanduser().resolve()
    if not root.is_dir():
        raise IndexErrorMessage("snapshot root is not a directory: {0}".format(root))

    files = []
    skipped_binary = 0
    for path in sorted(root.rglob("*")):
        if not path.is_file() or not should_index(path.relative_to(root)):
            continue
        record = index_one_file(path, root)
        if record is None:
            skipped_binary += 1
            continue
        files.append(record)

    hash_groups = {}
    for record in files:
        hash_groups.setdefault(record["sha256"], []).append(record["path"])
    duplicates = [
        {"sha256": digest, "paths": sorted(paths)}
        for digest, paths in sorted(hash_groups.items())
        if len(paths) > 1
    ]

    return {
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "snapshot_label": "DataPreprocessing",
        "file_count": len(files),
        "skipped_binary_count": skipped_binary,
        "duplicate_group_count": len(duplicates),
        "files": files,
        "duplicate_groups": duplicates,
    }


def write_json_atomic(path, value, force=False):
    path = Path(path).expanduser().resolve()
    if path.exists() and not force:
        raise IndexErrorMessage(
            "index already exists; use --force to replace it: {0}".format(path)
        )
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("w", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True, ensure_ascii=False)
        stream.write("\n")
    os.replace(str(temporary), str(path))
    return path


def read_index(path):
    with Path(path).expanduser().resolve().open("r", encoding="utf-8") as stream:
        return json.load(stream)


def validate_index(index):
    errors = []
    if index.get("schema_version") != 1:
        errors.append("schema_version must be 1")
    files = index.get("files")
    if not isinstance(files, list):
        return errors + ["files must be a list"]
    paths = []
    for number, record in enumerate(files):
        if not isinstance(record, dict):
            errors.append("files[{0}] must be an object".format(number))
            continue
        path = record.get("path", "")
        if not path or Path(path).is_absolute() or ".." in Path(path).parts:
            errors.append("invalid relative path in files[{0}]".format(number))
        paths.append(path)
        digest = record.get("sha256", "")
        if not re.match(r"^[0-9a-f]{64}$", digest):
            errors.append("invalid SHA-256 for {0}".format(path or number))
    duplicates = sorted({item for item in paths if paths.count(item) > 1})
    if duplicates:
        errors.append("duplicate indexed paths: {0}".format(", ".join(duplicates)))
    if index.get("file_count") != len(files):
        errors.append("file_count does not match files length")
    return errors


def searchable_text(record):
    values = [record.get("path", "")]
    for field in ("includes", "symbols", "branches", "root_objects", "root_files"):
        values.extend(record.get(field, []))
    return "\n".join(values).lower()


def query_records(index, query, field=None):
    needle = query.lower()
    matches = []
    for record in index.get("files", []):
        if field:
            value = record.get(field, [])
            if isinstance(value, list):
                haystack = "\n".join(value).lower()
            else:
                haystack = str(value).lower()
        else:
            haystack = searchable_text(record)
        if needle in haystack:
            matches.append(record)
    return matches


def print_record(record):
    print(record["path"])
    for field in ("symbols", "includes", "branches", "root_objects", "root_files"):
        values = record.get(field, [])
        if values:
            print("  {0}: {1}".format(field, ", ".join(values)))


def command_build(args):
    index = build_index(args.snapshot_root)
    errors = validate_index(index)
    if errors:
        raise IndexErrorMessage("generated index is invalid:\n  " + "\n  ".join(errors))
    output = write_json_atomic(args.output, index, force=args.force)
    print(
        "indexed {0} text files; {1} duplicate groups; output={2}".format(
            index["file_count"], index["duplicate_group_count"], output
        )
    )
    return 0


def command_check(args):
    index = read_index(args.index)
    errors = validate_index(index)
    if errors:
        raise IndexErrorMessage("index validation failed:\n  " + "\n  ".join(errors))
    print(
        "index validation passed: {0} files, {1} duplicate groups".format(
            index["file_count"], index.get("duplicate_group_count", 0)
        )
    )
    return 0


def command_summary(args):
    index = read_index(args.index)
    errors = validate_index(index)
    if errors:
        raise IndexErrorMessage("index validation failed:\n  " + "\n  ".join(errors))
    print("files\t{0}".format(index["file_count"]))
    print("duplicate_groups\t{0}".format(index.get("duplicate_group_count", 0)))
    print("skipped_binary\t{0}".format(index.get("skipped_binary_count", 0)))
    return 0


def command_query(args):
    index = read_index(args.index)
    matches = query_records(index, args.query, field=args.field)
    if args.limit < 1:
        raise IndexErrorMessage("--limit must be positive")
    for record in matches[: args.limit]:
        if args.paths_only:
            print(record["path"])
        else:
            print_record(record)
    print("matches\t{0}".format(len(matches)))
    if len(matches) > args.limit:
        print("shown\t{0}".format(args.limit))
    return 0 if matches else 1


def command_duplicates(args):
    index = read_index(args.index)
    groups = index.get("duplicate_groups", [])
    shown = 0
    for group in groups:
        if args.path_contains and not any(
            args.path_contains.lower() in path.lower() for path in group["paths"]
        ):
            continue
        print(group["sha256"])
        for path in group["paths"]:
            print("  " + path)
        shown += 1
    print("groups\t{0}".format(shown))
    return 0


def build_parser():
    parser = argparse.ArgumentParser(
        description="Build and query a read-only historical source index."
    )
    parser.add_argument("--version", action="version", version=TOOL_VERSION)
    subparsers = parser.add_subparsers(dest="command")
    subparsers.required = True

    build = subparsers.add_parser("build", help="scan a source snapshot")
    build.add_argument("--snapshot-root", default=str(DEFAULT_SNAPSHOT_ROOT))
    build.add_argument("--output", default=str(DEFAULT_INDEX_PATH))
    build.add_argument("--force", action="store_true")
    build.set_defaults(func=command_build)

    for name, function in (("check", command_check), ("summary", command_summary)):
        subparser = subparsers.add_parser(name)
        subparser.add_argument("--index", default=str(DEFAULT_INDEX_PATH))
        subparser.set_defaults(func=function)

    query = subparsers.add_parser("query", help="find candidate source files")
    query.add_argument("query")
    query.add_argument("--index", default=str(DEFAULT_INDEX_PATH))
    query.add_argument(
        "--field",
        choices=("path", "includes", "symbols", "branches", "root_objects", "root_files"),
    )
    query.add_argument("--limit", type=int, default=50)
    query.add_argument("--paths-only", action="store_true")
    query.set_defaults(func=command_query)

    duplicates = subparsers.add_parser("duplicates", help="show byte-identical files")
    duplicates.add_argument("--index", default=str(DEFAULT_INDEX_PATH))
    duplicates.add_argument("--path-contains", default="")
    duplicates.set_defaults(func=command_duplicates)
    return parser


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        return args.func(args)
    except (IndexErrorMessage, OSError, ValueError, json.JSONDecodeError) as error:
        print("error: {0}".format(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
