#ifndef __SHELL_PRIVATE_HEADER__
#define __SHELL_PRIVATE_HEADER__

#include "shell.h"

#define __priv __attribute__((section(".shell.priv"), used, aligned(4)))
#define __user __attribute__((section(".shell.user"), used, aligned(4)))

#define	SHELL_CHECK(expr, action)	if (!(expr)) { action; }
#define	SHELL_PARAM_MAX_CNT			(CONFIG_SHELL_PARAM_MAX)
#define	SHELL_HISTORY_MAX_CNT		(CONFIG_SHELL_HISTORY_MAX)
#define	SHELL_USER_NAME				(CONFIG_SHELL_USER_NAME)
#define SHELL_BUF_SIZE				(CONFIG_SHELL_TXBUF_SIZE)
#define SHELL_LINE_LIMIT			(CONFIG_SHELL_LINE_LIMIT)

#define SHELL_CMD_ITEM(_attr, _name, _func, _desc)	{							\
	.attr.value = (_attr),	 													\
	.data.cmd.name = _name,														\
	.data.cmd.func = (int (*)())_func,											\
	.data.cmd.desc = _desc														\
}

#define SHELL_KEY_ITEM(_attr, _value, _func, _desc)	{							\
	.attr.value = (_attr) | SHELL_TYPE_KEY,										\
	.data.key.value = _value,													\
	.data.key.func = (void (*)(shell_t *))_func,								\
	.data.key.desc = _desc														\
}

typedef enum {
	SHELL_TYPE_CMD_ARGS = 0,// chr/str/dec/hex/oct/..., like func(int a, ...)
	SHELL_TYPE_CMD_STRS,	// string format, like main(int argc, char **argv)
	SHELL_TYPE_KEY,			// key
} shell_cmd_type_t;

// refer to shell_cmd_t.attr.attrs
#define SHELL_TYPE_MASK				BITS(1, 0)
#define	SHELL_SHOW_RET				BIT(2)
#define	SHELL_NON_AUTH				BIT(3)
#define SHELL_ARGS_MASK				BIT(7, 4)

typedef struct shell shell_t;
typedef struct {
	union {
		struct {
			const char *name;
			int (*func)();
			const char *desc;
		} cmd;
		struct {
			const char *name;
			const char *desc;
		} usr;
		struct {
			int value;
			void (*func)(shell_t *);
			const char *desc;
		} key;
	} data;
	union {
		struct {
			uint8_t type : 2;
			uint8_t show_ret : 1;
			uint8_t skip_auth : 1;
			uint8_t arg_cnt : 4;
			uint32_t : 24;
		} attrs;
		uint32_t value;
	} attr;
} shell_cmd_t;

typedef struct shell {
	const shell_cmd_t *usr;
	struct {
		uint16_t len;
		uint16_t cursor;
		char *buf;			// input buffer
		char *param[SHELL_PARAM_MAX_CNT];
		uint16_t buf_size;	// input buffer size
		uint16_t arg_cnt;	// param count
		int key_val;
	} parser;
	struct {
		char *item[SHELL_HISTORY_MAX_CNT];
		uint16_t his_num;
		uint16_t his_pos;
		short his_offs;
		uint16_t : 16;
	} history;
	struct {
		void *base;
		uint16_t count;
		uint16_t : 16;
	} cmdlist;
	struct {
		uint8_t active : 1;
		uint32_t : 30;
	} status;
	void *user;
	shell_writes_t writes;
	char buff[SHELL_BUF_SIZE];
} shell_t;

enum shel_text_t {
	SHELL_TEXT_LOGO,
	SHELL_TEXT_CMD_LONG,
	SHELL_TEXT_CMD_MISS,
	SHELL_TEXT_HELP_HDR,
	SHELL_TEXT_ARG_ERR,
	SHELL_TEXT_CLR_CONSOLE,
	SHELL_TEXT_CLR_LINE,
	SHELL_TEXT_BACK_SPACE,
	SHELL_TEXT_MEM_FAIL,
};

static void shell_key_up(shell_t *shell);
static void shell_key_down(shell_t *shell);
static void shell_key_right(shell_t *shell);
static void shell_key_left(shell_t *shell);
static void shell_key_tab(shell_t *shell);
static void shell_key_back(shell_t *shell);
static void shell_key_del(shell_t *shell);
static void shell_key_enter(shell_t *shell);
static void shell_cmd_clear(void);
static int  shell_cmd_exec(int argc, char *argv[]);
static void shell_cmd_help(int argc, char *argv[]);
static void shell_cmd_dump(const void *base, int size);

#endif//__SHELL_PRIVATE_HEADER__
