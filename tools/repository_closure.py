#!/usr/bin/env python3
"""Audit the Chapter 3 reproducibility and future public-repository boundary.

This command validates records and source hygiene.  It does not run the
experimental data, build ROOT, approve figures, or publish a repository.
"""

from __future__ import print_function

import argparse
import datetime
import json
import os
import re
import subprocess
import sys
from pathlib import Path


TOOL_VERSION = "0.3.0"
SCRIPT_PATH = Path(__file__).resolve()
REPOSITORY_ROOT = SCRIPT_PATH.parents[1]
PIPELINE_REGISTRY = (
    REPOSITORY_ROOT
    / "analysis"
    / "data_preprocessing"
    / "provenance"
    / "pipeline_stages.json"
)
PRIVATE_MARKERS = (
    "/nas/",
    "/Users/",
    "token=",
    "password=",
)
PRIVATE_ACCOUNT_PATTERN = re.compile(
    r"\b[a-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,}\b", re.IGNORECASE
)
IGNORED_DIRECTORY_NAMES = {
    ".git",
    "__pycache__",
    "build",
    "local",
    "results",
}
SCAN_EXEMPTIONS = {
    "tools/figure_workflow.py",
    "tools/pipeline_workflow.py",
    "tools/repository_closure.py",
    "tools/tests/test_figure_workflow.py",
    "tools/tests/test_pipeline_workflow.py",
    "tools/tests/test_repository_closure.py",
}
REQUIRED_DOCUMENTS = (
    "LICENSE",
    "CITATION.cff",
    "README.md",
    "REPRODUCE.md",
    "docs/CHAPTER3_END_TO_END.md",
    "docs/ANALYSIS_IO_MAP.md",
    "docs/DATA_ACCESS.md",
    "docs/REPRODUCIBILITY_STATUS.md",
    "plotting/README.md",
    "plotting/records/README.md",
    "analysis/data_preprocessing/README.md",
    "analysis/data_preprocessing/provenance/source_manifest.tsv",
    "analysis/data_preprocessing/provenance/migration_manifest.tsv",
    "analysis/data_preprocessing/provenance/pipeline_stages.json",
)
RELEASE_FILES = ("LICENSE", "CITATION.cff")


def utc_now():
    return datetime.datetime.utcnow().replace(microsecond=0).isoformat() + "Z"


def is_ignored(path, root=REPOSITORY_ROOT):
    relative = Path(path).relative_to(root)
    return any(part in IGNORED_DIRECTORY_NAMES for part in relative.parts)


def text_files(root=REPOSITORY_ROOT):
    root = Path(root)
    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.name == ".DS_Store" or is_ignored(path, root):
            continue
        relative = path.relative_to(root).as_posix()
        if relative in SCAN_EXEMPTIONS:
            continue
        try:
            with path.open("rb") as stream:
                sample = stream.read(4096)
            if b"\x00" in sample:
                continue
            yield path
        except OSError:
            continue


def scan_public_boundary(root=REPOSITORY_ROOT):
    findings = []
    root = Path(root)
    for path in text_files(root):
        relative = path.relative_to(root).as_posix()
        try:
            lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
        except OSError as error:
            findings.append({"path": relative, "line": 0, "marker": "read-error", "text": str(error)})
            continue
        for line_number, line in enumerate(lines, 1):
            for marker in PRIVATE_MARKERS:
                if marker in line:
                    findings.append(
                        {
                            "path": relative,
                            "line": line_number,
                            "marker": marker,
                            "text": line.strip()[:240],
                        }
                    )
            if PRIVATE_ACCOUNT_PATTERN.search(line):
                findings.append(
                    {
                        "path": relative,
                        "line": line_number,
                        "marker": "account-or-email",
                        "text": line.strip()[:240],
                    }
                )
    return findings


def check_required_documents(root=REPOSITORY_ROOT):
    root = Path(root)
    return [item for item in REQUIRED_DOCUMENTS if not (root / item).is_file()]


def check_citation_metadata(root=REPOSITORY_ROOT):
    citation_path = Path(root) / "CITATION.cff"
    if not citation_path.is_file():
        return ["citation-file"]
    citation = citation_path.read_text(encoding="utf-8")
    required_tokens = (
        "cff-version: 1.2.0",
        'title: "CSHINE Sn+Sn Gamma-Spectrum Calibration and Reconstruction"',
        "license: BSD-3-Clause",
        "family-names: Xu",
        "given-names: Junhuai",
        'doi: "10.1103/dhz2-nl56"',
        'doi: "10.1103/jw1p-36pb"',
    )
    return [token for token in required_tokens if token not in citation]


def check_license(root=REPOSITORY_ROOT):
    license_path = Path(root) / "LICENSE"
    if not license_path.is_file():
        return ["license-file"]
    license_text = license_path.read_text(encoding="utf-8")
    required_tokens = (
        "BSD 3-Clause License",
        "Copyright (c) 2026, Junhuai Xu and contributors",
        'THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"',
    )
    return [token for token in required_tokens if token not in license_text]


def check_stage_guide(root=REPOSITORY_ROOT):
    guide = (Path(root) / "REPRODUCE.md").read_text(encoding="utf-8")
    missing = []
    for stage in range(2, 13):
        token = "data_preprocessing.py m{0}".format(stage)
        if token not in guide:
            missing.append("m{0}".format(stage))
    if "data_preprocessing.py m10b" not in guide:
        missing.append("m10b")
    return missing


def check_end_to_end_guide(root=REPOSITORY_ROOT):
    guide_path = Path(root) / "docs" / "CHAPTER3_END_TO_END.md"
    if not guide_path.is_file():
        return ["guide-file"]
    guide = guide_path.read_text(encoding="utf-8")
    required_tokens = (
        "central_beam_on_run_groups.tsv",
        "central_beam_off_run_groups.tsv",
        "data_preprocessing.py m6",
        "data_preprocessing.py m9",
        "data_preprocessing.py m10",
        "data_preprocessing.py m10b",
        "data_preprocessing.py m11",
        "data_preprocessing.py m12",
        "energy_calibration.root",
        "slow/spectrum_110.root",
    )
    return [token for token in required_tokens if token not in guide]


def run_check(command, root=REPOSITORY_ROOT):
    process = subprocess.Popen(
        command,
        cwd=str(root),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    )
    output, _ = process.communicate()
    return {
        "command": [str(item) for item in command],
        "return_code": process.returncode,
        "output": output.strip(),
    }


def pipeline_warnings(root=REPOSITORY_ROOT):
    registry_path = Path(root) / PIPELINE_REGISTRY.relative_to(REPOSITORY_ROOT)
    registry = json.loads(registry_path.read_text(encoding="utf-8"))
    warnings = []
    for stage in registry.get("stages", []):
        if stage.get("id") == "M0B" and stage.get("status") != "closed":
            warnings.append(
                "M0B remains {0}; the Chapter 3 source boundary is not closed.".format(
                    stage.get("status", "unknown")
                )
            )
    return warnings


def audit(root=REPOSITORY_ROOT, strict_release=False):
    root = Path(root).resolve()
    checks = [
        run_check([sys.executable, "tools/data_preprocessing.py", "verify"], root),
        run_check([sys.executable, "tools/pipeline_workflow.py", "check"], root),
        run_check([sys.executable, "tools/figure_workflow.py", "check"], root),
    ]
    errors = []
    warnings = []
    for item in checks:
        if item["return_code"] != 0:
            errors.append("command failed: {0}".format(" ".join(item["command"])))

    missing_documents = check_required_documents(root)
    if missing_documents:
        errors.append("missing required documents: {0}".format(", ".join(missing_documents)))

    missing_citation_tokens = check_citation_metadata(root)
    if missing_citation_tokens:
        errors.append(
            "CITATION.cff lacks required metadata: {0}".format(
                ", ".join(missing_citation_tokens)
            )
        )

    missing_license_tokens = check_license(root)
    if missing_license_tokens:
        errors.append(
            "LICENSE lacks required BSD-3-Clause text: {0}".format(
                ", ".join(missing_license_tokens)
            )
        )

    missing_stages = check_stage_guide(root)
    if missing_stages:
        errors.append("REPRODUCE.md lacks stage commands: {0}".format(", ".join(missing_stages)))

    missing_end_to_end_tokens = check_end_to_end_guide(root)
    if missing_end_to_end_tokens:
        errors.append(
            "Chapter 3 end-to-end guide lacks required handoffs: {0}".format(
                ", ".join(missing_end_to_end_tokens)
            )
        )

    private_findings = scan_public_boundary(root)
    if private_findings:
        errors.append("public-boundary scan found {0} private marker(s)".format(len(private_findings)))

    missing_release_files = [item for item in RELEASE_FILES if not (root / item).is_file()]
    if missing_release_files:
        message = "public release still requires an author-approved {0}".format(
            " and ".join(missing_release_files)
        )
        if strict_release:
            errors.append(message)
        else:
            warnings.append(message)
    warnings.extend(pipeline_warnings(root))

    return {
        "schema_version": 1,
        "tool_version": TOOL_VERSION,
        "checked_utc": utc_now(),
        "status": "passed" if not errors else "failed",
        "strict_release": bool(strict_release),
        "checks": checks,
        "missing_documents": missing_documents,
        "missing_citation_metadata": missing_citation_tokens,
        "missing_license_metadata": missing_license_tokens,
        "missing_stage_commands": missing_stages,
        "missing_end_to_end_handoffs": missing_end_to_end_tokens,
        "private_boundary_findings": private_findings,
        "missing_release_files": missing_release_files,
        "errors": errors,
        "warnings": warnings,
    }


def write_json_atomic(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(str(temporary), str(path))


def command_check(args):
    result = audit(args.root, args.strict_release)
    for item in result["checks"]:
        label = "PASS" if item["return_code"] == 0 else "FAIL"
        print("{0} {1}".format(label, " ".join(item["command"])))
    for warning in result["warnings"]:
        print("WARN {0}".format(warning))
    for error in result["errors"]:
        print("FAIL {0}".format(error))
    if args.output:
        write_json_atomic(args.output, result)
        print("wrote closure report: {0}".format(args.output))
    print("repository closure audit: {0}".format(result["status"]))
    return 0 if result["status"] == "passed" else 1


def build_parser():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", action="version", version=TOOL_VERSION)
    subparsers = parser.add_subparsers(dest="command")
    check_parser = subparsers.add_parser("check", help="run the repository closure audit")
    check_parser.add_argument("--root", default=str(REPOSITORY_ROOT))
    check_parser.add_argument("--output")
    check_parser.add_argument(
        "--strict-release",
        action="store_true",
        help="require every author-approved release file before declaring public-release readiness",
    )
    check_parser.set_defaults(function=command_check)
    return parser


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)
    if not hasattr(args, "function"):
        parser.print_help()
        return 2
    return args.function(args)


if __name__ == "__main__":
    sys.exit(main())
