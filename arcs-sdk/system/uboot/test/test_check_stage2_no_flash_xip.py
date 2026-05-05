#!/usr/bin/env python3

import importlib.util
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parent
MODULE_PATH = ROOT / "check_stage2_no_flash_xip.py"


def load_checker_module():
    spec = importlib.util.spec_from_file_location("check_stage2_no_flash_xip", MODULE_PATH)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


class CheckStage2NoFlashXipTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.module = load_checker_module()
        cls.sections = {
            ".init": cls.module.Range(name=".init", start=0x30000000, end=0x30000100),
        }
        cls.stage2_regions = [
            cls.module.Range(name=".psram.text", start=0x2005F000, end=0x20070000),
        ]

    def write_lst(self, content: str) -> Path:
        handle = tempfile.NamedTemporaryFile("w", delete=False)
        handle.write(content)
        handle.flush()
        handle.close()
        path = Path(handle.name)
        self.addCleanup(path.unlink, missing_ok=True)
        return path

    def test_ignores_non_control_transfer_with_flash_symbol_comment(self):
        lst_path = self.write_lst(
            "2005f70c:\t04a54783          \tlbu\ta5,74(a0) # 3000004a <vector_base+0x4a>\n"
        )

        failures = self.module.check_stage2_calls(lst_path, self.sections, self.stage2_regions)

        self.assertEqual([], failures)

    def test_reports_flash_jalr_from_stage2_runtime(self):
        lst_path = self.write_lst(
            "20060118:\t3b0080e7          \tjalr\t944(ra) # 3000004a <vector_base+0x4a>\n"
        )

        failures = self.module.check_stage2_calls(lst_path, self.sections, self.stage2_regions)

        self.assertEqual(1, len(failures))
        self.assertIn("caller=0x20060118", failures[0])

    def test_reports_direct_jal_to_flash_symbol(self):
        lst_path = self.write_lst(
            "20060118:\t04a000ef          \tjal\t3000004a <vector_base+0x4a>\n"
        )

        failures = self.module.check_stage2_calls(lst_path, self.sections, self.stage2_regions)

        self.assertEqual(1, len(failures))
        self.assertIn("target=0x3000004a", failures[0])

    def test_reports_early_state_symbol_in_psram(self):
        symbols = {
            "HARTID": self.module.Symbol(name="HARTID", addr=0x28002080, size=4, section=".psram.data"),
            "_impure_ptr": self.module.Symbol(
                name="_impure_ptr",
                addr=0x28002200,
                size=4,
                section=".psram.data",
            ),
            "_impure_data": self.module.Symbol(
                name="_impure_data",
                addr=0x28002208,
                size=0x120,
                section=".psram.data",
            ),
            "SystemCoreClock": self.module.Symbol(
                name="SystemCoreClock",
                addr=0x20062084,
                size=4,
                section=".fast.data",
            ),
            "SystemIRegionInfo": self.module.Symbol(
                name="SystemIRegionInfo",
                addr=0x2006269C,
                size=0x14,
                section=".fast.bss",
            ),
            "SystemExceptionHandlers": self.module.Symbol(
                name="SystemExceptionHandlers",
                addr=0x200626B0,
                size=0x44,
                section=".fast.bss",
            ),
        }

        failures = self.module.check_required_sram_state_symbols(symbols)

        self.assertEqual(3, len(failures))
        self.assertIn("HARTID", failures[0])
        self.assertTrue(any("_impure_ptr" in failure for failure in failures))
        self.assertTrue(any("_impure_data" in failure for failure in failures))

    def test_accepts_early_state_symbol_in_sram(self):
        symbols = {
            "HARTID": self.module.Symbol(name="HARTID", addr=0x20062080, size=4, section=".fast.data"),
            "_impure_ptr": self.module.Symbol(
                name="_impure_ptr",
                addr=0x20062088,
                size=4,
                section=".boot_f.data",
            ),
            "_impure_data": self.module.Symbol(
                name="_impure_data",
                addr=0x20062090,
                size=0x120,
                section=".boot_f.data",
            ),
            "SystemCoreClock": self.module.Symbol(
                name="SystemCoreClock",
                addr=0x20062084,
                size=4,
                section=".fast.data",
            ),
            "SystemIRegionInfo": self.module.Symbol(
                name="SystemIRegionInfo",
                addr=0x2006269C,
                size=0x14,
                section=".fast.bss",
            ),
            "SystemExceptionHandlers": self.module.Symbol(
                name="SystemExceptionHandlers",
                addr=0x200626B0,
                size=0x44,
                section=".fast.bss",
            ),
        }

        failures = self.module.check_required_sram_state_symbols(symbols)

        self.assertEqual([], failures)

    def test_reports_early_exec_symbol_in_psram(self):
        symbols = {
            "rand": self.module.Symbol(name="rand", addr=0x28012AA8, size=0x4C, section=".psram.text"),
        }

        failures = self.module.check_required_early_exec_symbols(symbols)

        self.assertEqual(1, len(failures))
        self.assertTrue(any("rand" in failure for failure in failures))

    def test_accepts_early_exec_symbol_in_boot_ramcode(self):
        symbols = {
            "rand": self.module.Symbol(name="rand", addr=0x20053F00, size=0x4C, section=".boot_f.ramcode"),
        }

        failures = self.module.check_required_early_exec_symbols(symbols)

        self.assertEqual([], failures)

    def test_reports_hidden_save_restore_from_main(self):
        lst_path = self.write_lst(
            "2005c22e:	4c6302e7          	jalr	t0,1222(t1) # 283136f0 <__riscv_save_0>\n"
        )
        symbols = {
            "main": self.module.Symbol(name="main", addr=0x2005C22A, size=0xB4, section=".fast.text"),
            "__riscv_save_0": self.module.Symbol(
                name="__riscv_save_0",
                addr=0x283136F0,
                size=0x0C,
                section=".psram.text",
            ),
        }

        failures = self.module.check_early_stage_save_restore(lst_path, symbols)

        self.assertEqual(1, len(failures))
        self.assertIn("caller=0x2005c22e", failures[0])
        self.assertIn("target_symbol=__riscv_save_0", failures[0])


if __name__ == "__main__":
    unittest.main()
