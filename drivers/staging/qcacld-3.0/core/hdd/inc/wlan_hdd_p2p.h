/*
 * Copyright (c) 2012-2018 The Linux Foundation. All rights reserved.
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

#ifndef __P2P_H
#define __P2P_H

/**
 * DOC: wlan_hdd_p2p.h
 *
 * Linux HDD P2P include file
 */
#define ACTION_FRAME_TX_TIMEOUT 2000
#define WAIT_CANCEL_REM_CHAN    1000
#define WAIT_REM_CHAN_READY     1000
#define WAIT_CHANGE_CHANNEL_FOR_OFFCHANNEL_TX 3000
#define COMPLETE_EVENT_PROPOGATE_TIME 10

#define ACTION_FRAME_DEFAULT_WAIT 500

#define WLAN_HDD_GET_TYPE_FRM_FC(__fc__)         (((__fc__) & 0x0F) >> 2)
#define WLAN_HDD_GET_SUBTYPE_FRM_FC(__fc__)      (((__fc__) & 0xF0) >> 4)
#define WLAN_HDD_80211_FRM_DA_OFFSET             4
#define WLAN_HDD_80211_FRM_SA_OFFSET            10
#define P2P_WILDCARD_SSID_LEN                    7
#define P2P_WILDCARD_SSID                        "DIRECT-"
#define WLAN_HDD_80211_PEER_ADDR_OFFSET (WLAN_HDD_80211_FRM_DA_OFFSET + \
					 MAC_ADDR_LEN)


#define P2P_ROC_DURATION_MULTIPLIER_GO_PRESENT   6
#define P2P_ROC_DURATION_MULTIPLIER_GO_ABSENT    10

#define HDD_P2P_MAX_ROC_DURATION 1500
#define MAX_ROC_REQ_QUEUE_ENTRY 10

#define P2P_POWER_SAVE_TYPE_OPPORTUNISTIC        (1 << 0)
#define P2P_POWER_SAVE_TYPE_PERIODIC_NOA         (1 << 1)
#define P2P_POWER_SAVE_TYPE_SINGLE_NOA           (1 << 2)

#define ACTION_FRAME_RSP_WAIT 500
#define ACTION_FRAME_ACK_WAIT 300

#ifdef WLAN_FEATURE_P2P_DEBUG
enum p2p_connection_status {
	P2P_NOT_ACTIVE,
	P2P_GO_NEG_PROCESS,
	P2P_GO_NEG_COMPLETED,
	P2P_CLIENT_CONNECTING_STATE_1,
	P2P_GO_COMPLETED_STATE,
	P2P_CLIENT_CONNECTED_STATE_1,
	P2P_CLIENT_DISCONNECTED_STATE,
	P2P_CLIENT_CONNECTING_STATE_2,
	P2P_CLIENT_COMPLETED_STATE
};

extern enum p2p_connection_status global_p2p_connection_status;
#endif

struct p2p_app_set_ps {
	uint8_t opp_ps;
	uint32_t ctWindow;
	uint8_t count;
	uint32_t duration;
	uint32_t interval;
	uint32_t single_noa_duration;
	uint8_t psSelection;
};

int wlan_hdd_cfg80211_remain_on_channel(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					struct ieee80211_channel *chan,
					unsigned int duration, u64 *cookie);

int wlan_hdd_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       u64 cookie);

int wlan_hdd_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  u64 cookie);

int hdd_set_p2p_ps(struct net_device *dev, void *msgData);
int hdd_set_p2p_opps(struct net_device *dev, uint8_t *command);
int hdd_set_p2p_noa(struct net_device *dev, uint8_t *command);

void __hdd_indicate_mgmt_frame(hdd_adapter_t *pAdapter,
			     uint32_t nFrameLength, uint8_t *pbFrames,
			     uint8_t frameType, uint32_t rxChan, int8_t rxRssi);

void hdd_remain_chan_ready_handler(hdd_adapter_t *pAdapter,
	uint32_t scan_id);
void hdd_send_action_cnf(hdd_adapter_t *pAdapter, bool actionSendSuccess);
int wlan_hdd_check_remain_on_channel(hdd_adapter_t *pAdapter);
void hdd_send_action_cnf_cb(uint32_t session_id, bool status);
void wlan_hdd_cancel_existing_remain_on_channel(hdd_adapter_t *pAdapter);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
int wlan_hdd_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
		     struct cfg80211_mgmt_tx_params *params, u64 *cookie);
#else
int wlan_hdd_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
		     struct ieee80211_channel *chan, bool offchan,
		     unsigned int wait,
		     const u8 *buf, size_t len, bool no_cck,
		     bool dont_wait_for_ack, u64 *cookie);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
struct wireless_dev *wlan_hdd_add_virtual_intf(struct wiphy *wiphy,
					       const char *name,
					       unsigned char name_assign_type,
					       enum nl80211_iftype type,
					       struct vif_params *params);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)) || defined(WITH_BACKPORTS)
struct wireless_dev *wlan_hdd_add_virtual_intf(struct wiphy *wiphy,
					       const char *name,
					       unsigned char name_assign_type,
					       enum nl80211_iftype type,
					       u32 *flags,
					       struct vif_params *params);
#else
struct wireless_dev *wlan_hdd_add_virtual_intf(struct wiphy *wiphy,
					       const char *name,
					       enum nl80211_iftype type,
					       u32 *flags,
					       struct vif_params *params);

#endif

int wlan_hdd_del_virtual_intf(struct wiphy *wiphy, struct wireless_dev *wdev);
int __wlan_hdd_del_virtual_intf(struct wiphy *wiphy, struct wireless_dev *wdev);


void wlan_hdd_cleanup_remain_on_channel_ctx(hdd_adapter_t *pAdapter);

void wlan_hdd_roc_request_dequeue(struct work_struct *work);

/**
 * hdd_check_random_mac() - Whether addr is present in random_mac array
 * @adapter: Pointer to adapter
 * @mac_addr: random mac address
 *
 * This function is used to check whether given mac addr is present in list of
 * random_mac structures in specified adapter
 *
 * Return: If random addr is present return true else false
 */
bool hdd_check_random_mac(hdd_adapter_t *adapter, uint8_t *random_mac_addr);
#endif /* __P2P_H */
