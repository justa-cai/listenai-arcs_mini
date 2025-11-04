/**
 ****************************************************************************************
 *
 * @file net_ip.h
 *
 * @brief Definitions for net ip
 *
 * Copyright (C) ListenAI 2024-2099
 *
 ****************************************************************************************
 */

#ifndef _NET_IP_H_
#define _NET_IP_H_


/**
 * Enum for IP address configuration mode
 */
enum ip_addr_mode
{
    IP_ADDR_NONE,
    IP_ADDR_STATIC_IPV4,
    IP_ADDR_DHCP_CLIENT,
};

/**
 * Fully Hosted IP address configuration (only IPv4 for now)
 */
struct ip_addr_cfg
{
    /**
     * Select how to configure ip address when calling @ref fhost_set_vif_ip
     * Indicate how ip was configured when updated by @ref fhost_get_vif_ip
     */
    enum ip_addr_mode mode;
    /**
     * Whether interface must be the default output interface
     * (Unspecified when calling @ref fhost_get_vif_ip)
     */
    bool default_output;
    union
    {
        /**
         * IPv4 config.
         * Must be set when calling @ref fhost_set_vif_ip with @p
         * mode==IP_ADDR_STATIC_IPV4
         * It is always updated by @ref fhost_get_vif_ip independently of @p mode value
         */
        struct
        {
            /**
             * IPv4 address
             */
            uint32_t addr;
            /**
             * IPv4 address mask
             */
            uint32_t mask;
            /**
             * IPv4 address of the gateway
             */
            uint32_t gw;
            /**
             * DNS server to use. (Ignored if set to 0)
             */
            uint32_t dns;
        } ipv4;
        /**
         * DHCP config.
         * Must be set when calling @ref fhost_set_vif_ip with @p
         * addr_mode==IP_ADDR_DHCP_CLIENT
         */
        struct
        {
            /**
             * Timeout, in ms, to obtained an IP address
             */
            uint32_t to_ms;
        } dhcp;
    };
};

/**
 ****************************************************************************************
 * @brief Retrieve IP address configuration of an interface
 *
 * This function can be used to retrieve ip address of an interface. So far
 * only IPv4 is supported.
 *
 * @note This function can only be used if the @ref NET_AL implements the IP
 * related functions
 *
 * @param[in]  vif_idx  Index of the interface
 * @param[out] cfg       Configuration of the interface
 *
 * @return LS_OK on success and LS_NET_IP_ERR_XXX if any error occurred.
 ****************************************************************************************
 */
int ls_get_ip(int vif_idx, struct ip_addr_cfg *cfg);
/**
 ****************************************************************************************
 * @brief Start DHCPC for an interface
 *
 * @note This function can only be used if the @ref NET_AL implements the IP
 * related functions
 *
 * @param[in]     vif_idx  Index of the interface
 *
 * @return LS_OK on success and LS_NET_IP_ERR_XXX if any error occurred.
 ****************************************************************************************
 */
int ls_dhcpc_start(int vif_idx);
/**
 ****************************************************************************************
 * @brief Configure an interface static IP address
 *
 * @note This function can only be used if the @ref NET_AL implements the IP
 * related functions
 *
 * @param[in]     vif_idx  Index of the interface
 * @param[in]       cfg    Configuration of the interface
 *
 * @return LS_OK on success and LS_NET_IP_ERR_XXX if any error occurred.
 ****************************************************************************************
 */
int ls_set_static_ip(int vif_idx, struct ip_addr_cfg *cfg);

/**
 ****************************************************************************************
 * @brief Stop DHCPC for an interface
 *
 * @note This function can only be used if the @ref NET_AL implements the IP
 * related functions
 *
 * @param[in]     vif_idx  Index of the interface
 *
 * @return LS_OK on success and LS_NET_IP_ERR_XXX if any error occurred.
 ****************************************************************************************
 */
int ls_dhcpc_stop(int vif_idx);

/**
 ****************************************************************************************
 * @brief Start DHCP Server for SoftAP interface
 *
 *
 * @param[in]     vif_idx  Index of the interface
 *
 * @return LS_OK on success and LS_NET_IP_ERR_XXX if any error occurred.
 ****************************************************************************************
 */
int ls_dhcps_start(int vif_idx);

/**
 ****************************************************************************************
 * @brief Stop DHCP Server for SoftAP interface
 *
 *
 * @return LS_OK on success and LS_NET_IP_ERR_XXX if any error occurred.
 ****************************************************************************************
 */
int ls_dhcps_stop(void);

/// @}

#endif // _NET_IP_H_
