/*
 * Copyright (c) 2014-2018 The Linux Foundation. All rights reserved.
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

#if !defined(WLAN_HDD_SOFTAP_TX_RX_H)
#define WLAN_HDD_SOFTAP_TX_RX_H

/**
 * DOC: wlan_hdd_softap_tx_rx.h
 *
 *Linux HDD SOFTAP Tx/RX APIs
 */

#include <wlan_hdd_hostapd.h>
#include <cdp_txrx_peer_ops.h>

netdev_tx_t hdd_softap_hard_start_xmit(struct sk_buff *skb,
				       struct net_device *dev);
void hdd_softap_tx_timeout(struct net_device *dev);
QDF_STATUS hdd_softap_init_tx_rx(hdd_adapter_t *pAdapter);
QDF_STATUS hdd_softap_deinit_tx_rx(hdd_adapter_t *pAdapter);
QDF_STATUS hdd_softap_init_tx_rx_sta(hdd_adapter_t *pAdapter,
				     uint8_t STAId,
				     struct qdf_mac_addr *pmacAddrSTA);
QDF_STATUS hdd_softap_deinit_tx_rx_sta(hdd_adapter_t *pAdapter,
				       uint8_t STAId);
void hdd_softap_notify_tx_compl_cbk(struct sk_buff *skb, void *context);
QDF_STATUS hdd_softap_rx_packet_cbk(void *context, qdf_nbuf_t rxBufChain);
#ifdef IPA_OFFLOAD
QDF_STATUS hdd_softap_rx_mul_packet_cbk(void *cds_context,
					qdf_nbuf_t rx_buf_list, uint8_t staId);
#endif /* IPA_OFFLOAD */

QDF_STATUS hdd_softap_deregister_sta(hdd_adapter_t *pAdapter,
				     uint8_t staId);
QDF_STATUS hdd_softap_register_sta(hdd_adapter_t *pAdapter,
				   bool fAuthRequired,
				   bool fPrivacyBit,
				   uint8_t staId,
				   uint8_t ucastSig,
				   uint8_t bcastSig,
				   struct qdf_mac_addr *pPeerMacAddress,
				   bool fWmmEnabled);
QDF_STATUS hdd_softap_register_bc_sta(hdd_adapter_t *pAdapter,
				      bool fPrivacyBit);
QDF_STATUS hdd_softap_deregister_bc_sta(hdd_adapter_t *pAdapter);
QDF_STATUS hdd_softap_stop_bss(hdd_adapter_t *pHostapdAdapter);
QDF_STATUS hdd_softap_change_sta_state(hdd_adapter_t *pAdapter,
				       struct qdf_mac_addr *pDestMacAddress,
				       enum ol_txrx_peer_state state);
QDF_STATUS hdd_softap_get_sta_id(hdd_adapter_t *pAdapter,
				 struct qdf_mac_addr *pMacAddress,
				 uint8_t *staId);

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
void hdd_softap_tx_resume_timer_expired_handler(void *adapter_context);
void hdd_softap_tx_resume_cb(void *adapter_context, bool tx_resume);
#else
static inline
void hdd_softap_tx_resume_timer_expired_handler(void *adapter_context)
{
}
static inline
void hdd_softap_tx_resume_cb(void *adapter_context, bool tx_resume)
{
}
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

/**
 * hdd_post_dhcp_ind() - Send DHCP START/STOP indication to FW
 * @adapter: pointer to hdd adapter
 * @sta_id: peer station ID
 * @type: WMA message type
 *
 * Return: None
 */
QDF_STATUS hdd_post_dhcp_ind(hdd_adapter_t *adapter,
			     uint8_t sta_id, uint16_t type);

/**
 * hdd_softap_notify_dhcp_ind() - Notify SAP for DHCP indication for tx desc
 * @context: pointer to HDD context
 * @netbuf: pointer to OS packet (sk_buff)
 *
 * Return: None
 */
void hdd_softap_notify_dhcp_ind(void *context,
				struct sk_buff *netbuf);

/**
 * hdd_dhcp_indication() - Send DHCP START/STOP indication to FW
 * @adapter: pointer to hdd adapter
 * @sta_id: peer station ID
 * @skb: pointer to OS packet (sk_buff)
 * @dir: direction
 *
 * Return: true if tx comp is to be notified for skb
 */
bool hdd_dhcp_indication(hdd_adapter_t *adapter,
			 uint8_t sta_id,
			 struct sk_buff *skb,
			 enum qdf_proto_dir dir);

#endif /* end #if !defined(WLAN_HDD_SOFTAP_TX_RX_H) */
