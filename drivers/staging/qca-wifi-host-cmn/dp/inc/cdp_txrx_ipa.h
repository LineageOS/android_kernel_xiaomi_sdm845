/*
 * Copyright (c) 2016-2017 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */
/**
 * @file cdp_txrx_ipa.h
 * @brief Define the host data path IP Acceleraor API functions
 */
#ifndef _CDP_TXRX_IPA_H_
#define _CDP_TXRX_IPA_H_


/**
 * ol_txrx_ipa_resources - Resources needed for IPA
 * @ce_sr: copyengine source ring resource
 * @ce_sr_ring_size: copyengine source ring size
 * @ce_reg_paddr: copyengine BAR register physical addrss
 * @tx_comp_ring: tx completion ring resource
 * @tx_num_alloc_buffer: tx number of allocated buffers
 * @rx_rdy_ring: rx ready ring resource
 * @rx_proc_done_idx: rx process done index physical address
 * @rx2_rdy_ring: rx2 ready ring resource
 * @rx2_proc_done_idx: rx2 process done index physical address
 */
struct ol_txrx_ipa_resources {
	qdf_shared_mem_t *ce_sr;
	uint32_t ce_sr_ring_size;
	qdf_dma_addr_t ce_reg_paddr;

	qdf_shared_mem_t *tx_comp_ring;
	uint32_t tx_num_alloc_buffer;

	qdf_shared_mem_t *rx_rdy_ring;
	qdf_shared_mem_t *rx_proc_done_idx;

	qdf_shared_mem_t *rx2_rdy_ring;
	qdf_shared_mem_t *rx2_proc_done_idx;
};

#ifdef IPA_OFFLOAD

void
ol_txrx_ipa_uc_get_resource(ol_txrx_pdev_handle pdev,
		 struct ol_txrx_ipa_resources *ipa_res);

void
ol_txrx_ipa_uc_set_doorbell_paddr(ol_txrx_pdev_handle pdev,
		 qdf_dma_addr_t ipa_tx_uc_doorbell_paddr,
		 qdf_dma_addr_t ipa_rx_uc_doorbell_paddr);

void
ol_txrx_ipa_uc_set_active(ol_txrx_pdev_handle pdev,
		 bool uc_active, bool is_tx);

void ol_txrx_ipa_uc_op_response(ol_txrx_pdev_handle pdev, uint8_t *op_msg);

void ol_txrx_ipa_uc_register_op_cb(ol_txrx_pdev_handle pdev,
		 void (*ipa_uc_op_cb_type)(uint8_t *op_msg,
		 void *osif_ctxt),
		 void *osif_dev);

void ol_txrx_ipa_uc_get_stat(ol_txrx_pdev_handle pdev);

void ol_txrx_ipa_uc_get_share_stats(ol_txrx_pdev_handle pdev,
				   uint8_t reset_stats);

void ol_txrx_ipa_uc_set_quota(ol_txrx_pdev_handle pdev, uint64_t quota_bytes);

qdf_nbuf_t ol_tx_send_ipa_data_frame(void *vdev, qdf_nbuf_t skb);
#else

static inline void
ol_txrx_ipa_uc_set_doorbell_paddr(ol_txrx_pdev_handle pdev,
				  qdf_dma_addr_t ipa_tx_uc_doorbell_paddr,
				  qdf_dma_addr_t ipa_rx_uc_doorbell_paddr)
{
}

static inline void
ol_txrx_ipa_uc_set_active(ol_txrx_pdev_handle pdev,
	bool uc_active, bool is_tx)
{
}

static inline void
ol_txrx_ipa_uc_op_response(ol_txrx_pdev_handle pdev, uint8_t *op_msg)
{
}

static inline void
ol_txrx_ipa_uc_register_op_cb(ol_txrx_pdev_handle pdev,
				   void (*ipa_uc_op_cb_type)(uint8_t *op_msg,
							     void *osif_ctxt),
				   void *osif_dev)
{
}

static inline void ol_txrx_ipa_uc_get_share_stats(ol_txrx_pdev_handle pdev,
						 uint8_t reset_stats)
{
}

static inline void ol_txrx_ipa_uc_set_quota(ol_txrx_pdev_handle pdev,
						 uint64_t quota_bytes)
{
}

static inline void ol_txrx_ipa_uc_get_stat(ol_txrx_pdev_handle pdev)
{
}
#endif /* IPA_OFFLOAD */

#endif /* _CDP_TXRX_IPA_H_ */

