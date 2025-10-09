import struct
import sys
import os

try:
    import sym_to_ld
except ImportError:
    # Append this path to the module search path to allow running this module
    sys.path.append(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    import sym_to_ld

usage = '''
Usage:
$ ./sym_to_h.py symbol.txt func_list.txt out.h
'''
temp_start = '''

/*
 * template function: void *patch_func_ptr2(void *org_func_ptr)

void *patch_func_ptr2(void *org_func_ptr)
{
    void *ret = NULL;

    switch((uint32_t)org_func_ptr) {
'''
temp_end = '''    default:
        ret = NULL;
        break;
    }

    return ret;
}

 * template end
 */

'''

code_start = '''
#include <stdint.h>
#include <stddef.h>


'''

code_end = '''
#endif

'''


def convert_h(fname, fsym_lst, outname):
    symb_dict = sym_to_ld.get_symb_dict(fname)
    symb_list = list()
    with open(fsym_lst, 'r') as fin:
        with open(outname, 'w') as fout:
            sname = os.path.basename(outname).upper().replace(".", "_")
            fout.write("#ifndef INCLUDE_%s_\n" % sname)
            fout.write("#define INCLUDE_%s_\n" % sname)
            fout.write(code_start)
            while True:
                line = fin.readline()
                if line == '':
                    break
                symb = line.rstrip().lstrip()
                if symb != "":
                    if symb_dict.get(symb, "") == "":
                        warning = "Warning!! Symbol \"" + symb + "\" not found in symbol table."
                        print(warning)
                        fout.write("//" + warning + "\n")
                        continue
                    dfn = "#define PTR_" + symb
                    strlen = len(dfn)
                    dfnlen = 70
                    if strlen < dfnlen:
                        dfn = dfn + ' '*(dfnlen-strlen)
                    else:
                        dfn = dfn + ' '
                    fout.write(dfn + symb_dict[symb] + "\n")
                    symb_list.append("PTR_" + symb)
            fout.write(temp_start)
            for symb in symb_list:
                fout.write("    " + "case " + symb + ":\n" + "        " + "break;\n")
            fout.write(temp_end)
            fout.write(code_end)


if __name__ == "__main__":
    if not len(sys.argv) == 4:
        print(usage)
        sys.exit(1)
    fname = sys.argv[1]
    fsym_lst = sys.argv[2]
    outname = sys.argv[3]
    convert_h(fname, fsym_lst, outname)
