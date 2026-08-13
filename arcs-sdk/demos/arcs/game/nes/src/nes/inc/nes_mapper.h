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
#pragma once

#ifdef __cplusplus
    extern "C" {
#endif

struct nes;
typedef struct nes nes_t;

/* https://www.nesdev.org/wiki/Mapper   */
typedef struct {
    void (*mapper_init)(nes_t* nes);
    void (*mapper_deinit)(nes_t* nes);
    void (*mapper_write)(nes_t* nes, uint16_t write_addr, uint8_t data);
    void (*mapper_sram)(nes_t* nes, uint16_t write_addr, uint8_t data);
    uint8_t (*mapper_read_sram)(nes_t* nes, uint16_t read_addr);
    void (*mapper_apu)(nes_t* nes, uint16_t write_addr, uint8_t data);
    uint8_t (*mapper_read_apu)(nes_t* nes, uint16_t read_addr);
    /* Callback at VSync */
    void (*mapper_vsync)(nes_t* nes);
    /* Callback at HSync */
    void (*mapper_hsync)(nes_t* nes);
    /* Callback at PPU read/write */
    void (*mapper_ppu)(nes_t* nes, uint16_t write_addr);
    /* Callback at Rendering Screen 1:BG, 0:Sprite */
    void (*mapper_render_screen)(nes_t* nes, uint8_t mode);
    uint8_t mapper_banked_chr;
    uint8_t* mapper_sram_direct_bank;
    void* mapper_register;
    void* mapper_data;
} nes_mapper_t;

/* prg rom */
uint16_t nes_prgrom_8k_count(nes_t* nes);
void nes_load_prgrom_8k(nes_t* nes,uint8_t des, uint16_t src);
void nes_load_prgrom_8k_wrap(nes_t* nes, uint8_t des, uint16_t src);
void nes_load_prgrom_16k(nes_t* nes,uint8_t des, uint16_t src);
void nes_load_prgrom_32k(nes_t* nes,uint8_t des, uint16_t src);

/* chr rom */
uint16_t nes_chrrom_1k_count(nes_t* nes);
void nes_load_chrrom_1k(nes_t* nes,uint8_t des, uint16_t src);
void nes_load_chrrom_1k_wrap(nes_t* nes, uint8_t des, uint16_t src);
void nes_load_chrrom_4k(nes_t* nes,uint8_t des, uint16_t src);
void nes_load_chrrom_8k(nes_t* nes,uint8_t des, uint16_t src);

/* mapper */
int nes_load_mapper(nes_t* nes);
int nes_mapper_infones_init(nes_t* nes);


#ifdef __cplusplus
    }
#endif
