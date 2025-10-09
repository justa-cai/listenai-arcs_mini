#ifndef __EFUSE_ARCS_H
#define __EFUSE_ARCS_H

#include "arcs_ap.h"

/**
 * @brief Reads a 32-bit word from the specified eFuse address.
 *
 * This function reads a 32-bit (word) value from the eFuse. The address is
 * specified in word units and automatically converted to a byte address
 * for the eFuse read operation.
 *
 * @param[in]  addr The eFuse address in word units, valid range is 0x0 to 0x80.
 * @param[out] val       Pointer to store the 32-bit data read from the eFuse.
 *
 * @return int8_t Returns 0 on success, -1 if the address is out of the valid range.
 *
 * @note This function assumes that the IP_EFUSE_CTRL registers are properly
 *       initialized, and the hardware supports the read command operations.
 */
int8_t efuse_read_word(uint8_t addr, uint32_t *val);

/**
 * @brief Controls the eFuse programming enable or disable state.
 *
 * This function enables or disables the eFuse programming mode by
 * setting the protection register. When enabling, the protection
 * register is set to a specific value if it is currently not set.
 * When disabling, the protection register is cleared.
 *
 * @param[in] enable A non-zero value enables the programming mode,
 *                   while zero disables it.
 */
void efuse_program_ctrl(char enable);

/**
 * @brief Writes a 32-bit word to the specified eFuse address.
 *
 * This function writes a 32-bit (word) value to the eFuse. The address is
 * specified in word units and automatically converted to a byte address
 * for the eFuse write operation. The function writes each bit of the
 * 32-bit value by calling `efuse_write_bit`.
 *
 * @param[in] addr_word The eFuse address in word units, valid range is 0x0 to 0x80.
 * @param[in] val       The 32-bit value to be written to the eFuse.
 *
 * @return int8_t Returns 0 on success, -1 if the address is out of the valid range,
 *                or a non-zero error code if the write operation fails.
 *
 * @note This function assumes that the eFuse programming mode is enabled and that
 *       the `efuse_write_bit` function is available to perform bit-level writes.
 */
int8_t efuse_write_word(uint8_t addr, uint32_t val);

/**
 * @brief Writes a single bit to the specified eFuse address.
 *
 * This function performs a bit-level write to the eFuse. It sets the
 * eFuse address and bit position, initiates the programming command,
 * and waits for the operation to complete.
 *
 * @param[in] addr The eFuse address in byte units, valid range is 0x0 to 0x1F.
 * @param[in] bit  The bit position within the byte, valid range is 0 to 7.
 *
 * @return int8_t Returns 0 on success, -1 if the address or bit position
 *                is out of the valid range.
 *
 * @note This function assumes that the eFuse programming mode is enabled.
 */
int8_t efuse_write_bit(uint8_t addr, uint8_t bit);

/**
 * @brief Forces the eFuse auto-load operation.
 *
 * This function initiates the auto-load process for the eFuse by setting
 * a specific value to the auto-load start register. It then waits for the
 * auto-load operation to complete.
 *
 * @note This function assumes that the system is ready for the auto-load
 *       operation and that the eFuse controller is properly configured.
 */
void efuse_force_auto_load(void);

uint64_t efuse_read_uuid(void);


#endif
