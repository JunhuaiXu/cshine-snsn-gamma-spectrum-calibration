#!/usr/bin/env python3

from __future__ import print_function

import argparse
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


TOOL_PATH = Path(__file__).resolve().parents[1] / "figure_workflow.py"
SPEC = importlib.util.spec_from_file_location("figure_workflow", str(TOOL_PATH))
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def init_arguments(directory):
    return argparse.Namespace(
        stable_id="example-figure",
        title="Example figure",
        figure_class="reproducible_redraw",
        thesis_section="Sec. 3.3.4",
        thesis_label="fig:example-figure",
        source_figure="Analysis-note Fig. X",
        historical_source=["DataPreprocessing/example.C"],
        output=str(Path(directory) / "example-figure.json"),
        force=False,
    )


class FigureWorkflowTest(unittest.TestCase):
    def test_new_staged_record_is_valid(self):
        with tempfile.TemporaryDirectory() as directory:
            args = init_arguments(directory)
            self.assertEqual(MODULE.command_init(args), 0)
            record = MODULE.read_json(args.output)
            self.assertEqual(record["stable_id"], "example-figure")
            self.assertEqual(MODULE.validate_record(record), [])

    def test_private_server_path_is_rejected(self):
        args = init_arguments("unused")
        record = MODULE.new_record(args)
        record["historical"]["source_files"][0]["path"] = "/nas/data/private.C"
        errors = MODULE.validate_record(record)
        self.assertTrue(any("private path" in item for item in errors))

    def test_account_or_email_is_rejected_without_storing_real_examples(self):
        args = init_arguments("unused")
        record = MODULE.new_record(args)
        record["historical"]["source_files"][0]["path"] = (
            "internal-user@example.invalid:DataPreprocessing/example.C"
        )
        errors = MODULE.validate_record(record)
        self.assertTrue(any("account or email" in item for item in errors))

    def test_validation_cannot_skip_contract_and_portable_entry(self):
        args = init_arguments("unused")
        record = MODULE.new_record(args)
        record["validation"]["status"] = "real_data_checked"
        record["validation"]["checks"] = ["count conservation"]
        record["validation"]["records"] = ["results/run_metadata.json"]
        errors = MODULE.validate_record(record)
        self.assertTrue(any("frozen physics contract" in item for item in errors))
        self.assertTrue(any("implemented portable entry" in item for item in errors))

    def test_artifact_records_checksum_without_storing_external_path(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            args = init_arguments(directory)
            MODULE.command_init(args)
            artifact = directory / "candidate.pdf"
            artifact.write_bytes(b"test-pdf")
            record = MODULE.read_json(args.output)
            record["physics_contract"].update(
                {
                    "status": "frozen",
                    "sample": "synthetic sample",
                    "inputs": ["input.root:tree"],
                    "variables": ["x"],
                    "selections": ["none"],
                    "binning": ["10 bins from 0 to 1"],
                    "normalization": "none",
                    "display_policy": ["linear scale"],
                }
            )
            record["portable"].update(
                {
                    "status": "implemented",
                    "python_entries": ["tools/figure_workflow.py"],
                }
            )
            MODULE.write_json_atomic(args.output, record)
            artifact_args = argparse.Namespace(
                record=args.output,
                stage="candidate",
                role="pdf",
                path=str(artifact),
                logical_path="candidate.pdf",
            )
            self.assertEqual(MODULE.command_artifact(artifact_args), 0)
            record = MODULE.read_json(args.output)
            item = record["candidate"]["artifacts"][0]
            self.assertEqual(item["path"], "candidate.pdf")
            self.assertEqual(item["sha256"], MODULE.sha256_file(artifact))


if __name__ == "__main__":
    unittest.main()
