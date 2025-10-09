#include "shell_def.h"
#include "string.h"
#include "sw_version.h"

#define SHELL_TEXT_CMD_TOO_LONG "\r\nWarning: Command is too long\r\n"
static void shell_left(struct shell_env *shell);
static void shell_right(struct shell_env *shell);
static void shell_backspace(struct shell_env *shell);
static void shell_delete(struct shell_env *shell);
static void shell_enter(struct shell_env *shell);
static void shell_up(struct shell_env *shell);
static void shell_down(struct shell_env *shell);

static uint8_t g_enable_shell_echo = 1;

struct command_key{
    uint32_t key_value;
    void  (*func)(struct shell_env*);
};

const struct command_key key_tab [] = {
    {SHELL_KEY_RIGHT, shell_right},
    {SHELL_KEY_LEFT, shell_left},
    {SHELL_KEY_DELETE, shell_delete},
    {SHELL_KEY_BACKSPACE, shell_backspace},
    {SHELL_KEY_LF, shell_enter},
    {SHELL_KEY_CR, shell_enter},
    {SHELL_KEY_UP, shell_up},
    {SHELL_KEY_DOWN, shell_down},
    {SHELL_KEY_INVALID, NULL},
};

int32_t shell_cmd_key(struct shell_env* shell, uint8_t key)
{
    int32_t ret = 0;
    uint32_t key_byte_offset = 24;
    uint32_t key_filter = 0x00000000;
    const struct command_key *cmd;

    if ((shell->parser.key_value & 0x0000FF00) != 0x00000000)
    {
        key_byte_offset = 0;
        key_filter = 0xFFFFFF00;
    }
    else if ((shell->parser.key_value & 0x00FF0000) != 0x00000000)
    {
        key_byte_offset = 8;
        key_filter = 0xFFFF0000;
    }
    else if ((shell->parser.key_value & 0xFF000000) != 0x00000000)
    {
        key_byte_offset = 16;
        key_filter = 0xFF000000;
    }

    for (cmd = key_tab; cmd->key_value != SHELL_KEY_INVALID; cmd++)
    {
        if ( ((cmd->key_value & key_filter) == shell->parser.key_value)
             && ((cmd->key_value & (0xFF << key_byte_offset)) == (key << key_byte_offset))
           )
        {
            ret = 1;
            shell->parser.key_value |= key << key_byte_offset;
            if ((key_byte_offset == 0)
                || ((cmd->key_value & (0xFF << (key_byte_offset - 8))) == 0x00000000)
               )
            {
                  if (cmd->func)
                      cmd->func(shell);
                  shell->parser.key_value = 0x00000000;
                  break;
            }
        }
    }

    return ret;
}

int32_t shell_write_string(struct shell_env *shell, const char *string)
{ 
    uint32_t count = 0;
    const char *p = string;

    while(*p++)
        count++;

    return shell->output((uint8_t*)string, count);
}

void set_shell_echo(uint8_t enable)
{
    g_enable_shell_echo = enable;
}

void shell_print_prompt(struct shell_env* shell)
{
    if(g_enable_shell_echo) {
        shell_write_string(shell, SHELL_ENTER_STRING);
        shell_write_string(shell, SHELL_PROMPT_STRING);
    }
}

void shell_print_time(struct shell_env* shell)
{
    shell_write_string(shell, SHELL_ENTER_STRING);
    shell_write_string(shell, FHOST_VERSION);
}

static void shell_write_byte(struct shell_env *shell, uint8_t data)
{
    if(g_enable_shell_echo) {
        shell->output(&data, 1);
    }
}

static void shell_get_byte(struct shell_env *shell, uint8_t data)
{
    int32_t i;

    if (shell->parser.length >= shell->parser.buffer_size - 1)
    {
        shell_write_string(shell, SHELL_TEXT_CMD_TOO_LONG);
        shell_print_prompt(shell);
        shell_write_string(shell, shell->parser.buffer);
        return;
    }

    if (shell->parser.cursor == shell->parser.length)
    {
        shell->parser.buffer[shell->parser.length++] = data;
        shell->parser.buffer[shell->parser.length] = 0;
        shell->parser.cursor++;
        shell_write_byte(shell, data);
    }
    else if (shell->parser.cursor < shell->parser.length)
    {
        for (i = shell->parser.length - shell->parser.cursor; i > 0; i--)
        {
            shell->parser.buffer[shell->parser.cursor + i] = 
                shell->parser.buffer[shell->parser.cursor + i - 1];
        }
        shell->parser.buffer[shell->parser.cursor++] = data;
        shell->parser.buffer[++shell->parser.length] = 0;
        for (i = shell->parser.cursor - 1; i < shell->parser.length; i++)
        {
            shell_write_byte(shell, shell->parser.buffer[i]);
        }
        for (i = shell->parser.length - shell->parser.cursor; i > 0; i--)
        {
            shell_write_byte(shell, '\b');
        }
    }
}

static void shell_delete_command(struct shell_env *shell, int32_t length)
{
    while (length--)
        shell_write_string(shell, "\b \b");
}
#if SHELL_HISTORY_NUMBER > 0
static void shell_clear_command(struct shell_env *shell)
{
	int i;

    for (i = shell->parser.length - shell->parser.cursor; i > 0; i--)
    {
    	shell_write_byte(shell, ' ');
    }
    shell_delete_command(shell, shell->parser.length);
}

static uint32_t shell_string_copy(char *dest, char* src)
{
	uint32_t count = 0;

    while (*(src + count))
    {
        *(dest + count) = *(src + count);
        count++;
    }
    *(dest + count) = 0;

    return count;
}

static void shell_history_add(struct shell_env *shell)
{
    shell->history.offset = 0;
    if (shell->history.number > 0
        && strcmp(shell->history.item[(shell->history.record == 0 ?
        		SHELL_HISTORY_NUMBER : shell->history.record) - 1],
                shell->parser.buffer) == 0)
    {
        return;
    }
    if (shell_string_copy(shell->history.item[shell->history.record],
                        shell->parser.buffer) != 0)
    {
        shell->history.record++;
    }
    if (++shell->history.number > SHELL_HISTORY_NUMBER)
    {
        shell->history.number = SHELL_HISTORY_NUMBER;
    }
    if (shell->history.record >= SHELL_HISTORY_NUMBER)
    {
        shell->history.record = 0;
    }
}

static void shell_history(struct shell_env *shell, int32_t dir)
{
    if (dir > 0)
    {
        if (shell->history.offset-- <=
            -((shell->history.number > shell->history.record) ?
                shell->history.number : shell->history.record))
        {
            shell->history.offset = -((shell->history.number > shell->history.record)
                                    ? shell->history.number : shell->history.record);
        }
    }
    else if (dir < 0)
    {
        if (++shell->history.offset > 0)
        {
            shell->history.offset = 0;
            return;
        }
    }
    else
    {
        return;
    }
    shell_clear_command(shell);

    if (shell->history.offset == 0)
    {
        shell->parser.cursor = shell->parser.length = 0;
    }
    else
    {
        if ((shell->parser.length = shell_string_copy(shell->parser.buffer,
                shell->history.item[(shell->history.record + SHELL_HISTORY_NUMBER
                    + shell->history.offset) % SHELL_HISTORY_NUMBER])) == 0)
        {
            return;
        }
        shell->parser.cursor = shell->parser.length;
        shell_write_string(shell, shell->parser.buffer);
    }

}

static void shell_up(struct shell_env *shell)
{
    shell_history(shell, 1);
}

static void shell_down(struct shell_env *shell)
{
    shell_history(shell, -1);
}
#endif

static int32_t shell_handler(struct shell_env* shell)
{
    char *ptr;
    int32_t ret = 0;
    int32_t i, len;

    ptr = shell->parser.buffer;
    len = shell->parser.length;
    while (*ptr == ' ')
    {
        ptr++;
        len--;
    }

    if (shell->process)
        ret = shell->process(ptr, len+1, shell->output);

    return ret;
}

static void shell_exec(struct shell_env *shell)
{
#if SHELL_HISTORY_NUMBER > 0
    shell_history_add(shell);
#endif
    shell_handler(shell);
}

static void shell_delete_byte(struct shell_env *shell, int32_t direction)
{
    int32_t i, offset;

    if (((shell->parser.cursor == 0) && (direction == SHELL_DEL_DIR_LEFT))
        || ((shell->parser.cursor == shell->parser.length) && (direction == SHELL_DEL_DIR_RIGHT)))
    {
        return;
    }

    if ((shell->parser.cursor == shell->parser.length) && (direction == SHELL_DEL_DIR_LEFT))
    {
        shell->parser.cursor--;
        shell->parser.length--;
        shell->parser.buffer[shell->parser.length] = 0;
        shell_delete_command(shell, 1);
    }
    else
    {
        offset = (direction == SHELL_DEL_DIR_RIGHT) ? 1 : 0;
        for (i = offset; i < shell->parser.length - shell->parser.cursor; i++)
        {
            shell->parser.buffer[shell->parser.cursor + i - 1] = 
                shell->parser.buffer[shell->parser.cursor + i];
        }
        shell->parser.length--;
        if (!offset)
        {
            shell->parser.cursor--;
            shell_write_byte(shell, '\b');
        }
        shell->parser.buffer[shell->parser.length] = 0;

        for (i = shell->parser.cursor; i < shell->parser.length; i++)
        {
            shell_write_byte(shell, shell->parser.buffer[i]);
        }
        shell_write_byte(shell, ' ');
        for (i = shell->parser.length - shell->parser.cursor + 1; i > 0; i--)
        {
            shell_write_byte(shell, '\b');
        }
    }
}

static void shell_left(struct shell_env *shell)
{
    if (shell->parser.cursor > 0)
    {
        shell_write_byte(shell, '\b');
        shell->parser.cursor--;
    }
}

static void shell_right(struct shell_env *shell)
{
    if (shell->parser.cursor < shell->parser.length)
    {
        shell_write_byte(shell, shell->parser.buffer[shell->parser.cursor++]);
    }
}

static void shell_backspace(struct shell_env *shell)
{
    shell_delete_byte(shell, SHELL_DEL_DIR_LEFT);
}

static void shell_delete(struct shell_env *shell)
{
    shell_delete_byte(shell, SHELL_DEL_DIR_RIGHT);
}

static void shell_enter(struct shell_env *shell)
{
    if (shell->parser.length)
    {
        shell->parser.buffer[shell->parser.length] = '\0';
        shell_write_string(shell, SHELL_ENTER_STRING);
        shell_exec(shell);
        shell->parser.length = 0;
        shell->parser.cursor = 0;
    }
    shell_print_prompt(shell);
}

static void shell_get_char(struct shell_env *shell, uint8_t data)
{
    shell_get_byte(shell, data);
}

int32_t shell_parse(struct shell_env* shell, uint8_t key)
{
    if (0 == shell_cmd_key(shell, key))
        shell_get_char(shell, key);

    return 0;
}
