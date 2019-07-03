/*
 * Copyright (c) 2013-2019 The Linux Foundation. All rights reserved.
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

#if !defined(WLAN_HDD_TX_RX_H)
#define WLAN_HDD_TX_RX_H

/**
 *
 * DOC: wlan_hdd_tx_rx.h
 *
 * Linux HDD Tx/RX APIs
 */

#include <wlan_hdd_includes.h>
#include <cds_api.h>
#include <linux/skbuff.h>
#include "ol_txrx_osif_api.h"
#include "cdp_txrx_flow_ctrl_legacy.h"

#define HDD_ETHERTYPE_802_1_X              0x888E
#define HDD_ETHERTYPE_802_1_X_FRAME_OFFSET 12
#ifdef FEATURE_WLAN_WAPI
#define HDD_ETHERTYPE_WAI                  0x88b4
#define IS_HDD_ETHERTYPE_WAI(_skb) (ntohs(_skb->protocol) == \
					HDD_ETHERTYPE_WAI)
#else
#define IS_HDD_ETHERTYPE_WAI(_skb) (false)
#endif

#define HDD_PSB_CFG_INVALID                   0xFF
#define HDD_PSB_CHANGED                       0xFF
#define SME_QOS_UAPSD_CFG_BK_CHANGED_MASK     0xF1
#define SME_QOS_UAPSD_CFG_BE_CHANGED_MASK     0xF2
#define SME_QOS_UAPSD_CFG_VI_CHANGED_MASK     0xF4
#define SME_QOS_UAPSD_CFG_VO_CHANGED_MASK     0xF8

netdev_tx_t hdd_hard_start_xmit(struct sk_buff *skb, struct net_device *dev);
void hdd_tx_timeout(struct net_device *dev);

QDF_STATUS hdd_init_tx_rx(hdd_adapter_t *pAdapter);
QDF_STATUS hdd_deinit_tx_rx(hdd_adapter_t *pAdapter);
QDF_STATUS hdd_rx_packet_cbk(void *context, qdf_nbuf_t rxBuf);

/**
 * hdd_reset_all_adapters_connectivity_stats() - reset connectivity stats
 * @hdd_ctx: pointer to HDD Station Context
 *
 * Return: None
 */
void hdd_reset_all_adapters_connectivity_stats(hdd_context_t *hdd_ctx);

/**
 * hdd_tx_rx_collect_connectivity_stats_info() - collect connectivity stats
 * @skb: pointer to skb data
 * @adapter: pointer to vdev apdapter
 * @action: action done on pkt.
 * @pkt_type: data pkt type
 *
 * Return: None
 */
void hdd_tx_rx_collect_connectivity_stats_info(struct sk_buff *skb,
		void *adapter, enum connectivity_stats_pkt_status action,
		uint8_t *pkt_type);

/**
 * hdd_rx_ol_init() - Initialize Rx mode(LRO or GRO) method
 * @hdd_ctx: pointer to HDD Station Context
 *
 * Return: 0 on success and non zero on failure.
 */
int hdd_rx_ol_init(hdd_context_t *hdd_ctx);
void hdd_gro_destroy(void);
void ol_deregister_offld_flush_cb(void (*offload_deinit_cb)(void *data));

/**
 * hdd_enable_rx_ol_in_concurrency() - Enable Rx offload
 * @hdd_ctx: hdd context
 *
 * Enable Rx offload if for inactive concurrency is not active
 *
 * Return: none
 */
void hdd_enable_rx_ol_in_concurrency(hdd_context_t *hdd_ctx);

/**
 * hdd_disable_rx_ol_in_concurrency() - Disable Rx offload due to concurrency
 * @hdd_ctx: hdd context
 *
 * Return: none
 */
void hdd_disable_rx_ol_in_concurrency(hdd_context_t *hdd_ctx);

/**
 * hdd_disable_rx_ol_for_low_tput() - Disable Rx offload in low TPUT scenario
 * @hdd_ctx: hdd context
 * @disable: 1 disable, 0 enable
 *
 * Return: none
 */
void hdd_disable_rx_ol_for_low_tput(hdd_context_t *hdd_ctx, bool disable);

#define CFG_LRO_ENABLED		1
#define CFG_GRO_ENABLED		2

#ifdef IPA_OFFLOAD
QDF_STATUS hdd_rx_mul_packet_cbk(void *cds_context,
				 qdf_nbuf_t rx_buf_list, uint8_t staId);
#endif /* IPA_OFFLOAD */

QDF_STATUS hdd_get_peer_sta_id(hdd_station_ctx_t *sta_ctx,
				struct qdf_mac_addr *peer_mac_addr,
				uint8_t *sta_id);

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
void hdd_tx_resume_cb(void *adapter_context, bool tx_resume);

/**
 * hdd_tx_flow_control_is_pause() - Is TX Q paused by flow control
 * @adapter_context: pointer to vdev apdapter
 *
 * Return: true if TX Q is paused by flow control
 */
bool hdd_tx_flow_control_is_pause(void *adapter_context);
void hdd_tx_resume_timer_expired_handler(void *adapter_context);

/**
 * hdd_register_tx_flow_control() - Register TX Flow control
 * @adapter: adapter handle
 * @timer_callback: timer callback
 * @flow_control_fp: txrx flow control
 * @flow_control_is_pause_fp: is txrx paused by flow control
 *
 * Return: none
 */
void hdd_register_tx_flow_control(hdd_adapter_t *adapter,
		qdf_mc_timer_callback_t timer_callback,
		ol_txrx_tx_flow_control_fp flowControl,
		ol_txrx_tx_flow_control_is_pause_fp flow_control_is_pause);
void hdd_deregister_tx_flow_control(hdd_adapter_t *adapter);
void hdd_get_tx_resource(hdd_adapter_t *adapter,
			uint8_t STAId, uint16_t timer_value);

#else
static inline void hdd_tx_resume_cb(void *adapter_context, bool tx_resume)
{
}
static inline bool hdd_tx_flow_control_is_pause(void *adapter_context)
{
	return false;
}
static inline void hdd_tx_resume_timer_expired_handler(void *adapter_context)
{
}
static inline void hdd_register_tx_flow_control(hdd_adapter_t *adapter,
		qdf_mc_timer_callback_t timer_callback,
		ol_txrx_tx_flow_control_fp flowControl,
		ol_txrx_tx_flow_control_is_pause_fp flow_control_is_pause)
{
}
static inline void hdd_deregister_tx_flow_control(hdd_adapter_t *adapter)
{
}
static inline void hdd_get_tx_resource(hdd_adapter_t *adapter,
			uint8_t STAId, uint16_t timer_value)
{
}
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

int hdd_get_peer_idx(hdd_station_ctx_t *sta_ctx, struct qdf_mac_addr *addr);

const char *hdd_reason_type_to_string(enum netif_reason_type reason);
const char *hdd_action_type_to_string(enum netif_action_type action);
void wlan_hdd_netif_queue_control(hdd_adapter_t *adapter,
		enum netif_action_type action, enum netif_reason_type reason);
int hdd_set_mon_rx_cb(struct net_device *dev);

#ifdef WLAN_FEATURE_PKT_CAPTURE
/**
 * hdd_set_mon_mode_cb() - Set pkt capture mode callback
 * @dev:        Pointer to net_device structure
 *
 * Return: 0 on success; non-zero for failure
 */
int hdd_set_mon_mode_cb(struct net_device *dev);

/**
 * hdd_reset_mon_mode_cb() - Reset pkt capture mode callback
 * @void
 *
 * Return: None
 */
void hdd_reset_mon_mode_cb(void);
#else
static inline
int hdd_set_mon_mode_cb(struct net_device *dev)
{
	return -ENOTSUPP;
}

static inline
void hdd_reset_mon_mode_cb(void)
{
}
#endif /* WLAN_FEATURE_PKT_CAPTURE */

void hdd_send_rps_ind(hdd_adapter_t *adapter);
void hdd_send_rps_disable_ind(hdd_adapter_t *adapter);
void wlan_hdd_classify_pkt(struct sk_buff *skb);

#ifdef MSM_PLATFORM
void hdd_reset_tcp_delack(hdd_context_t *hdd_ctx);
#define HDD_MSM_CFG(msm_cfg)	msm_cfg
#else
static inline void hdd_reset_tcp_delack(hdd_context_t *hdd_ctx) {}
#define HDD_MSM_CFG(msm_cfg)	0
#endif

#ifdef FEATURE_WLAN_DIAG_SUPPORT
void hdd_event_eapol_log(struct sk_buff *skb, enum qdf_proto_dir dir);
#else
static inline
void hdd_event_eapol_log(struct sk_buff *skb, enum qdf_proto_dir dir)
{}
#endif

/*
 * As of the 4.7 kernel, net_device->trans_start is removed. Create shims to
 * support compiling against older versions of the kernel.
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0))
static inline void netif_trans_update(struct net_device *dev)
{
	dev->trans_start = jiffies;
}

#define TX_TIMEOUT_TRACE(dev, module_id) QDF_TRACE( \
	module_id, QDF_TRACE_LEVEL_ERROR, \
	"%s: Transmission timeout occurred jiffies %lu trans_start %lu", \
	__func__, jiffies, dev->trans_start)
#else
#define TX_TIMEOUT_TRACE(dev, module_id) QDF_TRACE( \
	module_id, QDF_TRACE_LEVEL_ERROR, \
	"%s: Transmission timeout occurred jiffies %lu", \
	__func__, jiffies)
#endif

static inline void
hdd_skb_fill_gso_size(struct net_device *dev,
					struct sk_buff *skb) {
	if (skb_cloned(skb) && skb_is_nonlinear(skb) &&
		skb_shinfo(skb)->gso_size == 0 &&
		ip_hdr(skb)->protocol == IPPROTO_TCP) {
		skb_shinfo(skb)->gso_size = dev->mtu -
			((skb_transport_header(skb) - skb_network_header(skb))
				+ tcp_hdrlen(skb));
	}
}

#ifdef CONFIG_HL_SUPPORT
static inline QDF_STATUS
hdd_skb_nontso_linearize(struct sk_buff *skb)
{
	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
hdd_skb_nontso_linearize(struct sk_buff *skb)
{
	if (qdf_nbuf_is_nonlinear(skb) && qdf_nbuf_is_tso(skb) == false) {
		if (qdf_unlikely(skb_linearize(skb)))
			return QDF_STATUS_E_NOMEM;
	}
	return QDF_STATUS_SUCCESS;
}
#endif

#endif /* end #if !defined(WLAN_HDD_TX_RX_H) */
