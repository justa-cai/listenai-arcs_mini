/*
 * Copyright (c) 2026 Anhui Listenai Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lnn_resnet18_real_ipc.h"

volatile lnn_resnet18_real_ipc_t lnn_resnet18_real_ipc_block
    __attribute__((section(".ipc.lnn"), aligned(64)));

_Static_assert((sizeof(lnn_resnet18_real_ipc_t) % 64U) == 0U,
               "LNN IPC shared block must stay cache-line aligned");
_Static_assert(sizeof(lnn_resnet18_real_msg_t) == 16U,
               "LNN mailbox message must fit in four mailbox words");
