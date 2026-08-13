import struct
import sys
import os

usage = '''
Usage:
$ ./sym_to_ld.py symbol.txt out.ld
'''

code_start = '''
SECTIONS
{

'''

code_end = '''
}

'''

EXCLUDED_SYMBOLS = {
    "BOOT_HARTID",
    "__HEAP_SIZE",
    "__STACK_SIZE",
    "__TOT_STACK_SIZE",
    "__SMP_CPU_CNT",
    "vector_base",
    "_start",
    "_start_premain",
    "__skip_init",
    "__init_common",
    "__amp_wait",
    "early_exc_entry",
    "irq_entry",
    "exc_entry",
    "g_irqvector",
    "main",
    "_text",
    "_text_lma",
    "_etext",
    "_data",
    "_data_lma",
    "_edata",
    "__bss_start",
    "_end",
    "__global_pointer$",
    "__tls_base",
    "_sp",
}

EXCLUDED_PREFIXES = (
    "__copy_table",
    "__zero_table",
    "__ram",
    "__init_array",
    "__fini_array",
    "__preinit_array",
)


def should_export_symbol(symb):
    if symb in EXCLUDED_SYMBOLS:
        return False

    for prefix in EXCLUDED_PREFIXES:
        if symb.startswith(prefix):
            return False

    return True


def get_symb_addr(line):
    idx0 = 0
    idx1 = line.find(" ", idx0)  # find space
    addr = "0x" + line[idx0:idx1]  # addr: str = "0x" + line[idx0:idx1]
    idx0 = idx1 + 1
    idx1 = line.find(" ", idx0)  # find space
    attr = line[idx0:idx1]
    #if attr >= 'a':   # filter out a ~ z, it is local symbol
    #    return "", ""
    if attr == 'T' or attr == 't' or attr == 'W':  # function address must add 1
        val = int(addr, 16)  # val: int = int(addr, 16)
        # addr = '0x%x' % (val + 1) // TODO only use in ARM Arch
        addr = '0x%x' % (val)
    idx0 = idx1 + 1
    idx1 = line.find("\t", idx0)  # find tab
    if idx1 == -1:
        idx1 = len(line) - 1  # cut "\n" at end of line
    if idx0 == idx1:
        return "", ""
    symb = line[idx0:idx1]
    return symb, addr


def get_symb_dict(fname):
    symb_dict = dict()
    with open(fname, 'r') as fin:
        while True:
            line = fin.readline()
            if line == '':
                break
            symb, addr = get_symb_addr(line)
            if symb != "":
                symb_dict[symb] = addr
    return symb_dict


def convert_ld(fname, outname):
    with open(fname, 'r') as fin:
        with open(outname, 'w') as fout:
            fout.write(code_start)
            while True:
                line = fin.readline()
                if line == '':
                    break
                symb, addr = get_symb_addr(line)
                if symb != "" and should_export_symbol(symb):
                    fout.write("    " + symb + " = " + addr + ";\n")
            fout.write(code_end)


if __name__ == "__main__":
    if not len(sys.argv) == 3:
        print(usage)
        sys.exit(1)
    fname = sys.argv[1]
    outname = sys.argv[2]
    convert_ld(fname, outname)
