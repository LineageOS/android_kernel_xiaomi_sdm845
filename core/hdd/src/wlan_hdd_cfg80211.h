/*
 * Copyright (c) 2012-2019 The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: wlan_hdd_cfg80211.h
 *
 * WLAN host device driver cfg80211 functions declaration
 */

#if !defined(HDD_CFG80211_H__)
#define HDD_CFG80211_H__

#include "qca_vendor.h"

/* value for initial part of frames and number of bytes to be compared */
#define GAS_INITIAL_REQ "\x04\x0a"
#define GAS_INITIAL_REQ_SIZE 2

#define GAS_INITIAL_RSP "\x04\x0b"
#define GAS_INITIAL_RSP_SIZE 2

#define GAS_COMEBACK_REQ "\x04\x0c"
#define GAS_COMEBACK_REQ_SIZE 2

#define GAS_COMEBACK_RSP "\x04\x0d"
#define GAS_COMEBACK_RSP_SIZE 2

#define P2P_PUBLIC_ACTION_FRAME "\x04\x09\x50\x6f\x9a\x09"
#define P2P_PUBLIC_ACTION_FRAME_SIZE 6

#define P2P_ACTION_FRAME "\x7f\x50\x6f\x9a\x09"
#define P2P_ACTION_FRAME_SIZE 5

#define SA_QUERY_FRAME_REQ "\x08\x00"
#define SA_QUERY_FRAME_REQ_SIZE 2

#define SA_QUERY_FRAME_RSP "\x08\x01"
#define SA_QUERY_FRAME_RSP_SIZE 2

#define HDD_P2P_WILDCARD_SSID "DIRECT-"
#define HDD_P2P_WILDCARD_SSID_LEN 7

#define WNM_BSS_ACTION_FRAME "\x0a\x07"
#define WNM_BSS_ACTION_FRAME_SIZE 2

#define WNM_NOTIFICATION_FRAME "\x0a\x1a"
#define WNM_NOTIFICATION_FRAME_SIZE 2

#define WPA_OUI_TYPE   "\x00\x50\xf2\x01"
#define BLACKLIST_OUI_TYPE   "\x00\x50\x00\x00"
#define WHITELIST_OUI_TYPE   "\x00\x50\x00\x01"
#define WPA_OUI_TYPE_SIZE  4
#define WMM_OUI_TYPE   "\x00\x50\xf2\x02\x01"
#define WMM_OUI_TYPE_SIZE  5

#define VENDOR1_AP_OUI_TYPE "\x00\xE0\x4C"
#define VENDOR1_AP_OUI_TYPE_SIZE 3

#define WLAN_BSS_MEMBERSHIP_SELECTOR_VHT_PHY 126
#define WLAN_BSS_MEMBERSHIP_SELECTOR_HT_PHY 127
#define BASIC_RATE_MASK   0x80
#define RATE_MASK         0x7f

#ifdef WLAN_ENABLE_AGEIE_ON_SCAN_RESULTS
/* GPS application requirement */
#define QCOM_VENDOR_IE_ID 221
#define QCOM_OUI1         0x00
#define QCOM_OUI2         0xA0
#define QCOM_OUI3         0xC6
#define QCOM_VENDOR_IE_AGE_TYPE  0x100
#define QCOM_VENDOR_IE_AGE_LEN   (sizeof(qcom_ie_age) - 2)

/**
 * typedef struct qcom_ie_age - age ie
 *
 * @element_id: Element id
 * @len: Length
 * @oui_1: OUI 1
 * @oui_2: OUI 2
 * @oui_3: OUI 3
 * @type: Type
 * @age: Age
 * @tsf_delta: tsf delta from FW
 * @beacon_tsf: original beacon TSF
 * @seq_ctrl: sequence control field
 */
typedef struct {
	u8 element_id;
	u8 len;
	u8 oui_1;
	u8 oui_2;
	u8 oui_3;
	u32 type;
	u32 age;
	u32 tsf_delta;
	u64 beacon_tsf;
	u16 seq_ctrl;
} __attribute__ ((packed)) qcom_ie_age;
#endif

#ifndef NL80211_AUTHTYPE_FILS_SK
#define NL80211_AUTHTYPE_FILS_SK 5
#endif
#ifndef NL80211_AUTHTYPE_FILS_SK_PFS
#define NL80211_AUTHTYPE_FILS_SK_PFS 6
#endif
#ifndef NL80211_AUTHTYPE_FILS_PK
#define NL80211_AUTHTYPE_FILS_PK 7
#endif
#ifndef WLAN_AKM_SUITE_FILS_SHA256
#define WLAN_AKM_SUITE_FILS_SHA256 0x000FAC0E
#endif
#ifndef WLAN_AKM_SUITE_FILS_SHA384
#define WLAN_AKM_SUITE_FILS_SHA384 0x000FAC0F
#endif
#ifndef WLAN_AKM_SUITE_FT_FILS_SHA256
#define WLAN_AKM_SUITE_FT_FILS_SHA256 0x000FAC10
#endif
#ifndef WLAN_AKM_SUITE_FT_FILS_SHA384
#define WLAN_AKM_SUITE_FT_FILS_SHA384 0x000FAC11
#endif

#ifndef WLAN_AKM_SUITE_OWE
#define WLAN_AKM_SUITE_OWE 0x000FAC12
#endif

#define WLAN_AKM_SUITE_EAP_SHA256 0x000FAC0B
#define WLAN_AKM_SUITE_EAP_SHA384 0x000FAC0C

#ifndef WLAN_AKM_SUITE_SAE
#define WLAN_AKM_SUITE_SAE 0x000FAC08
#endif

#ifndef WLAN_AKM_SUITE_DPP_RSN
#define WLAN_AKM_SUITE_DPP_RSN 0x506F9A02
#endif

#ifdef FEATURE_WLAN_TDLS
#define WLAN_IS_TDLS_SETUP_ACTION(action) \
	((SIR_MAC_TDLS_SETUP_REQ <= action) && \
	(SIR_MAC_TDLS_SETUP_CNF >= action))
#if !defined(TDLS_MGMT_VERSION2)
#define TDLS_MGMT_VERSION2 0
#endif

#endif

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
void wlan_hdd_clear_link_layer_stats(hdd_adapter_t *adapter);
#else
static inline void wlan_hdd_clear_link_layer_stats(hdd_adapter_t *adapter) {}
#endif

#define MAX_CHANNEL (NUM_24GHZ_CHANNELS + NUM_5GHZ_CHANNELS)
#define MAX_SCAN_SSID 10

#define IS_CHANNEL_VALID(channel) ((channel >= 0 && channel < 15) \
			|| (channel >= 36 && channel <= 184))

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)) \
	|| defined(BACKPORTED_CHANNEL_SWITCH_PRESENT)
#define CHANNEL_SWITCH_SUPPORTED
#endif

#if defined(CFG80211_DEL_STA_V2) || (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)) || defined(WITH_BACKPORTS)
#define USE_CFG80211_DEL_STA_V2
#endif

#define MAX_CANDIDATE_INFO 10
/**
 * struct bss_candidate_info - Candidate bss information
 *
 * @bssid : BSSID of candidate bss
 * @status : status code for candidate bss
 */
struct bss_candidate_info {
	struct qdf_mac_addr bssid;
	uint32_t status;
};

/**
 * enum eDFS_CAC_STATUS: CAC status
 *
 * @DFS_CAC_NEVER_DONE: CAC never done
 * @DFS_CAC_IN_PROGRESS: CAC is in progress
 * @DFS_CAC_IN_PROGRESS: CAC already done
 */
typedef enum {
	DFS_CAC_NEVER_DONE,
	DFS_CAC_IN_PROGRESS,
	DFS_CAC_ALREADY_DONE,
} eDFS_CAC_STATUS;

#define MAX_REQUEST_ID			0xFFFFFFFF

/* Add more features here */
#define WIFI_TDLS_SUPPORT			BIT(0)
#define WIFI_TDLS_EXTERNAL_CONTROL_SUPPORT	BIT(1)
#define WIIF_TDLS_OFFCHANNEL_SUPPORT		BIT(2)

#if defined(FEATURE_WLAN_CH_AVOID) || defined(FEATURE_WLAN_FORCE_SAP_SCC)
#define HDD_MAX_AVOID_FREQ_RANGES   4

/**
 * typedef struct sHddAvoidFreqList - avoid frequency list
 *
 * @avoidFreqRangeCount: Avoid frequency range count
 * @avoidFreqRange: Avoid frequency list
 */

/* Feature defines */
#define WIFI_FEATURE_INFRA              0x0001  /* Basic infrastructure mode */
#define WIFI_FEATURE_INFRA_5G           0x0002  /* Support for 5 GHz Band */
#define WIFI_FEATURE_HOTSPOT            0x0004  /* Support for GAS/ANQP */
#define WIFI_FEATURE_P2P                0x0008  /* Wifi-Direct */
#define WIFI_FEATURE_SOFT_AP            0x0010  /* Soft AP */
#define WIFI_FEATURE_EXTSCAN            0x0020  /* Extended Scan APIs */
#define WIFI_FEATURE_NAN                0x0040  /* Neighbor Awareness
						   Networking */
#define WIFI_FEATURE_D2D_RTT		0x0080  /* Device-to-device RTT */
#define WIFI_FEATURE_D2AP_RTT           0x0100  /* Device-to-AP RTT */
#define WIFI_FEATURE_BATCH_SCAN         0x0200  /* Batched Scan (legacy) */
#define WIFI_FEATURE_PNO                0x0400  /* Preferred network offload */
#define WIFI_FEATURE_ADDITIONAL_STA     0x0800  /* Support for two STAs */
#define WIFI_FEATURE_TDLS               0x1000  /* Tunnel directed link
						   setup */
#define WIFI_FEATURE_TDLS_OFFCHANNEL	0x2000  /* Support for TDLS off
						   channel */
#define WIFI_FEATURE_EPR                0x4000  /* Enhanced power reporting */
#define WIFI_FEATURE_AP_STA             0x8000  /* Support for AP STA
						   Concurrency */
#define WIFI_FEATURE_LINK_LAYER_STATS   0x10000  /* Link layer stats */
#define WIFI_FEATURE_LOGGER             0x20000  /* WiFi Logger */
#define WIFI_FEATURE_HAL_EPNO           0x40000  /* WiFi PNO enhanced */
#define WIFI_FEATURE_RSSI_MONITOR       0x80000  /* RSSI Monitor */
#define WIFI_FEATURE_MKEEP_ALIVE        0x100000  /* WiFi mkeep_alive */
#define WIFI_FEATURE_CONFIG_NDO         0x200000  /* ND offload configure */
#define WIFI_FEATURE_TX_TRANSMIT_POWER  0x400000  /* Tx transmit power levels */
#define WIFI_FEATURE_CONTROL_ROAMING    0x800000  /* Enable/Disable roaming */
#define WIFI_FEATURE_IE_WHITELIST       0x1000000 /* Support Probe IE white listing */
#define WIFI_FEATURE_SCAN_RAND          0x2000000 /* Support MAC & Probe Sequence Number randomization */

/* Support Tx Power Limit setting */
#define WIFI_FEATURE_SET_TX_POWER_LIMIT 0x4000000


/**
 * typedef struct sHddAvoidFreqRange - avoid frequency range
 *
 * @startFreq: Start frequency range
 * @endFreq: End frequency range
 */
typedef struct sHddAvoidFreqRange {
	u32 startFreq;
	u32 endFreq;
} tHddAvoidFreqRange;

/**
 * typedef struct sHddAvoidFreqList - avoid frequency list
 *
 * @avoidFreqRangeCount: Avoid frequency range count
 * @avoidFreqRange: Avoid frequency list
 */
typedef struct sHddAvoidFreqList {
	u32 avoidFreqRangeCount;
	tHddAvoidFreqRange avoidFreqRange[HDD_MAX_AVOID_FREQ_RANGES];
} tHddAvoidFreqList;
#endif /* FEATURE_WLAN_CH_AVOID || FEATURE_WLAN_FORCE_SAP_SCC */

#define CFG_NON_AGG_RETRY_MAX                  (31)
#define CFG_AGG_RETRY_MAX                      (31)
#define CFG_MGMT_RETRY_MAX                     (31)
#define CFG_CTRL_RETRY_MAX                     (31)
#define CFG_PROPAGATION_DELAY_MAX              (63)
#define CFG_PROPAGATION_DELAY_BASE             (64)
#define CFG_AGG_RETRY_MIN                      (5)

/**
 * hdd_is_ie_valid() - Determine if an IE sequence is valid
 * @ie: Pointer to the IE buffer
 * @ie_len: Length of the IE buffer @ie
 *
 * This function validates that the IE sequence is valid by verifying
 * that the sum of the lengths of the embedded elements match the
 * length of the sequence.
 *
 * Note well that a 0-length IE sequence is considered valid.
 *
 * Return: true if the IE sequence is valid, false if it is invalid
 */
bool hdd_is_ie_valid(const uint8_t *ie, size_t ie_len);


#define CONNECTIVITY_CHECK_SET_ARP \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_ARP
#define CONNECTIVITY_CHECK_SET_DNS \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_DNS
#define CONNECTIVITY_CHECK_SET_TCP_HANDSHAKE \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_TCP_HANDSHAKE
#define CONNECTIVITY_CHECK_SET_ICMPV4 \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_ICMPV4
#define CONNECTIVITY_CHECK_SET_ICMPV6 \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_ICMPV6
#define CONNECTIVITY_CHECK_SET_TCP_SYN \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_TCP_SYN
#define CONNECTIVITY_CHECK_SET_TCP_SYN_ACK \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_TCP_SYN_ACK
#define CONNECTIVITY_CHECK_SET_TCP_ACK \
	QCA_WLAN_VENDOR_CONNECTIVITY_CHECK_SET_TCP_ACK


struct cfg80211_bss *wlan_hdd_cfg80211_update_bss_db(hdd_adapter_t *pAdapter,
						tCsrRoamInfo *pRoamInfo);

int wlan_hdd_cfg80211_pmksa_candidate_notify(hdd_adapter_t *pAdapter,
					tCsrRoamInfo *pRoamInfo,
					int index, bool preauth);

#ifdef FEATURE_WLAN_LFR_METRICS
QDF_STATUS wlan_hdd_cfg80211_roam_metrics_preauth(hdd_adapter_t *pAdapter,
						tCsrRoamInfo *pRoamInfo);

QDF_STATUS wlan_hdd_cfg80211_roam_metrics_preauth_status(hdd_adapter_t *
							 pAdapter,
							 tCsrRoamInfo *
							 pRoamInfo,
							 bool preauth_status);

QDF_STATUS wlan_hdd_cfg80211_roam_metrics_handover(hdd_adapter_t *pAdapter,
						   tCsrRoamInfo *pRoamInfo);
#endif

hdd_context_t *hdd_cfg80211_wiphy_alloc(int priv_size);

int wlan_hdd_cfg80211_tdls_scan(struct wiphy *wiphy,
				struct cfg80211_scan_request *request,
				uint8_t source);

int wlan_hdd_cfg80211_scan(struct wiphy *wiphy,
			   struct cfg80211_scan_request *request);

int wlan_hdd_cfg80211_init(struct device *dev,
			   struct wiphy *wiphy, struct hdd_config *pCfg);

void wlan_hdd_cfg80211_deinit(struct wiphy *wiphy);

void wlan_hdd_update_wiphy(hdd_context_t *hdd_ctx);

void wlan_hdd_update_11n_mode(struct hdd_config *cfg);

int wlan_hdd_cfg80211_register(struct wiphy *wiphy);
int wlan_hdd_cfg80211_register_frames(hdd_adapter_t *pAdapter);

void wlan_hdd_cfg80211_deregister_frames(hdd_adapter_t *pAdapter);

void hdd_reg_notifier(struct wiphy *wiphy,
				 struct regulatory_request *request);

extern void hdd_conn_set_connection_state(hdd_adapter_t *pAdapter,
					  eConnectionState connState);
QDF_STATUS wlan_hdd_validate_operation_channel(hdd_adapter_t *pAdapter,
					       int channel);
#ifdef FEATURE_WLAN_TDLS
int wlan_hdd_cfg80211_send_tdls_discover_req(struct wiphy *wiphy,
					     struct net_device *dev, u8 *peer);
#endif
#ifdef WLAN_FEATURE_GTK_OFFLOAD
extern void wlan_hdd_cfg80211_update_replay_counter_cb(
						void *callbackContext,
						tpSirGtkOffloadGetInfoRspParams
						pGtkOffloadGetInfoRsp);
#endif
void hdd_select_cbmode(hdd_adapter_t *pAdapter, uint8_t operationChannel,
		       struct ch_params_s *ch_params);

uint8_t *wlan_hdd_cfg80211_get_ie_ptr(const uint8_t *ies_ptr, int length,
				      uint8_t eid);
/**
 * wlan_hdd_is_ap_supports_immediate_power_save() - to find certain vendor APs
 *				which do not support immediate power-save.
 * @ies: beacon IE of the AP which STA is connecting/connected to
 * @length: beacon IE length only
 *
 * This API takes the IE of connected/connecting AP and determines that
 * whether it has specific vendor OUI. If it finds then it will return false to
 * notify that AP doesn't support immediate power-save.
 *
 * Return: true or false based on findings
 */
bool wlan_hdd_is_ap_supports_immediate_power_save(uint8_t *ies, int length);
void wlan_hdd_del_station(hdd_adapter_t *adapter);

#if defined(USE_CFG80211_DEL_STA_V2)
int wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
				  struct net_device *dev,
				  struct station_del_parameters *param);
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0))
int wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
				  struct net_device *dev,
				  const uint8_t *mac);
#else
int wlan_hdd_cfg80211_del_station(struct wiphy *wiphy,
				  struct net_device *dev,
				  uint8_t *mac);
#endif
#endif /* USE_CFG80211_DEL_STA_V2 */


#if  defined(QCA_WIFI_FTM)     && defined(CONFIG_NL80211_TESTMODE)
void wlan_hdd_testmode_rx_event(void *buf, size_t buf_len);
#endif

#if defined(FEATURE_WLAN_CH_AVOID) || defined(FEATURE_WLAN_FORCE_SAP_SCC)
int wlan_hdd_send_avoid_freq_event(hdd_context_t *pHddCtx,
				tHddAvoidFreqList * pAvoidFreqList);
#endif /* FEATURE_WLAN_CH_AVOID || FEATURE_WLAN_FORCE_SAP_SCC */

/**
 * wlan_hdd_send_hang_reason_event() - Send hang reason to the userspace
 * @hdd_ctx: Pointer to hdd context
 * @reason: cds recovery reason
 *
 * Return: 0 on success or failure reason
 */
int wlan_hdd_send_hang_reason_event(hdd_context_t *hdd_ctx, uint32_t reason);
#ifdef FEATURE_WLAN_EXTSCAN
void wlan_hdd_cfg80211_extscan_callback(void *ctx,
					const uint16_t evType, void *pMsg);
#endif /* FEATURE_WLAN_EXTSCAN */

/**
 * wlan_hdd_rso_cmd_status_cb() - HDD callback to read RSO command status
 * @ctx: void pointer to hdd context
 * @rso_status: rso command status
 *
 * This callback function is invoked by firmware to update
 * the RSO(ROAM SCAN OFFLOAD) command status.
 *
 * Return: None
 */
void wlan_hdd_rso_cmd_status_cb(void *ctx, struct rso_cmd_status *rso_status);

/**
 * wlan_hdd_cfg80211_chainrssi_callback - chainrssi callback
 * @hdd_ctx: hdd context
 * @pmsg: pmsg
 * @context: callback context
 *
 * Return: void
 */
void wlan_hdd_cfg80211_chainrssi_callback(void *hdd_ctx, void *pmsg,
					  void *context);

void hdd_rssi_threshold_breached(void *hddctx,
				 struct rssi_breach_event *data);

/*
 * wlan_hdd_cfg80211_unlink_bss :to inform nl80211
 * interface that BSS might have been lost.
 * @pAdapter: adapter
 * @bssid: bssid which might have been lost
 *
 * Return: void
 */
void wlan_hdd_cfg80211_unlink_bss(hdd_adapter_t *pAdapter, tSirMacAddr bssid);

int wlan_hdd_cfg80211_update_bss(struct wiphy *wiphy,
						hdd_adapter_t *pAdapter,
						uint32_t scan_timestamp);

void wlan_hdd_cfg80211_acs_ch_select_evt(hdd_adapter_t *adapter);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
int wlan_hdd_send_roam_auth_event(hdd_adapter_t *adapter, uint8_t *bssid,
		uint8_t *req_rsn_ie, uint32_t req_rsn_length, uint8_t
		*rsp_rsn_ie, uint32_t rsp_rsn_length, tCsrRoamInfo
		*roam_info_ptr);
#else
static inline int wlan_hdd_send_roam_auth_event(hdd_adapter_t *adapter,
		uint8_t *bssid, uint8_t *req_rsn_ie, uint32_t req_rsn_length,
		uint8_t *rsp_rsn_ie, uint32_t rsp_rsn_length, tCsrRoamInfo
		*roam_info_ptr)
{
	return 0;
}
#endif

int wlan_hdd_cfg80211_update_apies(hdd_adapter_t *adapter);

#if !(defined(SUPPORT_WDEV_CFG80211_VENDOR_EVENT_ALLOC)) &&	\
	(LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)) &&	\
	!(defined(WITH_BACKPORTS))

static inline struct sk_buff *
backported_cfg80211_vendor_event_alloc(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					int approxlen,
					int event_idx, gfp_t gfp)
{
	struct sk_buff *skb;

	skb = cfg80211_vendor_event_alloc(wiphy, approxlen, event_idx, gfp);

	if (skb && wdev) {
		struct nlattr *attr;
		u32 ifindex = wdev->netdev->ifindex;

		nla_nest_cancel(skb, ((void **)skb->cb)[2]);
		if (nla_put_u32(skb, NL80211_ATTR_IFINDEX, ifindex))
			goto nla_fail;

		attr = nla_nest_start(skb, NL80211_ATTR_VENDOR_DATA);
		((void **)skb->cb)[2] = attr;
	}

	return skb;

nla_fail:
	kfree_skb(skb);
	return NULL;
}
#define cfg80211_vendor_event_alloc backported_cfg80211_vendor_event_alloc
#endif
int wlan_hdd_request_pre_cac(uint8_t channel);
int wlan_hdd_sap_cfg_dfs_override(hdd_adapter_t *adapter);

enum cds_con_mode wlan_hdd_convert_nl_iftype_to_hdd_type(
					enum nl80211_iftype type);

int wlan_hdd_disable_dfs_chan_scan(hdd_context_t *hdd_ctx,
				   hdd_adapter_t *adapter,
				   uint32_t no_dfs_flag);

int wlan_hdd_cfg80211_update_band(struct wiphy *wiphy,
				  tSirRFBand eBand);

#if defined(CFG80211_DISCONNECTED_V2) || \
(LINUX_VERSION_CODE >= KERNEL_VERSION(4, 2, 0))
static inline void wlan_hdd_cfg80211_indicate_disconnect(struct net_device *dev,
							bool locally_generated,
							int reason)
{
	cfg80211_disconnected(dev, reason, NULL, 0,
				locally_generated, GFP_KERNEL);
}
#else
static inline void wlan_hdd_cfg80211_indicate_disconnect(struct net_device *dev,
							bool locally_generated,
							int reason)
{
	cfg80211_disconnected(dev, reason, NULL, 0,
				GFP_KERNEL);
}
#endif
struct cfg80211_bss *wlan_hdd_cfg80211_inform_bss_frame(hdd_adapter_t *pAdapter,
						tSirBssDescription *bss_desc);

/**
 * hdd_lost_link_info_cb() - callback function to get lost link information
 * @context: HDD context
 * @lost_link_info: lost link information
 *
 * Return: none
 */
void hdd_lost_link_info_cb(void *context,
			struct sir_lost_link_info *lost_link_info);
/*
 * hdd_get_sap_operating_band:  Get current operating channel
 * for sap.
 * @hdd_ctx: hdd context
 *
 * Return : Corresponding band for SAP operating channel
 */
uint8_t hdd_get_sap_operating_band(hdd_context_t *hdd_ctx);

/**
 * hdd_process_defer_disconnect() - Handle the deferred disconnect
 * @adapter: HDD Adapter
 *
 * If roaming is in progress and there is a request to
 * disconnect the session, then it is deferred. Once
 * roaming is complete/aborted, then this routine is
 * used to resume the disconnect that was deferred
 *
 * Return: None
 */
void hdd_process_defer_disconnect(hdd_adapter_t *adapter);

/**
 * wlan_hdd_try_disconnect() - try disconnnect from previous connection
 * @adapter: Pointer to adapter
 *
 * This function is used to disconnect from previous connection
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_try_disconnect(hdd_adapter_t *adapter);

/**
 * wlan_hdd_disconnect() - hdd disconnect api
 * @pAdapter: Pointer to adapter
 * @reason: Disconnect reason code
 *
 * This function is used to issue a disconnect request to SME
 *
 * Return: 0 for success, non-zero for failure
 */
int wlan_hdd_disconnect(hdd_adapter_t *pAdapter, u16 reason);

/**
 * hdd_bt_activity_cb() - callback function to receive bt activity
 * @context: HDD context
 * @bt_activity: specifies the kind of bt activity
 *
 * Return: none
 */
void hdd_bt_activity_cb(void *context, uint32_t bt_activity);

/**
 * hdd_update_cca_info_cb() - stores congestion value in station context
 * @context : HDD context
 * @congestion : congestion
 * @vdev_id : vdev id
 *
 * Return: None
 */
void hdd_update_cca_info_cb(void *context, uint32_t congestion,
			uint32_t vdev_id);

/**
 * wlan_hdd_init_chan_info() - init chan info in hdd context
 * @hdd_ctx: HDD context pointer
 *
 * Return: none
 */
void wlan_hdd_init_chan_info(hdd_context_t *hdd_ctx);

/**
 * wlan_hdd_deinit_chan_info() - deinit chan info in hdd context
 * @hdd_ctx: hdd context pointer
 *
 * Return: none
 */
void wlan_hdd_deinit_chan_info(hdd_context_t *hdd_ctx);

/**
 * wlan_hdd_save_gtk_offload_params() - Save gtk offload parameters in STA
 *                                      context for offload operations.
 * @adapter: Adapter context
 * @kck_ptr: KCK buffer pointer
 * @kek_ptr: KEK buffer pointer
 * @kek_len: KEK length
 * @replay_ctr: Pointer to 64 bit long replay counter
 * @big_endian: true if replay_ctr is in big endian format
 * @ul_flags: Offload flags
 *
 * Return: None
 */
void wlan_hdd_save_gtk_offload_params(hdd_adapter_t *adapter,
					     uint8_t *kck_ptr,
					     uint8_t *kek_ptr,
					     uint32_t kek_len,
					     uint8_t *replay_ctr,
					     bool big_endian,
					     uint32_t ul_flags);
/**
 * wlan_hdd_send_mode_change_event() - API to send hw mode change event to
 * userspace
 *
 * Return : 0 on success and errno on failure
 */
int wlan_hdd_send_mode_change_event(void);

/*
 * wlan_hdd_send_sta_authorized_event() - Function to send station authorized
 * event to user space in case of SAP
 * @pAdapter: Pointer to the adapter
 * @pHddCtx: HDD Context
 * @mac_addr: MAC address of the STA for which the Authorized event needs to
 * be sent
 *
 * This api is used to send station authorized event to user space
 *
 * Return: Returns QDF_STATUS_SUCCESS on success else rturns error value
 */

QDF_STATUS wlan_hdd_send_sta_authorized_event(
					hdd_adapter_t *pAdapter,
					hdd_context_t *pHddCtx,
					const struct qdf_mac_addr *mac_addr);

/**
 * wlan_hdd_restore_channels() - Restore the channels which were cached
 * and disabled in wlan_hdd_disable_channels api.
 * @hdd_ctx: Pointer to the HDD context
 *
 * Return: 0 on success, Error code on failure
 */
int wlan_hdd_restore_channels(hdd_context_t *hdd_ctx);

#endif
