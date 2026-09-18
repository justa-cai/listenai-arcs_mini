#!/usr/bin/env python3
"""Check the actual ARCS ELF's ADB FIFO allocation, without third-party modules."""
import argparse
import struct
from pathlib import Path


def check(elf):
    data = Path(elf).read_bytes()
    assert data[:6] == b'\x7fELF\x01\x01', 'Expected a little-endian ELF32 firmware'
    header = struct.unpack_from('<16sHHIIIIIHHHHHH', data)
    sections = [struct.unpack_from('<10I', data, header[6] + i * header[11])
                for i in range(header[12])]
    table = None
    for section in sections:
        if section[1] != 2:  # SHT_SYMTAB
            continue
        strings_section = sections[section[6]]
        strings = data[strings_section[4]:strings_section[4] + strings_section[5]]
        for offset in range(section[4], section[4] + section[5], section[9]):
            name, value, size, _, _, index = struct.unpack_from('<IIIBBH', data, offset)
            if strings[name:].split(b'\0', 1)[0] == b'musb_device_table':
                owner = sections[index]
                start = owner[4] + value - owner[3]
                table = data[start:start + size]
    assert table is not None, 'musb_device_table is missing from ELF'
    assert len(table) % 8 == 0, 'Unexpected musb_fifo_cfg layout'
    offset = 0
    adb = {}
    for ep, style, mode, maxpacket in struct.iter_unpack('<BBBxI', table):
        assert mode == 0, 'This check expects single-buffered FIFOs'
        size = 1 << max(3, (maxpacket - 1).bit_length())
        if ep == 2:
            for direction in ({0: ('TX',), 1: ('RX',), 2: ('TX', 'RX')}[style]):
                assert direction not in adb, 'Duplicate ADB FIFO direction'
                adb[direction] = (offset, offset + size)
                assert size >= 512, 'HS bulk packet does not fit in FIFO'
        offset += size
    assert set(adb) == {'TX', 'RX'}, 'ADB needs both directions'
    tx, rx = adb['TX'], adb['RX']
    assert tx[1] <= rx[0] or rx[1] <= tx[0], 'ADB TX/RX FIFO RAM overlaps'
    assert offset <= 4096, 'ARCS FIFO RAM overflow'
    print(f'EP2 TX={tx}, RX={rx}; total FIFO RAM={offset}/4096 bytes: PASS')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('elf', help='Built arcs-mini ELF, not the .bin image')
    check(parser.parse_args().elf)
