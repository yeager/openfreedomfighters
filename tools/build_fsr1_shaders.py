#!/usr/bin/env python3
"""Compile project FSR 1 fragment wrappers against a pinned external GPUOpen checkout.

All generated artifacts are written to an explicitly supplied directory outside
the repository. The upstream source tree and binaries are never copied here.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile


PINNED_REVISION = "a21ffb8f6c13233ba336352bdff293894c706575"
SHADERS = {
    "easu": {"source": "fsr1_easu.frag", "stage": "frag", "uniform_size": 64},
    "rcas": {"source": "fsr1_rcas.frag", "stage": "frag", "uniform_size": 16},
    "fullscreen_vert": {"source": "fsr1_fullscreen.vert", "stage": "vert"},
}


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def run(command, env):
    subprocess.run([str(part) for part in command], check=True, env=env)


def validate_reflection(path, spec):
    reflection = json.loads(path.read_text(encoding="utf-8"))
    stage = spec["stage"]
    if reflection.get("entryPoints") != [{"name": "main", "mode": stage}]:
        raise RuntimeError(f"Unexpected {stage} entry point in {path.name}")
    inputs = sorted((item["location"], item["type"])
                    for item in reflection.get("inputs", []))
    outputs = sorted((item["location"], item["type"])
                     for item in reflection.get("outputs", []))
    textures = sorted((item["set"], item["binding"], item["type"])
                      for item in reflection.get("textures", []))
    ubos = reflection.get("ubos", [])
    if stage == "vert":
        if inputs or outputs or reflection.get("textures") or reflection.get("ubos"):
            raise RuntimeError(f"Unexpected full-screen vertex interface in {path.name}")
        return
    if inputs or outputs != [(0, "vec4")]:
        raise RuntimeError(f"Unexpected stage interface in {path.name}: {inputs}, {outputs}")
    if textures != [(2, 0, "sampler2D")]:
        raise RuntimeError(f"Unexpected sampled texture interface in {path.name}: {textures}")
    if len(ubos) != 1 or (ubos[0]["set"], ubos[0]["binding"],
                          ubos[0]["block_size"]) != (3, 0, spec["uniform_size"]):
        raise RuntimeError(f"Unexpected constants interface in {path.name}")


def validate_dxil_container(blob):
    if len(blob) < 32 or blob[:4] != b"DXBC":
        raise RuntimeError("DXC did not produce a shader container")
    size, count = int.from_bytes(blob[24:28], "little"), int.from_bytes(blob[28:32], "little")
    if size != len(blob) or count > (len(blob) - 32) // 4:
        raise RuntimeError("Invalid DXIL container bounds")
    found_dxil = False
    for index in range(count):
        offset = int.from_bytes(blob[32 + index * 4:36 + index * 4], "little")
        if offset < 32 + count * 4 or offset > len(blob) - 8:
            raise RuntimeError("Invalid DXIL chunk offset")
        chunk_size = int.from_bytes(blob[offset + 4:offset + 8], "little")
        if chunk_size > len(blob) - offset - 8:
            raise RuntimeError("Invalid DXIL chunk size")
        found_dxil |= blob[offset:offset + 4] == b"DXIL"
    if not found_dxil:
        raise RuntimeError("DXIL container has no DXIL program")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", type=Path, required=True,
                        help="External git checkout of GPUOpen-Effects/FidelityFX-FSR")
    parser.add_argument("--out-dir", type=Path, required=True,
                        help="Existing output directory outside the repository")
    parser.add_argument("--glslang", required=True)
    parser.add_argument("--spirv-cross", required=True)
    parser.add_argument("--spirv-val", required=True)
    parser.add_argument("--dxc", help="Optional DXC executable to validate generated HLSL")
    parser.add_argument("--dxil-dir", type=Path,
                        help="Directory containing DXIL compiled by the pinned DXC toolchain")
    parser.add_argument("--metal", help="Optional Apple metal compiler to validate generated MSL")
    args = parser.parse_args()

    root = Path(__file__).resolve().parents[1]
    source_dir = args.source_dir.resolve()
    out_dir = args.out_dir.resolve()
    upstream_shader_dir = source_dir / "ffx-fsr"
    for directory, label in ((source_dir, "--source-dir"), (out_dir, "--out-dir")):
        if not directory.is_dir():
            parser.error(f"{label} must name an existing directory")
    if root == out_dir or root in out_dir.parents:
        parser.error("--out-dir must be outside the repository")
    if not (upstream_shader_dir / "ffx_a.h").is_file() or not (upstream_shader_dir / "ffx_fsr1.h").is_file():
        parser.error("--source-dir is missing the official ffx-fsr headers")
    revision = subprocess.check_output(
        ["git", "-C", str(source_dir), "rev-parse", "HEAD"], text=True).strip()
    if revision != PINNED_REVISION:
        parser.error(f"expected GPUOpen revision {PINNED_REVISION}, found {revision}")

    env = os.environ.copy()
    env.update({key: str(out_dir) for key in ("TMPDIR", "TMP", "TEMP")})
    header_hashes = {
        name: sha256((upstream_shader_dir / name).read_bytes())
        for name in ("ffx_a.h", "ffx_fsr1.h")
    }
    manifest = {
        "upstream": "GPUOpen-Effects/FidelityFX-FSR",
        "revision": revision,
        "upstream_header_sha256": header_hashes,
        "passes": {},
        "validation": {"spirv_val": True, "dxc": False, "metal": False},
    }

    with tempfile.TemporaryDirectory(prefix="off-fsr1-", dir=out_dir) as scratch_name:
        scratch = Path(scratch_name)
        for name, spec in SHADERS.items():
            source = root / "assets/shaders" / spec["source"]
            stage = spec["stage"]
            spv = scratch / f"{name}.{stage}.spv"
            reflection = scratch / f"{name}.reflection.json"
            msl = scratch / f"{name}.{stage}.metal"
            hlsl = scratch / f"{name}.{stage}.hlsl"
            run([args.glslang, "-V", "--target-env", "vulkan1.0", "-S", stage,
                 f"-I{upstream_shader_dir}", "-o", spv, source], env)
            run([args.spirv_val, "--target-env", "vulkan1.0", spv], env)
            reflection.write_text(subprocess.check_output(
                [args.spirv_cross, str(spv), "--reflect"], text=True, env=env),
                encoding="utf-8")
            validate_reflection(reflection, spec)
            run([args.spirv_cross, spv, "--msl", "--msl-version", "20100",
                 "--output", msl], env)
            run([args.spirv_cross, spv, "--hlsl", "--shader-model", "60",
                 "--output", hlsl], env)
            metal_source = msl.read_text(encoding="utf-8")
            expected_msl_bindings = (("[[buffer(0)]]", "[[texture(0)]]", "[[sampler(0)]]")
                                     if stage == "frag" else ())
            for token in expected_msl_bindings:
                if token not in metal_source:
                    raise RuntimeError(f"{name}: generated MSL is missing {token}")
            if ")OFF_FSR1_MSL\"" in metal_source:
                raise RuntimeError(f"{name}: MSL collides with generated C++ literal delimiter")

            prefix = ("// Generated from project wrapper and pinned public GPUOpen FSR 1 source.\n"
                      f"// GPUOpen revision: {revision}\n"
                      f"// Wrapper SHA256: {sha256(source.read_bytes())}\n"
                      f"// ffx_a.h SHA256: {header_hashes['ffx_a.h']}\n"
                      f"// ffx_fsr1.h SHA256: {header_hashes['ffx_fsr1.h']}\n"
                      "// AMD copyright and MIT license: assets/shaders/FSR1_UPSTREAM_NOTICE.txt\n")
            outputs = {
                f"fsr1_{name}.spv": spv.read_bytes(),
                f"fsr1_{name}.msl": (prefix + metal_source).encode("utf-8"),
                f"fsr1_{name}.hlsl": (prefix + hlsl.read_text(encoding="utf-8")).encode("utf-8"),
                f"fsr1_{name}.reflection.json": reflection.read_bytes(),
            }
            dxil = scratch / f"{name}.dxil"
            if args.dxc:
                profile = "vs_6_0" if stage == "vert" else "ps_6_0"
                run([args.dxc, "-T", profile, "-E", "main", "-HV", "2021",
                     "-O3", "-Ges", "-WX", "-Fo", dxil, hlsl], env)
            elif args.dxil_dir:
                dxil_source = args.dxil_dir / f"fsr1_{name}.dxil"
                if not dxil_source.is_file():
                    raise RuntimeError(f"Missing precompiled DXIL: {dxil_source.name}")
                dxil.write_bytes(dxil_source.read_bytes())
            if args.dxc or args.dxil_dir:
                dxil_bytes = dxil.read_bytes()
                validate_dxil_container(dxil_bytes)
                outputs[f"fsr1_{name}.dxil"] = dxil_bytes
            for filename, content in outputs.items():
                (out_dir / filename).write_bytes(content)
            manifest["passes"][name] = {
                "wrapper": spec["source"],
                "wrapper_sha256": sha256(source.read_bytes()),
                "spirv_bytes": len(outputs[f"fsr1_{name}.spv"]),
                "msl_bytes": len(outputs[f"fsr1_{name}.msl"]),
                "hlsl_bytes": len(outputs[f"fsr1_{name}.hlsl"]),
                "dxil_bytes": len(outputs[f"fsr1_{name}.dxil"])
                    if f"fsr1_{name}.dxil" in outputs else 0,
                "outputs_sha256": {
                    filename: sha256(content)
                    for filename, content in outputs.items()
                },
                "reflection": json.loads(outputs[f"fsr1_{name}.reflection.json"]),
            }

            if args.metal:
                air = scratch / f"{name}.air"
                run([args.metal, "-std=macos-metal2.1", "-c", msl, "-o", air], env)

        manifest["validation"]["metal"] = bool(args.metal)
        manifest["validation"]["dxc"] = bool(args.dxc or args.dxil_dir)
        (out_dir / "fsr1_shader_manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(f"Generated FSR 1 vertex/EASU/RCAS SPIR-V, MSL, HLSL, reflection, and manifest in {out_dir}")
    print(f"SPIR-V validation: passed; Metal compiler validation: {bool(args.metal)}; DXC validation: {bool(args.dxc or args.dxil_dir)}")


if __name__ == "__main__":
    main()
