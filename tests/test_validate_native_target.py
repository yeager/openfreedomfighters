import pathlib
import shutil
import subprocess
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
CASE = ROOT / "tests" / "validate_native_target_case.cmake"
CMAKE = shutil.which("cmake")


def validate(system, processor, pointer_size=8, osx_architectures=""):
    if CMAKE is None:
        raise unittest.SkipTest("CMake is not available")
    command = [
        CMAKE,
        f"-DTARGET_SYSTEM={system}",
        f"-DTARGET_PROCESSOR={processor}",
        f"-DTARGET_POINTER_SIZE={pointer_size}",
    ]
    if osx_architectures:
        command.append(f"-DTARGET_OSX_ARCHITECTURES={osx_architectures}")
    command.extend(["-P", str(CASE)])
    return subprocess.run(command, text=True, capture_output=True, check=False)


class ValidateNativeTargetTests(unittest.TestCase):
    def test_accepts_declared_native_matrix(self):
        for system, processor, macos_architectures in (
            ("Windows", "x64", ""),
            ("Linux", "x86_64", ""),
            ("Darwin", "arm64", ""),
            ("Darwin", "x86_64", ""),
            ("Darwin", "arm64", "x86_64;arm64"),
        ):
            with self.subTest(system=system, processor=processor,
                              macos_architectures=macos_architectures):
                result = validate(system, processor,
                                  osx_architectures=macos_architectures)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_rejects_32_bit_or_unsupported_target_architectures(self):
        for system, processor, pointer_size, message in (
            ("Linux", "x86", 4, "requires a 64-bit target"),
            ("Linux", "aarch64", 8, "does not support aarch64"),
            ("Windows", "ARM64", 8, "does not support ARM64"),
            ("Darwin", "ppc64", 8, "does not support ppc64"),
            ("FreeBSD", "x86_64", 8, "supports native Windows"),
        ):
            with self.subTest(system=system, processor=processor):
                result = validate(system, processor, pointer_size)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn(message, result.stderr)


if __name__ == "__main__":
    unittest.main()
