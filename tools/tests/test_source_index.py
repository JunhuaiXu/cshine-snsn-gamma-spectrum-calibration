#!/usr/bin/env python3

from __future__ import print_function

import importlib.util
import tempfile
import unittest
from pathlib import Path


TOOL_PATH = Path(__file__).resolve().parents[1] / "source_index.py"
SPEC = importlib.util.spec_from_file_location("source_index", str(TOOL_PATH))
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


SOURCE = r'''
#include "gamma_time_cali.hh"
class ExampleCalibration {};
void build_histogram() {
  tree->SetBranchAddress("GammaTime", values);
  TH2D histogram("ALL_h2_TOF_TotalE", "title", 10, 0, 1, 10, 0, 1);
  TFile input("input.root");
  input.Get("cali_20240308");
}
'''


class SourceIndexTest(unittest.TestCase):
    def test_build_extracts_exact_candidate_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "step3-time").mkdir()
            source = root / "step3-time" / "example.C"
            source.write_text(SOURCE, encoding="utf-8")

            index = MODULE.build_index(root)
            self.assertEqual(MODULE.validate_index(index), [])
            self.assertEqual(index["file_count"], 1)
            record = index["files"][0]
            self.assertEqual(record["path"], "step3-time/example.C")
            self.assertIn("gamma_time_cali.hh", record["includes"])
            self.assertIn("GammaTime", record["branches"])
            self.assertIn("ALL_h2_TOF_TotalE", record["root_objects"])
            self.assertIn("cali_20240308", record["root_objects"])
            self.assertIn("input.root", record["root_files"])
            self.assertIn("ExampleCalibration", record["symbols"])

    def test_query_returns_candidates_without_assigning_roles(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "one.C").write_text(SOURCE, encoding="utf-8")
            (root / "two.C").write_text("void unrelated() {}\n", encoding="utf-8")
            index = MODULE.build_index(root)
            matches = MODULE.query_records(index, "GammaTime")
            self.assertEqual([item["path"] for item in matches], ["one.C"])

    def test_duplicate_groups_use_exact_file_hashes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "a.C").write_text(SOURCE, encoding="utf-8")
            (root / "b.C").write_text(SOURCE, encoding="utf-8")
            index = MODULE.build_index(root)
            self.assertEqual(index["duplicate_group_count"], 1)
            self.assertEqual(index["duplicate_groups"][0]["paths"], ["a.C", "b.C"])

    def test_index_never_stores_snapshot_absolute_path(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "example.C").write_text(SOURCE, encoding="utf-8")
            index = MODULE.build_index(root)
            rendered = str(index)
            self.assertNotIn(str(root), rendered)


if __name__ == "__main__":
    unittest.main()
