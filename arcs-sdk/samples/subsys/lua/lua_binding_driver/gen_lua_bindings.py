#!/usr/bin/env python3
"""
Auto-generate Lua C bindings for LISA driver APIs.

Parses lisa_*.h headers, extracts static inline API functions,
and generates C source files that register Lua wrapper functions.

Usage:
    python3 scripts/gen_lua_bindings.py \
        --drivers-dir drivers \
        --output-dir samples/modules/lua/src/bindings

Lua API design:
    local dev = lisa.device("gpioa")   -- returns lightuserdata
    lisa.gpio.configure(dev, 5, 0x11)  -- call driver API
    local val = lisa.gpio.read_pin(dev, 5)
    local ret, remaining = lisa.wdt.get_remaining_time(dev)  -- out-params as extra returns
"""

import re
import os
import sys
import argparse
from typing import Optional
from pathlib import Path

# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------

class Param:
    def __init__(self, c_type, name, is_device=False, is_out_ptr=False,
                 is_const_struct_ptr=False, is_callback=False,
                 is_void_ptr=False, base_type=""):
        self.c_type = c_type          # e.g. "uint32_t", "const lisa_wdt_config_t *"
        self.name = name              # e.g. "pin", "config"
        self.is_device = is_device
        self.is_out_ptr = is_out_ptr
        self.is_const_struct_ptr = is_const_struct_ptr
        self.is_callback = is_callback
        self.is_void_ptr = is_void_ptr
        self.base_type = base_type    # scalar type without pointer/const


class Function:
    def __init__(self, name, return_type, params, raw_sig=""):
        self.name = name              # e.g. "lisa_gpio_configure"
        self.return_type = return_type
        self.params = params
        self.raw_sig = raw_sig


class EnumValue:
    def __init__(self, name, value):
        self.name = name
        self.value = value


class EnumDef:
    def __init__(self, name, values):
        self.name = name              # typedef name, e.g. "lisa_gpio_mode_t"
        self.values = values


class Define:
    def __init__(self, name, value):
        self.name = name
        self.value = value


class StructField:
    def __init__(self, c_type, name):
        self.c_type = c_type
        self.name = name


class StructDef:
    def __init__(self, name, fields):
        self.name = name              # typedef name, e.g. "lisa_wdt_config_t"
        self.fields = fields


class Driver:
    def __init__(self, module, header_path, functions=None, enums=None,
                 defines=None, structs=None):
        self.module = module          # e.g. "gpio"
        self.header_path = header_path
        self.functions = [] if functions is None else functions
        self.enums = [] if enums is None else enums
        self.defines = [] if defines is None else defines
        self.structs = [] if structs is None else structs

# ---------------------------------------------------------------------------
# C type classification helpers
# ---------------------------------------------------------------------------

SCALAR_TYPES = {
    "int", "unsigned int",
    "uint8_t", "uint16_t", "uint32_t", "uint64_t",
    "int8_t", "int16_t", "int32_t", "int64_t",
    "size_t", "ssize_t",
    "bool",
    "float", "double",
}

CALLBACK_RE = re.compile(r"lisa_\w+_callback_t")

# Enum types we know are integers
ENUM_TYPE_RE = re.compile(r"lisa_\w+_t")


def is_scalar(base: str) -> bool:
    base = base.strip()
    if base in SCALAR_TYPES:
        return True
    if ENUM_TYPE_RE.match(base) and "config" not in base.lower() \
       and "capabilities" not in base.lower() and "parameters" not in base.lower() \
       and "layout" not in base.lower() and "msg" not in base.lower() \
       and "transfer" not in base.lower() and "time" not in base.lower() \
       and "alarm" not in base.lower() and "buf_config" not in base.lower():
        return True
    return False

# ---------------------------------------------------------------------------
# Parser
# ---------------------------------------------------------------------------

def strip_comments(text: str) -> str:
    """Remove C comments."""
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
    text = re.sub(r'//[^\n]*', '', text)
    return text


def parse_enum(text: str) -> list:
    """Extract typedef enum { ... } name_t; definitions."""
    results = []
    pattern = re.compile(
        r'typedef\s+enum\s*\{([^}]*)\}\s*(\w+)\s*;',
        re.DOTALL
    )
    for m in pattern.finditer(text):
        body, name = m.group(1), m.group(2)
        values = []
        for line in body.split(','):
            line = line.strip()
            if not line:
                continue
            eq = line.split('=', 1)
            vname = eq[0].strip().split('/')[-1].strip()  # handle trailing comments
            vname = vname.split()[0] if vname else ""
            if not vname or not re.match(r'^[A-Z_]\w*$', vname):
                continue
            vval = eq[1].strip() if len(eq) > 1 else None
            values.append(EnumValue(vname, vval))
        if values and name not in SKIP_ENUMS:
            results.append(EnumDef(name, values))
    return results


def parse_defines(text: str) -> list:
    """Extract #define LISA_xxx integer/hex constants."""
    results = []
    for m in re.finditer(r'#define\s+(LISA_\w+)\s+((?:0x[\da-fA-F]+|\d+)(?:U|UL|L)?)\b', text):
        name, val = m.group(1), m.group(2)
        # Skip masks that are complex expressions
        if '(' in val or '<<' in val:
            continue
        results.append(Define(name, val))
    # Also match bit-shift defines: #define LISA_xxx (expr)
    for m in re.finditer(r'#define\s+(LISA_\w+)\s+\((\d+\s*<<\s*\d+)\)', text):
        name, val = m.group(1), m.group(2)
        results.append(Define(name, val))
    # Match OR-combined defines: (X | Y)
    for m in re.finditer(r'#define\s+(LISA_\w+)\s+\(([A-Z_\d\s|]+)\)', text):
        name, val = m.group(1), m.group(2).strip()
        if '<<' not in val and '|' in val:
            results.append(Define(name, val))
    return results


STRUCT_SCALAR_TYPES = SCALAR_TYPES | {"lisa_adc_reference_t", "lisa_adc_resolution_t",
    "lisa_pwm_polarity_t", "lisa_hwtimer_mode_t",
    "lisa_uart_baudrate_t", "lisa_uart_data_bits_t", "lisa_uart_stop_bits_t",
    "lisa_uart_parity_t", "lisa_uart_flow_control_t", "lisa_uart_transfer_mode_t",
    "lisa_wdt_action_t", "lisa_spi_mode_t", "lisa_spi_bit_order_t",
    "lisa_spi_transfer_flags_t", "lisa_spi_transfer_mode_t",
    "lisa_i2c_speed_t",
}


def parse_struct(text: str) -> list:
    """Extract simple typedef struct { scalar fields } name_t;"""
    results = []
    pattern = re.compile(
        r'typedef\s+struct\s*\{([^}]*)\}\s*(\w+_(?:config|time|alarm)_t)\s*;',
        re.DOTALL
    )
    for m in pattern.finditer(text):
        body, name = m.group(1), m.group(2)
        fields = []
        for line in body.split(';'):
            line = line.strip()
            if not line or line.startswith('struct') or line.startswith('union'):
                continue
            # Try to parse "type name"
            parts = line.rsplit(None, 1)
            if len(parts) == 2:
                ft, fn = parts[0].strip(), parts[1].strip()
                if re.match(r'^\w+$', fn) and not fn.startswith('('):
                    # Only include fields with scalar types
                    if ft.strip() in STRUCT_SCALAR_TYPES:
                        fields.append(StructField(ft.strip(), fn))
        if fields:
            results.append(StructDef(name, fields))
    return results


def parse_functions(text: str) -> list:
    """Extract static inline function declarations."""
    results = []
    pattern = re.compile(
        r'static\s+inline\s+([\w\s\*]+?)\s+(lisa_\w+)\s*\(([^)]*)\)',
        re.MULTILINE
    )
    for m in pattern.finditer(text):
        ret_type = m.group(1).strip()
        fname = m.group(2).strip()
        params_str = m.group(3).strip()
        raw_sig = m.group(0)

        # Skip internal / non-API functions
        if '_api_t' in fname:
            continue

        params = parse_params(params_str)
        results.append(Function(fname, ret_type, params, raw_sig))
    return results


def parse_params(params_str: str) -> list:
    """Parse a C parameter list string."""
    params = []
    if not params_str or params_str == "void":
        return params

    for p in params_str.split(','):
        p = p.strip()
        if not p:
            continue

        param = classify_param(p)
        if param:
            params.append(param)
    return params


def classify_param(decl: str) -> Optional[Param]:
    """Classify a single parameter declaration."""
    decl = decl.strip()
    # Remove array brackets if any
    decl = re.sub(r'\[\d*\]', '', decl)

    # Split into type and name
    m = re.match(r'^(.*?)(\w+)\s*$', decl)
    if not m:
        return None
    c_type = m.group(1).strip()
    name = m.group(2).strip()

    param = Param(c_type=c_type, name=name)

    # Device pointer
    if 'lisa_device_t' in c_type:
        param.is_device = True
        return param

    # Callback function pointer
    if CALLBACK_RE.search(c_type):
        param.is_callback = True
        return param

    # void pointer (user_data) or non-const void *
    if re.match(r'^(const\s+)?void\s*\*$', c_type.strip()):
        if 'const' in c_type:
            # const void * - buffer input, treat as const_struct_ptr with base="void"
            param.is_const_struct_ptr = True
            param.base_type = "void"
            return param
        param.is_void_ptr = True
        return param

    # Const struct pointer - input from Lua table
    if 'const' in c_type and '*' in c_type:
        base = c_type.replace('const', '').replace('*', '').strip()
        if not is_scalar(base):
            param.is_const_struct_ptr = True
            param.base_type = base
            return param
        # const scalar pointer - treat as input scalar
        param.base_type = base
        return param

    # Non-const pointer - out-param
    if '*' in c_type:
        base = c_type.replace('*', '').strip()
        param.is_out_ptr = True
        param.base_type = base
        return param

    # Plain scalar / enum
    param.base_type = c_type
    return param


def parse_header(path: str) -> Driver:
    """Parse a single driver header file."""
    basename = os.path.basename(path)
    module = basename.replace('lisa_', '').replace('.h', '')

    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
        raw = f.read()

    text = strip_comments(raw)

    driver = Driver(
        module=module,
        header_path=path,
        functions=parse_functions(text),
        enums=parse_enum(text),
        defines=parse_defines(raw),  # parse defines from raw (before comment strip)
        structs=parse_struct(text),
    )
    return driver

# ---------------------------------------------------------------------------
# Code generator
# ---------------------------------------------------------------------------

def lua_func_name(c_name: str, module: str) -> str:
    """Convert lisa_gpio_read_pin -> read_pin"""
    prefix = f"lisa_{module}_"
    if c_name.startswith(prefix):
        return c_name[len(prefix):]
    return c_name


# Functions that are behind optional #ifdef and should not be bound
SKIP_FUNCTIONS = {
    "lisa_uart_write_async",
    "lisa_uart_set_callback",
    "lisa_uart_write_abort",
    "lisa_uart_get_tx_count",
    # 仅在 CONFIG_LISA_PM=y 下定义，lua 样例未启用 PM，跳过避免未声明类型
    "lisa_gpio_configure_wakeup",
    "lisa_gpio_clear_wakeup",
}

# Enum typedefs guarded by optional config; skip emitting their constants
SKIP_ENUMS = {
    "lisa_gpio_wakeup_trigger_t",
}


def can_bind(func: Function) -> bool:
    """Check if we can auto-generate a binding for this function."""
    if func.name in SKIP_FUNCTIONS:
        return False
    for p in func.params:
        if p.is_callback or p.is_void_ptr:
            return False
        # Skip functions with const void * buffer params (flash read/write, etc.)
        if p.is_const_struct_ptr and p.base_type == "void":
            return False
    return True


def gen_param_fetch(param: Param, idx: int) -> tuple:
    """Generate code to fetch a param from Lua stack. Returns (decl, fetch, idx_next)."""
    if param.is_device:
        decl = f"    lisa_device_t *{param.name} = (lisa_device_t *)lua_touserdata(L, {idx});"
        return decl, idx + 1

    if param.is_out_ptr:
        decl = f"    {param.base_type} {param.name} = 0;"
        return decl, idx  # no stack consumption

    if param.is_const_struct_ptr:
        # Generate code to read struct fields from Lua table
        return None, idx + 1  # handled specially

    base = param.base_type or param.c_type
    if base == "bool":
        decl = f"    bool {param.name} = lua_toboolean(L, {idx});"
    elif base in ("float", "double"):
        decl = f"    {base} {param.name} = ({base})lua_tonumber(L, {idx});"
    else:
        decl = f"    {base} {param.name} = ({base})luaL_checkinteger(L, {idx});"
    return decl, idx + 1


def gen_struct_from_table(param: Param, idx: int) -> list:
    """Generate code to populate a struct from a Lua table."""
    # We need the struct definition - this is handled at generation time
    return idx + 1


def gen_binding_func(func: Function, module: str, structs: dict) -> Optional[str]:
    """Generate a single Lua C binding function."""
    if not can_bind(func):
        return None

    lua_name = lua_func_name(func.name, module)
    c_func = f"l_{module}_{lua_name}"

    lines = []
    lines.append(f"static int {c_func}(lua_State *L)")
    lines.append("{")

    # Parse parameters
    idx = 1
    call_args = []
    out_params = []
    struct_params = []

    for p in func.params:
        if p.is_device:
            lines.append(f"    lisa_device_t *{p.name} = (lisa_device_t *)lua_touserdata(L, {idx});")
            call_args.append(p.name)
            idx += 1
        elif p.is_out_ptr:
            if is_scalar(p.base_type):
                lines.append(f"    {p.base_type} {p.name} = 0;")
            else:
                lines.append(f"    {p.base_type} {p.name};")
                lines.append(f"    memset(&{p.name}, 0, sizeof({p.name}));")
            call_args.append(f"&{p.name}")
            out_params.append(p)
        elif p.is_const_struct_ptr:
            # Read struct from Lua table
            sname = p.base_type
            sdef = structs.get(sname)
            lines.append(f"    {sname} {p.name};")
            if sdef:
                lines.append(f"    memset(&{p.name}, 0, sizeof({p.name}));")
                lines.append(f"    if (lua_istable(L, {idx})) {{")
                for sf in sdef.fields:
                    lines.append(f"        lua_getfield(L, {idx}, \"{sf.name}\");")
                    if sf.c_type == "bool":
                        lines.append(f"        if (!lua_isnil(L, -1)) {p.name}.{sf.name} = lua_toboolean(L, -1);")
                    else:
                        lines.append(f"        if (!lua_isnil(L, -1)) {p.name}.{sf.name} = ({sf.c_type})lua_tointeger(L, -1);")
                    lines.append(f"        lua_pop(L, 1);")
                lines.append(f"    }}")
            else:
                lines.append(f"    memset(&{p.name}, 0, sizeof({p.name}));")
                lines.append(f"    /* WARNING: struct {sname} not parsed, fields not populated */")
            call_args.append(f"&{p.name}")
            idx += 1
        else:
            base = p.base_type or p.c_type
            if base == "bool":
                lines.append(f"    bool {p.name} = lua_toboolean(L, {idx});")
            elif base in ("float", "double"):
                lines.append(f"    {base} {p.name} = ({base})lua_tonumber(L, {idx});")
            else:
                lines.append(f"    {base} {p.name} = ({base})luaL_checkinteger(L, {idx});")
            call_args.append(p.name)
            idx += 1

    # Call the actual C function
    call = f"    int ret = {func.name}({', '.join(call_args)});"
    if func.return_type == "void":
        call = f"    {func.name}({', '.join(call_args)});"
    lines.append(call)

    # Push return values
    nret = 0
    if func.return_type != "void":
        lines.append("    lua_pushinteger(L, ret);")
        nret = 1

    for op in out_params:
        base = op.base_type
        if is_scalar(base):
            if base == "bool":
                lines.append(f"    lua_pushboolean(L, {op.name});")
            elif base in ("float", "double"):
                lines.append(f"    lua_pushnumber(L, {op.name});")
            else:
                lines.append(f"    lua_pushinteger(L, {op.name});")
        else:
            # Struct out-param - return as Lua table
            sdef = structs.get(base)
            lines.append(f"    lua_newtable(L);")
            if sdef:
                for sf in sdef.fields:
                    if sf.c_type == "bool":
                        lines.append(f"    lua_pushboolean(L, {op.name}.{sf.name});")
                    else:
                        lines.append(f"    lua_pushinteger(L, {op.name}.{sf.name});")
                    lines.append(f'    lua_setfield(L, -2, "{sf.name}");')
        nret += 1

    lines.append(f"    return {nret};")
    lines.append("}")
    return "\n".join(lines)


def gen_driver_file(driver: Driver, structs_map: dict) -> str:
    """Generate the complete C file for a driver module."""
    lines = []
    lines.append("/* Auto-generated by gen_lua_bindings.py - DO NOT EDIT */")
    lines.append("")
    lines.append('#include "lua.h"')
    lines.append('#include "lauxlib.h"')
    lines.append('#include "lualib.h"')
    lines.append('#include "lisa_device.h"')
    lines.append(f'#include "lisa_{driver.module}.h"')
    lines.append('#include <string.h>')
    lines.append("")

    # Generate binding functions
    bound_funcs = []
    for func in driver.functions:
        code = gen_binding_func(func, driver.module, structs_map)
        if code:
            lines.append(code)
            lines.append("")
            bound_funcs.append(func)

    # Generate luaL_Reg table
    lines.append(f"static const luaL_Reg {driver.module}_lib[] = {{")
    for func in bound_funcs:
        lua_name = lua_func_name(func.name, driver.module)
        c_func = f"l_{driver.module}_{lua_name}"
        lines.append(f'    {{"{lua_name}", {c_func}}},')
    lines.append("    {NULL, NULL}")
    lines.append("};")
    lines.append("")

    # Generate constants registration
    lines.append(f"static void {driver.module}_register_constants(lua_State *L)")
    lines.append("{")
    for enum in driver.enums:
        for ev in enum.values:
            lines.append(f'    lua_pushinteger(L, {ev.name});')
            lines.append(f'    lua_setfield(L, -2, "{ev.name}");')
    for d in driver.defines:
        lines.append(f'    lua_pushinteger(L, {d.name});')
        lines.append(f'    lua_setfield(L, -2, "{d.name}");')
    lines.append("}")
    lines.append("")

    # Generate luaopen function
    lines.append(f"int luaopen_lisa_{driver.module}(lua_State *L)")
    lines.append("{")
    lines.append(f'    luaL_newlib(L, {driver.module}_lib);')
    lines.append(f'    {driver.module}_register_constants(L);')
    lines.append("    return 1;")
    lines.append("}")
    lines.append("")

    return "\n".join(lines)


def gen_registry_header(drivers: list) -> str:
    """Generate the master header that registers all driver modules."""
    lines = []
    lines.append("/* Auto-generated by gen_lua_bindings.py - DO NOT EDIT */")
    lines.append("#pragma once")
    lines.append("")
    lines.append('#include "lua.h"')
    lines.append('#include "lauxlib.h"')
    lines.append('#include "lisa_device.h"')
    lines.append('#include "FreeRTOS.h"')
    lines.append('#include "task.h"')
    lines.append("")

    for d in drivers:
        lines.append(f"int luaopen_lisa_{d.module}(lua_State *L);")
    lines.append("")

    # Helper: lisa.device("name") -> lightuserdata
    lines.append("static int l_lisa_device_get(lua_State *L)")
    lines.append("{")
    lines.append('    const char *name = luaL_checkstring(L, 1);')
    lines.append("    lisa_device_t *dev = lisa_device_get(name);")
    lines.append("    if (!dev) {")
    lines.append('        return luaL_error(L, "device not found: %s", name);')
    lines.append("    }")
    lines.append("    lua_pushlightuserdata(L, dev);")
    lines.append("    return 1;")
    lines.append("}")
    lines.append("")

    # Helper: lisa.device_ready(dev) -> bool
    lines.append("static int l_lisa_device_ready(lua_State *L)")
    lines.append("{")
    lines.append("    lisa_device_t *dev = (lisa_device_t *)lua_touserdata(L, 1);")
    lines.append("    lua_pushboolean(L, dev && lisa_device_ready(dev));")
    lines.append("    return 1;")
    lines.append("}")
    lines.append("")

    # Helper: lisa.msleep(ms) - FreeRTOS delay
    lines.append("static int l_lisa_msleep(lua_State *L)")
    lines.append("{")
    lines.append("    int ms = (int)luaL_checkinteger(L, 1);")
    lines.append("    vTaskDelay(pdMS_TO_TICKS(ms));")
    lines.append("    return 0;")
    lines.append("}")
    lines.append("")

    # Registration function
    lines.append("static void luaopen_lisa_all(lua_State *L)")
    lines.append("{")
    lines.append('    /* Create top-level "lisa" table */')
    lines.append("    lua_newtable(L);")
    lines.append("")
    lines.append('    /* lisa.device(name) */')
    lines.append('    lua_pushcfunction(L, l_lisa_device_get);')
    lines.append('    lua_setfield(L, -2, "device");')
    lines.append("")
    lines.append('    /* lisa.device_ready(dev) */')
    lines.append('    lua_pushcfunction(L, l_lisa_device_ready);')
    lines.append('    lua_setfield(L, -2, "device_ready");')
    lines.append("")
    lines.append('    /* lisa.msleep(ms) */')
    lines.append('    lua_pushcfunction(L, l_lisa_msleep);')
    lines.append('    lua_setfield(L, -2, "msleep");')
    lines.append("")

    for d in drivers:
        lines.append(f'    /* lisa.{d.module} */')
        lines.append(f'    luaL_requiref(L, "lisa.{d.module}", luaopen_lisa_{d.module}, 0);')
        lines.append(f'    lua_setfield(L, -2, "{d.module}");')
        lines.append("")

    lines.append('    lua_setglobal(L, "lisa");')
    lines.append("}")
    lines.append("")

    return "\n".join(lines)

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

# Which drivers to generate bindings for
TARGET_DRIVERS = [
    "gpio", "pwm", "adc", "wdt", "rtc", "hwtimer",
    "flash", "spi", "i2c", "uart",
]


def main():
    parser = argparse.ArgumentParser(description="Generate Lua FFI bindings for LISA drivers")
    parser.add_argument("--drivers-dir", required=True, help="Path to drivers/ directory")
    parser.add_argument("--output-dir", required=True, help="Output directory for generated files")
    args = parser.parse_args()

    drivers_dir = Path(args.drivers_dir)
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)

    drivers = []
    all_structs = {}

    # Parse all target driver headers
    for mod in TARGET_DRIVERS:
        header = drivers_dir / f"lisa_{mod}" / f"lisa_{mod}.h"
        if not header.exists():
            print(f"WARNING: {header} not found, skipping", file=sys.stderr)
            continue

        print(f"Parsing {header}...")
        driver = parse_header(str(header))

        # Build struct map
        for s in driver.structs:
            all_structs[s.name] = s

        drivers.append(driver)

    # Generate C files
    for driver in drivers:
        code = gen_driver_file(driver, all_structs)
        out_path = output_dir / f"lua_lisa_{driver.module}.c"
        with open(out_path, 'w', encoding='utf-8') as f:
            f.write(code)
        nfuncs = sum(1 for func in driver.functions if can_bind(func))
        nskip = len(driver.functions) - nfuncs
        print(f"  {driver.module}: {nfuncs} functions bound, {nskip} skipped -> {out_path}")

    # Generate registry header
    header_code = gen_registry_header(drivers)
    header_path = output_dir / "lua_lisa_all.h"
    with open(header_path, 'w', encoding='utf-8') as f:
        f.write(header_code)
    print(f"Registry header -> {header_path}")

    print(f"\nDone! Generated bindings for {len(drivers)} drivers.")


if __name__ == "__main__":
    main()
