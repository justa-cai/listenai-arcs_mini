/*
 * Shared declarations for split InfoNES mapper compatibility bodies.
 */
#pragma once

#include <stddef.h>
#include "nes.h"
#include "nes_mapper.h"
#include "nes_mapper_infones_config.h"

#define INFONES_PRG_PAGE_SIZE 0x2000u
#define INFONES_CHR_PAGE_SIZE 0x0400u
#define INFONES_DRAM_SIZE     0xA000u
#define DRAM_SIZE             INFONES_DRAM_SIZE
#define APU_MAX_SAMPLES_PER_SYNC 882u
#define STEP_PER_SCANLINE NES_PPU_CPU_CLOCKS

typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;

struct NesHeader_tag {
    BYTE byID[4];
    BYTE byRomSize;
    BYTE byVRomSize;
    BYTE byInfo1;
    BYTE byInfo2;
    BYTE byReserve[8];
};

extern nes_t *infones_current_nes;
extern struct NesHeader_tag NesHeader;
extern BYTE MapperNo;
extern BYTE ROM_Mirroring;
extern BYTE ROM_SRAM;
extern BYTE ROM_Trainer;
extern BYTE ROM_FourScr;
extern BYTE *SRAM;
extern BYTE *SRAMBANK;
extern BYTE *DRAM;
extern BYTE byVramWriteEnable;
extern BYTE ChrBufUpdate;
extern BYTE FrameIRQ_Enable;
extern WORD FrameStep;
extern BYTE IRQ_State;
extern BYTE IRQ_Wiring;
extern BYTE NMI_State;
extern BYTE APU_Reg[0x20];
extern BYTE ApuFdsEnable;
extern BYTE ApuMmc5Enable;
extern BYTE ApuSunsoft5BEnable;
extern BYTE ApuVrc6Enable;
extern BYTE ApuMmc5P1Atl;
extern BYTE ApuMmc5P2Atl;
extern BYTE ApuMmc5PcmValue;

extern void (*MapperInit)(void);
extern void (*MapperWrite)(WORD wAddr, BYTE byData);
extern void (*MapperSram)(WORD wAddr, BYTE byData);
extern void (*MapperApu)(WORD wAddr, BYTE byData);
extern BYTE (*MapperReadApu)(WORD wAddr);
extern void (*MapperVSync)(void);
extern void (*MapperHSync)(void);
extern void (*MapperPPU)(WORD wAddr);
extern void (*MapperRenderScreen)(BYTE byMode);
extern int (*MapperBlobSize)(void);
extern void (*MapperSaveBlob)(BYTE *pBuf);
extern void (*MapperLoadBlob)(BYTE *pBuf);

#define ROM      (infones_current_nes->nes_rom.prg_rom)
#define ROMBANK  (infones_current_nes->nes_cpu.prg_banks)
#define ROMBANK0 (infones_current_nes->nes_cpu.prg_banks[0])
#define ROMBANK1 (infones_current_nes->nes_cpu.prg_banks[1])
#define ROMBANK2 (infones_current_nes->nes_cpu.prg_banks[2])
#define ROMBANK3 (infones_current_nes->nes_cpu.prg_banks[3])
#define PPUBANK  (infones_current_nes->nes_ppu.chr_banks)
#define NAME_TABLE0 8
#define NAME_TABLE1 9
#define NAME_TABLE2 10
#define NAME_TABLE3 11
#define PPU_Scanline (infones_current_nes->scanline)
#define PPU_R1 (infones_current_nes->nes_ppu.ppu_mask)
#define R1_SHOW_SCR 0x08
#define R1_SHOW_SP  0x10
#define InfoNES_MemorySet nes_memset

static inline BYTE *infones_prg_page(DWORD page)
{
    const DWORD total_pages = (DWORD)infones_current_nes->nes_rom.prg_rom_size * 2u;
    if (!infones_current_nes->nes_rom.prg_rom || total_pages == 0) {
        return NULL;
    }
    return infones_current_nes->nes_rom.prg_rom + (page % total_pages) * INFONES_PRG_PAGE_SIZE;
}

static inline BYTE *infones_chr_page(DWORD page)
{
    DWORD total_pages = (DWORD)infones_current_nes->nes_rom.chr_rom_size * 8u;
    if (total_pages == 0) {
        total_pages = 8;
    }
    if (!infones_current_nes->nes_rom.chr_rom) {
        return NULL;
    }
    return infones_current_nes->nes_rom.chr_rom + (page % total_pages) * INFONES_CHR_PAGE_SIZE;
}

#define ROMPAGE(a) infones_prg_page((DWORD)(a))
#define ROMLASTPAGE(a) infones_prg_page((DWORD)(((NesHeader.byRomSize << 1) - ((a) + 1))))
#define VROMPAGE(a) infones_chr_page((DWORD)(a))
#define CRAMPAGE(a) infones_chr_page((DWORD)(a))
#define VRAMPAGE(a) (infones_current_nes->nes_ppu.ppu_vram[(a) & 0x03])
#define PATTBL(a) (0)
#define IRQ_REQ infones_irq_request()
#define NMI_REQ do { NMI_State = 0; } while (0)

void InfoNES_Mirroring(int nType);
void InfoNES_SetupChr(void);
void K6502_Set_Int_Wiring(BYTE byNMI_Wiring, BYTE byIRQ_Wiring);
void infones_irq_request(void);

static inline void ApuWriteMmc5Ctrl(WORD wAddr, BYTE byData) { (void)wAddr; (void)byData; }
static inline void ApuWriteMmc5P1a(WORD wAddr, BYTE byData) { (void)wAddr; ApuMmc5P1Atl = byData; }
static inline void ApuWriteMmc5P1c(WORD wAddr, BYTE byData) { (void)wAddr; (void)byData; }
static inline void ApuWriteMmc5P1d(WORD wAddr, BYTE byData) { (void)wAddr; (void)byData; }
static inline void ApuWriteMmc5P2a(WORD wAddr, BYTE byData) { (void)wAddr; ApuMmc5P2Atl = byData; }
static inline void ApuWriteMmc5P2c(WORD wAddr, BYTE byData) { (void)wAddr; (void)byData; }
static inline void ApuWriteMmc5P2d(WORD wAddr, BYTE byData) { (void)wAddr; (void)byData; }
static inline void ApuWriteSunsoft5B(BYTE reg, BYTE byData) { (void)reg; (void)byData; }
static inline void ApuWriteVrc6Freq(WORD wAddr, BYTE byData) { (void)wAddr; (void)byData; }
static inline void ApuWriteVrc6P1a(WORD wAddr, BYTE byData) { (void)wAddr; (void)byData; }
static inline void ApuWriteVrc6P1b(WORD wAddr, BYTE byData) { (void)wAddr; (void)byData; }
static inline void ApuWriteVrc6P1c(WORD wAddr, BYTE byData) { (void)wAddr; (void)byData; }
static inline void ApuWriteVrc6P2a(WORD wAddr, BYTE byData) { (void)wAddr; (void)byData; }
static inline void ApuWriteVrc6P2b(WORD wAddr, BYTE byData) { (void)wAddr; (void)byData; }
static inline void ApuWriteVrc6P2c(WORD wAddr, BYTE byData) { (void)wAddr; (void)byData; }
static inline void ApuWriteVrc6SawA(WORD wAddr, BYTE byData) { (void)wAddr; (void)byData; }
static inline void ApuWriteVrc6SawB(WORD wAddr, BYTE byData) { (void)wAddr; (void)byData; }
static inline void ApuWriteVrc6SawC(WORD wAddr, BYTE byData) { (void)wAddr; (void)byData; }

struct infones_mapper_entry {
    uint16_t mapper_id;
    void (*init)(void);
};

extern const struct infones_mapper_entry infones_mapper_table[];
extern const size_t infones_mapper_count;

/* Mapper 206 is a Namcot 118 variant and reuses Mapper 4's MMC3 state. */
extern BYTE Map4_Regs[8];
extern DWORD Map4_Rom_Bank;
extern DWORD Map4_Prg0;
extern DWORD Map4_Prg1;
extern DWORD Map4_Chr01;
extern DWORD Map4_Chr23;
extern DWORD Map4_Chr4;
extern DWORD Map4_Chr5;
extern DWORD Map4_Chr6;
extern DWORD Map4_Chr7;
extern BYTE Map4_IRQ_Enable;
extern BYTE Map4_IRQ_Cnt;
extern BYTE Map4_IRQ_Latch;
extern BYTE Map4_IRQ_Request;
extern BYTE Map4_IRQ_Present;
extern BYTE Map4_IRQ_Present_Vbl;

#if INFONES_ENABLE_MAPPER_019
extern BYTE *Map19_Chr_Ram;
#endif
#if INFONES_ENABLE_MAPPER_024
extern BYTE (*vrc6_wave_buffers)[APU_MAX_SAMPLES_PER_SYNC];
#endif
#if INFONES_ENABLE_MAPPER_069
extern BYTE (*s5b_wave_buffers)[APU_MAX_SAMPLES_PER_SYNC];
#endif
#if INFONES_ENABLE_MAPPER_185
extern BYTE *Map185_Dummy_Chr_Rom;
#endif

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wparentheses"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

BYTE Map0_ReadApu(WORD wAddr);
BYTE Map187_ReadApu(WORD wAddr);
BYTE Map19_ReadApu(WORD wAddr);
BYTE Map255_ReadApu(WORD wAddr);
BYTE Map43_ReadApu(WORD wAddr);
BYTE Map83_ReadApu(WORD wAddr);
BYTE Map90_ReadApu(WORD wAddr);
void Map0_Apu(WORD wAddr, BYTE byData);
void Map0_HSync(void);
void Map0_Init(void);
void Map0_PPU(WORD wAddr);
void Map0_RenderScreen(BYTE byMode);
void Map0_Sram(WORD wAddr, BYTE byData);
void Map0_VSync(void);
void Map0_Write(WORD wAddr, BYTE byData);
void Map100_HSync(void);
void Map100_Init(void);
void Map100_Set_CPU_Banks(void);
void Map100_Set_PPU_Banks(void);
void Map100_Write(WORD wAddr, BYTE byData);
void Map101_Init(void);
void Map101_Write(WORD wAddr, BYTE byData);
void Map105_HSync(void);
void Map105_Init(void);
void Map105_Write(WORD wAddr, BYTE byData);
void Map107_Init(void);
void Map107_Write(WORD wAddr, BYTE byData);
void Map108_Init(void);
void Map108_Write(WORD wAddr, BYTE byData);
void Map109_Apu(WORD wAddr, BYTE byData);
void Map109_Init(void);
void Map109_Set_PPU_Banks(void);
void Map10_Init(void);
void Map10_PPU(WORD wAddr);
void Map10_Write(WORD wAddr, BYTE byData);
void Map110_Apu(WORD wAddr, BYTE byData);
void Map110_Init(void);
void Map112_HSync(void);
void Map112_Init(void);
void Map112_Set_CPU_Banks(void);
void Map112_Set_PPU_Banks(void);
void Map112_Write(WORD wAddr, BYTE byData);
void Map113_Apu(WORD wAddr, BYTE byData);
void Map113_Init(void);
void Map113_Write(WORD wAddr, BYTE byData);
void Map114_HSync(void);
void Map114_Init(void);
void Map114_Set_CPU_Banks(void);
void Map114_Set_PPU_Banks(void);
void Map114_Sram(WORD wAddr, BYTE byData);
void Map114_Write(WORD wAddr, BYTE byData);
void Map115_HSync(void);
void Map115_Init(void);
void Map115_Set_CPU_Banks(void);
void Map115_Set_PPU_Banks(void);
void Map115_Sram(WORD wAddr, BYTE byData);
void Map115_Write(WORD wAddr, BYTE byData);
void Map116_HSync(void);
void Map116_Init(void);
void Map116_Set_CPU_Banks(void);
void Map116_Set_PPU_Banks(void);
void Map116_Write(WORD wAddr, BYTE byData);
void Map117_HSync(void);
void Map117_Init(void);
void Map117_Write(WORD wAddr, BYTE byData);
void Map118_HSync(void);
void Map118_Init(void);
void Map118_Set_CPU_Banks(void);
void Map118_Set_PPU_Banks(void);
void Map118_Write(WORD wAddr, BYTE byData);
void Map119_HSync(void);
void Map119_Init(void);
void Map119_Set_CPU_Banks(void);
void Map119_Set_PPU_Banks(void);
void Map119_Write(WORD wAddr, BYTE byData);
void Map11_Init(void);
void Map11_Write(WORD wAddr, BYTE byData);
void Map122_Init(void);
void Map122_Sram(WORD wAddr, BYTE byData);
void Map133_Apu(WORD wAddr, BYTE byData);
void Map133_Init(void);
void Map134_Apu(WORD wAddr, BYTE byData);
void Map134_Init(void);
void Map135_Apu(WORD wAddr, BYTE byData);
void Map135_Init(void);
void Map135_Set_PPU_Banks(void);
void Map13_Init(void);
void Map13_Write(WORD wAddr, BYTE byData);
void Map140_Apu(WORD wAddr, BYTE byData);
void Map140_Init(void);
void Map140_Sram(WORD wAddr, BYTE byData);
void Map151_Init(void);
void Map151_Write(WORD wAddr, BYTE byData);
void Map15_Init(void);
void Map15_Write(WORD wAddr, BYTE byData);
void Map160_HSync(void);
void Map160_Init(void);
void Map160_Write(WORD wAddr, BYTE byData);
void Map16_HSync(void);
void Map16_Init(void);
void Map16_Write(WORD wAddr, BYTE byData);
void Map17_Apu(WORD wAddr, BYTE byData);
void Map17_HSync(void);
void Map17_Init(void);
void Map180_Init(void);
void Map180_Write(WORD wAddr, BYTE byData);
void Map181_Apu(WORD wAddr, BYTE byData);
void Map181_Init(void);
void Map182_HSync(void);
void Map182_Init(void);
void Map182_Write(WORD wAddr, BYTE byData);
void Map183_HSync(void);
void Map183_Init(void);
void Map183_Write(WORD wAddr, BYTE byData);
void Map184_Init(void);
void Map184_Sram(WORD wAddr, BYTE byData);
void Map185_Init(void);
void Map185_Write(WORD wAddr, BYTE byData);
void Map187_Apu(WORD wAddr, BYTE byData);
void Map187_HSync(void);
void Map187_Init(void);
void Map187_Set_CPU_Banks(void);
void Map187_Set_PPU_Banks(void);
void Map187_Write(WORD wAddr, BYTE byData);
void Map188_Init(void);
void Map188_Write(WORD wAddr, BYTE byData);
void Map189_Apu(WORD wAddr, BYTE byData);
void Map189_HSync(void);
void Map189_Init(void);
void Map189_Write(WORD wAddr, BYTE byData);
void Map18_HSync(void);
void Map18_Init(void);
void Map18_Write(WORD wAddr, BYTE byData);
void Map191_Apu(WORD wAddr, BYTE byData);
void Map191_Init(void);
void Map191_Set_CPU_Banks(void);
void Map191_Set_PPU_Banks(void);
void Map193_Init(void);
void Map193_Sram(WORD wAddr, BYTE byData);
void Map194_Init(void);
void Map194_Write(WORD wAddr, BYTE byData);
void Map19_Apu(WORD wAddr, BYTE byData);
void Map19_HSync(void);
void Map19_Init(void);
void Map19_Write(WORD wAddr, BYTE byData);
void Map1_Init(void);
void Map1_Write(WORD wAddr, BYTE byData);
void Map1_set_ROM_banks(void);
void Map200_Init(void);
void Map200_Write(WORD wAddr, BYTE byData);
void Map201_Init(void);
void Map201_Write(WORD wAddr, BYTE byData);
void Map202_Apu(WORD wAddr, BYTE byData);
void Map202_Init(void);
void Map202_Write(WORD wAddr, BYTE byData);
void Map202_WriteSub(WORD wAddr, BYTE byData);
void Map206_Init(void);
void Map206_Write(WORD wAddr, BYTE byData);
void Map21_HSync(void);
void Map21_Init(void);
void Map21_Write(WORD wAddr, BYTE byData);
void Map222_Init(void);
void Map222_Write(WORD wAddr, BYTE byData);
void Map225_Init(void);
void Map225_Write(WORD wAddr, BYTE byData);
void Map226_Init(void);
void Map226_Write(WORD wAddr, BYTE byData);
void Map227_Init(void);
void Map227_Write(WORD wAddr, BYTE byData);
void Map228_Init(void);
void Map228_Write(WORD wAddr, BYTE byData);
void Map229_Init(void);
void Map229_Write(WORD wAddr, BYTE byData);
void Map22_Init(void);
void Map22_Write(WORD wAddr, BYTE byData);
void Map230_Init(void);
void Map230_Write(WORD wAddr, BYTE byData);
void Map231_Init(void);
void Map231_Write(WORD wAddr, BYTE byData);
void Map232_Init(void);
void Map232_Write(WORD wAddr, BYTE byData);
void Map233_Init(void);
void Map233_Write(WORD wAddr, BYTE byData);
void Map234_Init(void);
void Map234_Set_Banks(void);
void Map234_Write(WORD wAddr, BYTE byData);
void Map235_Init(void);
void Map235_Write(WORD wAddr, BYTE byData);
void Map236_Init(void);
void Map236_Write(WORD wAddr, BYTE byData);
void Map23_HSync(void);
void Map23_Init(void);
void Map23_Write(WORD wAddr, BYTE byData);
void Map240_Apu(WORD wAddr, BYTE byData);
void Map240_Init(void);
void Map241_Init(void);
void Map241_Write(WORD wAddr, BYTE byData);
void Map242_Init(void);
void Map242_Write(WORD wAddr, BYTE byData);
void Map243_Apu(WORD wAddr, BYTE byData);
void Map243_Init(void);
void Map244_Init(void);
void Map244_Write(WORD wAddr, BYTE byData);
void Map245_HSync(void);
void Map245_Init(void);
void Map245_Write(WORD wAddr, BYTE byData);
void Map246_Init(void);
void Map246_Sram(WORD wAddr, BYTE byData);
void Map248_Apu(WORD wAddr, BYTE byData);
void Map248_HSync(void);
void Map248_Init(void);
void Map248_Set_CPU_Banks(void);
void Map248_Set_PPU_Banks(void);
void Map248_Sram(WORD wAddr, BYTE byData);
void Map248_Write(WORD wAddr, BYTE byData);
void Map249_Apu(WORD wAddr, BYTE byData);
void Map249_HSync(void);
void Map249_Init(void);
void Map249_Write(WORD wAddr, BYTE byData);
void Map24_HSync(void);
void Map24_Init(void);
void Map24_Write(WORD wAddr, BYTE byData);
void Map251_Init(void);
void Map251_Set_Banks(void);
void Map251_Sram(WORD wAddr, BYTE byData);
void Map251_Write(WORD wAddr, BYTE byData);
void Map252_HSync(void);
void Map252_Init(void);
void Map252_Write(WORD wAddr, BYTE byData);
void Map255_Apu(WORD wAddr, BYTE byData);
void Map255_Init(void);
void Map255_Write(WORD wAddr, BYTE byData);
void Map25_HSync(void);
void Map25_Init(void);
void Map25_Sync_Vrom(int nBank);
void Map25_Write(WORD wAddr, BYTE byData);
void Map26_HSync(void);
void Map26_Init(void);
void Map26_Write(WORD wAddr, BYTE byData);
void Map2_Init(void);
void Map2_Write(WORD wAddr, BYTE byData);
void Map30_Init(void);
void Map30_Write(WORD wAddr, BYTE byData);
void Map32_Init(void);
void Map32_Write(WORD wAddr, BYTE byData);
void Map33_HSync(void);
void Map33_Init(void);
void Map33_Write(WORD wAddr, BYTE byData);
void Map34_Init(void);
void Map34_Sram(WORD wAddr, BYTE byData);
void Map34_Write(WORD wAddr, BYTE byData);
void Map3_Init(void);
void Map3_Write(WORD wAddr, BYTE byData);
void Map40_HSync(void);
void Map40_Init(void);
void Map40_Write(WORD wAddr, BYTE byData);
void Map41_Init(void);
void Map41_Sram(WORD wAddr, BYTE byData);
void Map41_Write(WORD wAddr, BYTE byData);
void Map42_HSync(void);
void Map42_Init(void);
void Map42_Write(WORD wAddr, BYTE byData);
void Map43_Apu(WORD wAddr, BYTE byData);
void Map43_HSync(void);
void Map43_Init(void);
void Map43_Write(WORD wAddr, BYTE byData);
void Map44_HSync(void);
void Map44_Init(void);
void Map44_Set_CPU_Banks(void);
void Map44_Set_PPU_Banks(void);
void Map44_Write(WORD wAddr, BYTE byData);
void Map45_HSync(void);
void Map45_Init(void);
void Map45_Set_CPU_Bank4(BYTE byData);
void Map45_Set_CPU_Bank5(BYTE byData);
void Map45_Set_CPU_Bank6(BYTE byData);
void Map45_Set_CPU_Bank7(BYTE byData);
void Map45_Set_PPU_Banks(void);
void Map45_Sram(WORD wAddr, BYTE byData);
void Map45_Write(WORD wAddr, BYTE byData);
void Map46_Init(void);
void Map46_Set_ROM_Banks(void);
void Map46_Sram(WORD wAddr, BYTE byData);
void Map46_Write(WORD wAddr, BYTE byData);
void Map47_HSync(void);
void Map47_Init(void);
void Map47_Set_CPU_Banks(void);
void Map47_Set_PPU_Banks(void);
void Map47_Sram(WORD wAddr, BYTE byData);
void Map47_Write(WORD wAddr, BYTE byData);
void Map48_HSync(void);
void Map48_Init(void);
void Map48_Write(WORD wAddr, BYTE byData);
void Map49_HSync(void);
void Map49_Init(void);
void Map49_Set_CPU_Banks(void);
void Map49_Set_PPU_Banks(void);
void Map49_Sram(WORD wAddr, BYTE byData);
void Map49_Write(WORD wAddr, BYTE byData);
void Map4_HSync(void);
void Map4_Init(void);
void Map4_Set_CPU_Banks(void);
void Map4_Set_PPU_Banks(void);
void Map4_Write(WORD wAddr, BYTE byData);
void Map50_Apu(WORD wAddr, BYTE byData);
void Map50_HSync(void);
void Map50_Init(void);
void Map51_Init(void);
void Map51_Set_CPU_Banks(void);
void Map51_Sram(WORD wAddr, BYTE byData);
void Map51_Write(WORD wAddr, BYTE byData);
void Map57_Init(void);
void Map57_Write(WORD wAddr, BYTE byData);
void Map58_Init(void);
void Map58_Write(WORD wAddr, BYTE byData);
void Map60_Init(void);
void Map60_Write(WORD wAddr, BYTE byData);
void Map61_Init(void);
void Map61_Write(WORD wAddr, BYTE byData);
void Map62_Init(void);
void Map62_Write(WORD wAddr, BYTE byData);
void Map64_Init(void);
void Map64_Write(WORD wAddr, BYTE byData);
void Map65_HSync(void);
void Map65_Init(void);
void Map65_Write(WORD wAddr, BYTE byData);
void Map66_Init(void);
void Map66_Write(WORD wAddr, BYTE byData);
void Map67_HSync(void);
void Map67_Init(void);
void Map67_Write(WORD wAddr, BYTE byData);
void Map68_Init(void);
void Map68_SyncMirror(void);
void Map68_Write(WORD wAddr, BYTE byData);
void Map69_HSync(void);
void Map69_Init(void);
void Map69_Write(WORD wAddr, BYTE byData);
void Map70_Init(void);
void Map70_Write(WORD wAddr, BYTE byData);
void Map71_Init(void);
void Map71_Write(WORD wAddr, BYTE byData);
void Map72_Init(void);
void Map72_Write(WORD wAddr, BYTE byData);
void Map73_HSync(void);
void Map73_Init(void);
void Map73_Write(WORD wAddr, BYTE byData);
void Map74_HSync(void);
void Map74_Init(void);
void Map74_Set_CPU_Banks(void);
void Map74_Set_PPU_Banks(void);
void Map74_Write(WORD wAddr, BYTE byData);
void Map75_Init(void);
void Map75_Write(WORD wAddr, BYTE byData);
void Map76_Init(void);
void Map76_Write(WORD wAddr, BYTE byData);
void Map77_Init(void);
void Map77_Write(WORD wAddr, BYTE byData);
void Map78_Init(void);
void Map78_Write(WORD wAddr, BYTE byData);
void Map79_Apu(WORD wAddr, BYTE byData);
void Map79_Init(void);
void Map7_Init(void);
void Map7_Write(WORD wAddr, BYTE byData);
void Map80_Init(void);
void Map80_Sram(WORD wAddr, BYTE byData);
void Map82_Init(void);
void Map82_Sram(WORD wAddr, BYTE byData);
void Map83_Apu(WORD wAddr, BYTE byData);
void Map83_HSync(void);
void Map83_Init(void);
void Map83_Write(WORD wAddr, BYTE byData);
void Map86_Init(void);
void Map86_Sram(WORD wAddr, BYTE byData);
void Map87_Init(void);
void Map87_Sram(WORD wAddr, BYTE byData);
void Map88_Init(void);
void Map88_Write(WORD wAddr, BYTE byData);
void Map89_Init(void);
void Map89_Write(WORD wAddr, BYTE byData);
void Map8_Init(void);
void Map8_Write(WORD wAddr, BYTE byData);
void Map90_Apu(WORD wAddr, BYTE byData);
void Map90_HSync(void);
void Map90_Init(void);
void Map90_Sync_Chr_Banks(void);
void Map90_Sync_Mirror(void);
void Map90_Sync_Prg_Banks(void);
void Map90_Write(WORD wAddr, BYTE byData);
void Map91_Init(void);
void Map91_Sram(WORD wAddr, BYTE byData);
void Map92_Init(void);
void Map92_Write(WORD wAddr, BYTE byData);
void Map93_Init(void);
void Map93_Sram(WORD wAddr, BYTE byData);
void Map94_Init(void);
void Map94_Write(WORD wAddr, BYTE byData);
void Map95_Init(void);
void Map95_Set_CPU_Banks(void);
void Map95_Set_PPU_Banks(void);
void Map95_Write(WORD wAddr, BYTE byData);
void Map96_Init(void);
void Map96_PPU(WORD wAddr);
void Map96_Set_Banks(void);
void Map96_Write(WORD wAddr, BYTE byData);
void Map97_Init(void);
void Map97_Write(WORD wAddr, BYTE byData);
void Map9_Init(void);
void Map9_PPU(WORD wAddr);
void Map9_Write(WORD wAddr, BYTE byData);
