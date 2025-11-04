/*  NVS: non volatile storage in flash
 *
 * Copyright (c) 2018 Laczen
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ZEPHYR_INCLUDE_FS_NVS_H_
#define ZEPHYR_INCLUDE_FS_NVS_H_

#include <sys/types.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Non-volatile Storage (NVS)
 * @defgroup nvs Non-volatile Storage (NVS)
 * @since 1.12
 * @version 1.0.0
 * @ingroup file_system_storage
 * @{
 * @}
 */

/**
 * @brief Non-volatile Storage Data Structures
 * @defgroup nvs_data_structures Non-volatile Storage Data Structures
 * @ingroup nvs
 * @{
 */

/**
 * Flash memory parameters. Contents of this structure suppose to be
 * filled in during flash device initialization and stay constant
 * through a runtime.
 */
struct flash_parameters {
	const size_t write_block_size;
	uint8_t erase_value; /* Byte value of erased flash */
};

/**
 * @brief Non-volatile Storage File system structure
 */
struct nvs_fs {
	 /** File system offset in flash **/
	off_t offset;
	/** Allocation table entry write address.
	 * Addresses are stored as uint32_t:
	 * - high 2 bytes correspond to the sector
	 * - low 2 bytes are the offset in the sector
	 */
	uint32_t ate_wra;
	/** Data write address */
	uint32_t data_wra;
	/** File system is split into sectors, each sector must be multiple of erase-block-size */
	uint16_t sector_size;
	/** Number of sectors in the file system */
	uint16_t sector_count;
	/** Flag indicating if the file system is initialized */
	bool ready;
	/** Mutex */
	// struct k_mutex nvs_lock;
	/** Flash device runtime structure */
	struct FLASH_DEV *flash_device;
	/** Flash memory parameters structure */
	const struct flash_parameters *flash_parameters;
#if CONFIG_NVS_LOOKUP_CACHE
	uint32_t lookup_cache[CONFIG_NVS_LOOKUP_CACHE_SIZE];
#endif
};

/**
 * @}
 */

/**
 * @brief Non-volatile Storage APIs
 * @defgroup nvs_high_level_api Non-volatile Storage APIs
 * @ingroup nvs
 * @{
 */

/**
 * @brief Mount an NVS file system onto the flash device specified in @p fs.
 *
 * @param fs Pointer to file system
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int nvs_mount(struct nvs_fs *fs);

/**
 * @brief Clear the NVS file system from flash.
 *
 * @param fs Pointer to file system
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int nvs_clear(struct nvs_fs *fs);

/**
 * @brief Write an entry to the file system.
 *
 * @note  When @p len parameter is equal to @p 0 then entry is effectively removed (it is
 * equivalent to calling of nvs_delete). Any calls to nvs_read for entries with data of length
 * @p 0 will return error.@n It is not possible to distinguish between deleted entry and entry
 * with data of length 0.
 *
 * @param fs Pointer to file system
 * @param id Id of the entry to be written
 * @param data Pointer to the data to be written
 * @param len Number of bytes to be written
 *
 * @return Number of bytes written. On success, it will be equal to the number of bytes requested
 * to be written. When a rewrite of the same data already stored is attempted, nothing is written
 * to flash, thus 0 is returned. On error, returns negative value of errno.h defined error codes.
 */
ssize_t nvs_write(struct nvs_fs *fs, uint16_t id, const void *data, size_t len);

/**
 * @brief Delete an entry from the file system
 *
 * @param fs Pointer to file system
 * @param id Id of the entry to be deleted
 * @retval 0 Success
 * @retval -ERRNO errno code if error
 */
int nvs_delete(struct nvs_fs *fs, uint16_t id);

/**
 * @brief Read an entry from the file system.
 *
 * @param fs Pointer to file system
 * @param id Id of the entry to be read
 * @param data Pointer to data buffer
 * @param len Number of bytes to be read
 *
 * @return Number of bytes read. On success, it will be equal to the number of bytes requested
 * to be read. When the return value is larger than the number of bytes requested to read this
 * indicates not all bytes were read, and more data is available. On error, returns negative
 * value of errno.h defined error codes.
 */
ssize_t nvs_read(struct nvs_fs *fs, uint16_t id, void *data, size_t len);

ssize_t nvs_readptr(struct nvs_fs *fs, uint16_t id, uint32_t *addr);

/**
 * @brief Read a history entry from the file system.
 *
 * @param fs Pointer to file system
 * @param id Id of the entry to be read
 * @param data Pointer to data buffer
 * @param len Number of bytes to be read
 * @param cnt History counter: 0: latest entry, 1: one before latest ...
 *
 * @return Number of bytes read. On success, it will be equal to the number of bytes requested
 * to be read. When the return value is larger than the number of bytes requested to read this
 * indicates not all bytes were read, and more data is available. On error, returns negative
 * value of errno.h defined error codes.
 */
ssize_t nvs_read_hist(struct nvs_fs *fs, uint16_t id, void *data, size_t len, uint16_t cnt);

/**
 * @brief Calculate the available free space in the file system.
 *
 * @param fs Pointer to file system
 *
 * @return Number of bytes free. On success, it will be equal to the number of bytes that can
 * still be written to the file system. Calculating the free space is a time consuming operation,
 * especially on spi flash. On error, returns negative value of errno.h defined error codes.
 */
ssize_t nvs_calc_free_space(struct nvs_fs *fs);

/**
 * @}
 */

/// Possible Returned Status
enum NVDS_STATUS {
	/// NVDS status OK
	NVDS_OK,
	/// generic NVDS status KO
	NVDS_FAIL,
	/// NVDS TAG unrecognized
	NVDS_TAG_NOT_DEFINED,
	/// No space for NVDS
	NVDS_NO_SPACE_AVAILABLE,
	/// Length violation
	NVDS_LENGTH_OUT_OF_RANGE,
	/// NVDS parameter locked
	NVDS_PARAM_LOCKED,
	/// NVDS corrupted
	NVDS_CORRUPT
};

/**
 ****************************************************************************************
 * @brief Initialize NVDS.
 * @return NVDS_OK
 ****************************************************************************************
 */
uint8_t nvds_init(struct nvs_fs *fs);
#if CFG_NVS
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
uint8_t nvds_get(uint16_t tag, size_t *lengthPtr, uint8_t *buf);


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
uint8_t nvds_getptr(uint16_t tag, size_t *lengthPtr, uint32_t *buf);

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
uint8_t nvds_del(uint16_t tag);

/**
 ****************************************************************************************
 * @brief This function adds a specific TAG to the NVDS.
 *
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
uint8_t nvds_put(uint16_t tag, size_t length, uint8_t *buf);
#elif defined(CFG_AMP_IPC_MRPC_CLIENT_NVS)
uint8_t ipc_nvds_get(uint16_t tag, uint8_t * buf, size_t * buf_len);
uint8_t ipc_nvds_put(uint16_t tag, uint8_t * buf, size_t buf_len);
uint8_t ipc_nvds_del(uint16_t tag);

#define nvds_get(tag, buf_len, buf)  ipc_nvds_get(tag, buf, buf_len)
#define nvds_put(tag, buf_len, buf)  ipc_nvds_put(tag, buf, buf_len)
#define nvds_del(tag)  ipc_nvds_del(tag)
#endif
#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_INCLUDE_FS_NVS_H_ */
