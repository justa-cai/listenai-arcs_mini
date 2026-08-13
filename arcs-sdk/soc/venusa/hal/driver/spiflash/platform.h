/**
 * @file platform.h
 * @brief Platform-specific definitions and SPIB controller interface for AE210P platform
 *
 * This header provides platform configuration flags, memory section attributes,
 * and complete API for SPIB (Serial Peripheral Interface Block) controller operations.
 * It includes timing mode definitions, version information, and all necessary
 * control/data register access functions.
 *
 * @note All SPIB base address parameters refer to the physical address of the SPIB controller instance.
 */
#ifndef __PLATFORM__
#define __PLATFORM__

/** Enable Quad SPI Flash Mode */
#define FLASH_QUAD_MODE_EN

/**
 * @brief Section attribute for external RAM code storage when enabled
 * @details When CONFIG_EXT_RAM is defined, this places code in dedicated .ramcode section
 */
#ifdef CONFIG_EXT_RAM
#define _EXT_RAM __attribute__((section (".ramcode")))
#else
#define _EXT_RAM
#endif

/**
 * @enum SPIB_TimingModes
 * @brief SPIB transaction mode selection codes
 *
 * These constants define different combinations of read/write operations during SPIB transactions.
 * Each code represents a specific sequence of operations relative to clock edges.
 */
#define SPIB_TM_WRsim               0x0 ///< Write Simultaneously (default)
#define SPIB_TM_WRonly              0x1 ///< Write Only
#define SPIB_TM_RDonly              0x2 ///< Read Only
#define SPIB_TM_WR_RD               0x3 ///< Write then Read
#define SPIB_TM_RD_WR               0x4 ///< Read then Write
#define SPIB_TM_WR_DY_RD            0x5 ///< Write During Rising Edge, Read Falling Edge
#define SPIB_TM_RD_DY_WR            0x6 ///< Read During Rising Edge, Write Falling Edge
#define SPIB_TM_NONE				0x7 ///< No Operation
#define SPIB_TM_DY_WR				0x8 ///< Dynamic Write
#define SPIB_TM_DY_RD				0x9 ///< Dynamic Read

/** Version identifier for SPIB controller implementation */
#define SPIB_VERSION                0x02002000

/**
 * @brief Retrieve current interrupt function set configuration
 * @param[in] base - Base address of SPIB controller
 * @return Current interrupt function set register value
 */
extern unsigned int spib_get_ifset (unsigned long base);

/**
 * @brief Set interrupt function set configuration
 * @param[in] base - Base address of SPIB controller
 * @param[in] reg - New interrupt function set value
 */
extern void spib_set_ifset (unsigned long base, unsigned int reg);

/**
 * @brief Get current PIO (Pin Input/Output) configuration
 * @param[in] base - Base address of SPIB controller
 * @return Current PIO configuration register value
 */
extern unsigned int spib_get_pio (unsigned long base);

/**
 * @brief Set PIO configuration
 * @param[in] base - Base address of SPIB controller
 * @param[in] reg - New PIO configuration value
 */
extern void spib_set_pio (unsigned long base, unsigned int reg);

/**
 * @brief Retrieve current control register state
 * @param[in] base - Base address of SPIB controller
 * @return Current control register value
 */
extern unsigned int spib_get_ctrl (unsigned long base);

/**
 * @brief Set control register configuration
 * @param[in] base - Base address of SPIB controller
 * @param[in] reg - New control register value
 */
extern void spib_set_ctrl (unsigned long base, unsigned int reg);

/**
 * @brief Retrieve current interrupt enable status
 * @param[in] base - Base address of SPIB controller
 * @return Current interrupt enable register value
 */
extern unsigned int spib_get_inten (unsigned long base);

/**
 * @brief Set interrupt enable configuration
 * @param[in] base - Base address of SPIB controller
 * @param[in] reg - New interrupt enable mask
 */
extern void spib_set_inten (unsigned long base, unsigned int reg);

/**
 * @brief Get current interrupt status flags
 * @param[in] base - Base address of SPIB controller
 * @return Current interrupt status register value
 */
extern unsigned int spib_get_intst (unsigned long base);

/**
 * @brief Set interrupt status flags
 * @param[in] base - Base address of SPIB controller
 * @param[in] reg - New interrupt status flags
 */
extern void spib_set_intst (unsigned long base, unsigned int reg);

/**
 * @brief Retrieve current data control register state
 * @param[in] base - Base address of SPIB controller
 * @return Current data control register value
 */
extern unsigned int spib_get_dctrl (unsigned long base);

/**
 * @brief Set data control register configuration
 * @param[in] base - Base address of SPIB controller
 * @param[in] reg - New data control register value
 */
extern void spib_set_dctrl (unsigned long base, unsigned int reg);

/**
 * @brief Retrieve current command register value
 * @param[in] base - Base address of SPIB controller
 * @return Current command register contents
 */
extern unsigned int spib_get_cmd(unsigned long base);

/**
 * @brief Set command register with new value
 * @param[in] base - Base address of SPIB controller
 * @param[in] cmd - Command to issue to SPIB controller
 */
extern void spib_set_cmd(unsigned long base, unsigned int cmd);

/**
 * @brief Retrieve current address register value
 * @param[in] base - Base address of SPIB controller
 * @return Current address pointer value
 */
extern unsigned int spib_get_addr(unsigned long base);

/**
 * @brief Set address register with new value
 * @param[in] base - Base address of SPIB controller
 * @param[in] addr - New address value to load
 */
extern void spib_set_addr(unsigned long base, unsigned int addr);

/**
 * @brief Retrieve current data register value
 * @param[in] base - Base address of SPIB controller
 * @return Current data register contents
 */
extern unsigned int spib_get_data(unsigned long base);

/**
 * @brief Set data register with new value
 * @param[in] base - Base address of SPIB controller
 * @param[in] data - Data value to write
 */
extern void spib_set_data(unsigned long base, unsigned int data);

/**
 * @brief Retrieve current register timing configuration
 * @param[in] base - Base address of SPIB controller
 * @return Current timing settings register value
 */
extern unsigned int spib_get_regtiming(unsigned long base);

/**
 * @brief Set register timing configuration
 * @param[in] base - Base address of SPIB controller
 * @param[in] data - New timing configuration value
 */
extern void spib_set_regtiming(unsigned long base, unsigned int data);

/**
 * @brief Prepare complex data control parameters
 * @param[in] cmden - Command enable flag
 * @param[in] addren - Address enable flag
 * @param[in] tm - Timing mode selection
 * @param[in] wcnt - Word count value
 * @param[in] dycnt - Dummy cycle count
 * @param[in] rcnt - Read count value
 * @return Composite data control register value
 * @note This combines multiple control elements into a single DCTRL register value
 */
extern unsigned int spib_prepare_dctrl(unsigned int cmden,
	    unsigned int addren,
	    unsigned int tm,
	    unsigned int wcnt,
	    unsigned int dycnt,
	    unsigned int rcnt);

/**
 * @brief Wait for SPIB controller to become idle
 * @param[in] base - Base address of SPIB controller
 * @return Status indicating success/failure
 * @retval 0 Success
 * @retval Non-zero Error condition
 */
extern int spib_wait_spi (unsigned long base);

/**
 * @brief Retrieve SPIB controller version information
 * @param[in] base - Base address of SPIB controller
 * @return Version identification number
 */
extern unsigned int spib_get_version (unsigned long base);

/**
 * @brief Check if SPIB controller is busy
 * @param[in] base - Base address of SPIB controller
 * @return Non-zero if busy, zero if ready
 */
extern unsigned int spib_get_busy (unsigned long base);

/**
 * @brief Check if receive FIFO is empty
 * @param[in] base - Base address of SPIB controller
 * @return Non-zero if empty, zero if entries exist
 */
extern unsigned int spib_get_rx_empty (unsigned long base);

/**
 * @brief Wait until receive FIFO becomes empty
 * @param[in] base - Base address of SPIB controller
 * @return Status indicating success/failure
 * @retval 0 Success
 * @retval Non-zero Error condition or timeout
 */
extern int spib_wait_rx_empty(unsigned long base);

/**
 * @brief Get number of entries currently in receive FIFO
 * @param[in] base - Base address of SPIB controller
 * @return Number of received entries available
 */
extern unsigned int spib_get_rx_entries (unsigned long base);

/**
 * @brief Clear SPIB receive FIFO buffer
 * @param[in] base - Base address of SPIB controller
 */
extern void spib_clr_fifo (unsigned long base);

/**
 * @brief Execute SPIB command with specified parameters
 * @param[in] base - Base address of SPIB controller
 * @param[in] op_addr - Operation address/command
 * @param[in] spib_dctrl - Preconfigured data control register value
 */
extern void spib_exe_cmmd (unsigned long base, unsigned int op_addr, unsigned int spib_dctrl);

/**
 * @brief Read data from SPIB receiver buffer
 * @param[in] base - Base address of SPIB controller
 * @param[out] pRxdata - Pointer to buffer for received data
 * @param[in] RxBytes - Number of bytes to receive
 */
extern void spib_rx_data (unsigned long base, unsigned int *pRxdata, int RxBytes);

/**
 * @brief Transmit data through SPIB transmitter
 * @param[in] base - Base address of SPIB controller
 * @param[in] pTxdata - Pointer to data buffer to transmit
 * @param[in] TxBytes - Number of bytes to transmit
 */
extern void spib_tx_data (unsigned long base, void *pTxdata, int TxBytes);

/**
 * @brief Advanced data control preparation with additional formatting options
 * @param[in] cmden - Command enable flag
 * @param[in] addren - Address enable flag
 * @param[in] tm - Timing mode selection
 * @param[in] wcnt - Word count value
 * @param[in] dycnt - Dummy cycle count
 * @param[in] rcnt - Read count value
 * @param[in] addrfmt - Address format specification
 * @param[in] datafmt - Data format specification
 * @param[in] tokenen - Token enable flag
 * @return Fully configured data control register value
 * @note Includes additional formatting options beyond basic prepare_dctrl()
 */
extern unsigned int spib_prepare_dctrl2(unsigned int cmden,
	    unsigned int addren,
	    unsigned int tm,
	    unsigned int wcnt,
	    unsigned int dycnt,
	    unsigned int rcnt,
		unsigned int addrfmt,
		unsigned int datafmt,
		unsigned int tokenen);

/**
 * @brief Execute command with separate operation and address parameters
 * @param[in] base - Base address of SPIB controller
 * @param[in] op - Operation code
 * @param[in] addr - Target address
 * @param[in] spib_dctrl - Preconfigured data control register value
 */
extern void spib_exe_cmmd2(unsigned long base, unsigned int op, unsigned int addr, unsigned int spib_dctrl);

#endif
