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
                if symb != "":
                    fout.write("    " + symb + " = " + addr + ";\n")
            fout.write(code_end)


if __name__ == "__main__":
    if not len(sys.argv) == 3:
        print(usage)
        sys.exit(1)
    fname = sys.argv[1]
    outname = sys.argv[2]
    convert_ld(fname, outname)
