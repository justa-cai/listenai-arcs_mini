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

#ifndef CONFIG_NES_VISIBLE_X_OFFSET
#define CONFIG_NES_VISIBLE_X_OFFSET 8
#endif
#ifndef CONFIG_NES_VISIBLE_WIDTH
#define CONFIG_NES_VISIBLE_WIDTH 240
#endif

#define NES_VISIBLE_RENDER_X0 ((uint16_t)CONFIG_NES_VISIBLE_X_OFFSET)
#define NES_VISIBLE_RENDER_X1 ((uint16_t)(CONFIG_NES_VISIBLE_X_OFFSET + CONFIG_NES_VISIBLE_WIDTH))

#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
static const uint8_t g_nes_packed_pixel_shift[8] = {14, 6, 12, 4, 10, 2, 8, 0};
#endif

#if CONFIG_NES_CORE_PROFILE_ENABLE
#include "nmsis_core.h"

extern volatile uint32_t SystemCoreClock;
#endif

//https://www.nesdev.org/pal.txt

static nes_color_t nes_palette[]={
#if (NES_COLOR_DEPTH == 32) // ARGB8888
    0xFF757575, 0xFF271B8F, 0xFF0000AB, 0xFF47009F, 0xFF8F0077, 0xFFAB0013, 0xFFA70000, 0xFF7F0B00,0xFF432F00, 0xFF004700, 0xFF005100, 0xFF003F17, 0xFF1B3F5F, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFBCBCBC, 0xFF0073EF, 0xFF233BEF, 0xFF8300F3, 0xFFBF00BF, 0xFFF7005B, 0xFFDB2B00, 0xFFCB4F0F,0xFF8B7300, 0xFF009700, 0xFF00AB00, 0xFF00933B, 0xFF00838B, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFFFFFFF, 0xFF3FBFFF, 0xFF5F97FF, 0xFFA78BFD, 0xFFF77BFF, 0xFFF77B7B, 0xFFFF7763, 0xFFFF9B3B,0xFFF3BF3F, 0xFF83D313, 0xFF4FDF4B, 0xFF58F898, 0xFF00EBDB, 0xFF000000, 0xFF000000, 0xFF000000,
    0xFFFFFFFF, 0xFFABE7FF, 0xFFC7D7FF, 0xFFD7CBFF, 0xFFFFC7FF, 0xFFFFC7DB, 0xFFFFBFB3, 0xFFFFDBAB,0xFFFFE7A3, 0xFFE3FFA3, 0xFFABF3BF, 0xFFB3FFCF, 0xFF9FFFF3, 0xFF000000, 0xFF000000, 0xFF000000,
#elif (NES_COLOR_DEPTH == 16)
#if (NES_COLOR_SWAP == 0) // RGB565
    0x73AE, 0x20D1, 0x0015, 0x4013, 0x880E, 0xA802, 0xA000, 0x7840,0x4160, 0x0220, 0x0280, 0x01E2, 0x19EB, 0x0000, 0x0000, 0x0000,
    0xBDF7, 0x039D, 0x21DD, 0x801E, 0xB817, 0xF00B, 0xD940, 0xCA61,0x8B80, 0x04A0, 0x0540, 0x0487, 0x0411, 0x0000, 0x0000, 0x0000,
    0xFFFF, 0x3DFF, 0x5CBF, 0xA45F, 0xF3DF, 0xF3CF, 0xFBAC, 0xFCC7,0xF5E7, 0x8682, 0x4EE9, 0x5FD3, 0x075B, 0x0000, 0x0000, 0x0000,
    0xFFFF, 0xAF3F, 0xC6BF, 0xD65F, 0xFE3F, 0xFE3B, 0xFDF6, 0xFED5,0xFF34, 0xE7F4, 0xAF97, 0xB7F9, 0x9FFE, 0x0000, 0x0000, 0x0000,
#else // RGB565_SWAP
    0xAE73, 0xD120, 0x1500, 0x1340, 0x0E88, 0x02A8, 0x00A0, 0x4078,0x6041, 0x2002, 0x8002, 0xE201, 0xEB19, 0x0000, 0x0000, 0x0000,
    0xF7BD, 0x9D03, 0xDD21, 0x1E80, 0x17B8, 0x0BF0, 0x40D9, 0x61CA,0x808B, 0xA004, 0x4005, 0x8704, 0x1104, 0x0000, 0x0000, 0x0000,
    0xFFFF, 0xFF3D, 0xBF5C, 0x5FA4, 0xDFF3, 0xCFF3, 0xACFB, 0xC7FC,0xE7F5, 0x8286, 0xE94E, 0xD35F, 0x5B07, 0x0000, 0x0000, 0x0000,
    0xFFFF, 0x3FAF, 0xBFC6, 0x5FD6, 0x3FFE, 0x3BFE, 0xF6FD, 0xD5FE,0x34FF, 0xF4E7, 0x97AF, 0xF9B7, 0xFE9F, 0x0000, 0x0000, 0x0000,
#endif /* NES_COLOR_SWAP */
#endif /* NES_COLOR_DEPTH */
};

nes_t* nes_init(void){
    nes_t* nes = (nes_t *)nes_sram_malloc(sizeof(nes_t));
    if (nes == NULL) {
        return NULL;
    }
    nes_memset(nes, 0, sizeof(nes_t));
    nes->nes_draw_data = (nes_color_t *)nes_malloc(sizeof(nes_color_t) * NES_DRAW_SIZE);
    if (nes->nes_draw_data == NULL) {
        nes_sram_free(nes);
        return NULL;
    }
    nes_initex(nes);
    return nes;
}

int nes_deinit(nes_t *nes){
    nes->nes_quit = 1;
    if (nes->nes_mapper.mapper_deinit) {
        nes->nes_mapper.mapper_deinit(nes);
        nes->nes_mapper.mapper_deinit = NULL;
    }
    nes_deinitex(nes);
    if (nes){
        nes_chr_packed_cache_free(nes);
        if (nes->nes_draw_data) {
            nes_free(nes->nes_draw_data);
            nes->nes_draw_data = NULL;
        }
        nes_sram_free(nes);
        nes = NULL;
    }
    return NES_OK;
}

static inline void nes_palette_generate(nes_t* nes){
    for (uint8_t i = 0; i < 32; i++) {
        nes->nes_ppu.palette[i] = nes_palette[nes->nes_ppu.palette_indexes[i]];
    }
    for (uint8_t i = 1; i < 8; i++){
        nes->nes_ppu.palette[4 * i] = nes->nes_ppu.palette[0];
    }
}

static inline const uint8_t* nes_pattern_ptr(nes_t* nes, uint16_t pattern_addr)
{
    if (nes->nes_mapper.mapper_ppu) {
        nes->nes_mapper.mapper_ppu(nes, pattern_addr);
    }
    return nes->nes_ppu.pattern_table[(pattern_addr >> 10) & 0x07U] + (pattern_addr & 0x03ffU);
}

static inline uint8_t nes_pattern_is_mapped(nes_t* nes)
{
    return (nes->nes_mapper.mapper_ppu != NULL) || nes->nes_mapper.mapper_banked_chr;
}

static inline uint8_t nes_pattern_is_direct_banked(nes_t* nes)
{
    return (nes->nes_mapper.mapper_ppu == NULL) && nes->nes_mapper.mapper_banked_chr;
}

static inline void nes_render_background_tile8(nes_color_t* draw_data,
                                               const nes_color_t* tile_palette,
                                               uint16_t pattern)
{
#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
    draw_data[0] = tile_palette[(pattern >> 14) & 3];
    draw_data[1] = tile_palette[(pattern >> 6) & 3];
    draw_data[2] = tile_palette[(pattern >> 12) & 3];
    draw_data[3] = tile_palette[(pattern >> 4) & 3];
    draw_data[4] = tile_palette[(pattern >> 10) & 3];
    draw_data[5] = tile_palette[(pattern >> 2) & 3];
    draw_data[6] = tile_palette[(pattern >> 8) & 3];
    draw_data[7] = tile_palette[pattern & 3];
#else
    const uint8_t bit0 = (pattern >> 8) & 0xff;
    const uint8_t bit1 = pattern & 0xff;
    draw_data[0] = tile_palette[((bit0 >> 7) & 0x01) | (((bit1 >> 7) << 1) & 0x02)];
    draw_data[1] = tile_palette[((bit0 >> 6) & 0x01) | (((bit1 >> 6) << 1) & 0x02)];
    draw_data[2] = tile_palette[((bit0 >> 5) & 0x01) | (((bit1 >> 5) << 1) & 0x02)];
    draw_data[3] = tile_palette[((bit0 >> 4) & 0x01) | (((bit1 >> 4) << 1) & 0x02)];
    draw_data[4] = tile_palette[((bit0 >> 3) & 0x01) | (((bit1 >> 3) << 1) & 0x02)];
    draw_data[5] = tile_palette[((bit0 >> 2) & 0x01) | (((bit1 >> 2) << 1) & 0x02)];
    draw_data[6] = tile_palette[((bit0 >> 1) & 0x01) | (((bit1 >> 1) << 1) & 0x02)];
    draw_data[7] = tile_palette[(bit0 & 0x01) | ((bit1 << 1) & 0x02)];
#endif
}

#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
static inline uint16_t nes_pack_pattern_row(uint8_t bit0, uint8_t bit1)
{
    return ((uint16_t)(bit1 & 0xAA) << 8) |
           ((uint16_t)(bit1 & 0x55) << 1) |
           ((uint16_t)(bit0 & 0xAA) << 7) |
           (bit0 & 0x55);
}

static inline uint16_t nes_get_direct_banked_pattern_row(nes_t* nes,
                                                         const uint16_t* const packed_banks[8],
                                                         uint16_t pattern_addr,
                                                         const uint8_t** row_ptr_out)
{
    const uint8_t bank = (uint8_t)((pattern_addr >> 10) & 0x07U);
    const uint16_t bank_offset = (uint16_t)(pattern_addr & 0x03ffU);
    const uint8_t* row_ptr = nes->nes_ppu.pattern_table[bank] + bank_offset;
    const uint16_t* packed_bank = packed_banks[bank];
    if (row_ptr_out) {
        *row_ptr_out = row_ptr;
    }
    if (packed_bank) {
        return packed_bank[((bank_offset >> 4) << 3) + (bank_offset & 0x07U)];
    }
    return nes_pack_pattern_row(row_ptr[0], row_ptr[8U]);
}

static inline uint8_t nes_mapped_chr_bank_for_row(nes_t* nes, const uint8_t* row_ptr)
{
    const uintptr_t row_addr = (uintptr_t)row_ptr;
    for (uint8_t bank = 0; bank < 8U; bank++) {
        const uintptr_t bank_base = (uintptr_t)nes->nes_ppu.pattern_table[bank];
        if (row_addr >= bank_base && row_addr + 8U < bank_base + 0x400U) {
            return bank;
        }
    }
    return 0xffU;
}

static inline uint16_t nes_get_packed_pattern_row(nes_t* nes, const uint8_t* tile_base, uint8_t row)
{
    if (nes->nes_chr_packed_rows && nes->nes_rom.chr_rom) {
        const uintptr_t chr_base = (uintptr_t)nes->nes_rom.chr_rom;
        const uintptr_t tile_addr = (uintptr_t)tile_base;
        const uintptr_t chr_end = chr_base + (uintptr_t)nes->nes_rom.chr_rom_size * CHR_ROM_UNIT_SIZE;
        if (tile_addr >= chr_base && tile_addr + 16U <= chr_end) {
            const uint32_t chr_offset = (uint32_t)(tile_addr - chr_base);
            const uint32_t packed_index = ((chr_offset >> 4) << 3) + row;
            if (packed_index < nes->nes_chr_packed_row_count) {
                return nes->nes_chr_packed_rows[packed_index];
            }
        }
    }
    const uint8_t* row_ptr = tile_base + row;
    const uint8_t bank = nes_mapped_chr_bank_for_row(nes, row_ptr);
    if (bank != 0xffU) {
        return nes_chr_packed_mapper_row(nes, bank, row_ptr);
    }
    return nes_pack_pattern_row(row_ptr[0], row_ptr[8U]);
}

static inline const uint16_t* nes_get_packed_chr_rows(nes_t* nes, const uint8_t* chr_ptr)
{
    if (nes->nes_chr_packed_rows && nes->nes_rom.chr_rom) {
        const uintptr_t chr_base = (uintptr_t)nes->nes_rom.chr_rom;
        const uintptr_t ptr_addr = (uintptr_t)chr_ptr;
        const uintptr_t chr_end = chr_base + (uintptr_t)nes->nes_rom.chr_rom_size * CHR_ROM_UNIT_SIZE;
        if (ptr_addr >= chr_base && ptr_addr < chr_end) {
            const uint32_t chr_offset = (uint32_t)(ptr_addr - chr_base);
            const uint32_t packed_index = (chr_offset >> 4) << 3;
            if (packed_index < nes->nes_chr_packed_row_count) {
                return nes->nes_chr_packed_rows + packed_index;
            }
        }
    }
    return NULL;
}
#endif

static void nes_render_background_line(nes_t* nes,uint16_t scanline,nes_color_t* draw_data){
    (void)scanline;
    uint16_t p = 0;
#if CONFIG_NES_FAST_VISIBLE_RENDER
    const uint16_t render_x1 = NES_VISIBLE_RENDER_X1;
#else
    const uint16_t render_x1 = NES_WIDTH;
#endif
    const uint8_t scroll_x = nes->nes_ppu.x;
    const uint8_t dx = (const uint8_t)nes->nes_ppu.v.coarse_x;
    const uint8_t dy = (const uint8_t)nes->nes_ppu.v.fine_y;
    const uint8_t tile_y = (const uint8_t)nes->nes_ppu.v.coarse_y;
    const uint16_t tile_row = (uint16_t)tile_y << 5;
    const uint16_t attr_row = 960U + ((uint16_t)(tile_y >> 2) << 3);
    const uint8_t attr_y_shift = (tile_y & 2U) << 1;
    const uint8_t pattern_bank_base = nes->nes_ppu.CTRL_B ? 4U : 0U;
#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
    const uint8_t pattern_direct_banked = nes_pattern_is_direct_banked(nes);
    const uint8_t pattern_mapped = pattern_direct_banked || (nes->nes_mapper.mapper_ppu != NULL);
#else
    const uint8_t pattern_mapped = nes_pattern_is_mapped(nes);
#endif
    const uint8_t* pattern_base = nes->nes_ppu.pattern_table[pattern_bank_base];
#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
    const uint16_t* packed_mapper_banks[8] = {0};
    if (pattern_direct_banked) {
        for (uint8_t bank = 0; bank < 8U; bank++) {
            packed_mapper_banks[bank] = nes_chr_packed_mapper_bank(nes, bank);
        }
    }
    const uint16_t* packed_pattern_base = pattern_mapped ? NULL : nes_get_packed_chr_rows(nes, pattern_base);
#endif
    uint8_t nametable_id = (uint8_t)nes->nes_ppu.v.nametable;
    uint8_t tile_x = dx;
    const uint8_t tile_count = 32U + (scroll_x ? 1U : 0U);

#if CONFIG_NES_FAST_VISIBLE_RENDER
    if (scroll_x == 0U) {
        uint16_t out_x = NES_VISIBLE_RENDER_X0;
        uint8_t visible_nametable_id = nametable_id;
        uint8_t visible_tile_x = (uint8_t)(dx + 1U);
        if (visible_tile_x == 32U) {
            visible_tile_x = 0;
            visible_nametable_id ^= 1U;
        }

        for (uint8_t tile = 0; tile < (uint8_t)((NES_VISIBLE_RENDER_X1 - NES_VISIBLE_RENDER_X0) >> 3); tile++) {
            const uint8_t* name_table = nes->nes_ppu.name_table[visible_nametable_id];
            const uint8_t pattern_id = name_table[tile_row + visible_tile_x];
            const uint16_t pattern_addr = (uint16_t)(((uint16_t)pattern_bank_base << 10) + ((uint16_t)pattern_id << 4) + dy);
#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
            uint16_t pattern;
            if (pattern_direct_banked) {
                pattern = nes_get_direct_banked_pattern_row(nes, packed_mapper_banks, pattern_addr, NULL);
            } else {
                const uint8_t* bit0_p = pattern_mapped
                                       ? nes_pattern_ptr(nes, pattern_addr)
                                       : pattern_base + ((uint16_t)pattern_id << 4) + dy;
                pattern = pattern_mapped
                        ? nes_chr_packed_mapper_row(nes, (uint8_t)((pattern_addr >> 10) & 0x07U), bit0_p)
                        : packed_pattern_base
                        ? packed_pattern_base[((uint16_t)pattern_id << 3) + dy]
                        : nes_get_packed_pattern_row(nes, bit0_p - dy, dy);
            }
#else
            const uint8_t* bit0_p = pattern_mapped
                                   ? nes_pattern_ptr(nes, pattern_addr)
                                   : pattern_base + ((uint16_t)pattern_id << 4) + dy;
            const uint16_t pattern = ((uint16_t)bit0_p[8U] << 8) | bit0_p[0];
#endif
            const uint8_t attribute = name_table[attr_row + (visible_tile_x >> 2)];
            const uint8_t high_bit = ((attribute >> (attr_y_shift | (visible_tile_x & 2U))) & 3U) << 2;
            nes_render_background_tile8(draw_data + out_x, nes->nes_ppu.background_palette + high_bit, pattern);

            out_x += 8U;
            visible_tile_x++;
            if (visible_tile_x == 32U) {
                visible_tile_x = 0;
                visible_nametable_id ^= 1U;
            }
        }
        return;
    }
#endif

    for (uint8_t tile = 0; tile < tile_count
#if CONFIG_NES_FAST_VISIBLE_RENDER
            && p < NES_VISIBLE_RENDER_X1
#endif
            ; tile++) {
        int8_t m = (tile == 0) ? (int8_t)(7U - scroll_x) : 7;
        const int8_t end_m = (scroll_x && tile == (tile_count - 1U)) ? (int8_t)(8U - scroll_x) : 0;
#if CONFIG_NES_FAST_VISIBLE_RENDER
        const uint16_t tile_pixels = (uint16_t)(m - end_m + 1);
        if (p + tile_pixels <= NES_VISIBLE_RENDER_X0) {
            p += tile_pixels;
            tile_x++;
            if (tile_x == 32U) {
                tile_x = 0;
                nametable_id ^= 1U;
            }
            continue;
        }
#endif
        const uint8_t* name_table = nes->nes_ppu.name_table[nametable_id];
        const uint8_t pattern_id = name_table[tile_row + tile_x];
        const uint16_t pattern_addr = (uint16_t)(((uint16_t)pattern_bank_base << 10) + ((uint16_t)pattern_id << 4) + dy);
#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
        uint16_t pattern;
        if (pattern_direct_banked) {
            pattern = nes_get_direct_banked_pattern_row(nes, packed_mapper_banks, pattern_addr, NULL);
        } else {
            const uint8_t* bit0_p = pattern_mapped
                                   ? nes_pattern_ptr(nes, pattern_addr)
                                   : pattern_base + ((uint16_t)pattern_id << 4) + dy;
            pattern = pattern_mapped
                    ? nes_chr_packed_mapper_row(nes, (uint8_t)((pattern_addr >> 10) & 0x07U), bit0_p)
                    : packed_pattern_base
                    ? packed_pattern_base[((uint16_t)pattern_id << 3) + dy]
                    : nes_get_packed_pattern_row(nes, bit0_p - dy, dy);
        }
#else
        const uint8_t* bit0_p = pattern_mapped
                               ? nes_pattern_ptr(nes, pattern_addr)
                               : pattern_base + ((uint16_t)pattern_id << 4) + dy;
        const uint16_t pattern = ((uint16_t)bit0_p[8U] << 8) | bit0_p[0];
#endif
        const uint8_t attribute = name_table[attr_row + (tile_x >> 2)];
        const uint8_t high_bit = ((attribute >> (attr_y_shift | (tile_x & 2U))) & 3U) << 2;
        const nes_color_t* tile_palette = nes->nes_ppu.background_palette + high_bit;
#if CONFIG_NES_FAST_VISIBLE_RENDER
        while (p < NES_VISIBLE_RENDER_X0 && m >= end_m) {
            p++;
            m--;
        }
#endif
        if (m == 7 && end_m == 0 && p + 8U <= render_x1) {
            nes_render_background_tile8(draw_data + p, tile_palette, pattern);
            p += 8U;
        } else {
            for (; m >= end_m
#if CONFIG_NES_FAST_VISIBLE_RENDER
                    && p < render_x1
#endif
                    ; m--) {
#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
                const uint8_t palette_index = (pattern >> g_nes_packed_pixel_shift[7 - m]) & 3U;
#else
                const uint8_t shift = (uint8_t)m;
                const uint8_t low_bit = ((pattern >> shift) & 0x01) |
                                        (((pattern >> (8U + shift)) << 1) & 0x02);
                const uint8_t palette_index = low_bit;
#endif
                draw_data[p++] = tile_palette[palette_index];
            }
        }
        tile_x++;
        if (tile_x == 32U) {
            tile_x = 0;
            nametable_id ^= 1U;
        }
    }
}

static inline void nes_render_sprite_pixels(nes_t* nes,
                                            nes_color_t* draw_data,
                                            const sprite_info_t* sprite_info,
                                            nes_color_t background_color,
                                            uint16_t sprite_pattern)
{
    int8_t start = 0;
    int8_t end = 7;
#if CONFIG_NES_FAST_VISIBLE_RENDER
    if (sprite_info->x < NES_VISIBLE_RENDER_X0) {
        start = (int8_t)(NES_VISIBLE_RENDER_X0 - sprite_info->x);
    }
    if ((uint16_t)sprite_info->x + 7U >= NES_VISIBLE_RENDER_X1) {
        end = (int8_t)(NES_VISIBLE_RENDER_X1 - 1U - sprite_info->x);
    }
    if (start > end) {
        return;
    }
#endif
    const nes_color_t* sprite_palette = nes->nes_ppu.sprite_palette + ((uint8_t)sprite_info->sprite_palette << 2);
    nes_color_t* dst = draw_data + sprite_info->x + start;

    if (sprite_info->flip_h) {
        for (int8_t i = start; i <= end; i++, dst++) {
#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
            const uint8_t low_bit = (uint8_t)((sprite_pattern >> g_nes_packed_pixel_shift[7 - i]) & 3U);
#else
            const uint8_t low_bit = ((sprite_pattern >> i) & 0x01) | (((sprite_pattern >> (8 + i)) << 1) & 0x02);
#endif
            if (low_bit && (!sprite_info->priority || *dst == background_color)) {
                *dst = sprite_palette[low_bit];
            }
        }
    } else {
        for (int8_t i = start; i <= end; i++, dst++) {
#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
            const uint8_t low_bit = (uint8_t)((sprite_pattern >> g_nes_packed_pixel_shift[i]) & 3U);
#else
            const uint8_t m = 7U - (uint8_t)i;
            const uint8_t low_bit = ((sprite_pattern >> m) & 0x01) | (((sprite_pattern >> (8 + m)) << 1) & 0x02);
#endif
            if (low_bit && (!sprite_info->priority || *dst == background_color)) {
                *dst = sprite_palette[low_bit];
            }
        }
    }
}

static void nes_render_sprite_line(nes_t* nes,uint16_t scanline,nes_color_t* draw_data){
    const nes_color_t background_color = nes->nes_ppu.background_palette[0];
#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
    const uint8_t pattern_direct_banked = nes_pattern_is_direct_banked(nes);
    const uint8_t pattern_mapped = pattern_direct_banked || (nes->nes_mapper.mapper_ppu != NULL);
    const uint16_t* packed_mapper_banks[8] = {0};
    if (pattern_direct_banked) {
        for (uint8_t bank = 0; bank < 8U; bank++) {
            packed_mapper_banks[bank] = nes_chr_packed_mapper_bank(nes, bank);
        }
    }
#else
    const uint8_t pattern_mapped = nes_pattern_is_mapped(nes);
#endif
    uint8_t sprite[8] = {0};
    uint8_t sprite_numbers = 0;
    const uint8_t sprite_size = nes->nes_ppu.CTRL_H?16:8;

    // 遍历显示的精灵和检测是否精灵溢出
    for (uint8_t i = 0; i < 64; i++){
        if (nes->nes_ppu.sprite_info[i].y >= 0xEF){
            continue;
        }
        uint8_t sprite_y = (uint8_t)(nes->nes_ppu.sprite_info[i].y + 1);
        if (scanline < sprite_y || scanline >= sprite_y + sprite_size){
            continue;
        }
        if (sprite_numbers==8){
            nes->nes_ppu.STATUS_O = 1;
            break;
        }
        sprite[sprite_numbers++]=i;
    }
    // 显示精灵
    for (uint8_t sprite_number = sprite_numbers; sprite_number > 0; sprite_number--){
        const uint8_t sprite_id = sprite[sprite_number-1];
        const sprite_info_t sprite_info = nes->nes_ppu.sprite_info[sprite_id];
        const uint8_t sprite_y = (uint8_t)(sprite_info.y + 1);
        const uint8_t sprite_pattern_bank = nes->nes_ppu.CTRL_H ? ((sprite_info.pattern_8x16) ? 4U : 0U)
                                                                 : (nes->nes_ppu.CTRL_S ? 4U : 0U);
        const uint8_t sprite_pattern_index = nes->nes_ppu.CTRL_H ? (sprite_info.tile_index_8x16 << 1)
                                                                  : sprite_info.tile_index_number;
        uint16_t sprite_pattern_addr = (uint16_t)(((uint16_t)sprite_pattern_bank << 10) +
                                                  ((uint16_t)sprite_pattern_index << 4));
        const uint8_t* sprite_bit0_p = nes->nes_ppu.pattern_table[sprite_pattern_bank] + ((uint16_t)sprite_pattern_index << 4);
        const uint8_t* sprite_bit1_p = sprite_bit0_p + 8;

        uint8_t dy = (uint8_t)(scanline - sprite_y);

        if (nes->nes_ppu.CTRL_H){
            if (sprite_info.flip_v){
                if (dy < 8){
                    sprite_bit0_p +=16;
                    sprite_bit1_p +=16;
                    sprite_pattern_addr += 16U;
                    dy = sprite_size - dy - 1 -8;
                }else{
                    dy = sprite_size - dy - 1;
                }
            }else{
                if (dy > 7){
                    sprite_bit0_p +=16;
                    sprite_bit1_p +=16;
                    sprite_pattern_addr += 16U;
                    dy-=8;
                }
            }
        }else{
            if (sprite_info.flip_v){
                dy = sprite_size - dy - 1;
            }
        }

        const uint16_t sprite_row_addr = (uint16_t)(sprite_pattern_addr + dy);
        const uint8_t* sprite_row_p;
#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
        uint16_t sprite_pattern = 0;
        if (pattern_direct_banked) {
            sprite_pattern = nes_get_direct_banked_pattern_row(nes, packed_mapper_banks, sprite_row_addr, &sprite_row_p);
        } else
#endif
        {
            sprite_row_p = pattern_mapped
                         ? nes_pattern_ptr(nes, sprite_row_addr)
                         : sprite_bit0_p + dy;
        }
        const uint8_t sprite_bit0 = sprite_row_p[0];
        const uint8_t sprite_bit1 = pattern_mapped ? sprite_row_p[8] : sprite_bit1_p[dy];
        const uint8_t sprite_date = sprite_bit0 | sprite_bit1;
#if (NES_FRAME_SKIP != 0)
        if(nes->nes_frame_skip_count == 0)
#endif
        {
            if (sprite_date
#if CONFIG_NES_FAST_VISIBLE_RENDER
                    && sprite_info.x < NES_VISIBLE_RENDER_X1
                    && (uint16_t)sprite_info.x + 7U >= NES_VISIBLE_RENDER_X0
#endif
            ) {
#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
                if (!pattern_direct_banked) {
                    sprite_pattern = pattern_mapped
                                   ? nes_chr_packed_mapper_row(nes, (uint8_t)((sprite_row_addr >> 10) & 0x07U), sprite_row_p)
                                   : nes_get_packed_pattern_row(nes, sprite_bit0_p, dy);
                }
#else
                const uint16_t sprite_pattern = ((uint16_t)sprite_bit1 << 8) | sprite_bit0;
#endif
                nes_render_sprite_pixels(nes, draw_data, &sprite_info, background_color, sprite_pattern);
            }
        }
        // 检测精灵0命中
        if (sprite_id==0){
            if (sprite_date && nes->nes_ppu.MASK_b && nes->nes_ppu.STATUS_S == 0){
                // printf("scanline:%d x:%d MASK_m:%d MASK_M:%d\n",
                //     scanline,nes->nes_ppu.x,nes->nes_ppu.MASK_m,nes->nes_ppu.MASK_M);
                const uint8_t nametable_id = (uint8_t)nes->nes_ppu.v.nametable;
                const uint8_t tile_x = (nes->nes_ppu.sprite_info[0].x) >> 3;
                const uint8_t tile_y = (uint8_t)(scanline >> 3);
                const uint8_t pattern_id = nes->nes_ppu.name_table[nametable_id][tile_x + (tile_y << 5)];
                const uint8_t background_pattern_bank = nes->nes_ppu.CTRL_B ? 4U : 0U;
                const uint16_t background_pattern_addr = (uint16_t)(((uint16_t)background_pattern_bank << 10) +
                                                                    ((uint16_t)pattern_id << 4) + dy);
                const uint8_t* bit0_p;
#if CONFIG_NES_FAST_PPU_PACKED_PATTERN
                if (pattern_direct_banked) {
                    (void)nes_get_direct_banked_pattern_row(nes, packed_mapper_banks, background_pattern_addr, &bit0_p);
                } else
#endif
                {
                    bit0_p = pattern_mapped
                           ? nes_pattern_ptr(nes, background_pattern_addr)
                           : nes->nes_ppu.pattern_table[background_pattern_bank] + ((uint16_t)pattern_id << 4) + dy;
                }
                const uint8_t background_date = bit0_p[0] | (bit0_p[8] << 1);
                if (sprite_date & background_date){
                    nes->nes_ppu.STATUS_S = 1;
                    // printf("scanline:%d sprite_bit0:%d sprite_bit1:%d sprite_date:%d bit0_p:%d bit1_p:%d background_date:%d \n",
                    // scanline,sprite_bit0,sprite_bit1,sprite_date,bit0_p[dy],bit1_p[dy],background_date);
                }
            }
        }
        
    }
}

static inline void nes_check_sprite0_hit_line(nes_t* nes, uint16_t scanline)
{
    if (nes->nes_ppu.STATUS_S || !nes->nes_ppu.MASK_b || !nes->nes_ppu.MASK_s) {
        return;
    }

    const sprite_info_t sprite_info = nes->nes_ppu.sprite_info[0];
    if (sprite_info.y >= 0xEF) {
        return;
    }

    const uint8_t sprite_size = nes->nes_ppu.CTRL_H ? 16 : 8;
    const uint8_t sprite_y = (uint8_t)(sprite_info.y + 1);
    if (scanline < sprite_y || scanline >= sprite_y + sprite_size) {
        return;
    }

    uint8_t dy = (uint8_t)(scanline - sprite_y);
    const uint8_t sprite_pattern_bank = nes->nes_ppu.CTRL_H ? ((sprite_info.pattern_8x16) ? 4U : 0U)
                                                             : (nes->nes_ppu.CTRL_S ? 4U : 0U);
    const uint8_t sprite_pattern_index = nes->nes_ppu.CTRL_H ? (sprite_info.tile_index_8x16 << 1)
                                                              : sprite_info.tile_index_number;
    uint16_t sprite_pattern_addr = (uint16_t)(((uint16_t)sprite_pattern_bank << 10) +
                                              ((uint16_t)sprite_pattern_index << 4));
    const uint8_t *sprite_bit0_p = nes->nes_ppu.pattern_table[sprite_pattern_bank] + ((uint16_t)sprite_pattern_index << 4);
    const uint8_t *sprite_bit1_p = sprite_bit0_p + 8;

    if (nes->nes_ppu.CTRL_H) {
        if (sprite_info.flip_v) {
            if (dy < 8) {
                sprite_bit0_p += 16;
                sprite_bit1_p += 16;
                sprite_pattern_addr += 16U;
                dy = sprite_size - dy - 1 - 8;
            } else {
                dy = sprite_size - dy - 1;
            }
        } else if (dy > 7) {
            sprite_bit0_p += 16;
            sprite_bit1_p += 16;
            sprite_pattern_addr += 16U;
            dy -= 8;
        }
    } else if (sprite_info.flip_v) {
        dy = sprite_size - dy - 1;
    }

    const uint8_t pattern_direct_banked = nes_pattern_is_direct_banked(nes);
    const uint8_t pattern_mapped = pattern_direct_banked || (nes->nes_mapper.mapper_ppu != NULL);
    const uint16_t sprite_row_addr = (uint16_t)(sprite_pattern_addr + dy);
    const uint8_t *sprite_row_p = pattern_direct_banked
                                ? nes->nes_ppu.pattern_table[(sprite_row_addr >> 10) & 0x07U] + (sprite_row_addr & 0x03ffU)
                                : pattern_mapped
                                ? nes_pattern_ptr(nes, sprite_row_addr)
                                : sprite_bit0_p + dy;
    const uint8_t sprite_date = sprite_row_p[0] |
                                (pattern_mapped ? sprite_row_p[8] : sprite_bit1_p[dy]);
    if (!sprite_date) {
        return;
    }

    const uint8_t nametable_id = (uint8_t)nes->nes_ppu.v.nametable;
    const uint8_t tile_x = sprite_info.x >> 3;
    const uint8_t tile_y = (uint8_t)(scanline >> 3);
    const uint8_t pattern_id = nes->nes_ppu.name_table[nametable_id][tile_x + (tile_y << 5)];
    const uint8_t background_pattern_bank = nes->nes_ppu.CTRL_B ? 4U : 0U;
    const uint16_t background_pattern_addr = (uint16_t)(((uint16_t)background_pattern_bank << 10) +
                                                        ((uint16_t)pattern_id << 4) + dy);
    const uint8_t *bit0_p = pattern_direct_banked
                           ? nes->nes_ppu.pattern_table[(background_pattern_addr >> 10) & 0x07U] + (background_pattern_addr & 0x03ffU)
                           : pattern_mapped
                           ? nes_pattern_ptr(nes, background_pattern_addr)
                           : nes->nes_ppu.pattern_table[background_pattern_bank] + ((uint16_t)pattern_id << 4) + dy;
    const uint8_t background_date = bit0_p[0] | (bit0_p[8] << 1);
    if (sprite_date & background_date) {
        nes->nes_ppu.STATUS_S = 1;
    }
}

#if CONFIG_NES_CORE_PROFILE_ENABLE
typedef struct {
    uint32_t start_ms;
    uint32_t last_ms;
    uint32_t frames;
    uint32_t draw_frames;
    uint32_t skip_frames;
    uint32_t cpu_calls;
    uint32_t bg_calls;
    uint32_t sprite_calls;
    uint32_t sprite0_calls;
    uint32_t apu_calls;
    uint32_t mapper_calls;
    uint64_t loop_cycles;
    uint64_t loop_max_cycles;
    uint64_t cpu_cycles;
    uint64_t bg_cycles;
    uint64_t sprite_cycles;
    uint64_t sprite0_cycles;
    uint64_t apu_cycles;
    uint64_t mapper_cycles;
} nes_core_profile_t;

static nes_core_profile_t g_nes_core_profile;

static inline uint32_t nes_core_cycles_to_us(uint64_t cycles)
{
    const uint32_t hz = SystemCoreClock ? (uint32_t)SystemCoreClock : 300000000U;
    return (uint32_t)((cycles * 1000000ULL + (hz / 2U)) / hz);
}

static inline uint32_t nes_core_pct(uint32_t used_us, uint32_t total_us)
{
    if (total_us == 0) {
        return 0;
    }
    return (uint32_t)(((uint64_t)used_us * 100ULL + (total_us / 2U)) / total_us);
}

static inline void nes_core_profile_acc(uint64_t *cycles, uint32_t *calls, uint64_t begin)
{
    *cycles += __get_rv_cycle() - begin;
    (*calls)++;
}

static void nes_core_profile_frame_done(uint32_t now_ms, uint64_t frame_begin, uint8_t drew_frame)
{
    nes_core_profile_t *prof = &g_nes_core_profile;
    const uint64_t frame_cycles = __get_rv_cycle() - frame_begin;

    if (prof->last_ms == 0) {
        prof->start_ms = now_ms;
        prof->last_ms = now_ms;
    }

    prof->frames++;
    if (drew_frame) {
        prof->draw_frames++;
    } else {
        prof->skip_frames++;
    }
    prof->loop_cycles += frame_cycles;
    if (frame_cycles > prof->loop_max_cycles) {
        prof->loop_max_cycles = frame_cycles;
    }

    const uint32_t elapsed_ms = now_ms - prof->last_ms;
    if (elapsed_ms < CONFIG_NES_PROFILE_LOG_MS) {
        return;
    }

    const uint32_t elapsed_us = elapsed_ms * 1000U;
    const uint32_t loop_avg_us = prof->frames ? nes_core_cycles_to_us(prof->loop_cycles / prof->frames) : 0;
    const uint32_t loop_max_us = nes_core_cycles_to_us(prof->loop_max_cycles);
    const uint32_t cpu_us = nes_core_cycles_to_us(prof->cpu_cycles);
    const uint32_t bg_us = nes_core_cycles_to_us(prof->bg_cycles);
    const uint32_t sprite_us = nes_core_cycles_to_us(prof->sprite_cycles);
    const uint32_t sprite0_us = nes_core_cycles_to_us(prof->sprite0_cycles);
    const uint32_t apu_us = nes_core_cycles_to_us(prof->apu_cycles);
    const uint32_t mapper_us = nes_core_cycles_to_us(prof->mapper_cycles);

    nes_log_printf("NES_CORE: t=%lums frame=%lu draw/skip=%lu/%lu loop avg/max=%lu/%luus "
                   "cpu=%luus(%lu%%,%luc) bg=%luus(%lu%%,%luc) spr=%luus(%lu%%,%luc) "
                   "s0=%luus(%lu%%,%luc) map=%luus(%lu%%,%luc) apu=%luus(%lu%%,%luc)\n",
                   (unsigned long)(now_ms - prof->start_ms),
                   (unsigned long)prof->frames,
                   (unsigned long)prof->draw_frames,
                   (unsigned long)prof->skip_frames,
                   (unsigned long)loop_avg_us,
                   (unsigned long)loop_max_us,
                   (unsigned long)cpu_us,
                   (unsigned long)nes_core_pct(cpu_us, elapsed_us),
                   (unsigned long)prof->cpu_calls,
                   (unsigned long)bg_us,
                   (unsigned long)nes_core_pct(bg_us, elapsed_us),
                   (unsigned long)prof->bg_calls,
                   (unsigned long)sprite_us,
                   (unsigned long)nes_core_pct(sprite_us, elapsed_us),
                   (unsigned long)prof->sprite_calls,
                   (unsigned long)sprite0_us,
                   (unsigned long)nes_core_pct(sprite0_us, elapsed_us),
                   (unsigned long)prof->sprite0_calls,
                   (unsigned long)mapper_us,
                   (unsigned long)nes_core_pct(mapper_us, elapsed_us),
                   (unsigned long)prof->mapper_calls,
                   (unsigned long)apu_us,
                   (unsigned long)nes_core_pct(apu_us, elapsed_us),
                   (unsigned long)prof->apu_calls);

    const uint32_t start_ms = prof->start_ms;
    nes_memset(prof, 0, sizeof(*prof));
    prof->start_ms = start_ms;
    prof->last_ms = now_ms;
}

#define NES_CORE_PROF_RUN(cycles_field, calls_field, code) do { \
    const uint64_t _nes_prof_begin = __get_rv_cycle(); \
    code \
    nes_core_profile_acc(&g_nes_core_profile.cycles_field, &g_nes_core_profile.calls_field, _nes_prof_begin); \
} while (0)
#else
#define NES_CORE_PROF_RUN(cycles_field, calls_field, code) do { code } while (0)
#endif

// https://www.nesdev.org/wiki/PPU_rendering


// static void nes_background_pattern_test(nes_t* nes){
//     nes_palette_generate(nes);
//     nes_memset(nes->nes_draw_data, nes->nes_ppu.background_palette[0], sizeof(nes_color_t) * NES_DRAW_SIZE);

//     uint8_t nametable_id = 0;
//     for (uint8_t j = 0; j < 16 * 8; j++){
//         uint16_t p = j*NES_WIDTH;
//         uint8_t tile_y = j/8;
//         uint8_t dy = j%8;
//         int8_t m = 7;
//         for (uint8_t i = 0; i < 16; i++){
//             uint8_t tile_x = i;
//             const uint8_t pattern_id = tile_y*16 + tile_x;
//             const uint8_t* bit0_p = nes->nes_ppu.pattern_table[1 ? 4 : 0] + pattern_id * 16;
//             const uint8_t* bit1_p = bit0_p + 8;
//             const uint8_t bit0 = bit0_p[dy];
//             const uint8_t bit1 = bit1_p[dy];
//             const uint8_t attribute = nes->nes_ppu.name_table[nametable_id][960 + ((tile_y >> 2) << 3) + (tile_x >> 2)];
//             const uint8_t high_bit = ((attribute >> (((tile_y & 2) << 1) | (tile_x & 2))) & 3) << 2;
//             for (; m >= 0; m--){
//                 uint8_t low_bit = ((bit0 >> m) & 0x01) | ((bit1 >> m)<<1 & 0x02);
//                 uint8_t palette_index = (high_bit & 0x0c) | low_bit;
//                 nes->nes_draw_data[p++] = nes->nes_ppu.background_palette[palette_index];
//             }
//             m = 7;
//         }
//     }
//     nes_draw(0, 0, NES_WIDTH-1, NES_HEIGHT-1, nes->nes_draw_data);
//     nes_frame(nes);
// }

#if (NES_FRAME_SKIP != 0) || (CONFIG_NES_DRAW_FPS_TARGET > 0)
#define NES_DRAW_SKIP_ENABLED 1
#else
#define NES_DRAW_SKIP_ENABLED 0
#endif

#if NES_DRAW_SKIP_ENABLED
#define NES_LOGIC_FRAME_RATE 60U

static uint8_t nes_draw_paced_frame(uint8_t target_fps)
{
    static uint8_t frame_phase;
    static uint8_t last_target_fps;
    static uint8_t draw_period;

    if (target_fps == 0U) {
        frame_phase = 0U;
        last_target_fps = 0U;
        draw_period = 0U;
        return 0U;
    }

    uint8_t period = 1U;
    if (target_fps < NES_LOGIC_FRAME_RATE) {
        period = (uint8_t)((NES_LOGIC_FRAME_RATE + (target_fps / 2U)) / target_fps);
        if (period == 0U) {
            period = 1U;
        }
    }

    if (last_target_fps != target_fps || draw_period != period) {
        frame_phase = 0U;
        last_target_fps = target_fps;
        draw_period = period;
    }

    const uint8_t draw_frame = (frame_phase == 0U) ? 1U : 0U;
    frame_phase++;

    if (frame_phase >= draw_period) {
        frame_phase = 0U;
    }

    return draw_frame;
}
#endif

void nes_run(nes_t* nes){
    NES_LOG_DEBUG("mapper:%03d\n",nes->nes_rom.mapper_number);
    NES_LOG_DEBUG("prg_rom_size:%d*16kB\n",nes->nes_rom.prg_rom_size);
    NES_LOG_DEBUG("chr_rom_size:%d*8kB\n",nes->nes_rom.chr_rom_size);
    NES_LOG_DEBUG("mirroring_type:%d\n",nes->nes_rom.mirroring_type);
    NES_LOG_DEBUG("four_screen:%d\n",nes->nes_rom.four_screen);

    nes_cpu_reset(nes);
    uint64_t frame_cnt = 0;

    while (!nes->nes_quit){
        frame_cnt++;
#if NES_DRAW_SKIP_ENABLED
        const uint8_t frame_skip = (uint8_t)nes_frame_skip_get();
        const uint8_t draw_target_fps = (uint8_t)nes_draw_target_fps_get();
        uint8_t draw_frame;
        if (draw_target_fps > 0U) {
            draw_frame = nes_draw_paced_frame(draw_target_fps);
        } else {
            nes_draw_paced_frame(0U);
            draw_frame = (uint8_t)(frame_skip == 0U || nes->nes_frame_skip_count == 0U);
        }
#else
        const uint8_t draw_frame = 1U;
#endif
#if CONFIG_NES_CORE_PROFILE_ENABLE
        const uint64_t prof_frame_begin = __get_rv_cycle();
        const uint8_t prof_draw_frame = draw_frame;
#endif
#if (NES_ENABLE_SOUND==1)
        const int no_apu_frame = nes_diag_no_apu_frame();
#endif
        const int no_ppu_render = nes_diag_no_ppu_render();
        if (draw_frame) {
            nes_palette_generate(nes);
        }
        if (nes->nes_ppu.MASK_b == 0){
            if (draw_frame) {
                nes_memset(nes->nes_draw_data, nes->nes_ppu.background_palette[0], sizeof(nes_color_t) * NES_DRAW_SIZE);
            }
        }
#if (NES_ENABLE_SOUND==1)
        if (!no_apu_frame) {
            NES_CORE_PROF_RUN(apu_cycles, apu_calls, {
                nes_apu_frame(nes);
            });
        }
#endif
        for(nes->scanline = 0; nes->scanline < NES_HEIGHT; nes->scanline++) {
            if (nes->nes_ppu.MASK_b && !no_ppu_render){
                if (draw_frame) {
                    if (nes->nes_mapper.mapper_render_screen) {
                        NES_CORE_PROF_RUN(mapper_cycles, mapper_calls, {
                            nes->nes_mapper.mapper_render_screen(nes, 1);
                        });
                    }
#if (NES_RAM_LACK == 1)
                NES_CORE_PROF_RUN(bg_cycles, bg_calls, {
                    nes_render_background_line(nes, nes->scanline, nes->nes_draw_data + nes->scanline%(NES_HEIGHT/2) * NES_WIDTH);
                });
#else
                NES_CORE_PROF_RUN(bg_cycles, bg_calls, {
                    nes_render_background_line(nes, nes->scanline, nes->nes_draw_data + nes->scanline * NES_WIDTH);
                });
#endif
                }
            }
            if (nes->nes_ppu.MASK_s && !no_ppu_render){
#if CONFIG_NES_FAST_SKIP_SPRITES && NES_DRAW_SKIP_ENABLED
                if (draw_frame) {
#endif
                if (nes->nes_mapper.mapper_render_screen) {
                    NES_CORE_PROF_RUN(mapper_cycles, mapper_calls, {
                        nes->nes_mapper.mapper_render_screen(nes, 0);
                    });
                }
#if (NES_RAM_LACK == 1)
                NES_CORE_PROF_RUN(sprite_cycles, sprite_calls, {
                    nes_render_sprite_line(nes, nes->scanline,nes->nes_draw_data + nes->scanline%(NES_HEIGHT/2) * NES_WIDTH);
                });
#else
                NES_CORE_PROF_RUN(sprite_cycles, sprite_calls, {
                    nes_render_sprite_line(nes, nes-> scanline,nes->nes_draw_data + nes->scanline * NES_WIDTH);
                });
#endif
#if CONFIG_NES_FAST_SKIP_SPRITES && NES_DRAW_SKIP_ENABLED
                } else {
                    NES_CORE_PROF_RUN(sprite0_cycles, sprite0_calls, {
                        nes_check_sprite0_hit_line(nes, nes->scanline);
                    });
                }
#endif
            }
#if CONFIG_NES_FAST_SCANLINE_CPU
            NES_CORE_PROF_RUN(cpu_cycles, cpu_calls, {
                nes_opcode(nes,NES_PPU_CPU_CLOCKS);
            });
#else
            NES_CORE_PROF_RUN(cpu_cycles, cpu_calls, {
                nes_opcode(nes,85);
            });
#endif
            if (nes->nes_ppu.MASK_b){
                if ((nes->nes_ppu.v.fine_y) < 7) {
                    nes->nes_ppu.v.fine_y++;
                }else {
                    nes->nes_ppu.v.fine_y = 0;
                    uint8_t y = (uint8_t)(nes->nes_ppu.v.coarse_y);
                    if (y == 29) {
                        y = 0;
                        nes->nes_ppu.v_reg ^= 0x0800;
                    }else if (y == 31) {
                        y = 0;
                    }else {
                        y++;
                    }
                    nes->nes_ppu.v.coarse_y = y;
                }
                nes->nes_ppu.v_reg = (nes->nes_ppu.v_reg & (uint16_t)0xFBE0) | (nes->nes_ppu.t_reg & (uint16_t)0x041F);
            }
#if !CONFIG_NES_FAST_SCANLINE_CPU
            NES_CORE_PROF_RUN(cpu_cycles, cpu_calls, {
                nes_opcode(nes,NES_PPU_CPU_CLOCKS-85);
            });
#endif
            if (nes->nes_mapper.mapper_hsync) {
                NES_CORE_PROF_RUN(mapper_cycles, mapper_calls, {
                    nes->nes_mapper.mapper_hsync(nes);
                });
            }
#if (NES_ENABLE_SOUND==1)
            if (!no_apu_frame && nes->scanline % 66 == 65) {
                NES_CORE_PROF_RUN(apu_cycles, apu_calls, {
                    nes_apu_frame(nes);
                });
            }
#endif
#if (NES_RAM_LACK == 1)
            if (draw_frame) {
                if (nes->scanline == NES_HEIGHT/2-1){
                    nes_draw(0, 0, NES_WIDTH-1, NES_HEIGHT/2-1, nes->nes_draw_data);
                }else if(nes->scanline == NES_HEIGHT-1){
                    nes_draw(0, NES_HEIGHT/2, NES_WIDTH-1, NES_HEIGHT-1, nes->nes_draw_data);
                }
            }
#endif
        }
#if (NES_RAM_LACK == 0)
        if (draw_frame) {
            if (!no_ppu_render) {
                nes_draw(0, 0, NES_WIDTH-1, NES_HEIGHT-1, nes->nes_draw_data);
            }
        }
#endif
        NES_CORE_PROF_RUN(cpu_cycles, cpu_calls, {
            nes_opcode(nes,NES_PPU_CPU_CLOCKS); //240 Post-render line
        });

        nes->nes_ppu.STATUS_V = 1;
        if (nes->nes_mapper.mapper_vsync) {
            NES_CORE_PROF_RUN(mapper_cycles, mapper_calls, {
                nes->nes_mapper.mapper_vsync(nes);
            });
        }
        if (nes->nes_ppu.CTRL_V) {
            nes->nes_cpu.irq_nmi=1;
        }

        for(uint8_t i = 0; i < 20; i++){
            NES_CORE_PROF_RUN(cpu_cycles, cpu_calls, {
                nes_opcode(nes,NES_PPU_CPU_CLOCKS);
            });
        }
        nes->nes_ppu.ppu_status = 0;
        NES_CORE_PROF_RUN(cpu_cycles, cpu_calls, {
            nes_opcode(nes,NES_PPU_CPU_CLOCKS);
        });

        if (nes->nes_ppu.MASK_b){
            nes->nes_ppu.v_reg = (nes->nes_ppu.v_reg & (uint16_t)0x841F) | (nes->nes_ppu.t_reg & (uint16_t)0x7BE0);
        }
        nes_frame(nes);
#if CONFIG_NES_CORE_PROFILE_ENABLE
        nes_core_profile_frame_done(nes_get_ms(), prof_frame_begin, prof_draw_frame);
#endif
#if NES_DRAW_SKIP_ENABLED
        if (draw_target_fps > 0U) {
            nes->nes_frame_skip_count = 0;
        } else if (frame_skip == 0U) {
            nes->nes_frame_skip_count = 0;
        } else if (++nes->nes_frame_skip_count > frame_skip) {
            nes->nes_frame_skip_count = 0;
        }
#endif
    }
}
