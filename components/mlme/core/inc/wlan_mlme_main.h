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
 * DOC: declare internal APIs related to the mlme component
 */

#ifndef _WLAN_MLME_MAIN_H_
#define _WLAN_MLME_MAIN_H_

#include <wlan_objmgr_vdev_obj.h>
#include <wmi_unified_param.h>

#define mlme_fatal(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_MLME, params)
#define mlme_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_MLME, params)
#define mlme_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_MLME, params)
#define mlme_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_MLME, params)
#define mlme_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_MLME, params)

/**
 * struct vdev_mlme_obj - VDEV MLME component object
 * @dynamic_cfg: current configuration of nss, chains for vdev.
 * @ini_cfg: Max configuration of nss, chains supported for vdev.
 */
struct vdev_mlme_priv_obj {
	struct mlme_nss_chains dynamic_cfg;
	struct mlme_nss_chains ini_cfg;
};

/**
 * mlme_get_dynamic_vdev_config() - get the vdev dynamic config params
 * @vdev: vdev pointer
 *
 * Return: pointer to the dynamic vdev config structure
 */
struct mlme_nss_chains *mlme_get_dynamic_vdev_config(
					struct wlan_objmgr_vdev *vdev);

/**
 * mlme_get_ini_vdev_config() - get the vdev ini config params
 * @vdev: vdev pointer
 *
 * Return: pointer to the ini vdev config structure
 */
struct mlme_nss_chains *mlme_get_ini_vdev_config(
					struct wlan_objmgr_vdev *vdev);

/**
 * mlme_vdev_object_created_notification(): mlme vdev create handler
 * @vdev: vdev which is going to created by objmgr
 * @arg: argument for vdev create handler
 *
 * Register this api with objmgr to detect vdev is created
 *
 * Return: QDF_STATUS status in case of success else return error
 */

QDF_STATUS
mlme_vdev_object_created_notification(struct wlan_objmgr_vdev *vdev,
				      void *arg);

/**
 * mlme_vdev_object_destroyed_notification(): mlme vdev delete handler
 * @psoc: vdev which is going to delete by objmgr
 * @arg: argument for vdev delete handler
 *
 * Register this api with objmgr to detect vdev is deleted
 *
 * Return: QDF_STATUS status in case of success else return error
 */
QDF_STATUS
mlme_vdev_object_destroyed_notification(struct wlan_objmgr_vdev *vdev,
					void *arg);

#endif
