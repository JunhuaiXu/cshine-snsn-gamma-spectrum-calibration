#!/usr/bin/env python3

from __future__ import print_function

import importlib.util
import tempfile
import unittest
from pathlib import Path


TOOL_PATH = Path(__file__).resolve().parents[1] / "repository_closure.py"
SPEC = importlib.util.spec_from_file_location("repository_closure", str(TOOL_PATH))
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class RepositoryClosureTest(unittest.TestCase):
    def test_public_scan_ignores_private_results_but_checks_sources(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "tools").mkdir()
            (root / "results").mkdir()
            (root / "README.md").write_text("safe\n", encoding="utf-8")
            (root / "results" / "run.json").write_text("/nas/private\n", encoding="utf-8")
            self.assertEqual(MODULE.scan_public_boundary(root), [])
            (root / "source.py").write_text("input = '/nas/private'\n", encoding="utf-8")
            findings = MODULE.scan_public_boundary(root)
            self.assertEqual(len(findings), 1)
            self.assertEqual(findings[0]["path"], "source.py")

    def test_public_scan_rejects_account_or_email_without_storing_real_examples(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "README.md").write_text(
                "connect as internal-user@example.invalid\n", encoding="utf-8"
            )
            findings = MODULE.scan_public_boundary(root)
            self.assertEqual(len(findings), 1)
            self.assertEqual(findings[0]["marker"], "account-or-email")

    def test_required_document_check_reports_only_missing_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for relative in MODULE.REQUIRED_DOCUMENTS[:-1]:
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text("test\n", encoding="utf-8")
            self.assertEqual(
                MODULE.check_required_documents(root),
                [MODULE.REQUIRED_DOCUMENTS[-1]],
            )

    def test_citation_check_requires_project_and_article_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "CITATION.cff").write_text(
                "\n".join(
                    (
                        "cff-version: 1.2.0",
                        'title: "CSHINE Sn+Sn Gamma-Spectrum Calibration and Reconstruction"',
                        "license: BSD-3-Clause",
                        "authors:",
                        "  - family-names: Xu",
                        "    given-names: Junhuai",
                        'doi: "10.1103/dhz2-nl56"',
                        'doi: "10.1103/jw1p-36pb"',
                    )
                ),
                encoding="utf-8",
            )
            self.assertEqual(MODULE.check_citation_metadata(root), [])
            (root / "CITATION.cff").write_text(
                "cff-version: 1.2.0\n",
                encoding="utf-8",
            )
            self.assertIn(
                'doi: "10.1103/dhz2-nl56"',
                MODULE.check_citation_metadata(root),
            )
            self.assertIn(
                'doi: "10.1103/jw1p-36pb"',
                MODULE.check_citation_metadata(root),
            )

    def test_license_check_requires_approved_bsd_text(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "LICENSE").write_text(
                "\n".join(
                    (
                        "BSD 3-Clause License",
                        "Copyright (c) 2026, Junhuai Xu and contributors",
                        'THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"',
                    )
                ),
                encoding="utf-8",
            )
            self.assertEqual(MODULE.check_license(root), [])
            (root / "LICENSE").write_text("custom terms\n", encoding="utf-8")
            self.assertIn("BSD 3-Clause License", MODULE.check_license(root))

    def test_end_to_end_guide_requires_both_m6_manifests(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            guide = root / "docs" / "CHAPTER3_END_TO_END.md"
            guide.parent.mkdir(parents=True)
            guide.write_text(
                "\n".join(
                    token
                    for token in (
                        "central_beam_on_run_groups.tsv",
                        "data_preprocessing.py m6",
                        "data_preprocessing.py m9",
                        "data_preprocessing.py m10",
                        "data_preprocessing.py m10b",
                        "data_preprocessing.py m11",
                        "data_preprocessing.py m12",
                        "energy_calibration.root",
                        "slow/spectrum_110.root",
                    )
                ),
                encoding="utf-8",
            )
            self.assertEqual(
                MODULE.check_end_to_end_guide(root),
                ["central_beam_off_run_groups.tsv"],
            )


if __name__ == "__main__":
    unittest.main()
