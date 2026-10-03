/*
 * Copyright PeakRacing
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nes.h"

uint16_t nes_prgrom_8k_count(nes_t* nes)
{
    return (uint16_t)(nes->nes_rom.prg_rom_size * 2U);
}

uint16_t nes_chrrom_1k_count(nes_t* nes)
{
    return (uint16_t)(nes->nes_rom.chr_rom_size * 8U);
}

static uint16_t nes_wrap_bank(uint16_t bank, uint16_t count)
{
    return count ? (uint16_t)(bank % count) : 0U;
}

/* load 8k PRG-ROM */
void nes_load_prgrom_8k(nes_t* nes,uint8_t des, uint16_t src) {
    const uint16_t count = nes_prgrom_8k_count(nes);
    if (des >= 4U || count == 0U || nes->nes_rom.prg_rom == NULL) {
        if (des < 4U) {
            nes->nes_cpu.prg_banks[des] = NULL;
        }
        return;
    }
    src = nes_wrap_bank(src, count);
    nes->nes_cpu.prg_banks[des] = nes->nes_rom.prg_rom + 8 * 1024 * src;
}

void nes_load_prgrom_8k_wrap(nes_t* nes, uint8_t des, uint16_t src)
{
    nes_load_prgrom_8k(nes, des, src);
}

void nes_load_chrrom_1k_wrap(nes_t* nes, uint8_t des, uint16_t src)
{
    nes_load_chrrom_1k(nes, des, src);
}

/* load 16k PRG-ROM */
void nes_load_prgrom_16k(nes_t* nes,uint8_t des, uint16_t src) {
    nes_load_prgrom_8k(nes, (uint8_t)(des * 2U), (uint16_t)(src * 2U));
    nes_load_prgrom_8k(nes, (uint8_t)(des * 2U + 1U), (uint16_t)(src * 2U + 1U));
}

/* load 32k PRG-ROM */
void nes_load_prgrom_32k(nes_t* nes,uint8_t des, uint16_t src) {
    (void)des;
    nes_load_prgrom_8k(nes, 0, (uint16_t)(src * 4U));
    nes_load_prgrom_8k(nes, 1, (uint16_t)(src * 4U + 1U));
    nes_load_prgrom_8k(nes, 2, (uint16_t)(src * 4U + 2U));
    nes_load_prgrom_8k(nes, 3, (uint16_t)(src * 4U + 3U));
}

/* load 1k CHR-ROM */
void nes_load_chrrom_1k(nes_t* nes,uint8_t des, uint16_t src) {
    if (des >= 8U || nes->nes_rom.chr_rom == NULL) {
        if (des < 8U) {
            nes->nes_ppu.pattern_table[des] = NULL;
        }
        return;
    }
    /* CHR-RAM 卡带 (header chr_size==0): 卡上是固定在 $0000-$1FFF 的 8KB RAM,
     * CHR bank 寄存器被忽略, 始终恒等映射; 缓冲由 nes_load_rom/file 按 8KB 分配。
     * 不能再走下面 count==0 的分支 —— 那会把 pattern_table 清成 NULL, 游戏用
     * $2007 上传图案数据时会写空指针 (Store/AMO access fault, MTVAL=0)。
     * 未处理时 mapper0/1/3/7 等 CHR-RAM 卡带一进游戏就崩。 */
    if (nes->nes_rom.chr_rom_size == 0U) {
        nes->nes_ppu.pattern_table[des] = nes->nes_rom.chr_rom + 1024U * des;
        return;
    }
    const uint16_t count = nes_chrrom_1k_count(nes);
    if (count == 0U) {
        nes->nes_ppu.pattern_table[des] = NULL;
        return;
    }
    src = nes_wrap_bank(src, count);
    nes->nes_ppu.pattern_table[des] = nes->nes_rom.chr_rom + 1024 * src;
}

/* load 4k CHR-ROM */
void nes_load_chrrom_4k(nes_t* nes,uint8_t des, uint16_t src) {
    for (size_t i = 0; i < 4; i++){
        nes_load_chrrom_1k(nes, (uint8_t)(des * 4U + i), (uint16_t)(src * 4U + i));
    }
}

/* load 8k CHR-ROM */
void nes_load_chrrom_8k(nes_t* nes,uint8_t des, uint16_t src) {
    for (size_t i = 0; i < 8; i++){
        nes_load_chrrom_1k(nes, (uint8_t)(des + i), (uint16_t)(src * 8U + i));
    }
}

#ifndef CONFIG_NES_MAPPER_INFONES_ENABLE
#define CONFIG_NES_MAPPER_INFONES_ENABLE 1
#endif

int nes_mapper0_init(nes_t* nes);
int nes_mapper1_init(nes_t* nes);
int nes_mapper2_init(nes_t* nes);
int nes_mapper3_init(nes_t* nes);
int nes_mapper7_init(nes_t* nes);
int nes_mapper94_init(nes_t* nes);
int nes_mapper117_init(nes_t* nes);
int nes_mapper180_init(nes_t* nes);

#define NES_CASE_LOAD_NATIVE_MAPPER(mapper_id) case mapper_id: return nes_mapper##mapper_id##_init(nes)

int nes_load_mapper(nes_t* nes){
    nes_memset(&nes->nes_mapper, 0, sizeof(nes->nes_mapper));
#if CONFIG_NES_MAPPER_INFONES_ENABLE
    if (nes_mapper_infones_init(nes) == NES_OK) {
        return NES_OK;
    }
#endif
    switch (nes->nes_rom.mapper_number) {
        NES_CASE_LOAD_NATIVE_MAPPER(0);
        NES_CASE_LOAD_NATIVE_MAPPER(1);
        NES_CASE_LOAD_NATIVE_MAPPER(2);
        NES_CASE_LOAD_NATIVE_MAPPER(3);
        NES_CASE_LOAD_NATIVE_MAPPER(7);
        NES_CASE_LOAD_NATIVE_MAPPER(94);
        NES_CASE_LOAD_NATIVE_MAPPER(117);
        NES_CASE_LOAD_NATIVE_MAPPER(180);
        default:
            break;
    }
    NES_LOG_ERROR("mapper:%03d is unsupported\n",nes->nes_rom.mapper_number);
    return NES_ERROR;
}
