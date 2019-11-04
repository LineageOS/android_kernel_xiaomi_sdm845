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

#include "qdf_module.h"
#include "qdf_trace.h"
#include "qdf_platform.h"

/**
 * The following callbacks should be defined static to make sure they are
 * initialized to NULL
 */
static qdf_self_recovery_callback           self_recovery_cb;
static qdf_ssr_callback                     ssr_protect_cb;
static qdf_ssr_callback                     ssr_unprotect_cb;
static qdf_is_module_state_transitioning_cb module_state_transitioning_cb;
static qdf_is_fw_down_callback		    is_fw_down_cb;
static qdf_is_recovering_callback           is_recovering_cb;

void qdf_register_fw_down_callback(qdf_is_fw_down_callback is_fw_down)
{
	is_fw_down_cb = is_fw_down;
}

qdf_export_symbol(qdf_register_fw_down_callback);

bool qdf_is_fw_down(void)
{
	if (!is_fw_down_cb) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			"fw down callback is not registered");
			return false;
	}

	return is_fw_down_cb();
}

qdf_export_symbol(qdf_is_fw_down);

void qdf_register_self_recovery_callback(qdf_self_recovery_callback callback)
{
	self_recovery_cb = callback;
}

qdf_export_symbol(qdf_register_self_recovery_callback);

void __qdf_trigger_self_recovery(const char *func, const uint32_t line)
{
	if (self_recovery_cb)
		self_recovery_cb(QDF_REASON_UNSPECIFIED, func, line);
}

qdf_export_symbol(__qdf_trigger_self_recovery);

void qdf_register_ssr_protect_callbacks(qdf_ssr_callback protect,
					qdf_ssr_callback unprotect)
{
	ssr_protect_cb   = protect;
	ssr_unprotect_cb = unprotect;
}

qdf_export_symbol(qdf_register_ssr_protect_callbacks);

void qdf_ssr_protect(const char *caller)
{
	if (ssr_protect_cb)
		ssr_protect_cb(caller);
}

qdf_export_symbol(qdf_ssr_protect);

void qdf_ssr_unprotect(const char *caller)
{
	if (ssr_unprotect_cb)
		ssr_unprotect_cb(caller);
}

qdf_export_symbol(qdf_ssr_unprotect);

void qdf_register_module_state_query_callback(
			qdf_is_module_state_transitioning_cb query)
{
	module_state_transitioning_cb = query;
}

qdf_export_symbol(qdf_register_module_state_query_callback);

bool qdf_is_module_state_transitioning(void)
{
	if (module_state_transitioning_cb)
		return module_state_transitioning_cb();
	return false;
}

qdf_export_symbol(qdf_is_module_state_transitioning);

void qdf_register_recovering_state_query_callback(
			qdf_is_recovering_callback is_recovering)
{
	is_recovering_cb = is_recovering;
}

bool qdf_is_recovering(void)
{
	if (is_recovering_cb)
		return is_recovering_cb();
	return false;
}

qdf_export_symbol(qdf_is_recovering);
