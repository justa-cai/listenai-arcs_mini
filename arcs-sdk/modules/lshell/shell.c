#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include "shell_priv.h"

static shell_t *shell_inst = NULL;

static const shell_cmd_t __priv shell_item_tab[] = {
	// register key
	SHELL_KEY_ITEM(0, 				0x1B5B4100, shell_key_up	, "up"),
	SHELL_KEY_ITEM(0, 				0x1B5B4200, shell_key_down	, "down"),
	SHELL_KEY_ITEM(0, 				0x09000000, shell_key_tab	, "tab"),
	SHELL_KEY_ITEM(SHELL_NON_AUTH,  0x1B5B4300, shell_key_right	, "right"),
	SHELL_KEY_ITEM(SHELL_NON_AUTH,  0x1B5B4400, shell_key_left	, "left"),
	SHELL_KEY_ITEM(SHELL_NON_AUTH,  0x08000000, shell_key_back	, "back"),
	SHELL_KEY_ITEM(SHELL_NON_AUTH,  0x7F000000, shell_key_del	, "del"),
	SHELL_KEY_ITEM(SHELL_NON_AUTH,  0x1B5B337E, shell_key_del	, "del"),
	SHELL_KEY_ITEM(SHELL_NON_AUTH,  0x0A000000, shell_key_enter	, "enter"),
	SHELL_KEY_ITEM(SHELL_NON_AUTH,  0x0D000000, shell_key_enter	, "enter"),

	// register cmd
	SHELL_CMD_ITEM(SHELL_TYPE_CMD_STRS, "help", shell_cmd_help, "help [cmd]"),
	SHELL_CMD_ITEM(SHELL_TYPE_CMD_STRS, "clear", shell_cmd_clear, "clear console"),
	SHELL_CMD_ITEM(SHELL_TYPE_CMD_ARGS, "dump", shell_cmd_dump, "memory dump"),
	SHELL_CMD_ITEM(SHELL_TYPE_CMD_STRS| SHELL_SHOW_RET, "run", shell_cmd_exec, "run <addr> [...]")
};
static const char *shell_text_tab[] = {
	[SHELL_TEXT_LOGO] = CORDEF
		CRLF " _   _       _                  _         _          _ _ "
		CRLF "| | |_| ___ | |_ ___  __   ___ |_|    ___| |__   ___| | |"
		CRLF "| |  _ / __/|  _/ _ \\/  \\ / _ \\ _    / __| |_ \\_/ _ \\ | |"
		CRLF "| |_| |\\__ \\| | |__ / /\\ \\ |_\\ | |   \\__ \\ | | |  __/ | |"
		CRLF "|___|_|/___/\\__\\\\__\\|_||_|\\__/\\__|   |___/_| |_|\\___\\_|_|"
		CRLF CRLF "VER: 1.2@" __DATE__", Copyright: (c)2020 Listenai Co., Ltd.",
	[SHELL_TEXT_CMD_LONG] = CRLF "cmd too long" CRLF,
	[SHELL_TEXT_CMD_MISS] = "cmd not found" CRLF,
	[SHELL_TEXT_HELP_HDR] = "cmd help of ",
	[SHELL_TEXT_ARG_ERR] = "param err" CRLF,
	[SHELL_TEXT_CLR_CONSOLE] = "\033[2J\033[1H",
	[SHELL_TEXT_CLR_LINE] = "\033[2K\r",
	[SHELL_TEXT_BACK_SPACE] = "\b \b",
	[SHELL_TEXT_MEM_FAIL] = CRLF "<!SHELL:FAIL!>" CRLF
};

static inline shell_t *shell_get_inst(bool force)
{
	return shell_inst && (force || shell_inst->status.active) ? shell_inst : NULL;
}

static inline int shell_write_chr(shell_t *shell, const char chr)
{
	SHELL_CHECK(shell, return 0);
	return shell->writes(&chr, 1, shell->user);
}

static inline int shell_write_str(shell_t *shell, const char *str)
{
	SHELL_CHECK(shell, return 0);
	return shell->writes(str, strlen(str), shell->user);
}

static inline int shell_write_text(shell_t *shell, enum shel_text_t text)
{
	return shell_write_str(shell, shell_text_tab[text]);
}

static void shell_inner_printf(shell_t *shell, char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	int len = vsnprintf(NULL, 0, fmt, ap);
	if (len > 0) {
		if (len > CONFIG_SHELL_LINE_LIMIT - 1) len = CONFIG_SHELL_LINE_LIMIT - 1;
		char *const buf = shell_malloc(len + 1);
		vsnprintf(buf, len + 1, fmt, ap);
		shell_write_str(shell, buf);
		shell_free(buf);
	}
	va_end(ap);
}

static inline char shell_parse_chr(char *str)
{
	if (str[0] != '\\') return str[0];
	switch (str[1]) {
	case 'b': return '\b';
	case 'r': return '\r';
	case 'n': return '\n';
	case 't': return '\t';
	case '0': return 0;
	default : return str[1];
	}
}

static uint32_t shell_parse_param(shell_t *shell, char *str)
{
	if (str[0] == '\'' && str[1]) return (uint32_t)shell_parse_chr(str + 1);
	if (str[0] == '-' || (str[0] >= '0' && str[0] <= '9')) {
		int type = 0;
		int radix = 10;
		int sign = 1;
		uint32_t lval = 0;
		uint32_t div = 0;
		if (str[0] == '-') sign = -1, str++;
		if (str[0] == '0')
			switch (*++str) {
			case 'x':
			case 'X': radix = 16; str++; break;
			case 'b':
			case 'B': radix = 2; str++; break;
			case '.': type = 1; break;
			default: radix = 8; break;
			}
		else if (strchr(str + 1, '.')) type = 1;
		while (*str) {
			char code = *str++;
			if (code == '.') div = 1;
			else {
				lval *= radix;
				div *= 10;
				switch (code) {
				case '0' ... '9': lval += code - '0'; break;
				case 'a' ... 'f': lval += code - 'a' + 10; break;
				case 'A' ... 'F': lval += code - 'A' + 10; break;
				default: return 0;
				}
			}
		}
		if (type && div) return (uint32_t)((float)lval / div * sign);
		return (uint32_t)(lval * sign);
	}
	if (str[0]) {
		char *p = str;
		int pos = 0;
		if (str[0] == '\"') p = ++str;
		while (*p) {
			switch (*p) {
			case '\\': str[pos++] = shell_parse_chr(p++); break;
			case '\"': str[pos++] = 0; break;
			default  : str[pos++] = *p; break;
			}
			p++;
		}
		str[pos] = 0;
		return (uint32_t)str;
	}
	return 0;
}

static inline void shell_print_prot(shell_t *shell, uint8_t newline)
{
	shell_inner_printf(shell, CORDEF "%sroot:/$ ", newline ? CRLF : "");
}

static inline uint16_t shell_strcmp(const char *dest, const char *src)
{
	uint16_t match = 0;
	while (src[match] && dest[match] == src[match]) match++;
	return match;
}

static inline const char *shell_get_name(shell_cmd_t *cmd)
{
	static char key[10] = { 0 };
	switch (cmd->attr.attrs.type) {
	case SHELL_TYPE_CMD_STRS:
	case SHELL_TYPE_CMD_ARGS: return (char *)cmd->data.cmd.name;
	case SHELL_TYPE_KEY: 
		snprintf(key, sizeof(key) - 1, "%08x", cmd->data.key.value); 
		return key;
	default: return NULL;
	}
}

static inline const char *shell_get_desc(shell_cmd_t *cmd)
{
	switch (cmd->attr.attrs.type) {
	case SHELL_TYPE_CMD_STRS: return cmd->data.cmd.desc;
	case SHELL_TYPE_CMD_ARGS: return cmd->data.cmd.desc;
	case SHELL_TYPE_KEY:      return cmd->data.key.desc;
	default:				  return NULL;
	}
}

static void shell_clr_line(shell_t *shell)
{
	for (short i = shell->parser.len - shell->parser.cursor; i > 0; i--)
		shell_write_chr(shell, ' ');
	for (short i = 0; i < shell->parser.len; i++)
		shell_write_text(shell, SHELL_TEXT_BACK_SPACE);
}

static shell_cmd_t *shell_seek_cmd(shell_t *shell, const char *cmd, shell_cmd_t *base, uint16_t len)
{
	uint16_t cnt = shell->cmdlist.count - ((int)base - (int)shell->cmdlist.base) / sizeof(shell_cmd_t);
	for (uint16_t i = 0; i < cnt; i++)
		if (base[i].attr.attrs.type != SHELL_TYPE_KEY) {
			const char *name = shell_get_name(&base[i]);
			if (!(len ? strncmp(cmd, name, len) : strcmp(cmd, name))) return &base[i];
		}
	return NULL;
}

static void shell_get_history(shell_t *shell, char dir)
{
	SHELL_CHECK(dir, return);
	if (dir > 0) {
		short val = shell->history.his_num > shell->history.his_pos ? -shell->history.his_num : -shell->history.his_pos;
		if (shell->history.his_offs-- <= val) shell->history.his_offs = val;
	} else if (++shell->history.his_offs > 0) {
		shell->history.his_offs = 0;
		return;
	}
	shell_clr_line(shell);
	if (shell->history.his_offs == 0) shell->parser.cursor = shell->parser.len = 0;
	else {
		int pos = (shell->history.his_pos + shell->history.his_offs + SHELL_HISTORY_MAX_CNT) % SHELL_HISTORY_MAX_CNT;
		shell->parser.len = strlen(strcpy(shell->parser.buf, shell->history.item[pos]));
		SHELL_CHECK(shell->parser.len, return);
		shell->parser.cursor = shell->parser.len;
		shell_write_str(shell, shell->parser.buf);
	}
}

static int shell_ext_run(shell_t *shell, shell_cmd_t *cmd, int argc, char *argv[])
{
	uint32_t va[SHELL_PARAM_MAX_CNT] = { 0 };
	argc--;
	for (int i = 0; i < argc; i++) va[i] = shell_parse_param(shell, argv[i + 1]);
	switch (cmd->attr.attrs.arg_cnt > argc ? cmd->attr.attrs.arg_cnt : argc) {
#if SHELL_PARAM_MAX_CNT > 0
	case 0: return cmd->data.cmd.func();
#if SHELL_PARAM_MAX_CNT > 1
	case 1: return cmd->data.cmd.func(va[0]);
#if SHELL_PARAM_MAX_CNT > 2
	case 2: return cmd->data.cmd.func(va[0], va[1]);
#if SHELL_PARAM_MAX_CNT > 3
	case 3: return cmd->data.cmd.func(va[0], va[1], va[2]);
#if SHELL_PARAM_MAX_CNT > 4
	case 4: return cmd->data.cmd.func(va[0], va[1], va[2], va[3]);
#if SHELL_PARAM_MAX_CNT > 5
	case 5: return cmd->data.cmd.func(va[0], va[1], va[2], va[3], va[4]);
#if SHELL_PARAM_MAX_CNT > 6
	case 6: return cmd->data.cmd.func(va[0], va[1], va[2], va[3], va[4], va[5]);
#if SHELL_PARAM_MAX_CNT > 7
	case 7: return cmd->data.cmd.func(va[0], va[1], va[2], va[3], va[4], va[5], va[6]);
#endif// > 0
#endif// > 1
#endif// > 2
#endif// > 3
#endif// > 4
#endif// > 5
#endif// > 6
#endif// > 7
	default: return -1;
	}
}

static void shell_exec_proc(shell_t *shell)
{
	SHELL_CHECK(shell->parser.len, return);
	shell->parser.buf[shell->parser.len] = 0;
	// history process
	shell->history.his_offs = 0;
	if (shell->history.his_num <= 0 || strcmp(shell->parser.buf
		, shell->history.item[(shell->history.his_pos ? shell->history.his_pos : SHELL_HISTORY_MAX_CNT) - 1])) {
		if (strlen(strcpy(shell->history.item[shell->history.his_pos], shell->parser.buf))) shell->history.his_pos++;
		if (++shell->history.his_num > SHELL_HISTORY_MAX_CNT) shell->history.his_num = SHELL_HISTORY_MAX_CNT;
		if (shell->history.his_pos >= SHELL_HISTORY_MAX_CNT) shell->history.his_pos = 0;
	}
	// param pre-parse
	uint8_t quotes = 1;
	uint8_t his_pos = 1;
	for (short i = 0; i < SHELL_PARAM_MAX_CNT; i++) shell->parser.param[i] = NULL;
	shell->parser.arg_cnt = 0;
	for (uint16_t i = 0; i < shell->parser.len; i++)
		if (quotes && (shell->parser.buf[i] == ' ' || shell->parser.buf[i] == '\0')) {
			shell->parser.buf[i] = 0;
			his_pos = 1;
		} else {
			if (shell->parser.buf[i] == '\"') quotes = !quotes;
			if (his_pos == 1) {
				if (shell->parser.arg_cnt < SHELL_PARAM_MAX_CNT)
					shell->parser.param[shell->parser.arg_cnt++] = &(shell->parser.buf[i]);
				his_pos = 0;
			}
			if (shell->parser.buf[i] == '\\' && shell->parser.buf[i + 1]) i++;
		}
	// command seek
	shell->parser.len = shell->parser.cursor = 0;
	SHELL_CHECK(shell->parser.arg_cnt, return);
	shell_write_str(shell, CRLF);
	shell_cmd_t *cmd = shell_seek_cmd(shell, shell->parser.param[0], shell->cmdlist.base, 0);
	if (!cmd) {
		shell_write_text(shell, SHELL_TEXT_CMD_MISS);
		return;
	}
	// command execute
	int ret = 0;
	shell->status.active = 1;
	switch (cmd->attr.attrs.type) {
	case SHELL_TYPE_CMD_STRS:
		// remove the quotations around the argument
		for (uint16_t i = 0; i < shell->parser.arg_cnt; i++) {
			if (shell->parser.param[i][0] == '\"') {
				shell->parser.param[i][0] = 0;
				shell->parser.param[i] = &shell->parser.param[i][1];
			}
			uint16_t last = strlen(shell->parser.param[i]) - 1;
			if (shell->parser.param[i][last] == '\"') shell->parser.param[i][last] = 0;
		}
		// run command immediately without format converting
		ret = cmd->data.cmd.func(shell->parser.arg_cnt, shell->parser.param);
		if (cmd->attr.attrs.show_ret) shell_inner_printf(shell, "return: %#08x(%d)" CRLF, ret, ret);
		break;
	case SHELL_TYPE_CMD_ARGS:
		// convert arguments from string to actually format, then run the command
		ret = shell_ext_run(shell, cmd, shell->parser.arg_cnt, shell->parser.param);
		if (cmd->attr.attrs.show_ret) shell_inner_printf(shell, "return: %#08x(%d)" CRLF, ret, ret);
		break;
	default:
		break;
	}
	shell->status.active = 0;
}

static int shell_exec_str(const char *cmd)
{
	shell_t *shell = shell_get_inst(false);
	SHELL_CHECK(shell && cmd, return -1);
	char active = shell->status.active;
	if (strlen(cmd) > shell->parser.buf_size - 1) {
		shell_write_text(shell, SHELL_TEXT_CMD_LONG);
		return -1;
	}
	shell->parser.len = strlen(strcpy(shell->parser.buf, (char *)cmd));
	shell_exec_proc(shell);
	shell->status.active = active;
	return 0;
}

static void shell_key_up(shell_t *shell)
{
	shell_get_history(shell, 1);
}

static void shell_key_down(shell_t *shell)
{
	shell_get_history(shell, -1);
}

static void shell_key_left(shell_t *shell)
{
	SHELL_CHECK(shell->parser.cursor > 0, return);
	shell_write_chr(shell, '\b');
	shell->parser.cursor--;
}

static void shell_key_right(shell_t *shell)
{
	if (shell->parser.cursor < shell->parser.len) shell_write_chr(shell, shell->parser.buf[shell->parser.cursor++]);
}

static void shell_key_tab(shell_t *shell)
{
	uint16_t max = shell->parser.buf_size;
	uint16_t match = 0;
	uint16_t num = 0;
	if (shell->parser.len == 0) {
		uint8_t active = shell->status.active;
		shell->status.active = 1;
		shell_exec_str("help");
		shell->status.active = active;
		shell_print_prot(shell, 1);
	} else if (shell->parser.len > 0) {
		shell->parser.buf[shell->parser.len] = 0;
		shell_cmd_t *base = (shell_cmd_t *)shell->cmdlist.base;
		for (short i = 0; i < shell->cmdlist.count; i++) {
			if (shell->parser.len == shell_strcmp(shell->parser.buf, shell_get_name(base + i))) {
				if (num) {
					if (num == 1) shell_write_str(shell, CRLF);
					shell_inner_printf(shell, "%-23s\t%s" CRLF, shell_get_name(base + match), shell_get_desc(base + match));
					uint16_t len = shell_strcmp(shell_get_name(&base[match]), shell_get_name(base + i));
					max = max > len ? len : max;
				}
				match = i;
				num++;
			}
		}
		SHELL_CHECK(num, return);
		if (num == 1) shell_clr_line(shell);
		shell->parser.len = strlen(strcpy(shell->parser.buf, shell_get_name(base + match)));
		if (num > 1) {
			shell_inner_printf(shell, "%-23s\t%s" CRLF, shell_get_name(base + match), shell_get_desc(base + match));
			shell_print_prot(shell, 1);
			shell->parser.len = max;
		}
		shell->parser.buf[shell->parser.len] = 0;
		shell->parser.cursor = shell->parser.len;
		shell_write_str(shell, shell->parser.buf);
	}
}

static void shell_key_back(shell_t *shell)
{
	SHELL_CHECK(shell->parser.cursor, return);
	if (shell->parser.cursor == shell->parser.len) {
		shell->parser.cursor--;
		shell->parser.len--;
		shell->parser.buf[shell->parser.len] = 0;
		shell_write_str(shell, "\b \b");
	} else {
		for (short i = shell->parser.cursor; i < shell->parser.len; i++) shell->parser.buf[i - 1] = shell->parser.buf[i];
		shell->parser.len--;
		shell->parser.cursor--;
		shell_write_chr(shell, '\b');
		shell->parser.buf[shell->parser.len] = 0;
		shell_write_str(shell, &shell->parser.buf[shell->parser.cursor]);
		shell_write_chr(shell, ' ');
		for (short i = 0; i <= shell->parser.len - shell->parser.cursor; i++) shell_write_chr(shell, '\b');
	}
}

static void shell_key_del(shell_t *shell)
{
	SHELL_CHECK(shell->parser.cursor != shell->parser.len, return);
	for (short i = shell->parser.cursor + 1; i < shell->parser.len; i++)
		shell->parser.buf[i - 1] = shell->parser.buf[i];
	shell->parser.len--;
	shell->parser.buf[shell->parser.len] = 0;
	shell->writes(&shell->parser.buf[shell->parser.cursor], shell->parser.len - shell->parser.cursor, shell->user);
	shell_write_chr(shell, ' ');
	for (short i = 0; i <= shell->parser.len - shell->parser.cursor; i++) shell_write_chr(shell, '\b');
}

static void shell_key_enter(shell_t *shell)
{
	shell_exec_proc(shell);
	shell_print_prot(shell, 1);
}

static void shell_cmd_help(int argc, char *argv[])
{
	shell_t *shell = shell_get_inst(false);
	SHELL_CHECK(shell, return);
	if (argc == 1) {
		shell_cmd_t *base = (shell_cmd_t *)shell->cmdlist.base;
		for (short i = 0; i < shell->cmdlist.count; i++)
			if (base[i].attr.attrs.type == SHELL_TYPE_CMD_ARGS || base[i].attr.attrs.type == SHELL_TYPE_CMD_STRS)
				shell_inner_printf(shell, "%-23s\t%s" CRLF, base[i].data.cmd.name, base[i].data.cmd.desc);
	} else if (argc > 1) {
		shell_cmd_t *base = shell_seek_cmd(shell, argv[1], shell->cmdlist.base, 0);
		shell_inner_printf(shell, "%s%s" CRLF "%s" CRLF
			, shell_text_tab[SHELL_TEXT_HELP_HDR], shell_get_name(base), shell_get_desc(base));
	}
}

static void shell_cmd_clear(void)
{
	shell_t *shell = shell_get_inst(false);
	SHELL_CHECK(shell, return);
	shell_write_text(shell, SHELL_TEXT_CLR_CONSOLE);
}

static int shell_cmd_exec(int argc, char *argv[])
{
	shell_t *shell = shell_get_inst(false);
	if (!shell || argc < 2) {
		shell_write_text(shell, SHELL_TEXT_ARG_ERR);
		return -1;
	}
	shell_cmd_t cmd = {
		.attr.value = SHELL_TYPE_CMD_ARGS | SHELL_SHOW_RET,
		.data.cmd.func = (int (*)())shell_parse_param(shell, argv[1]),
	};
	return shell_ext_run(shell, &cmd, argc - 1, &argv[1]);
}

void shell_print_endl(void *pshell, const char *buf, int len)
{
	shell_t *shell = (shell_t *)pshell;
	SHELL_CHECK(shell, return);
	shell_write_text(shell, SHELL_TEXT_CLR_LINE);
	shell->writes(buf, len, shell->user);
	shell_print_prot(shell, 0);
	if (shell->parser.len > 0) {
		shell_write_str(shell, shell->parser.buf);
		for (short i = 0; i < shell->parser.len - shell->parser.cursor; i++) 
			shell_write_chr(shell, '\b');
	}
}

static void shell_dump_data(const void *base, int size)
{
	shell_t *shell = shell_get_inst(true);
	SHELL_CHECK(shell, return);
	const int len = size;
	const uint32_t ubase = (uint32_t)base;
	uint8_t *addr = (uint8_t *)(ubase & ~0xF);
	size = (size + ubase - (uint32_t)addr + 0xF) & ~0xF;
	shell_write_str(shell, COR_FG_CYAN \
		"| [ADDRESS] 00-01-02-03--04-05-06-07--08-09-0A-0B--0C-0D-0E-0F | ++++++++++++++++ |" CRLF);

	// should ensusre line length in manual, in particular, pay attention to
	// other invisible characters, such as color code
	char *const line = shell_malloc(SHELL_LINE_LIMIT);
	while (size > 0) {
		uint32_t uaddr = (uint32_t)addr;
		int lpos = snprintf(line, SHELL_LINE_LIMIT, COR_FG_CYAN "| %08X:" CORDEF, uaddr);
		for (int i = 0; i < 16; i++) {
			uaddr = (uint32_t)&addr[i];
			if (ubase > uaddr || uaddr >= ubase + len) memcpy(&line[lpos], "   ", 3);
			else snprintf(&line[lpos], SHELL_LINE_LIMIT - lpos, " %02x", addr[i]);
			lpos += 3;
			if ((i % 4) == 3) line[lpos++] = ' ';
		}
		lpos += snprintf(&line[lpos], SHELL_LINE_LIMIT - lpos, COR_FG_CYAN "| " CORDEF);
		for (int i = 0; i < 16; i++) {
			char chr = '.';
			uaddr = (uint32_t)&addr[i];
			if (uaddr < ubase || uaddr >= ubase + len) chr = ' ';
			else if (0x20 <= addr[i] && addr[i] <= 0x7E) chr = (char)addr[i];
			line[lpos++] = chr;
		}
		ASSERT(lpos < SHELL_LINE_LIMIT, "line(>%d)", lpos);
		lpos += snprintf(&line[lpos], SHELL_LINE_LIMIT - lpos, COR_FG_CYAN " |" CRLF);
		line[lpos] = 0;
	
		// cannot calls as shell_inner_printf(shell, line), as '%' may exist!
		// shell_inner_printf(shell, "%s", line);
		shell_write_str(shell, line);
	
		addr += 16;
		size -= 16;
	}
	shell_free(line);
	shell_write_str(shell, COR_FG_CYAN \ 
		"| ++++++++: 00-01-02-03--04-05-06-07--08-09-0A-0B--0C-0D-0E-0F | ++++++++++++++++ |" CORDEF);
}

static void shell_cmd_dump(const void *base, int size)
{
	shell_t *shell = shell_get_inst(true);
	SHELL_CHECK(shell && size, return);
	shell_inner_printf(shell, "memory dump, start: %p, size: %d:" CRLF, base, size);
	shell_dump_data(base, size);
}

void shell_dbg_dump(const void *addr, int size)
{
	shell_t *shell = shell_get_inst(true);
	SHELL_CHECK(shell && size, return);
	shell_write_text(shell, SHELL_TEXT_CLR_LINE);
	shell_dump_data(addr, size);
	shell_print_prot(shell, 1);
}

void shell_recv_proc(void *pshell, char data)
{
	shell_t *shell = (shell_t *)pshell;
	SHELL_CHECK(data, return);
	char kpos = 24;
	int kmsk = 0;
	if (shell->parser.key_val & 0x0000FF00) {
		kpos = 0;
		kmsk = 0xFFFFFF00;
	} else if (shell->parser.key_val & 0x00FF0000) {
		kpos = 8;
		kmsk = 0xFFFF0000;
	} else if (shell->parser.key_val & 0xFF000000) {
		kpos = 16;
		kmsk = 0xFF000000;
	}
	shell_cmd_t *cmd = (shell_cmd_t *)shell->cmdlist.base;
	for (short i = 0; i < shell->cmdlist.count; i++, cmd++)
		if (cmd->attr.attrs.type == SHELL_TYPE_KEY && (cmd->data.key.value & kmsk) == shell->parser.key_val
				&& (cmd->data.key.value & (0xFF << kpos)) == (data << kpos)) {
			shell->parser.key_val |= data << kpos;
			data = 0x00;
			if (!kpos || !(cmd->data.key.value & (0xFF << (kpos - 8)))) {
				if (cmd->data.key.func) cmd->data.key.func(shell);
				shell->parser.key_val = 0;
				break;
			}
		}
	if (data) {
		shell->parser.key_val = 0;
		if (shell->parser.len >= shell->parser.buf_size - 1) {
			shell_write_text(shell, SHELL_TEXT_CMD_LONG);
			shell_print_prot(shell, 1);
			shell_write_str(shell, shell->parser.buf);
		} else if (shell->parser.cursor == shell->parser.len) {
			shell->parser.buf[shell->parser.len++] = data;
			shell->parser.buf[shell->parser.len] = 0;
			shell->parser.cursor++;
			shell_write_chr(shell, data);
		} else if (shell->parser.cursor < shell->parser.len) {
			for (short i = shell->parser.len - shell->parser.cursor; i > 0; i--)
				shell->parser.buf[shell->parser.cursor + i] = shell->parser.buf[shell->parser.cursor + i - 1];
			shell->parser.buf[shell->parser.cursor++] = data;
			shell->parser.buf[++shell->parser.len] = 0;
			shell_write_str(shell, &shell->parser.buf[shell->parser.cursor - 1]);
			for (short i = shell->parser.len - shell->parser.cursor; i > 0; i--) shell_write_chr(shell, '\b');
		}
	}
}

void *shell_init(shell_writes_t writes, void *const base, void *const user)
{
	shell_t *shell = (shell_t *)shell_malloc(sizeof(shell_t));
	memset(shell, 0, sizeof(shell_t));
	shell->user = user;
	shell->writes = writes;
	shell->parser.buf = &shell->buff[0];
	shell->parser.buf_size = sizeof(shell->buff) / (SHELL_HISTORY_MAX_CNT + 1);
	for (short i = 0; i < SHELL_HISTORY_MAX_CNT; i++)
		shell->history.item[i] = &shell->buff[shell->parser.buf_size * (i + 1)];
	
	shell->cmdlist.count = sizeof(shell_item_tab) / sizeof(shell_cmd_t)	// priv
		+ ((shell_item_t *)&shell_item_tab[0] - (shell_item_t *)base);	// user
	shell->cmdlist.base = base;

	// shell_write_text(shell, SHELL_TEXT_CLR_CONSOLE);
	shell_write_text(shell, SHELL_TEXT_LOGO);
	shell_print_prot(shell, 1);
	return (shell_inst = shell);
}

void shell_uninit(void)
{
	if (shell_inst) {
		shell_free(shell_inst);
		shell_inst = NULL;
	}
}
