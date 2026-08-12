#!/usr/bin/env python3

from __future__ import print_function

import importlib.util
import json
import os
import stat
import tempfile
import unittest
from pathlib import Path


TOOL_PATH = Path(__file__).resolve().parents[1] / "remote_figure.py"
SPEC = importlib.util.spec_from_file_location("remote_figure", str(TOOL_PATH))
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def valid_config():
    return {
        "host": "example.org",
        "user": "researcher",
        "port": 22,
        "analysis_root": "/analysis",
        "remote_workdir": "/analysis/reproducible",
        "connection_attempts": 2,
    }


def valid_job():
    return {
        "schema_version": 1,
        "job_id": "example-job",
        "uploads": [
            {"local": "plotting/example.py", "remote_name": "example.py"}
        ],
        "commands": [
            ["python3", "{remote_code_dir}/example.py", "--output", "{run_output_dir}"]
        ],
        "outputs": ["example/output.json"],
    }


class RemoteFigureTest(unittest.TestCase):
    def test_config_rejects_credentials(self):
        config = valid_config()
        config["password"] = "must-not-be-stored"
        with self.assertRaises(MODULE.WorkflowError):
            MODULE.validate_config(config)

    def test_config_accepts_private_environment_setup(self):
        config = valid_config()
        config["environment_setup"] = ["source /opt/root/bin/thisroot.sh"]
        self.assertEqual(MODULE.validate_config(config), config)

    def test_config_rejects_multiline_environment_setup(self):
        config = valid_config()
        config["environment_setup"] = ["source /opt/root/bin/thisroot.sh\necho bad"]
        with self.assertRaises(MODULE.WorkflowError):
            MODULE.validate_config(config)

    def test_private_config_requires_restricted_permissions(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "remote.json"
            path.write_text(json.dumps(valid_config()), encoding="utf-8")
            os.chmod(str(path), stat.S_IRUSR | stat.S_IWUSR | stat.S_IRGRP)
            with self.assertRaises(MODULE.WorkflowError):
                MODULE.validate_config(valid_config(), path)

    def test_job_rejects_parent_path(self):
        job = valid_job()
        job["outputs"] = ["../private/output.json"]
        with self.assertRaises(MODULE.WorkflowError):
            MODULE.validate_job(job)

    def test_job_commands_render_only_declared_context(self):
        job = MODULE.validate_job(valid_job())
        commands = MODULE.render_commands(
            job,
            {
                "remote_code_dir": "/remote/code",
                "run_output_dir": "/remote/results",
            },
        )
        self.assertEqual(commands[0][1], "/remote/code/example.py")
        self.assertEqual(commands[0][3], "/remote/results")

    def test_remote_sha256_lines_are_parsed(self):
        output = (
            "plot completed\n"
            + "a" * 64
            + "  /remote/results/example.pdf\n"
        )
        self.assertEqual(
            MODULE.parse_remote_hashes(output)["/remote/results/example.pdf"],
            "a" * 64,
        )

    def test_ssh_options_enable_bounded_connection_reuse(self):
        options = MODULE.ssh_options(valid_config())
        joined = " ".join(options)
        self.assertIn("ControlMaster=auto", joined)
        self.assertIn("ControlPersist=900", joined)
        self.assertIn("ConnectionAttempts=2", joined)
        self.assertIn("ServerAliveInterval=60", joined)


if __name__ == "__main__":
    unittest.main()
