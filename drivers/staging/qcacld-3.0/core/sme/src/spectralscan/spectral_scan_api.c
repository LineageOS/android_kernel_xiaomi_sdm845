/*
 * Copyright (c) 2017 The Linux Foundation. All rights reserved.
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

#include "sme_api.h"
#include "sir_mac_prop_exts.h"
#include "lim_ft_defs.h"
#include "sme_ft_api.h"
#include "csr_inside_api.h"
#include "sme_inside.h"
#include "wma_types.h"
#include "spectral_scan_api.h"

QDF_STATUS sme_start_spectral_scan(struct vdev_spectral_enable_params *req)
{
	struct vdev_spectral_enable_params *data;
	cds_msg_t msg;

	data = qdf_mem_malloc(sizeof(*data));
	if (data == NULL) {
		sme_err("Memory allocation failure");
		return QDF_STATUS_E_NOMEM;
	}

	*data = *req;

	msg.type = SIR_HAL_SPECTRAL_SCAN_REQUEST;
	msg.reserved = 0;
	msg.bodyptr = data;

	if (QDF_STATUS_SUCCESS !=
			cds_mq_post_message(QDF_MODULE_ID_WMA, &msg)) {
		sme_err("Failed to post WMA_SPECTRAL_SCAN_REQUEST message");
		qdf_mem_free(data);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_spectral_scan_config(struct vdev_spectral_configure_params *req)
{

	struct vdev_spectral_configure_params *data;
	cds_msg_t msg;

	data = qdf_mem_malloc(sizeof(*data));
	if (data == NULL) {
		sme_err("Memory allocation failure");
		return QDF_STATUS_E_NOMEM;
	}

	*data = *req;

	msg.type = SIR_HAL_SPECTRAL_SCAN_CONFIG;
	msg.reserved = 0;
	msg.bodyptr = data;

	if (QDF_STATUS_SUCCESS !=
			cds_mq_post_message(QDF_MODULE_ID_WMA, &msg)) {
		sme_err("Failed to post WMA_SPECTRAL_SCAN_CONFIG message");
		qdf_mem_free(data);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sme_spectral_scan_register_callback(tHalHandle hal,
				void (*cb)(void *, struct spectral_samp_msg *))
{
	QDF_STATUS status;
	tpAniSirGlobal mac = PMAC_STRUCT(hal);

	status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->sme.spectral_scan_cb = cb;
		sme_release_global_lock(&mac->sme);
	} else {
		sme_err("sme_acquire_global_lock error status %d", status);
	}

	return status;
}
