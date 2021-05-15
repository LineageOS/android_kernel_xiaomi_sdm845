/*
 * Copyright (c) 2017-2018 The Linux Foundation. All rights reserved.
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
 * DOC: Implement various api / helper function which shall be used
 * PMO user and target interface.
 */

#include "wlan_pmo_main.h"
#include "wlan_pmo_obj_mgmt_public_struct.h"

static struct wlan_pmo_ctx *gp_pmo_ctx;

QDF_STATUS pmo_allocate_ctx(void)
{
	/* If it is already created, ignore */
	if (gp_pmo_ctx != NULL) {
		pmo_debug("already allocated pmo_ctx");
		return QDF_STATUS_SUCCESS;
	}

	/* allocate offload mgr ctx */
	gp_pmo_ctx = (struct wlan_pmo_ctx *)qdf_mem_malloc(
			sizeof(*gp_pmo_ctx));
	if (!gp_pmo_ctx) {
		pmo_err("unable to allocate pmo_ctx");
		QDF_ASSERT(0);
		return QDF_STATUS_E_NOMEM;
	}
	qdf_spinlock_create(&gp_pmo_ctx->lock);

	return QDF_STATUS_SUCCESS;
}

void pmo_free_ctx(void)
{
	if (!gp_pmo_ctx) {
		pmo_err("pmo ctx is already freed");
		QDF_ASSERT(0);
		return;
	}
	qdf_spinlock_destroy(&gp_pmo_ctx->lock);
	qdf_mem_free(gp_pmo_ctx);
	gp_pmo_ctx = NULL;
}

struct wlan_pmo_ctx *pmo_get_context(void)
{
	return gp_pmo_ctx;
}

bool pmo_is_vdev_in_beaconning_mode(enum QDF_OPMODE vdev_opmode)
{
	switch (vdev_opmode) {
	case QDF_SAP_MODE:
	case QDF_P2P_GO_MODE:
	case QDF_IBSS_MODE:
		return true;
	default:
		return false;
	}
}

QDF_STATUS pmo_get_vdev_bss_peer_mac_addr(struct wlan_objmgr_vdev *vdev,
		struct qdf_mac_addr *bss_peer_mac_address)
{
	struct wlan_objmgr_peer *peer;

	if (!vdev) {
		pmo_err("vdev is null");
		return QDF_STATUS_E_INVAL;
	}

	peer = wlan_vdev_get_bsspeer(vdev);
	if (!peer) {
		pmo_err("peer is null");
		return QDF_STATUS_E_INVAL;
	}

	wlan_peer_obj_lock(peer);
	qdf_mem_copy(bss_peer_mac_address->bytes, wlan_peer_get_macaddr(peer),
		QDF_MAC_ADDR_SIZE);
	wlan_peer_obj_unlock(peer);

	return QDF_STATUS_SUCCESS;
}

bool pmo_core_is_ap_mode_supports_arp_ns(struct wlan_objmgr_psoc *psoc,
	enum QDF_OPMODE vdev_opmode)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	psoc_ctx = pmo_psoc_get_priv(psoc);

	if ((vdev_opmode == QDF_SAP_MODE ||
		vdev_opmode == QDF_P2P_GO_MODE) &&
		!psoc_ctx->psoc_cfg.ap_arpns_support) {
		pmo_debug("ARP/NS Offload is not supported in SAP/P2PGO mode");
		return false;
	}

	return true;
}

bool pmo_core_is_vdev_supports_offload(struct wlan_objmgr_vdev *vdev)
{
	enum QDF_OPMODE opmode;
	bool val;

	opmode = pmo_get_vdev_opmode(vdev);
	pmo_debug("vdev opmode: %d", opmode);
	switch (opmode) {
	case QDF_STA_MODE:
	case QDF_P2P_CLIENT_MODE:
	case QDF_NDI_MODE:
		val = true;
		break;
	default:
		val = false;
		break;
	}

	return val;
}

QDF_STATUS pmo_core_get_psoc_config(struct wlan_objmgr_psoc *psoc,
		struct pmo_psoc_cfg *psoc_cfg)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pmo_enter();
	if (!psoc || !psoc_cfg) {
		pmo_err("%s is null", !psoc ? "psoc":"psoc_cfg");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		qdf_mem_copy(psoc_cfg, &psoc_ctx->psoc_cfg, sizeof(*psoc_cfg));
	}

out:
	pmo_exit();

	return status;
}

QDF_STATUS pmo_core_update_psoc_config(struct wlan_objmgr_psoc *psoc,
		struct pmo_psoc_cfg *psoc_cfg)
{
	struct pmo_psoc_priv_obj *psoc_ctx;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	pmo_enter();
	if (!psoc || !psoc_cfg) {
		pmo_err("%s is null", !psoc ? "psoc":"psoc_cfg");
		status = QDF_STATUS_E_NULL_VALUE;
		goto out;
	}

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		qdf_mem_copy(&psoc_ctx->psoc_cfg, psoc_cfg, sizeof(*psoc_cfg));
	}

out:
	pmo_exit();

	return status;
}

void pmo_psoc_set_caps(struct wlan_objmgr_psoc *psoc,
		       struct pmo_device_caps *caps)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		qdf_mem_copy(&psoc_ctx->caps, caps, sizeof(psoc_ctx->caps));
	}
}

void pmo_core_psoc_set_hif_handle(struct wlan_objmgr_psoc *psoc,
				  void *hif_hdl)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->hif_hdl = hif_hdl;
	}
}

void *pmo_core_psoc_get_hif_handle(struct wlan_objmgr_psoc *psoc)
{
	void *hif_hdl = NULL;
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		hif_hdl = psoc_ctx->hif_hdl;
	}

	return hif_hdl;
}

void pmo_core_psoc_set_txrx_handle(struct wlan_objmgr_psoc *psoc,
				   void *txrx_hdl)
{
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		psoc_ctx->txrx_hdl = txrx_hdl;
	}
}

void *pmo_core_psoc_get_txrx_handle(struct wlan_objmgr_psoc *psoc)
{
	void *txrx_hdl = NULL;
	struct pmo_psoc_priv_obj *psoc_ctx;

	pmo_psoc_with_ctx(psoc, psoc_ctx) {
		txrx_hdl = psoc_ctx->txrx_hdl;
	}

	return txrx_hdl;
}
