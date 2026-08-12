#!/usr/bin/env python3
"""Copy small historical event-display PDFs into one isolated audit result.

The historical analysis directory is treated as read-only.  This helper copies
only PDF candidates below ``EventDisplay`` and writes a SHA-256 manifest next
to them; it does not open ROOT files or infer figure provenance.
"""

from __future__ import print_function

import argparse
import hashlib
import json
import shutil
from pathlib import Path


def sha256_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--analysis-root", required=True)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()

    source_root = Path(args.analysis_root).resolve() / "EventDisplay"
    output_root = Path(args.output_dir).resolve()
    output_root.mkdir(parents=True, exist_ok=True)

    records = []
    for source in sorted(source_root.rglob("*.pdf")):
        relative = source.relative_to(source_root)
        destination = output_root / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(str(source), str(destination))
        records.append(
            {
                "source_relative_path": str(Path("EventDisplay") / relative),
                "copied_relative_path": str(relative),
                "size_bytes": destination.stat().st_size,
                "sha256": sha256_file(destination),
            }
        )

    manifest = {
        "schema_version": 1,
        "read_only_source": True,
        "file_count": len(records),
        "files": records,
    }
    manifest_path = output_root / "manifest.json"
    with manifest_path.open("w", encoding="utf-8") as stream:
        json.dump(manifest, stream, indent=2, sort_keys=True)
        stream.write("\n")
    print("PASS exported {0} event-display PDF candidates".format(len(records)))


if __name__ == "__main__":
    main()
