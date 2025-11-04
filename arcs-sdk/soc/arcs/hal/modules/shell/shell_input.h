#ifndef _SHELL_INPUT_H_
#define _SHELL_INPUT_H_

extern void shell_print_prompt(struct shell_env* shell);
extern int32_t shell_write_string(struct shell_env *shell, const char *string);
extern int32_t shell_parse(struct shell_env* shell, int8_t key);
void shell_print_time(struct shell_env* shell);


#endif
