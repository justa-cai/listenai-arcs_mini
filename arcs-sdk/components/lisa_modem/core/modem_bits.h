/**
 * @file modem_bits.h
 * @brief Shared bit helpers for lisa_modem
 */

#ifndef LISA_MODEM_CORE_MODEM_BITS_H
#define LISA_MODEM_CORE_MODEM_BITS_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef BIT
#define BIT(n) (1U << (n))
#endif

#define BIT0 BIT(0)
#define BIT1 BIT(1)
#define BIT2 BIT(2)
#define BIT3 BIT(3)
#define BIT4 BIT(4)
#define BIT5 BIT(5)
#define BIT6 BIT(6)
#define BIT7 BIT(7)

#ifdef __cplusplus
}
#endif

#endif
