#!/usr/bin/env python3
"""Create a repeat-gated, source-free loader-tail contract receipt.

The tool accepts only sanitized loader-tail traces kept outside the checkout.
It does not admit the loader tail, invoke callbacks, or retain game material.
"""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import stat
import sys
from typing import Any

import intro_loader_tail_observation as observation

FORMAT = "off.intro-loader-tail-contract/v1"
MAX_TRACE_BYTES = 128 * 1024
REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent


def _outside_repository(path: pathlib.Path, label: str) -> pathlib.Path:
    """Return an absolute private path after rejecting traversal and links."""
    absolute = path if path.is_absolute() else pathlib.Path.cwd() / path
    current = pathlib.Path(absolute.anchor)
    for component in absolute.parts[1:]:
        if component == "..":
            raise ValueError(f"{label} must not contain parent traversal")
        current /= component
        try:
            metadata = os.lstat(current)
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(metadata.st_mode):
            raise ValueError(f"{label} must not traverse a symlink")
    resolved = absolute.resolve()
    if resolved.is_relative_to(REPOSITORY_ROOT):
        raise ValueError(f"{label} must be outside the repository")
    return resolved


def _open_parent(path: pathlib.Path, label: str) -> tuple[int, str]:
    try:
        descriptor = os.open(path.parent, os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    except OSError as error:
        raise ValueError(f"{label} parent must be an existing real directory") from error
    return descriptor, path.name


def _read_private_bytes(path: pathlib.Path, label: str) -> bytes:
    parent, name = _open_parent(path, label)
    try:
        try:
            descriptor = os.open(name, os.O_RDONLY | os.O_NONBLOCK | os.O_NOFOLLOW,
                                 dir_fd=parent)
        except OSError as error:
            raise ValueError(f"{label} must be a bounded regular private file") from error
        try:
            metadata = os.fstat(descriptor)
            if not stat.S_ISREG(metadata.st_mode) or not 0 < metadata.st_size <= MAX_TRACE_BYTES:
                raise ValueError(f"{label} must be a bounded regular private file")
            chunks: list[bytes] = []
            remaining = metadata.st_size
            while remaining:
                chunk = os.read(descriptor, remaining)
                if not chunk:
                    break
                chunks.append(chunk)
                remaining -= len(chunk)
            data = b"".join(chunks)
            if len(data) != metadata.st_size:
                raise ValueError(f"{label} changed while being read")
            return data
        finally:
            os.close(descriptor)
    finally:
        os.close(parent)


def _parse_trace(data: bytes, label: str) -> dict[str, Any]:
    try:
        parsed = json.loads(data.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError(f"{label} must be UTF-8 JSON") from error
    if not isinstance(parsed, dict) or frozenset(parsed) != {"format", "events"}:
        raise ValueError(f"{label} has an unsupported trace schema")
    if parsed["format"] != observation.OUTPUT_FORMAT:
        raise ValueError(f"{label} must be a sanitized loader-tail trace")
    try:
        verified = observation.sanitize_trace(
            {"format": observation.INPUT_FORMAT, "events": parsed["events"]})
    except (KeyError, TypeError, ValueError) as error:
        raise ValueError(f"{label} is not a complete sanitized loader-tail trace") from error
    if verified["events"] != parsed["events"]:
        raise ValueError(f"{label} is not a canonical sanitized loader-tail trace")
    return parsed


def _callback(trace: dict[str, Any]) -> int:
    events = trace["events"]
    return events[0]["callback_ordinal"]


def _validate_failure(success: dict[str, Any], failure: dict[str, Any]) -> None:
    events = failure["events"]
    terminal = events[-1]
    if terminal["stage"] != "failure" or terminal["outcome"] != "failure":
        raise ValueError("failure trace must end in terminal failure")
    if any(event["outcome"] == "success" for event in events):
        raise ValueError("failure trace must not report success")
    if _callback(success) != _callback(failure):
        raise ValueError("failure trace callback does not match successful traces")
    prefix = events[:-1]
    success_events = success["events"]
    if len(prefix) > len(success_events):
        raise ValueError("failure trace has an unobserved callback order")
    for index, event in enumerate(prefix):
        expected = success_events[index]
        if (event["observation_order"], event["stage"], event["callback_ordinal"]) != (
                expected["observation_order"], expected["stage"], expected["callback_ordinal"]):
            raise ValueError("failure trace callback order does not match successful traces")


def contract_from_traces(success_a_bytes: bytes, success_b_bytes: bytes,
                         failure_bytes: bytes) -> dict[str, object]:
    """Validate a repeat gate and return the intentionally small receipt."""
    if success_a_bytes != success_b_bytes:
        raise ValueError("successful traces must be byte-identical")
    if failure_bytes == success_a_bytes:
        raise ValueError("failure trace must be distinct from successful traces")
    success = _parse_trace(success_a_bytes, "first successful trace")
    second = _parse_trace(success_b_bytes, "second successful trace")
    failure = _parse_trace(failure_bytes, "failure trace")
    if success["events"][-1]["stage"] != "completion" or success["events"][-1]["outcome"] != "success":
        raise ValueError("successful traces must end in completion")
    if _callback(success) != _callback(second):
        raise ValueError("successful trace callback does not match")
    _validate_failure(success, failure)
    return {"format": FORMAT, "repeat_gate": "two-byte-identical-successes",
            "success": "complete-concrete-loader-tail", "failure": "distinct-terminal-failure"}


def _write_new_receipt(path: pathlib.Path, receipt: dict[str, object]) -> None:
    parent, name = _open_parent(path, "receipt")
    try:
        try:
            descriptor = os.open(name, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW,
                                 0o600, dir_fd=parent)
        except OSError as error:
            raise ValueError("refusing to overwrite receipt") from error
        try:
            with os.fdopen(descriptor, "w", encoding="utf-8", closefd=False) as stream:
                json.dump(receipt, stream, indent=2, sort_keys=True)
                stream.write("\n")
        finally:
            os.close(descriptor)
    finally:
        os.close(parent)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("success_a", type=pathlib.Path)
    parser.add_argument("success_b", type=pathlib.Path)
    parser.add_argument("failure", type=pathlib.Path)
    parser.add_argument("receipt", type=pathlib.Path)
    args = parser.parse_args()
    try:
        paths = [_outside_repository(value, label) for value, label in (
            (args.success_a, "first successful trace"), (args.success_b, "second successful trace"),
            (args.failure, "failure trace"), (args.receipt, "receipt"))]
        if len(set(paths)) != len(paths):
            raise ValueError("all trace and receipt paths must differ")
        if paths[-1].exists():
            raise ValueError("refusing to overwrite receipt")
        receipt = contract_from_traces(_read_private_bytes(paths[0], "first successful trace"),
                                       _read_private_bytes(paths[1], "second successful trace"),
                                       _read_private_bytes(paths[2], "failure trace"))
        _write_new_receipt(paths[3], receipt)
    except (OSError, ValueError) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("wrote source-free repeat-gated intro loader-tail contract receipt")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
