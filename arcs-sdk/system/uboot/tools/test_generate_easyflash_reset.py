import importlib.util
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("generate_easyflash_reset.py")
SPEC = importlib.util.spec_from_file_location("generate_easyflash_reset", MODULE_PATH)
generate_easyflash_reset = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(generate_easyflash_reset)


class GenerateEasyflashResetTest(unittest.TestCase):
    def test_write_reset_image_creates_zero_filled_file(self):
        with tempfile.TemporaryDirectory() as tmp_dir:
            output = Path(tmp_dir) / "easyflash_reset.bin"

            generate_easyflash_reset.write_reset_image(output, 0x2000)

            self.assertEqual(0x2000, output.stat().st_size)
            self.assertEqual({0}, set(output.read_bytes()))


if __name__ == "__main__":
    unittest.main()
