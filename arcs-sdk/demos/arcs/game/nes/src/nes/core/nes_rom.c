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

/* SRAM 紧张的平台可外部定义为 0, 走 nes_malloc 动态分配 (PSRAM) */
#ifndef NES_STATIC_CHR_PACKED_ROW_CAPACITY
#define NES_STATIC_CHR_PACKED_ROW_CAPACITY 4096U
#endif
#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
static uint16_t g_nes_chr_packed_rows[NES_STATIC_CHR_PACKED_ROW_CAPACITY];
static const uint8_t* g_nes_chr_packed_bank_base[8];
static const uint16_t* g_nes_chr_packed_bank_rows[8];
#endif

#ifndef CONFIG_NES_CHR_PACKED_DYNAMIC_MAX_BYTES
#define CONFIG_NES_CHR_PACKED_DYNAMIC_MAX_BYTES (0)
#endif

void nes_chr_packed_cache_free(nes_t* nes)
{
    if (nes && nes->nes_chr_packed_rows) {
        if (!nes->nes_chr_packed_rows_static) {
            nes_free(nes->nes_chr_packed_rows);
        }
        nes->nes_chr_packed_rows = NULL;
        nes->nes_chr_packed_row_count = 0;
        nes->nes_chr_packed_rows_static = 0;
    }
#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
    nes_memset(g_nes_chr_packed_bank_base, 0, sizeof(g_nes_chr_packed_bank_base));
    nes_memset(g_nes_chr_packed_bank_rows, 0, sizeof(g_nes_chr_packed_bank_rows));
#endif
}

void nes_chr_packed_cache_build(nes_t* nes)
{
    nes_chr_packed_cache_free(nes);
    if (!nes || !nes->nes_rom.chr_rom || nes->nes_rom.chr_rom_size == 0) {
        return;
    }
    const uint32_t chr_bytes = (uint32_t)nes->nes_rom.chr_rom_size * CHR_ROM_UNIT_SIZE;
    const uint32_t row_count = chr_bytes / 2U;
    uint16_t* rows = NULL;
#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
    if (row_count <= NES_STATIC_CHR_PACKED_ROW_CAPACITY) {
        rows = g_nes_chr_packed_rows;
        nes->nes_chr_packed_rows_static = 1;
    }
#endif
#if CONFIG_NES_CHR_PACKED_DYNAMIC_MAX_BYTES > 0
    if (!rows) {
        const uint32_t packed_bytes = row_count * (uint32_t)sizeof(uint16_t);
        if (packed_bytes <= CONFIG_NES_CHR_PACKED_DYNAMIC_MAX_BYTES) {
            rows = (uint16_t*)nes_try_malloc((int)packed_bytes);
            nes->nes_chr_packed_rows_static = 0;
            if (!rows) {
                NES_LOG_INFO("NES: CHR packed cache optional alloc failed, bytes=%lu\n",
                             (unsigned long)packed_bytes);
            }
        }
    }
#endif
    if (!rows) {
        NES_LOG_INFO("NES: CHR packed cache skipped, bytes=%lu rows=%lu static-cap=%u mapper-bank-cache=%u\n",
                     (unsigned long)(row_count * sizeof(uint16_t)),
                     (unsigned long)row_count,
                     (unsigned int)NES_STATIC_CHR_PACKED_ROW_CAPACITY,
                     (unsigned int)(NES_STATIC_CHR_PACKED_ROW_CAPACITY * (uint32_t)sizeof(uint16_t)));
        return;
    }
    for (uint32_t tile = 0; tile < chr_bytes / 16U; tile++) {
        const uint8_t* tile_base = nes->nes_rom.chr_rom + (tile << 4);
        uint16_t* row_base = rows + (tile << 3);
        for (uint8_t row = 0; row < 8; row++) {
            const uint8_t bit0 = tile_base[row];
            const uint8_t bit1 = tile_base[row + 8U];
            row_base[row] = ((uint16_t)(bit1 & 0xAA) << 8) |
                            ((uint16_t)(bit1 & 0x55) << 1) |
                            ((uint16_t)(bit0 & 0xAA) << 7) |
                            (bit0 & 0x55);
        }
    }
    nes->nes_chr_packed_rows = rows;
    nes->nes_chr_packed_row_count = row_count;
    NES_LOG_INFO("NES: CHR packed cache ready, bytes=%lu rows=%lu%s\n",
                 (unsigned long)(row_count * sizeof(uint16_t)),
                 (unsigned long)row_count,
                 nes->nes_chr_packed_rows_static ? " static" : "");
}

#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
static inline uint16_t nes_chr_pack_pattern_row(uint8_t bit0, uint8_t bit1)
{
    return ((uint16_t)(bit1 & 0xAA) << 8) |
           ((uint16_t)(bit1 & 0x55) << 1) |
           ((uint16_t)(bit0 & 0xAA) << 7) |
           (bit0 & 0x55);
}

static inline const uint16_t* nes_chr_packed_full_bank(nes_t* nes, const uint8_t* bank_base)
{
    if (!nes || !nes->nes_chr_packed_rows || !nes->nes_rom.chr_rom || !bank_base) {
        return NULL;
    }

    const uintptr_t chr_start = (uintptr_t)nes->nes_rom.chr_rom;
    const uintptr_t chr_end = chr_start + (uintptr_t)nes->nes_rom.chr_rom_size * CHR_ROM_UNIT_SIZE;
    const uintptr_t base = (uintptr_t)bank_base;
    if (base < chr_start || base + 0x400U > chr_end) {
        return NULL;
    }

    const uint32_t chr_offset = (uint32_t)(base - chr_start);
    const uint32_t packed_index = (chr_offset >> 4) << 3;
    if (packed_index + 512U > nes->nes_chr_packed_row_count) {
        return NULL;
    }

    return nes->nes_chr_packed_rows + packed_index;
}

const uint16_t* nes_chr_packed_mapper_bank(nes_t* nes, uint8_t bank)
{
    if (!nes || bank >= 8U) {
        return NULL;
    }

    /* 无 packed 缓存时必须直接返回: 设备端把静态容量设为 0 (省 SRAM) 且关闭动态
     * 分配时, nes_chr_packed_cache_build 会留空缓存, 而 g_nes_chr_packed_rows 是
     * 0 长数组。若继续走到下面的写入, 会向数组外写 512*2 字节 -> 破坏相邻内存
     * (MMC1 等会切 CHR bank 的 mapper 触发; UxROM 不触发, 故此前未暴露)。 */
    if (!nes->nes_chr_packed_rows || nes->nes_chr_packed_row_count == 0U) {
        return NULL;
    }

    const uint8_t* bank_base = nes->nes_ppu.pattern_table[bank];
    const uint16_t* full_bank = nes_chr_packed_full_bank(nes, bank_base);
    if (full_bank) {
        g_nes_chr_packed_bank_base[bank] = bank_base;
        g_nes_chr_packed_bank_rows[bank] = full_bank;
        return full_bank;
    }

    const uintptr_t chr_start = (uintptr_t)nes->nes_rom.chr_rom;
    const uintptr_t chr_end = chr_start + (uintptr_t)nes->nes_rom.chr_rom_size * CHR_ROM_UNIT_SIZE;
    const uintptr_t base = (uintptr_t)bank_base;
    if (!nes->nes_rom.chr_rom || base < chr_start || base + 0x400U > chr_end) {
        return NULL;
    }

    if (g_nes_chr_packed_bank_base[bank] != bank_base) {
        uint16_t* packed_bank = g_nes_chr_packed_rows + ((uint16_t)bank << 9);
        for (uint16_t tile = 0; tile < 64U; tile++) {
            const uint8_t* tile_base = bank_base + ((uint16_t)tile << 4);
            for (uint8_t row = 0; row < 8U; row++) {
                packed_bank[(tile << 3) + row] = nes_chr_pack_pattern_row(tile_base[row], tile_base[row + 8U]);
            }
        }
        g_nes_chr_packed_bank_base[bank] = bank_base;
        g_nes_chr_packed_bank_rows[bank] = packed_bank;
    }

    return g_nes_chr_packed_bank_rows[bank];
}

uint16_t nes_chr_packed_mapper_row(nes_t* nes, uint8_t bank, const uint8_t* row_ptr)
{
    if (!nes || !row_ptr || bank >= 8U) {
        return 0;
    }

    const uint16_t* packed_bank = nes_chr_packed_mapper_bank(nes, bank);
    if (!packed_bank) {
        return nes_chr_pack_pattern_row(row_ptr[0], row_ptr[8]);
    }

    const uint8_t* bank_base = nes->nes_ppu.pattern_table[bank];
    const uintptr_t offset = (uintptr_t)row_ptr - (uintptr_t)bank_base;
    const uint16_t tile = (uint16_t)((offset & 0x03ffU) >> 4);
    const uint8_t row = (uint8_t)(offset & 0x07U);
    return packed_bank[(tile << 3) + row];
}
#endif

#if (NES_USE_FS == 1)
int nes_load_file(nes_t* nes, const char* file_path ){
    nes_header_ines_t nes_header_info = {0};

    void* nes_file = nes_fopen(file_path, "rb");
    if (nes_file == NULL){
        NES_LOG_ERROR("nes_load_file: failed to open file %s\n", file_path);
        goto error;
    }
#if (NES_USE_SRAM == 1)
    nes->nes_rom.sram = (uint8_t*)nes_malloc(SRAM_SIZE);
    if (nes->nes_rom.sram == NULL) {
        goto error;
    }
    nes_memset(nes->nes_rom.sram, 0xff, SRAM_SIZE);
#endif
    if (nes_fread(&nes_header_info, sizeof(nes_header_info), 1, nes_file)) {
        if (nes_memcmp(nes_header_info.identification, "NES\x1a", 4)){
            goto error;
        }
        if (nes_header_info.trainer){
#if (NES_USE_SRAM == 1)
            if (nes_fread(nes->nes_rom.sram, TRAINER_SIZE, 1, nes_file)==0){
                goto error;
            }
#else
            nes_fseek(nes_file, TRAINER_SIZE, SEEK_CUR);
#endif
        }
        if (nes_header_info.identifier==2){ //NES 2.0
            nes_header_nes2_t* nes2_header_info = (nes_header_nes2_t*)&nes_header_info;
            nes->nes_rom.prg_rom_size = ((nes2_header_info->prg_rom_size_m << 8) & 0xF00) | nes2_header_info->prg_rom_size_l;
            nes->nes_rom.chr_rom_size = ((nes2_header_info->chr_rom_size_m << 8) & 0xF00) | nes2_header_info->chr_rom_size_l;
            nes->nes_rom.mapper_number = ((nes2_header_info->mapper_number_h << 8) & 0xF00) | ((nes2_header_info->mapper_number_m << 4) & 0xF0) | (nes2_header_info->mapper_number_l & 0x0F);

        }else{  //INES
            nes_header_ines_t* ines_header_info = (nes_header_ines_t*)&nes_header_info;
            nes->nes_rom.prg_rom_size = ines_header_info->prg_rom_size;
            nes->nes_rom.chr_rom_size = ines_header_info->chr_rom_size;
            nes->nes_rom.mapper_number = ines_header_info->mapper_number_l | ines_header_info->mapper_number_h << 4;
        }
        nes->nes_rom.mirroring_type = nes_header_info.mirroring;
        nes->nes_rom.four_screen = nes_header_info.four_screen;
        nes->nes_rom.save_ram = nes_header_info.save;
        nes->nes_rom.prg_rom = (uint8_t*)nes_malloc(PRG_ROM_UNIT_SIZE * nes->nes_rom.prg_rom_size);
        if (nes->nes_rom.prg_rom == NULL) {
            goto error;
        }
        if (nes_fread(nes->nes_rom.prg_rom, PRG_ROM_UNIT_SIZE, nes->nes_rom.prg_rom_size, nes_file)==0){
            goto error;
        }
        nes->nes_rom.chr_rom = (uint8_t*)nes_malloc(CHR_ROM_UNIT_SIZE * (nes->nes_rom.chr_rom_size ? nes->nes_rom.chr_rom_size : 1));
        if (nes->nes_rom.chr_rom == NULL) {
            goto error;
        }
        if (nes->nes_rom.chr_rom_size){
            if (nes_fread(nes->nes_rom.chr_rom, CHR_ROM_UNIT_SIZE, (nes->nes_rom.chr_rom_size), nes_file)==0){
                goto error;
            }
        } else {
            nes_memset(nes->nes_rom.chr_rom, 0x00, CHR_ROM_UNIT_SIZE);
        }
    }else{
        goto error;
    }
    nes_fclose(nes_file);
    nes_file = NULL;
    nes_cpu_init(nes);
#if (NES_ENABLE_SOUND==1)
    nes_apu_init(nes);
#endif
    nes_ppu_init(nes);
    if(nes_load_mapper(nes)){
        goto error;
    }
    nes->nes_mapper.mapper_init(nes);
    nes_chr_packed_cache_build(nes);
    return NES_OK;
error:
    if (nes_file){
        nes_fclose(nes_file);
    }
    if (nes){
        nes_unload_file(nes);
    }
    return NES_ERROR;

}


int nes_unload_file(nes_t* nes){
    if (nes->nes_mapper.mapper_deinit){
        nes->nes_mapper.mapper_deinit(nes);
        nes->nes_mapper.mapper_deinit = NULL;
    }
    nes_chr_packed_cache_free(nes);
    if (nes->nes_rom.prg_rom){
        nes_free(nes->nes_rom.prg_rom);
        nes->nes_rom.prg_rom = NULL;
    }
    if (nes->nes_rom.chr_rom){
        nes_free(nes->nes_rom.chr_rom);
        nes->nes_rom.chr_rom = NULL;
    }
    if (nes->nes_rom.sram){
        nes_free(nes->nes_rom.sram);
        nes->nes_rom.sram = NULL;
    }
    return NES_OK;
}

#endif

int nes_load_rom(nes_t* nes, const uint8_t* nes_rom){
    nes_header_ines_t* nes_header_info = (nes_header_ines_t*)nes_rom;
#if (NES_USE_SRAM == 1)
    nes->nes_rom.sram = (uint8_t*)nes_malloc(SRAM_SIZE);
    if (nes->nes_rom.sram == NULL) {
        goto error;
    }
    nes_memset(nes->nes_rom.sram, 0xff, SRAM_SIZE);
#endif
    if ( nes_memcmp( nes_header_info->identification, "NES\x1a", 4 )){
        goto error;
    }
    uint8_t* nes_bin = (uint8_t*)nes_rom + sizeof(nes_header_ines_t);
    if (nes_header_info->trainer){
#if (NES_USE_SRAM == 1)
#else
#endif
        nes_bin += TRAINER_SIZE;
    }

    if (nes_header_info->identifier==2){ //NES 2.0
        nes_header_nes2_t* nes2_header_info = (nes_header_nes2_t*)nes_header_info;
        nes->nes_rom.prg_rom_size = ((nes2_header_info->prg_rom_size_m << 8) & 0xF00) | nes2_header_info->prg_rom_size_l;
        nes->nes_rom.chr_rom_size = ((nes2_header_info->chr_rom_size_m << 8) & 0xF00) | nes2_header_info->chr_rom_size_l;
        nes->nes_rom.mapper_number = ((nes2_header_info->mapper_number_h << 8) & 0xF00) | ((nes2_header_info->mapper_number_m << 4) & 0xF0) | (nes2_header_info->mapper_number_l & 0x0F);

    }else{  //INES
        nes_header_ines_t* ines_header_info = (nes_header_ines_t*)nes_header_info;
        nes->nes_rom.prg_rom_size = ines_header_info->prg_rom_size;
        nes->nes_rom.chr_rom_size = ines_header_info->chr_rom_size;
        nes->nes_rom.mapper_number = ines_header_info->mapper_number_l | ines_header_info->mapper_number_h << 4;
    }

    nes->nes_rom.mirroring_type = (nes_header_info->mirroring);
    nes->nes_rom.four_screen = (nes_header_info->four_screen);
    nes->nes_rom.save_ram = (nes_header_info->save);

    nes->nes_rom.prg_rom = nes_bin;
    nes_bin += PRG_ROM_UNIT_SIZE * nes->nes_rom.prg_rom_size;

    if (nes->nes_rom.chr_rom_size){
        nes->nes_rom.chr_rom = nes_bin;
    } else {
        nes->nes_rom.chr_rom = (uint8_t*)nes_malloc(CHR_ROM_UNIT_SIZE);
        if (nes->nes_rom.chr_rom == NULL) {
            goto error;
        }
        nes_memset(nes->nes_rom.chr_rom, 0x00, CHR_ROM_UNIT_SIZE);
    }
    nes_cpu_init(nes);
#if (NES_ENABLE_SOUND==1)
    nes_apu_init(nes);
#endif
    nes_ppu_init(nes);
    if(nes_load_mapper(nes)){
        goto error;
    }
    nes->nes_mapper.mapper_init(nes);
    nes_chr_packed_cache_build(nes);
    return NES_OK;
error:
    if (nes){
        nes_unload_rom(nes);
    }
    return NES_ERROR;
}

int nes_unload_rom(nes_t* nes){
    if (nes->nes_mapper.mapper_deinit){
        nes->nes_mapper.mapper_deinit(nes);
        nes->nes_mapper.mapper_deinit = NULL;
    }
    nes_chr_packed_cache_free(nes);
    if (nes->nes_rom.chr_rom_size == 0 && nes->nes_rom.chr_rom){
        nes_free(nes->nes_rom.chr_rom);
        nes->nes_rom.chr_rom = NULL;
    }
#if (NES_USE_SRAM == 1)
    if (nes->nes_rom.sram){
        nes_free(nes->nes_rom.sram);
        nes->nes_rom.sram = NULL;
    }
#endif
    return NES_OK;
}
