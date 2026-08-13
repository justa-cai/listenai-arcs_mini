#ifndef __MRPC_FLASH_IF_API_MSG_H__
#define __MRPC_FLASH_IF_API_MSG_H__
#include <stdio.h>
#include <stdint.h>
#include "ls_err.h"
#include "ls_wifi_type.h"
#include "mrpc.h"

typedef struct
{
    struct mrpc_req_msg hdr;
    FLASH_DEV dev;
    unsigned char ud0;
    unsigned char ud1;
} mrpc_flash_if_init_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_flash_if_init_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    int32_t offset;
    void * data;
    size_t len;
} mrpc_flash_if_read_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_flash_if_read_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    int32_t offset;
    void * data;
    size_t len;
} mrpc_flash_if_write_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_flash_if_write_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    int32_t offset;
    size_t size;
} mrpc_flash_if_erase_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_flash_if_erase_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    bool enable;
} mrpc_flash_if_write_protection_set_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_flash_if_write_protection_set_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    off_t offset;
    void * data;
    size_t len;
} mrpc_flash_if_security_read_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_flash_if_security_read_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    off_t offset;
    void * data;
    size_t len;
} mrpc_flash_if_security_write_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_flash_if_security_write_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
    off_t offset;
} mrpc_flash_if_security_erase_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_flash_if_security_erase_resp_t;

typedef struct
{
    struct mrpc_req_msg hdr;
} mrpc_flash_if_check_security_support_req_t;

typedef struct
{
    struct mrpc_resp_msg hdr;
} mrpc_flash_if_check_security_support_resp_t;

#endif //__MRPC_FLASH_IF_API_MSG_H__