import argparse
import hashlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).resolve().parents[1] / "data_preprocessing.py"
SPEC = importlib.util.spec_from_file_location("data_preprocessing", str(SCRIPT))
DATA_PREPROCESSING = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(DATA_PREPROCESSING)


FAKE_M2_EXECUTABLE = """#!/usr/bin/env python3
import os
import sys

if "--print-config" in sys.argv:
    print("name=synthetic_m2")
    sys.exit(0)

def value(option):
    return sys.argv[sys.argv.index(option) + 1]

input_dir = value("--input-dir")
output = value("--output")
report = value("--report")
input_path = os.path.join(input_dir, "input.root")
with open(output, "wb") as stream:
    stream.write(b"synthetic-root-output")
with open(report, "w") as stream:
    stream.write("record\\tfield_1\\tfield_2\\tfield_3\\n")
    stream.write("input\\tsource\\t0\\t{0}\\n".format(input_path))
print("output=" + output)
"""


FAKE_M3_EXECUTABLE = """#!/usr/bin/env python3
import os
import sys

if "--print-config" in sys.argv:
    print("name=synthetic_m3")
    sys.exit(0)

def value(option):
    return sys.argv[sys.argv.index(option) + 1]

input_dir = value("--input-dir")
output = value("--output")
report = value("--report")
parameters = value("--parameters")
input_path = os.path.join(input_dir, "input.root")
with open(output, "wb") as stream:
    stream.write(b"synthetic-gain-output")
with open(parameters, "w") as stream:
    stream.write("synthetic parameters\\n")
with open(report, "w") as stream:
    stream.write("record\\tfield_1\\tfield_2\\n")
    stream.write("input\\t0\\t{0}\\n".format(input_path))
print("output=" + output)
"""


FAKE_M4_EXECUTABLE = """#!/usr/bin/env python3
import sys

if "--print-config" in sys.argv:
    print("name=synthetic_m4")
    sys.exit(0)

def value(option):
    return sys.argv[sys.argv.index(option) + 1]

source_spectra = value("--source-spectra")
gain_relation = value("--gain-relation")
output = value("--output")
report = value("--report")
with open(output, "wb") as stream:
    stream.write(b"synthetic-calibration-output")
with open(report, "w") as stream:
    stream.write("record\\tfield_1\\tfield_2\\n")
    stream.write("input\\tsource_spectra\\t{0}\\n".format(source_spectra))
    stream.write("input\\tgain_relation\\t{0}\\n".format(gain_relation))
print("output=" + output)
"""


FAKE_M5_EXECUTABLE = """#!/usr/bin/env python3
import sys

if "--print-config" in sys.argv:
    print("name=synthetic_m5_audit")
    sys.exit(0)

def value(option):
    return sys.argv[sys.argv.index(option) + 1]

fits_dir = value("--fits-dir")
report = value("--report")
with open(report, "w") as stream:
    stream.write("crystal\\tparameters_match_production\\n")
    for crystal in range(15):
        stream.write("{0}\\ttrue\\n".format(crystal))
print("fits_dir=" + fits_dir)
"""


FAKE_M5_SPECTRA_EXECUTABLE = """#!/usr/bin/env python3
import sys

if "--print-config" in sys.argv:
    print("name=synthetic_m5_spectra")
    print("mode=" + sys.argv[sys.argv.index("--mode") + 1])
    sys.exit(0)

def value(option):
    return sys.argv[sys.argv.index(option) + 1]

output = value("--output")
report = value("--report")
with open(output, "wb") as stream:
    stream.write(b"synthetic-time-spectrum-output")
with open(report, "w") as stream:
    stream.write("record\\tfield_1\\tfield_2\\tfield_3\\n")
    stream.write("summary\\ttree_entries\\t3\\t\\n")
print("output=" + output)
"""


FAKE_M6_EXECUTABLE = """#!/usr/bin/env python3
import sys

if "--print-config" in sys.argv:
    print("name=synthetic_m6")
    print("output_tree=GammaCaliData")
    sys.exit(0)

def value(option):
    return sys.argv[sys.argv.index(option) + 1]

inputs = [sys.argv[index + 1] for index, item in enumerate(sys.argv[:-1]) if item == "--input"]
output = value("--output")
report = value("--report")
with open(output, "wb") as stream:
    stream.write(b"synthetic-calibrated-events")
with open(report, "w") as stream:
    stream.write("record\\tfield_1\\tfield_2\\n")
    for index, path in enumerate(inputs):
        stream.write("input_file\\t{0}\\t{1}\\n".format(index, path))
    stream.write("summary\\toutput_entries\\t4\\n")
print("output=" + output)
"""


FAKE_M7_EXECUTABLE = """#!/usr/bin/env python3
import sys

if "--print-config" in sys.argv:
    print("name=synthetic_m7")
    print("objects=h2_all,h2_cut,hh_diff,h1,h3,h4")
    sys.exit(0)

def value(option):
    return sys.argv[sys.argv.index(option) + 1]

inputs = [sys.argv[index + 1] for index, item in enumerate(sys.argv[:-1]) if item == "--input"]
output = value("--output")
report = value("--report")
with open(output, "wb") as stream:
    stream.write(b"synthetic-neighbor-time-diagnostics")
with open(report, "w") as stream:
    stream.write("record\\tfield_1\\tfield_2\\n")
    for index, path in enumerate(inputs):
        stream.write("input_file\\t{0}\\t{1}\\n".format(index, path))
    stream.write("object\\th2_all\\tTH2F\\n")
print("output=" + output)
"""


FAKE_M8_EXECUTABLE = """#!/usr/bin/env python3
import sys

if "--print-config" in sys.argv:
    print("name=synthetic_m8")
    print("energy_threshold_mev=1")
    print("neighbor_time_ns=50")
    print("separate_core_time_ns=100")
    sys.exit(0)

def value(option):
    return sys.argv[sys.argv.index(option) + 1]

inputs = [sys.argv[index + 1] for index, item in enumerate(sys.argv[:-1]) if item == "--input"]
output = value("--output")
report = value("--report")
with open(output, "wb") as stream:
    stream.write(b"synthetic-reconstructed-events")
with open(report, "w") as stream:
    stream.write("record\\tfield_1\\tfield_2\\n")
    for index, path in enumerate(inputs):
        stream.write("input_file\\t{0}\\t{1}\\n".format(index, path))
    stream.write("summary\\toutput_entries\\t4\\n")
print("output=" + output)
"""


FAKE_M9_MERGE_EXECUTABLE = """#!/usr/bin/env python3
import sys

if "--print-config" in sys.argv:
    print("name=synthetic_m9_merge")
    print("central_crystals=5,6,9,10;input=h_recon")
    print("side_crystals=4,7,8,11,13,14;input=h_recon_veto")
    sys.exit(0)

def value(option):
    return sys.argv[sys.argv.index(option) + 1]

inputs = [sys.argv[index + 1] for index, item in enumerate(sys.argv[:-1]) if item == "--input"]
per_crystal = value("--per-crystal-output")
output = value("--output")
report = value("--report")
sample_role = value("--sample-role")
with open(per_crystal, "wb") as stream:
    stream.write(b"synthetic-per-crystal-spectra")
with open(output, "wb") as stream:
    stream.write(b"synthetic-merged-spectra")
with open(report, "w") as stream:
    stream.write("record\\tfield_1\\tfield_2\\n")
    stream.write("config\\tsample_role\\t{0}\\n".format(sample_role))
    for index, path in enumerate(inputs):
        stream.write("input_file\\t{0}\\t{1}\\n".format(index, path))
print("output=" + output)
"""


FAKE_M10_EXECUTABLE = """#!/usr/bin/env python3
import sys

if "--print-config" in sys.argv:
    print("name=synthetic_m10")
    print("reconstruction=stored_recon_result")
    sys.exit(0)

def value(option):
    return sys.argv[sys.argv.index(option) + 1]

inputs = [sys.argv[index + 1] for index, item in enumerate(sys.argv[:-1]) if item == "--input"]
output = value("--output")
report = value("--report")
sample_role = value("--sample-role")
with open(output, "wb") as stream:
    stream.write(b"synthetic-chapter3-diagnostics")
with open(report, "w") as stream:
    stream.write("record\\tfield_1\\tfield_2\\n")
    stream.write("config\\tsample_role\\t{0}\\n".format(sample_role))
    for index, path in enumerate(inputs):
        stream.write("input_file\\t{0}\\t{1}\\n".format(index, path))
print("output=" + output)
"""


FAKE_M10B_EXECUTABLE = """#!/usr/bin/env python3
import sys

if "--print-config" in sys.argv:
    print("name=synthetic_m10b")
    print("conditioned_indices=17,18,19,20,22")
    sys.exit(0)

def value(option):
    return sys.argv[sys.argv.index(option) + 1]

inputs = [sys.argv[index + 1] for index, item in enumerate(sys.argv[:-1]) if item == "--input"]
output = value("--output")
report = value("--report")
with open(output, "wb") as stream:
    stream.write(b"synthetic-trigger-diagnostics")
with open(report, "w") as stream:
    stream.write("record\\tfield_1\\tfield_2\\n")
    for index, path in enumerate(inputs):
        stream.write("input_file\\t{0}\\t{1}\\n".format(index, path))
print("output=" + output)
"""


FAKE_M11_FAST_EXECUTABLE = """#!/usr/bin/env python3
import sys
if "--print-config" in sys.argv:
    print("name=synthetic_m11_fast")
    sys.exit(0)
def value(option):
    return sys.argv[sys.argv.index(option) + 1]
inputs = [sys.argv[index + 1] for index, item in enumerate(sys.argv[:-1]) if item == "--input"]
signal = value("--signal-output")
random = value("--random-output")
report = value("--report")
for path, content in ((signal, b"synthetic-fast-window"),
                      (random, b"synthetic-random-window")):
    with open(path, "wb") as stream:
        stream.write(content)
with open(report, "w") as stream:
    stream.write("record\\tfield_1\\tfield_2\\n")
    for index, path in enumerate(inputs):
        stream.write("input_file\\t{0}\\t{1}\\n".format(index, path))
print("signal_output=" + signal)
"""


FAKE_M11_SPECTRUM_EXECUTABLE = """#!/usr/bin/env python3
import sys
if "--print-config" in sys.argv:
    print("name=synthetic_m11_spectrum")
    sys.exit(0)
def value(option):
    return sys.argv[sys.argv.index(option) + 1]
output = value("--output")
report = value("--report")
mode = value("--mode")
with open(output, "wb") as stream:
    stream.write(("synthetic-" + mode + "-spectrum").encode("ascii"))
with open(report, "w") as stream:
    stream.write("config\\tmode\\t{0}\\n".format(mode))
print("output=" + output)
"""


FAKE_M12_EXECUTABLE = """#!/usr/bin/env python3
import sys
if "--print-config" in sys.argv:
    print("object_name=histDiff")
    print("object_class=TH1D")
    print("binning=200:0:200_MeV")
    print("energy_frame=laboratory")
    sys.exit(0)
def value(option):
    return sys.argv[sys.argv.index(option) + 1]
input_path = value("--input")
report = value("--report")
with open(report, "w") as stream:
    stream.write("record\\tfield\\tvalue\\tdetail\\n")
    stream.write("input\\troot_file\\t{0}\\n".format(input_path))
    stream.write("contract\\tobject_name\\thistDiff\\n")
    stream.write("contract\\tobject_class\\tTH1D\\n")
print("object_class=TH1D")
"""


class DataPreprocessingToolTest(unittest.TestCase):
    def test_portable_manifest_is_self_consistent(self):
        summary = DATA_PREPROCESSING.verify_manifests()
        self.assertGreater(summary["portable_files_checked"], 0)
        self.assertEqual(summary["source_files_checked"], 0)

    def test_central_m6_manifest_has_sixty_unique_run_groups(self):
        _, rows = DATA_PREPROCESSING.read_m6_run_groups(
            DATA_PREPROCESSING.DEFAULT_M6_RUN_MANIFEST
        )
        self.assertEqual(len(rows), 60)
        self.assertEqual(rows[0]["run_id"], "a20240304_SnSn_GOAL_ALLCOIN.006")
        self.assertEqual(rows[-1]["run_id"], "a20240310_SnSn_GOAL_ALLCOIN.006")

    def test_central_m9_beam_off_manifest_has_six_reviewed_run_groups(self):
        _, rows = DATA_PREPROCESSING.read_m6_run_groups(
            DATA_PREPROCESSING.DEFAULT_M9_BEAM_OFF_RUN_MANIFEST
        )
        self.assertEqual(len(rows), 6)
        self.assertEqual(rows[0]["run_id"], "a20240308_BKG_ALLOR.000")
        self.assertEqual(rows[-1]["run_id"], "a20240311_GAMMA_BKG.000")

    def test_reconstruction_spectra_figure_manifest_has_exact_historical_sample(self):
        summary = (
            DATA_PREPROCESSING.verify_reconstruction_spectra_figure_manifest()
        )
        self.assertEqual(summary["run_group_count"], 59)
        self.assertEqual(
            summary["excluded_central_run_groups"],
            ["a20240304_SnSn_GOAL_ALLCOIN.006"],
        )
        _, rows = DATA_PREPROCESSING.read_m6_run_groups(
            DATA_PREPROCESSING.DEFAULT_RECONSTRUCTION_SPECTRA_FIGURE_MANIFEST
        )
        self.assertEqual(rows[0]["run_id"], "a20240305_SnSn_GOAL_ALLCOIN.000")
        self.assertEqual(rows[-1]["run_id"], "a20240310_SnSn_GOAL_ALLCOIN.006")

    def test_reconstruction_spectra_run_enforces_and_merges_frozen_sample(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            reconstructed = root / "reconstructed"
            results = root / "results"
            build.mkdir()
            reconstructed.mkdir()

            merger = build / "merge_reconstructed_spectra"
            merger.write_text(FAKE_M9_MERGE_EXECUTABLE, encoding="utf-8")
            merger.chmod(0o755)
            _, rows = DATA_PREPROCESSING.read_m6_run_groups(
                DATA_PREPROCESSING.DEFAULT_RECONSTRUCTION_SPECTRA_FIGURE_MANIFEST
            )
            for row in rows:
                (reconstructed / row["output_name"]).write_bytes(
                    b"synthetic-m8-figure-input"
                )

            arguments = argparse.Namespace(
                build_dir=str(build),
                results_dir=str(results),
                run_id="test-run",
                reconstructed_run_dir=str(reconstructed),
                run_manifest=str(
                    DATA_PREPROCESSING.DEFAULT_RECONSTRUCTION_SPECTRA_FIGURE_MANIFEST
                ),
                hash_inputs=False,
            )
            DATA_PREPROCESSING.run_reconstruction_spectra(arguments)
            run_directory = results / "reconstruction-spectra" / "test-run"
            metadata = json.loads(
                (run_directory / "run_metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["status"], "completed")
            self.assertEqual(metadata["run_group_count"], 59)
            self.assertEqual(
                metadata["figure_manifest_verification"]["run_group_count"], 59
            )
            self.assertEqual(len(metadata["inputs"]), 60)
            self.assertTrue((run_directory / "all_notree_figure.root").is_file())
            self.assertTrue((run_directory / "all_recon_figure.root").is_file())

            altered_manifest = root / "altered.tsv"
            altered_manifest.write_text(
                "index\trun_id\traw_pattern\toutput_name\n"
                "0\ta20240308_SnSn_GOAL_ALLCOIN.003\tunused*.root\t"
                "a20240308_SnSn_GOAL_ALLCOIN.003.root\n",
                encoding="utf-8",
            )
            arguments.run_id = "altered-run"
            arguments.run_manifest = str(altered_manifest)
            with self.assertRaises(RuntimeError):
                DATA_PREPROCESSING.run_reconstruction_spectra(arguments)

    def test_m2_run_writes_complete_metadata(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            inputs = root / "inputs"
            results = root / "results"
            build.mkdir()
            inputs.mkdir()
            input_path = inputs / "input.root"
            input_path.write_bytes(b"synthetic-input")

            executable = build / "build_source_spectra"
            executable.write_text(FAKE_M2_EXECUTABLE, encoding="utf-8")
            executable.chmod(0o755)

            arguments = argparse.Namespace(
                build_dir=str(build),
                results_dir=str(results),
                run_id="test-run",
                input_dir=str(inputs),
                hash_inputs=True,
            )
            DATA_PREPROCESSING.run_stage(arguments, "m2")

            run_directory = results / "m2" / "test-run"
            metadata = json.loads(
                (run_directory / "run_metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["status"], "completed")
            self.assertEqual(metadata["return_code"], 0)
            self.assertEqual(len(metadata["inputs"]), 1)
            self.assertEqual(
                metadata["inputs"][0]["sha256"],
                hashlib.sha256(b"synthetic-input").hexdigest(),
            )
            self.assertTrue((run_directory / "config_used.txt").is_file())
            self.assertTrue((run_directory / "input_manifest.tsv").is_file())
            self.assertTrue((run_directory / "run.log").is_file())
            self.assertTrue((run_directory / "run_report.tsv").is_file())
            self.assertTrue((run_directory / "source_background.root").is_file())

            with self.assertRaises(RuntimeError):
                DATA_PREPROCESSING.run_stage(arguments, "m2")

    def test_m3_run_records_parameter_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            inputs = root / "inputs"
            results = root / "results"
            build.mkdir()
            inputs.mkdir()
            (inputs / "input.root").write_bytes(b"synthetic-input")

            executable = build / "fit_gain_relation"
            executable.write_text(FAKE_M3_EXECUTABLE, encoding="utf-8")
            executable.chmod(0o755)

            arguments = argparse.Namespace(
                build_dir=str(build),
                results_dir=str(results),
                run_id="test-run",
                input_dir=str(inputs),
                hash_inputs=False,
            )
            DATA_PREPROCESSING.run_stage(arguments, "m3")

            run_directory = results / "m3" / "test-run"
            metadata = json.loads(
                (run_directory / "run_metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["status"], "completed")
            self.assertEqual(metadata["inputs"][0]["sha256"], "not_computed")
            self.assertTrue((run_directory / "gain_relation.root").is_file())
            self.assertTrue((run_directory / "gain_parameters.txt").is_file())

    def test_m4_run_records_both_root_inputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            inputs = root / "inputs"
            results = root / "results"
            build.mkdir()
            inputs.mkdir()
            source_spectra = inputs / "source.root"
            gain_relation = inputs / "gain.root"
            source_spectra.write_bytes(b"synthetic-source")
            gain_relation.write_bytes(b"synthetic-gain")

            executable = build / "fit_energy_calibration"
            executable.write_text(FAKE_M4_EXECUTABLE, encoding="utf-8")
            executable.chmod(0o755)

            arguments = argparse.Namespace(
                build_dir=str(build),
                results_dir=str(results),
                run_id="test-run",
                source_spectra=str(source_spectra),
                gain_relation=str(gain_relation),
                hash_inputs=True,
            )
            DATA_PREPROCESSING.run_stage(arguments, "m4")

            run_directory = results / "m4" / "test-run"
            metadata = json.loads(
                (run_directory / "run_metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["status"], "completed")
            self.assertEqual(len(metadata["inputs"]), 2)
            self.assertEqual(
                {item["role"] for item in metadata["inputs"]},
                {"source_spectra", "gain_relation"},
            )
            self.assertTrue((run_directory / "energy_calibration.root").is_file())
            self.assertTrue((run_directory / "run_report.tsv").is_file())

    def test_m5_audit_records_all_historical_pairs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            fits = root / "fits"
            results = root / "results"
            build.mkdir()
            fits.mkdir()
            for crystal in range(15):
                stem = "f_{0:02d}".format(crystal)
                (fits / (stem + ".root")).write_bytes(b"synthetic-root")
                (fits / (stem + ".out")).write_text(
                    "synthetic-fit-output\n", encoding="utf-8"
                )

            executable = build / "inspect_time_fit_outputs"
            executable.write_text(FAKE_M5_EXECUTABLE, encoding="utf-8")
            executable.chmod(0o755)

            arguments = argparse.Namespace(
                build_dir=str(build),
                results_dir=str(results),
                run_id="test-run",
                fits_dir=str(fits),
                tolerance=1.0e-9,
                hash_inputs=True,
            )
            DATA_PREPROCESSING.run_m5_audit(arguments)

            run_directory = results / "m5-audit" / "test-run"
            metadata = json.loads(
                (run_directory / "run_metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["status"], "completed")
            self.assertEqual(metadata["return_code"], 0)
            self.assertEqual(len(metadata["inputs"]), 30)
            self.assertTrue(
                all(item["sha256"] != "not_computed" for item in metadata["inputs"])
            )
            self.assertTrue((run_directory / "time_fit_audit.tsv").is_file())
            self.assertTrue((run_directory / "input_manifest.tsv").is_file())

    def test_m5_spectra_records_all_resolved_raw_inputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            inputs = root / "inputs"
            results = root / "results"
            build.mkdir()
            inputs.mkdir()
            for pattern in DATA_PREPROCESSING.M5_TIME_INPUT_PATTERNS:
                filename = pattern.replace("*", "0000")
                (inputs / filename).write_bytes(filename.encode("utf-8"))

            executable = build / "build_time_amplitude_spectra"
            executable.write_text(
                FAKE_M5_SPECTRA_EXECUTABLE, encoding="utf-8"
            )
            executable.chmod(0o755)

            arguments = argparse.Namespace(
                build_dir=str(build),
                results_dir=str(results),
                run_id="test-run",
                input_dir=str(inputs),
                mode="original",
                threads=2,
                hash_inputs=True,
            )
            DATA_PREPROCESSING.run_m5_spectra(arguments)

            run_directory = results / "m5-spectra" / "test-run"
            metadata = json.loads(
                (run_directory / "run_metadata.json").read_text(
                    encoding="utf-8"
                )
            )
            self.assertEqual(metadata["status"], "completed")
            self.assertEqual(metadata["mode"], "original")
            self.assertEqual(len(metadata["inputs"]), 9)
            self.assertTrue(
                all(item["sha256"] != "not_computed" for item in metadata["inputs"])
            )
            self.assertTrue((run_directory / "time_orig.root").is_file())
            self.assertTrue((run_directory / "run_report.tsv").is_file())
            self.assertTrue((run_directory / "input_manifest.tsv").is_file())

            with self.assertRaises(RuntimeError):
                DATA_PREPROCESSING.run_m5_spectra(arguments)

    def test_m6_run_records_manifest_calibration_and_grouped_inputs(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            inputs = root / "inputs"
            results = root / "results"
            build.mkdir()
            inputs.mkdir()
            calibration = root / "energy_calibration.root"
            calibration.write_bytes(b"synthetic-calibration")
            run_file = inputs / "a20240308_SnSn_GOAL_ALLCOIN.003.part0.root"
            run_file.write_bytes(b"synthetic-raw-events")
            manifest = root / "runs.tsv"
            manifest.write_text(
                "index\trun_id\traw_pattern\toutput_name\n"
                "0\ta20240308_SnSn_GOAL_ALLCOIN.003\t"
                "a20240308_SnSn_GOAL_ALLCOIN.003*.root\t"
                "a20240308_SnSn_GOAL_ALLCOIN.003.root\n",
                encoding="utf-8",
            )
            executable = build / "build_calibrated_event_tree"
            executable.write_text(FAKE_M6_EXECUTABLE, encoding="utf-8")
            executable.chmod(0o755)
            arguments = argparse.Namespace(
                build_dir=str(build),
                results_dir=str(results),
                run_id="test-run",
                input_dir=str(inputs),
                calibration=str(calibration),
                run_manifest=str(manifest),
                hash_inputs=True,
            )
            DATA_PREPROCESSING.run_m6(arguments)
            run_directory = results / "m6" / "test-run"
            metadata = json.loads(
                (run_directory / "run_metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["status"], "completed")
            self.assertEqual(metadata["run_group_count"], 1)
            self.assertEqual(len(metadata["inputs"]), 3)
            self.assertEqual(
                {item["role"] for item in metadata["inputs"]},
                {"calibration", "run_manifest", "raw:a20240308_SnSn_GOAL_ALLCOIN.003"},
            )
            self.assertTrue(
                (run_directory / "events" /
                 "a20240308_SnSn_GOAL_ALLCOIN.003.root").is_file()
            )
            self.assertTrue(
                (run_directory / "reports" /
                 "a20240308_SnSn_GOAL_ALLCOIN.003.tsv").is_file()
            )

    def test_m7_run_resolves_exact_m6_outputs_and_records_diagnostics(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            inputs = root / "events"
            results = root / "results"
            build.mkdir()
            inputs.mkdir()
            event_name = "a20240308_SnSn_GOAL_ALLCOIN.003.root"
            event_file = inputs / event_name
            event_file.write_bytes(b"synthetic-calibrated-events")
            manifest = root / "runs.tsv"
            manifest.write_text(
                "index\trun_id\traw_pattern\toutput_name\n"
                "0\ta20240308_SnSn_GOAL_ALLCOIN.003\t"
                "a20240308_SnSn_GOAL_ALLCOIN.003*.root\t"
                + event_name + "\n",
                encoding="utf-8",
            )
            executable = build / "build_neighbor_time_diagnostics"
            executable.write_text(FAKE_M7_EXECUTABLE, encoding="utf-8")
            executable.chmod(0o755)
            arguments = argparse.Namespace(
                build_dir=str(build),
                results_dir=str(results),
                run_id="test-run",
                input_dir=str(inputs),
                run_manifest=str(manifest),
                hash_inputs=True,
            )
            DATA_PREPROCESSING.run_m7(arguments)
            run_directory = results / "m7" / "test-run"
            metadata = json.loads(
                (run_directory / "run_metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["status"], "completed")
            self.assertEqual(metadata["input_file_count"], 1)
            self.assertEqual(len(metadata["inputs"]), 2)
            self.assertEqual(
                {item["role"] for item in metadata["inputs"]},
                {"run_manifest", "calibrated_event_tree:a20240308_SnSn_GOAL_ALLCOIN.003"},
            )
            self.assertTrue(
                (run_directory / "neighbor_time_diagnostics.root").is_file()
            )
            self.assertTrue((run_directory / "run_report.tsv").is_file())

            with self.assertRaises(RuntimeError):
                DATA_PREPROCESSING.run_m7(arguments)

    def test_m8_run_accepts_only_explicit_unique_calibrated_trees(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            inputs = root / "events"
            results = root / "results"
            build.mkdir()
            inputs.mkdir()
            first = inputs / "run_a.root"
            second = inputs / "run_b.root"
            first.write_bytes(b"synthetic-calibrated-events-a")
            second.write_bytes(b"synthetic-calibrated-events-b")
            executable = build / "build_reconstructed_event_tree"
            executable.write_text(FAKE_M8_EXECUTABLE, encoding="utf-8")
            executable.chmod(0o755)
            arguments = argparse.Namespace(
                build_dir=str(build),
                results_dir=str(results),
                run_id="test-run",
                input=[str(first), str(second)],
                hash_inputs=True,
            )
            DATA_PREPROCESSING.run_m8(arguments)
            run_directory = results / "m8" / "test-run"
            metadata = json.loads(
                (run_directory / "run_metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["status"], "completed")
            self.assertEqual(metadata["input_file_count"], 2)
            self.assertEqual(len(metadata["inputs"]), 2)
            self.assertTrue(
                all(item["role"] == "calibrated_event_tree" for item in metadata["inputs"])
            )
            self.assertTrue((run_directory / "reconstructed_events.root").is_file())
            self.assertTrue((run_directory / "run_report.tsv").is_file())

            duplicate_arguments = argparse.Namespace(
                build_dir=str(build),
                results_dir=str(results),
                run_id="duplicate-run",
                input=[str(first), str(first)],
                hash_inputs=False,
            )
            with self.assertRaises(RuntimeError):
                DATA_PREPROCESSING.run_m8(duplicate_arguments)

    def test_m9_run_reconstructs_and_merges_manifest_defined_samples(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            beam_on = root / "beam-on"
            beam_off = root / "beam-off"
            results = root / "results"
            for directory in (build, beam_on, beam_off):
                directory.mkdir()

            on_name = "a20240308_SnSn_GOAL_ALLCOIN.003.root"
            off_name = "a20240308_BKG_ALLOR.000.root"
            (beam_on / on_name).write_bytes(b"synthetic-beam-on")
            (beam_off / off_name).write_bytes(b"synthetic-beam-off")
            on_manifest = root / "beam-on.tsv"
            off_manifest = root / "beam-off.tsv"
            on_manifest.write_text(
                "index\trun_id\traw_pattern\toutput_name\n"
                "0\ta20240308_SnSn_GOAL_ALLCOIN.003\tunused*.root\t"
                + on_name + "\n",
                encoding="utf-8",
            )
            off_manifest.write_text(
                "index\trun_id\traw_pattern\toutput_name\n"
                "0\ta20240308_BKG_ALLOR.000\tunused*.root\t"
                + off_name + "\n",
                encoding="utf-8",
            )
            reconstruction = build / "build_reconstructed_event_tree"
            reconstruction.write_text(FAKE_M8_EXECUTABLE, encoding="utf-8")
            reconstruction.chmod(0o755)
            merger = build / "merge_reconstructed_spectra"
            merger.write_text(FAKE_M9_MERGE_EXECUTABLE, encoding="utf-8")
            merger.chmod(0o755)

            arguments = argparse.Namespace(
                build_dir=str(build),
                results_dir=str(results),
                run_id="test-run",
                beam_on_input_dir=str(beam_on),
                beam_off_input_dir=str(beam_off),
                beam_on_run_manifest=str(on_manifest),
                beam_off_run_manifest=str(off_manifest),
                hash_inputs=True,
            )
            DATA_PREPROCESSING.run_m9(arguments)
            run_directory = results / "m9" / "test-run"
            metadata = json.loads(
                (run_directory / "run_metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["status"], "completed")
            self.assertEqual(metadata["beam_on_run_group_count"], 1)
            self.assertEqual(metadata["beam_off_run_group_count"], 1)
            self.assertEqual(len(metadata["inputs"]), 4)
            self.assertEqual(len(metadata["commands"]), 4)
            self.assertTrue(
                (run_directory / "beam-on" / "all_notree.root").is_file()
            )
            self.assertTrue(
                (run_directory / "beam-on" / "all_recon.root").is_file()
            )
            self.assertTrue(
                (run_directory / "beam-off" / "all_notree_BKG.root").is_file()
            )
            self.assertTrue(
                (run_directory / "beam-off" / "all_recon_BKG.root").is_file()
            )
            self.assertTrue(
                (run_directory / "beam-on" / "reconstructed_runs" / on_name).is_file()
            )
            self.assertTrue(
                (run_directory / "beam-off" / "reconstructed_runs" / off_name).is_file()
            )

            with self.assertRaises(RuntimeError):
                DATA_PREPROCESSING.run_m9(arguments)

    def test_m10_run_resolves_exact_m9_reconstructed_samples(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            beam_on = root / "beam-on"
            beam_off = root / "beam-off"
            results = root / "results"
            for directory in (build, beam_on, beam_off):
                directory.mkdir()
            on_name = "a20240308_SnSn_GOAL_ALLCOIN.003.root"
            off_name = "a20240308_BKG_ALLOR.000.root"
            (beam_on / on_name).write_bytes(b"synthetic-m8-beam-on")
            (beam_off / off_name).write_bytes(b"synthetic-m8-beam-off")
            on_manifest = root / "beam-on.tsv"
            off_manifest = root / "beam-off.tsv"
            on_manifest.write_text(
                "index\trun_id\traw_pattern\toutput_name\n"
                "0\ta20240308_SnSn_GOAL_ALLCOIN.003\tunused*.root\t"
                + on_name + "\n",
                encoding="utf-8",
            )
            off_manifest.write_text(
                "index\trun_id\traw_pattern\toutput_name\n"
                "0\ta20240308_BKG_ALLOR.000\tunused*.root\t"
                + off_name + "\n",
                encoding="utf-8",
            )
            executable = build / "build_chapter3_diagnostics"
            executable.write_text(FAKE_M10_EXECUTABLE, encoding="utf-8")
            executable.chmod(0o755)
            arguments = argparse.Namespace(
                build_dir=str(build),
                results_dir=str(results),
                run_id="test-run",
                beam_on_input_dir=str(beam_on),
                beam_off_input_dir=str(beam_off),
                beam_on_run_manifest=str(on_manifest),
                beam_off_run_manifest=str(off_manifest),
                hash_inputs=True,
            )
            DATA_PREPROCESSING.run_m10(arguments)
            run_directory = results / "m10" / "test-run"
            metadata = json.loads(
                (run_directory / "run_metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["status"], "completed")
            self.assertEqual(metadata["stage"], "m10a")
            self.assertEqual(len(metadata["inputs"]), 4)
            self.assertEqual(len(metadata["commands"]), 2)
            self.assertTrue(
                (run_directory / "beam-on" / "h2_check.root").is_file()
            )
            self.assertTrue(
                (run_directory / "beam-off" / "h2_check_BKG.root").is_file()
            )

            with self.assertRaises(RuntimeError):
                DATA_PREPROCESSING.run_m10(arguments)

    def test_m10b_run_resolves_exact_beam_on_reconstructed_samples(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            beam_on = root / "beam-on"
            results = root / "results"
            build.mkdir()
            beam_on.mkdir()
            name = "a20240308_SnSn_GOAL_ALLCOIN.003.root"
            (beam_on / name).write_bytes(b"synthetic-m8-beam-on")
            manifest = root / "beam-on.tsv"
            manifest.write_text(
                "index\trun_id\traw_pattern\toutput_name\n"
                "0\ta20240308_SnSn_GOAL_ALLCOIN.003\tunused*.root\t"
                + name + "\n",
                encoding="utf-8",
            )
            executable = build / "build_trigger_diagnostics"
            executable.write_text(FAKE_M10B_EXECUTABLE, encoding="utf-8")
            executable.chmod(0o755)
            arguments = argparse.Namespace(
                build_dir=str(build), results_dir=str(results),
                run_id="test-run", beam_on_input_dir=str(beam_on),
                beam_on_run_manifest=str(manifest), hash_inputs=True,
            )
            DATA_PREPROCESSING.run_m10b(arguments)
            run_directory = results / "m10b" / "test-run"
            metadata = json.loads(
                (run_directory / "run_metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["status"], "completed")
            self.assertEqual(metadata["stage"], "m10b")
            self.assertEqual(len(metadata["inputs"]), 2)
            self.assertEqual(len(metadata["commands"]), 1)
            self.assertTrue((run_directory / "trigger_diagnostics.root").is_file())
            self.assertTrue((run_directory / "trigger_diagnostics.run.tsv").is_file())
            with self.assertRaises(RuntimeError):
                DATA_PREPROCESSING.run_m10b(arguments)

    def test_m11_run_keeps_slow_and_fast_background_definitions_separate(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            beam_on = root / "beam-on"
            results = root / "results"
            build.mkdir()
            beam_on.mkdir()
            name = "a20240308_SnSn_GOAL_ALLCOIN.003.root"
            (beam_on / name).write_bytes(b"synthetic-m8-beam-on")
            manifest = root / "beam-on.tsv"
            manifest.write_text(
                "index\trun_id\traw_pattern\toutput_name\n"
                "0\ta20240308_SnSn_GOAL_ALLCOIN.003\tunused*.root\t"
                + name + "\n",
                encoding="utf-8",
            )
            slow_signal = root / "all_recon.root"
            slow_background = root / "all_recon_BKG.root"
            slow_signal.write_bytes(b"synthetic-slow-signal")
            slow_background.write_bytes(b"synthetic-slow-background")
            fast_executable = build / "build_fast_coincidence_spectra"
            spectrum_executable = build / "build_observed_spectrum"
            fast_executable.write_text(FAKE_M11_FAST_EXECUTABLE, encoding="utf-8")
            spectrum_executable.write_text(
                FAKE_M11_SPECTRUM_EXECUTABLE, encoding="utf-8"
            )
            fast_executable.chmod(0o755)
            spectrum_executable.chmod(0o755)
            arguments = argparse.Namespace(
                build_dir=str(build), results_dir=str(results),
                run_id="test-run", slow_signal=str(slow_signal),
                slow_background=str(slow_background),
                beam_on_input_dir=str(beam_on),
                beam_on_run_manifest=str(manifest), hash_inputs=True,
            )
            DATA_PREPROCESSING.run_m11(arguments)
            run_directory = results / "m11" / "test-run"
            metadata = json.loads(
                (run_directory / "run_metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["status"], "completed")
            self.assertEqual(metadata["stage"], "m11")
            self.assertEqual(len(metadata["commands"]), 3)
            self.assertEqual(metadata["commands"][1][6], "slow")
            self.assertEqual(metadata["commands"][2][6], "fast")
            self.assertTrue((run_directory / "slow" / "spectrum_110.root").is_file())
            self.assertTrue((run_directory / "fast" / "fast_window.root").is_file())
            self.assertTrue((run_directory / "fast" / "random_window.root").is_file())
            self.assertTrue((run_directory / "fast" / "spectrum_110.root").is_file())
            with self.assertRaises(RuntimeError):
                DATA_PREPROCESSING.run_m11(arguments)

    def test_m12_run_records_only_the_observed_spectrum_interface(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            results = root / "results"
            build.mkdir()
            observed_spectrum = root / "spectrum_110.root"
            observed_spectrum.write_bytes(b"synthetic-th1d-histdiff")
            executable = build / "inspect_observed_spectrum"
            executable.write_text(FAKE_M12_EXECUTABLE, encoding="utf-8")
            executable.chmod(0o755)
            arguments = argparse.Namespace(
                build_dir=str(build), results_dir=str(results),
                run_id="test-run", observed_spectrum=str(observed_spectrum),
                hash_inputs=True,
            )
            DATA_PREPROCESSING.run_m12(arguments)
            run_directory = results / "m12" / "test-run"
            metadata = json.loads(
                (run_directory / "run_metadata.json").read_text(encoding="utf-8")
            )
            self.assertEqual(metadata["status"], "completed")
            self.assertEqual(metadata["stage"], "m12")
            self.assertEqual(len(metadata["commands"]), 1)
            self.assertEqual(len(metadata["inputs"]), 1)
            self.assertEqual(
                metadata["inputs"][0]["role"],
                "detector-level-observed-spectrum",
            )
            self.assertTrue(
                (run_directory / "observed_spectrum_interface.tsv").is_file()
            )
            self.assertFalse(any(path.suffix == ".root" for path in run_directory.iterdir()))
            with self.assertRaises(RuntimeError):
                DATA_PREPROCESSING.run_m12(arguments)


if __name__ == "__main__":
    unittest.main()
