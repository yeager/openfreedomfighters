#!/usr/bin/env python3
"""Compile the pinned FSR3 Vulkan GLSL permutations into a build manifest.

This emits build products only. It does not generate FidelityFX's C++ accessor
headers yet; the manifest retains raw SPIRV-Cross reflection for that step.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from fsr3_shader_permutation_plan import make_plan


def run_checked(command: list[str]) -> subprocess.CompletedProcess[str]:
    try:
        return subprocess.run(command, check=True, capture_output=True, text=True)
    except subprocess.CalledProcessError as error:
        details = error.stderr or error.stdout or "(no diagnostic output)"
        raise RuntimeError(
            f"{command[0]} failed with exit code {error.returncode}:\n{details.strip()}"
        ) from error


def run_json(command: list[str]) -> dict[str, object]:
    result = run_checked(command)
    try:
        value = json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise RuntimeError(f"command did not emit valid JSON: {command[0]}") from error
    if not isinstance(value, dict):
        raise RuntimeError(f"command emitted a non-object JSON document: {command[0]}")
    return value


def compile_manifest(
    sdk_root: Path,
    output_root: Path,
    glslang: str,
    spirv_cross: str,
) -> dict[str, object]:
    sdk_root = sdk_root.resolve()
    output_root = output_root.resolve()
    plan = make_plan(sdk_root)
    output_root.mkdir(parents=True, exist_ok=True)
    blob_root = output_root / "blobs"
    blob_root.mkdir(exist_ok=True)
    blob_records: dict[str, dict[str, object]] = {}
    passes: list[dict[str, object]] = []

    with tempfile.TemporaryDirectory(prefix="off-fsr3-") as temp_name:
        temp_root = Path(temp_name)
        for pass_plan in plan["passes"]:
            pass_name = str(pass_plan["pass"])
            shader = sdk_root / str(pass_plan["shader"])
            include_dirs = [sdk_root / path for path in pass_plan["include_dirs"]]
            variants: list[dict[str, object]] = []
            for variant in pass_plan["variants"]:
                jobs: list[dict[str, object]] = []
                for job in variant["jobs"]:
                    compile_index = int(job["blob_index"])
                    spv_path = temp_root / f"{pass_name}-{variant['name'] or 'default'}-{compile_index}.spv"
                    command = [
                        glslang,
                        "-V",
                        "--target-env",
                        str(plan["shader_target"]),
                        "-S",
                        str(plan["stage"]),
                        "-e",
                        str(plan["entry_point"]),
                        str(plan["optimization"]),
                    ]
                    command.extend(f"-I{directory}" for directory in include_dirs)
                    command.extend(
                        f"-D{name}={value}"
                        for name, value in job["definitions"].items()
                    )
                    command.extend(("-o", str(spv_path), str(shader)))
                    run_checked(command)
                    binary = spv_path.read_bytes()
                    md5 = hashlib.md5(binary).hexdigest()
                    sha256 = hashlib.sha256(binary).hexdigest()
                    record = blob_records.get(sha256)
                    if record is None:
                        filename = f"{md5}.spv"
                        destination = blob_root / filename
                        if destination.exists() and destination.read_bytes() != binary:
                            filename = f"{md5}-{sha256[:12]}.spv"
                            destination = blob_root / filename
                        shutil.copyfile(spv_path, destination)
                        reflection = run_json([spirv_cross, str(spv_path), "--reflect"])
                        record = {
                            "file": f"blobs/{filename}",
                            "size": len(binary),
                            "md5": md5,
                            "sha256": sha256,
                            "reflection": reflection,
                        }
                        blob_records[sha256] = record
                    jobs.append(
                        {
                            "compile_index": compile_index,
                            "blob_sha256": sha256,
                            "used_options": job["used_options"],
                            "definitions": job["definitions"],
                        }
                    )
                variants.append({"name": variant["name"], "jobs": jobs})
            passes.append(
                {
                    "pass": pass_name,
                    "shader": pass_plan["shader"],
                    "axis_order": pass_plan["axis_order"],
                    "axis_fields": pass_plan["axis_fields"],
                    "used_axes": pass_plan["used_axes"],
                    "runtime_lookup": pass_plan["runtime_lookup"],
                    "variants": variants,
                }
            )

    manifest: dict[str, object] = {
        "schema": 1,
        "sdk_tag": plan["sdk_tag"],
        "sdk_commit": plan["sdk_commit"],
        "shader_target": plan["shader_target"],
        "compiler": {
            "glslang": glslang,
            "spirv_cross": spirv_cross,
            "entry_point": plan["entry_point"],
            "stage": plan["stage"],
            "optimization": plan["optimization"],
        },
        "blobs": sorted(blob_records.values(), key=lambda blob: str(blob["sha256"])),
        "passes": passes,
    }
    (output_root / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--output-root", required=True, type=Path)
    parser.add_argument("--glslang", default="glslangValidator")
    parser.add_argument("--spirv-cross", default="spirv-cross")
    args = parser.parse_args()
    try:
        manifest = compile_manifest(
            args.sdk_root, args.output_root, args.glslang, args.spirv_cross
        )
    except (OSError, RuntimeError) as error:
        print(error, file=sys.stderr)
        return 2
    print(
        f"compiled {len(manifest['passes'])} FSR3 passes; "
        f"deduplicated to {len(manifest['blobs'])} SPIR-V blobs"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
