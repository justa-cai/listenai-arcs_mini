/**
 * @file mmio.h
 * @brief Microcontroller Memory-Mapped I/O (MMIO) Access Functions
 * @details This header provides low-level routines for direct memory access to MCU peripherals and RAM,
 *          including full register writes/reads and bit-field manipulation functions.
 * @note All functions use 'volatile' qualifier to prevent compiler optimization issues with hardware registers.
 */
#ifndef __MMIO_H
#define __MMIO_H

#include <stdint.h>

/**
 * @brief Write a 32-bit value directly to MCU memory address
 * @param[in] addr: Target memory address (physical address mapping)
 * @param[in] value: 32-bit value to write
 * @return void
 * @note Uses volatile pointer dereferencing to ensure immediate hardware update
 */
static inline void mmio_write32(uint32_t addr, uint32_t value)
{
    *(volatile uint32_t *)(addr) = value;
}

/**
 * @brief Read a 32-bit value from MCU memory address
 * @param[in] addr: Source memory address (physical address mapping)
 * @return The 32-bit value stored at the specified address
 * @note Uses volatile pointer dereferencing for accurate hardware reads
 */
static inline uint32_t mmio_read32(uint32_t addr)
{
    return *(volatile uint32_t *)(addr);
}

/**
 * @brief Modify specific bit field in a 32-bit register
 * @param[in] addr: Target register address
 * @param[in] value: New value for the bit field
 * @param[in] bit_width: Number of significant bits in the value (1-32)
 * @param[in] shift: Bit position where the field starts (LSB alignment)
 * @return void
 * @details Operation sequence:
 *          1. Read current register value
 *          2. Clear existing field using mask
 *          3. Apply new value with proper shifting
 *          4. Write updated value back to register
 * @note Field operations maintain unchanged bits outside the specified field
 */
static inline void mmio_write32_field(uint32_t addr, uint32_t value, uint8_t bit_width, uint8_t shift)
{
    uint32_t reg_value;
    uint32_t field_mask;

    reg_value = mmio_read32(addr);

    field_mask = (1L << bit_width) - 1;

    reg_value &= ~(field_mask << shift);
    reg_value |= (value & field_mask) << shift;

    mmio_write32(addr, reg_value);
}

/**
 * @brief Extract specific bit field from a 32-bit register
 * @param[in] addr: Source register address
 * @param[in] bit_width: Number of significant bits to extract (1-32)
 * @param[in] shift: Bit position where the field starts (LSB alignment)
 * @return Extracted field value (right-aligned in return value)
 * @details Extraction steps:
 *          1. Read entire register value
 *          2. Right shift to align field with LSB
 *          3. Apply bit mask to clear extraneous bits
 * @note Returns zero-extended value matching the requested bit width
 */
static inline uint32_t mmio_read32_field(uint32_t addr, uint8_t bit_width, uint8_t shift)
{
    uint32_t reg_value;
    uint32_t field_mask;

    reg_value = mmio_read32(addr);

    field_mask = (1L << bit_width) - 1;

    reg_value >>= shift;
    reg_value &= field_mask;

    return reg_value;
}

#endif /* __MMIO_H */
