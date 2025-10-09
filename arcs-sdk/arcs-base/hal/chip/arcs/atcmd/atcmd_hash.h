// Copyright 2024-2025 ListenAI
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef _ATCMD_HASH_H_
#define _ATCMD_HASH_H_

#include "atcmd.h"

void atcmd_entry_add_table(const atcmd_item_t *item_table, int len);
void* atcmd_item_action(char *cmd);
void atcmd_hash_init(void);


#endif //_ATCMD_HASH_H_
