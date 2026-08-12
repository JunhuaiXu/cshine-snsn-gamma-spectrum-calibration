#!/usr/bin/env python3
"""Run one bounded figure job through a reused SSH connection.

The tool is an orchestration layer only.  It uploads portable plotting files,
runs commands declared by a public-safe job definition, downloads one isolated
result directory, and verifies SHA-256 values.  It does not modify historical
analysis code or encode physics selections.
"""

from __future__ import print_function

import argparse
import datetime
import hashlib
import json
import os
import re
import shlex
import shutil
import stat
import subprocess
import sys
import tempfile
from pathlib import Path, PurePosixPath


TOOL_VERSION = "0.1.0"
SCRIPT_PATH = Path(__file__).resolve()
REPOSITORY_ROOT = SCRIPT_PATH.parents[1]
DEFAULT_CONFIG = REPOSITORY_ROOT / "local" / "remote.json"
DEFAULT_JOB_DIRECTORY = REPOSITORY_ROOT / "remote_jobs"
DEFAULT_DOWNLOAD_ROOT = REPOSITORY_ROOT / "results" / "remote_runs"
SAFE_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
SHA_LINE = re.compile(r"^([0-9a-fA-F]{64})[ \t]+(?:\*?)(.+)$")
FORBIDDEN_CONFIG_KEYS = ("password", "passwd", "token", "secret", "private_key")


class WorkflowError(RuntimeError):
    pass


class CommandFailure(WorkflowError):
    def __init__(self, command, return_code, output):
        WorkflowError.__init__(
            self,
            "command failed with exit code {0}: {1}".format(
                return_code, " ".join(str(item) for item in command)
            ),
        )
        self.command = [str(item) for item in command]
        self.return_code = return_code
        self.output = output


def utc_now():
    return datetime.datetime.utcnow().replace(microsecond=0).isoformat() + "Z"


def default_run_id():
    return datetime.datetime.utcnow().strftime("%Y%m%dT%H%M%SZ")


def sha256_file(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        while True:
            block = stream.read(1024 * 1024)
            if not block:
                break
            digest.update(block)
    return digest.hexdigest()


def read_json(path):
    with Path(path).open("r", encoding="utf-8") as stream:
        return json.load(stream)


def write_json_atomic(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    with temporary.open("w", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, sort_keys=True, ensure_ascii=False)
        stream.write("\n")
    os.replace(str(temporary), str(path))


def run_command(command, check=True, echo=True):
    command = [str(item) for item in command]
    process = subprocess.Popen(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    )
    output, _ = process.communicate()
    output = output or ""
    if echo and output:
        print(output, end="" if output.endswith("\n") else "\n")
    if check and process.returncode != 0:
        raise CommandFailure(command, process.returncode, output)
    return process.returncode, output


def require_program(name):
    if shutil.which(name) is None:
        raise WorkflowError("required program is not available: {0}".format(name))


def reject_credential_fields(value, prefix=""):
    if isinstance(value, dict):
        for key, item in value.items():
            lowered = str(key).lower()
            if any(marker in lowered for marker in FORBIDDEN_CONFIG_KEYS):
                raise WorkflowError(
                    "credentials must not be stored in remote config: {0}{1}".format(
                        prefix, key
                    )
                )
            reject_credential_fields(item, prefix + str(key) + ".")
    elif isinstance(value, list):
        for index, item in enumerate(value):
            reject_credential_fields(item, prefix + str(index) + ".")


def require_private_permissions(path):
    mode = stat.S_IMODE(Path(path).stat().st_mode)
    if mode & 0o077:
        raise WorkflowError(
            "local remote config must not be readable by group or others; "
            "run chmod 600 {0}".format(path)
        )


def validate_config(config, config_path=None):
    reject_credential_fields(config)
    required = ("host", "user", "port", "analysis_root", "remote_workdir")
    missing = [key for key in required if key not in config]
    if missing:
        raise WorkflowError("remote config is missing: {0}".format(", ".join(missing)))
    if not str(config["host"]).strip() or not str(config["user"]).strip():
        raise WorkflowError("remote host and user must be non-empty")
    port = int(config["port"])
    if port < 1 or port > 65535:
        raise WorkflowError("remote port is outside 1--65535")
    for key in ("analysis_root", "remote_workdir"):
        if not PurePosixPath(str(config[key])).is_absolute():
            raise WorkflowError("{0} must be an absolute remote path".format(key))
    persist = int(config.get("control_persist_seconds", 900))
    if persist < 60 or persist > 86400:
        raise WorkflowError("control_persist_seconds must be between 60 and 86400")
    attempts = int(config.get("connection_attempts", 2))
    if attempts < 1 or attempts > 3:
        raise WorkflowError("connection_attempts must be between 1 and 3")
    environment_setup = config.get("environment_setup", [])
    if not isinstance(environment_setup, list):
        raise WorkflowError("environment_setup must be a list of shell commands")
    for command in environment_setup:
        if not isinstance(command, str) or not command.strip():
            raise WorkflowError(
                "environment_setup entries must be non-empty shell commands"
            )
        if "\n" in command or "\r" in command:
            raise WorkflowError(
                "environment_setup entries must each occupy one line"
            )
    if config_path is not None:
        require_private_permissions(config_path)
    return config


def safe_relative_path(value, label):
    path = PurePosixPath(str(value))
    if path.is_absolute() or ".." in path.parts or not path.parts:
        raise WorkflowError("{0} must be a safe relative path: {1}".format(label, value))
    return path


def validate_job(job):
    required = ("schema_version", "job_id", "uploads", "commands", "outputs")
    missing = [key for key in required if key not in job]
    if missing:
        raise WorkflowError("job definition is missing: {0}".format(", ".join(missing)))
    if int(job["schema_version"]) != 1:
        raise WorkflowError("unsupported job schema version")
    job_id = str(job["job_id"])
    if not SAFE_ID.match(job_id):
        raise WorkflowError("invalid job_id: {0}".format(job_id))
    if not job["uploads"] or not job["commands"] or not job["outputs"]:
        raise WorkflowError("uploads, commands, and outputs must be non-empty")
    remote_names = []
    for entry in job["uploads"]:
        if not isinstance(entry, dict) or "local" not in entry:
            raise WorkflowError("each upload must contain a local path")
        safe_relative_path(entry["local"], "upload local path")
        remote_name = entry.get("remote_name", PurePosixPath(entry["local"]).name)
        remote_path = safe_relative_path(remote_name, "upload remote_name")
        if len(remote_path.parts) != 1:
            raise WorkflowError("upload remote_name must be a file name")
        remote_names.append(str(remote_path))
    if len(remote_names) != len(set(remote_names)):
        raise WorkflowError("duplicate upload remote_name values")
    for command in job["commands"]:
        if not isinstance(command, list) or not command:
            raise WorkflowError("each command must be a non-empty argument list")
        if not all(isinstance(item, str) and item for item in command):
            raise WorkflowError("command arguments must be non-empty strings")
    for output in job["outputs"]:
        safe_relative_path(output, "output path")
    return job


def load_config(path):
    path = Path(path).expanduser().resolve()
    if not path.is_file():
        raise WorkflowError(
            "private remote config not found: {0}; copy the public example first".format(
                path
            )
        )
    return validate_config(read_json(path), path), path


def resolve_job(value):
    candidate = Path(value)
    if not candidate.suffix:
        candidate = DEFAULT_JOB_DIRECTORY / (str(value) + ".json")
    elif not candidate.is_absolute():
        candidate = (Path.cwd() / candidate).resolve()
    if not candidate.is_file():
        raise WorkflowError("job definition not found: {0}".format(candidate))
    return validate_job(read_json(candidate)), candidate.resolve()


def connection_values(config):
    return {
        "port": int(config["port"]),
        "control_path": str(config.get("control_path", "%d/.ssh/cm-%C")),
        "control_persist": int(config.get("control_persist_seconds", 900)),
        "connect_timeout": int(config.get("connect_timeout_seconds", 15)),
        "connection_attempts": int(config.get("connection_attempts", 2)),
        "server_alive_interval": int(config.get("server_alive_interval_seconds", 60)),
        "server_alive_count_max": int(config.get("server_alive_count_max", 3)),
        "target": "{0}@{1}".format(config["user"], config["host"]),
    }


def ssh_options(config, for_scp=False, master_mode="auto"):
    values = connection_values(config)
    options = ["-P" if for_scp else "-p", str(values["port"])]
    option_pairs = [
        ("ControlMaster", master_mode),
        ("ControlPath", values["control_path"]),
        ("ControlPersist", str(values["control_persist"])),
        ("ConnectTimeout", str(values["connect_timeout"])),
        ("ConnectionAttempts", str(values["connection_attempts"])),
        ("ServerAliveInterval", str(values["server_alive_interval"])),
        ("ServerAliveCountMax", str(values["server_alive_count_max"])),
    ]
    for key, value in option_pairs:
        options.extend(["-o", "{0}={1}".format(key, value)])
    return options


def master_status(config, echo=False):
    values = connection_values(config)
    command = [
        "ssh",
        "-p",
        str(values["port"]),
        "-o",
        "ControlPath={0}".format(values["control_path"]),
        "-O",
        "check",
        values["target"],
    ]
    return run_command(command, check=False, echo=echo)


def ensure_master(config):
    status, _ = master_status(config, echo=False)
    if status == 0:
        print("reusing existing SSH master connection")
        return False
    values = connection_values(config)
    command = ["ssh"] + ssh_options(config, master_mode="yes")
    command.extend(["-M", "-N", "-f", values["target"]])
    print("opening one reusable SSH master connection")
    run_command(command)
    status, output = master_status(config, echo=False)
    if status != 0:
        raise WorkflowError("SSH master connection did not become available: " + output)
    return True


def close_master(config):
    values = connection_values(config)
    command = [
        "ssh",
        "-p",
        str(values["port"]),
        "-o",
        "ControlPath={0}".format(values["control_path"]),
        "-O",
        "exit",
        values["target"],
    ]
    return run_command(command, check=False)


def render_argument(value, context):
    try:
        return str(value).format(**context)
    except KeyError as error:
        raise WorkflowError("unknown job placeholder: {0}".format(error))


def render_commands(job, context):
    commands = []
    for command in job["commands"]:
        commands.append([render_argument(item, context) for item in command])
    return commands


def shell_join(arguments):
    return " ".join(shlex.quote(str(item)) for item in arguments)


def compile_uploaded_python(upload_paths):
    for path in upload_paths:
        if path.suffix != ".py":
            continue
        source = path.read_text(encoding="utf-8")
        compile(source, str(path), "exec")


def parse_remote_hashes(output):
    values = {}
    for raw_line in output.splitlines():
        match = SHA_LINE.match(raw_line.strip())
        if match:
            values[match.group(2)] = match.group(1).lower()
    return values


def command_validate(args):
    job, job_path = resolve_job(args.job)
    config = None
    config_path = None
    if args.config:
        config, config_path = load_config(args.config)
    upload_paths = []
    for entry in job["uploads"]:
        path = (REPOSITORY_ROOT / entry["local"]).resolve()
        try:
            path.relative_to(REPOSITORY_ROOT)
        except ValueError:
            raise WorkflowError("upload escapes repository root: {0}".format(path))
        if not path.is_file():
            raise WorkflowError("upload file is missing: {0}".format(path))
        upload_paths.append(path)
    compile_uploaded_python(upload_paths)
    print("PASS job={0} uploads={1} outputs={2}".format(
        job["job_id"], len(upload_paths), len(job["outputs"])
    ))
    print("job definition: {0}".format(job_path))
    if config is not None:
        print("private config: {0}".format(config_path))
    return 0


def command_list(_args):
    for path in sorted(DEFAULT_JOB_DIRECTORY.glob("*.json")):
        try:
            job = validate_job(read_json(path))
            print("{0}\t{1}".format(job["job_id"], job.get("description", "")))
        except (ValueError, WorkflowError) as error:
            print("INVALID\t{0}\t{1}".format(path.name, error))
    return 0


def command_status(args):
    config, _ = load_config(args.config)
    status, output = master_status(config, echo=False)
    if status == 0:
        print("SSH master connection is active")
        if output.strip():
            print(output.strip())
        return 0
    print("SSH master connection is not active")
    return 1


def command_close(args):
    config, _ = load_config(args.config)
    status, output = close_master(config)
    if status == 0:
        print("SSH master connection closed")
        return 0
    if output.strip():
        print(output.strip())
    return 1


def command_run(args):
    require_program("ssh")
    require_program("scp")
    config, config_path = load_config(args.config)
    job, job_path = resolve_job(args.job)
    run_id = args.run_id or default_run_id()
    if not SAFE_ID.match(run_id):
        raise WorkflowError("invalid run_id: {0}".format(run_id))

    upload_paths = []
    remote_names = []
    for entry in job["uploads"]:
        path = (REPOSITORY_ROOT / entry["local"]).resolve()
        try:
            path.relative_to(REPOSITORY_ROOT)
        except ValueError:
            raise WorkflowError("upload escapes repository root: {0}".format(path))
        if not path.is_file():
            raise WorkflowError("upload file is missing: {0}".format(path))
        upload_paths.append(path)
        remote_names.append(entry.get("remote_name", path.name))
    compile_uploaded_python(upload_paths)

    remote_workdir = str(PurePosixPath(str(config["remote_workdir"])))
    remote_run_dir = str(
        PurePosixPath(remote_workdir) / "remote_runs" / job["job_id"] / run_id
    )
    remote_code_dir = str(PurePosixPath(remote_run_dir) / "code")
    remote_output_dir = str(PurePosixPath(remote_run_dir) / "results")
    context = {
        "analysis_root": str(PurePosixPath(str(config["analysis_root"]))),
        "remote_workdir": remote_workdir,
        "remote_run_dir": remote_run_dir,
        "remote_code_dir": remote_code_dir,
        "run_output_dir": remote_output_dir,
        "run_id": run_id,
        "job_id": job["job_id"],
    }
    commands = render_commands(job, context)
    remote_outputs = [
        str(PurePosixPath(remote_output_dir) / safe_relative_path(item, "output"))
        for item in job["outputs"]
    ]

    download_root = Path(args.download_root).expanduser().resolve()
    local_run_dir = download_root / job["job_id"] / run_id
    if local_run_dir.exists():
        raise WorkflowError("local run directory already exists: {0}".format(local_run_dir))

    ensure_master(config)
    values = connection_values(config)
    mkdir_command = "mkdir -p {0} {1}".format(
        shlex.quote(remote_code_dir), shlex.quote(remote_output_dir)
    )
    run_command(
        ["ssh"] + ssh_options(config) + [values["target"], mkdir_command]
    )

    with tempfile.TemporaryDirectory(prefix="remote-figure-upload-") as directory:
        staging_directory = Path(directory)
        staged_paths = []
        for path, remote_name in zip(upload_paths, remote_names):
            staged_path = staging_directory / remote_name
            shutil.copy2(str(path), str(staged_path))
            staged_paths.append(staged_path)
        upload_command = ["scp"] + ssh_options(config, for_scp=True)
        upload_command.extend([str(path) for path in staged_paths])
        upload_command.append(
            "{0}:{1}/".format(values["target"], shlex.quote(remote_code_dir))
        )
        run_command(upload_command)

    # Environment setup belongs to the private configuration because module
    # paths and software installations are host-specific.  The public job
    # definition remains portable and contains only the executable command.
    run_parts = list(config.get("environment_setup", []))
    run_parts.append("cd {0}".format(shlex.quote(remote_workdir)))
    run_parts.extend(shell_join(command) for command in commands)
    run_parts.append("sha256sum " + " ".join(shlex.quote(item) for item in remote_outputs))
    remote_command = " && ".join(run_parts)
    _, remote_output = run_command(
        ["ssh"] + ssh_options(config) + [values["target"], remote_command]
    )
    remote_hashes = parse_remote_hashes(remote_output)
    missing_hashes = [item for item in remote_outputs if item not in remote_hashes]
    if missing_hashes:
        raise WorkflowError(
            "remote run did not report SHA-256 for: {0}".format(
                ", ".join(missing_hashes)
            )
        )

    local_run_dir.parent.mkdir(parents=True, exist_ok=True)
    download_command = ["scp"] + ssh_options(config, for_scp=True)
    download_command.extend(
        ["-r", "{0}:{1}".format(values["target"], shlex.quote(remote_output_dir)),
         str(local_run_dir)]
    )
    run_command(download_command)

    artifacts = []
    for relative_value, remote_path in zip(job["outputs"], remote_outputs):
        local_path = local_run_dir / Path(relative_value)
        if not local_path.is_file():
            raise WorkflowError("downloaded output is missing: {0}".format(local_path))
        local_hash = sha256_file(local_path)
        remote_hash = remote_hashes[remote_path]
        if local_hash != remote_hash:
            raise WorkflowError(
                "SHA-256 mismatch after download: {0}".format(relative_value)
            )
        artifacts.append(
            {
                "path": str(relative_value),
                "bytes": local_path.stat().st_size,
                "sha256": local_hash,
            }
        )

    metadata = {
        "schema_version": 1,
        "tool": {"name": "remote_figure.py", "version": TOOL_VERSION},
        "completed_utc": utc_now(),
        "job_id": job["job_id"],
        "run_id": run_id,
        "job_definition": str(job_path),
        "job_definition_sha256": sha256_file(job_path),
        "private_config": str(config_path),
        "connection": {
            "host": config["host"],
            "port": int(config["port"]),
            "user": config["user"],
            "master_reuse": True,
            "control_persist_seconds": int(config.get("control_persist_seconds", 900)),
            "automatic_retry_loop": False,
        },
        "uploads": [
            {
                "path": str(path.relative_to(REPOSITORY_ROOT)),
                "remote_name": name,
                "sha256": sha256_file(path),
            }
            for path, name in zip(upload_paths, remote_names)
        ],
        "artifacts": artifacts,
    }
    write_json_atomic(local_run_dir / "remote_workflow.json", metadata)
    print("PASS remote figure job: {0}".format(job["job_id"]))
    print("local run directory: {0}".format(local_run_dir))
    print("the reusable SSH connection will expire after the configured idle period")
    return 0


def build_parser():
    parser = argparse.ArgumentParser(
        description="Run bounded figure jobs through one reusable SSH connection."
    )
    subparsers = parser.add_subparsers(dest="command")

    list_parser = subparsers.add_parser("list", help="list public-safe job definitions")
    list_parser.set_defaults(function=command_list)

    validate_parser = subparsers.add_parser(
        "validate", help="validate one job without connecting to a server"
    )
    validate_parser.add_argument("job")
    validate_parser.add_argument("--config")
    validate_parser.set_defaults(function=command_validate)

    run_parser = subparsers.add_parser(
        "run", help="upload, run, download, and checksum one isolated job"
    )
    run_parser.add_argument("job")
    run_parser.add_argument("--config", default=str(DEFAULT_CONFIG))
    run_parser.add_argument("--run-id")
    run_parser.add_argument("--download-root", default=str(DEFAULT_DOWNLOAD_ROOT))
    run_parser.set_defaults(function=command_run)

    status_parser = subparsers.add_parser("status", help="check the reusable connection")
    status_parser.add_argument("--config", default=str(DEFAULT_CONFIG))
    status_parser.set_defaults(function=command_status)

    close_parser = subparsers.add_parser("close", help="close the reusable connection")
    close_parser.add_argument("--config", default=str(DEFAULT_CONFIG))
    close_parser.set_defaults(function=command_close)
    return parser


def main(argv=None):
    parser = build_parser()
    args = parser.parse_args(argv)
    if not hasattr(args, "function"):
        parser.print_help()
        return 2
    try:
        return args.function(args)
    except (OSError, ValueError, WorkflowError) as error:
        print("ERROR: {0}".format(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
