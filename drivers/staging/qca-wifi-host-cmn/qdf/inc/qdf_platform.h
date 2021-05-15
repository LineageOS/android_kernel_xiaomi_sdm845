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
 * DOC: qdf_platform.h
 * This file defines platform API abstractions.
 */

#ifndef _QDF_PLATFORM_H
#define _QDF_PLATFORM_H

/**
 * qdf_self_recovery_callback() - callback for self recovery
 * @psoc: pointer to the posc object
 * @reason: the reason for the recovery request
 * @func: the caller's function name
 * @line: the line number of the callsite
 *
 * Return: none
 */
typedef void (*qdf_self_recovery_callback)(void *psoc,
					   enum qdf_hang_reason reason,
					   const char *func,
					   const uint32_t line);

/**
 * qdf_ssr_callback() - callback for ssr
 *
 * Return: true if fw is down and false if fw is not down
 */
typedef void (*qdf_ssr_callback)(const char *);

/**
 * qdf_is_module_state_transitioning_cb() - callback to check module state
 *
 * Return: true if module is in transition, else false
 */
typedef int (*qdf_is_module_state_transitioning_cb)(void);

/**
 * qdf_is_fw_down_callback() - callback to query if fw is down
 *
 * Return: true if fw is down and false if fw is not down
 */
typedef bool (*qdf_is_fw_down_callback)(void);

/**
 * qdf_register_fw_down_callback() - API to register fw down callback
 * @is_fw_down: callback to query if fw is down or not
 *
 * Return: none
 */
void qdf_register_fw_down_callback(qdf_is_fw_down_callback is_fw_down);

/**
 * qdf_is_fw_down() - API to check if fw is down or not
 *
 * Return: true: if fw is down
 *	   false: if fw is not down
 */
bool qdf_is_fw_down(void);

/**
 * qdf_register_self_recovery_callback() - register self recovery callback
 * @callback:  self recovery callback
 *
 * Return: None
 */
void qdf_register_self_recovery_callback(qdf_self_recovery_callback callback);

/**
 * qdf_trigger_self_recovery () - tirgger self recovery
 *
 * Return: None
 */
#define qdf_trigger_self_recovery(psoc, reason) \
	__qdf_trigger_self_recovery(psoc, reason, __func__, __LINE__)
void __qdf_trigger_self_recovery(void *psoc, enum qdf_hang_reason reason,
				 const char *func, const uint32_t line);

/**
 * qdf_register_ssr_protect_callbacks() - register [un]protect callbacks
 *
 * Return: None
 */
void qdf_register_ssr_protect_callbacks(qdf_ssr_callback protect,
					qdf_ssr_callback unprotect);

/**
 * qdf_ssr_protect() - start SSR protection
 *
 * Return: None
 */
void qdf_ssr_protect(const char *caller);

/**
 * qdf_ssr_unprotect() - remove SSR protection
 *
 * Return: None
 */
void qdf_ssr_unprotect(const char *caller);

/**
 * qdf_register_module_state_query_callback() - register module state query
 *
 * Return: None
 */
void qdf_register_module_state_query_callback(
			qdf_is_module_state_transitioning_cb query);

/**
 * qdf_is_module_state_transitioning() - query module state transition
 *
 * Return: true if in transition else false
 */
bool qdf_is_module_state_transitioning(void);

/**
 * qdf_is_recovering_callback() - callback to get driver recovering in progress
 * or not
 *
 * Return: true if driver is doing recovering else false
 */
typedef bool (*qdf_is_recovering_callback)(void);

/**
 * qdf_register_recovering_state_query_callback() - register recover status
 * query callback
 *
 * Return: none
 */
void qdf_register_recovering_state_query_callback(
	qdf_is_recovering_callback is_recovering);

/**
 * qdf_is_recovering() - get driver recovering in progress status
 * or not
 *
 * Return: true if driver is doing recovering else false
 */
bool qdf_is_recovering(void);
#endif /*_QDF_PLATFORM_H*/
