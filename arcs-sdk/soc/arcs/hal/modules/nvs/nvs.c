

/*  NVS: non volatile storage in flash
 *
 * Copyright (c) 2018 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "nvs.h"
#include <sys/param.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include "nvs_priv.h"
#ifdef CFG_FLASH_IF
#include "flash_if.h"
#else
#include "spiflash.h"
#endif
#include "log_print.h"
#include "chip.h"
#include "types.h"


#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include "nvs_priv.h"

#include "nvs_port.h"

#ifdef CONFIG_NVS_FLASH_XIP
#include "cache.h"
#ifndef NVS_FLASH_XIP_BASE
#define NVS_FLASH_XIP_BASE 0x30000000
#endif
#endif /* CONFIG_NVS_FLASH_XIP */

#define NVS_SECTOR_ERASE_SIZE        4096
#define NVS_SECTOR_PAGE_SIZE         256

static int nvs_prev_ate(struct nvs_fs *fs, uint32_t *addr, struct nvs_ate *ate);
static int nvs_ate_valid(struct nvs_fs *fs, const struct nvs_ate *entry);


#ifdef CFG_FLASH_IF
#define flash_write_protection_set(dev, enable)            flash_if_write_protection_set(enable)
#define flash_write(dev, offset, data, len)                flash_if_write(offset, data, len)
#define flash_read(dev, offset, data, len)                 flash_if_read(offset, data, len)
#define flash_erase(dev, offset, sector_size)              flash_if_erase(offset, sector_size)
#endif

volatile uint32_t __sect_mask = ADDR_SECT_MASK;
volatile uint32_t __sect_shift = ADDR_SECT_SHIFT;
volatile uint32_t __offs_mask = ADDR_OFFS_MASK;

struct flash_parameters flash_para = {
	.erase_value = 0xFF,
	.write_block_size = 32,
};

#ifdef CONFIG_NVS_LOOKUP_CACHE

static inline size_t nvs_lookup_cache_pos(uint16_t id)
{
	uint16_t hash;

	/* 16-bit integer hash function found by https://github.com/skeeto/hash-prospector. */
	hash = id;
	hash ^= hash >> 8;
	hash *= 0x88b5U;
	hash ^= hash >> 7;
	hash *= 0xdb2dU;
	hash ^= hash >> 9;

	return hash % CONFIG_NVS_LOOKUP_CACHE_SIZE;
}

static int nvs_lookup_cache_rebuild(struct nvs_fs *fs)
{
	int rc;
	uint32_t addr, ate_addr;
	uint32_t *cache_entry;
	struct nvs_ate ate;

	memset(fs->lookup_cache, 0xff, sizeof(fs->lookup_cache));
	addr = fs->ate_wra;

	while (true) {
		/* Make a copy of 'addr' as it will be advanced by nvs_pref_ate() */
		ate_addr = addr;
		rc = nvs_prev_ate(fs, &addr, &ate);

		if (rc) {
			return rc;
		}

		cache_entry = &fs->lookup_cache[nvs_lookup_cache_pos(ate.id)];

		if (ate.id != 0xFFFF && *cache_entry == NVS_LOOKUP_CACHE_NO_ADDR &&
		    nvs_ate_valid(fs, &ate)) {
			*cache_entry = ate_addr;
		}

		if (addr == fs->ate_wra) {
			break;
		}
	}

	return 0;
}

static void nvs_lookup_cache_invalidate(struct nvs_fs *fs, uint32_t sector)
{
	uint32_t *cache_entry = fs->lookup_cache;
	uint32_t *const cache_end = &fs->lookup_cache[CONFIG_NVS_LOOKUP_CACHE_SIZE];

	for (; cache_entry < cache_end; ++cache_entry) {
		if ((*cache_entry >> __sect_shift) == sector) {
			*cache_entry = NVS_LOOKUP_CACHE_NO_ADDR;
		}
	}
}

#endif /* CONFIG_NVS_LOOKUP_CACHE */

/* basic routines */
/* nvs_al_size returns size aligned to fs->write_block_size */
static inline size_t nvs_al_size(struct nvs_fs *fs, size_t len)
{
	uint8_t write_block_size = fs->flash_parameters->write_block_size;

	if (write_block_size <= 1U) {
		return len;
	}
	return (len + (write_block_size - 1U)) & ~(write_block_size - 1U);
}
/* end basic routines */

/* flash routines */
/* basic aligned flash write to nvs address */
static int nvs_flash_al_wrt(struct nvs_fs *fs, uint32_t addr, const void *data,
			     size_t len)
{
	const uint8_t *data8 = (const uint8_t *)data;
	int rc = 0;
	off_t offset;
	size_t blen;
	uint8_t buf[NVS_BLOCK_SIZE];

	if (!len) {
		/* Nothing to write, avoid changing the flash protection */
		return 0;
	}

	offset = fs->offset;
	offset += fs->sector_size * (addr >> __sect_shift);
	offset += addr & __offs_mask;


	flash_write_protection_set(fs->flash_device, false);

	blen = len & ~(fs->flash_parameters->write_block_size - 1U);
	if (blen > 0) {
		//CLOGI("before flash write, offset: 0x%X, data8: %p, len: %d", offset, data8, blen);
		rc = flash_write(fs->flash_device, offset, data8, blen);
		//CLOGI("after flash write, len: %d", blen);
		if (rc) {
			/* flash write error */
			goto end;
		}
		len -= blen;
		offset += blen;
		data8 += blen;
	}
	if (len) {
		memcpy(buf, data8, len);
		(void)memset(buf + len, fs->flash_parameters->erase_value,
			fs->flash_parameters->write_block_size - len);

		rc = flash_write(fs->flash_device, offset, buf,
				 fs->flash_parameters->write_block_size);
	}

end:
	flash_write_protection_set(fs->flash_device, true);
	return rc;
}

/* basic flash read from nvs address */
static int nvs_flash_rd(struct nvs_fs *fs, uint32_t addr, void *data,
			 size_t len)
{
	int rc;
	off_t offset;

	offset = fs->offset;
	offset += fs->sector_size * (addr >> __sect_shift);
	offset += addr & __offs_mask;

#ifdef CONFIG_NVS_FLASH_XIP
    /* Read from Flash with XIP support */
    HAL_InvalidateDCache_by_Addr((uint32_t *)(NVS_FLASH_XIP_BASE + offset), len);
    memcpy(data, (const void *)(NVS_FLASH_XIP_BASE + offset), len);
	rc = 0;
#else
    /* Read from Flash using API */
    rc = flash_read(fs->flash_device, offset, data, len);
#endif
	return rc;
}

/* allocation entry write */
static int nvs_flash_ate_wrt(struct nvs_fs *fs, const struct nvs_ate *entry)
{
	int rc;

	rc = nvs_flash_al_wrt(fs, fs->ate_wra, entry,
			       sizeof(struct nvs_ate));
#ifdef CONFIG_NVS_LOOKUP_CACHE
	/* 0xFFFF is a special-purpose identifier. Exclude it from the cache */
	if (entry->id != 0xFFFF) {
		fs->lookup_cache[nvs_lookup_cache_pos(entry->id)] = fs->ate_wra;
	}
#endif
	fs->ate_wra -= nvs_al_size(fs, sizeof(struct nvs_ate));

	return rc;
}

/* data write */
static int nvs_flash_data_wrt(struct nvs_fs *fs, const void *data, size_t len)
{
	int rc;

	rc = nvs_flash_al_wrt(fs, fs->data_wra, data, len);
	fs->data_wra += nvs_al_size(fs, len);

	return rc;
}

/* flash ate read */
static int nvs_flash_ate_rd(struct nvs_fs *fs, uint32_t addr,
			     struct nvs_ate *entry)
{
	return nvs_flash_rd(fs, addr, entry, sizeof(struct nvs_ate));
}

/* end of basic flash routines */

/* advanced flash routines */

/* nvs_flash_block_cmp compares the data in flash at addr to data
 * in blocks of size NVS_BLOCK_SIZE aligned to fs->write_block_size
 * returns 0 if equal, 1 if not equal, errcode if error
 */
static int nvs_flash_block_cmp(struct nvs_fs *fs, uint32_t addr, const void *data,
				size_t len)
{
	const uint8_t *data8 = (const uint8_t *)data;
	int rc;
	size_t bytes_to_cmp, block_size;
	uint8_t buf[NVS_BLOCK_SIZE];

	block_size =
		NVS_BLOCK_SIZE & ~(fs->flash_parameters->write_block_size - 1U);

	while (len) {
		bytes_to_cmp = MIN(block_size, len);
		rc = nvs_flash_rd(fs, addr, buf, bytes_to_cmp);
		if (rc) {
			return rc;
		}
		rc = memcmp(data8, buf, bytes_to_cmp);
		if (rc) {
			return 1;
		}
		len -= bytes_to_cmp;
		addr += bytes_to_cmp;
		data8 += bytes_to_cmp;
	}
	return 0;
}

/* nvs_flash_cmp_const compares the data in flash at addr to a constant
 * value. returns 0 if all data in flash is equal to value, 1 if not equal,
 * errcode if error
 */
static int nvs_flash_cmp_const(struct nvs_fs *fs, uint32_t addr, uint8_t value,
				size_t len)
{
	int rc;
	size_t bytes_to_cmp, block_size;
	uint8_t buf[NVS_BLOCK_SIZE];

	block_size =
		NVS_BLOCK_SIZE & ~(fs->flash_parameters->write_block_size - 1U);

	while (len) {
		bytes_to_cmp = MIN(block_size, len);
		rc = nvs_flash_rd(fs, addr, buf, bytes_to_cmp);
		if (rc) {
			return rc;
		}

		for (size_t i = 0; i < bytes_to_cmp; i++) {
			if (buf[i] != value) {
				return 1;
			}
		}

		len -= bytes_to_cmp;
		addr += bytes_to_cmp;
	}
	return 0;
}

/* flash block move: move a block at addr to the current data write location
 * and updates the data write location.
 */
static int nvs_flash_block_move(struct nvs_fs *fs, uint32_t addr, size_t len)
{
	int rc;
	size_t bytes_to_copy, block_size;
	uint8_t buf[NVS_BLOCK_SIZE];

	block_size =
		NVS_BLOCK_SIZE & ~(fs->flash_parameters->write_block_size - 1U);

	while (len) {
		bytes_to_copy = MIN(block_size, len);
		rc = nvs_flash_rd(fs, addr, buf, bytes_to_copy);
		if (rc) {
			return rc;
		}
		rc = nvs_flash_data_wrt(fs, buf, bytes_to_copy);
		if (rc) {
			return rc;
		}
		len -= bytes_to_copy;
		addr += bytes_to_copy;
	}
	return 0;
}

/* erase a sector and verify erase was OK.
 * return 0 if OK, errorcode on error.
 */
static int nvs_flash_erase_sector(struct nvs_fs *fs, uint32_t addr)
{
	int rc;
	off_t offset;

	addr &= __sect_mask;

	offset = fs->offset;
	offset += fs->sector_size * (addr >> __sect_shift);

#ifdef CONFIG_NVS_LOOKUP_CACHE
	nvs_lookup_cache_invalidate(fs, addr >> __sect_shift);
#endif
	LOG_INF("fs->flash_device: %p, offset: 0x%X, fs->sector_size: %d", fs->flash_device, offset, fs->sector_size);

	flash_write_protection_set(fs->flash_device, false);
	rc = flash_erase(fs->flash_device, offset, fs->sector_size);
	flash_write_protection_set(fs->flash_device, true);

	if (rc) {
		//LOG_DBG("flash erase rc: %d", rc);
		return rc;
	}

	if (nvs_flash_cmp_const(fs, addr, fs->flash_parameters->erase_value,
			fs->sector_size)) {
		rc = -ENXIO;
	}

	return rc;
}

/* crc update on allocation entry */
static void nvs_ate_crc8_update(struct nvs_ate *entry)
{
	uint8_t crc8;

	crc8 = crc8_ccitt(0xff, entry, offsetof(struct nvs_ate, crc8));
	entry->crc8 = crc8;
}

/* crc check on allocation entry
 * returns 0 if OK, 1 on crc fail
 */
static int nvs_ate_crc8_check(const struct nvs_ate *entry)
{
	uint8_t crc8;

	crc8 = crc8_ccitt(0xff, entry, offsetof(struct nvs_ate, crc8));
	if (crc8 == entry->crc8) {
		return 0;
	}
	return 1;
}

/* nvs_ate_cmp_const compares an ATE to a constant value. returns 0 if
 * the whole ATE is equal to value, 1 if not equal.
 */

static int nvs_ate_cmp_const(const struct nvs_ate *entry, uint8_t value)
{
	const uint8_t *data8 = (const uint8_t *)entry;
	int i;

	for (i = 0; i < sizeof(struct nvs_ate); i++) {
		if (data8[i] != value) {
			return 1;
		}
	}

	return 0;
}

/* nvs_ate_valid validates an ate:
 *     return 1 if crc8 and offset valid,
 *            0 otherwise
 */
static int nvs_ate_valid(struct nvs_fs *fs, const struct nvs_ate *entry)
{
	size_t ate_size;
	uint32_t position;

	ate_size = nvs_al_size(fs, sizeof(struct nvs_ate));
	position = entry->offset + entry->len;

	if ((nvs_ate_crc8_check(entry)) ||
		(position >= (fs->sector_size - ate_size))) {
			return 0;
	}

	return 1;
}

/* nvs_close_ate_valid validates an sector close ate: a valid sector close ate:
 * - valid ate
 * - len = 0 and id = 0xFFFF
 * - offset points to location at ate multiple from sector size
 * return 1 if valid, 0 otherwise
 */
static int nvs_close_ate_valid(struct nvs_fs *fs, const struct nvs_ate *entry)
{
	size_t ate_size;

	if ((!nvs_ate_valid(fs, entry)) || (entry->len != 0U) ||
	    (entry->id != 0xFFFF)) {
		return 0;
	}

	ate_size = nvs_al_size(fs, sizeof(struct nvs_ate));
	if ((fs->sector_size - entry->offset) % ate_size) {
		return 0;
	}

	return 1;
}

/* store an entry in flash */
static int nvs_flash_wrt_entry(struct nvs_fs *fs, uint16_t id, const void *data,
				size_t len)
{
	int rc;
	struct nvs_ate entry;

	entry.id = id;
	entry.offset = (uint16_t)(fs->data_wra & __offs_mask);
	entry.len = (uint16_t)len;
	entry.part = 0xff;

	nvs_ate_crc8_update(&entry);

	rc = nvs_flash_data_wrt(fs, data, len);
	if (rc) {
		return rc;
	}
	rc = nvs_flash_ate_wrt(fs, &entry);
	if (rc) {
		return rc;
	}

	return 0;
}
/* end of flash routines */

/* If the closing ate is invalid, its offset cannot be trusted and
 * the last valid ate of the sector should instead try to be recovered by going
 * through all ate's.
 *
 * addr should point to the faulty closing ate and will be updated to the last
 * valid ate. If no valid ate is found it will be left untouched.
 */
static int nvs_recover_last_ate(struct nvs_fs *fs, uint32_t *addr)
{
	uint32_t data_end_addr, ate_end_addr;
	struct nvs_ate end_ate;
	size_t ate_size;
	int rc;

	//LOG_DBG("Recovering last ate from sector %d",
	//	(*addr >> __sect_shift));

	ate_size = nvs_al_size(fs, sizeof(struct nvs_ate));

	*addr -= ate_size;
	ate_end_addr = *addr;
	data_end_addr = *addr & __sect_mask;
	while (ate_end_addr > data_end_addr) {
		rc = nvs_flash_ate_rd(fs, ate_end_addr, &end_ate);
		if (rc) {
			return rc;
		}
		if (nvs_ate_valid(fs, &end_ate)) {
			/* found a valid ate, update data_end_addr and *addr */
			data_end_addr &= __sect_mask;
			data_end_addr += end_ate.offset + end_ate.len;
			*addr = ate_end_addr;
		}
		ate_end_addr -= ate_size;
	}

	return 0;
}

/* walking through allocation entry list, from newest to oldest entries
 * read ate from addr, modify addr to the previous ate
 */
static int nvs_prev_ate(struct nvs_fs *fs, uint32_t *addr, struct nvs_ate *ate)
{
	int rc;
	struct nvs_ate close_ate;
	size_t ate_size;

	ate_size = nvs_al_size(fs, sizeof(struct nvs_ate));

	rc = nvs_flash_ate_rd(fs, *addr, ate);
	if (rc) {
		return rc;
	}

	*addr += ate_size;
	if (((*addr) & __offs_mask) != (fs->sector_size - ate_size)) {
		return 0;
	}

	/* last ate in sector, do jump to previous sector */
	if (((*addr) >> __sect_shift) == 0U) {
		*addr += ((fs->sector_count - 1) << __sect_shift);
	} else {
		*addr -= (1 << __sect_shift);
	}

	rc = nvs_flash_ate_rd(fs, *addr, &close_ate);
	if (rc) {
		return rc;
	}

	rc = nvs_ate_cmp_const(&close_ate, fs->flash_parameters->erase_value);
	/* at the end of filesystem */
	if (!rc) {
		*addr = fs->ate_wra;
		return 0;
	}

	/* Update the address if the close ate is valid.
	 */
	if (nvs_close_ate_valid(fs, &close_ate)) {
		(*addr) &= __sect_mask;
		(*addr) += close_ate.offset;
		return 0;
	}

	/* The close_ate was invalid, `lets find out the last valid ate
	 * and point the address to this found ate.
	 *
	 * remark: if there was absolutely no valid data in the sector *addr
	 * is kept at sector_end - 2*ate_size, the next read will contain
	 * invalid data and continue with a sector jump
	 */
	return nvs_recover_last_ate(fs, addr);
}

static void nvs_sector_advance(struct nvs_fs *fs, uint32_t *addr)
{
	*addr += (1 << __sect_shift);
	if ((*addr >> __sect_shift) == fs->sector_count) {
		*addr -= (fs->sector_count << __sect_shift);
	}
}

/* allocation entry close (this closes the current sector) by writing offset
 * of last ate to the sector end.
 */
static int nvs_sector_close(struct nvs_fs *fs)
{
	struct nvs_ate close_ate;
	size_t ate_size;

	ate_size = nvs_al_size(fs, sizeof(struct nvs_ate));

	close_ate.id = 0xFFFF;
	close_ate.len = 0U;
	close_ate.offset = (uint16_t)((fs->ate_wra + ate_size) & __offs_mask);
	close_ate.part = 0xff;

	fs->ate_wra &= __sect_mask;
	fs->ate_wra += (fs->sector_size - ate_size);

	nvs_ate_crc8_update(&close_ate);

	(void)nvs_flash_ate_wrt(fs, &close_ate);

	nvs_sector_advance(fs, &fs->ate_wra);

	fs->data_wra = fs->ate_wra & __sect_mask;

	return 0;
}

static int nvs_add_gc_done_ate(struct nvs_fs *fs)
{
	struct nvs_ate gc_done_ate;

	LOG_DBG("Adding gc done ate at %x", fs->ate_wra & __offs_mask);
	gc_done_ate.id = 0xffff;
	gc_done_ate.len = 0U;
	gc_done_ate.part = 0xff;
	gc_done_ate.offset = (uint16_t)(fs->data_wra & __offs_mask);
	nvs_ate_crc8_update(&gc_done_ate);

	return nvs_flash_ate_wrt(fs, &gc_done_ate);
}

/* garbage collection: the address ate_wra has been updated to the new sector
 * that has just been started. The data to gc is in the sector after this new
 * sector.
 */
static int nvs_gc(struct nvs_fs *fs)
{
	int rc;
	struct nvs_ate close_ate, gc_ate, wlk_ate;
	uint32_t sec_addr, gc_addr, gc_prev_addr, wlk_addr, wlk_prev_addr,
	      data_addr, stop_addr;
	size_t ate_size;

	ate_size = nvs_al_size(fs, sizeof(struct nvs_ate));

	sec_addr = (fs->ate_wra & __sect_mask);
	nvs_sector_advance(fs, &sec_addr);
	gc_addr = sec_addr + fs->sector_size - ate_size;

	/* if the sector is not closed don't do gc */
	rc = nvs_flash_ate_rd(fs, gc_addr, &close_ate);
	if (rc < 0) {
		/* flash error */
		return rc;
	}

	rc = nvs_ate_cmp_const(&close_ate, fs->flash_parameters->erase_value);
	if (!rc) {
		goto gc_done;
	}

	stop_addr = gc_addr - ate_size;

	if (nvs_close_ate_valid(fs, &close_ate)) {
		gc_addr &= __sect_mask;
		gc_addr += close_ate.offset;
	} else {
		rc = nvs_recover_last_ate(fs, &gc_addr);
		if (rc) {
			return rc;
		}
	}
    
	if(gc_addr >= stop_addr) {
		goto gc_done;
	}

	do {
		gc_prev_addr = gc_addr;
		rc = nvs_prev_ate(fs, &gc_addr, &gc_ate);
		if (rc) {
			return rc;
		}

		if (!nvs_ate_valid(fs, &gc_ate)) {
			continue;
		}

#ifdef CONFIG_NVS_LOOKUP_CACHE
		wlk_addr = fs->lookup_cache[nvs_lookup_cache_pos(gc_ate.id)];

		if (wlk_addr == NVS_LOOKUP_CACHE_NO_ADDR) {
			wlk_addr = fs->ate_wra;
		}
#else
		wlk_addr = fs->ate_wra;
#endif
		do {
			wlk_prev_addr = wlk_addr;
			rc = nvs_prev_ate(fs, &wlk_addr, &wlk_ate);
			if (rc) {
				return rc;
			}
			/* if ate with same id is reached we might need to copy.
			 * only consider valid wlk_ate's. Something wrong might
			 * have been written that has the same ate but is
			 * invalid, don't consider these as a match.
			 */
			if ((wlk_ate.id == gc_ate.id) &&
			    (nvs_ate_valid(fs, &wlk_ate))) {
				break;
			}
		} while (wlk_addr != fs->ate_wra);

		/* if walk has reached the same address as gc_addr copy is
		 * needed unless it is a deleted item.
		 */
		if ((wlk_prev_addr == gc_prev_addr) && gc_ate.len) {
			/* copy needed */
			LOG_DBG("Moving %d, len %d", gc_ate.id, gc_ate.len);

			data_addr = (gc_prev_addr & __sect_mask);
			data_addr += gc_ate.offset;

			gc_ate.offset = (uint16_t)(fs->data_wra & __offs_mask);
			nvs_ate_crc8_update(&gc_ate);
			
			if(gc_ate.len >= fs->sector_size) {
				goto gc_done;
			}

			rc = nvs_flash_block_move(fs, data_addr, gc_ate.len);
			if (rc) {
				return rc;
			}

			rc = nvs_flash_ate_wrt(fs, &gc_ate);
			if (rc) {
				return rc;
			}
		}
	} while (gc_prev_addr != stop_addr);

gc_done:

	/* Make it possible to detect that gc has finished by writing a
	 * gc done ate to the sector. In the field we might have nvs systems
	 * that do not have sufficient space to add this ate, so for these
	 * situations avoid adding the gc done ate.
	 */

	if (fs->ate_wra >= (fs->data_wra + ate_size)) {
		rc = nvs_add_gc_done_ate(fs);
		if (rc) {
			return rc;
		}
	}

	/* Erase the gc'ed sector */
	rc = nvs_flash_erase_sector(fs, sec_addr);
	if (rc) {
		return rc;
	}
	return 0;
}

static int nvs_startup(struct nvs_fs *fs)
{
	int rc;
	struct nvs_ate last_ate;
	size_t ate_size, empty_len;
	/* Initialize addr to 0 for the case fs->sector_count == 0. This
	 * should never happen as this is verified in nvs_mount() but both
	 * Coverity and GCC believe the contrary.
	 */
	uint32_t addr = 0U;
	uint16_t i, closed_sectors = 0;
	uint8_t erase_value = fs->flash_parameters->erase_value;

	k_mutex_lock(&fs->nvs_lock, K_FOREVER);

	ate_size = nvs_al_size(fs, sizeof(struct nvs_ate));
	/* step through the sectors to find a open sector following
	 * a closed sector, this is where NVS can write.
	 */
	for (i = 0; i < fs->sector_count; i++) {
		addr = (i << __sect_shift) +
		       (uint16_t)(fs->sector_size - ate_size);
		rc = nvs_flash_cmp_const(fs, addr, erase_value,
					 sizeof(struct nvs_ate));
		if (rc) {
			/* closed sector */
			closed_sectors++;
			nvs_sector_advance(fs, &addr);
			rc = nvs_flash_cmp_const(fs, addr, erase_value,
						 sizeof(struct nvs_ate));
			if (!rc) {
				/* open sector */
				break;
			}
		}
	}
	/* all sectors are closed, this is not a nvs fs */
	if (closed_sectors == fs->sector_count) {
		rc = -EDEADLK;
		goto end;
	}

	if (i == fs->sector_count) {
		/* none of the sectors where closed, in most cases we can set
		 * the address to the first sector, except when there are only
		 * two sectors. Then we can only set it to the first sector if
		 * the last sector contains no ate's. So we check this first
		 */
		rc = nvs_flash_cmp_const(fs, addr - ate_size, erase_value,
				sizeof(struct nvs_ate));
		if (!rc) {
			/* empty ate */
			nvs_sector_advance(fs, &addr);
		}
	}

	/* addr contains address of closing ate in the most recent sector,
	 * search for the last valid ate using the recover_last_ate routine
	 */

	rc = nvs_recover_last_ate(fs, &addr);
	if (rc) {
		goto end;
	}

	/* addr contains address of the last valid ate in the most recent sector
	 * search for the first ate containing all cells erased, in the process
	 * also update fs->data_wra.
	 */
	fs->ate_wra = addr;
	fs->data_wra = addr & __sect_mask;

	while (fs->ate_wra >= fs->data_wra) {
		rc = nvs_flash_ate_rd(fs, fs->ate_wra, &last_ate);
		if (rc) {
			goto end;
		}

		rc = nvs_ate_cmp_const(&last_ate, erase_value);

		if (!rc) {
			/* found ff empty location */
			break;
		}

		if (nvs_ate_valid(fs, &last_ate)) {
			/* complete write of ate was performed */
			fs->data_wra = addr & __sect_mask;
			/* Align the data write address to the current
			 * write block size so that it is possible to write to
			 * the sector even if the block size has changed after
			 * a software upgrade (unless the physical ATE size
			 * will change)."
			 */
			fs->data_wra += nvs_al_size(fs, last_ate.offset + last_ate.len);

			/* ate on the last position within the sector is
			 * reserved for deletion an entry
			 */
			if (fs->ate_wra == fs->data_wra && last_ate.len) {
				/* not a delete ate */
				rc = -ESPIPE;
				goto end;
			}
		}

		fs->ate_wra -= ate_size;
	}

	/* if the sector after the write sector is not empty gc was interrupted
	 * we might need to restart gc if it has not yet finished. Otherwise
	 * just erase the sector.
	 * When gc needs to be restarted, first erase the sector otherwise the
	 * data might not fit into the sector.
	 */
	addr = fs->ate_wra & __sect_mask;
	nvs_sector_advance(fs, &addr);
	rc = nvs_flash_cmp_const(fs, addr, erase_value, fs->sector_size);
	if (rc < 0) {
		goto end;
	}
	if (rc) {
		/* the sector after fs->ate_wrt is not empty, look for a marker
		 * (gc_done_ate) that indicates that gc was finished.
		 */
		bool gc_done_marker = false;
		struct nvs_ate gc_done_ate;

		addr = fs->ate_wra + ate_size;
		while ((addr & __offs_mask) < (fs->sector_size - ate_size)) {
			rc = nvs_flash_ate_rd(fs, addr, &gc_done_ate);
			if (rc) {
				goto end;
			}
			if (nvs_ate_valid(fs, &gc_done_ate) &&
			    (gc_done_ate.id == 0xffff) &&
			    (gc_done_ate.len == 0U)) {
				gc_done_marker = true;
				break;
			}
			addr += ate_size;
		}

		if (gc_done_marker) {
			/* erase the next sector */
			LOG_INF("GC Done marker found");
			addr = fs->ate_wra & __sect_mask;
			nvs_sector_advance(fs, &addr);
			rc = nvs_flash_erase_sector(fs, addr);
			goto end;
		}
		LOG_INF("No GC Done marker found: restarting gc");
		rc = nvs_flash_erase_sector(fs, fs->ate_wra);
		if (rc) {
			goto end;
		}
		fs->ate_wra &= __sect_mask;
		fs->ate_wra += (fs->sector_size - 2 * ate_size);
		fs->data_wra = (fs->ate_wra & __sect_mask);
#ifdef CONFIG_NVS_LOOKUP_CACHE
		/**
		 * At this point, the lookup cache wasn't built but the gc function need to use it.
		 * So, temporarily, we set the lookup cache to the end of the fs.
		 * The cache will be rebuilt afterwards
		 **/
		for (i = 0; i < CONFIG_NVS_LOOKUP_CACHE_SIZE; i++) {
			fs->lookup_cache[i] = fs->ate_wra;
		}
#endif
		rc = nvs_gc(fs);
		goto end;
	}

	/* possible data write after last ate write, update data_wra */
	while (fs->ate_wra > fs->data_wra) {
		empty_len = fs->ate_wra - fs->data_wra;

		rc = nvs_flash_cmp_const(fs, fs->data_wra, erase_value,
				empty_len);
		if (rc < 0) {
			goto end;
		}
		if (!rc) {
			break;
		}

		fs->data_wra += fs->flash_parameters->write_block_size;
	}

	/* If the ate_wra is pointing to the first ate write location in a
	 * sector and data_wra is not 0, erase the sector as it contains no
	 * valid data (this also avoids closing a sector without any data).
	 */
	if (((fs->ate_wra + 2 * ate_size) == fs->sector_size) &&
	    (fs->data_wra != (fs->ate_wra & __sect_mask))) {
		rc = nvs_flash_erase_sector(fs, fs->ate_wra);
		if (rc) {
			goto end;
		}
		fs->data_wra = fs->ate_wra & __sect_mask;
	}

end:

#ifdef CONFIG_NVS_LOOKUP_CACHE
	if (!rc) {
		rc = nvs_lookup_cache_rebuild(fs);
	}
#endif
	/* If the sector is empty add a gc done ate to avoid having insufficient
	 * space when doing gc.
	 */
	if ((!rc) && ((fs->ate_wra & __offs_mask) ==
		      (fs->sector_size - 2 * ate_size))) {

		rc = nvs_add_gc_done_ate(fs);
	}
	k_mutex_unlock(&fs->nvs_lock);
	LOG_INF("nvs_startup return rc: %d", rc);
	return rc;
}

int nvs_clear(struct nvs_fs *fs)
{
	int rc;
	uint32_t addr;

	if (!fs->ready) {
		LOG_ERR("NVS not initialized");
		return -EACCES;
	}

	LOG_INF("Sector count: %d", fs->sector_count);

	for (uint16_t i = 0; i < fs->sector_count; i++) {
		addr = i << __sect_shift;
		rc = nvs_flash_erase_sector(fs, addr);
		if (rc) {
			return rc;
		}
	}

	/* nvs needs to be reinitialized after clearing */
	fs->ready = false;

	return 0;
}

int nvs_mount(struct nvs_fs *fs)
{

	int rc;
	size_t write_block_size;

	k_mutex_init(&fs->nvs_lock);

	fs->flash_parameters = &flash_para;
	if (fs->flash_parameters == NULL) {
		LOG_ERR("Could not obtain flash parameters");
		return -EINVAL;
	}
	// TODO Not adopt to all chip and flash die
	write_block_size = fs->flash_parameters->write_block_size;

	/* check that the write block size is supported */
	if (write_block_size > NVS_BLOCK_SIZE || write_block_size == 0) {
		LOG_ERR("Unsupported write block size");
		return -EINVAL;
	}

	if (!fs->sector_size || fs->sector_size % NVS_SECTOR_PAGE_SIZE) {
		LOG_ERR("Invalid sector size");
		return -EINVAL;
	}

	/* check the number of sectors, it should be at least 2 */
	if (fs->sector_count < 2) {
		LOG_ERR("Configuration error - sector count");
		return -EINVAL;
	}

	rc = nvs_startup(fs);
	if (rc) {
		return rc;
	}

	/* nvs is ready for use */
	fs->ready = true;

	CLOGI("%d Sectors of %d bytes", fs->sector_count, fs->sector_size);
	CLOGI("ate wra: %d|%x(sector|offset)",
		(fs->ate_wra >> __sect_shift),
		(fs->ate_wra & __offs_mask));
	CLOGI("data wra: %d|%x(sector|offset)",
		(fs->data_wra >> __sect_shift),
		(fs->data_wra & __offs_mask));

	return 0;
}

ssize_t nvs_write(struct nvs_fs *fs, uint16_t id, const void *data, size_t len)
{
	int rc, gc_count;
	size_t ate_size, data_size;
	struct nvs_ate wlk_ate;
	uint32_t wlk_addr, rd_addr;
	uint16_t required_space = 0U; /* no space, appropriate for delete ate */
	bool prev_found = false;

	if (!fs->ready) {
		LOG_ERR("NVS not initialized");
		return -EACCES;
	}

	ate_size = nvs_al_size(fs, sizeof(struct nvs_ate));
	data_size = nvs_al_size(fs, len);

	/* The maximum data size is sector size - 4 ate
	 * where: 1 ate for data, 1 ate for sector close, 1 ate for gc done,
	 * and 1 ate to always allow a delete.
	 */
	if ((len > (fs->sector_size - 4 * ate_size)) ||
	    ((len > 0) && (data == NULL))) {
		return -EINVAL;
	}

	/* find latest entry with same id */
#ifdef CONFIG_NVS_LOOKUP_CACHE
	wlk_addr = fs->lookup_cache[nvs_lookup_cache_pos(id)];

	if (wlk_addr == NVS_LOOKUP_CACHE_NO_ADDR) {
		goto no_cached_entry;
	}
#else
	wlk_addr = fs->ate_wra;
#endif
	rd_addr = wlk_addr;

	while (1) {
		rd_addr = wlk_addr;
		rc = nvs_prev_ate(fs, &wlk_addr, &wlk_ate);
		if (rc) {
			return rc;
		}
		if ((wlk_ate.id == id) && (nvs_ate_valid(fs, &wlk_ate))) {
			prev_found = true;
			break;
		}
		if (wlk_addr == fs->ate_wra) {
			break;
		}
	}


#ifdef CONFIG_NVS_LOOKUP_CACHE
no_cached_entry:
#endif

	if (prev_found) {
		/* previous entry found */
		rd_addr &= __sect_mask;
		rd_addr += wlk_ate.offset;

		if (len == 0) {
			/* do not try to compare with empty data */
			if (wlk_ate.len == 0U) {
				/* skip delete entry as it is already the
				 * last one
				 */
				return 0;
			}
		} else if (len == wlk_ate.len) {
			/* do not try to compare if lengths are not equal */
			/* compare the data and if equal return 0 */
			rc = nvs_flash_block_cmp(fs, rd_addr, data, len);
			if (rc <= 0) {
				return rc;
			}
		}
	} else {
		/* skip delete entry for non-existing entry */
		if (len == 0) {
			return 0;
		}
	}

	/* calculate required space if the entry contains data */
	if (data_size) {
		/* Leave space for delete ate */
		required_space = data_size + ate_size;
	}

	k_mutex_lock(&fs->nvs_lock, K_FOREVER);

	gc_count = 0;
	while (1) {
		if (gc_count == fs->sector_count) {
			/* gc'ed all sectors, no extra space will be created
			 * by extra gc.
			 */
			rc = -ENOSPC;
			goto end;
		}

		if (fs->ate_wra >= (fs->data_wra + required_space)) {

			rc = nvs_flash_wrt_entry(fs, id, data, len);
			if (rc) {
				goto end;
			}
			break;
		}


		rc = nvs_sector_close(fs);
		if (rc) {
			goto end;
		}

		rc = nvs_gc(fs);
		if (rc) {
			goto end;
		}
		gc_count++;
	}
	rc = len;
end:
	k_mutex_unlock(&fs->nvs_lock);
	return rc;
}

int nvs_delete(struct nvs_fs *fs, uint16_t id)
{
	return nvs_write(fs, id, NULL, 0);
}

ssize_t nvs_read_hist(struct nvs_fs *fs, uint16_t id, void *data, size_t len,
		      uint16_t cnt)
{
	int rc;
	uint32_t wlk_addr, rd_addr;
	uint16_t cnt_his;
	struct nvs_ate wlk_ate;
	size_t ate_size;

	if (!fs->ready) {
		LOG_ERR("NVS not initialized");
		return -EACCES;
	}

	ate_size = nvs_al_size(fs, sizeof(struct nvs_ate));

	if (len > (fs->sector_size - 2 * ate_size)) {
		return -EINVAL;
	}

	cnt_his = 0U;

#ifdef CONFIG_NVS_LOOKUP_CACHE
	wlk_addr = fs->lookup_cache[nvs_lookup_cache_pos(id)];

	if (wlk_addr == NVS_LOOKUP_CACHE_NO_ADDR) {
		rc = -ENOENT;
		goto err;
	}
#else
	wlk_addr = fs->ate_wra;
#endif
	rd_addr = wlk_addr;

	while (cnt_his <= cnt) {
		rd_addr = wlk_addr;
		rc = nvs_prev_ate(fs, &wlk_addr, &wlk_ate);
		if (rc) {
			goto err;
		}
		if ((wlk_ate.id == id) &&  (nvs_ate_valid(fs, &wlk_ate))) {
			cnt_his++;
		}
		if (wlk_addr == fs->ate_wra) {
			break;
		}
	}

	if (((wlk_addr == fs->ate_wra) && (wlk_ate.id != id)) ||
	    (wlk_ate.len == 0U) || (cnt_his < cnt)) {
		return -ENOENT;
	}

	rd_addr &= __sect_mask;
	rd_addr += wlk_ate.offset;
	rc = nvs_flash_rd(fs, rd_addr, data, MIN(len, wlk_ate.len));
	if (rc) {
		goto err;
	}

	return wlk_ate.len;

err:
	return rc;
}

ssize_t nvs_read(struct nvs_fs *fs, uint16_t id, void *data, size_t len)
{
	int rc;

	rc = nvs_read_hist(fs, id, data, len, 0);
	return rc;
}

ssize_t nvs_readptr(struct nvs_fs *fs, uint16_t id, uint32_t *addr)
{
	int rc;
	uint32_t wlk_addr, rd_addr;
	struct nvs_ate wlk_ate;
	size_t ate_size;
	off_t offset;

	if (!fs->ready) {
		LOG_ERR("NVS not initialized");
		return -EACCES;
	}

	ate_size = nvs_al_size(fs, sizeof(struct nvs_ate));

	wlk_addr = fs->ate_wra;
	rd_addr = wlk_addr;

	while (1) {
		rd_addr = wlk_addr;
		rc = nvs_prev_ate(fs, &wlk_addr, &wlk_ate);
		if (rc) {
			goto err;
		}
		if ((wlk_ate.id == id) &&  (nvs_ate_valid(fs, &wlk_ate))) {
			break;
		}
		if (wlk_addr == fs->ate_wra) {
			break;
		}
	}

	if (((wlk_addr == fs->ate_wra) && (wlk_ate.id != id)) ||
	    (wlk_ate.len == 0U)) {
		return -ENOENT;
	}

	rd_addr &= __sect_mask;
	rd_addr += wlk_ate.offset;

	// Calculate the offset in the flash memory
	offset = fs->offset;
	offset += fs->sector_size * (rd_addr >> __sect_shift);
	offset += rd_addr & __offs_mask;

	*addr = (uint32_t)offset;

	return wlk_ate.len;

err:
	return rc;
}

ssize_t nvs_calc_free_space(struct nvs_fs *fs)
{

	int rc;
	struct nvs_ate step_ate, wlk_ate;
	uint32_t step_addr, wlk_addr;
	size_t ate_size, free_space;

	if (!fs->ready) {
		LOG_ERR("NVS not initialized");
		return -EACCES;
	}

	ate_size = nvs_al_size(fs, sizeof(struct nvs_ate));

	free_space = 0;
	for (uint16_t i = 1; i < fs->sector_count; i++) {
		free_space += (fs->sector_size - ate_size);
	}

	step_addr = fs->ate_wra;

	while (1) {
		rc = nvs_prev_ate(fs, &step_addr, &step_ate);
		if (rc) {
			return rc;
		}

		wlk_addr = fs->ate_wra;

		while (1) {
			rc = nvs_prev_ate(fs, &wlk_addr, &wlk_ate);
			if (rc) {
				return rc;
			}
			if ((wlk_ate.id == step_ate.id) ||
			    (wlk_addr == fs->ate_wra)) {
				break;
			}
		}

		if ((wlk_addr == step_addr) && step_ate.len &&
		    (nvs_ate_valid(fs, &step_ate))) {
			/* count needed */
			free_space -= nvs_al_size(fs, step_ate.len);
			free_space -= ate_size;
		}

		if (step_addr == fs->ate_wra) {
			break;
		}
	}
	return free_space;
}


struct nvs_fs *nvs_fs;
/**
 ****************************************************************************************
 * @brief Initialize NVDS.
 * @return NVDS_OK
 ****************************************************************************************
 */
uint8_t nvds_init(struct nvs_fs *fs)
{
	nvs_fs = fs;
	return nvs_mount(nvs_fs);
}

/**
 ****************************************************************************************
 * @brief Look for a specific tag and return, if found and matching (in length), the
 *        DATA part of the TAG.
 *
 * If the length does not match, the TAG header structure is still filled, in order for
 * the caller to be able to check the actual length of the TAG.
 *
 * @param[in]  tag     TAG to look for whose DATA is to be retrieved
 * @param[in]  length  Expected length of the TAG
 * @param[out] buf     A pointer to the buffer allocated by the caller to be filled with
 *                     the DATA part of the TAG
 *
 * @return  NVDS_OK                  The read operation was performed
 *          NVDS_LENGTH_OUT_OF_RANGE The length passed in parameter is different than the TAG's
 ****************************************************************************************
 */
uint8_t nvds_get(uint16_t tag, size_t *lengthPtr, uint8_t *buf)
{
	int res = nvs_read(nvs_fs, tag, buf, *lengthPtr);
	if (res <= 0)
		return NVDS_FAIL;
	else {
		// TODO int32 -> uint32
		*lengthPtr = res;
		return NVDS_OK;
	}
}


/**
 ****************************************************************************************
 * @brief Look for a specific tag and return a pointer to the DATA part of the TAG.
 *
 * The actual length of the TAG is returned via lengthPtr, allowing the caller to verify
 * the size of the TAG's DATA.
 *
 * @param[in]  tag        TAG to look for whose DATA pointer is to be retrieved
 * @param[out] lengthPtr  Pointer to store the actual length of the TAG's DATA
 * @param[out] buf        A pointer to the buffer allocated by the caller to store the
 *                        address of the DATA part of the TAG
 *
 * @return  ssize_t       The size of the data retrieved, or a negative error code
 *                        (e.g., -NVDS_LENGTH_OUT_OF_RANGE if the TAG is not found or invalid)
 ****************************************************************************************
 */
uint8_t nvds_getptr(uint16_t tag, size_t *lengthPtr, uint32_t *buf){
	ssize_t res;
	res = nvs_readptr(nvs_fs, tag, buf);
	if (res <= 0){
		*lengthPtr = 0;
		return NVDS_FAIL;
	}
	else {
		// TODO int32 -> uint32
		*lengthPtr = res;
		return NVDS_OK;
	}
}

/**
 ****************************************************************************************
 * @brief Look for a specific tag and delete it (Status set to invalid)
 *
 * Implementation notes
 * 1. The write function call return status is not handled
 *
 * @param[in]  tag    TAG to mark as deleted
 *
 * @return NVDS_OK                TAG found and deleted
 *         NVDS_PARAM_LOCKED    TAG found but can not be deleted because it is locked
 *         (others)        return values from function call @ref nvds_browse_tag
 ****************************************************************************************
 */
uint8_t nvds_del(uint16_t tag)
{
	int res = nvs_delete(nvs_fs, tag);
	if (res <= 0)
		return NVDS_FAIL;
	else
		return NVDS_OK;
}

/**
 ****************************************************************************************
 * @brief This function adds a specific TAG to the NVDS.
 *
 * Steps:
 * 1) parse all the TAGs to:
 * 1.1) calculate the total size of all the valid TAGs
 * 1.2) erase the existing TAGs that have the same ID
 * 1.3) check if we can use the same TAG area in case of an EEPROM
 * 1.4) check that the TAG is not locked
 * 2) if we have to add the new TAG at the end fo the NVDS (cant use same area):
 * 2.1) allocate the appropriate amount of memory
 * 2.2) purge the NVDS
 * 2.3) free the memory allocated
 * 2.4) check that there is now enough room for the new TAG or return
 *      NO_SPACE_AVAILABLE
 * 3) add the new TAG
 *
 * @param[in]  tag     TAG to look for whose DATA is to be retrieved
 * @param[in]  length  Expected length of the TAG
 * @param[in]  buf     Pointer to the buffer containing the DATA part of the TAG to add to
 *                     the NVDS
 *
 * @return NVDS_OK                  New TAG correctly written to the NVDS
 *         NVDS_PARAM_LOCKED        New TAG is trying to overwrite a TAG that is locked
 *         NO_SPACE_AVAILABLE       New TAG can not fit in the available space in the NVDS
 ****************************************************************************************
 */
uint8_t nvds_put(uint16_t tag, size_t length, uint8_t *buf)
{
	int res = nvs_write(nvs_fs, tag, buf, length);
	if (res <= 0)
		return NVDS_FAIL;
	else
		return NVDS_OK;
}

