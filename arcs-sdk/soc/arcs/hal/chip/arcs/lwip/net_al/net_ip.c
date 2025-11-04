/**
 ****************************************************************************************
 *
 * @file net_ip.c
 *
 * @brief Implementation of the function related to IP configuration.
 *
 * Copyright (C) ListenAI 2024 ~ 2099
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @defgroup NET_IP NET_IP
 * @ingroup NET
 * @{
 ****************************************************************************************
 */
#include "ls_err.h"
#include "ls_wifi_type.h"

#include "log_print.h"
#include "ls_event.h"
#include "utils_math.h"

#include "net_al.h"
#include "net_ip.h"
#include "dhcps.h"

static uint32_t stop_dhcpc = 0;

/**
 ******************************************************************************
 * @brief Stop using DHCP
 *
 * Release DHCP lease for the specified interface and stop DHCP procedure.
 *
 * @param[in] net_if  Pointer to network interface structure
 *
 * @return 0 if DHCP is stopped, != 0 an error occurred.
 ******************************************************************************
 */
static int stop_dhcp(net_if_t *net_if)
{
    // Release DHCP lease
    if (!net_dhcp_address_obtained(net_if))
    {
        if (net_dhcp_release(net_if))
        {
            CLOGE("Failed to release DHCP");
            return LS_NET_IP_ERR_DHCPCRELEASE;
        }

        CLOGI("IP released");
    }

    // Stop DHCP
    net_dhcp_stop(net_if);

    return 0;
}


/*
 ****************************************************************************************
 * PUBLIC FUNCTIONS
 ****************************************************************************************
 */

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
int ls_get_ip(int vif_idx, struct ip_addr_cfg *cfg)
{
    net_if_t *net_if;

    if (vif_idx >= WLIF_IDX_MAX)
        return LS_NET_IP_ERR_VIFIDX;

    net_if = net_if_get(vif_idx);
    if (!net_if)
        return LS_NET_IP_ERR_NONETIF;

    if (!net_dhcp_address_obtained(net_if))
        cfg->mode = IP_ADDR_DHCP_CLIENT;
    else
        cfg->mode = IP_ADDR_STATIC_IPV4;

    cfg->default_output = false;

    net_if_get_ip(net_if, &(cfg->ipv4.addr), &(cfg->ipv4.mask), &(cfg->ipv4.gw));
    net_get_dns(&(cfg->ipv4.dns));

    return LS_OK;
}

void ls_netif_status_callback_handler(struct netif *netif)
{
    struct ip_addr_cfg cfg = {0};
    wifi_mode_e mode;
    uint8_t vif_idx;

    if ((netif == NULL) || (!netif_is_up(netif)) || (!netif_is_link_up(netif)))
    {
        return;
    }

    vif_idx = net_if_to_idx(netif);
    if (LS_OK != wifi_get_mode(vif_idx, &mode))
    {
        return;
    }

    if (mode != WIFI_MODE_STA)
    {
        return;
    }

    if (net_dhcp_address_obtained(netif))
    {
        CLOGI("DHCP IP invalid");
        //wifi_sta_disconnect();
        return;
    }

    netif_set_default(netif);

    net_if_get_ip(netif, &(cfg.ipv4.addr), &(cfg.ipv4.mask), &(cfg.ipv4.gw));
    net_get_dns(&cfg.ipv4.dns);

    ls_event_post(EVENT_WIFI, EVENT_WIFI_GOT_IP, NULL, 0, LS_NEVER_TIMEOUT, 0);

    CLOGI("{VIF-%d} ip=%d.%d.%d.%d/%d",
                vif_idx, cfg.ipv4.addr & 0xff, (cfg.ipv4.addr >> 8) & 0xff,
                (cfg.ipv4.addr >> 16) & 0xff, (cfg.ipv4.addr >> 24) & 0xff,
                32 - co_clz(cfg.ipv4.mask));
}


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
int net_dhcpc_start(int vif_idx)
{
    net_if_t *net_if;
    int ret = LS_OK;
    uint32_t start_ms =0, to_ms = 15000;
    struct ip_addr_cfg cfg = {0};

    if (vif_idx >= WLIF_IDX_MAX)
    {
        ret = LS_NET_IP_ERR_VIFIDX;
        goto err_out;
    }

    net_if = net_if_get(vif_idx);
    if (!net_if)
    {
        ret = LS_NET_IP_ERR_NONETIF;
        goto err_out;
    }

    if (!netif_is_up(net_if))
    {
        ret = LS_NET_IP_ERR_IFDOWN;
        goto err_out;
    }

    netif_set_status_callback(net_if, ls_netif_status_callback_handler);
    ret = net_dhcp_start(net_if);

err_out:
    if (ret)
    {
        CLOGE("%s failed reason = %d ", __func__, ret);
        ls_event_post(EVENT_WIFI, EVENT_WIFI_STA_DHCP_FAIL, NULL, 0, LS_NEVER_TIMEOUT, 0);
    }

    return ret;
}

int ls_dhcpc_start(int vif_idx)
{
    CLOGI("vif[%d] start dhcp...", vif_idx);

    if(vif_idx >= WLIF_IDX_MAX)
         vif_idx = 0;

    net_dhcpc_start(vif_idx);
}

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
int ls_set_static_ip(int vif_idx, struct ip_addr_cfg *cfg)
{
    net_if_t *net_if;
    int ret = LS_OK;

    if (vif_idx >= WLIF_IDX_MAX)
    {
        ret = LS_NET_IP_ERR_VIFIDX;
        goto err_out;
    }

    net_if = net_if_get(vif_idx);
    if (!net_if)
    {
        ret = LS_NET_IP_ERR_NONETIF;
        goto err_out;
    }

    // To be safe, stop dhcp first
    stop_dhcp(net_if);
    net_if->dhcp_started = 0;
    net_if_set_ip(net_if, cfg->ipv4.addr, cfg->ipv4.mask, cfg->ipv4.gw);
    net_if->static_ip = 1;

    if (cfg->ipv4.dns)
        net_set_dns(cfg->ipv4.dns);
    else
        net_get_dns(&cfg->ipv4.dns);

    if (cfg->default_output)
        net_if_set_default(net_if);
err_out:
    CLOGI("vif[%d] static ip %d\r\n", vif_idx, ret);
    return ret;
}

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
int ls_dhcpc_stop(int vif_idx)
{
    net_if_t *net_if;
    int ret = LS_OK;

    if (vif_idx >= WLIF_IDX_MAX)
    {
        ret = LS_NET_IP_ERR_VIFIDX;
        goto err_out;
    }

    net_if = net_if_get(vif_idx);
    if (!net_if)
    {
        ret = LS_NET_IP_ERR_NONETIF;
        goto err_out;
    }

    net_if->static_ip = 0;
    net_if->dhcp_started = 0;
    // clear current IP address
    stop_dhcp(net_if);
    netif_set_status_callback(net_if, NULL);
    net_if_set_ip(net_if, 0, 0, 0);
    stop_dhcpc = 1;

err_out:
    CLOGI("vif[%d] dhcp stop %d\r\n", vif_idx, ret);
    return ret;
}


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
int ls_dhcps_start(int vif_idx)
{
    net_if_t *net_if;
    int ret = LS_OK;

    if (vif_idx >= WLIF_IDX_MAX)
    {
        ret = LS_NET_IP_ERR_VIFIDX;
        goto err_out;
    }

    net_if = net_if_get(vif_idx);
    if (!net_if)
    {
        ret = LS_NET_IP_ERR_NONETIF;
        goto err_out;
    }
    // set default netif
    net_if_set_default(net_if);

    // start dhcp server
    dhcps_start(net_if);

err_out:
    return ret;
}

/**
 ****************************************************************************************
 * @brief Stop DHCP Server for SoftAP interface
 *
 *
 * @return LS_OK on success and LS_NET_IP_ERR_XXX if any error occurred.
 ****************************************************************************************
 */
int ls_dhcps_stop(void)
{
    dhcps_stop();
    return LS_OK;
}

/// @}
