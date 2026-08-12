#!/usr/bin/env python3
"""Validate and query the machine-readable Chapter 3 analysis-stage registry.

The registry records reviewed relationships.  This tool never infers physics
roles from filenames and never executes an analysis stage.
"""

from __future__ import print_function

import argparse
import csv
import json
import re
import sys
from pathlib import Path


TOOL_VERSION = "0.1.0"
SCRIPT_PATH = Path(__file__).resolve()
REPRODUCIBLE_ROOT = SCRIPT_PATH.parents[1]
PROJECT_ROOT = REPRODUCIBLE_ROOT.parent
DEFAULT_REGISTRY = (
    REPRODUCIBLE_ROOT
    / "analysis"
    / "data_preprocessing"
    / "provenance"
    / "pipeline_stages.json"
)
SOURCE_MANIFEST = (
    REPRODUCIBLE_ROOT
    / "analysis"
    / "data_preprocessing"
    / "provenance"
    / "source_manifest.tsv"
)
THESIS_OUTPUT_MAP = PROJECT_ROOT / "THESIS_OUTPUT_CODE_MAP.md"
FIGURE_RECORD_DIRECTORY = REPRODUCIBLE_ROOT / "plotting" / "records"

STAGE_ID_PATTERN = re.compile(r"^M(?:0B|[1-9]|1[0-3])$")
ARTIFACT_ID_PATTERN = re.compile(r"^[a-z0-9][a-z0-9-]*$")
ALLOWED_STATUSES = {
    "planned",
    "in_progress",
    "source_frozen",
    "code_migrated",
    "numerically_validated",
    "closed",
}
COMPLETED_STATUSES = {
    "source_frozen",
    "code_migrated",
    "numerically_validated",
    "closed",
}
PRIVATE_MARKERS = ("/nas/", "/Users/", "token=", "password=")
PRIVATE_ACCOUNT_PATTERN = re.compile(
    r"\b[a-z0-9._%+-]+@[a-z0-9.-]+\.[a-z]{2,}\b", re.IGNORECASE
)


class RegistryError(RuntimeError):
    pass


def read_json(path):
    with Path(path).expanduser().resolve().open("r", encoding="utf-8") as stream:
        return json.load(stream)


def read_source_ids(path=SOURCE_MANIFEST):
    with Path(path).open("r", encoding="utf-8", newline="") as stream:
        return {row["source_id"] for row in csv.DictReader(stream, delimiter="\t")}


def read_thesis_output_ids(path=THESIS_OUTPUT_MAP):
    text = Path(path).read_text(encoding="utf-8")
    return set(re.findall(r"^\|\s*`([^`]+)`\s*\|", text, re.MULTILINE))


def iter_strings(value):
    if isinstance(value, dict):
        for item in value.values():
            for text in iter_strings(item):
                yield text
    elif isinstance(value, list):
        for item in value:
            for text in iter_strings(item):
                yield text
    elif isinstance(value, str):
        yield value


def stage_map(registry):
    return {stage["id"]: stage for stage in registry.get("stages", [])}


def artifact_map(registry):
    artifacts = {}
    for stage in registry.get("stages", []):
        for artifact in stage.get("outputs", []):
            artifacts[artifact["id"]] = (stage["id"], artifact)
    return artifacts


def dependency_closure(stage_id, stages):
    result = set()
    pending = list(stages[stage_id].get("depends_on", []))
    while pending:
        dependency = pending.pop()
        if dependency in result or dependency not in stages:
            continue
        result.add(dependency)
        pending.extend(stages[dependency].get("depends_on", []))
    return result


def find_cycles(stages):
    cycles = []
    visiting = set()
    visited = set()

    def visit(stage_id, path):
        if stage_id in visiting:
            start = path.index(stage_id)
            cycles.append(path[start:] + [stage_id])
            return
        if stage_id in visited or stage_id not in stages:
            return
        visiting.add(stage_id)
        path.append(stage_id)
        for dependency in stages[stage_id].get("depends_on", []):
            visit(dependency, path)
        path.pop()
        visiting.remove(stage_id)
        visited.add(stage_id)

    for stage_id in stages:
        visit(stage_id, [])
    return cycles


def validate_registry(
    registry,
    source_ids=None,
    thesis_output_ids=None,
    repository_root=REPRODUCIBLE_ROOT,
):
    errors = []
    warnings = []
    if registry.get("schema_version") != 1:
        errors.append("schema_version must be 1")
    stages_value = registry.get("stages")
    if not isinstance(stages_value, list) or not stages_value:
        return ["stages must be a non-empty list"], warnings

    identifiers = [stage.get("id", "") for stage in stages_value if isinstance(stage, dict)]
    duplicate_stages = sorted({item for item in identifiers if identifiers.count(item) > 1})
    if duplicate_stages:
        errors.append("duplicate stage IDs: {0}".format(", ".join(duplicate_stages)))
    stages = stage_map(registry)

    output_ids = []
    for stage in stages_value:
        if not isinstance(stage, dict):
            errors.append("each stage must be an object")
            continue
        stage_id = stage.get("id", "")
        if not STAGE_ID_PATTERN.match(stage_id):
            errors.append("invalid stage ID: {0}".format(stage_id or "<empty>"))
        if stage.get("status") not in ALLOWED_STATUSES:
            errors.append("invalid status for {0}".format(stage_id))
        for key in ("title", "thesis_scope"):
            if not isinstance(stage.get(key), str) or not stage.get(key).strip():
                errors.append("{0}.{1} must be non-empty".format(stage_id, key))
        for key in (
            "depends_on",
            "historical_source_ids",
            "portable_entries",
            "external_inputs",
            "inputs",
            "outputs",
            "thesis_outputs",
            "figure_outputs",
            "unresolved",
        ):
            if not isinstance(stage.get(key), list):
                errors.append("{0}.{1} must be a list".format(stage_id, key))

        for dependency in stage.get("depends_on", []):
            if dependency not in stages:
                errors.append("{0} depends on unknown stage {1}".format(stage_id, dependency))
        for source_id in stage.get("historical_source_ids", []):
            if source_ids is not None and source_id not in source_ids:
                errors.append("{0} references unknown source ID {1}".format(stage_id, source_id))
        for entry in stage.get("portable_entries", []):
            path = Path(entry)
            if path.is_absolute() or ".." in path.parts:
                errors.append("{0} has non-portable entry {1}".format(stage_id, entry))
            elif stage.get("status") in COMPLETED_STATUSES or stage.get("status") == "in_progress":
                if not (Path(repository_root) / path).is_file():
                    errors.append("{0} portable entry is missing: {1}".format(stage_id, entry))
        if stage.get("status") in {"code_migrated", "numerically_validated", "closed"}:
            if not stage.get("historical_source_ids"):
                errors.append("{0} migrated stage lacks historical source IDs".format(stage_id))
            if not stage.get("portable_entries"):
                errors.append("{0} migrated stage lacks portable entries".format(stage_id))
        for output in stage.get("outputs", []):
            if not isinstance(output, dict):
                errors.append("{0}.outputs items must be objects".format(stage_id))
                continue
            artifact_id = output.get("id", "")
            if not ARTIFACT_ID_PATTERN.match(artifact_id):
                errors.append("invalid output artifact ID in {0}: {1}".format(stage_id, artifact_id))
            output_ids.append(artifact_id)
            if not isinstance(output.get("kind"), str) or not output.get("kind"):
                errors.append("{0}.{1} requires kind".format(stage_id, artifact_id))
            if not isinstance(output.get("root_objects"), list):
                errors.append("{0}.{1}.root_objects must be a list".format(stage_id, artifact_id))
            if not isinstance(output.get("terminal"), bool):
                errors.append("{0}.{1}.terminal must be boolean".format(stage_id, artifact_id))

        for thesis_id in stage.get("thesis_outputs", []):
            if thesis_output_ids is not None and thesis_id not in thesis_output_ids:
                errors.append("{0} references unknown thesis output {1}".format(stage_id, thesis_id))
        for figure_id in stage.get("figure_outputs", []):
            if figure_id not in stage.get("thesis_outputs", []):
                errors.append(
                    "{0} figure output is not listed in thesis_outputs: {1}".format(
                        stage_id, figure_id
                    )
                )

    duplicate_outputs = sorted({item for item in output_ids if output_ids.count(item) > 1})
    if duplicate_outputs:
        errors.append("duplicate output artifact IDs: {0}".format(", ".join(duplicate_outputs)))

    for cycle in find_cycles(stages):
        errors.append("dependency cycle: {0}".format(" -> ".join(cycle)))

    artifacts = artifact_map(registry)
    consumers = {artifact_id: [] for artifact_id in artifacts}
    for stage in stages_value:
        stage_id = stage.get("id", "")
        allowed_producers = dependency_closure(stage_id, stages) if stage_id in stages else set()
        for artifact_id in stage.get("inputs", []):
            if artifact_id not in artifacts:
                errors.append("{0} consumes unknown artifact {1}".format(stage_id, artifact_id))
                continue
            producer_id = artifacts[artifact_id][0]
            consumers[artifact_id].append(stage_id)
            if producer_id not in allowed_producers:
                errors.append(
                    "{0} consumes {1} from {2} without a dependency path".format(
                        stage_id, artifact_id, producer_id
                    )
                )

    for artifact_id, (producer_id, artifact) in artifacts.items():
        if not artifact.get("terminal") and not consumers.get(artifact_id):
            warnings.append("orphan non-terminal artifact {0} from {1}".format(artifact_id, producer_id))

    for location, text in enumerate(iter_strings(registry)):
        for marker in PRIVATE_MARKERS:
            if marker.lower() in text.lower():
                errors.append("private marker {0!r} found in registry string #{1}".format(marker, location))
        if PRIVATE_ACCOUNT_PATTERN.search(text):
            errors.append("account or email found in registry string #{0}".format(location))

    return errors, warnings


def figure_record_ids(directory=FIGURE_RECORD_DIRECTORY):
    return {path.stem for path in Path(directory).glob("*.json")}


def print_status(registry):
    print("stage\tstatus\tthesis_scope\ttitle")
    for stage in registry["stages"]:
        print(
            "{0}\t{1}\t{2}\t{3}".format(
                stage["id"], stage["status"], stage["thesis_scope"], stage["title"]
            )
        )


def select_next_stage(registry):
    stages = stage_map(registry)
    for stage in registry["stages"]:
        if stage["status"] == "in_progress":
            return stage
    for stage in registry["stages"]:
        if stage["status"] != "planned":
            continue
        if all(stages[item]["status"] in COMPLETED_STATUSES for item in stage["depends_on"]):
            return stage
    return None


def trace_registry(registry, query):
    query_lower = query.lower()
    matches = []
    for stage in registry["stages"]:
        values = [stage["id"], stage["title"], stage["thesis_scope"]]
        values.extend(stage["historical_source_ids"])
        values.extend(stage["portable_entries"])
        values.extend(stage["inputs"])
        values.extend(stage["thesis_outputs"])
        values.extend(stage["figure_outputs"])
        for output in stage["outputs"]:
            values.append(output["id"])
            values.extend(output["root_objects"])
        if any(query_lower in value.lower() for value in values):
            matches.append(stage)
    return matches


def mermaid_graph(registry):
    lines = ["flowchart LR"]
    for stage in registry["stages"]:
        label = "{0}: {1}".format(stage["id"], stage["title"]).replace('"', "'")
        lines.append('  {0}["{1}"]'.format(stage["id"], label))
    for stage in registry["stages"]:
        for dependency in stage["depends_on"]:
            lines.append("  {0} --> {1}".format(dependency, stage["id"]))
    return "\n".join(lines)


def load_and_validate(args):
    registry = read_json(args.registry)
    errors, warnings = validate_registry(
        registry,
        source_ids=read_source_ids(),
        thesis_output_ids=read_thesis_output_ids(),
    )
    return registry, errors, warnings


def command_check(args):
    registry, errors, warnings = load_and_validate(args)
    record_ids = figure_record_ids()
    missing_records = sorted(
        {
            figure_id
            for stage in registry["stages"]
            for figure_id in stage["figure_outputs"]
            if figure_id not in record_ids
        }
    )
    for figure_id in missing_records:
        warnings.append("missing figure-workflow record: {0}".format(figure_id))
    if errors:
        raise RegistryError("registry validation failed:\n  " + "\n  ".join(errors))
    for warning in warnings:
        print("warning: " + warning)
    if args.strict and warnings:
        raise RegistryError("strict validation failed with {0} warnings".format(len(warnings)))
    print(
        "registry validation passed: {0} stages, {1} warnings".format(
            len(registry["stages"]), len(warnings)
        )
    )
    return 0


def command_status(args):
    registry, errors, warnings = load_and_validate(args)
    if errors:
        raise RegistryError("registry validation failed:\n  " + "\n  ".join(errors))
    print_status(registry)
    return 0


def command_show(args):
    registry, errors, warnings = load_and_validate(args)
    if errors:
        raise RegistryError("registry validation failed:\n  " + "\n  ".join(errors))
    stages = stage_map(registry)
    if args.stage not in stages:
        raise RegistryError("unknown stage: {0}".format(args.stage))
    print(json.dumps(stages[args.stage], indent=2, sort_keys=True, ensure_ascii=False))
    return 0


def command_trace(args):
    registry, errors, warnings = load_and_validate(args)
    if errors:
        raise RegistryError("registry validation failed:\n  " + "\n  ".join(errors))
    matches = trace_registry(registry, args.query)
    for stage in matches:
        print("{0}\t{1}\t{2}".format(stage["id"], stage["status"], stage["title"]))
    print("matches\t{0}".format(len(matches)))
    return 0 if matches else 1


def command_next(args):
    registry, errors, warnings = load_and_validate(args)
    if errors:
        raise RegistryError("registry validation failed:\n  " + "\n  ".join(errors))
    stage = select_next_stage(registry)
    if stage is None:
        print("no runnable stage")
        return 1
    print("{0}\t{1}\t{2}".format(stage["id"], stage["status"], stage["title"]))
    return 0


def command_graph(args):
    registry, errors, warnings = load_and_validate(args)
    if errors:
        raise RegistryError("registry validation failed:\n  " + "\n  ".join(errors))
    print(mermaid_graph(registry))
    return 0


def build_parser():
    parser = argparse.ArgumentParser(
        description="Validate and query the reviewed Chapter 3 pipeline registry."
    )
    parser.add_argument("--version", action="version", version=TOOL_VERSION)
    parser.add_argument("--registry", default=str(DEFAULT_REGISTRY))
    subparsers = parser.add_subparsers(dest="command")
    subparsers.required = True

    check = subparsers.add_parser("check")
    check.add_argument("--strict", action="store_true")
    check.set_defaults(func=command_check)

    status = subparsers.add_parser("status")
    status.set_defaults(func=command_status)

    show = subparsers.add_parser("show")
    show.add_argument("stage")
    show.set_defaults(func=command_show)

    trace = subparsers.add_parser("trace")
    trace.add_argument("query")
    trace.set_defaults(func=command_trace)

    next_stage = subparsers.add_parser("next")
    next_stage.set_defaults(func=command_next)

    graph = subparsers.add_parser("graph")
    graph.set_defaults(func=command_graph)
    return parser


def main(argv=None):
    args = build_parser().parse_args(argv)
    try:
        return args.func(args)
    except (RegistryError, OSError, ValueError, json.JSONDecodeError) as error:
        print("error: {0}".format(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
