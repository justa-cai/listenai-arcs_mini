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

#define NES_CPU_INLINE_TEXT __attribute__((always_inline))

#if CONFIG_NES_FAST_INLINE_CPU_HELPERS
#define NES_CPU_HOT_TEXT NES_CPU_INLINE_TEXT
#elif CONFIG_NES_FAST_LOCK_CPU_HELPERS
#define NES_CPU_HOT_TEXT __attribute__((section(".fast_text.nes_cpu")))
#else
#define NES_CPU_HOT_TEXT
#endif

#define NES_CPU_FETCH_TEXT NES_CPU_INLINE_TEXT
#define NES_CPU_BRANCH_TEXT NES_CPU_INLINE_TEXT

static inline uint8_t nes_read_joypad(nes_t* nes,uint16_t address){
    uint8_t state = 0;
    if (address == 0x4016){
        state = (nes->nes_cpu.joypad.joypad & (0x8000 >> (nes->nes_cpu.joypad.offset1 & nes->nes_cpu.joypad.mask))) ? 1 : 0;
        nes->nes_cpu.joypad.offset1++;
    }else if(address == 0x4017){
        state = (nes->nes_cpu.joypad.joypad & (0x80 >> (nes->nes_cpu.joypad.offset2 & nes->nes_cpu.joypad.mask))) ? 1 : 0;
        nes->nes_cpu.joypad.offset2++;
    }
    // NES_LOG_DEBUG("nes_read joypad %04X %d %02X %d\n",address,nes->nes_cpu.joypad.mask,nes->nes_cpu.joypad.joypad,state);
    return state;
}

static inline void nes_write_joypad(nes_t* nes,uint8_t data){
    nes->nes_cpu.joypad.mask = (data & 1)?0x00:0x07;
    if (data & 1)
        nes->nes_cpu.joypad.offset1 = nes->nes_cpu.joypad.offset2 = 0;
    // NES_LOG_DEBUG("nes_write joypad %04X %02X %d\n",address,data,nes->nes_cpu.joypad.mask);
}

static inline uint8_t NES_CPU_HOT_TEXT nes_read_cpu(nes_t* nes,uint16_t address);

static inline uint8_t NES_CPU_FETCH_TEXT nes_read_prg(nes_t* nes, uint16_t address)
{
    return nes->nes_cpu.prg_banks[(address >> 13) - 4U][address & (uint16_t)0x1fff];
}

static inline uint8_t NES_CPU_FETCH_TEXT nes_fetch_cpu(nes_t* nes)
{
    const uint16_t address = nes->nes_cpu.PC++;
    if (address >= 0x8000U) {
        return nes_read_prg(nes, address);
    }
    return nes_read_cpu(nes, address);
}

static inline uint16_t NES_CPU_FETCH_TEXT nes_fetchw_cpu(nes_t* nes)
{
    const uint16_t address = nes->nes_cpu.PC;
    if (address >= 0x8000U && address != 0xffffU) {
        nes->nes_cpu.PC = (uint16_t)(address + 2U);
        return (uint16_t)nes_read_prg(nes, address) |
               ((uint16_t)nes_read_prg(nes, (uint16_t)(address + 1U)) << 8);
    }

    const uint8_t low_byte = nes_fetch_cpu(nes);
    const uint16_t high_byte = (uint16_t)nes_fetch_cpu(nes) << 8;
    return high_byte | low_byte;
}

static inline uint8_t NES_CPU_HOT_TEXT nes_read_cpu(nes_t* nes,uint16_t address){
    if (address >= 0x8000U) {
        return nes_read_prg(nes, address);
    }
    if (address < 0x2000U) {
        return nes->nes_cpu.cpu_ram[address & (uint16_t)0x07ff];
    }
    if (address < 0x4000U) {
        return nes_read_ppu_register(nes,address);
    }
    if (address < 0x6000U) {
        if (address == 0x4016 || address == 0x4017) { // I/O registers
            return nes_read_joypad(nes, address);
        }
        if (address < 0x4016){                 // APU registers
#if (NES_ENABLE_SOUND == 1)
            return nes_read_apu_register(nes, address);
#endif
        } else if (nes->nes_mapper.mapper_read_apu) {
            return nes->nes_mapper.mapper_read_apu(nes, address);
        } else {
            NES_LOG_DEBUG("nes_read address %04X not support \n",address);
        }
        return 0;
    }
    if (address < 0x8000U) { //$6000-$7FFF SRAM
        uint8_t* const sram_bank = nes->nes_mapper.mapper_sram_direct_bank;
        if (sram_bank) {
            return sram_bank[address & (uint16_t)0x1fff];
        }
        if (nes->nes_mapper.mapper_read_sram) {
            return nes->nes_mapper.mapper_read_sram(nes, address);
        }
#if (NES_USE_SRAM == 1)
        return nes->nes_rom.sram[address & (uint16_t)0x1fff];
#else
        return 0;
#endif
    }
    return 0;
}

static inline uint8_t NES_CPU_FETCH_TEXT nes_read_data_fast(nes_t* nes, uint16_t address)
{
    if (address >= 0x8000U) {
        return nes_read_prg(nes, address);
    }
    if (address < 0x2000U) {
        return nes->nes_cpu.cpu_ram[address & (uint16_t)0x07ff];
    }
    if (address >= 0x6000U && address < 0x8000U) {
        uint8_t* const sram_bank = nes->nes_mapper.mapper_sram_direct_bank;
        if (sram_bank) {
            return sram_bank[address & (uint16_t)0x1fff];
        }
    }
    return nes_read_cpu(nes, address);
}

static inline uint16_t nes_readw_cpu(nes_t* nes,uint16_t address){
    return nes_read_cpu(nes,address) | (uint16_t)(nes_read_cpu(nes,address + 1)) << 8;
}

static inline uint8_t* nes_get_dma_address(nes_t* nes,uint8_t data) {
    switch (data >> 5){
        case 0:
            return nes->nes_cpu.cpu_ram + ((uint16_t)(data & 0x07) << 8);
        case 4: case 5: case 6: case 7:// 高一位为1, [$8000, $10000) PRG-ROM
            return nes->nes_cpu.prg_banks[(data >> 4)&0x03] + ((uint16_t)(data & 0x0f) << 8);
        default:
            NES_LOG_DEBUG("nes_get_dma_address error %02X\n",data);
            return NULL;
    }
}

static inline void NES_CPU_HOT_TEXT nes_write_cpu(nes_t* nes,uint16_t address, uint8_t data){
    if (address < 0x2000U) { //$0000-$1FFF 2KB internal RAM + Mirrors of $0000-$07FF
        nes->nes_cpu.cpu_ram[address & (uint16_t)0x07ff] = data;
        return;
    }
    if (address < 0x4000U) { //$2000-$3FFF NES PPU registers + Mirrors of $2000-2007
        nes_write_ppu_register(nes,address, data);
        return;
    }
    if (address < 0x6000U) { //$4000-$5FFF NES APU and I/O registers
        if (address == 0x4016)
            nes_write_joypad(nes,data);
        else if (address == 0x4014){
            // NES_LOG_DEBUG("nes_write DMA data:0x%02X oam_addr:0x%02X\n",data,nes->nes_ppu.oam_addr);
            if (nes->nes_ppu.oam_addr) {
                uint8_t* dst = nes->nes_ppu.oam_data;
                const uint8_t len = nes->nes_ppu.oam_addr;
                const uint8_t* src = nes_get_dma_address(nes,data);
                nes_memcpy(dst, src + len, len);
                nes_memcpy(dst + len, src, NES_PPU_OAM_SIZE - len);
            } else {
                nes_memcpy(nes->nes_ppu.oam_data, nes_get_dma_address(nes,data), NES_PPU_OAM_SIZE);
            }
            nes->nes_cpu.cycles += 513;
            nes->nes_cpu.cycles += nes->nes_cpu.cycles & 1; //奇数周期需要多sleep 1个CPU时钟周期
        }else if (address < 0x4016 || address == 0x4017){
#if (NES_ENABLE_SOUND == 1)
            nes_write_apu_register(nes, address,data);
#endif
        }else if (nes->nes_mapper.mapper_apu){
            nes->nes_mapper.mapper_apu(nes, address, data);
        }else{
            NES_LOG_DEBUG("nes_write address %04X not support\n",address);
        }
        return;
    }
    if (address < 0x8000U) { //$6000-$7FFF SRAM
        uint8_t* const sram_bank = nes->nes_mapper.mapper_sram_direct_bank;
        if (sram_bank) {
            sram_bank[address & (uint16_t)0x1fff] = data;
            return;
        }
        if (nes->nes_mapper.mapper_sram) {
            nes->nes_mapper.mapper_sram(nes, address, data);
        }
#if (NES_USE_SRAM == 1)
        nes->nes_rom.sram[address & (uint16_t)0x1fff] = data;
#endif
        return;
    }
    if (nes->nes_mapper.mapper_write) {
        nes->nes_mapper.mapper_write(nes, address, data); //$8000-$FFFF PRG-ROM
    }
}

static inline void NES_CPU_FETCH_TEXT nes_write_data_fast(nes_t* nes, uint16_t address, uint8_t data)
{
    if (address < 0x2000U) {
        nes->nes_cpu.cpu_ram[address & (uint16_t)0x07ff] = data;
        return;
    }
    if (address >= 0x6000U && address < 0x8000U) {
        uint8_t* const sram_bank = nes->nes_mapper.mapper_sram_direct_bank;
        if (sram_bank) {
            sram_bank[address & (uint16_t)0x1fff] = data;
            return;
        }
    }
    nes_write_cpu(nes, address, data);
}

#define NES_FLAG_C      (1 << 0)
#define NES_FLAG_Z      (1 << 1)
#define NES_FLAG_I      (1 << 2)
#define NES_FLAG_D      (1 << 3)
#define NES_FLAG_B      (1 << 4)
#define NES_FLAG_U      (1 << 5)
#define NES_FLAG_V      (1 << 6)
#define NES_FLAG_N      (1 << 7)

#define NES_CPU_P       (nes->nes_cpu.P)

#define NES_CPU_C       (NES_CPU_P & (uint8_t)NES_FLAG_C)
#define NES_CPU_Z       (NES_CPU_P & (uint8_t)NES_FLAG_Z)
#define NES_CPU_I       (NES_CPU_P & (uint8_t)NES_FLAG_I)
#define NES_CPU_D       (NES_CPU_P & (uint8_t)NES_FLAG_D)
#define NES_CPU_B       (NES_CPU_P & (uint8_t)NES_FLAG_B)
#define NES_CPU_V       (NES_CPU_P & (uint8_t)NES_FLAG_V)
#define NES_CPU_N       (NES_CPU_P & (uint8_t)NES_FLAG_N)
// SET
#define NES_C_SET       (NES_CPU_P |= (uint8_t)NES_FLAG_C)
#define NES_Z_SET       (NES_CPU_P |= (uint8_t)NES_FLAG_Z)
#define NES_I_SET       (NES_CPU_P |= (uint8_t)NES_FLAG_I)
#define NES_D_SET       (NES_CPU_P |= (uint8_t)NES_FLAG_D)
#define NES_B_SET       (NES_CPU_P |= (uint8_t)NES_FLAG_B)
#define NES_U_SET       (NES_CPU_P |= (uint8_t)NES_FLAG_U)
#define NES_V_SET       (NES_CPU_P |= (uint8_t)NES_FLAG_V)
#define NES_N_SET       (NES_CPU_P |= (uint8_t)NES_FLAG_N)
// CLR
#define NES_C_CLR       (NES_CPU_P &= ~(uint8_t)NES_FLAG_C)
#define NES_Z_CLR       (NES_CPU_P &= ~(uint8_t)NES_FLAG_Z)
#define NES_I_CLR       (NES_CPU_P &= ~(uint8_t)NES_FLAG_I)
#define NES_D_CLR       (NES_CPU_P &= ~(uint8_t)NES_FLAG_D)
#define NES_B_CLR       (NES_CPU_P &= ~(uint8_t)NES_FLAG_B)
#define NES_V_CLR       (NES_CPU_P &= ~(uint8_t)NES_FLAG_V)
#define NES_N_CLR       (NES_CPU_P &= ~(uint8_t)NES_FLAG_N)

// 状态寄存器检查位
#define NES_CHECK_N(x) do { \
    const uint8_t _nes_check_n_value = (uint8_t)(x); \
    NES_CPU_P = (uint8_t)((NES_CPU_P & (uint8_t)~NES_FLAG_N) | (_nes_check_n_value & (uint8_t)NES_FLAG_N)); \
} while (0)
#define NES_CHECK_Z(x) do { \
    const uint8_t _nes_check_z_value = (uint8_t)(x); \
    uint8_t _nes_check_z_p = NES_CPU_P & (uint8_t)~NES_FLAG_Z; \
    if (_nes_check_z_value == 0U) { \
        _nes_check_z_p |= (uint8_t)NES_FLAG_Z; \
    } \
    NES_CPU_P = _nes_check_z_p; \
} while (0)
#define NES_CHECK_NZ(x) do { \
    const uint8_t _nes_check_nz_value = (uint8_t)(x); \
    uint8_t _nes_check_nz_p = NES_CPU_P & (uint8_t)~(NES_FLAG_N | NES_FLAG_Z); \
    if (_nes_check_nz_value == 0U) { \
        _nes_check_nz_p |= (uint8_t)NES_FLAG_Z; \
    } \
    _nes_check_nz_p |= _nes_check_nz_value & (uint8_t)NES_FLAG_N; \
    NES_CPU_P = _nes_check_nz_p; \
} while (0)

static inline void NES_CPU_HOT_TEXT nes_set_nz_fast(nes_t* nes, uint8_t value)
{
    uint8_t p = NES_CPU_P & (uint8_t)~(NES_FLAG_N | NES_FLAG_Z);
    if (value == 0U) {
        p |= (uint8_t)NES_FLAG_Z;
    }
    p |= value & (uint8_t)NES_FLAG_N;
    NES_CPU_P = p;
}

static inline void NES_CPU_HOT_TEXT nes_cmp_data_fast(nes_t* nes, uint8_t reg, uint8_t data)
{
    const uint16_t value = (uint16_t)reg - (uint16_t)data;
    uint8_t p = NES_CPU_P & (uint8_t)~(NES_FLAG_N | NES_FLAG_Z | NES_FLAG_C);
    if ((uint8_t)value == 0U) {
        p |= (uint8_t)NES_FLAG_Z;
    }
    if (value < 0x100U) {
        p |= (uint8_t)NES_FLAG_C;
    }
    p |= (uint8_t)value & (uint8_t)NES_FLAG_N;
    NES_CPU_P = p;
}

static inline void NES_CPU_HOT_TEXT nes_lsr_a_fast(nes_t* nes)
{
    const uint8_t old_a = nes->nes_cpu.A;
    const uint8_t new_a = old_a >> 1;
    uint8_t p = NES_CPU_P & (uint8_t)~(NES_FLAG_N | NES_FLAG_Z | NES_FLAG_C);
    p |= old_a & (uint8_t)NES_FLAG_C;
    if (new_a == 0U) {
        p |= (uint8_t)NES_FLAG_Z;
    }
    nes->nes_cpu.A = new_a;
    NES_CPU_P = p;
}

static inline void NES_CPU_HOT_TEXT nes_bit_data_fast(nes_t* nes, uint8_t data)
{
    uint8_t p = NES_CPU_P & (uint8_t)~(NES_FLAG_N | NES_FLAG_V | NES_FLAG_Z);
    p |= data & (uint8_t)(NES_FLAG_N | NES_FLAG_V);
    if ((nes->nes_cpu.A & data) == 0U) {
        p |= (uint8_t)NES_FLAG_Z;
    }
    NES_CPU_P = p;
}

static inline void NES_CPU_HOT_TEXT nes_lda_data_fast(nes_t* nes, uint8_t data)
{
    nes->nes_cpu.A = data;
    nes_set_nz_fast(nes, data);
}

// 入栈
#define NES_PUSH(nes,data)      (nes->nes_cpu.cpu_ram + 0x100)[nes->nes_cpu.SP--] = (uint8_t)(data)
#define NES_PUSHW(nes,data)     NES_PUSH(nes, ((data) >> 8) ); NES_PUSH(nes, ((data) & 0xff))
// 出栈
#define NES_POP(nes)            ((nes->nes_cpu.cpu_ram + 0x100)[++nes->nes_cpu.SP])
#define NES_POPW(nes)           ((uint16_t)NES_POP(nes)|(uint16_t)(NES_POP(nes) << 8))


static inline void nes_dummy_read(nes_t* nes){
    nes_read_cpu(nes,nes->nes_cpu.PC);
}

// https://www.nesdev.org/6502_cn.txt

/* 
    Adressing modes:
    https://www.nesdev.org/wiki/CPU_addressing_modes
*/

/*
    Implicit:Instructions like RTS or CLC have no address operand, the destination of results are implied.
*/

/*
    #v:Immediate: Uses the 8-bit operand itself as the value for the operation, 
                    rather than fetching a value from a memory address.
*/
static inline uint16_t nes_imm(nes_t* nes){
    return nes->nes_cpu.PC++;
}

/*
    label:Relative::Branch instructions (e.g. BEQ, BCS) have a relative addressing mode 
                    that specifies an 8-bit signed offset relative to the current PC.
*/
static inline uint16_t nes_rel(nes_t* nes){
    const int8_t data = (int8_t)nes_fetch_cpu(nes);
    return nes->nes_cpu.PC + data;
}

/*
    a:Absolute::Fetches the value from a 16-bit address anywhere in memory.
*/
static inline uint16_t nes_abs(nes_t* nes){
    return nes_fetchw_cpu(nes);
}

/*
    a,x:Absolute indexed:val = PEEK(arg + X)
*/
static inline uint16_t nes_abx(nes_t* nes){
    const uint16_t base_address = nes_abs(nes);
    const uint16_t address = base_address + nes->nes_cpu.X;
    if ((address>>8) != (base_address>>8))nes->nes_cpu.cycles++;
    return address;
}

static inline uint16_t nes_abx2(nes_t* nes){
    const uint16_t base_address = nes_abs(nes);
    return base_address + nes->nes_cpu.X;
}

/*
    a,y:Absolute indexed:val = PEEK(arg + Y)
*/
static inline uint16_t nes_aby(nes_t* nes){
    const uint16_t base_address = nes_abs(nes);
    const uint16_t address = base_address + nes->nes_cpu.Y;
    if ((address>>8) != (base_address>>8))nes->nes_cpu.cycles++;
    return address;
}

static inline uint16_t nes_aby2(nes_t* nes){
    const uint16_t base_address = nes_abs(nes);
    return base_address + nes->nes_cpu.Y;
}

/*
    d:Zero page:Fetches the value from an 8-bit address on the zero page.
*/
static inline uint16_t nes_zp(nes_t* nes){
    // return nes->nes_cpu.cpu_ram[ nes->nes_cpu.PC++ & (uint16_t)0x07ff];
    return nes_fetch_cpu(nes);
}

/*
    d,x:Zero page indexed:val = PEEK((arg + X) % 256)
*/
static inline uint16_t nes_zpx(nes_t* nes){
    return (nes_zp(nes) + nes->nes_cpu.X) & 0x00FF;
}

/*
    d,y:Zero page indexed:val = PEEK((arg + Y) % 256)
*/
static inline uint16_t nes_zpy(nes_t* nes){
    return (nes_zp(nes) + nes->nes_cpu.Y) & 0x00FF;
}

/*
    (d,x):Indexed indirect:val = PEEK(PEEK((arg + X) % 256) + PEEK((arg + X + 1) % 256) * 256)
*/
static inline uint16_t nes_izx(nes_t* nes){
    const uint8_t address = (uint8_t)nes_zp(nes) + nes->nes_cpu.X;
    return nes->nes_cpu.cpu_ram[address] | (uint16_t)nes->nes_cpu.cpu_ram[(uint8_t)(address + 1)] << 8;
}

/*
    (d),y:Indexed indirect:val = PEEK(PEEK(arg) + PEEK((arg + 1) % 256) * 256 + Y)
*/
static inline uint16_t nes_izy(nes_t* nes){
    const uint8_t value = (uint8_t)nes_zp(nes);
    const uint16_t address = nes->nes_cpu.cpu_ram[value] | (uint16_t)nes->nes_cpu.cpu_ram[(uint8_t)(value + 1)] << 8;
    if ((address>>8) != ((address+nes->nes_cpu.Y)>>8))nes->nes_cpu.cycles++;
    return address + nes->nes_cpu.Y;
}

static inline uint16_t nes_izy2(nes_t* nes){
    const uint8_t value = (uint8_t)nes_zp(nes);
    const uint16_t address = nes->nes_cpu.cpu_ram[value] | (uint16_t)nes->nes_cpu.cpu_ram[(uint8_t)(value + 1)] << 8;
    return address + nes->nes_cpu.Y;
}

/*
    (a):Indirect:The JMP instruction has a special indirect addressing mode 
                    that can jump to the address stored in a 16-bit pointer anywhere in memory.
*/
static inline uint16_t nes_ind(nes_t* nes){
    // 6502 BUG
    const uint16_t address = nes_abs(nes);
    return nes_read_cpu(nes,address) | (uint16_t)(nes_read_cpu(nes,(uint16_t)((address & (uint16_t)0xFF00)|((address + 1) & (uint16_t)0x00FF)))) << 8;
}

/* 6502/6510/8500/8502 Opcode matrix: https://www.oxyron.de/html/opcodes02.html */
/* Logical and arithmetic commands: */

/* 
    https://www.nesdev.org/wiki/Instruction_reference#ORA
    A :=A or {adr}
    N  V  U  B  D  I  Z  C
    *                 *
*/
static inline void nes_ora_data(nes_t* nes, const uint8_t data){
    nes->nes_cpu.A |= data;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

static inline void nes_ora(nes_t* nes, const uint16_t address){
    nes_ora_data(nes, nes_read_cpu(nes,address));
}

/* 
    https://www.nesdev.org/wiki/Instruction_reference#AND
    A := A & {adr}
    N  V  U  B  D  I  Z  C
    *                 *
*/
static inline void nes_and_data(nes_t* nes, const uint8_t data){
    nes->nes_cpu.A &= data;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

static inline void nes_and(nes_t* nes, const uint16_t address){
    nes_and_data(nes, nes_read_cpu(nes, address));
}

/* 
    https://www.nesdev.org/wiki/Instruction_reference#EOR
    A := A exor {adr}
    N  V  U  B  D  I  Z  C
    *                 *
*/
static inline void nes_eor_data(nes_t* nes, const uint8_t data){
    nes->nes_cpu.A ^= data;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

static inline void nes_eor(nes_t* nes, const uint16_t address){
    nes_eor_data(nes, nes_read_cpu(nes, address));
}

/* 
    https://www.nesdev.org/wiki/Instruction_reference#ADC
    A:=A+{adr}
    N  V  U  B  D  I  Z  C
    *  *              *  *
*/
static inline void nes_adc_data(nes_t* nes, const uint8_t src){
    const uint16_t result16 = nes->nes_cpu.A + src + NES_CPU_C;
    nes->nes_cpu.C = result16 >> 8;
    const uint8_t result8 = (uint8_t)result16;

    if (!((nes->nes_cpu.A ^ src) & 0x80) && ((nes->nes_cpu.A ^ result8) & 0x80)) nes->nes_cpu.V = 1;
    else nes->nes_cpu.V = 0;

    nes->nes_cpu.A = result8;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

static inline void nes_adc(nes_t* nes, const uint16_t address){
    nes_adc_data(nes, nes_read_cpu(nes, address));
}

/* 
    https://www.nesdev.org/wiki/Instruction_reference#SBC
    A:=A-{adr}
    N  V  U  B  D  I  Z  C
    *  *              *  *
*/
static inline void nes_sbc_data(nes_t* nes, const uint8_t src){
    const uint16_t result16 = nes->nes_cpu.A - src - !nes->nes_cpu.C;
    nes->nes_cpu.C = !(result16 >> 8);
    const uint8_t result8 = (uint8_t)result16;

    if (((nes->nes_cpu.A ^ src) & 0x80) && ((nes->nes_cpu.A ^ result8) & 0x80)) nes->nes_cpu.V = 1;
    else nes->nes_cpu.V = 0;

    nes->nes_cpu.A = result8;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

static inline void nes_sbc(nes_t* nes, const uint16_t address){
    nes_sbc_data(nes, nes_read_cpu(nes, address));
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#CMP
    A-{adr}
    N  V  U  B  D  I  Z  C
    *                 *  *
*/
static inline void nes_cmp_data(nes_t* nes, const uint8_t data){
    nes_cmp_data_fast(nes, nes->nes_cpu.A, data);
}

static inline void nes_cmp(nes_t* nes, const uint16_t address){
    nes_cmp_data(nes, nes_read_cpu(nes, address));
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#CPX
    X-{adr}
    N  V  U  B  D  I  Z  C
    *                 *  *
*/
static inline void nes_cpx_data(nes_t* nes, const uint8_t data){
    nes_cmp_data_fast(nes, nes->nes_cpu.X, data);
}

static inline void nes_cpx(nes_t* nes, const uint16_t address){
    nes_cpx_data(nes, nes_read_cpu(nes, address));
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#CPY
    Y-{adr}
    N  V  U  B  D  I  Z  C
    *                 *  *
*/
static inline void nes_cpy_data(nes_t* nes, const uint8_t data){
    nes_cmp_data_fast(nes, nes->nes_cpu.Y, data);
}

static inline void nes_cpy(nes_t* nes, const uint16_t address){
    nes_cpy_data(nes, nes_read_cpu(nes, address));
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#DEC
    {adr}:={adr}-1
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_dec(nes_t* nes, const uint16_t address){
    uint8_t data = nes_read_cpu(nes, address)-1;
    nes_write_cpu(nes, address, data);
    NES_CHECK_NZ(data);
}

static inline void nes_dec_ram(nes_t* nes, uint8_t *data_ptr){
    const uint8_t data = (uint8_t)(*data_ptr - 1U);
    *data_ptr = data;
    NES_CHECK_NZ(data);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#DEX
    X:=X-1
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_dex(nes_t* nes, const uint16_t address){
    (void)address;
    nes->nes_cpu.X--;
    NES_CHECK_NZ(nes->nes_cpu.X);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#DEY
    Y:=Y-1
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_dey(nes_t* nes, const uint16_t address){
    (void)address;
    nes->nes_cpu.Y--;
    NES_CHECK_NZ(nes->nes_cpu.Y);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#INC
    {adr}:={adr}+1
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_inc(nes_t* nes, const uint16_t address){
    uint8_t data = nes_read_cpu(nes, address)+1;
    nes_write_cpu(nes, address, data);
    NES_CHECK_NZ(data);
}

static inline void nes_inc_ram(nes_t* nes, uint8_t *data_ptr){
    const uint8_t data = (uint8_t)(*data_ptr + 1U);
    *data_ptr = data;
    NES_CHECK_NZ(data);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#INX
    X:=X+1
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_inx(nes_t* nes, const uint16_t address){
    (void)address;
    nes->nes_cpu.X++;
    NES_CHECK_NZ(nes->nes_cpu.X);
}

/*  
    https://www.nesdev.org/wiki/Instruction_reference#INY
    Y:=Y+1
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_iny(nes_t* nes, const uint16_t address){
    (void)address;
    nes->nes_cpu.Y++;
    NES_CHECK_NZ(nes->nes_cpu.Y);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#ASL
    {adr}:={adr}*2
    N  V  U  B  D  I  Z  C
    *                 *  *
*/
static inline void nes_asl(nes_t* nes, const uint16_t address){
        uint8_t data = nes_read_cpu(nes, address);
        nes->nes_cpu.C = data >> 7;
        data <<= 1;
        nes_write_cpu(nes, address, data);
        NES_CHECK_NZ(data);
}

static inline void nes_asl_ram(nes_t* nes, uint8_t *data_ptr){
    uint8_t data = *data_ptr;
    nes->nes_cpu.C = data >> 7;
    data <<= 1;
    *data_ptr = data;
    NES_CHECK_NZ(data);
}

static inline void nes_asla(nes_t* nes, const uint16_t address){
    (void)address;
    nes->nes_cpu.C = nes->nes_cpu.A >> 7;
    nes->nes_cpu.A <<= 1;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#ROL
    {adr}:={adr}*2+C
    N  V  U  B  D  I  Z  C
    *                 *  *
*/
static inline void nes_rol(nes_t* nes, const uint16_t address){
    uint16_t data = nes_read_cpu(nes, address);
    data <<= 1;
    data |= nes->nes_cpu.C;
    nes->nes_cpu.C = (uint8_t)(data>>8);
    nes_write_cpu(nes, address, (uint8_t)data);
    NES_CHECK_NZ(data);
}

static inline void nes_rol_ram(nes_t* nes, uint8_t *data_ptr){
    uint16_t data = *data_ptr;
    data <<= 1;
    data |= nes->nes_cpu.C;
    nes->nes_cpu.C = (uint8_t)(data >> 8);
    *data_ptr = (uint8_t)data;
    NES_CHECK_NZ(data);
}

static inline void nes_rola(nes_t* nes, const uint16_t address){
    (void)address;
    uint16_t data = nes->nes_cpu.A;
    data <<= 1;
    data |= nes->nes_cpu.C;
    nes->nes_cpu.C = (uint8_t)(data>>8);
    nes->nes_cpu.A = (uint8_t)data;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#LSR
    {adr}:={adr}/2
    N  V  U  B  D  I  Z  C
    *                 *  *
*/
static inline void nes_lsr(nes_t* nes, const uint16_t address){
    uint8_t value = nes_read_cpu(nes, address);
    nes->nes_cpu.C = value & 0x01;
    value >>= 1;
    NES_CHECK_NZ(value);
    nes_write_cpu(nes, address, value);
}

static inline void nes_lsr_ram(nes_t* nes, uint8_t *data_ptr){
    uint8_t value = *data_ptr;
    nes->nes_cpu.C = value & 0x01;
    value >>= 1;
    NES_CHECK_NZ(value);
    *data_ptr = value;
}

static inline void nes_lsra(nes_t* nes, const uint16_t address){
    (void)address;
    nes->nes_cpu.C = nes->nes_cpu.A & 0x01;
    nes->nes_cpu.A >>= 1;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#ROR
    {adr}:={adr}/2+C*128
    N  V  U  B  D  I  Z  C
    *                 *  *
*/
static inline void nes_ror(nes_t* nes, const uint16_t address){
    uint16_t data = nes_read_cpu(nes, address);
    data |= (uint16_t)nes->nes_cpu.C << 8;
    nes->nes_cpu.C = data & 0x01;
    data >>= 1;
    nes_write_cpu(nes, address, (uint8_t)data);
    NES_CHECK_NZ(data);
}

static inline void nes_ror_ram(nes_t* nes, uint8_t *data_ptr){
    uint16_t data = *data_ptr;
    data |= (uint16_t)nes->nes_cpu.C << 8;
    nes->nes_cpu.C = data & 0x01;
    data >>= 1;
    *data_ptr = (uint8_t)data;
    NES_CHECK_NZ(data);
}

static inline void nes_rora(nes_t* nes, const uint16_t address){
    (void)address;
    uint16_t data = nes->nes_cpu.A;
    data |= (uint16_t)nes->nes_cpu.C << 8;
    nes->nes_cpu.C = data & 0x01;
    data >>= 1;
    nes->nes_cpu.A = (uint8_t)data;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

/* Move commands: */

/*
    https://www.nesdev.org/wiki/Instruction_reference#LDA
    A:={adr}
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_lda(nes_t* nes, const uint16_t address){
    nes->nes_cpu.A = nes_read_cpu(nes, address);
    NES_CHECK_NZ(nes->nes_cpu.A);
}

static inline void nes_lda_data(nes_t* nes, const uint8_t data){
    nes->nes_cpu.A = data;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#STA
    {adr}:=A
    N  V  U  B  D  I  Z  C

*/
static inline void nes_sta(nes_t* nes, const uint16_t address){
    nes_write_cpu(nes, address, nes->nes_cpu.A);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#LDX
    X:={adr}
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_ldx(nes_t* nes, const uint16_t address){
    nes->nes_cpu.X = nes_read_cpu(nes, address);
    NES_CHECK_NZ(nes->nes_cpu.X);
}

static inline void nes_ldx_data(nes_t* nes, const uint8_t data){
    nes->nes_cpu.X = data;
    NES_CHECK_NZ(nes->nes_cpu.X);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#STX
    {adr}:=X
    N  V  U  B  D  I  Z  C

*/
static inline void nes_stx(nes_t* nes, const uint16_t address){
    nes_write_cpu(nes, address, nes->nes_cpu.X);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#LDY
    Y:={adr}
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_ldy(nes_t* nes, const uint16_t address){
    nes->nes_cpu.Y = nes_read_cpu(nes, address);
    NES_CHECK_NZ(nes->nes_cpu.Y);
}

static inline void nes_ldy_data(nes_t* nes, const uint8_t data){
    nes->nes_cpu.Y = data;
    NES_CHECK_NZ(nes->nes_cpu.Y);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#STY
    {adr}:=Y
    N  V  U  B  D  I  Z  C

*/
static inline void nes_sty(nes_t* nes, const uint16_t address){
    nes_write_cpu(nes, address, nes->nes_cpu.Y);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#TAX
    X:=A
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_tax(nes_t* nes, const uint16_t address){
    (void)address;
    nes->nes_cpu.X = nes->nes_cpu.A;
    NES_CHECK_NZ(nes->nes_cpu.X);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#TXA
    A:=X
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_txa(nes_t* nes, const uint16_t address){
    (void)address;
    nes->nes_cpu.A = nes->nes_cpu.X;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#TAY
    Y:=A
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_tay(nes_t* nes, const uint16_t address){
    (void)address;
    nes->nes_cpu.Y = nes->nes_cpu.A;
    NES_CHECK_NZ(nes->nes_cpu.Y);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#TYA
    A:=Y
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_tya(nes_t* nes, const uint16_t address){
    (void)address;
    nes->nes_cpu.A = nes->nes_cpu.Y;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#TAY
    X:=S
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_tsx(nes_t* nes, const uint16_t address){
    (void)address;
    nes->nes_cpu.X = nes->nes_cpu.SP;
    NES_CHECK_NZ(nes->nes_cpu.X);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#TXS
    S:=X
    N  V  U  B  D  I  Z  C

*/
static inline void nes_txs(nes_t* nes, const uint16_t address){
    (void)address;
    nes->nes_cpu.SP = nes->nes_cpu.X;
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#PLA
    A:=+(S)
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_pla(nes_t* nes, const uint16_t address){
    (void)address;
    nes_dummy_read(nes);
    nes->nes_cpu.A = NES_POP(nes);
    NES_CHECK_NZ(nes->nes_cpu.A);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#PHA
    (S)-:=A
    N  V  U  B  D  I  Z  C

*/
static inline void nes_pha(nes_t* nes, const uint16_t address){
    (void)address;
    NES_PUSH(nes,nes->nes_cpu.A);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#PLP
    P:=+(S)
    N  V  U  B  D  I  Z  C
    *  *        *  *  *  *
*/
static inline void nes_plp(nes_t* nes, const uint16_t address){
    (void)address;
    nes_dummy_read(nes);
    nes->nes_cpu.P = NES_POP(nes);
    NES_U_SET;
    NES_B_CLR;

    if (!nes->nes_cpu.I){
        /* code */
    }
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#PHP
    (S)-:=P
    N  V  U  B  D  I  Z  C

*/
static inline void nes_php(nes_t* nes, const uint16_t address){
    (void)address;
    NES_U_SET;
    NES_B_SET;
    NES_PUSH(nes,nes->nes_cpu.P);
    // NES_B_CLR;
}

// Jump/Flag commands:

static inline void nes_branch(nes_t* nes,const uint16_t address) {
    nes_dummy_read(nes);
    const uint16_t pc_old = nes->nes_cpu.PC;
    nes->nes_cpu.PC = address;
    nes->nes_cpu.cycles++;
    if ((nes->nes_cpu.PC ^ pc_old) >> 8){
        nes_dummy_read(nes);
        nes->nes_cpu.cycles++;
    }
}

#if CONFIG_NES_FAST_CPU_BRANCH
static inline void NES_CPU_BRANCH_TEXT nes_branch_fast(nes_t* nes, const uint8_t branch, const uint16_t ticks)
{
    const uint16_t opcode_pc = (uint16_t)(nes->nes_cpu.PC - 1U);
    const int8_t offset = (int8_t)nes_fetch_cpu(nes);
    nes->nes_cpu.cycles += 2;
    if (branch) {
        const uint16_t pc_old = nes->nes_cpu.PC;
        const uint16_t target = (uint16_t)(pc_old + offset);
        const uint16_t page_cross = (uint16_t)((target ^ pc_old) >> 8);
        nes->nes_cpu.PC = target;
        nes->nes_cpu.cycles += (uint16_t)(1U + page_cross);

        uint16_t loop_cycles = 0;
        if (target >= 0x8000U) {
            const uint16_t loop_bytes = (uint16_t)(opcode_pc - target);
            if (loop_bytes == 2U && nes_read_prg(nes, target) == 0xA5U) {
                loop_cycles = (uint16_t)(6U + page_cross);
            } else if (loop_bytes == 3U) {
                const uint8_t target_opcode = nes_read_prg(nes, target);
                if ((target_opcode == 0xADU || target_opcode == 0x2CU) &&
                    nes_read_prg(nes, (uint16_t)(target + 1U)) == 0x02U &&
                    nes_read_prg(nes, (uint16_t)(target + 2U)) == 0x20U) {
                    loop_cycles = (uint16_t)(7U + page_cross);
                }
            }
        }
        if (loop_cycles != 0U) {
            if (ticks > nes->nes_cpu.cycles) {
                const uint16_t remaining = (uint16_t)(ticks - nes->nes_cpu.cycles);
                const uint16_t loops = (uint16_t)((remaining + loop_cycles - 1U) / loop_cycles);
                nes->nes_cpu.cycles = (uint16_t)(nes->nes_cpu.cycles + (uint16_t)(loops * loop_cycles));
            }
        }
    }
}

static inline void NES_CPU_BRANCH_TEXT nes_branch_bne_fast(nes_t* nes, const uint16_t ticks)
{
    const uint16_t opcode_pc = (uint16_t)(nes->nes_cpu.PC - 1U);
    const int8_t offset = (int8_t)nes_fetch_cpu(nes);
    nes->nes_cpu.cycles += 2;
    if ((NES_CPU_P & (uint8_t)NES_FLAG_Z) != 0U) {
        return;
    }

    const uint16_t pc_old = nes->nes_cpu.PC;
    const uint16_t target = (uint16_t)(pc_old + offset);
    const uint16_t page_cross = (uint16_t)((target ^ pc_old) >> 8);
    nes->nes_cpu.PC = target;
    nes->nes_cpu.cycles += (uint16_t)(1U + page_cross);

    uint16_t loop_cycles = 0;
    if (target >= 0x8000U) {
        const uint16_t loop_bytes = (uint16_t)(opcode_pc - target);
        if (loop_bytes == 2U && nes_read_prg(nes, target) == 0xA5U) {
            loop_cycles = (uint16_t)(6U + page_cross);
        } else if (loop_bytes == 3U) {
            const uint8_t target_opcode = nes_read_prg(nes, target);
            if ((target_opcode == 0xADU || target_opcode == 0x2CU) &&
                nes_read_prg(nes, (uint16_t)(target + 1U)) == 0x02U &&
                nes_read_prg(nes, (uint16_t)(target + 2U)) == 0x20U) {
                loop_cycles = (uint16_t)(7U + page_cross);
            }
        }
    }
    if (loop_cycles != 0U) {
        if (ticks > nes->nes_cpu.cycles) {
            const uint16_t remaining = (uint16_t)(ticks - nes->nes_cpu.cycles);
            const uint16_t loops = (uint16_t)((remaining + loop_cycles - 1U) / loop_cycles);
            nes->nes_cpu.cycles = (uint16_t)(nes->nes_cpu.cycles + (uint16_t)(loops * loop_cycles));
        }
        return;
    }

    if (target < 0x8000U || page_cross != 0U || (uint16_t)(target + 1U) != opcode_pc ||
        ticks <= nes->nes_cpu.cycles) {
        return;
    }

    uint8_t* loop_reg = NULL;
    int8_t loop_delta = 0;
    switch (nes_read_prg(nes, target)) {
        case 0x88: // DEY
            loop_reg = &nes->nes_cpu.Y;
            loop_delta = -1;
            break;
        case 0xC8: // INY
            loop_reg = &nes->nes_cpu.Y;
            loop_delta = 1;
            break;
        case 0xCA: // DEX
            loop_reg = &nes->nes_cpu.X;
            loop_delta = -1;
            break;
        case 0xE8: // INX
            loop_reg = &nes->nes_cpu.X;
            loop_delta = 1;
            break;
        default:
            return;
    }

    const uint8_t value = *loop_reg;
    if (value == 0U) {
        return;
    }

    const uint16_t remaining = (uint16_t)(ticks - nes->nes_cpu.cycles);
    const uint16_t taken_loop_cycles = 5U; // one register op plus BNE taken
    const uint16_t steps_to_zero = (loop_delta > 0) ? (uint16_t)(256U - value) : (uint16_t)value;
    const uint16_t cycles_to_exit = (uint16_t)(((uint16_t)(steps_to_zero - 1U) * taken_loop_cycles) + 4U);

    if (remaining >= cycles_to_exit) {
        *loop_reg = 0;
        nes_set_nz_fast(nes, 0);
        nes->nes_cpu.PC = pc_old;
        nes->nes_cpu.cycles = (uint16_t)(nes->nes_cpu.cycles + cycles_to_exit);
        return;
    }

    const uint16_t loops = (uint16_t)(remaining / taken_loop_cycles);
    if (loops != 0U) {
        const uint8_t next_value = (loop_delta > 0) ? (uint8_t)(value + loops) : (uint8_t)(value - loops);
        *loop_reg = next_value;
        nes_set_nz_fast(nes, next_value);
        nes->nes_cpu.cycles = (uint16_t)(nes->nes_cpu.cycles + (uint16_t)(loops * taken_loop_cycles));
    }
}

static inline void NES_CPU_HOT_TEXT nes_jmp_abs_zp_adc_loop_fast(nes_t* nes, const uint16_t target, const uint16_t ticks)
{
    if (target < 0x8000U || target > 0xfff7U || ticks <= nes->nes_cpu.cycles) {
        return;
    }

    if (nes_read_prg(nes, target) != 0xA5U ||
        nes_read_prg(nes, (uint16_t)(target + 2U)) != 0x65U ||
        nes_read_prg(nes, (uint16_t)(target + 4U)) != 0x85U ||
        nes_read_prg(nes, (uint16_t)(target + 6U)) != 0x4CU) {
        return;
    }

    const uint8_t src_zp = nes_read_prg(nes, (uint16_t)(target + 1U));
    const uint8_t dst_zp = nes_read_prg(nes, (uint16_t)(target + 3U));
    if (nes_read_prg(nes, (uint16_t)(target + 5U)) != dst_zp) {
        return;
    }

    const uint16_t jmp_target = (uint16_t)nes_read_prg(nes, (uint16_t)(target + 7U)) |
                                ((uint16_t)nes_read_prg(nes, (uint16_t)(target + 8U)) << 8);
    if (jmp_target != target) {
        return;
    }

    const uint16_t loop_cycles = 12U; // LDA zp + ADC zp + STA zp + JMP abs
    uint16_t loops = (uint16_t)((ticks - nes->nes_cpu.cycles) / loop_cycles);
    if (loops == 0U) {
        return;
    }

    const uint16_t loop_count = loops;
    uint8_t* const ram = nes->nes_cpu.cpu_ram;
    do {
        const uint8_t src = ram[src_zp];
        const uint8_t dst = ram[dst_zp];
        const uint16_t result16 = (uint16_t)src + (uint16_t)dst + (uint16_t)(NES_CPU_P & (uint8_t)NES_FLAG_C);
        const uint8_t result8 = (uint8_t)result16;

        uint8_t p = NES_CPU_P & (uint8_t)~(NES_FLAG_C | NES_FLAG_Z | NES_FLAG_V | NES_FLAG_N);
        if (result16 >= 0x100U) {
            p |= (uint8_t)NES_FLAG_C;
        }
        if (((src ^ dst) & 0x80U) == 0U && ((src ^ result8) & 0x80U) != 0U) {
            p |= (uint8_t)NES_FLAG_V;
        }
        if (result8 == 0U) {
            p |= (uint8_t)NES_FLAG_Z;
        }
        p |= result8 & (uint8_t)NES_FLAG_N;

        ram[dst_zp] = result8;
        nes->nes_cpu.A = result8;
        NES_CPU_P = p;
    } while (--loops != 0U);

    nes->nes_cpu.cycles = (uint16_t)(nes->nes_cpu.cycles + (uint16_t)(loop_count * loop_cycles));
}
#endif

static inline void nes_jmp_abs_fast(nes_t* nes, uint16_t ticks)
{
#if CONFIG_NES_FAST_CPU_JMP_IDLE
    const uint16_t opcode_pc = (uint16_t)(nes->nes_cpu.PC - 1U);
#endif
    const uint16_t address = nes_abs(nes);
    nes->nes_cpu.PC = address;
#if CONFIG_NES_FAST_CPU_JMP_IDLE
    if (address == opcode_pc) {
        const uint16_t remaining = ticks - nes->nes_cpu.cycles;
        const uint16_t loops = (uint16_t)((remaining + 2U) / 3U);
        nes->nes_cpu.cycles += (uint16_t)(loops * 3U);
        return;
    }
#endif
    nes->nes_cpu.cycles += 3;
#if CONFIG_NES_FAST_CPU_JMP_IDLE
    if ((uint16_t)(address + 6U) == opcode_pc) {
        nes_jmp_abs_zp_adc_loop_fast(nes, address, ticks);
    }
#endif
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#BPL
    branch on N=0
    N  V  U  B  D  I  Z  C

*/
static inline void nes_bpl(nes_t* nes, const uint16_t address){
    if (NES_CPU_N==0) nes_branch(nes, address);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#BMI
    branch on N=1
    N  V  U  B  D  I  Z  C

*/
static inline void nes_bmi(nes_t* nes, const uint16_t address){
    if (NES_CPU_N)nes_branch(nes, address);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#BVC
    branch on V=0
    N  V  U  B  D  I  Z  C

*/
static inline void nes_bvc(nes_t* nes, const uint16_t address){
    if (nes->nes_cpu.V==0) nes_branch(nes, address);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#BVS
    branch on V=1
    N  V  U  B  D  I  Z  C

*/
static inline void nes_bvs(nes_t* nes, const uint16_t address){
    if (nes->nes_cpu.V) nes_branch(nes, address);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#BCC
    branch on C=0
    N  V  U  B  D  I  Z  C

*/
static inline void nes_bcc(nes_t* nes, const uint16_t address){
    if (nes->nes_cpu.C==0) nes_branch(nes, address);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#BCS
    branch on C=1
    N  V  U  B  D  I  Z  C

*/
static inline void nes_bcs(nes_t* nes, const uint16_t address){
    if (nes->nes_cpu.C) nes_branch(nes, address);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#BNE
    branch on Z=0
    N  V  U  B  D  I  Z  C

*/
static inline void nes_bne(nes_t* nes, const uint16_t address){
    if (nes->nes_cpu.Z==0) nes_branch(nes, address);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#BEQ
    branch on Z=1
    N  V  U  B  D  I  Z  C

*/
static inline void nes_beq(nes_t* nes, const uint16_t address){
    if (nes->nes_cpu.Z) nes_branch(nes, address);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#BRK
    (S)-:=PC,P PC:=($FFFE)
    N  V  U  B  D  I  Z  C
             1     1
*/
static inline void nes_brk(nes_t* nes, const uint16_t address){
    (void)address;
    nes->nes_cpu.PC++;
    NES_PUSHW(nes,nes->nes_cpu.PC);
    NES_B_SET;
    NES_PUSH(nes,nes->nes_cpu.P);
    NES_I_SET;
    NES_D_SET;
    nes->nes_cpu.PC = nes_readw_cpu(nes, NES_VERCTOR_IRQBRK);
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#RTI
    P,PC:=+(S)
    N  V  U  B  D  I  Z  C
    *  *        *  *  *  *
*/
static inline void nes_rti(nes_t* nes, const uint16_t address){
    (void)address;
    nes_dummy_read(nes);
    // P:=+(S)
    nes->nes_cpu.P = NES_POP(nes);
    // NES_U_SET;
    // NES_B_SET;
    // PC:=+(S)
    const uint8_t low_byte = (nes->nes_cpu.cpu_ram + 0x100)[++nes->nes_cpu.SP];
    const uint8_t high_byte = (nes->nes_cpu.cpu_ram + 0x100)[++nes->nes_cpu.SP];
    nes->nes_cpu.PC =  (uint16_t)high_byte << 8 | low_byte;
    // 清计数
    
}


/*
    https://www.nesdev.org/wiki/Instruction_reference#JSR
    (S)-:=PC PC:={adr}
    N  V  U  B  D  I  Z  C

*/
static inline void nes_jsr(nes_t* nes, const uint16_t address){
    nes_dummy_read(nes);
    NES_PUSHW(nes,nes->nes_cpu.PC-1);
    nes->nes_cpu.PC = address;
}


/*
    https://www.nesdev.org/wiki/Instruction_reference#RTS
    PC:=+(S)
    N  V  U  B  D  I  Z  C

*/
static inline void nes_rts(nes_t* nes, const uint16_t address){
    (void)address;
    const uint8_t low_byte = (nes->nes_cpu.cpu_ram + 0x100)[++nes->nes_cpu.SP];
    const uint8_t high_byte = (nes->nes_cpu.cpu_ram + 0x100)[++nes->nes_cpu.SP];
    nes->nes_cpu.PC =  (uint16_t)high_byte << 8 | low_byte;
    nes_dummy_read(nes);
    nes_dummy_read(nes);
    nes->nes_cpu.PC++;
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#JMP
    PC:={adr}
    N  V  U  B  D  I  Z  C

*/
static inline void nes_jmp(nes_t* nes, const uint16_t address){
    nes->nes_cpu.PC = address;
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#BIT
    N:=b7 V:=b6 Z:=A&{adr}
    N  V  U  B  D  I  Z  C
    *  *              *  
*/
static inline void nes_bit(nes_t* nes, const uint16_t address){
    const uint8_t value = nes_read_cpu(nes, address);
    nes->nes_cpu.N = value >> 7;
    nes->nes_cpu.V = (value >> 6) & 1;
    NES_CHECK_Z((nes->nes_cpu.A & value));
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#CLC
    C:=0
    N  V  U  B  D  I  Z  C
                         0
*/
static inline void nes_clc(nes_t* nes, const uint16_t address){
    (void)address;
    NES_C_CLR;
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#SEC
    C:=1
    N  V  U  B  D  I  Z  C
                         1
*/
static inline void nes_sec(nes_t* nes, const uint16_t address){
    (void)address;
    NES_C_SET;
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#CLD
    D:=0
    N  V  U  B  D  I  Z  C
                0         
*/
static inline void nes_cld(nes_t* nes, const uint16_t address){
    (void)address;
    NES_D_CLR;
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#SED
    D:=1
    N  V  U  B  D  I  Z  C
                1         
*/
static inline void nes_sed(nes_t* nes, const uint16_t address){
    (void)address;
    NES_D_SET;
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#CLI
    I:=0
    N  V  U  B  D  I  Z  C
                   0      
*/
static inline void nes_cli(nes_t* nes, const uint16_t address){
    (void)address;
    NES_I_CLR;
    // irq_counter
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#SEI
    I:=1
    N  V  U  B  D  I  Z  C
                   1      
*/
static inline void nes_sei(nes_t* nes, const uint16_t address){
    (void)address;
    NES_I_SET;
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#CLV
    V:=0
    N  V  U  B  D  I  Z  C
       0                  
*/
static inline void nes_clv(nes_t* nes, const uint16_t address){
    (void)address;
    NES_V_CLR;
}

/*
    https://www.nesdev.org/wiki/Instruction_reference#NOP
    N  V  U  B  D  I  Z  C

*/
static inline void nes_nop(nes_t* nes, const uint16_t address){
    (void)nes;
    (void)address;
}

/* Illegal opcodes: */

/*
    {adr}:={adr}*2 A:=A or {adr}	
    N  V  U  B  D  I  Z  C
    *                 *  *
*/
static inline void nes_slo(nes_t* nes, const uint16_t address){
    uint8_t data = nes_read_cpu(nes, address);
    // asl
    nes->nes_cpu.C = data >> 7;
    data <<= 1;
    nes_write_cpu(nes, address, data);
    // ora
    nes->nes_cpu.A |= data;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

/*
    {adr}:={adr}rol A:=A and {adr}
    N  V  U  B  D  I  Z  C
    *                 *  *
*/
static inline void nes_rla(nes_t* nes, const uint16_t address){
    uint16_t data = nes_read_cpu(nes, address);
    // rol
    data <<= 1;
    data |= nes->nes_cpu.C;
    nes->nes_cpu.C = data >> 8;
    nes_write_cpu(nes, address, (uint8_t)data);
    // and
    nes->nes_cpu.A &= (uint8_t)data;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

/*
    {adr}:={adr}/2 A:=A exor {adr}
    N  V  U  B  D  I  Z  C
    *                 *  *
*/
static inline void nes_sre(nes_t* nes, const uint16_t address){
    uint8_t data = nes_read_cpu(nes, address);
    // lsr
    nes->nes_cpu.C = data & 0x01;
    data >>= 1;
    nes_write_cpu(nes, address, data);
    // eor
    nes->nes_cpu.A ^= data;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

/*
    {adr}:={adr}ror A:=A adc {adr}
    N  V  U  B  D  I  Z  C
    *  *              *  *
*/
static inline void nes_rra(nes_t* nes, const uint16_t address){
    uint16_t data = nes_read_cpu(nes, address);
    // ror
    data |= (nes->nes_cpu.C << 8);
    uint8_t cflag = data & 0x01;
    data >>= 1;
    nes_write_cpu(nes,address,(uint8_t)data);
    // adc
    const uint16_t data1 = nes->nes_cpu.A + data + cflag;
    nes->nes_cpu.C = data1 >> 8;
    if (!((nes->nes_cpu.A ^ data) & 0x80) && ((nes->nes_cpu.A ^ (uint8_t)data1) & 0x80)) nes->nes_cpu.V = 1;
    else nes->nes_cpu.V = 0;
    nes->nes_cpu.A = (uint8_t)data1;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

/*
    {adr}:=A&X
    N  V  U  B  D  I  Z  C

*/
static inline void nes_sax(nes_t* nes, const uint16_t address){
    nes_write_cpu(nes, address, nes->nes_cpu.A & nes->nes_cpu.X);
}

/*
    A,X:={adr}
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_lax(nes_t* nes, const uint16_t address){
    nes->nes_cpu.A = nes->nes_cpu.X = nes_read_cpu(nes, address);
    NES_CHECK_NZ(nes->nes_cpu.A);
}

/*
    {adr}:={adr}-1 A-{adr}
    N  V  U  B  D  I  Z  C
    *                 *  *
*/
static inline void nes_dcp(nes_t* nes, const uint16_t address){
    uint8_t data = nes_read_cpu(nes, address);
    // dec
    data--;
    nes_write_cpu(nes, address, data);
    // cmp
    const uint16_t data1 = (uint16_t)nes->nes_cpu.A - (uint16_t)data;
    nes->nes_cpu.C = !(data1 >> 15);
    NES_CHECK_NZ((uint8_t)data1);
}

/*
    {adr}:={adr}+1 A:=A-{adr}
    N  V  U  B  D  I  Z  C
    *  *              *  *
*/
static inline void nes_isc(nes_t* nes, const uint16_t address){
    uint8_t data = nes_read_cpu(nes, address);
    // inc
    nes_write_cpu(nes, address, ++data);
    // sbc
    const uint16_t data1 = nes->nes_cpu.A - data - !nes->nes_cpu.C;
    nes->nes_cpu.C = !(data1 >> 8);
    const uint8_t data2 = (uint8_t)data1;

    if (((nes->nes_cpu.A ^ data) & 0x80) && ((nes->nes_cpu.A ^ data2) & 0x80)) nes->nes_cpu.V = 1;
    else nes->nes_cpu.V = 0;

    nes->nes_cpu.A = data2;
    NES_CHECK_NZ(nes->nes_cpu.A);
}

/*
    A:=A&#{imm}
    N  V  U  B  D  I  Z  C
    *                 *  *
*/
static inline void nes_anc(nes_t* nes, const uint16_t address){
    nes->nes_cpu.A &= nes_read_cpu(nes, address);
    NES_CHECK_NZ(nes->nes_cpu.A);
    nes->nes_cpu.C = nes->nes_cpu.A >> 7;
}

/*
    A:=(A&#{imm})/2
    N  V  U  B  D  I  Z  C
    *                 *  *
*/
static inline void nes_alr(nes_t* nes, const uint16_t address){
    (void)nes;
    (void)address;
}

/*
    A:=(A&#{imm})/2
    N  V  U  B  D  I  Z  C
    *  *              *  *
*/
static inline void nes_arr(nes_t* nes, const uint16_t address){
    nes->nes_cpu.A &= nes_read_cpu(nes, address);
    nes->nes_cpu.A = (nes->nes_cpu.A>>1)|(nes->nes_cpu.C<<7);
    NES_CHECK_NZ(nes->nes_cpu.A);
    nes->nes_cpu.C = (nes->nes_cpu.A >> 6)&1;
    if (((nes->nes_cpu.A >> 5) ^ (nes->nes_cpu.A >> 6)) & 1) nes->nes_cpu.V = 1;
    else nes->nes_cpu.V = 0;
}

/*
    A:=X&#{imm}
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_xaa(nes_t* nes, const uint16_t address){
    nes->nes_cpu.A = nes->nes_cpu.X & nes_read_cpu(nes, address);
    NES_CHECK_NZ(nes->nes_cpu.A);
    nes->nes_cpu.C = nes->nes_cpu.A >> 7;
}

/*
    X:=A&X-#{imm}
    N  V  U  B  D  I  Z  C
    *                 *  *
*/
static inline void nes_axs(nes_t* nes, const uint16_t address){
    uint16_t data = (nes->nes_cpu.A & nes->nes_cpu.X) - address;
    nes->nes_cpu.X = (uint8_t)data;
    NES_CHECK_NZ(nes->nes_cpu.X);
    nes->nes_cpu.C = !(data >> 15);
}

/*
    {adr}:=A&X&H
    N  V  U  B  D  I  Z  C

*/
static inline void nes_ahx(nes_t* nes, const uint16_t address){
    (void)nes;
    (void)address;
}

/*
    {adr}:=Y&H
    N  V  U  B  D  I  Z  C

*/
static inline void nes_shy(nes_t* nes, const uint16_t address){
    (void)nes;
    (void)address;
}

/*
    {adr}:=X&H
    N  V  U  B  D  I  Z  C

*/
static inline void nes_shx(nes_t* nes, const uint16_t address){
    (void)nes;
    (void)address;
}

/*
    S:=A&X {adr}:=S&H
    N  V  U  B  D  I  Z  C

*/
static inline void nes_tas(nes_t* nes, const uint16_t address){
    (void)nes;
    (void)address;
}

/*
    A,X,S:={adr}&S
    N  V  U  B  D  I  Z  C
    *                 *  
*/
static inline void nes_las(nes_t* nes, const uint16_t address){
    (void)nes;
    (void)address;
}

/*
    Hardware vectors:
    $FFFA = NMI vector (NMI=not maskable interupts)
    $FFFC = Reset vector
    $FFFE = IRQ vector
*/

static inline void nes_nmi(nes_t* nes){
    NES_PUSHW(nes,nes->nes_cpu.PC);
    NES_U_SET;
    NES_B_CLR;
    NES_PUSH(nes,nes->nes_cpu.P);
    NES_I_SET;
    nes->nes_cpu.PC = nes_readw_cpu(nes,NES_VERCTOR_NMI);
    nes->nes_cpu.cycles += 7;
}

void nes_cpu_irq(nes_t* nes){
    if (nes->nes_cpu.I==0){
        NES_PUSHW(nes,nes->nes_cpu.PC);
        NES_PUSH(nes,nes->nes_cpu.P);
        NES_B_CLR;
        NES_I_SET;
        nes->nes_cpu.PC = nes_readw_cpu(nes,NES_VERCTOR_IRQBRK);
        nes->nes_cpu.cycles += 7;
    }
}

// https://www.nesdev.org/wiki/CPU_power_up_state#After_reset
void nes_cpu_reset(nes_t* nes){
    NES_I_SET;                          // The I (IRQ disable) flag was set to true
    nes->nes_cpu.SP -= 3;               // S was decremented by 3 (but nothing was written to the stack)
    nes_write_cpu(nes,0x4015, 0x00);    // APU was silenced ($4015 = 0)

    nes->nes_cpu.PC = nes_readw_cpu(nes,NES_VERCTOR_RESET);
    nes->nes_cpu.cycles = 7;
}

// https://www.nesdev.org/wiki/CPU_power_up_state#At_power-up
void nes_cpu_init(nes_t* nes){
    // Status: Carry, Zero, Decimal, Overflow, Negative clear. Interrupt Disable set.
    // A, X, Y = 0
    nes->nes_cpu.A = nes->nes_cpu.X = nes->nes_cpu.Y = nes->nes_cpu.P = 0;

    NES_CPU_P = 0;
    NES_U_SET;
    NES_N_SET;
    nes->nes_cpu.SP = 0x00;             // reset: S = $00-$03 = $FD
}

#ifdef __DEBUG__

static char* nes_opcode_name[256] = {
    "BRK    ","ORA IZX","KIL    ","SLO IZX","NOP ZP ","ORA ZP ","ASL ZP ","SLO ZP ","PHP","ORA IMM","ASL","ANC IMM","NOP ABS","ORA ABS","ASL ABS","SLO ABS",
    "BPL REL","ORA IZY","KIL    ","SLO IZY","NOP ZPX","ORA ZPX","ASL ZPX","SLO ZPX","CLC","ORA ABY","NOP","SLO ABY","NOP ABX","ORA ABX","ASL ABX","SLO ABX",
    "JSR ABS","AND IZX","KIL    ","RLA IZX","BIT ZP ","AND ZP ","ROL ZP ","RLA ZP ","PLP","AND IMM","ROL","ANC IMM","BIT ABS","AND ABS","ROL ABS","RLA ABS",
    "BMI REL","AND IZY","KIL    ","RLA IZY","NOP ZPX","AND ZPX","ROL ZPX","RLA ZPX","SEC","AND ABY","NOP","RLA ABY","NOP ABX","AND ABX","ROL ABX","RLA ABX",
    "RTI    ","EOR IZX","KIL    ","SRE IZX","NOP ZP ","EOR ZP ","LSR ZP ","SRE ZP ","PHA","EOR IMM","LSR","ALR IMM","JMP ABS","EOR ABS","LSR ABS","SRE ABS",
    "BVC REL","EOR IZY","KIL    ","SRE IZY","NOP ZPX","EOR ZPX","LSR ZPX","SRE ZPX","CLI","EOR ABY","NOP","SRE ABY","NOP ABX","EOR ABX","LSR ABX","SRE ABX",
    "RTS    ","ADC IZX","KIL    ","RRA IZX","NOP ZP ","ADC ZP ","ROR ZP ","RRA ZP ","PLA","ADC IMM","ROR","ARR IMM","JMP IND","ADC ABS","ROR ABS","RRA ABS",
    "BVS REL","ADC IZY","KIL    ","RRA IZY","NOP ZPX","ADC ZPX","ROR ZPX","RRA ZPX","SEI","ADC ABY","NOP","RRA ABY","NOP ABX","ADC ABX","ROR ABX","RRA ABX",
    "NOP IMM","STA IZX","NOP IMM","SAX IZX","STY ZP ","STA ZP ","STX ZP ","SAX ZP ","DEY","NOP IMM","TXA","XAA IMM","STY ABS","STA ABS","STX ABS","SAX ABS",
    "BCC REL","STA IZY","KIL    ","AHX IZY","STY ZPX","STA ZPX","STX ZPY","SAX ZPY","TYA","STA ABY","TXS","TAS ABY","SHY ABX","STA ABX","SHX ABY","AHX ABY",
    "LDY IMM","LDA IZX","LDX IMM","LAX IZX","LDY ZP ","LDA ZP ","LDX ZP ","LAX ZP ","TAY","LDA IMM","TAX","LAX IMM","LDY ABS","LDA ABS","LDX ABS","LAX ABS",
    "BCS REL","LDA IZY","KIL    ","LAX IZY","LDY ZPX","LDA ZPX","LDX ZPY","LAX ZPY","CLV","LDA ABY","TSX","LAS ABY","LDY ABX","LDA ABX","LDX ABY","LAX ABY",
    "CPY IMM","CMP IZX","NOP IMM","DCP IZX","CPY ZP ","CMP ZP ","DEC ZP ","DCP ZP ","INY","CMP IMM","DEX","AXS IMM","CPY ABS","CMP ABS","DEC ABS","DCP ABS",
    "BNE REL","CMP IZY","KIL    ","DCP IZY","NOP ZPX","CMP ZPX","DEC ZPX","DCP ZPX","CLD","CMP ABY","NOP","DCP ABY","NOP ABX","CMP ABX","DEC ABX","DCP ABX",
    "CPX IMM","SBC IZX","NOP IMM","ISC IZX","CPX ZP ","SBC ZP ","INC ZP ","ISC ZP ","INX","SBC IMM","NOP","SBC IMM","CPX ABS","SBC ABS","INC ABS","ISC ABS",
    "BEQ REL","SBC IZY","KIL    ","ISC IZY","NOP ZPX","SBC ZPX","INC ZPX","ISC ZPX","SED","SBC ABY","NOP","ISC ABY","NOP ABX","SBC ABX","INC ABX","ISC ABX",
};
static uint64_t cycles = 7;

extern FILE * debug_fp;
uint8_t cycles_old = 0;
#endif

#if CONFIG_NES_CPU_OPCODE_PROFILE_ENABLE
#define NES_OPCODE_PROFILE_TOP_N 8U
#define NES_OPCODE_PROFILE_PC_SLOTS 128U
#define NES_OPCODE_PROFILE_PC_PROBES 4U

static uint32_t g_nes_opcode_counts[256];
static uint16_t g_nes_opcode_pc_keys[NES_OPCODE_PROFILE_PC_SLOTS];
static uint32_t g_nes_opcode_pc_counts[NES_OPCODE_PROFILE_PC_SLOTS];
static uint8_t g_nes_opcode_pc_ops[NES_OPCODE_PROFILE_PC_SLOTS];
static uint32_t g_nes_opcode_total;
static uint32_t g_nes_opcode_last_ms;

static inline void nes_opcode_profile_hit(uint16_t pc, uint8_t opcode)
{
    g_nes_opcode_counts[opcode]++;
    g_nes_opcode_total++;

    uint16_t slot = (uint16_t)((pc ^ (pc >> 4) ^ opcode) & (NES_OPCODE_PROFILE_PC_SLOTS - 1U));
    for (uint8_t probe = 0; probe < NES_OPCODE_PROFILE_PC_PROBES; probe++) {
        uint16_t idx = (uint16_t)((slot + probe) & (NES_OPCODE_PROFILE_PC_SLOTS - 1U));
        if (g_nes_opcode_pc_counts[idx] == 0U || g_nes_opcode_pc_keys[idx] == pc) {
            g_nes_opcode_pc_keys[idx] = pc;
            g_nes_opcode_pc_ops[idx] = opcode;
            g_nes_opcode_pc_counts[idx]++;
            return;
        }
    }
}

static void nes_opcode_profile_log(void)
{
    uint32_t now_ms = nes_get_ms();
    if (g_nes_opcode_last_ms == 0U) {
        g_nes_opcode_last_ms = now_ms;
        return;
    }

    if ((now_ms - g_nes_opcode_last_ms) < CONFIG_NES_PROFILE_LOG_MS) {
        return;
    }

    uint8_t top_op[NES_OPCODE_PROFILE_TOP_N] = {0};
    uint32_t top_count[NES_OPCODE_PROFILE_TOP_N] = {0};
    uint16_t top_pc[NES_OPCODE_PROFILE_TOP_N] = {0};
    uint8_t top_pc_op[NES_OPCODE_PROFILE_TOP_N] = {0};
    uint32_t top_pc_count[NES_OPCODE_PROFILE_TOP_N] = {0};
    for (uint16_t opcode = 0; opcode < 256U; opcode++) {
        const uint32_t count = g_nes_opcode_counts[opcode];
        if (count <= top_count[NES_OPCODE_PROFILE_TOP_N - 1U]) {
            continue;
        }
        for (uint8_t pos = 0; pos < NES_OPCODE_PROFILE_TOP_N; pos++) {
            if (count > top_count[pos]) {
                for (uint8_t move = NES_OPCODE_PROFILE_TOP_N - 1U; move > pos; move--) {
                    top_count[move] = top_count[move - 1U];
                    top_op[move] = top_op[move - 1U];
                }
                top_count[pos] = count;
                top_op[pos] = (uint8_t)opcode;
                break;
            }
        }
    }
    for (uint16_t slot = 0; slot < NES_OPCODE_PROFILE_PC_SLOTS; slot++) {
        const uint32_t count = g_nes_opcode_pc_counts[slot];
        if (count <= top_pc_count[NES_OPCODE_PROFILE_TOP_N - 1U]) {
            continue;
        }
        for (uint8_t pos = 0; pos < NES_OPCODE_PROFILE_TOP_N; pos++) {
            if (count > top_pc_count[pos]) {
                for (uint8_t move = NES_OPCODE_PROFILE_TOP_N - 1U; move > pos; move--) {
                    top_pc_count[move] = top_pc_count[move - 1U];
                    top_pc[move] = top_pc[move - 1U];
                    top_pc_op[move] = top_pc_op[move - 1U];
                }
                top_pc_count[pos] = count;
                top_pc[pos] = g_nes_opcode_pc_keys[slot];
                top_pc_op[pos] = g_nes_opcode_pc_ops[slot];
                break;
            }
        }
    }

    nes_log_printf("NES_OPTOP: total=%lu top=%02X:%lu,%02X:%lu,%02X:%lu,%02X:%lu,%02X:%lu,%02X:%lu,%02X:%lu,%02X:%lu\n",
                   (unsigned long)g_nes_opcode_total,
                   top_op[0], (unsigned long)top_count[0],
                   top_op[1], (unsigned long)top_count[1],
                   top_op[2], (unsigned long)top_count[2],
                   top_op[3], (unsigned long)top_count[3],
                   top_op[4], (unsigned long)top_count[4],
                   top_op[5], (unsigned long)top_count[5],
                   top_op[6], (unsigned long)top_count[6],
                   top_op[7], (unsigned long)top_count[7]);
    nes_log_printf("NES_HOTPC: total=%lu top=%04X/%02X:%lu,%04X/%02X:%lu,%04X/%02X:%lu,%04X/%02X:%lu,%04X/%02X:%lu,%04X/%02X:%lu,%04X/%02X:%lu,%04X/%02X:%lu\n",
                   (unsigned long)g_nes_opcode_total,
                   top_pc[0], top_pc_op[0], (unsigned long)top_pc_count[0],
                   top_pc[1], top_pc_op[1], (unsigned long)top_pc_count[1],
                   top_pc[2], top_pc_op[2], (unsigned long)top_pc_count[2],
                   top_pc[3], top_pc_op[3], (unsigned long)top_pc_count[3],
                   top_pc[4], top_pc_op[4], (unsigned long)top_pc_count[4],
                   top_pc[5], top_pc_op[5], (unsigned long)top_pc_count[5],
                   top_pc[6], top_pc_op[6], (unsigned long)top_pc_count[6],
                   top_pc[7], top_pc_op[7], (unsigned long)top_pc_count[7]);

    for (uint16_t opcode = 0; opcode < 256U; opcode++) {
        g_nes_opcode_counts[opcode] = 0;
    }
    for (uint16_t slot = 0; slot < NES_OPCODE_PROFILE_PC_SLOTS; slot++) {
        g_nes_opcode_pc_counts[slot] = 0;
    }
    g_nes_opcode_total = 0;
    g_nes_opcode_last_ms = now_ms;
}
#else
#define nes_opcode_profile_hit(pc, opcode) do { (void)(pc); (void)(opcode); } while (0)
#define nes_opcode_profile_log() do { } while (0)
#endif

// https://www.nesdev.org/wiki/CPU_unofficial_opcodes
// https://www.oxyron.de/html/opcodes02.html

void nes_opcode(nes_t* nes,uint16_t ticks){
    if (nes->nes_cpu.irq_nmi) {
        nes_nmi(nes);
        nes->nes_cpu.irq_nmi = 0;
    }
    while (ticks > nes->nes_cpu.cycles){
#ifdef __DEBUG__
        // fprintf(debug_fp,"A:0x%02X X:0x%02X Y:0x%02X SP:0x%02X \nP:0x%02X \nC:0x%02X Z:0x%02X I:0x%02X D:0x%02X B:0x%02X V:0x%02X N:0x%02X \n",
        //         nes->nes_cpu.A,nes->nes_cpu.X,nes->nes_cpu.Y,nes->nes_cpu.SP,
        //         nes->nes_cpu.P,nes->nes_cpu.C,nes->nes_cpu.Z,nes->nes_cpu.I,nes->nes_cpu.D,nes->nes_cpu.B,nes->nes_cpu.V,NES_CPU_N);
        // fprintf(debug_fp,"PC: 0x%04X cycles:%lld \n", nes->nes_cpu.PC,cycles);
        // if (cycles == 56955){
        //     printf("cycles");
        //     printf("cycles");
        //     printf("cycles");
        // }
        // cycles_old = nes->nes_cpu.cycles;
#endif
        const uint16_t opcode_pc = nes->nes_cpu.PC;
        nes->nes_cpu.opcode = nes_fetch_cpu(nes);
        nes_opcode_profile_hit(opcode_pc, nes->nes_cpu.opcode);

        // https://www.nesdev.org/wiki/CPU_unofficial_opcodes
        // https://www.oxyron.de/html/opcodes02.html

        switch (nes->nes_cpu.opcode){
        case 0x00:{nes_brk(nes, 0);            nes->nes_cpu.cycles += 7;break;}// BRK         7
        case 0x01:{nes_ora(nes, nes_izx(nes)); nes->nes_cpu.cycles += 6;break;}// ORA IZX     6
        case 0x02:{                                                     break;}// KIL         0
        case 0x03:{nes_slo(nes, nes_izx(nes)); nes->nes_cpu.cycles += 8;break;}// SLO IZX     8
        case 0x04:{nes_nop(nes, nes_zp(nes));  nes->nes_cpu.cycles += 3;break;}// NOP ZP      3
        case 0x05:{nes_ora_data(nes, nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 3;break;}// ORA ZP      3
        case 0x06:{nes_asl_ram(nes, &nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 5;break;}// ASL ZP      5
        case 0x07:{nes_slo(nes, nes_zp(nes));  nes->nes_cpu.cycles += 5;break;}// SLO ZP      5
        case 0x08:{nes_php(nes, 0);            nes->nes_cpu.cycles += 3;break;}// PHP         3
        case 0x09:{nes_ora_data(nes, nes_fetch_cpu(nes)); nes->nes_cpu.cycles += 2;break;}// ORA IMM     2
        case 0x0A:{nes_asla(nes, 0);           nes->nes_cpu.cycles += 2;break;}// ASL         2
        case 0x0B:{nes_anc(nes, nes_imm(nes)); nes->nes_cpu.cycles += 2;break;}// ANC IMM     2
        case 0x0C:{nes_nop(nes, nes_abs(nes)); nes->nes_cpu.cycles += 4;break;}// NOP ABS     4
        case 0x0D:{nes_ora(nes, nes_abs(nes)); nes->nes_cpu.cycles += 4;break;}// ORA ABS     4
        case 0x0E:{nes_asl(nes, nes_abs(nes)); nes->nes_cpu.cycles += 6;break;}// ASL ABS     6
        case 0x0F:{nes_slo(nes, nes_abs(nes)); nes->nes_cpu.cycles += 6;break;}// SLO ABS     6
        case 0x10:{
#if CONFIG_NES_FAST_CPU_BRANCH
            nes_branch_fast(nes, (NES_CPU_P & (uint8_t)NES_FLAG_N) == 0U, ticks);
            break;
#else
            nes_bpl(nes, nes_rel(nes));
            nes->nes_cpu.cycles += 2;break;
#endif
        }// BPL REL     2*
        case 0x11:{nes_ora(nes, nes_izy(nes)); nes->nes_cpu.cycles += 5;break;}// ORA IZY     5*
        case 0x12:{                                                     break;}// KIL         0
        case 0x13:{nes_slo(nes, nes_izy2(nes));nes->nes_cpu.cycles += 8;break;}// SLO IZY     8
        case 0x14:{nes_nop(nes, nes_zpx(nes)); nes->nes_cpu.cycles += 4;break;}// NOP ZPX     4
        case 0x15:{nes_ora_data(nes, nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff]); nes->nes_cpu.cycles += 4;break;}// ORA ZPX     4
        case 0x16:{nes_asl_ram(nes, &nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff]); nes->nes_cpu.cycles += 6;break;}// ASL ZPX     6
        case 0x17:{nes_slo(nes, nes_zpx(nes)); nes->nes_cpu.cycles += 6;break;}// SLO ZPX     6
        case 0x18:{nes_clc(nes, 0);            nes->nes_cpu.cycles += 2;break;}// CLC         2
        case 0x19:{nes_ora(nes, nes_aby(nes)); nes->nes_cpu.cycles += 4;break;}// ORA ABY     4*
        case 0x1A:{nes_nop(nes, 0);            nes->nes_cpu.cycles += 2;break;}// NOP         2
        case 0x1B:{nes_slo(nes, nes_aby2(nes));nes->nes_cpu.cycles += 7;break;}// SLO ABY     7
        case 0x1C:{nes_nop(nes, nes_abx(nes)); nes->nes_cpu.cycles += 4;break;}// NOP ABX     4*
        case 0x1D:{nes_ora(nes, nes_abx(nes)); nes->nes_cpu.cycles += 4;break;}// ORA ABX     4*
        case 0x1E:{nes_asl(nes, nes_abx2(nes));nes->nes_cpu.cycles += 7;break;}// ASL ABX     7
        case 0x1F:{nes_slo(nes, nes_abx2(nes));nes->nes_cpu.cycles += 7;break;}// SLO ABX     7
        case 0x20:{
            const uint16_t address = nes_abs(nes);
            NES_PUSHW(nes, nes->nes_cpu.PC - 1U);
            nes->nes_cpu.PC = address;
            nes->nes_cpu.cycles += 6;
            break;
        }// JSR ABS     6
        case 0x21:{nes_and(nes, nes_izx(nes)); nes->nes_cpu.cycles += 6;break;}// AND IZX     6
        case 0x22:{                                                     break;}// KIL         0
        case 0x23:{nes_rla(nes, nes_izx(nes)); nes->nes_cpu.cycles += 8;break;}// RLA IZX     8
        case 0x24:{nes_bit_data_fast(nes, nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 3;break;}// BIT ZP      3
        case 0x25:{nes_and_data(nes, nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 3;break;}// AND ZP      3
        case 0x26:{nes_rol_ram(nes, &nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 5;break;}// ROL ZP      5
        case 0x27:{nes_rla(nes, nes_zp(nes));  nes->nes_cpu.cycles += 5;break;}// RLA ZP      5
        case 0x28:{nes_plp(nes, 0);            nes->nes_cpu.cycles += 4;break;}// PLP         4
        case 0x29:{nes->nes_cpu.A &= nes_fetch_cpu(nes); nes_set_nz_fast(nes, nes->nes_cpu.A); nes->nes_cpu.cycles += 2;break;}// AND IMM     2
        case 0x2A:{nes_rola(nes, 0);           nes->nes_cpu.cycles += 2;break;}// ROL         2
        case 0x2B:{nes_anc(nes, nes_imm(nes)); nes->nes_cpu.cycles += 2;break;}// ANC IMM     2
        case 0x2C:{nes_bit_data_fast(nes, nes_read_data_fast(nes, nes_abs(nes))); nes->nes_cpu.cycles += 4;break;}// BIT ABS     4
        case 0x2D:{nes_and(nes, nes_abs(nes)); nes->nes_cpu.cycles += 4;break;}// AND ABS     4
        case 0x2E:{nes_rol(nes, nes_abs(nes)); nes->nes_cpu.cycles += 6;break;}// ROL ABS     6
        case 0x2F:{nes_rla(nes, nes_abs(nes)); nes->nes_cpu.cycles += 6;break;}// RLA ABS     6
        case 0x30:{
#if CONFIG_NES_FAST_CPU_BRANCH
            nes_branch_fast(nes, (NES_CPU_P & (uint8_t)NES_FLAG_N) != 0U, ticks);
            break;
#else
            nes_bmi(nes, nes_rel(nes));
            nes->nes_cpu.cycles += 2;break;
#endif
        }// BMI REL     2*
        case 0x31:{nes_and(nes, nes_izy(nes)); nes->nes_cpu.cycles += 5;break;}// AND IZY     5*
        case 0x32:{                                                     break;}// KIL         0
        case 0x33:{nes_rla(nes, nes_izy2(nes));nes->nes_cpu.cycles += 8;break;}// RLA IZY     8
        case 0x34:{nes_nop(nes, nes_zpx(nes)); nes->nes_cpu.cycles += 4;break;}// NOP ZPX     4
        case 0x35:{nes_and_data(nes, nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff]); nes->nes_cpu.cycles += 4;break;}// AND ZPX     4
        case 0x36:{nes_rol_ram(nes, &nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff]); nes->nes_cpu.cycles += 6;break;}// ROL ZPX     6
        case 0x37:{nes_rla(nes, nes_zpx(nes)); nes->nes_cpu.cycles += 6;break;}// RLA ZPX     6
        case 0x38:{nes_sec(nes, 0);            nes->nes_cpu.cycles += 2;break;}// SEC         2
        case 0x39:{nes_and(nes, nes_aby(nes)); nes->nes_cpu.cycles += 4;break;}// AND ABY     4*
        case 0x3A:{nes_nop(nes, 0);            nes->nes_cpu.cycles += 2;break;}// NOP         2
        case 0x3B:{nes_rla(nes, nes_aby2(nes));nes->nes_cpu.cycles += 7;break;}// RLA ABY     7
        case 0x3C:{nes_nop(nes, nes_abx(nes)); nes->nes_cpu.cycles += 4;break;}// NOP ABX     4*
        case 0x3D:{nes_and(nes, nes_abx(nes)); nes->nes_cpu.cycles += 4;break;}// AND ABX     4*
        case 0x3E:{nes_rol(nes, nes_abx2(nes));nes->nes_cpu.cycles += 7;break;}// ROL ABX     7
        case 0x3F:{nes_rla(nes, nes_abx2(nes));nes->nes_cpu.cycles += 7;break;}// RLA ABX     7
        case 0x40:{nes_rti(nes, 0);            nes->nes_cpu.cycles += 6;break;}// RTI         6
        case 0x41:{nes_eor(nes, nes_izx(nes)); nes->nes_cpu.cycles += 6;break;}// EOR IZX     6
        case 0x42:{                                                     break;}// KIL         0
        case 0x43:{nes_sre(nes, nes_izx(nes)); nes->nes_cpu.cycles += 8;break;}// SRE IZX     8
        case 0x44:{nes_nop(nes, nes_zp(nes));  nes->nes_cpu.cycles += 3;break;}// NOP ZP      3
        case 0x45:{nes_eor_data(nes, nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 3;break;}// EOR ZP      3
        case 0x46:{nes_lsr_ram(nes, &nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 5;break;}// LSR ZP      5
        case 0x47:{nes_sre(nes, nes_zp(nes));  nes->nes_cpu.cycles += 5;break;}// SRE ZP      5
        case 0x48:{nes_pha(nes, 0);            nes->nes_cpu.cycles += 3;break;}// PHA         3
        case 0x49:{nes_eor_data(nes, nes_fetch_cpu(nes)); nes->nes_cpu.cycles += 2;break;}// EOR IMM     2
        case 0x4A:{nes_lsr_a_fast(nes);        nes->nes_cpu.cycles += 2;break;}// LSR         2
        case 0x4B:{nes_alr(nes, nes_imm(nes)); nes->nes_cpu.cycles += 2;break;}// ALR IMM     2
        case 0x4C:{nes_jmp_abs_fast(nes, ticks); break;}// JMP ABS     3
        case 0x4D:{nes_eor(nes, nes_abs(nes)); nes->nes_cpu.cycles += 4;break;}// EOR ABS     4
        case 0x4E:{nes_lsr(nes, nes_abs(nes)); nes->nes_cpu.cycles += 6;break;}// LSR ABS     6
        case 0x4F:{nes_sre(nes, nes_abs(nes)); nes->nes_cpu.cycles += 6;break;}// SRE ABS     6
        case 0x50:{
#if CONFIG_NES_FAST_CPU_BRANCH
            nes_branch_fast(nes, (NES_CPU_P & (uint8_t)NES_FLAG_V) == 0U, ticks);
            break;
#else
            nes_bvc(nes, nes_rel(nes));
            nes->nes_cpu.cycles += 2;break;
#endif
        }// BVC REL     2*
        case 0x51:{nes_eor(nes, nes_izy(nes)); nes->nes_cpu.cycles += 5;break;}// EOR IZY     5*
        case 0x52:{                                                     break;}// KIL         0
        case 0x53:{nes_sre(nes, nes_izy2(nes));nes->nes_cpu.cycles += 8;break;}// SRE IZY     8
        case 0x54:{nes_nop(nes, nes_zpx(nes)); nes->nes_cpu.cycles += 4;break;}// NOP ZPX     4
        case 0x55:{nes_eor_data(nes, nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff]); nes->nes_cpu.cycles += 4;break;}// EOR ZPX     4
        case 0x56:{nes_lsr_ram(nes, &nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff]); nes->nes_cpu.cycles += 6;break;}// LSR ZPX     6
        case 0x57:{nes_sre(nes, nes_zpx(nes)); nes->nes_cpu.cycles += 6;break;}// SRE ZPX     6
        case 0x58:{nes_cli(nes, 0);            nes->nes_cpu.cycles += 2;break;}// CLI         2
        case 0x59:{nes_eor(nes, nes_aby(nes)); nes->nes_cpu.cycles += 4;break;}// EOR ABY     4*
        case 0x5A:{nes_nop(nes, 0);            nes->nes_cpu.cycles += 2;break;}// NOP         2
        case 0x5B:{nes_sre(nes, nes_aby2(nes));nes->nes_cpu.cycles += 7;break;}// SRE ABY     7
        case 0x5C:{nes_nop(nes, nes_abx(nes)); nes->nes_cpu.cycles += 4;break;}// NOP ABX     4*
        case 0x5D:{nes_eor(nes, nes_abx(nes)); nes->nes_cpu.cycles += 4;break;}// EOR ABX     4*
        case 0x5E:{nes_lsr(nes, nes_abx2(nes));nes->nes_cpu.cycles += 7;break;}// LSR ABX     7
        case 0x5F:{nes_sre(nes, nes_abx2(nes));nes->nes_cpu.cycles += 7;break;}// SRE ABX     7
        case 0x60:{
            const uint8_t *stack = nes->nes_cpu.cpu_ram + 0x100;
            const uint8_t low_byte = stack[++nes->nes_cpu.SP];
            const uint8_t high_byte = stack[++nes->nes_cpu.SP];
            nes->nes_cpu.PC = (uint16_t)((((uint16_t)high_byte << 8) | low_byte) + 1U);
            nes->nes_cpu.cycles += 6;
            break;
        }// RTS         6
        case 0x61:{nes_adc(nes, nes_izx(nes)); nes->nes_cpu.cycles += 6;break;}// ADC IZX     6
        case 0x62:{                                                     break;}// KIL         0
        case 0x63:{nes_rra(nes, nes_izx(nes)); nes->nes_cpu.cycles += 8;break;}// RRA IZX     8
        case 0x64:{nes_nop(nes, nes_zp(nes));  nes->nes_cpu.cycles += 3;break;}// NOP ZP      3
        case 0x65:{nes_adc_data(nes, nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 3;break;}// ADC ZP      3
        case 0x66:{nes_ror_ram(nes, &nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 5;break;}// ROR ZP      5
        case 0x67:{nes_rra(nes, nes_zp(nes));  nes->nes_cpu.cycles += 5;break;}// RRA ZP      5
        case 0x68:{nes_pla(nes, 0);            nes->nes_cpu.cycles += 4;break;}// PLA         4
        case 0x69:{nes_adc_data(nes, nes_fetch_cpu(nes)); nes->nes_cpu.cycles += 2;break;}// ADC IMM     2
        case 0x6A:{nes_rora(nes, 0);           nes->nes_cpu.cycles += 2;break;}// ROR         2
        case 0x6B:{nes_arr(nes, nes_imm(nes)); nes->nes_cpu.cycles += 2;break;}// ARR IMM     2
        case 0x6C:{nes_jmp(nes, nes_ind(nes)); nes->nes_cpu.cycles += 5;break;}// JMP IND     5
        case 0x6D:{nes_adc(nes, nes_abs(nes)); nes->nes_cpu.cycles += 4;break;}// ADC ABS     4
        case 0x6E:{nes_ror(nes, nes_abs(nes)); nes->nes_cpu.cycles += 6;break;}// ROR ABS     6
        case 0x6F:{nes_rra(nes, nes_abs(nes)); nes->nes_cpu.cycles += 6;break;}// RRA ABS     6
        case 0x70:{
#if CONFIG_NES_FAST_CPU_BRANCH
            nes_branch_fast(nes, (NES_CPU_P & (uint8_t)NES_FLAG_V) != 0U, ticks);
            break;
#else
            nes_bvs(nes, nes_rel(nes));
            nes->nes_cpu.cycles += 2;break;
#endif
        }// BVS REL     2*
        case 0x71:{nes_adc(nes, nes_izy(nes)); nes->nes_cpu.cycles += 5;break;}// ADC IZY     5*
        case 0x72:{                                                     break;}// KIL         0
        case 0x73:{nes_rra(nes, nes_izy2(nes));nes->nes_cpu.cycles += 8;break;}// RRA IZY     8
        case 0x74:{nes_nop(nes, nes_zpx(nes)); nes->nes_cpu.cycles += 4;break;}// NOP ZPX     4
        case 0x75:{nes_adc_data(nes, nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff]); nes->nes_cpu.cycles += 4;break;}// ADC ZPX     4
        case 0x76:{nes_ror_ram(nes, &nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff]); nes->nes_cpu.cycles += 6;break;}// ROR ZPX     6
        case 0x77:{nes_rra(nes, nes_zpx(nes)); nes->nes_cpu.cycles += 6;break;}// RRA ZPX     6
        case 0x78:{nes_sei(nes, 0);            nes->nes_cpu.cycles += 2;break;}// SEI         2
        case 0x79:{nes_adc(nes, nes_aby(nes)); nes->nes_cpu.cycles += 4;break;}// ADC ABY     4*
        case 0x7A:{nes_nop(nes, 0);            nes->nes_cpu.cycles += 2;break;}// NOP         2
        case 0x7B:{nes_rra(nes, nes_aby2(nes));nes->nes_cpu.cycles += 7;break;}// RRA ABY     7
        case 0x7C:{nes_nop(nes, nes_abx(nes)); nes->nes_cpu.cycles += 4;break;}// NOP ABX     4*
        case 0x7D:{nes_adc(nes, nes_abx(nes)); nes->nes_cpu.cycles += 4;break;}// ADC ABX     4*
        case 0x7E:{nes_ror(nes, nes_abx2(nes));nes->nes_cpu.cycles += 7;break;}// ROR ABX     7
        case 0x7F:{nes_rra(nes, nes_abx2(nes));nes->nes_cpu.cycles += 7;break;}// RRA ABX     7
        case 0x80:{nes_nop(nes, nes_imm(nes)); nes->nes_cpu.cycles += 2;break;}// NOP IMM     2
        case 0x81:{nes_write_data_fast(nes, nes_izx(nes), nes->nes_cpu.A); nes->nes_cpu.cycles += 6;break;}// STA IZX     6
        case 0x82:{nes_nop(nes, nes_imm(nes)); nes->nes_cpu.cycles += 2;break;}// NOP IMM     2
        case 0x83:{nes_sax(nes, nes_izx(nes)); nes->nes_cpu.cycles += 6;break;}// SAX IZX     6
        case 0x84:{nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)] = nes->nes_cpu.Y; nes->nes_cpu.cycles += 3;break;}// STY ZP      3
        case 0x85:{nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)] = nes->nes_cpu.A; nes->nes_cpu.cycles += 3;break;}// STA ZP      3
        case 0x86:{nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)] = nes->nes_cpu.X; nes->nes_cpu.cycles += 3;break;}// STX ZP      3
        case 0x87:{nes_sax(nes, nes_zp(nes));  nes->nes_cpu.cycles += 3;break;}// SAX ZP      3
        case 0x88:{nes->nes_cpu.Y--; nes_set_nz_fast(nes, nes->nes_cpu.Y); nes->nes_cpu.cycles += 2;break;}// DEY         2
        case 0x89:{nes_nop(nes, nes_imm(nes)); nes->nes_cpu.cycles += 2;break;}// NOP IMM     2
        case 0x8A:{nes->nes_cpu.A = nes->nes_cpu.X; nes_set_nz_fast(nes, nes->nes_cpu.A); nes->nes_cpu.cycles += 2;break;}// TXA         2
        case 0x8B:{nes_xaa(nes, nes_imm(nes)); nes->nes_cpu.cycles += 2;break;}// XAA IMM     2
        case 0x8C:{nes_sty(nes, nes_abs(nes)); nes->nes_cpu.cycles += 4;break;}// STY ABS     4
        case 0x8D:{nes_write_data_fast(nes, nes_abs(nes), nes->nes_cpu.A); nes->nes_cpu.cycles += 4;break;}// STA ABS     4
        case 0x8E:{nes_stx(nes, nes_abs(nes)); nes->nes_cpu.cycles += 4;break;}// STX ABS     4
        case 0x8F:{nes_sax(nes, nes_abs(nes)); nes->nes_cpu.cycles += 4;break;}// SAX ABS     4
        case 0x90:{
#if CONFIG_NES_FAST_CPU_BRANCH
            nes_branch_fast(nes, (NES_CPU_P & (uint8_t)NES_FLAG_C) == 0U, ticks);
            break;
#else
            nes_bcc(nes, nes_rel(nes));
            nes->nes_cpu.cycles += 2;break;
#endif
        }// BCC REL     2*
        case 0x91:{nes_write_data_fast(nes, nes_izy2(nes), nes->nes_cpu.A); nes->nes_cpu.cycles += 6;break;}// STA IZY     6
        case 0x92:{                                                     break;}// KIL         0
        case 0x93:{nes_ahx(nes, nes_izy2(nes));nes->nes_cpu.cycles += 6;break;}// AHX IZY     6
        case 0x94:{nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff] = nes->nes_cpu.Y; nes->nes_cpu.cycles += 4;break;}// STY ZPX     4
        case 0x95:{nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff] = nes->nes_cpu.A; nes->nes_cpu.cycles += 4;break;}// STA ZPX     4
        case 0x96:{nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.Y) & 0xff] = nes->nes_cpu.X; nes->nes_cpu.cycles += 4;break;}// STX ZPY     4
        case 0x97:{nes_sax(nes, nes_zpy(nes)); nes->nes_cpu.cycles += 4;break;}// SAX ZPY     4
        case 0x98:{nes->nes_cpu.A = nes->nes_cpu.Y; nes_set_nz_fast(nes, nes->nes_cpu.A); nes->nes_cpu.cycles += 2;break;}// TYA         2
        case 0x99:{nes_write_data_fast(nes, nes_aby2(nes), nes->nes_cpu.A); nes->nes_cpu.cycles += 5;break;}// STA ABY     5
        case 0x9A:{nes->nes_cpu.SP = nes->nes_cpu.X; nes->nes_cpu.cycles += 2;break;}// TXS         2
        case 0x9B:{nes_tas(nes, nes_aby2(nes));nes->nes_cpu.cycles += 5;break;}// TAS ABY     5
        case 0x9C:{nes_shy(nes, nes_abx2(nes));nes->nes_cpu.cycles += 5;break;}// SHY ABX     5
        case 0x9D:{nes_write_data_fast(nes, nes_abx2(nes), nes->nes_cpu.A); nes->nes_cpu.cycles += 5;break;}// STA ABX     5
        case 0x9E:{nes_shx(nes, nes_aby2(nes));nes->nes_cpu.cycles += 5;break;}// SHX ABY     5
        case 0x9F:{nes_ahx(nes, nes_aby2(nes));nes->nes_cpu.cycles += 5;break;}// AHX ABY     5
        case 0xA0:{nes->nes_cpu.Y = nes_fetch_cpu(nes); nes_set_nz_fast(nes, nes->nes_cpu.Y); nes->nes_cpu.cycles += 2;break;}// LDY IMM     2
        case 0xA1:{nes_lda_data_fast(nes, nes_read_data_fast(nes, nes_izx(nes))); nes->nes_cpu.cycles += 6;break;}// LDA IZX     6
        case 0xA2:{nes->nes_cpu.X = nes_fetch_cpu(nes); nes_set_nz_fast(nes, nes->nes_cpu.X); nes->nes_cpu.cycles += 2;break;}// LDX IMM     2
        case 0xA3:{nes_lax(nes, nes_izx(nes)); nes->nes_cpu.cycles += 6;break;}// LAX IZX     6
        case 0xA4:{nes->nes_cpu.Y = nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]; nes_set_nz_fast(nes, nes->nes_cpu.Y); nes->nes_cpu.cycles += 3;break;}// LDY ZP      3
        case 0xA5:{nes_lda_data_fast(nes, nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 3;break;}// LDA ZP      3
        case 0xA6:{nes->nes_cpu.X = nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]; nes_set_nz_fast(nes, nes->nes_cpu.X); nes->nes_cpu.cycles += 3;break;}// LDX ZP      3
        case 0xA7:{nes_lax(nes, nes_zp(nes));  nes->nes_cpu.cycles += 3;break;}// LAX ZP      3
        case 0xA8:{nes->nes_cpu.Y = nes->nes_cpu.A; nes_set_nz_fast(nes, nes->nes_cpu.Y); nes->nes_cpu.cycles += 2;break;}// TAY         2
        case 0xA9:{nes_lda_data_fast(nes, nes_fetch_cpu(nes)); nes->nes_cpu.cycles += 2;break;}// LDA IMM     2
        case 0xAA:{nes->nes_cpu.X = nes->nes_cpu.A; nes_set_nz_fast(nes, nes->nes_cpu.X); nes->nes_cpu.cycles += 2;break;}// TAX         2
        case 0xAB:{nes_lax(nes, nes_imm(nes)); nes->nes_cpu.cycles += 2;break;}// LAX IMM     2
        case 0xAC:{nes_ldy(nes, nes_abs(nes)); nes->nes_cpu.cycles += 4;break;}// LDY ABS     4
        case 0xAD:{nes_lda_data_fast(nes, nes_read_data_fast(nes, nes_abs(nes))); nes->nes_cpu.cycles += 4;break;}// LDA ABS     4
        case 0xAE:{nes_ldx(nes, nes_abs(nes)); nes->nes_cpu.cycles += 4;break;}// LDX ABS     4
        case 0xAF:{nes_lax(nes, nes_abs(nes)); nes->nes_cpu.cycles += 4;break;}// LAX ABS     4
        case 0xB0:{
#if CONFIG_NES_FAST_CPU_BRANCH
            nes_branch_fast(nes, (NES_CPU_P & (uint8_t)NES_FLAG_C) != 0U, ticks);
            break;
#else
            nes_bcs(nes, nes_rel(nes));
            nes->nes_cpu.cycles += 2;break;
#endif
        }// BCS REL     2*
        case 0xB1:{nes_lda_data_fast(nes, nes_read_data_fast(nes, nes_izy(nes))); nes->nes_cpu.cycles += 5;break;}// LDA IZY     5*
        case 0xB2:{                                                     break;}// KIL         0
        case 0xB3:{nes_lax(nes, nes_izy(nes)); nes->nes_cpu.cycles += 5;break;}// LAX IZY     5*
        case 0xB4:{nes->nes_cpu.Y = nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff]; nes_set_nz_fast(nes, nes->nes_cpu.Y); nes->nes_cpu.cycles += 4;break;}// LDY ZPX     4
        case 0xB5:{nes_lda_data_fast(nes, nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff]); nes->nes_cpu.cycles += 4;break;}// LDA ZPX     4
        case 0xB6:{nes->nes_cpu.X = nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.Y) & 0xff]; nes_set_nz_fast(nes, nes->nes_cpu.X); nes->nes_cpu.cycles += 4;break;}// LDX ZPY     4
        case 0xB7:{nes_lax(nes, nes_zpy(nes)); nes->nes_cpu.cycles += 4;break;}// LAX ZPY     4
        case 0xB8:{nes_clv(nes, 0);            nes->nes_cpu.cycles += 2;break;}// CLV         2
        case 0xB9:{nes_lda_data_fast(nes, nes_read_data_fast(nes, nes_aby(nes))); nes->nes_cpu.cycles += 4;break;}// LDA ABY     4*
        case 0xBA:{nes->nes_cpu.X = nes->nes_cpu.SP; nes_set_nz_fast(nes, nes->nes_cpu.X); nes->nes_cpu.cycles += 2;break;}// TSX         2
        case 0xBB:{nes_las(nes, nes_aby(nes)); nes->nes_cpu.cycles += 4;break;}// LAS ABY     4*
        case 0xBC:{nes_ldy(nes, nes_abx(nes)); nes->nes_cpu.cycles += 4;break;}// LDY ABX     4*
        case 0xBD:{nes_lda_data_fast(nes, nes_read_data_fast(nes, nes_abx(nes))); nes->nes_cpu.cycles += 4;break;}// LDA ABX     4*
        case 0xBE:{nes_ldx(nes, nes_aby(nes)); nes->nes_cpu.cycles += 4;break;}// LDX ABY     4*
        case 0xBF:{nes_lax(nes, nes_aby(nes)); nes->nes_cpu.cycles += 4;break;}// LAX ABY     4*
        case 0xC0:{nes_cpy_data(nes, nes_fetch_cpu(nes)); nes->nes_cpu.cycles += 2;break;}// CPY IMM     2
        case 0xC1:{nes_cmp_data_fast(nes, nes->nes_cpu.A, nes_read_data_fast(nes, nes_izx(nes))); nes->nes_cpu.cycles += 6;break;}// CMP IZX     6
        case 0xC2:{nes_nop(nes, nes_imm(nes)); nes->nes_cpu.cycles += 2;break;}// NOP IMM     2
        case 0xC3:{nes_dcp(nes, nes_izx(nes)); nes->nes_cpu.cycles += 8;break;}// DCP IZX     8
        case 0xC4:{nes_cpy_data(nes, nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 3;break;}// CPY ZP      3
        case 0xC5:{nes_cmp_data_fast(nes, nes->nes_cpu.A, nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 3;break;}// CMP ZP      3
        case 0xC6:{nes_dec_ram(nes, &nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 5;break;}// DEC ZP      5
        case 0xC7:{nes_dcp(nes, nes_zp(nes));  nes->nes_cpu.cycles += 5;break;}// DCP ZP      5
        case 0xC8:{nes->nes_cpu.Y++; nes_set_nz_fast(nes, nes->nes_cpu.Y); nes->nes_cpu.cycles += 2;break;}// INY         2
        case 0xC9:{nes_cmp_data_fast(nes, nes->nes_cpu.A, nes_fetch_cpu(nes)); nes->nes_cpu.cycles += 2;break;}// CMP IMM     2
        case 0xCA:{nes->nes_cpu.X--; nes_set_nz_fast(nes, nes->nes_cpu.X); nes->nes_cpu.cycles += 2;break;}// DEX         2
        case 0xCB:{nes_axs(nes, nes_imm(nes)); nes->nes_cpu.cycles += 2;break;}// AXS IMM     2
        case 0xCC:{nes_cpy(nes, nes_abs(nes)); nes->nes_cpu.cycles += 4;break;}// CPY ABS     4
        case 0xCD:{nes_cmp_data_fast(nes, nes->nes_cpu.A, nes_read_data_fast(nes, nes_abs(nes))); nes->nes_cpu.cycles += 4;break;}// CMP ABS     4
        case 0xCE:{nes_dec(nes, nes_abs(nes)); nes->nes_cpu.cycles += 6;break;}// DEC ABS     6
        case 0xCF:{nes_dcp(nes, nes_abs(nes)); nes->nes_cpu.cycles += 6;break;}// DCP ABS     6
        case 0xD0:{
#if CONFIG_NES_FAST_CPU_BRANCH
            nes_branch_bne_fast(nes, ticks);
            break;
#else
            nes_bne(nes, nes_rel(nes));
            nes->nes_cpu.cycles += 2;break;
#endif
        }// BNE REL     2*
        case 0xD1:{nes_cmp_data_fast(nes, nes->nes_cpu.A, nes_read_data_fast(nes, nes_izy(nes))); nes->nes_cpu.cycles += 5;break;}// CMP IZY     5*
        case 0xD2:{                                                     break;}// KIL         0
        case 0xD3:{nes_dcp(nes, nes_izy2(nes));nes->nes_cpu.cycles += 8;break;}// DCP IZY     8
        case 0xD4:{nes_nop(nes, nes_zpx(nes)); nes->nes_cpu.cycles += 4;break;}// NOP ZPX     4
        case 0xD5:{nes_cmp_data_fast(nes, nes->nes_cpu.A, nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff]); nes->nes_cpu.cycles += 4;break;}// CMP ZPX     4
        case 0xD6:{nes_dec_ram(nes, &nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff]); nes->nes_cpu.cycles += 6;break;}// DEC ZPX     6
        case 0xD7:{nes_dcp(nes, nes_zpx(nes)); nes->nes_cpu.cycles += 6;break;}// DCP ZPX     6
        case 0xD8:{nes_cld(nes, 0);            nes->nes_cpu.cycles += 2;break;}// CLD         2
        case 0xD9:{nes_cmp_data_fast(nes, nes->nes_cpu.A, nes_read_data_fast(nes, nes_aby(nes))); nes->nes_cpu.cycles += 4;break;}// CMP ABY     4*
        case 0xDA:{nes_nop(nes, 0);            nes->nes_cpu.cycles += 2;break;}// NOP         2
        case 0xDB:{nes_dcp(nes, nes_aby2(nes));nes->nes_cpu.cycles += 7;break;}// DCP ABY     7
        case 0xDC:{nes_nop(nes, nes_abx(nes)); nes->nes_cpu.cycles += 4;break;}// NOP ABX     4*
        case 0xDD:{nes_cmp_data_fast(nes, nes->nes_cpu.A, nes_read_data_fast(nes, nes_abx(nes))); nes->nes_cpu.cycles += 4;break;}// CMP ABX     4*
        case 0xDE:{nes_dec(nes, nes_abx2(nes));nes->nes_cpu.cycles += 7;break;}// DEC ABX     7
        case 0xDF:{nes_dcp(nes, nes_abx2(nes));nes->nes_cpu.cycles += 7;break;}// DCP ABX     7
        case 0xE0:{nes_cpx_data(nes, nes_fetch_cpu(nes)); nes->nes_cpu.cycles += 2;break;}// CPX IMM     2
        case 0xE1:{nes_sbc(nes, nes_izx(nes)); nes->nes_cpu.cycles += 6;break;}// SBC IZX     6
        case 0xE2:{nes_nop(nes, nes_imm(nes)); nes->nes_cpu.cycles += 2;break;}// NOP IMM     2
        case 0xE3:{nes_isc(nes, nes_izx(nes)); nes->nes_cpu.cycles += 8;break;}// ISC IZX     8
        case 0xE4:{nes_cpx_data(nes, nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 3;break;}// CPX ZP      3
        case 0xE5:{nes_sbc_data(nes, nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 3;break;}// SBC ZP      3
        case 0xE6:{nes_inc_ram(nes, &nes->nes_cpu.cpu_ram[nes_fetch_cpu(nes)]); nes->nes_cpu.cycles += 5;break;}// INC ZP      5
        case 0xE7:{nes_isc(nes, nes_zp(nes));  nes->nes_cpu.cycles += 5;break;}// ISC ZP      5
        case 0xE8:{nes->nes_cpu.X++; nes_set_nz_fast(nes, nes->nes_cpu.X); nes->nes_cpu.cycles += 2;break;}// INX         2
        case 0xE9:{nes_sbc_data(nes, nes_fetch_cpu(nes)); nes->nes_cpu.cycles += 2;break;}// SBC IMM     2
        case 0xEA:{nes_nop(nes, 0);            nes->nes_cpu.cycles += 2;break;}// NOP         2
        case 0xEB:{nes_sbc_data(nes, nes_fetch_cpu(nes)); nes->nes_cpu.cycles += 2;break;}// SBC IMM     2
        case 0xEC:{nes_cpx(nes, nes_abs(nes)); nes->nes_cpu.cycles += 4;break;}// CPX ABS     4
        case 0xED:{nes_sbc(nes, nes_abs(nes)); nes->nes_cpu.cycles += 4;break;}// SBC ABS     4
        case 0xEE:{nes_inc(nes, nes_abs(nes)); nes->nes_cpu.cycles += 6;break;}// INC ABS     6
        case 0xEF:{nes_isc(nes, nes_abs(nes)); nes->nes_cpu.cycles += 6;break;}// ISC ABS     6
        case 0xF0:{
#if CONFIG_NES_FAST_CPU_BRANCH
            nes_branch_fast(nes, (NES_CPU_P & (uint8_t)NES_FLAG_Z) != 0U, ticks);
            break;
#else
            nes_beq(nes, nes_rel(nes));
            nes->nes_cpu.cycles += 2;break;
#endif
        }// BEQ REL     2*
        case 0xF1:{nes_sbc(nes, nes_izy(nes)); nes->nes_cpu.cycles += 5;break;}// SBC IZY     5*
        case 0xF2:{                                                     break;}// KIL         0
        case 0xF3:{nes_isc(nes, nes_izy2(nes));nes->nes_cpu.cycles += 8;break;}// ISC IZY     8
        case 0xF4:{nes_nop(nes, nes_zpx(nes)); nes->nes_cpu.cycles += 4;break;}// NOP ZPX     4
        case 0xF5:{nes_sbc_data(nes, nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff]); nes->nes_cpu.cycles += 4;break;}// SBC ZPX     4
        case 0xF6:{nes_inc_ram(nes, &nes->nes_cpu.cpu_ram[(nes_fetch_cpu(nes) + nes->nes_cpu.X) & 0xff]); nes->nes_cpu.cycles += 6;break;}// INC ZPX     6
        case 0xF7:{nes_isc(nes, nes_zpx(nes)); nes->nes_cpu.cycles += 6;break;}// ISC ZPX     6
        case 0xF8:{nes_sed(nes, 0);            nes->nes_cpu.cycles += 2;break;}// SED         2
        case 0xF9:{nes_sbc(nes, nes_aby(nes)); nes->nes_cpu.cycles += 4;break;}// SBC ABY     4*
        case 0xFA:{nes_nop(nes, 0);            nes->nes_cpu.cycles += 2;break;}// NOP         2
        case 0xFB:{nes_isc(nes, nes_aby2(nes));nes->nes_cpu.cycles += 7;break;}// ISC ABY     7
        case 0xFC:{nes_nop(nes, nes_abx(nes)); nes->nes_cpu.cycles += 4;break;}// NOP ABX     4*
        case 0xFD:{nes_sbc(nes, nes_abx(nes)); nes->nes_cpu.cycles += 4;break;}// SBC ABX     4*
        case 0xFE:{nes_inc(nes, nes_abx2(nes));nes->nes_cpu.cycles += 7;break;}// INC ABX     7
        case 0xFF:{nes_isc(nes, nes_abx2(nes));nes->nes_cpu.cycles += 7;break;}// ISC ABX     7
        default:
            break;
        }
#ifdef __DEBUG__
        // cycles += nes->nes_cpu.cycles - cycles_old;
        // fprintf(debug_fp,"\nopcode: %s \n",nes_opcode_name[nes->nes_cpu.opcode]);
#endif
    }
    nes_opcode_profile_log();
    nes->nes_cpu.cycles -= ticks;
}
