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

/**
 * DOC: wlan_hdd_apf.h
 *
 * Android Packet Filter related API's and definitions
 */

#ifndef __WLAN_HDD_APF_H
#define __WLAN_HDD_APF_H

#include "qdf_nbuf.h"
#include "qdf_types.h"
#include "sir_api.h"
#include "wlan_hdd_main.h"
#include "wmi_unified.h"
#include "wmi_unified_api.h"
#include "wmi_unified_param.h"

#define MAX_APF_MEMORY_LEN	4096

/* APF commands wait times in msec */
#define WLAN_WAIT_TIME_APF_GET_CAPS     1000
#define WLAN_WAIT_TIME_APF_READ_MEM     10000

/**
 * hdd_apf_read_memory_callback - HDD Callback for the APF read memory
 *	operation
 * @context: Hdd context
 * @read_mem_evt: APF read memory event response parameters
 *
 * Return: 0 on success, errno on failure
 */
void
hdd_apf_read_memory_callback(void *context,
			     struct wmi_apf_read_memory_resp_event_params
								*read_mem_evt);

/**
 * hdd_apf_context_init - APF Context initialization operations
 *
 * Return: None
 */
void hdd_apf_context_init(hdd_adapter_t *adapter);

/**
 * hdd_apf_context_destroy - APF Context de-init operations
 *
 * Return: None
 */
void hdd_apf_context_destroy(hdd_adapter_t *adapter);

/**
 * hdd_get_apf_capabilities_cb() - Callback function to get APF capabilities
 * @hdd_context: hdd_context
 * @apf_get_offload: struct for get offload
 *
 * This function receives the response/data from the lower layer and
 * checks to see if the thread is still waiting then post the results to
 * upper layer, if the request has timed out then ignore.
 *
 * Return: None
 */
void hdd_get_apf_capabilities_cb(void *hdd_context,
				 struct sir_apf_get_offload *data);

/**
 * wlan_hdd_cfg80211_apf_offload() - SSR Wrapper to APF Offload
 * @wiphy:    wiphy structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of @data
 *
 * Return: 0 on success; errno on failure
 */

int wlan_hdd_cfg80211_apf_offload(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  const void *data, int data_len);
#endif /* WLAN_HDD_APF_H */
