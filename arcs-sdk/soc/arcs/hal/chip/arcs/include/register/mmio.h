#ifndef __MMIO_H
#define __MMIO_H

#include <stdint.h>


/**
* @brief Write mcu's register or ram
* @param[in] addr: mcu's address
* @param[in] value: The value to be written
* @return void
**/
static inline void mmio_write32(uint32_t addr, uint32_t value)
{
    *(volatile uint32_t *)(addr) = value;
}


/**
* @brief Read mcu's register or ram
* @param[in] addr: mcu's address
* @return The value of the mcu's address
**/
static inline uint32_t mmio_read32(uint32_t addr)
{
    return *(volatile uint32_t *)(addr);
}


/**
* @brief Write some bits of the mcu's register or ram
* @param[in] addr: mcu's address
* @param[in] value: The value to be written
* @param[in] bit_width: The width bits of the value to be written
* @param[in] shift: The number of bits that need to be left shifted for the value to be written
* @return void
**/
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
* @brief Read some bits of the mcu's register or ram
* @param[in] addr: mcu's address
* @param[in] bit_width: The width bits of the value to be read out
* @param[in] shift: The number of bits that need to be left shifted for the value to be read out
* @return The value to be read out
**/
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


