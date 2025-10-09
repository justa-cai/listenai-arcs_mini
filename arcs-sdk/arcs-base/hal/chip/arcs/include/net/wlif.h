#ifndef _LS_NETIF_H_
#define _LS_NETIF_H_

#include "co_list.h"
#include "utils_math.h"
#include "net_def.h"
#include "ls_wifi_type.h"

#ifndef CO_BIT
#define CO_BIT(pos) (1UL << (pos))
#endif


//#define WLIF_IDX_MAX                 1
#define TX_STATUS_ACKNOWLEDGED       CO_BIT(3)
#define NX_MAX_MSDU_PER_RX_AMSDU     8
#define DEF_BLK_SZ                   128
#define LLC_LLC_LEN                  3
#define LLC_SNAP_LEN                 5
#define MAC_AMSDU_HDR_LEN            14
#define LLC_802_2_HDR_LEN            (LLC_LLC_LEN + LLC_SNAP_LEN)
#define LLC_ETHER_MTU                1500
#define RX_MAX_AMSDU_SUBFRAME_LEN    (MAC_AMSDU_HDR_LEN + LLC_802_2_HDR_LEN + LLC_ETHER_MTU)
#define FHOST_RX_BUF_SIZE            (RX_MAX_AMSDU_SUBFRAME_LEN + 1)

/// Rx Buffer action status
enum rx_buf_action_status
{
    // forwarded to upper layer
    RX_BUF_FORWARD = CO_BIT(0),
    // resent on wireless interface
    RX_BUF_RESEND = CO_BIT(1),
};

enum net_tx_buf_type
{
    /// Packet with an ethernet header (from the network stack)
    IEEE802_3,
    /// Packet with a wifi header
    IEEE802_11,
};

 /// Interface types
enum mac_vif_type
{
    /// ESS STA interface
    VIF_STA,
    /// IBSS STA interface
    VIF_IBSS,
    /// AP interface
    VIF_AP,
    /// Mesh Point interface
    VIF_MESH_POINT,
    /// Monitor interface
    VIF_MONITOR,
    /// Unknown type
    VIF_UNKNOWN,
};

struct vif_info_tag
{
    uint8_t type;
    bool active;
};

struct wlif
{
    /// RTOS network interface structure
    net_if_t *netif;
    /// MAC address of the VIF
    uint8_t mac_addr[6];
    struct vif_info_tag mac_vif;
};

struct wlif_env
{
    struct wlif vif[WLIF_IDX_MAX];
};

struct wlif_status
{
    /**
     * Maximum number of interface supported
     */
    int vif_max_cnt;
    /**
     * Number of active interface
     */
    int vif_active_cnt;
    /**
     * Index of the first active interface. (Valid only if vif_active_cnt is not 0)
     */
    int vif_first_active;
};

extern struct wlif_env netif_env;

/// MAC address length in bytes.
#define MAC_ADDR_LEN 6

/// MAC address structure.
struct mac_addr
{
    /// Array of 16-bit words that make up the MAC address.
    uint16_t array[MAC_ADDR_LEN / 2];
};

struct mac_eth_hdr
{
    /// Destination Address
    struct mac_addr da;
    /// Source Address
    struct mac_addr sa;
    /// Length / Type
    uint16_t len;
} __PACKED;

/// Receive Vector specific part for NON-HT and NON-HT-DUP-OFDM frames
struct rx_vect_1_leg
{
    /// Dynamic Bandwidth
    uint8_t dyn_bw_in_non_ht     : 1;
    /// Channel Bandwidth
    uint8_t chn_bw_in_non_ht     : 2;
    /// Not used (offset only)
    uint8_t rsvd_nht             : 4;
    /// L-SIG Valid
    uint8_t lsig_valid           : 1;
} __PACKED;

/// Receive Vector specific part for HT frames
struct rx_vect_1_ht
{
    /// Sounding bit
    uint16_t sounding             : 1;
    /// Smoothing bit
    uint16_t smoothing            : 1;
    /// Guard Interval Type bit
    uint16_t short_gi             : 1;
    /// MPDU Aggregate bit
    uint16_t aggregation          : 1;
    /// Space Time Block Coding bit
    uint16_t stbc                 : 1;
    /// Number of Extension Spatial Streams
    uint16_t num_extn_ss          : 2;
    /// L-SIG Valid
    uint16_t lsig_valid           : 1;
    /// Modulation Coding Scheme
    uint16_t mcs                  : 7;
    /// FEC Coding
    uint16_t fec                  : 1;
    /// Lenght of HT PPDU
    uint16_t length               : 16;
} __PACKED;

/// Receive Vector specific part for VHT frames
struct rx_vect_1_vht
{
    /// Sounding bit
    uint8_t sounding              : 1;
    /// BeamFormed bit
    uint8_t beamformed            : 1;
    /// Guard Interval Type
    uint8_t short_gi              : 1;
    /// Not used (offset only)
    uint8_t rsvd_vht1             : 1;
    /// Space Time Block Coding
    uint8_t stbc                  : 1;
    /// TXOP PS Not Allowed
    uint8_t doze_not_allowed      : 1;
    /// First User
    uint8_t first_user            : 1;
    /// Not used (offset only)
    uint8_t rsvd_vht2             : 1;
    /// Partial AID
    uint16_t partial_aid          : 9;
    /// Group ID
    uint16_t group_id             : 6;
    /// Not used (offset only)
    uint16_t rsvd_vht3            : 1;
    /// Modulation Coding Scheme
    uint32_t mcs                  : 4;
    /// Number of Spatial Streams
    uint32_t nss                  : 3;
    /// FEC Coding
    uint32_t fec                  : 1;
    /// Lenght of VHT PPDU
    uint32_t length               : 20;
    /// Not used (offset only)
    uint32_t rsvd_vht4            : 4;
} __PACKED;

/// Receive Vector specific part for HE frames
struct rx_vect_1_he
{
    /// Sounding bit
    uint8_t sounding              : 1;
    /// BeamFormed bit
    uint8_t beamformed            : 1;
    /// Guard Interval Type
    uint8_t gi_type               : 2;
    /// Space Time Block Coding
    uint8_t stbc                  : 1;
    /// Not Used (offset only)
    uint8_t rsvd_he1              : 3;
    /// UP link Flag
    uint8_t uplink_flag           : 1;
    /// Beam Change
    uint8_t beam_change           : 1;
    /// Dual Carrier Modulation
    uint8_t dcm                   : 1;
    /// Type of HE-LTF
    uint8_t he_ltf_type           : 2;
    /// Doppler bit
    uint8_t doppler               : 1;
    /// Not Used (offset only)
    uint8_t rsvd_he2              : 2;
    /// BSS Color
    uint8_t bss_color             : 6;
    /// Not Used (offset only)
    uint8_t rsvd_he3              : 2;
    /// Duration of TX OP
    uint8_t txop_duration         : 7;
    /// Not Used (offset only)
    uint8_t rsvd_he4              : 1;
    /// Packet Extension Duration
    uint8_t pe_duration           : 4;
    /// Spatial Reuse
    uint8_t spatial_reuse         : 4;

    /// SIG-B Compression Mode
    uint8_t sig_b_comp_mode       : 1;
    /// SIG-B Dual Carrier Modulation
    uint8_t dcm_sig_b             : 1;
    /// SIG-B Modulation Coding Scheme
    uint8_t mcs_sig_b             : 3;
    /// RU Size
    uint8_t ru_size               : 3;

    /// Modulation Coding Scheme
    uint32_t mcs                   : 4;
    /// Number of Spatial Streams
    uint32_t nss                   : 3;
    /// FEC Coding
    uint32_t fec                   : 1;
    /// Length of PPDU
    uint32_t length                : 20;
    /// Not Used (offset only)
    uint32_t rsvd_he6              : 4;
} __PACKED;

/// Structure for receive Vector 1
struct rx_vector_1
{
    /// Format Modulation
    uint8_t format_mod         : 4;
    /// Channel Bandwidth
    uint8_t ch_bw              : 3;
    /// Preamble Type
    uint8_t pre_type           : 1;
    /// Antenna Set
    uint8_t antenna_set        : 8;
    /// RSSI Legacy
    int32_t rssi_leg           : 8;
    /// Legacy Length
    uint32_t leg_length        : 12;
    /// Legacy Rate
    uint32_t leg_rate          : 4;
    /// RSSI
    int32_t rssi1              : 8;
    union
    {
        /// non-ht and non-ht-dup-ofdm bitmap
        struct rx_vect_1_leg leg;
        /// ht-mm and ht-gf bitmap
        struct rx_vect_1_ht ht;
        /// vht bitmap
        struct rx_vect_1_vht vht;
        /// he bitmap
        struct rx_vect_1_he he;
    };
} __PACKED;
/// Structure for receive Vector 2
struct rx_vector_2
{
    /// Contains the bytes 4 - 1 of Receive Vector 2
    uint32_t recvec2a;
    ///  Contains the bytes 8 - 5 of Receive Vector 2
    uint32_t recvec2b;
};

struct rx_vector
{
    /// Total length of the received MPDU
    uint16_t frmlen;
    /// AMPDU status information
    uint16_t ampdu_stat_info;
    /// TSF Low
    uint32_t tsflo;
    /// TSF High
    uint32_t tsfhi;
    /// Receive Vector 1
    struct rx_vector_1 rx_vec_1;
    /// Receive Vector 2
    struct rx_vector_2 rx_vec_2;
    /// MPDU status information
    uint32_t statinfo;
};

struct phy_channel_info
{
    /// PHY channel information 1
    uint32_t info1;
    /// PHY channel information 2
    uint32_t info2;
};

/// Structure containing the information about the received payload
struct rx_info
{
    /// Rx header descriptor (this element MUST be the first of the structure)
    struct rx_vector vect;
    /// Structure containing information about the PHY channel that was used for this RX
    struct phy_channel_info phy_info;
    /// UMAC SW flags about the RX packet (@ref rx_flags_bf)
    uint32_t flags;
    /// Array of host buffer identifiers for the other A-MSDU subframes
    uint32_t amsdu_hostids[NX_MAX_MSDU_PER_RX_AMSDU - 1];
    /// Array of A-MSDU subframe length (including the first one)
    uint16_t amsdu_len[NX_MAX_MSDU_PER_RX_AMSDU];
    /// Spare room for LMAC FW to write a pattern when last DMA is sent
    uint32_t pattern;
};

/// Structure containing the information about the payload that should be resend
struct tx_info
{
    /// Network stack buffer element
    void *tx_buf;
    void *ip_hdr;
};

/// FHOST RX environment structure
struct wlif_rx_buf_tag
{
    net_buf_rx_t net_buf;
    /// Structure containing the information to resend on Tx
    struct tx_info info_tx;
    /// Number of reference to Rx buffer
    uint8_t ref;
    /// Structure containing the information about the received payload - Payload must be
    /// just after !!!
    struct rx_info info;
    /// Payload buffer space - must be after rx_info  !!!
    uint32_t payload[CO_ALIGN4_HI(FHOST_RX_BUF_SIZE) / sizeof(uint32_t)];
};

/// Structure containing FHOST control information for the present buffer
struct wlif_tx_ctrl_tag
{
    /// Pointer to the network stack buffer structure
    void *buf;
    /// TX confirmation callback (Only used for mgmt frame)
    void *cfm_cb;
    /// TX confirmation callback argument
    void *cfm_cb_arg;
    /// RX environment structure that is resent
    struct wlif_rx_buf_tag *buf_rx;
    /// Buffer timeout
    uint32_t timeout;
};

/// Structure mapped into the TX buffer for internal handling
struct wlif_tx_desc_tag_partial
{
    /// Chained list element
    struct ls_list_hdr hdr;
    /// FHOST TX control information
    struct wlif_tx_ctrl_tag ctrl;
    /// TX SW descriptor passed to MAC
};

__INLINE net_if_t *wlif_get_if(uint8_t vif_idx)
{
    ASSERT_ERR(vif_idx < WLIF_IDX_MAX);
    return (netif_env.vif[vif_idx].netif);
}

__INLINE struct vif_info_tag *wlif_get_vif(uint8_t vif_idx)
{
    struct vif_info_tag *mac_vif = &netif_env.vif[vif_idx].mac_vif;

    // Sanity check - Currently we consider that when this function is called there shall
    // be a MAC VIF attached to the FHOST VIF. If in the future this has to change then
    // this assertion will be removed
    ASSERT_ERR(mac_vif != NULL);

    return mac_vif;
}

__INLINE struct vif_info_tag *wlif_get_mac_vif(uint8_t vif_idx)
{
    struct vif_info_tag *mac_vif = &netif_env.vif[vif_idx].mac_vif;

    // Sanity check - Currently we consider that when this function is called there shall
    // be a MAC VIF attached to the FHOST VIF. If in the future this has to change then
    // this assertion will be removed
    ASSERT_ERR(mac_vif != NULL);

    return mac_vif;
}


int32_t wlif_init(void);
void wlif_vif_init(int vif_idx, uint8_t *base_mac_addr);
int wlif_name(int vif_idx, char *name, int len);
int wlif_idx_from_name(const char *name);
void wlif_get_status(struct wlif_status *status);
void wlif_tx_cfm(void *data, uint32_t status);
void wlif_rx_buf_forward(void *data);
net_if_t* wlif_get_default_if(void);
void wlif_netif_up(uint8_t vif_idx, uint8_t vif_type);
void wlif_netif_down(uint8_t vif_idx);


#endif
