#!/usr/bin/env python3
"""Emit FidelityFX v1.1.4 GLSL blob/permutation headers from a build manifest."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path


RESOURCE_GROUPS = (
    ("constant_buffers", "ubos", "CBV", "constantBuffer"),
    ("srv_textures", "separate_images", "TextureSRV", "srvTexture"),
    ("uav_textures", "images", "TextureUAV", "uavTexture"),
    ("srv_buffers", "srv_buffers", "BufferSRV", "srvBuffer"),
    ("uav_buffers", "uav_buffers", "BufferUAV", "uavBuffer"),
    ("samplers", "separate_samplers", "Sampler", "sampler"),
    ("rt_acceleration_structures", "acceleration_structures", "RTAccelerationStructure", "rtAccelerationStructure"),
)

RESOURCE_STRUCT_FIELDS = (
    ("numConstantBuffers", "constantBufferNames", "constantBufferBindings", "constantBufferCounts", "constantBufferSpaces"),
    ("numSRVTextures", "srvTextureNames", "srvTextureBindings", "srvTextureCounts", "srvTextureSpaces"),
    ("numUAVTextures", "uavTextureNames", "uavTextureBindings", "uavTextureCounts", "uavTextureSpaces"),
    ("numSRVBuffers", "srvBufferNames", "srvBufferBindings", "srvBufferCounts", "srvBufferSpaces"),
    ("numUAVBuffers", "uavBufferNames", "uavBufferBindings", "uavBufferCounts", "uavBufferSpaces"),
    ("numSamplers", "samplerNames", "samplerBindings", "samplerCounts", "samplerSpaces"),
    ("numRTAccelerationStructures", "rtAccelerationStructureNames", "rtAccelerationStructureBindings", "rtAccelerationStructureCounts", "rtAccelerationStructureSpaces"),
)

KNOWN_REFLECTION_KEYS = {
    "entryPoints",
    "types",
    "ubos",
    "separate_images",
    "images",
    "ssbos",
    "separate_samplers",
    "acceleration_structures",
}


def descriptor_count(resource: dict[str, object]) -> int:
    dimensions = resource.get("array", [])
    if not isinstance(dimensions, list):
        raise RuntimeError(f"unsupported descriptor array metadata: {dimensions!r}")
    count = math.prod(int(dimension) for dimension in dimensions) if dimensions else 1
    if count <= 0:
        raise RuntimeError("runtime-sized descriptor arrays are not supported")
    return count


def resource_groups(reflection: dict[str, object]) -> dict[str, list[dict[str, object]]]:
    unknown = set(reflection) - KNOWN_REFLECTION_KEYS
    if unknown:
        raise RuntimeError(f"unsupported SPIRV-Cross reflection categories: {sorted(unknown)}")
    if reflection.get("ssbos"):
        raise RuntimeError("FSR3 storage buffers need an explicit SPIR-V-Reflect-compatible mapping")

    result: dict[str, list[dict[str, object]]] = {}
    for group_name, source_key, _, _ in RESOURCE_GROUPS:
        entries = reflection.get(source_key, [])
        if entries is None:
            entries = []
        if not isinstance(entries, list):
            raise RuntimeError(f"invalid descriptor list for {source_key}")
        resources: list[dict[str, object]] = []
        for entry in entries:
            if not isinstance(entry, dict):
                raise RuntimeError(f"invalid descriptor entry in {source_key}")
            if not isinstance(entry.get("name"), str):
                raise RuntimeError(f"descriptor in {source_key} has no name")
            resources.append(
                {
                    "name": entry["name"],
                    "binding": int(entry["binding"]),
                    "count": descriptor_count(entry),
                    "set": int(entry.get("set", 0)),
                }
            )
        resources.sort(key=lambda resource: (resource["set"], resource["binding"]))
        result[group_name] = resources
    return result


def cpp_quote(value: str) -> str:
    return json.dumps(value, ensure_ascii=True)


def blob_symbol(shader_name: str, digest: str) -> str:
    return f"{shader_name}_{digest}"


def write_blob_header(
    path: Path,
    symbol: str,
    binary: bytes,
    resources: dict[str, list[dict[str, object]]],
) -> None:
    lines = ["#pragma once", "#include <stdint.h>", ""]
    for group_name, _, sdk_group, _ in RESOURCE_GROUPS:
        entries = resources[group_name]
        if not entries:
            continue
        lines.append(f"static const char* g_{symbol}_{sdk_group}ResourceNames[] = {{")
        lines.extend(f"    {cpp_quote(str(entry['name']))}," for entry in entries)
        lines.append("};")
        for suffix, field in (
            ("Bindings", "binding"),
            ("Counts", "count"),
            ("Sets", "set"),
        ):
            lines.append(f"static const uint32_t g_{symbol}_{sdk_group}Resource{suffix}[] = {{")
            lines.extend(f"    {int(entry[field])}," for entry in entries)
            lines.append("};")
        lines.append("")

    lines.extend((f"static const uint32_t g_{symbol}_size = {len(binary)};", ""))
    lines.append(f"static const unsigned char g_{symbol}_data[] = {{")
    for offset in range(0, len(binary), 16):
        chunk = binary[offset : offset + 16]
        suffix = "," if offset + len(chunk) < len(binary) else ""
        lines.append("    " + ", ".join(f"0x{byte:02x}" for byte in chunk) + suffix)
    lines.extend(("};", ""))
    path.write_text("\n".join(lines), encoding="utf-8")


def write_permutation_header(
    source_root: Path,
    output_root: Path,
    output_name: str,
    pass_plan: dict[str, object],
    variant: dict[str, object],
    manifest_blobs: dict[str, dict[str, object]],
) -> None:
    shader_name = Path(str(pass_plan["shader"])).stem
    variant_name = str(variant["name"])
    if variant_name:
        shader_name += f"_{variant_name}"

    jobs = sorted(variant["jobs"], key=lambda job: int(job["compile_index"]))
    compile_to_digest: dict[int, str] = {}
    unique_digests: list[str] = []
    for job in jobs:
        compile_index = int(job["compile_index"])
        digest = str(job["blob_sha256"])
        compile_to_digest[compile_index] = digest
        if digest not in unique_digests:
            unique_digests.append(digest)

    digest_to_index = {digest: index for index, digest in enumerate(unique_digests)}
    digest_to_symbol: dict[str, str] = {}
    lines = ["#pragma once", "#include <stdint.h>", ""]
    for digest in unique_digests:
        blob = manifest_blobs.get(digest)
        if blob is None:
            raise RuntimeError(f"manifest references missing SPIR-V digest {digest}")
        symbol = blob_symbol(shader_name, Path(str(blob["file"])).stem)
        digest_to_symbol[digest] = symbol
        header_name = f"{symbol}.h"
        lines.append(f'#include "{header_name}"')
        binary_path = source_root / str(blob["file"])
        binary = binary_path.read_bytes()
        if hashlib.sha256(binary).hexdigest() != digest:
            raise RuntimeError(f"SPIR-V content hash mismatch: {binary_path}")
        if hashlib.md5(binary).hexdigest() != blob["md5"]:
            raise RuntimeError(f"SPIR-V MD5 does not match the SDK filename: {binary_path}")
        resources = resource_groups(blob["reflection"])
        write_blob_header(output_root / header_name, symbol, binary, resources)
    lines.extend(("", f"typedef union {shader_name}_PermutationKey {{", "    struct {"))
    for axis_field in pass_plan["axis_fields"]:
        lines.append(f"        uint32_t {axis_field} : 1;")
    lines.extend(("    };", "    uint32_t index;", f"}} {shader_name}_PermutationKey;", ""))

    lines.append(f"typedef struct {shader_name}_PermutationInfo {{")
    lines.extend(("    const uint32_t blobSize;", "    const unsigned char* blobData;"))
    for fields in RESOURCE_STRUCT_FIELDS:
        lines.append("")
        lines.append(f"    const uint32_t {fields[0]};")
        lines.append(f"    const char** {fields[1]};")
        lines.append(f"    const uint32_t* {fields[2]};")
        lines.append(f"    const uint32_t* {fields[3]};")
        lines.append(f"    const uint32_t* {fields[4]};")
    lines.extend((f"}} {shader_name}_PermutationInfo;", ""))

    lookup = sorted(pass_plan["runtime_lookup"], key=lambda entry: int(entry["key"]))
    if len(lookup) != 64 or [int(entry["key"]) for entry in lookup] != list(range(64)):
        raise RuntimeError(f"pass {pass_plan['pass']} does not have a complete 64-key lookup")
    lines.append(f"static const uint32_t g_{shader_name}_IndirectionTable[] = {{")
    for entry in lookup:
        compile_index = int(entry["blob_index"])
        digest = compile_to_digest.get(compile_index)
        if digest is None:
            raise RuntimeError(f"lookup references missing compile index {compile_index}")
        lines.append(f"    {digest_to_index[digest]},")
    lines.extend(("};", ""))

    lines.append(f"static const {shader_name}_PermutationInfo g_{shader_name}_PermutationInfo[] = {{")
    for digest in unique_digests:
        symbol = digest_to_symbol[digest]
        blob = manifest_blobs[digest]
        resources = resource_groups(blob["reflection"])
        fields: list[str] = [f"g_{symbol}_size", f"g_{symbol}_data"]
        for group_name, _, sdk_group, _ in RESOURCE_GROUPS:
            entries = resources[group_name]
            if not entries:
                fields.extend(("0", "0", "0", "0", "0"))
            else:
                fields.extend(
                    (
                        str(len(entries)),
                        f"g_{symbol}_{sdk_group}ResourceNames",
                        f"g_{symbol}_{sdk_group}ResourceBindings",
                        f"g_{symbol}_{sdk_group}ResourceCounts",
                        f"g_{symbol}_{sdk_group}ResourceSets",
                    )
                )
        lines.append("    { " + ", ".join(fields) + " },")
    lines.extend(("};", ""))
    (output_root / f"{output_name}_permutations.h").write_text(
        "\n".join(lines), encoding="utf-8"
    )


def emit_headers(manifest_path: Path, output_root: Path) -> tuple[int, int]:
    manifest_path = manifest_path.resolve()
    source_root = manifest_path.parent
    output_root = output_root.resolve()
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest.get("sdk_commit") != "c6efa6bf7f2027b3ec94f28578bb5965eabb9e55":
        raise RuntimeError("manifest does not identify the pinned FidelityFX SDK commit")
    output_root.mkdir(parents=True, exist_ok=True)
    manifest_blobs = {str(blob["sha256"]): blob for blob in manifest["blobs"]}
    header_count = 0
    for pass_plan in manifest["passes"]:
        for variant in pass_plan["variants"]:
            output_name = Path(str(pass_plan["shader"])).stem
            if variant["name"]:
                output_name += f"_{variant['name']}"
            write_permutation_header(
                source_root,
                output_root,
                output_name,
                pass_plan,
                variant,
                manifest_blobs,
            )
            header_count += 1
    return header_count, len(manifest_blobs)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--output-root", required=True, type=Path)
    args = parser.parse_args()
    try:
        header_count, blob_count = emit_headers(args.manifest, args.output_root)
    except (OSError, ValueError, RuntimeError, KeyError, TypeError) as error:
        print(error, file=sys.stderr)
        return 2
    print(f"emitted {header_count} permutation headers from {blob_count} deduplicated blobs")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
