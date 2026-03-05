#ifndef __LISA_SHELL_CMD_H__
#define __LISA_SHELL_CMD_H__

struct listen_cmd_t {
    char *name;
    int (*exec)(int argc, char **argv);
    char *help;
};

#endif
