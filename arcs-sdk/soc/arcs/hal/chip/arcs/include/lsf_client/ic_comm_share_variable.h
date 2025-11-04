#ifndef IC_COMM_SHARE_VARIABLE_H
#define IC_COMM_SHARE_VARIABLE_H

//------------------------------------------------
// lib: clib

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

//------------------------------------------------

typedef struct _IC_Comm_ShareVariable IC_Comm_ShareVariable;

struct _IC_Comm_ShareVariable
{
    uint32_t *mShareVar;
};

//------------------------------------------------
// class methods

// char a[32] alignment 32;
void IC_Comm_ShareVariable_ctor(IC_Comm_ShareVariable* self, uint32_t* shareVar);

void IC_Comm_ShareVariable_dtor(IC_Comm_ShareVariable *self);

int IC_Comm_ShareVariable_signal(IC_Comm_ShareVariable* self, uint32_t value);

int IC_Comm_ShareVariable_wait(IC_Comm_ShareVariable* self, uint32_t value);

#endif // IC_COMM_SHARE_VARIABLE_H
