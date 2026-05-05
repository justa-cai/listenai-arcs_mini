import importlib.util
import lzma
import os
import sys
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest import mock


MODULE_PATH = Path(__file__).with_name("package_ota.py")
SPEC = importlib.util.spec_from_file_location("package_ota", MODULE_PATH)
package_ota = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(package_ota)


class PackageOtaTest(unittest.TestCase):
    def test_create_tar_reads_multiapp_config_directly(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            config_dir = tmp_path / "sample"
            build_dir = config_dir / "build"
            output_tar = tmp_path / "ota.tar"
            config_path = config_dir / "my_config.json"
            ap_bin = build_dir / "upgrade_fw" / "app.bin"
            cp_bin = build_dir / "cp_payload.bin"

            ap_bin.parent.mkdir(parents=True)
            cp_bin.parent.mkdir(parents=True, exist_ok=True)
            ap_bin.write_bytes(b"ap-config")
            cp_bin.write_bytes(b"cp-config")
            config_path.parent.mkdir(parents=True, exist_ok=True)
            config_path.write_text(
                """
{
  "version": "1.0",
  "apps": [
    {
      "app_name": "flash_real_flow",
      "images": [
        {
          "bin_path": "build/upgrade_fw/app.bin",
          "flash_addr": "0x30026000"
        },
        {
          "bin_path": "build/cp_payload.bin",
          "flash_addr": "0x30226000"
        }
      ]
    }
  ]
}
                """.strip(),
                encoding="utf-8",
            )

            package_ota.create_tar_from_config(config_path, output_tar)

            with tarfile.open(output_tar, "r") as tar:
                self.assertEqual(
                    ["config.json", "image/app.bin", "image/cp_payload.bin"],
                    tar.getnames(),
                )
                config_member = tar.extractfile("config.json")
                assert config_member is not None
                config_payload = config_member.read()

            self.assertIn(b'"app_name": "flash_real_flow"', config_payload)
            self.assertIn(b'"file": "app.bin"', config_payload)
            self.assertIn(b'"addr": "0x30026000"', config_payload)
            self.assertIn(b'"size": "9"', config_payload)

    def test_create_tar_packs_ap_bin_without_manifest(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            input_bin = tmp_path / "upgrade.bin"
            output_tar = tmp_path / "ota.tar"

            input_bin.write_bytes(b"payload")

            package_ota.create_tar(input_bin, output_tar)

            with tarfile.open(output_tar, "r") as tar:
                names = tar.getnames()

            self.assertEqual(["AP.bin"], names)

    def test_create_tar_packs_multiple_partition_images(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            ap_bin = tmp_path / "ap.bin"
            cp_bin = tmp_path / "cp.bin"
            output_tar = tmp_path / "ota.tar"

            ap_bin.write_bytes(b"ap-payload")
            cp_bin.write_bytes(b"cp-payload")

            package_ota.create_tar(
                [("AP", ap_bin), ("CP", cp_bin)],
                output_tar,
            )

            with tarfile.open(output_tar, "r") as tar:
                names = tar.getnames()
                ap_member = tar.extractfile("AP.bin")
                cp_member = tar.extractfile("CP.bin")

                assert ap_member is not None
                assert cp_member is not None

                ap_payload = ap_member.read()
                cp_payload = cp_member.read()

            self.assertEqual(["AP.bin", "CP.bin"], names)
            self.assertEqual(b"ap-payload", ap_payload)
            self.assertEqual(b"cp-payload", cp_payload)

    def test_create_tar_packs_packager_output_layout(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            app_dir = tmp_path / "uboot_ota_flash_real_flow"
            image_dir = app_dir / "image"
            output_tar = tmp_path / "ota.tar"

            image_dir.mkdir(parents=True)
            (app_dir / "config.json").write_text(
                '{"image":[{"file":"AP.bin"},{"file":"CP.bin"}]}',
                encoding="utf-8",
            )
            (image_dir / "AP.bin").write_bytes(b"ap-packager")
            (image_dir / "CP.bin").write_bytes(b"cp-packager")

            package_ota.create_tar_from_packager_dir(app_dir, output_tar)

            with tarfile.open(output_tar, "r") as tar:
                names = tar.getnames()
                config_member = tar.extractfile("config.json")
                ap_member = tar.extractfile("image/AP.bin")
                cp_member = tar.extractfile("image/CP.bin")

                assert config_member is not None
                assert ap_member is not None
                assert cp_member is not None

                config_payload = config_member.read()
                ap_payload = ap_member.read()
                cp_payload = cp_member.read()

            self.assertEqual(["config.json", "image/AP.bin", "image/CP.bin"], names)
            self.assertEqual(b'{"image":[{"file":"AP.bin"},{"file":"CP.bin"}]}', config_payload)
            self.assertEqual(b"ap-packager", ap_payload)
            self.assertEqual(b"cp-packager", cp_payload)

    def test_main_supports_multiple_image_arguments(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            ap_bin = tmp_path / "ap.bin"
            cp_bin = tmp_path / "cp.bin"
            output_txz = tmp_path / "ota.txz"

            ap_bin.write_bytes(b"ap-main")
            cp_bin.write_bytes(b"cp-main")

            argv = [
                "package_ota.py",
                "--image",
                f"AP={ap_bin}",
                "--image",
                f"CP={cp_bin}",
                "--txz",
                str(output_txz),
            ]

            with mock.patch.object(sys, "argv", argv):
                package_ota.main()

            with lzma.open(output_txz, "rb") as src:
                tar_bytes = src.read()

            extract_tar = tmp_path / "extract.tar"
            extract_tar.write_bytes(tar_bytes)

            with tarfile.open(extract_tar, "r") as tar:
                self.assertEqual(["AP.bin", "CP.bin"], tar.getnames())

    def test_main_supports_packager_dir_argument(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            app_dir = tmp_path / "uboot_ota_flash_real_flow"
            image_dir = app_dir / "image"
            output_txz = tmp_path / "ota.txz"

            image_dir.mkdir(parents=True)
            (app_dir / "config.json").write_text(
                '{"image":[{"file":"AP.bin"},{"file":"CP.bin"}]}',
                encoding="utf-8",
            )
            (image_dir / "AP.bin").write_bytes(b"ap-main")
            (image_dir / "CP.bin").write_bytes(b"cp-main")

            argv = [
                "package_ota.py",
                "--packager-dir",
                str(app_dir),
                "--txz",
                str(output_txz),
            ]

            with mock.patch.object(sys, "argv", argv):
                package_ota.main()

            with lzma.open(output_txz, "rb") as src:
                tar_bytes = src.read()

            extract_tar = tmp_path / "extract.tar"
            extract_tar.write_bytes(tar_bytes)

            with tarfile.open(extract_tar, "r") as tar:
                self.assertEqual(
                    ["config.json", "image/AP.bin", "image/CP.bin"],
                    tar.getnames(),
                )

    def test_main_supports_config_argument(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            config_dir = tmp_path / "sample"
            build_dir = config_dir / "build"
            output_txz = tmp_path / "ota.txz"
            config_path = config_dir / "my_config.json"
            ap_bin = build_dir / "upgrade_fw" / "app.bin"
            cp_bin = build_dir / "cp_payload.bin"

            ap_bin.parent.mkdir(parents=True)
            cp_bin.parent.mkdir(parents=True, exist_ok=True)
            ap_bin.write_bytes(b"ap-main")
            cp_bin.write_bytes(b"cp-main")
            config_path.parent.mkdir(parents=True, exist_ok=True)
            config_path.write_text(
                """
{
  "apps": [
    {
      "app_name": "flash_real_flow",
      "images": [
        {
          "bin_path": "build/upgrade_fw/app.bin",
          "flash_addr": "0x30026000"
        },
        {
          "bin_path": "build/cp_payload.bin",
          "flash_addr": "0x30226000"
        }
      ]
    }
  ]
}
                """.strip(),
                encoding="utf-8",
            )

            argv = [
                "package_ota.py",
                "--config",
                str(config_path),
                "--txz",
                str(output_txz),
            ]

            with mock.patch.object(sys, "argv", argv):
                package_ota.main()

            with lzma.open(output_txz, "rb") as src:
                tar_bytes = src.read()

            extract_tar = tmp_path / "extract.tar"
            extract_tar.write_bytes(tar_bytes)

            with tarfile.open(extract_tar, "r") as tar:
                self.assertEqual(
                    ["config.json", "image/app.bin", "image/cp_payload.bin"],
                    tar.getnames(),
                )

    def test_main_defaults_to_ota_txz_in_current_directory(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            input_bin = tmp_path / "upgrade.bin"
            output_txz = tmp_path / "ota.txz"

            input_bin.write_bytes(b"default-main")

            argv = [
                "package_ota.py",
                "--input",
                str(input_bin),
            ]

            old_cwd = Path.cwd()
            os.chdir(tmp_path)
            try:
                with mock.patch.object(sys, "argv", argv):
                    package_ota.main()
            finally:
                os.chdir(old_cwd)

            self.assertTrue(output_txz.exists())
            with lzma.open(output_txz, "rb") as src:
                tar_bytes = src.read()

            extract_tar = tmp_path / "extract-default.tar"
            extract_tar.write_bytes(tar_bytes)
            with tarfile.open(extract_tar, "r") as tar:
                self.assertEqual(["AP.bin"], tar.getnames())

    def test_main_supports_tar_only_output(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            input_bin = tmp_path / "upgrade.bin"
            output_tar = tmp_path / "ota.tar"
            output_txz = tmp_path / "ota.txz"

            input_bin.write_bytes(b"tar-only")

            argv = [
                "package_ota.py",
                "--input",
                str(input_bin),
                "--tar",
                str(output_tar),
            ]

            with mock.patch.object(sys, "argv", argv):
                package_ota.main()

            self.assertTrue(output_tar.exists())
            self.assertFalse(output_txz.exists())

            with tarfile.open(output_tar, "r") as tar:
                self.assertEqual(["AP.bin"], tar.getnames())

    def test_main_rejects_tar_and_txz_together(self):
        with tempfile.TemporaryDirectory() as tmpdir:
            tmp_path = Path(tmpdir)
            input_bin = tmp_path / "upgrade.bin"

            input_bin.write_bytes(b"invalid-both")

            argv = [
                "package_ota.py",
                "--input",
                str(input_bin),
                "--tar",
                str(tmp_path / "ota.tar"),
                "--txz",
                str(tmp_path / "ota.txz"),
            ]

            with mock.patch.object(sys, "argv", argv):
                with self.assertRaises(SystemExit) as ctx:
                    package_ota.main()

            self.assertEqual(2, ctx.exception.code)


if __name__ == "__main__":
    unittest.main()
