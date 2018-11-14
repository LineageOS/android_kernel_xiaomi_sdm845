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
 * DOC: define internal APIs related to the mlme component
 */
#include "wlan_mlme_main.h"

static struct vdev_mlme_priv_obj *
wlan_vdev_mlme_get_priv_obj(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_priv_obj *vdev_mlme;

	if (!vdev) {
		mlme_err("vdev is NULL");
		return NULL;
	}

	vdev_mlme = wlan_objmgr_vdev_get_comp_private_obj(vdev,
							  WLAN_UMAC_COMP_MLME);
	if (!vdev_mlme) {
		mlme_err(" MLME component object is NULL");
		return NULL;
	}

	return vdev_mlme;
}

struct mlme_nss_chains *mlme_get_dynamic_vdev_config(
				struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_priv_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_priv_obj(vdev);
	if (!vdev_mlme) {
		mlme_err("vdev component object is NULL");
		return NULL;
	}

	return &vdev_mlme->dynamic_cfg;
}

struct mlme_nss_chains *mlme_get_ini_vdev_config(
				struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_priv_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_priv_obj(vdev);
	if (!vdev_mlme) {
		mlme_err("vdev component object is NULL");
		return NULL;
	}

	return &vdev_mlme->ini_cfg;
}

QDF_STATUS
mlme_vdev_object_created_notification(struct wlan_objmgr_vdev *vdev,
				      void *arg)
{
	struct vdev_mlme_priv_obj *vdev_mlme;
	QDF_STATUS status;

	if (!vdev) {
		mlme_err(" VDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_mlme = qdf_mem_malloc(sizeof(*vdev_mlme));
	if (!vdev_mlme) {
		mlme_err(" MLME component object alloc failed");
		return QDF_STATUS_E_NOMEM;
	}

	status = wlan_objmgr_vdev_component_obj_attach(vdev,
						       WLAN_UMAC_COMP_MLME,
						       (void *)vdev_mlme,
						       QDF_STATUS_SUCCESS);

	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("unable to attach vdev priv obj to vdev obj");
		qdf_mem_free(vdev_mlme);
	}

	return status;
}

QDF_STATUS
mlme_vdev_object_destroyed_notification(struct wlan_objmgr_vdev *vdev,
					void *arg)
{
	struct vdev_mlme_priv_obj *vdev_mlme;
	QDF_STATUS status;

	if (!vdev) {
		mlme_err(" VDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_mlme = wlan_objmgr_vdev_get_comp_private_obj(vdev,
							  WLAN_UMAC_COMP_MLME);
	if (!vdev_mlme) {
		mlme_err(" VDEV MLME component object is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	status = wlan_objmgr_vdev_component_obj_detach(vdev,
						       WLAN_UMAC_COMP_MLME,
						       vdev_mlme);

	if (QDF_IS_STATUS_ERROR(status))
		mlme_err("unable to detach vdev priv obj to vdev obj");

	qdf_mem_free(vdev_mlme);

	return status;
}
