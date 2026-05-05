import binascii
import importlib.util
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("generate_boot_partab.py")
SPEC = importlib.util.spec_from_file_location("generate_boot_partab", MODULE_PATH)
generate_boot_partab = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(generate_boot_partab)


class GenerateBootPartabTest(unittest.TestCase):
    def test_write_partab_image_creates_valid_ota_partab(self):
        with tempfile.TemporaryDirectory() as tmp_dir:
            output = Path(tmp_dir) / "partab.bin"

            generate_boot_partab.write_partab_image(
                output,
                flash_base=0x30000000,
                boot_flash_size=0x1C000,
                partab_size=0x1000,
                control_store_base=0x30700000,
                control_store_size=0x2000,
                app_base=0x3001C000,
                app_size=0x200000,
                ota_base=0x30600000,
                ota_size=0x100000,
            )

            data = output.read_bytes()
            self.assertEqual(0x1000, len(data))

            header = generate_boot_partab.HEADER_STRUCT.unpack_from(data, 0)
            self.assertEqual(generate_boot_partab.PARTAB_MAGIC, header[0])
            self.assertEqual(generate_boot_partab.PARTAB_VERSION, header[1])
            self.assertEqual(generate_boot_partab.BOOT_CONFIG_SIZE, header[2])
            self.assertEqual(0x30700000, header[4])
            self.assertEqual(0x2000, header[5])
            self.assertEqual(generate_boot_partab.BOOT_SCHEME_OTA, header[6])
            self.assertEqual(2, header[10])

            crc_input = data[:12] + b"\x00\x00\x00\x00" + data[16:generate_boot_partab.BOOT_CONFIG_SIZE]
            self.assertEqual(header[3], binascii.crc32(crc_input) & 0xFFFFFFFF)

            partition = generate_boot_partab.PARTITION_STRUCT.unpack_from(data, generate_boot_partab.HEADER_STRUCT.size)
            self.assertEqual(b"AP", partition[0].split(b"\x00", 1)[0])
            self.assertEqual(0x3001C000, partition[1])
            self.assertEqual(0x200000, partition[2])
            self.assertEqual(0x3001C000, partition[3])

            ota_partition_offset = generate_boot_partab.HEADER_STRUCT.size + generate_boot_partab.PARTITION_STRUCT.size
            ota_partition = generate_boot_partab.PARTITION_STRUCT.unpack_from(data, ota_partition_offset)
            self.assertEqual(b"OTA_TXZ", ota_partition[0].split(b"\x00", 1)[0])
            self.assertEqual(0x30600000, ota_partition[1])
            self.assertEqual(0x100000, ota_partition[2])
            self.assertEqual(0xFFFFFFFF, ota_partition[3])
            self.assertEqual(generate_boot_partab.PART_FLAG_VALID, ota_partition[4])

    def test_write_partab_image_supports_ap_cp_and_ota_partitions(self):
        with tempfile.TemporaryDirectory() as tmp_dir:
            output = Path(tmp_dir) / "partab.bin"

            generate_boot_partab.write_partab_image(
                output,
                flash_base=0x30000000,
                boot_flash_size=0x1C000,
                partab_size=0x1000,
                control_store_base=0x30700000,
                control_store_size=0x2000,
                app_base=0x3001C000,
                app_size=0x200000,
                cp_base=0x3021C000,
                cp_size=0x100000,
                ota_base=0x30600000,
                ota_size=0x100000,
            )

            data = output.read_bytes()
            header = generate_boot_partab.HEADER_STRUCT.unpack_from(data, 0)
            self.assertEqual(3, header[10])

            ap_partition = generate_boot_partab.PARTITION_STRUCT.unpack_from(
                data, generate_boot_partab.HEADER_STRUCT.size
            )
            cp_partition = generate_boot_partab.PARTITION_STRUCT.unpack_from(
                data,
                generate_boot_partab.HEADER_STRUCT.size + generate_boot_partab.PARTITION_STRUCT.size,
            )
            ota_partition = generate_boot_partab.PARTITION_STRUCT.unpack_from(
                data,
                generate_boot_partab.HEADER_STRUCT.size + generate_boot_partab.PARTITION_STRUCT.size * 2,
            )

            self.assertEqual(b"AP", ap_partition[0].split(b"\x00", 1)[0])
            self.assertEqual(0x3001C000, ap_partition[1])
            self.assertEqual(0x200000, ap_partition[2])
            self.assertEqual(generate_boot_partab.PART_FLAG_VALID | generate_boot_partab.PART_FLAG_BOOTABLE, ap_partition[4])

            self.assertEqual(b"CP", cp_partition[0].split(b"\x00", 1)[0])
            self.assertEqual(0x3021C000, cp_partition[1])
            self.assertEqual(0x100000, cp_partition[2])
            self.assertEqual(0xFFFFFFFF, cp_partition[3])
            self.assertEqual(generate_boot_partab.PART_FLAG_VALID, cp_partition[4])

            self.assertEqual(b"OTA_TXZ", ota_partition[0].split(b"\x00", 1)[0])
            self.assertEqual(0x30600000, ota_partition[1])
            self.assertEqual(0x100000, ota_partition[2])


if __name__ == "__main__":
    unittest.main()
