import importlib.util
import io
import pathlib
import tarfile
import tempfile
import unittest
import zipfile


MODULE_PATH = pathlib.Path(__file__).parents[1] / "tools" / "verify_release_package.py"
SPEC = importlib.util.spec_from_file_location("verify_release_package", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


COMMON = {
    "README.md": b"readme",
    "LICENSE": b"license",
    "THIRD_PARTY.md": b"third party",
    "bin/assets/openfreedomfighters-splash.bmp": b"bmp",
    "bin/assets/Rajdhani-SemiBold.ttf": b"font",
}


def write_tar(path: pathlib.Path, files: dict[str, bytes], executable: set[str] = set()) -> None:
    with tarfile.open(path, "w:gz") as package:
        for name, content in files.items():
            member = tarfile.TarInfo(name)
            member.size = len(content)
            member.mode = 0o755 if name in executable else 0o644
            package.addfile(member, io.BytesIO(content))


def elf64_x86_64() -> bytes:
    header = bytearray(64)
    header[:7] = b"\x7fELF\x02\x01\x01"
    header[18:20] = (0x003E).to_bytes(2, "little")
    return bytes(header)


def macho64(architecture: str) -> bytes:
    cputype = {"x86_64": 0x01000007, "arm64": 0x0100000C}[architecture]
    return b"\xcf\xfa\xed\xfe" + cputype.to_bytes(4, "little") + bytes(24)


def pe_x86_64() -> bytes:
    header = bytearray(256)
    header[:2] = b"MZ"
    header[0x3C:0x40] = (0x80).to_bytes(4, "little")
    header[0x80:0x84] = b"PE\0\0"
    header[0x84:0x86] = (0x8664).to_bytes(2, "little")
    return bytes(header)


class VerifyReleasePackageTests(unittest.TestCase):
    def test_accepts_complete_linux_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = pathlib.Path(directory) / "linux.tar.gz"
            files = COMMON | {
                "bin/openfreedomfighters": elf64_x86_64(),
                "share/applications/openfreedomfighters.desktop": b"[Desktop Entry]\nExec=openfreedomfighters\n",
            }
            write_tar(archive, files, {"bin/openfreedomfighters"})
            MODULE.verify(archive, "linux")

    def test_rejects_non_executable_unix_binary(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = pathlib.Path(directory) / "macos.tar.gz"
            write_tar(archive, COMMON | {"bin/openfreedomfighters": macho64("x86_64")})
            with self.assertRaisesRegex(ValueError, "not marked executable"):
                MODULE.verify(archive, "macos")

    def test_rejects_retail_data_extension(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = pathlib.Path(directory) / "linux.tar.gz"
            files = COMMON | {
                "bin/openfreedomfighters": elf64_x86_64(),
                "share/applications/openfreedomfighters.desktop": b"Exec=openfreedomfighters\n",
                "assets/owned-data.zgf": b"retail data",
            }
            write_tar(archive, files, {"bin/openfreedomfighters"})
            with self.assertRaisesRegex(ValueError, "prohibited retail-data"):
                MODULE.verify(archive, "linux")

    def test_accepts_complete_windows_zip(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = pathlib.Path(directory) / "windows.zip"
            with zipfile.ZipFile(archive, "w") as package:
                for name, content in COMMON.items():
                    package.writestr(name, content)
                package.writestr("bin/openfreedomfighters.exe", pe_x86_64())
            MODULE.verify(archive, "windows")

    def test_rejects_macos_archive_named_arm64_with_x86_64_binary(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = pathlib.Path(directory) / "macos-arm64.tar.gz"
            write_tar(archive, COMMON | {"bin/openfreedomfighters": macho64("x86_64")},
                      {"bin/openfreedomfighters"})
            with self.assertRaisesRegex(ValueError, "architecture mismatch"):
                MODULE.verify(archive, "macos", "arm64")

    def test_rejects_non_native_linux_container(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = pathlib.Path(directory) / "linux.tar.gz"
            files = COMMON | {
                "bin/openfreedomfighters": b"not an ELF executable",
                "share/applications/openfreedomfighters.desktop": b"Exec=openfreedomfighters\n",
            }
            write_tar(archive, files, {"bin/openfreedomfighters"})
            with self.assertRaisesRegex(ValueError, "not a 64-bit ELF"):
                MODULE.verify(archive, "linux")


if __name__ == "__main__":
    unittest.main()
