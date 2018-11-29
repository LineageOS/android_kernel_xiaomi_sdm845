/*
 * Copyright (c) 2016-2018 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_nan_datapath.h
 *
 * WLAN Host Device Driver nan datapath API specification
 */
#ifndef __WLAN_HDD_NAN_DATAPATH_H
#define __WLAN_HDD_NAN_DATAPATH_H

struct hdd_context_s;
struct hdd_tgt_cfg;
struct hdd_config;
struct hdd_adapter_s;
struct wireless_dev;

/* NAN Social channels */
#define NAN_SOCIAL_CHANNEL_2_4GHZ 6
#define NAN_SOCIAL_CHANNEL_5GHZ_LOWER_BAND 44
#define NAN_SOCIAL_CHANNEL_5GHZ_UPPER_BAND 149

#define NDP_APP_INFO_LEN 255
#define NDP_PMK_LEN 32
#define NDP_SCID_BUF_LEN 256
#define NDP_NUM_INSTANCE_ID 255

#define NAN_MAX_SERVICE_NAME_LEN 255
#define NAN_PASSPHRASE_MIN_LEN 8
#define NAN_PASSPHRASE_MAX_LEN 63

#define NDP_BROADCAST_STAID           (0)

#define NAN_CH_INFO_MAX_LEN \
	(SIR_NAN_CH_INFO_MAX_CHANNELS * sizeof(uint32_t) * 2)

#ifdef WLAN_FEATURE_NAN_DATAPATH
#define WLAN_HDD_IS_NDI(adapter) ((adapter)->device_mode == QDF_NDI_MODE)

#define WLAN_HDD_IS_NDI_CONNECTED(adapter) ( \
	eConnectionState_NdiConnected ==\
		(adapter)->sessionCtx.station.conn_info.connState)
#else
#define WLAN_HDD_IS_NDI(adapter)	(false)
#define WLAN_HDD_IS_NDI_CONNECTED(adapter) (false)
#endif /* WLAN_FEATURE_NAN_DATAPATH */

/**
 * enum qca_wlan_vendor_attr_ndp_cfg_security - vendor security attribute
 * @QCA_WLAN_VENDOR_ATTR_NDP_SECURITY_ENABLE: Security enabled
 */
enum qca_wlan_vendor_attr_ndp_cfg_security {
	QCA_WLAN_VENDOR_ATTR_NDP_SECURITY_ENABLE = 1,
};

/**
 * enum qca_wlan_vendor_attr_ndp_qos - vendor qos attribute
 * @QCA_WLAN_VENDOR_ATTR_NDP_QOS_CONFIG: NDP QoS configuration
 */
enum qca_wlan_vendor_attr_ndp_qos {
	QCA_WLAN_VENDOR_ATTR_NDP_QOS_CONFIG = 1,
};

/** enum nan_datapath_state - NAN datapath states
 * @NAN_DATA_NDI_CREATING_STATE: NDI create is in progress
 * @NAN_DATA_NDI_CREATED_STATE: NDI successfully crated
 * @NAN_DATA_NDI_DELETING_STATE: NDI delete is in progress
 * @NAN_DATA_NDI_DELETED_STATE: NDI delete is in progress
 * @NAN_DATA_PEER_CREATE_STATE: Peer create is in progress
 * @NAN_DATA_PEER_DELETE_STATE: Peer delete is in progrss
 * @NAN_DATA_CONNECTING_STATE: Data connection in progress
 * @NAN_DATA_CONNECTED_STATE: Data connection successful
 * @NAN_DATA_END_STATE: NDP end is in progress
 * @NAN_DATA_DISCONNECTED_STATE: NDP is in disconnected state
 */
enum nan_datapath_state {
	NAN_DATA_NDI_CREATING_STATE = 0,
	NAN_DATA_NDI_CREATED_STATE = 1,
	NAN_DATA_NDI_DELETING_STATE = 2,
	NAN_DATA_NDI_DELETED_STATE = 3,
	NAN_DATA_PEER_CREATE_STATE = 4,
	NAN_DATA_PEER_DELETE_STATE = 5,
	NAN_DATA_CONNECTING_STATE = 6,
	NAN_DATA_CONNECTED_STATE = 7,
	NAN_DATA_END_STATE = 8,
	NAN_DATA_DISCONNECTED_STATE = 9,
};

/**
 * struct nan_datapath_ctx - context for nan data path
 * @state: Current state of NDP
 * @active_ndp_sessions: active ndp sessions per adapter
 * @active_ndp_peers: number of active ndp peers
 * @ndp_create_transaction_id: transaction id for create req
 * @ndp_delete_transaction_id: transaction id for delete req
 * @ndp_key_installed: NDP security key installed
 * @ndp_enc_key: NDP encryption key info
 * @ndp_debug_state: debug state info
 * @ndi_delete_rsp_reason: reason code for ndi_delete rsp
 * @ndi_delete_rsp_status: status for ndi_delete rsp
 */
struct nan_datapath_ctx {
	enum nan_datapath_state state;
	/* idx in following array should follow conn_info.peerMacAddress */
	uint32_t active_ndp_sessions[MAX_PEERS];
	uint32_t active_ndp_peers;
	uint16_t ndp_create_transaction_id;
	uint16_t ndp_delete_transaction_id;
	bool ndp_key_installed;
	tCsrRoamSetKey ndp_enc_key;
	uint32_t ndp_debug_state;
	uint32_t ndi_delete_rsp_reason;
	uint32_t ndi_delete_rsp_status;
};

#ifdef WLAN_FEATURE_NAN_DATAPATH
void hdd_ndp_print_ini_config(struct hdd_context_s *hdd_ctx);
void hdd_nan_datapath_target_config(struct hdd_context_s *hdd_ctx,
						struct wma_tgt_cfg *cfg);
void hdd_ndp_event_handler(struct hdd_adapter_s *adapter,
	tCsrRoamInfo *roam_info, uint32_t roam_id, eRoamCmdStatus roam_status,
	eCsrRoamResult roam_result);
int wlan_hdd_cfg80211_process_ndp_cmd(struct wiphy *wiphy,
	struct wireless_dev *wdev, const void *data, int data_len);
int hdd_init_nan_data_mode(struct hdd_adapter_s *adapter);
void hdd_ndp_session_end_handler(hdd_adapter_t *adapter);
#else
static inline void hdd_ndp_print_ini_config(struct hdd_context_s *hdd_ctx)
{
}
static inline void hdd_nan_datapath_target_config(struct hdd_context_s *hdd_ctx,
						struct wma_tgt_cfg *cfg)
{
}
static inline void hdd_ndp_event_handler(struct hdd_adapter_s *adapter,
	tCsrRoamInfo *roam_info, uint32_t roam_id, eRoamCmdStatus roam_status,
	eCsrRoamResult roam_result)
{
}
static inline int wlan_hdd_cfg80211_process_ndp_cmd(struct wiphy *wiphy,
		struct wireless_dev *wdev, const void *data, int data_len)
{
	return 0;
}
static inline int hdd_init_nan_data_mode(struct hdd_adapter_s *adapter)
{
	return 0;
}
static inline void hdd_ndp_session_end_handler(hdd_adapter_t *adapter)
{
}
#endif /* WLAN_FEATURE_NAN_DATAPATH */

#endif /* __WLAN_HDD_NAN_DATAPATH_H */
