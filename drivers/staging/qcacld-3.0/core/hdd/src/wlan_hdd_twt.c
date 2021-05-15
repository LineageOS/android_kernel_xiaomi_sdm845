/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
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
 * DOC : wlan_hdd_twt.c
 *
 * WLAN Host Device Driver file for TWT (Target Wake Time) support.
 *
 */

#include "wlan_hdd_twt.h"
#include "wlan_hdd_main.h"
#include "wlan_hdd_cfg.h"
#include "cfg_api.h"
#include "sme_api.h"
#include "wma_twt.h"

void hdd_twt_print_ini_config(struct hdd_context *hdd_ctx)
{
	hdd_debug("Name = [%s] Value = [%d]", CFG_ENABLE_TWT_NAME,
		  hdd_ctx->config->enable_twt);
	hdd_debug("Name = [%s] Value = [%d]", CFG_TWT_CONGESTION_TIMEOUT_NAME,
		  hdd_ctx->config->twt_congestion_timeout);
}

void hdd_update_tgt_twt_cap(struct hdd_context *hdd_ctx,
			    struct wma_tgt_cfg *cfg)
{
	struct wma_tgt_services *services = &cfg->services;

	hdd_debug("TWT: enable_twt=%d, tgt Req=%d, Res=%d",
		  hdd_ctx->config->enable_twt, services->twt_requestor,
		  services->twt_responder);

	sme_cfg_set_int(hdd_ctx->mac_handle, WNI_CFG_TWT_REQUESTOR,
			QDF_MIN(services->twt_requestor,
				hdd_ctx->config->enable_twt));

	sme_cfg_set_int(hdd_ctx->mac_handle, WNI_CFG_TWT_RESPONDER,
			QDF_MIN(services->twt_responder,
				hdd_ctx->config->enable_twt));

	/*
	 * Currently broadcast TWT is not supported
	 */
	sme_cfg_set_int(hdd_ctx->mac_handle, WNI_CFG_BCAST_TWT,
			QDF_MIN(0, hdd_ctx->config->enable_twt));
}

void hdd_send_twt_enable_cmd(struct hdd_context *hdd_ctx)
{
	uint8_t pdev_id = hdd_ctx->pdev->pdev_objmgr.wlan_pdev_id;
	uint32_t req_val = 0, resp_val = 0, bcast_val = 0;
	uint32_t congestion_timeout = hdd_ctx->config->twt_congestion_timeout;

	sme_cfg_get_int(hdd_ctx->mac_handle, WNI_CFG_TWT_REQUESTOR, &req_val);
	sme_cfg_get_int(hdd_ctx->mac_handle, WNI_CFG_TWT_RESPONDER, &resp_val);
	sme_cfg_get_int(hdd_ctx->mac_handle, WNI_CFG_BCAST_TWT, &bcast_val);

	hdd_debug("TWT cfg req:%d, responder:%d, bcast:%d, pdev:%d, cong:%d",
		  req_val, resp_val, bcast_val, pdev_id, congestion_timeout);

	if (req_val || resp_val || bcast_val)
		wma_send_twt_enable_cmd(pdev_id, congestion_timeout);
}

void hdd_twt_enable_comp_cb(void *hddctx,
		struct wmi_twt_enable_complete_event_param *params)
{
	struct hdd_context *hdd_ctx = hddctx;
	enum twt_status prev_state;

	if (!hdd_ctx) {
		hdd_err("TWT: Invalid HDD Context");
		return;
	}
	prev_state = hdd_ctx->twt_state;
	if (params->status == WMI_HOST_ENABLE_TWT_STATUS_OK ||
	    params->status == WMI_HOST_ENABLE_TWT_STATUS_ALREADY_ENABLED) {
		switch (prev_state) {
		case TWT_FW_TRIGGER_ENABLE_REQUESTED:
			hdd_ctx->twt_state = TWT_FW_TRIGGER_ENABLED;
			break;
		case TWT_HOST_TRIGGER_ENABLE_REQUESTED:
			hdd_ctx->twt_state = TWT_HOST_TRIGGER_ENABLED;
			break;
		default:
			break;
		}
	}
	if (params->status == WMI_HOST_ENABLE_TWT_INVALID_PARAM ||
	    params->status == WMI_HOST_ENABLE_TWT_STATUS_UNKNOWN_ERROR)
		hdd_ctx->twt_state = TWT_INIT;

	hdd_debug("TWT: pdev ID:%d, status:%d State transitioned from %d to %d",
		  params->pdev_id, params->status,
		  prev_state, hdd_ctx->twt_state);
}

void hdd_twt_disable_comp_cb(void *hddctx)
{
	struct hdd_context *hdd_ctx = hddctx;
	enum twt_status prev_state;

	if (!hdd_ctx) {
		hdd_err("TWT: Invalid HDD Context");
		return;
	}
	prev_state = hdd_ctx->twt_state;
	hdd_ctx->twt_state = TWT_DISABLED;

	hdd_debug("TWT: State transitioned from %d to %d",
		  prev_state, hdd_ctx->twt_state);
}

void wlan_hdd_twt_init(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;

	hdd_ctx->twt_state = TWT_INIT;
	status = sme_register_twt_enable_complete_cb(hdd_ctx->mac_handle,
						     hdd_twt_enable_comp_cb);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Register twt enable complete failed");
		return;
	}

	status = sme_register_twt_disable_complete_cb(hdd_ctx->mac_handle,
						      hdd_twt_disable_comp_cb);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Register twt disable complete failed");
		goto twt_init_fail;
	}
	hdd_send_twt_enable_cmd(hdd_ctx);
	return;

twt_init_fail:

	sme_deregister_twt_enable_complete_cb(hdd_ctx->mac_handle);
}

void wlan_hdd_twt_deinit(struct hdd_context *hdd_ctx)
{
	QDF_STATUS status;

	status  = sme_deregister_twt_disable_complete_cb(hdd_ctx->mac_handle);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("De-register of twt disable cb failed: %d", status);
	status  = sme_deregister_twt_enable_complete_cb(hdd_ctx->mac_handle);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("De-register of twt enable cb failed: %d", status);

	hdd_ctx->twt_state = TWT_CLOSED;
}
