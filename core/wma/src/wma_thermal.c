/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
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
 *  DOC: wma_thermal.c
 *  This file contains thermal configurations and related functions.
 */

#include "wma.h"
#include "wma_api.h"
#include "cds_api.h"
#include "wmi_unified_api.h"
#include "wmi_unified.h"

/**
 * enum wma_thermal_states - The various thermal states as supported by WLAN
 * @WMA_THERM_STATE_NORMAL - The normal working state
 * @WMA_THERM_STATE_MEDIUM - The intermediate state, WLAN must perform partial
 *                             mitigation
 * @WMA_THERM_STATE_HIGH - The highest state, WLAN must enter forced IMPS
 */
enum wma_thermal_levels {
	WMA_THERM_STATE_NORMAL = 0,
	WMA_THERM_STATE_MEDIUM = 1,
	WMA_THERM_STATE_HIGH = 2
};

/**
 * wma_update_thermal_mitigation_to_fw - update thermal mitigation to fw
 * @wma: wma handle
 * @thermal_level: thermal level
 *
 * This function sends down thermal mitigation params to the fw
 *
 * Returns: QDF_STATUS_SUCCESS for success otherwise failure
 */
QDF_STATUS wma_update_thermal_mitigation_to_fw(uint8_t thermal_state)
{
	struct thermal_mitigation_params therm_data = {0};
	tp_wma_handle wma_handle = cds_get_context(QDF_MODULE_ID_WMA);

	if (!wma_handle) {
		WMA_LOGE("%s : Failed to get wma_handle", __func__);
		return QDF_STATUS_E_INVAL;
	}

	switch (thermal_state) {
	case WMA_THERM_STATE_NORMAL:
		therm_data.enable = false;
		break;
	case WMA_THERM_STATE_MEDIUM:
		therm_data.levelconf[0].dcoffpercent =
			wma_handle->thermal_throt_dc;
		therm_data.enable = true;
		break;
	default:
		WMA_LOGE("Invalid thermal state: %d", thermal_state);
		return QDF_STATUS_E_INVAL;
	}

	therm_data.dc = wma_handle->thermal_sampling_time;
	therm_data.num_thermal_conf = 1;
	WMA_LOGD("Sending therm_throt with params enable:%d dc:%d dc_off:%d",
		 therm_data.enable, therm_data.dc,
		 therm_data.levelconf[0].dcoffpercent);

	return wmi_unified_thermal_mitigation_param_cmd_send(
					wma_handle->wmi_handle, &therm_data);
}

