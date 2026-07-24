/**
 * @file sd_music_fatfs_config.h
 * @brief FatFs config override for SD music Chinese file names.
 *
 * This file is included by FatFs ffconf.h when
 * CONFIG_FATFS_FFCONF_OVERRIDE is enabled. The settings are global to FatFs,
 * but they are required by sd_music to scan and play MP3 files with Chinese
 * names on the SD card.
 */

/* Use UTF-8 for FatFs API strings so file paths stay normal char strings. */
#undef FF_LFN_UNICODE
#define FF_LFN_UNICODE    2

/* Simplified Chinese OEM code page for short names and GBK ID3 fallback. */
#undef FF_CODE_PAGE
#define FF_CODE_PAGE      936

/* String I/O functions use UTF-8 when enabled. */
#undef FF_STRF_ENCODE
#define FF_STRF_ENCODE    3
