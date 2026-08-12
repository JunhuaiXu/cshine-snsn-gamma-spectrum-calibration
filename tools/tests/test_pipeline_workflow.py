#!/usr/bin/env python3

from __future__ import print_function

import importlib.util
import tempfile
import unittest
from pathlib import Path


TOOL_PATH = Path(__file__).resolve().parents[1] / "pipeline_workflow.py"
SPEC = importlib.util.spec_from_file_location("pipeline_workflow", str(TOOL_PATH))
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def stage(stage_id, status="planned", dependencies=None, inputs=None, outputs=None):
    return {
        "id": stage_id,
        "title": "Stage " + stage_id,
        "thesis_scope": "Sec. 3.3",
        "status": status,
        "depends_on": dependencies or [],
        "historical_source_ids": [],
        "portable_entries": [],
        "external_inputs": [],
        "inputs": inputs or [],
        "outputs": outputs or [],
        "thesis_outputs": [],
        "figure_outputs": [],
        "unresolved": [],
    }


def output(artifact_id, terminal=False):
    return {
        "id": artifact_id,
        "kind": "root-file",
        "root_objects": [],
        "terminal": terminal,
    }


class PipelineWorkflowTest(unittest.TestCase):
    def test_valid_dependency_and_artifact_chain(self):
        registry = {
            "schema_version": 1,
            "stages": [
                stage("M1", outputs=[output("first-output")]),
                stage(
                    "M2",
                    dependencies=["M1"],
                    inputs=["first-output"],
                    outputs=[output("final-output", terminal=True)],
                ),
            ],
        }
        errors, warnings = MODULE.validate_registry(
            registry, source_ids=set(), thesis_output_ids=set(), repository_root=Path(".")
        )
        self.assertEqual(errors, [])
        self.assertEqual(warnings, [])

    def test_missing_artifact_is_an_error(self):
        registry = {
            "schema_version": 1,
            "stages": [stage("M1", inputs=["not-produced"])],
        }
        errors, _ = MODULE.validate_registry(
            registry, source_ids=set(), thesis_output_ids=set(), repository_root=Path(".")
        )
        self.assertTrue(any("unknown artifact" in item for item in errors))

    def test_dependency_cycle_is_rejected(self):
        registry = {
            "schema_version": 1,
            "stages": [
                stage("M1", dependencies=["M2"]),
                stage("M2", dependencies=["M1"]),
            ],
        }
        errors, _ = MODULE.validate_registry(
            registry, source_ids=set(), thesis_output_ids=set(), repository_root=Path(".")
        )
        self.assertTrue(any("dependency cycle" in item for item in errors))

    def test_private_paths_are_rejected(self):
        item = stage("M1")
        item["external_inputs"] = ["/nas/data/private.root"]
        registry = {"schema_version": 1, "stages": [item]}
        errors, _ = MODULE.validate_registry(
            registry, source_ids=set(), thesis_output_ids=set(), repository_root=Path(".")
        )
        self.assertTrue(any("private marker" in value for value in errors))

    def test_account_or_email_is_rejected_without_storing_real_examples(self):
        item = stage("M1")
        item["external_inputs"] = ["internal-user@example.invalid"]
        registry = {"schema_version": 1, "stages": [item]}
        errors, _ = MODULE.validate_registry(
            registry, source_ids=set(), thesis_output_ids=set(), repository_root=Path(".")
        )
        self.assertTrue(any("account or email" in value for value in errors))

    def test_next_prefers_in_progress_stage(self):
        registry = {
            "schema_version": 1,
            "stages": [
                stage("M0B", status="in_progress"),
                stage("M1", status="planned"),
            ],
        }
        self.assertEqual(MODULE.select_next_stage(registry)["id"], "M0B")

    def test_trace_finds_root_object_and_thesis_output(self):
        item = stage("M1", outputs=[output("energy-calibration", terminal=True)])
        item["outputs"][0]["root_objects"] = ["cali_20240308"]
        item["thesis_outputs"] = ["cshine-energy-calibration-table"]
        registry = {"schema_version": 1, "stages": [item]}
        self.assertEqual(MODULE.trace_registry(registry, "cali_20240308")[0]["id"], "M1")
        self.assertEqual(
            MODULE.trace_registry(registry, "cshine-energy-calibration-table")[0]["id"],
            "M1",
        )


if __name__ == "__main__":
    unittest.main()
