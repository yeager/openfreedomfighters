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


class VerifyReleasePackageTests(unittest.TestCase):
    def test_accepts_complete_linux_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = pathlib.Path(directory) / "linux.tar.gz"
            files = COMMON | {
                "bin/openfreedomfighters": b"native executable",
                "share/applications/openfreedomfighters.desktop": b"[Desktop Entry]\nExec=openfreedomfighters\n",
            }
            write_tar(archive, files, {"bin/openfreedomfighters"})
            MODULE.verify(archive, "linux")

    def test_rejects_non_executable_unix_binary(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = pathlib.Path(directory) / "macos.tar.gz"
            write_tar(archive, COMMON | {"bin/openfreedomfighters": b"native executable"})
            with self.assertRaisesRegex(ValueError, "not marked executable"):
                MODULE.verify(archive, "macos")

    def test_rejects_retail_data_extension(self):
        with tempfile.TemporaryDirectory() as directory:
            archive = pathlib.Path(directory) / "linux.tar.gz"
            files = COMMON | {
                "bin/openfreedomfighters": b"native executable",
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
                package.writestr("bin/openfreedomfighters.exe", b"native executable")
            MODULE.verify(archive, "windows")


if __name__ == "__main__":
    unittest.main()
