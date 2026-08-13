/*
 * InfoNES mapper compatibility adapter.
 * Mapper bodies live under src/nes_mapper/infones and share this runtime
 * context to preserve the original InfoNES callback model.
 */

#include "infones/nes_mapper_infones.h"

nes_t *infones_current_nes;
struct NesHeader_tag NesHeader;
BYTE MapperNo;
BYTE ROM_Mirroring;
BYTE ROM_SRAM;
BYTE ROM_Trainer;
BYTE ROM_FourScr;
static BYTE *infones_fallback_sram;
BYTE *SRAM;
BYTE *SRAMBANK;
BYTE *DRAM;
BYTE byVramWriteEnable;
BYTE ChrBufUpdate;
BYTE FrameIRQ_Enable;
WORD FrameStep;
BYTE IRQ_State = 1;
BYTE IRQ_Wiring = 1;
BYTE NMI_State = 1;
BYTE APU_Reg[0x20];
BYTE ApuFdsEnable;
BYTE ApuMmc5Enable;
BYTE ApuSunsoft5BEnable;
BYTE ApuVrc6Enable;
BYTE ApuMmc5P1Atl;
BYTE ApuMmc5P2Atl;
BYTE ApuMmc5PcmValue;

void (*MapperInit)(void);
void (*MapperWrite)(WORD wAddr, BYTE byData);
void (*MapperSram)(WORD wAddr, BYTE byData);
void (*MapperApu)(WORD wAddr, BYTE byData);
BYTE (*MapperReadApu)(WORD wAddr);
void (*MapperVSync)(void);
void (*MapperHSync)(void);
void (*MapperPPU)(WORD wAddr);
void (*MapperRenderScreen)(BYTE byMode);
int (*MapperBlobSize)(void);
void (*MapperSaveBlob)(BYTE *pBuf);
void (*MapperLoadBlob)(BYTE *pBuf);

static void (*infones_selected_init)(void);

extern void nes_cpu_irq(nes_t *nes);

static int infones_ensure_fallback_sram(void)
{
    if (SRAM) {
        if (!SRAMBANK) {
            SRAMBANK = SRAM;
        }
        return 1;
    }

    if (!infones_fallback_sram) {
        infones_fallback_sram = (BYTE *)nes_malloc(SRAM_SIZE);
        if (!infones_fallback_sram) {
            NES_LOG_WARN("NES: InfoNES fallback SRAM alloc failed\n");
            return 0;
        }
        nes_memset(infones_fallback_sram, 0xff, SRAM_SIZE);
        NES_LOG_INFO("NES: InfoNES fallback SRAM allocated, bytes=%u\n", (unsigned int)SRAM_SIZE);
    }

    SRAM = infones_fallback_sram;
    SRAMBANK = SRAM;
    return 1;
}

static int infones_mapper_allows_fallback_sram(const nes_t *nes)
{
    if (!nes) {
        return 0;
    }
    if (nes->nes_rom.save_ram) {
        return 1;
    }

    switch (nes->nes_rom.mapper_number) {
    case 23:
        /* Some VRC2/VRC4 hacks omit the SRAM header bit but still use
         * $6000-$7FFF as work RAM during reset. */
        return 1;
    default:
        return 0;
    }
}

static void infones_free_runtime_buffers(void)
{
#if INFONES_ENABLE_MAPPER_019
    if (Map19_Chr_Ram) {
        nes_free(Map19_Chr_Ram);
        Map19_Chr_Ram = NULL;
    }
#endif
#if INFONES_ENABLE_MAPPER_024
    if (vrc6_wave_buffers) {
        nes_free(vrc6_wave_buffers);
        vrc6_wave_buffers = NULL;
    }
#endif
#if INFONES_ENABLE_MAPPER_069
    if (s5b_wave_buffers) {
        nes_free(s5b_wave_buffers);
        s5b_wave_buffers = NULL;
    }
#endif
#if INFONES_ENABLE_MAPPER_185
    if (Map185_Dummy_Chr_Rom) {
        nes_free(Map185_Dummy_Chr_Rom);
        Map185_Dummy_Chr_Rom = NULL;
    }
#endif
    if (DRAM) {
        nes_free(DRAM);
        DRAM = NULL;
    }
    if (infones_fallback_sram) {
        nes_free(infones_fallback_sram);
        infones_fallback_sram = NULL;
    }
    SRAM = NULL;
    SRAMBANK = NULL;
}

static void infones_sync_name_table_mirrors(nes_t *nes)
{
    for (uint8_t i = 0; i < 4; i++) {
        nes->nes_ppu.name_table_mirrors[i] = nes->nes_ppu.name_table[i];
    }
}

void InfoNES_Mirroring(int nType)
{
    static const uint8_t mirror_table[6][4] = {
        {0, 0, 1, 1},
        {0, 1, 0, 1},
        {1, 1, 1, 1},
        {0, 0, 0, 0},
        {0, 1, 2, 3},
        {0, 0, 0, 1},
    };
    const uint8_t type = (nType >= 0 && nType < 6) ? (uint8_t)nType : 0;
    for (uint8_t i = 0; i < 4; i++) {
        infones_current_nes->nes_ppu.name_table[i] =
            infones_current_nes->nes_ppu.ppu_vram[mirror_table[type][i]];
    }
    infones_sync_name_table_mirrors(infones_current_nes);
}

void InfoNES_SetupChr(void)
{
}

void K6502_Set_Int_Wiring(BYTE byNMI_Wiring, BYTE byIRQ_Wiring)
{
    NMI_State = byNMI_Wiring;
    IRQ_Wiring = byIRQ_Wiring;
    IRQ_State = byIRQ_Wiring;
}

void infones_irq_request(void)
{
    IRQ_State = 0;
    if (infones_current_nes) {
        nes_cpu_irq(infones_current_nes);
    }
}

static int infones_sram_bank_writable(void)
{
    if (!SRAM || !SRAMBANK) {
        return 0;
    }
    const uintptr_t bank = (uintptr_t)SRAMBANK;
    const uintptr_t start = (uintptr_t)SRAM;
    return bank >= start && bank < start + SRAM_SIZE;
}

static void infones_sync_fast_sram_bank(nes_t *nes)
{
    nes->nes_mapper.mapper_sram_direct_bank = NULL;
    if (nes->nes_rom.mapper_number == 23 && infones_sram_bank_writable()) {
        nes->nes_mapper.mapper_sram_direct_bank = SRAMBANK;
    }
}

static void infones_prepare_context(nes_t *nes)
{
    infones_current_nes = nes;
    MapperNo = (BYTE)nes->nes_rom.mapper_number;
    NesHeader.byRomSize = (BYTE)nes->nes_rom.prg_rom_size;
    NesHeader.byVRomSize = (BYTE)nes->nes_rom.chr_rom_size;
    NesHeader.byInfo1 = (BYTE)(((nes->nes_rom.mapper_number & 0x0f) << 4) |
                               (nes->nes_rom.four_screen ? 0x08 : 0x00) |
                               (nes->nes_rom.save_ram ? 0x02 : 0x00) |
                               (nes->nes_rom.mirroring_type ? 0x01 : 0x00));
    NesHeader.byInfo2 = (BYTE)(nes->nes_rom.mapper_number & 0xf0);
    ROM_Mirroring = nes->nes_rom.mirroring_type;
    ROM_SRAM = nes->nes_rom.save_ram ? 0x02 : 0x00;
    ROM_Trainer = 0;
    ROM_FourScr = nes->nes_rom.four_screen ? 0x08 : 0x00;
    SRAM = nes->nes_rom.sram;
    SRAMBANK = SRAM;
    byVramWriteEnable = nes->nes_rom.chr_rom_size == 0;
    for (uint8_t i = 0; i < 8; i++) {
        nes->nes_ppu.pattern_table[i] = infones_chr_page(i);
    }
    InfoNES_Mirroring(ROM_FourScr ? 4 : ROM_Mirroring);
}

static void infones_mapper_init(nes_t *nes)
{
    infones_prepare_context(nes);
    MapperInit = infones_selected_init;
    MapperWrite = Map0_Write;
    MapperSram = Map0_Sram;
    MapperApu = Map0_Apu;
    MapperReadApu = Map0_ReadApu;
    MapperVSync = Map0_VSync;
    MapperHSync = Map0_HSync;
    MapperPPU = Map0_PPU;
    MapperRenderScreen = Map0_RenderScreen;
    MapperBlobSize = NULL;
    MapperSaveBlob = NULL;
    MapperLoadBlob = NULL;
    if (infones_selected_init) {
        infones_selected_init();
    }
    infones_sync_name_table_mirrors(nes);
    if (MapperWrite == Map0_Write) {
        nes->nes_mapper.mapper_write = NULL;
    }
    if (MapperSram == Map0_Sram) {
        MapperSram = NULL;
    }
    if (MapperApu == Map0_Apu) {
        nes->nes_mapper.mapper_apu = NULL;
    }
    if (MapperReadApu == Map0_ReadApu) {
        nes->nes_mapper.mapper_read_apu = NULL;
    }
    if (MapperVSync == Map0_VSync) {
        nes->nes_mapper.mapper_vsync = NULL;
    }
    if (MapperHSync == Map0_HSync) {
        nes->nes_mapper.mapper_hsync = NULL;
    }
    if (MapperPPU == Map0_PPU) {
        nes->nes_mapper.mapper_ppu = NULL;
    }
    if (MapperRenderScreen == Map0_RenderScreen) {
        nes->nes_mapper.mapper_render_screen = NULL;
    }
    if (!MapperSram && !SRAMBANK && !infones_mapper_allows_fallback_sram(nes)) {
        nes->nes_mapper.mapper_sram = NULL;
        nes->nes_mapper.mapper_read_sram = NULL;
    }
    infones_sync_fast_sram_bank(nes);
    const char *sram_desc = (SRAM && SRAM == infones_fallback_sram) ? "fallback" :
                            (SRAM ? "rom" : (infones_mapper_allows_fallback_sram(nes) ? "lazy" : "none"));
    NES_LOG_INFO("NES: mapper %u InfoNES ready, prg=%u*16KB chr=%u*8KB mir=%u sram=%s\n",
                 (unsigned int)nes->nes_rom.mapper_number,
                 (unsigned int)nes->nes_rom.prg_rom_size,
                 (unsigned int)nes->nes_rom.chr_rom_size,
                 (unsigned int)nes->nes_rom.mirroring_type,
                 sram_desc);
}

static void infones_mapper_deinit(nes_t *nes)
{
    nes->nes_mapper.mapper_sram_direct_bank = NULL;
    infones_free_runtime_buffers();
    infones_current_nes = NULL;
    infones_selected_init = NULL;
}

static void infones_mapper_write(nes_t *nes, uint16_t write_addr, uint8_t data)
{
    infones_current_nes = nes;
    if (MapperWrite) {
        MapperWrite(write_addr, data);
    }
}

static void infones_mapper_sram(nes_t *nes, uint16_t write_addr, uint8_t data)
{
    infones_current_nes = nes;
    if (!SRAM && infones_mapper_allows_fallback_sram(nes) && !infones_ensure_fallback_sram()) {
        return;
    }
    if (MapperSram) {
        MapperSram(write_addr, data);
    }
    infones_sync_fast_sram_bank(nes);
    if (SRAMBANK && infones_sram_bank_writable()) {
        SRAMBANK[write_addr & 0x1fff] = data;
    }
}

static uint8_t infones_mapper_read_sram(nes_t *nes, uint16_t read_addr)
{
    infones_current_nes = nes;
    if (!SRAM && infones_mapper_allows_fallback_sram(nes) && !infones_ensure_fallback_sram()) {
        return 0;
    }
    infones_sync_fast_sram_bank(nes);
    if (SRAMBANK) {
        return SRAMBANK[read_addr & 0x1fff];
    }
    return 0;
}

static void infones_mapper_apu(nes_t *nes, uint16_t write_addr, uint8_t data)
{
    infones_current_nes = nes;
    if (MapperApu) {
        MapperApu(write_addr, data);
    }
}

static uint8_t infones_mapper_read_apu(nes_t *nes, uint16_t read_addr)
{
    infones_current_nes = nes;
    if (MapperReadApu) {
        return MapperReadApu(read_addr);
    }
    return (uint8_t)(read_addr >> 8);
}

static void infones_mapper_vsync(nes_t *nes)
{
    infones_current_nes = nes;
    if (MapperVSync) {
        MapperVSync();
    }
}

static void infones_mapper_hsync(nes_t *nes)
{
    infones_current_nes = nes;
    if (MapperHSync) {
        MapperHSync();
    }
}

static void infones_mapper_ppu(nes_t *nes, uint16_t ppu_addr)
{
    infones_current_nes = nes;
    if (MapperPPU) {
        MapperPPU(ppu_addr);
    }
}

static void infones_mapper_render_screen(nes_t *nes, uint8_t mode)
{
    infones_current_nes = nes;
    if (MapperRenderScreen) {
        MapperRenderScreen(mode);
    }
}

int nes_mapper_infones_init(nes_t *nes)
{
    for (size_t i = 0; i < infones_mapper_count; i++) {
        if (infones_mapper_table[i].mapper_id == nes->nes_rom.mapper_number) {
            infones_selected_init = infones_mapper_table[i].init;
            nes->nes_mapper.mapper_init = infones_mapper_init;
            nes->nes_mapper.mapper_deinit = infones_mapper_deinit;
            nes->nes_mapper.mapper_write = infones_mapper_write;
            nes->nes_mapper.mapper_sram = infones_mapper_sram;
            nes->nes_mapper.mapper_read_sram = infones_mapper_read_sram;
            nes->nes_mapper.mapper_apu = infones_mapper_apu;
            nes->nes_mapper.mapper_read_apu = infones_mapper_read_apu;
            nes->nes_mapper.mapper_vsync = infones_mapper_vsync;
            nes->nes_mapper.mapper_hsync = infones_mapper_hsync;
            nes->nes_mapper.mapper_ppu = infones_mapper_ppu;
            nes->nes_mapper.mapper_render_screen = infones_mapper_render_screen;
            nes->nes_mapper.mapper_banked_chr = nes->nes_rom.mapper_number != 0;
            return NES_OK;
        }
    }
    return NES_ERROR;
}
