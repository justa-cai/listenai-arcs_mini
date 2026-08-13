#ifndef FTSDC021_MMC_H
#define FTSDC021_MMC_H

typedef struct
{
    u32 CSD_STRUCTURE: 2;
    u32 SPEC_VERS: 4;
    u32 Reserved1: 2;
    u8 TAAC;
    u8 NSAC;
    u8 TRAN_SPEED;
    u32 CCC: 12;
    u32 READ_BL_LEN: 4;
    u32 READ_BL_PARTIAL: 1;
    u32 WRITE_BLK_MISALIGN: 1;
    u32 READ_BLK_MISALIGN: 1;
    u32 DSR_IMP: 1;
    u32 Reserved2: 2;
    u32 C_SIZE: 12;
    u32 VDD_R_CURR_MIN: 3;
    u32 VDD_R_CURR_MAX: 3;
    u32 VDD_W_CURR_MIN: 3;
    u32 VDD_W_CURR_MAX: 3;
    u32 C_SIZE_MULT: 3;
    u32 ERASE_GRP_SIZE: 5;
    u32 ERASE_GRP_MULT: 5;
    u32 WP_GRP_SIZE: 5;
    u32 WP_GRP_ENABLE: 1;
    u32 DEFAULT_ECC: 2;
    u32 R2W_FACTOR: 3;
    u32 WRITE_BL_LEN: 4;
    u32 WRITE_BL_PARTIAL: 1;
    u32 Reserved3: 4;
    u32 CONTENT_PROT_APP: 1;
    u32 FILE_FORMAT_GRP: 1;
    u32 COPY: 1;
    u32 PERM_WRITE_PROTECT: 1;
    u32 TMP_WRITE_PROTECT: 1;
    u32 FILE_FORMAT: 2;
    u32 ECC: 2;
} MMC_CSD;

#pragma pack(1)
typedef struct
{
    /* Modes Segment */
    u8 Reserved27[134];
    u8 SEC_BAD_BLK_MGMNT;
    u8 Reserved26;
    u8 ENH_SATRT_ADDR[4];
    u8 ENH_SIZE_MULT[3];
    u8 GP_SIZE_MULT[12];
    u8 PARTITION_SETTING_COMPLETED;
    u8 PARTITIONING_ATTRIBUTE;
    u8 MAX_ENH_SIZE_MULT[3];
    u8 PARTITIONING_SUPPORT;
    u8 Reserved25;
    u8 RST_n_FUNCTION;
    u8 Reserved24[5];
    u8 RPMB_SIZE_MULT;
    u8 FW_CONFIG;
    u8 Reserved23;
    u8 USER_WP;
    u8 Reserved22;
    u8 BOOT_WP;     /* [173] R/W & R/W/C_P */
    u8 Reserved21;
    u8 ERASE_GROUP_DEF;
    u8 Reserved20;
    u8 BOOT_BUS_WIDTH;  /* [177] R/W/E */
    u8 BOOT_CONFIG_PROT;    /* [178] R/W & R/W/C_P */
    u8 PARTITION_CONF;  /* [179] */
    u8 Reserved19;
    u8 ERASED_MEM_CONT;
    u8 Reserved18;
    u8 BUS_WIDTH;   /* [183] W/E_P */
    u8 Reserved17;
    u8 HS_TIMING;   /* [185] R/W/E_P */
    u8 Reserved16;
    u8 POWER_CLASS;
    u8 Reserved15;
    u8 CMD_SET_REV;
    u8 Reserved14;
    u8 CMD_SET;
    /* Properties Segment */
    u8 EXT_CSD_REV;
    u8 Reserved13;
    u8 CSD_STRUCTURE;
    u8 Reserved12;
    u8 CARDTYPE;
    u8 Reserved11[3];
    u8 PWR_CL_52_195;
    u8 PWR_CL_26_195;
    u8 PWR_CL_52_360;
    u8 PWR_CL_26_360;
    u8 Reserved10;
    u8 MIN_PERF_R_4_26;
    u8 MIN_PERF_W_4_26;
    u8 MIN_PERF_R_8_26_4_52;
    u8 MIN_PERF_W_8_26_4_52;
    u8 MIN_PERF_R_8_52;
    u8 MIN_PERF_W_8_52;
    u8 Reserved9;
    u32 SEC_COUNT;  /* [215:212] R */
    u8 Reserved8;
    u8 S_A_TIMEOUT;
    u8 Reserved7;
    u8 S_C_VCCQ;
    u8 S_C_VCC;
    u8 HC_WP_GRP_SIZE;
    u8 REL_WR_SEC_C;
    u8 ERASE_TIMEOUT_MULT;
    u8 HC_ERASE_GRP_SIZE;
    u8 ACC_SIZE;
    u8 BOOT_SIZE_MULTI; /* [226] R */
    u8 Reserved6;   /* [227] , (embedded mmc )is 2 bytes width. */
    u8 BOOT_INFO;   /* [228] R */
    u8 SEC_TRIM_MULT;
    u8 SEC_ERASE_MULT;
    u8 SEC_FEATURE_SUPPORT;
    u8 TRIM_MULT;
    u8 Reserved5;
    u8 MIN_PERF_DDR_R_8_52;
    u8 MIN_PERF_DDR_W_8_52;
    u8 Reserved4[2];
    u8 PWR_CL_DDR_52_195;   /* [238] */
    u8 PWR_CL_DDR_52_360;   /* [239] */
    u8 Reserved3;
    u8 INI_TIMEOUT_AP;  /* [241] */
    u8 Reserved2[262];
    u8 S_CMD_SET;   /* [504] */
    u8 Reserved1[7];    /* [511:505] */
} MMC_EXT_CSD;
#pragma pack()

/* CMD INDEX */
#define SDHCI_MMC_SWITCH        6
#define SDHCI_MMC_VENDOR_CMD    62

/* Cmd Set [2:0] of argument SWITCH command*/
#define EXT_CSD_CMD_SET_NORMAL          (1<<0)
#define EXT_CSD_CMD_SET_SECURE          (1<<1)
#define EXT_CSD_CMD_SET_CPSECURE        (1<<2)

/* Offset at EXT CSD to access */
#define EXT_CSD_PARTITION_SETTING_COMPLETED 156
#define EXT_CSD_PARTITION_CONF          179
#define EXT_CSD_BUS_WIDTH       183 /* R/W */
#define EXT_CSD_HS_TIMING       185 /* R/W */
#define EXT_CSD_CARD_TYPE       196 /* RO */
#define EXT_CSD_SEC_CNT         212 /* RO, 4 bytes */
#define EXT_CSD_BOOT_SIZE_MULT  226


#define EXT_CSD_Command_set     0x0
#define EXT_CSD_Set_bit         0x1
#define EXT_CSD_Clear_byte      0x2
#define EXT_CSD_Write_byte      0x3

#define EXT_CSD_Bus8bit         0x2
#define EXT_CSD_Bus4bit         0x1
#define EXT_CSD_Bus1bit         0x0

#define MMC_CMD6_Access_mode(x) (u32)( x << 24)
#define MMC_CMD6_Index(x)       (u32)( x << 16)
#define MMC_CMD6_Value(x)       (u32)( x << 8)
#define MMC_CMD6_Cmd_set(x)     (u32)( x )

#define MMC_CARD_BUSY   0x80000000UL  /* Card Power up status bit */

#endif
