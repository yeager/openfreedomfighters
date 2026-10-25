#!/usr/bin/env python3
"""Emit a deterministic compile and lookup plan for FidelityFX FSR3 GLSL."""

from __future__ import annotations

import argparse
import itertools
import json
import subprocess
import sys
from pathlib import Path


AXES = (
    ("REPROJECT_USE_LANCZOS_TYPE", "FFX_FSR3UPSCALER_OPTION_REPROJECT_USE_LANCZOS_TYPE"),
    ("HDR_COLOR_INPUT", "FFX_FSR3UPSCALER_OPTION_HDR_COLOR_INPUT"),
    ("LOW_RESOLUTION_MOTION_VECTORS", "FFX_FSR3UPSCALER_OPTION_LOW_RESOLUTION_MOTION_VECTORS"),
    ("JITTERED_MOTION_VECTORS", "FFX_FSR3UPSCALER_OPTION_JITTERED_MOTION_VECTORS"),
    ("INVERTED_DEPTH", "FFX_FSR3UPSCALER_OPTION_INVERTED_DEPTH"),
    ("APPLY_SHARPENING", "FFX_FSR3UPSCALER_OPTION_APPLY_SHARPENING"),
)

PASS_AXES = {
    "accumulate_pass": tuple(axis[0] for axis in AXES),
    "autogen_reactive_pass": ("JITTERED_MOTION_VECTORS", "INVERTED_DEPTH"),
    "debug_view_pass": ("JITTERED_MOTION_VECTORS", "INVERTED_DEPTH"),
    "luma_instability_pass": ("JITTERED_MOTION_VECTORS", "INVERTED_DEPTH"),
    "luma_pyramid_pass": ("JITTERED_MOTION_VECTORS", "INVERTED_DEPTH"),
    "prepare_inputs_pass": (
        "LOW_RESOLUTION_MOTION_VECTORS",
        "JITTERED_MOTION_VECTORS",
        "INVERTED_DEPTH",
    ),
    "prepare_reactivity_pass": ("JITTERED_MOTION_VECTORS", "INVERTED_DEPTH"),
    "rcas_pass": ("JITTERED_MOTION_VECTORS", "INVERTED_DEPTH"),
    "shading_change_pass": ("JITTERED_MOTION_VECTORS", "INVERTED_DEPTH"),
    "shading_change_pyramid_pass": ("JITTERED_MOTION_VECTORS", "INVERTED_DEPTH"),
}

VARIANTS = (
    ("", 0),
    ("wave64", 0),
    ("16bit", 1),
    ("wave64_16bit", 1),
)
SDK_COMMIT = "c6efa6bf7f2027b3ec94f28578bb5965eabb9e55"

SDK_BASE_DEFINITIONS = {
    "FFX_FSR3UPSCALER_OPTION_UPSAMPLE_SAMPLERS_USE_DATA_HALF": 0,
    "FFX_FSR3UPSCALER_OPTION_ACCUMULATE_SAMPLERS_USE_DATA_HALF": 0,
    "FFX_FSR3UPSCALER_OPTION_REPROJECT_SAMPLERS_USE_DATA_HALF": 1,
    "FFX_FSR3UPSCALER_OPTION_POSTPROCESSLOCKSTATUS_SAMPLERS_USE_DATA_HALF": 0,
    "FFX_FSR3UPSCALER_OPTION_UPSAMPLE_USE_LANCZOS_TYPE": 2,
}


def make_pass_plan(pass_name: str, sdk_root: Path) -> dict[str, object]:
    used_axes = PASS_AXES[pass_name]
    ordered_axes = used_axes + tuple(axis[0] for axis in AXES if axis[0] not in used_axes)
    bit_index = {name: index for index, name in enumerate(ordered_axes)}
    shader = (
        Path("src/backends/vk/shaders/fsr3upscaler")
        / f"ffx_fsr3upscaler_{pass_name}.glsl"
    )
    shader_path = sdk_root / shader
    if not shader_path.is_file():
        raise FileNotFoundError(f"FSR3 shader is missing: {shader_path}")

    used_value_tuples = tuple(itertools.product((0, 1), repeat=len(used_axes)))
    compile_index = {values: index for index, values in enumerate(used_value_tuples)}
    lookup: list[dict[str, object]] = []
    for values in itertools.product((0, 1), repeat=len(AXES)):
        options = {axis[0]: value for axis, value in zip(AXES, values)}
        key = sum(options[axis] << bit_index[axis] for axis in ordered_axes)
        used_values = tuple(options[axis] for axis in used_axes)
        lookup.append(
            {
                "key": key,
                "blob_index": compile_index[used_values],
                "options": options,
            }
        )
    lookup.sort(key=lambda entry: int(entry["key"]))

    variants: list[dict[str, object]] = []
    for variant_name, half in VARIANTS:
        jobs = []
        for used_values, blob_index in compile_index.items():
            definitions = dict(SDK_BASE_DEFINITIONS)
            definitions.update({
                "FFX_GPU": 1,
                "FFX_GLSL": 1,
                "FFX_HALF": half,
            })
            for axis, value in zip(used_axes, used_values):
                macro = next(macro for name, macro in AXES if name == axis)
                definitions[macro] = value
            jobs.append(
                {
                    "blob_index": blob_index,
                    "used_options": dict(zip(used_axes, used_values)),
                    "definitions": definitions,
                }
            )
        variants.append(
            {
                "name": variant_name,
                "half_precision": bool(half),
                "wave64_changes_glsl_compile": False,
                "jobs": jobs,
            }
        )

    return {
        "pass": pass_name,
        "shader": shader.as_posix(),
        "include_dirs": [
            "include/FidelityFX/gpu",
            "include/FidelityFX/gpu/fsr3upscaler",
        ],
        "axis_order": list(ordered_axes),
        "axis_fields": [
            next(macro for name, macro in AXES if name == axis)
            for axis in ordered_axes
        ],
        "used_axes": list(used_axes),
        "runtime_lookup": lookup,
        "variants": variants,
    }


def make_plan(sdk_root: Path) -> dict[str, object]:
    sdk_root = sdk_root.resolve()
    git_root = next(
        (parent for parent in (sdk_root, *sdk_root.parents) if (parent / ".git").exists()),
        None,
    )
    if git_root is None:
        raise RuntimeError("SDK root must belong to a Git checkout so its pin can be verified")
    revision = subprocess.run(
        ["git", "-C", str(git_root), "rev-parse", "HEAD"],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.strip()
    if revision != SDK_COMMIT:
        raise RuntimeError(
            f"expected FidelityFX SDK commit {SDK_COMMIT}, got {revision} at {git_root}"
        )
    return {
        "schema": 1,
        "sdk_tag": "v1.1.4",
        "sdk_commit": SDK_COMMIT,
        "shader_target": "vulkan1.2",
        "entry_point": "CS",
        "stage": "comp",
        "optimization": "-Os",
        "base_definitions": SDK_BASE_DEFINITIONS,
        "axes": [
            {"name": name, "macro": macro, "values": [0, 1]}
            for name, macro in AXES
        ],
        "passes": [make_pass_plan(pass_name, sdk_root) for pass_name in PASS_AXES],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk-root", required=True, type=Path)
    parser.add_argument("--output", type=Path, help="write JSON here; stdout by default")
    args = parser.parse_args()
    try:
        rendered = json.dumps(make_plan(args.sdk_root), indent=2, sort_keys=True) + "\n"
    except (FileNotFoundError, OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        return 2
    if args.output is None:
        sys.stdout.write(rendered)
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
