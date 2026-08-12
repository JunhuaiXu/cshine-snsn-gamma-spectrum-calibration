#!/usr/bin/env python3
"""Create a read-only manifest of historical CSHINE event-display files.

The audit is intentionally limited to ``ANALYSIS_ROOT/EventDisplay``.  It
records relative paths, file sizes, modification times, and SHA-256 values for
small source or output files.  It never opens ROOT trees, copies binary data,
or modifies the historical directory.
"""

from __future__ import print_function

import argparse
import datetime
import hashlib
import json
from pathlib import Path


HASH_LIMIT_BYTES = 200 * 1024 * 1024
INTERESTING_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".json",
    ".md",
    ".pdf",
    ".png",
    ".root",
    ".txt",
}


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Audit historical event-display sources without reading data."
    )
    parser.add_argument(
        "--analysis-root",
        type=Path,
        required=True,
        help="Root of the authorized gamma2024 analysis directory.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        required=True,
        help="JSON manifest to create.",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Replace an existing manifest.",
    )
    return parser.parse_args()


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def utc_timestamp(value):
    return datetime.datetime.utcfromtimestamp(value).replace(
        microsecond=0
    ).isoformat() + "Z"


def main():
    args = parse_arguments()
    analysis_root = args.analysis_root.expanduser().resolve()
    source_root = analysis_root / "EventDisplay"
    output_path = args.output.expanduser().resolve()

    if not source_root.is_dir():
        raise OSError("Historical EventDisplay directory not found: %s" % source_root)
    if output_path.exists() and not args.force:
        raise OSError("Output exists; pass --force to replace it: %s" % output_path)

    files = []
    for path in sorted(source_root.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in INTERESTING_SUFFIXES:
            continue
        stat = path.stat()
        item = {
            "path": str(path.relative_to(analysis_root)),
            "size_bytes": stat.st_size,
            "modified_utc": utc_timestamp(stat.st_mtime),
            "suffix": path.suffix.lower(),
        }
        if stat.st_size <= HASH_LIMIT_BYTES:
            item["sha256"] = sha256_file(path)
        else:
            item["sha256"] = None
            item["hash_note"] = "not hashed because file exceeds 200 MiB"
        files.append(item)

    payload = {
        "schema_version": 1,
        "scope": "EventDisplay",
        "read_only": True,
        "file_count": len(files),
        "files": files,
    }
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8") as stream:
        json.dump(payload, stream, indent=2, sort_keys=True)
        stream.write("\n")

    print("PASS event-display source audit: %d files" % len(files))
    print("manifest: %s" % output_path)


if __name__ == "__main__":
    main()
