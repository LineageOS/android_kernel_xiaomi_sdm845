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

/**
 *
 * Name:  spectral_scan_api.h
 *
 * Description: spectral scan SME APIs
 *
 */

#ifndef __SPECTRAL_SCAN_API_H__
#define __SPECTRAL_SCAN_API_H__

#include "qdf_types.h"
#include "wmi_unified_param.h"

/**
 * sme_start_spectral_scan() - start/stop spectral scan
 * @req: Pointer to spectral scan request
 *
 * This function is used to start or stop a spectral scan. It gets called when
 * HDD receives start spectral scan vendor command from userspace.
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS sme_start_spectral_scan(struct vdev_spectral_enable_params *req);

/**
 * sme_spectral_scan_config() - set config parameters for spectral scan
 * @req: Pointer to spectral scan configration parameters
 *
 * This function is used to config spectral scan parameters. It gets called when
 * HDD receives config spectral scan vendor command from userspace.
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS sme_spectral_scan_config(struct vdev_spectral_configure_params *req);

/**
 * sme_spectral_scan_register_callback() - Register a callback for passing
 *				spectral scan data to user space
 * @hal: HAL handle
 * @cb: callback function
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS sme_spectral_scan_register_callback(tHalHandle hal,
		void (*cb)(void *context, struct spectral_samp_msg *same_msg));
#endif
