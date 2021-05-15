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
 * DOC: Public APIs for crypto service
 */
#include <qdf_types.h>
#include <wlan_cmn.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>

#include "wlan_crypto_global_def.h"
#include "wlan_crypto_global_api.h"
#include "wlan_crypto_def_i.h"
#include "wlan_crypto_param_handling_i.h"
#include "wlan_crypto_obj_mgr_i.h"

#include <qdf_module.h>


const struct wlan_crypto_cipher *wlan_crypto_cipher_ops[WLAN_CRYPTO_CIPHER_MAX];

/**
 * wlan_crypto_vdev_get_crypto_params - called by mlme to get crypto params
 * @vdev:vdev
 *
 * This function gets called by mlme to get crypto params
 *
 * Return: wlan_crypto_params or NULL in case of failure
 */
static struct wlan_crypto_params *wlan_crypto_vdev_get_comp_params(
				struct wlan_objmgr_vdev *vdev,
				struct wlan_crypto_comp_priv **crypto_priv){
	*crypto_priv = (struct wlan_crypto_comp_priv *)
					wlan_get_vdev_crypto_obj(vdev);
	if (*crypto_priv == NULL) {
		qdf_print("%s[%d] crypto_priv NULL\n", __func__, __LINE__);
		return NULL;
	}

	return &((*crypto_priv)->crypto_params);
}

/**
 * wlan_crypto_peer_get_crypto_params - called by mlme to get crypto params
 * @peer:peer
 *
 * This function gets called by mlme to get crypto params
 *
 * Return: wlan_crypto_params or NULL in case of failure
 */
static struct wlan_crypto_params *wlan_crypto_peer_get_comp_params(
				struct wlan_objmgr_peer *peer,
				struct wlan_crypto_comp_priv **crypto_priv){

	*crypto_priv = (struct wlan_crypto_comp_priv *)
					wlan_get_peer_crypto_obj(peer);
	if (*crypto_priv == NULL) {
		qdf_print("%s[%d] crypto_priv NULL\n", __func__, __LINE__);
		return NULL;
	}

	return &((*crypto_priv)->crypto_params);
}

static QDF_STATUS wlan_crypto_set_igtk_key(struct wlan_crypto_key *key)
{
	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_crypto_set_param - called by ucfg to set crypto param
 * @crypto_params: crypto_params
 * @param: param to be set.
 * @value: value
 *
 * This function gets called from ucfg to set param
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
static QDF_STATUS wlan_crypto_set_param(struct wlan_crypto_params *crypto_params,
					wlan_crypto_param_type param,
					uint32_t value){
	QDF_STATUS status = QDF_STATUS_E_INVAL;

	switch (param) {
	case WLAN_CRYPTO_PARAM_AUTH_MODE:
		status = wlan_crypto_set_authmode(crypto_params, value);
		break;
	case WLAN_CRYPTO_PARAM_UCAST_CIPHER:
		status = wlan_crypto_set_ucastciphers(crypto_params, value);
		break;
	case WLAN_CRYPTO_PARAM_MCAST_CIPHER:
		status = wlan_crypto_set_mcastcipher(crypto_params, value);
		break;
	case WLAN_CRYPTO_PARAM_MGMT_CIPHER:
		status = wlan_crypto_set_mgmtcipher(crypto_params, value);
		break;
	case WLAN_CRYPTO_PARAM_CIPHER_CAP:
		status = wlan_crypto_set_cipher_cap(crypto_params, value);
		break;
	case WLAN_CRYPTO_PARAM_RSN_CAP:
		status = wlan_crypto_set_rsn_cap(crypto_params,	value);
		break;
	case WLAN_CRYPTO_PARAM_KEY_MGMT:
		status = wlan_crypto_set_key_mgmt(crypto_params, value);
		break;
	default:
		status = QDF_STATUS_E_INVAL;
	}
	return status;
}

/**
 * wlan_crypto_set_vdev_param - called by ucfg to set crypto param
 * @vdev: vdev
 * @param: param to be set.
 * @value: value
 *
 * This function gets called from ucfg to set param
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_crypto_set_vdev_param(struct wlan_objmgr_vdev *vdev,
					wlan_crypto_param_type param,
					uint32_t value){
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;

	crypto_priv = (struct wlan_crypto_comp_priv *)
					wlan_get_vdev_crypto_obj(vdev);

	if (crypto_priv == NULL) {
		qdf_print("%s[%d] crypto_priv NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}

	crypto_params = &(crypto_priv->crypto_params);

	status = wlan_crypto_set_param(crypto_params, param, value);

	return status;
}

/**
 * wlan_crypto_set_param - called by ucfg to set crypto param
 *
 * @peer: peer
 * @param: param to be set.
 * @value: value
 *
 * This function gets called from ucfg to set param
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_crypto_set_peer_param(struct wlan_objmgr_peer *peer,
				wlan_crypto_param_type param,
				uint32_t value){
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;

	crypto_params = wlan_crypto_peer_get_comp_params(peer,
							&crypto_priv);

	if (crypto_priv == NULL) {
		qdf_print("%s[%d] crypto_priv NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}

	crypto_params = &(crypto_priv->crypto_params);

	status = wlan_crypto_set_param(crypto_params, param, value);

	return status;
}

/**
 * wlan_crypto_get_param_value - called by crypto APIs to get value for param
 * @param: Crypto param type
 * @crypto_params: Crypto params struct
 *
 * This function gets called from in-within crypto layer
 *
 * Return: value or -1 for failure
 */
static int32_t wlan_crypto_get_param_value(wlan_crypto_param_type param,
				struct wlan_crypto_params *crypto_params)
{
	int32_t value = -1;

	switch (param) {
	case WLAN_CRYPTO_PARAM_AUTH_MODE:
		value = wlan_crypto_get_authmode(crypto_params);
		break;
	case WLAN_CRYPTO_PARAM_UCAST_CIPHER:
		value = wlan_crypto_get_ucastciphers(crypto_params);
		break;
	case WLAN_CRYPTO_PARAM_MCAST_CIPHER:
		value = wlan_crypto_get_mcastcipher(crypto_params);
		break;
	case WLAN_CRYPTO_PARAM_MGMT_CIPHER:
		value = wlan_crypto_get_mgmtciphers(crypto_params);
		break;
	case WLAN_CRYPTO_PARAM_CIPHER_CAP:
		value = wlan_crypto_get_cipher_cap(crypto_params);
		break;
	case WLAN_CRYPTO_PARAM_RSN_CAP:
		value = wlan_crypto_get_rsn_cap(crypto_params);
		break;
	case WLAN_CRYPTO_PARAM_KEY_MGMT:
		value = wlan_crypto_get_key_mgmt(crypto_params);
		break;
	default:
		value = QDF_STATUS_E_INVAL;
	}

	return value;
}

/**
 * wlan_crypto_get_param - called to get value for param from vdev
 * @vdev:  vdev
 * @param: Crypto param type
 *
 * This function gets called to get value for param from vdev
 *
 * Return: value or -1 for failure
 */
int32_t wlan_crypto_get_param(struct wlan_objmgr_vdev *vdev,
			      wlan_crypto_param_type param)
{
	int32_t value = -1;
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;
	crypto_priv = (struct wlan_crypto_comp_priv *)
				wlan_get_vdev_crypto_obj(vdev);

	if (crypto_priv == NULL) {
		qdf_print("%s[%d] crypto_priv NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}

	crypto_params = &(crypto_priv->crypto_params);
	value = wlan_crypto_get_param_value(param, crypto_params);

	return value;
}
/**
 * wlan_crypto_get_peer_param - called to get value for param from peer
 * @peer:  peer
 * @param: Crypto param type
 *
 * This function gets called to get value for param from peer
 *
 * Return: value or -1 for failure
 */
int32_t wlan_crypto_get_peer_param(struct wlan_objmgr_peer *peer,
				   wlan_crypto_param_type param)
{
	int32_t value = -1;
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;

	crypto_params = wlan_crypto_peer_get_comp_params(peer,
							&crypto_priv);

	if (crypto_params == NULL) {
		qdf_print("%s[%d] crypto_params NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}
	value = wlan_crypto_get_param_value(param, crypto_params);

	return value;
}
qdf_export_symbol(wlan_crypto_get_peer_param);
/**
 * wlan_crypto_is_htallowed - called to check is HT allowed for cipher
 * @vdev:  vdev
 * @peer:  peer
 *
 * This function gets called to check is HT allowed for cipher.
 * HT is not allowed for wep and tkip.
 *
 * Return: 0 - not allowed or 1 - allowed
 */
uint8_t wlan_crypto_is_htallowed(struct wlan_objmgr_vdev *vdev,
				 struct wlan_objmgr_peer *peer)
{
	int32_t ucast_cipher;

	if (!(vdev || peer)) {
		qdf_print("%s[%d] Invalid params\n", __func__, __LINE__);
		return 0;
	}

	if (vdev)
		ucast_cipher = wlan_crypto_get_param(vdev,
				WLAN_CRYPTO_PARAM_UCAST_CIPHER);
	else
		ucast_cipher = wlan_crypto_get_peer_param(peer,
				WLAN_CRYPTO_PARAM_UCAST_CIPHER);

	return (ucast_cipher & (1 << WLAN_CRYPTO_CIPHER_WEP)) ||
		((ucast_cipher & (1 << WLAN_CRYPTO_CIPHER_TKIP)) &&
		!(ucast_cipher & (1 << WLAN_CRYPTO_CIPHER_AES_CCM)) &&
		!(ucast_cipher & (1 << WLAN_CRYPTO_CIPHER_AES_GCM)) &&
		!(ucast_cipher & (1 << WLAN_CRYPTO_CIPHER_AES_GCM_256)) &&
		!(ucast_cipher & (1 << WLAN_CRYPTO_CIPHER_AES_CCM_256)));
}
qdf_export_symbol(wlan_crypto_is_htallowed);

/**
 * wlan_crypto_setkey - called by ucfg to setkey
 * @vdev: vdev
 * @req_key: req_key with cipher type, key macaddress
 *
 * This function gets called from ucfg to sey key
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_crypto_setkey(struct wlan_objmgr_vdev *vdev,
				struct wlan_crypto_req_key *req_key){

	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_peer *peer;
	struct wlan_crypto_key *key = NULL;
	const struct wlan_crypto_cipher *cipher;
	uint8_t macaddr[WLAN_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	bool isbcast;
	enum QDF_OPMODE vdev_mode;
	uint8_t igtk_idx = 0;

	if (!vdev || !req_key || req_key->keylen > (sizeof(req_key->keydata))) {
		qdf_print("%s[%d] Invalid params vdev%pK, req_key%pK\n",
				__func__, __LINE__, vdev, req_key);
		return QDF_STATUS_E_INVAL;
	}

	isbcast = qdf_is_macaddr_group(
				(struct qdf_mac_addr *)req_key->macaddr);
	if ((req_key->keylen == 0) && !IS_FILS_CIPHER(req_key->type)) {
		/* zero length keys, only set default key id if flags are set*/
		if ((req_key->flags & WLAN_CRYPTO_KEY_DEFAULT)
			&& (req_key->keyix != WLAN_CRYPTO_KEYIX_NONE)
			&& (!IS_MGMT_CIPHER(req_key->type))) {
			wlan_crypto_default_key(vdev,
				req_key->macaddr,
				req_key->keyix,
				!isbcast);
			return QDF_STATUS_SUCCESS;
		}
		qdf_print("%s[%d] req_key len zero\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}

	cipher = wlan_crypto_cipher_ops[req_key->type];

	if (!cipher && !IS_MGMT_CIPHER(req_key->type)) {
		qdf_print("%s[%d] cipher invalid\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}

	if (cipher && (!IS_FILS_CIPHER(req_key->type)) &&
	    (!IS_MGMT_CIPHER(req_key->type)) &&
	    ((req_key->keylen != (cipher->keylen / NBBY)) &&
	    (req_key->type != WLAN_CRYPTO_CIPHER_WEP))) {
		qdf_print("%s[%d] cipher invalid\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	} else if ((req_key->type == WLAN_CRYPTO_CIPHER_WEP) &&
		!((req_key->keylen == WLAN_CRYPTO_KEY_WEP40_LEN)
		|| (req_key->keylen == WLAN_CRYPTO_KEY_WEP104_LEN)
		|| (req_key->keylen == WLAN_CRYPTO_KEY_WEP128_LEN))) {
		qdf_print("%s[%d] wep key len invalid\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}

	if (req_key->keyix == WLAN_CRYPTO_KEYIX_NONE) {
		if (req_key->flags != (WLAN_CRYPTO_KEY_XMIT
						| WLAN_CRYPTO_KEY_RECV)) {
			req_key->flags |= (WLAN_CRYPTO_KEY_XMIT
						| WLAN_CRYPTO_KEY_RECV);
		}
	} else {
		if ((req_key->keyix >= WLAN_CRYPTO_MAXKEYIDX)
			&& (!IS_MGMT_CIPHER(req_key->type))) {
			return QDF_STATUS_E_INVAL;
		}

		req_key->flags |= (WLAN_CRYPTO_KEY_XMIT
					| WLAN_CRYPTO_KEY_RECV);
		if (isbcast)
			req_key->flags |= WLAN_CRYPTO_KEY_GROUP;
	}

	vdev_mode = wlan_vdev_mlme_get_opmode(vdev);

	wlan_vdev_obj_lock(vdev);
	qdf_mem_copy(macaddr, wlan_vdev_mlme_get_macaddr(vdev), WLAN_ALEN);
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		wlan_vdev_obj_unlock(vdev);
		qdf_print("%s[%d] psoc NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}
	wlan_vdev_obj_unlock(vdev);

	if (req_key->type == WLAN_CRYPTO_CIPHER_WEP) {
		if (wlan_crypto_vdev_has_auth_mode(vdev,
					(1 << WLAN_CRYPTO_AUTH_8021X))) {
			req_key->flags |= WLAN_CRYPTO_KEY_DEFAULT;
		}
	}

	if (isbcast) {
		crypto_params = wlan_crypto_vdev_get_comp_params(vdev,
								&crypto_priv);
		if (crypto_priv == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}

		if (IS_MGMT_CIPHER(req_key->type)) {
			igtk_idx = req_key->keyix - WLAN_CRYPTO_MAXKEYIDX;
			if (igtk_idx >= WLAN_CRYPTO_MAXIGTKKEYIDX) {
				qdf_print("%s[%d] igtk key invalid keyid %d \n",
						  __func__, __LINE__, igtk_idx);
				return QDF_STATUS_E_INVAL;
			}
			key = qdf_mem_malloc(sizeof(struct wlan_crypto_key));
			if (key == NULL) {
				qdf_print("%s[%d] igtk key alloc failed\n",
						__func__, __LINE__);
				return QDF_STATUS_E_NOMEM;
			}

			if (crypto_priv->igtk_key[igtk_idx])
				qdf_mem_free(crypto_priv->igtk_key[igtk_idx]);

			crypto_priv->igtk_key[igtk_idx] = key;
			crypto_priv->igtk_key_type = req_key->type;
			crypto_priv->def_igtk_tx_keyid = igtk_idx;
		} else {
			if (IS_FILS_CIPHER(req_key->type)) {
				qdf_print(FL(
				"FILS key is not for BroadCast packet\n"));
				return QDF_STATUS_E_INVAL;
			}
			if (!HAS_MCAST_CIPHER(crypto_params, req_key->type)
				&& (req_key->type != WLAN_CRYPTO_CIPHER_WEP)) {
				return QDF_STATUS_E_INVAL;
			}
			if (!crypto_priv->key[req_key->keyix]) {
				crypto_priv->key[req_key->keyix]
					= qdf_mem_malloc(
						sizeof(struct wlan_crypto_key));
				if (!crypto_priv->key[req_key->keyix])
					return QDF_STATUS_E_NOMEM;
			}
			key = crypto_priv->key[req_key->keyix];
		}
		if (vdev_mode == QDF_STA_MODE) {
			peer = wlan_vdev_get_bsspeer(vdev);
			if (!(peer && (QDF_STATUS_SUCCESS
				== wlan_objmgr_peer_try_get_ref(peer,
							WLAN_CRYPTO_ID)))) {
				qdf_print("%s[%d] peer %pK failed\n",
						__func__, __LINE__, peer);
				if (IS_MGMT_CIPHER(req_key->type)) {
					crypto_priv->igtk_key[igtk_idx] = NULL;
					crypto_priv->igtk_key_type
						= WLAN_CRYPTO_CIPHER_NONE;
				} else
					crypto_priv->key[req_key->keyix] = NULL;
				if (key)
					qdf_mem_free(key);
				return QDF_STATUS_E_INVAL;
			}
			qdf_mem_copy(macaddr, wlan_peer_get_macaddr(peer),
						WLAN_ALEN);
			wlan_objmgr_peer_release_ref(peer, WLAN_CRYPTO_ID);
		}
	} else {
		uint8_t pdev_id;

		pdev_id = wlan_objmgr_pdev_get_pdev_id(
				wlan_vdev_get_pdev(vdev));
		peer = wlan_objmgr_get_peer_by_mac_n_vdev(
					psoc,
					pdev_id,
					macaddr,
					req_key->macaddr,
					WLAN_CRYPTO_ID);

		if (peer == NULL) {
			qdf_print("%s[%d] peer NULL\n", __func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}

		qdf_mem_copy(macaddr, req_key->macaddr, WLAN_ALEN);
		crypto_params = wlan_crypto_peer_get_comp_params(peer,
								&crypto_priv);
		wlan_objmgr_peer_release_ref(peer, WLAN_CRYPTO_ID);

		if (crypto_priv == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}
		if (IS_MGMT_CIPHER(req_key->type)) {
			igtk_idx = req_key->keyix - WLAN_CRYPTO_MAXKEYIDX;
			if (igtk_idx >= WLAN_CRYPTO_MAXIGTKKEYIDX) {
				qdf_print("%s[%d] igtk key invalid keyid %d \n",
						  __func__, __LINE__, igtk_idx);
				return QDF_STATUS_E_INVAL;
			}
			key = qdf_mem_malloc(sizeof(struct wlan_crypto_key));
			if (key == NULL) {
				qdf_print("%s[%d] igtk key alloc failed\n",
						__func__, __LINE__);
				return QDF_STATUS_E_NOMEM;
			}
			if (crypto_priv->igtk_key[igtk_idx])
				qdf_mem_free(crypto_priv->igtk_key[igtk_idx]);

			crypto_priv->igtk_key[igtk_idx] = key;
			crypto_priv->igtk_key_type = req_key->type;
			crypto_priv->def_igtk_tx_keyid = igtk_idx;
		} else {
			uint16_t kid = req_key->keyix;
			if (kid == WLAN_CRYPTO_KEYIX_NONE)
				kid = 0;
			if (kid >= WLAN_CRYPTO_MAXKEYIDX) {
				qdf_print("%s[%d] invalid keyid %d \n",
						  __func__, __LINE__, kid);
				return QDF_STATUS_E_INVAL;
			}
			if (!crypto_priv->key[kid]) {
				crypto_priv->key[kid]
					= qdf_mem_malloc(
						sizeof(struct wlan_crypto_key));
				if (!crypto_priv->key[kid])
					return QDF_STATUS_E_NOMEM;
			}
			key = crypto_priv->key[kid];
		}
	}

	/* alloc key might not required as it is already there */
	key->cipher_table = (void *)cipher;
	key->keylen = req_key->keylen;
	key->flags = req_key->flags;

	if (req_key->keyix == WLAN_CRYPTO_KEYIX_NONE)
		key->keyix = 0;
	else
		key->keyix = req_key->keyix;

	if (req_key->flags & WLAN_CRYPTO_KEY_DEFAULT
		&& (!IS_MGMT_CIPHER(req_key->type)))  {
		crypto_priv->def_tx_keyid = key->keyix;
		key->flags |= WLAN_CRYPTO_KEY_DEFAULT;
	}
	if ((req_key->type == WLAN_CRYPTO_CIPHER_WAPI_SMS4)
		|| (req_key->type == WLAN_CRYPTO_CIPHER_WAPI_GCM4)) {
		uint8_t iv_AP[16] = {	0x5c, 0x36, 0x5c, 0x36,
					0x5c, 0x36, 0x5c, 0x36,
					0x5c, 0x36, 0x5c, 0x36,
					0x5c, 0x36, 0x5c, 0x37};
		uint8_t iv_STA[16] = {	0x5c, 0x36, 0x5c, 0x36,
					0x5c, 0x36, 0x5c, 0x36,
					0x5c, 0x36, 0x5c, 0x36,
					0x5c, 0x36, 0x5c, 0x36};

		/* During Tx PN should be increment and
		 * send but as per our implementation we increment only after
		 * Tx complete. So First packet PN check will be failed.
		 * To compensate increment the PN here by 2
		 */
		if (vdev_mode == QDF_SAP_MODE) {
			iv_AP[15] += 2;
			qdf_mem_copy(key->recviv, iv_STA,
						WLAN_CRYPTO_WAPI_IV_SIZE);
			qdf_mem_copy(key->txiv, iv_AP,
						WLAN_CRYPTO_WAPI_IV_SIZE);
		} else {
			iv_STA[15] += 2;
			qdf_mem_copy(key->recviv, iv_AP,
						WLAN_CRYPTO_WAPI_IV_SIZE);
			qdf_mem_copy(key->txiv, iv_STA,
						WLAN_CRYPTO_WAPI_IV_SIZE);
		}
	} else {
		uint8_t i = 0;
		qdf_mem_copy((uint8_t *)(&key->keytsc),
			(uint8_t *)(&req_key->keytsc), sizeof(key->keytsc));
		for (i = 0; i < WLAN_CRYPTO_TID_SIZE; i++) {
			qdf_mem_copy((uint8_t *)(&key->keyrsc[i]),
					(uint8_t *)(&req_key->keyrsc),
					sizeof(key->keyrsc[0]));
		}
	}

	qdf_mem_copy(key->keyval, req_key->keydata, sizeof(key->keyval));
	key->valid = 1;
	if ((IS_MGMT_CIPHER(req_key->type))) {
		if (HAS_CIPHER_CAP(crypto_params,
					WLAN_CRYPTO_CAP_PMF_OFFLOAD)) {
			if (WLAN_CRYPTO_TX_OPS_SETKEY(psoc)) {
				WLAN_CRYPTO_TX_OPS_SETKEY(psoc)(vdev,
						key, macaddr, req_key->type);
			}
		}
		wlan_crypto_set_mgmtcipher(crypto_params, req_key->type);
		status = wlan_crypto_set_igtk_key(key);
		return status;
	} else if (IS_FILS_CIPHER(req_key->type)) {
		/* Take request key object to FILS setkey */
		key->private = req_key;
	} else {
		if (WLAN_CRYPTO_TX_OPS_SETKEY(psoc)) {
			WLAN_CRYPTO_TX_OPS_SETKEY(psoc)(vdev, key,
							macaddr, req_key->type);
		}
	}
	status = cipher->setkey(key);

	if ((req_key->flags & WLAN_CRYPTO_KEY_DEFAULT) &&
	    (req_key->keyix != WLAN_CRYPTO_KEYIX_NONE) &&
	    (!IS_MGMT_CIPHER(req_key->type))) {
		/* default xmit key */
		wlan_crypto_default_key(vdev,
					req_key->macaddr,
					req_key->keyix,
					!isbcast);
		}

	return status;
}

/**
 * wlan_crypto_get_keytype - get keytype
 * @key: key
 *
 * This function gets keytype from key
 *
 * Return: keytype
 */
wlan_crypto_cipher_type wlan_crypto_get_key_type(
						struct wlan_crypto_key *key){
	if (key && key->cipher_table) {
		return ((struct wlan_crypto_cipher *)
						(key->cipher_table))->cipher;
	}
	return WLAN_CRYPTO_CIPHER_NONE;
}
qdf_export_symbol(wlan_crypto_get_key_type);
/**
 * wlan_crypto_vdev_getkey - get key from vdev
 * @vdev: vdev
 * @keyix: keyix
 *
 * This function gets key from vdev
 *
 * Return: key or NULL
 */
struct wlan_crypto_key *wlan_crypto_vdev_getkey(struct wlan_objmgr_vdev *vdev,
						uint16_t keyix){
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;
	struct wlan_crypto_key *key = NULL;

	crypto_params = wlan_crypto_vdev_get_comp_params(vdev, &crypto_priv);

	if (crypto_priv == NULL) {
		qdf_print("%s[%d] crypto_priv NULL\n", __func__, __LINE__);
		return NULL;
	}

	if (keyix == WLAN_CRYPTO_KEYIX_NONE || keyix >= WLAN_CRYPTO_MAXKEYIDX)
		key = crypto_priv->key[crypto_priv->def_tx_keyid];
	else
		key = crypto_priv->key[keyix];

	if (key && key->valid)
		return key;

	return NULL;
}
qdf_export_symbol(wlan_crypto_vdev_getkey);

/**
 * wlan_crypto_peer_getkey - get key from peer
 * @peer: peer
 * @keyix: keyix
 *
 * This function gets key from peer
 *
 * Return: key or NULL
 */
struct wlan_crypto_key *wlan_crypto_peer_getkey(struct wlan_objmgr_peer *peer,
						uint16_t keyix){
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;
	struct wlan_crypto_key *key = NULL;

	crypto_params = wlan_crypto_peer_get_comp_params(peer, &crypto_priv);

	if (crypto_priv == NULL) {
		qdf_print("%s[%d] crypto_priv NULL\n", __func__, __LINE__);
		return NULL;
	}

	if (keyix == WLAN_CRYPTO_KEYIX_NONE || keyix >= WLAN_CRYPTO_MAXKEYIDX)
		key = crypto_priv->key[crypto_priv->def_tx_keyid];
	else
		key = crypto_priv->key[keyix];

	if (key && key->valid)
		return key;

	return NULL;
}
qdf_export_symbol(wlan_crypto_peer_getkey);

/**
 * wlan_crypto_getkey - called by ucfg to get key
 * @vdev: vdev
 * @req_key: key value will be copied in this req_key
 * @mac_address: mac address of the peer for unicast key
 *			       or broadcast address if group key is requested.
 *
 * This function gets called from ucfg to get key
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_crypto_getkey(struct wlan_objmgr_vdev *vdev,
				struct wlan_crypto_req_key *req_key,
				uint8_t *mac_addr){
	struct wlan_crypto_cipher *cipher_table;
	struct wlan_crypto_key *key;
	struct wlan_objmgr_psoc *psoc;
	uint8_t macaddr[WLAN_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	if ((req_key->keyix != WLAN_CRYPTO_KEYIX_NONE) &&
		(req_key->keyix >= WLAN_CRYPTO_MAXKEYIDX)) {
		qdf_print("%s[%d] invalid keyix %d\n", __func__, __LINE__,
							req_key->keyix);
		return QDF_STATUS_E_INVAL;
	}

	wlan_vdev_obj_lock(vdev);
	qdf_mem_copy(macaddr, wlan_vdev_mlme_get_macaddr(vdev), WLAN_ALEN);
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		wlan_vdev_obj_unlock(vdev);
		qdf_print("%s[%d] psoc NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}
	wlan_vdev_obj_unlock(vdev);

	if (qdf_is_macaddr_broadcast((struct qdf_mac_addr *)mac_addr)) {
		key = wlan_crypto_vdev_getkey(vdev, req_key->keyix);
		if (!key)
			return QDF_STATUS_E_INVAL;
	} else {
		struct wlan_objmgr_peer *peer;
		uint8_t pdev_id;

		pdev_id = wlan_objmgr_pdev_get_pdev_id(
				wlan_vdev_get_pdev(vdev));
		peer = wlan_objmgr_get_peer_by_mac_n_vdev(
					psoc,
					pdev_id,
					macaddr,
					mac_addr,
					WLAN_CRYPTO_ID);
		if (peer == NULL) {
			QDF_TRACE(QDF_MODULE_ID_CRYPTO, QDF_TRACE_LEVEL_ERROR,
				"%s[%d] peer NULL\n", __func__, __LINE__);
			return QDF_STATUS_E_NOENT;
		}
		key = wlan_crypto_peer_getkey(peer, req_key->keyix);
		wlan_objmgr_peer_release_ref(peer, WLAN_CRYPTO_ID);
		if (!key)
			return QDF_STATUS_E_INVAL;
	}

	if (key->valid) {
		qdf_mem_copy(req_key->keydata,
				key->keyval, key->keylen);
		qdf_mem_copy((uint8_t *)(&req_key->keytsc),
				(uint8_t *)(&key->keytsc),
				sizeof(req_key->keytsc));
		qdf_mem_copy((uint8_t *)(&req_key->keyrsc),
				(uint8_t *)(&key->keyrsc[0]),
				sizeof(req_key->keyrsc));
		req_key->keylen = key->keylen;
		req_key->flags = key->flags;
		cipher_table = (struct wlan_crypto_cipher *)key->cipher_table;

		if (!cipher_table)
			return QDF_STATUS_SUCCESS;

		req_key->type = cipher_table->cipher;
		if (req_key->type == WLAN_CRYPTO_CIPHER_WAPI_SMS4) {
			qdf_mem_copy((uint8_t *)(&req_key->txiv),
					(uint8_t *)(key->txiv),
					sizeof(req_key->txiv));
			qdf_mem_copy((uint8_t *)(&req_key->recviv),
					(uint8_t *)(key->recviv),
					sizeof(req_key->recviv));
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_crypto_delkey - called by ucfg to delete key
 * @vdev: vdev
 * @mac_address: mac address of the peer for unicast key
 *                or broadcast address if group key is deleted.
 * @key_idx: key index to be deleted
 *
 * This function gets called from ucfg to delete key
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_crypto_delkey(struct wlan_objmgr_vdev *vdev,
				uint8_t *macaddr,
				uint8_t key_idx){
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;
	struct wlan_crypto_key *key;
	struct wlan_crypto_cipher *cipher_table;
	struct wlan_objmgr_psoc *psoc;
	uint8_t bssid_mac[WLAN_ALEN];

	if (!vdev || !macaddr ||
		(key_idx >=
			(WLAN_CRYPTO_MAXKEYIDX + WLAN_CRYPTO_MAXIGTKKEYIDX))) {
			QDF_TRACE(QDF_MODULE_ID_CRYPTO, QDF_TRACE_LEVEL_ERROR,
				"%s[%d] Invalid params vdev %pK, macaddr %pK"
					"keyidx %d\n", __func__, __LINE__, vdev,
					macaddr, key_idx);
		return QDF_STATUS_E_INVAL;
	}

	wlan_vdev_obj_lock(vdev);
	qdf_mem_copy(bssid_mac, wlan_vdev_mlme_get_macaddr(vdev), WLAN_ALEN);
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		wlan_vdev_obj_unlock(vdev);
		qdf_print("%s[%d] psoc NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}
	wlan_vdev_obj_unlock(vdev);

	if (qdf_is_macaddr_broadcast((struct qdf_mac_addr *)macaddr)) {
		crypto_params = wlan_crypto_vdev_get_comp_params(vdev,
								&crypto_priv);
		if (crypto_priv == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}
	} else {
		struct wlan_objmgr_peer *peer;
		uint8_t pdev_id;

		pdev_id = wlan_objmgr_pdev_get_pdev_id(
				wlan_vdev_get_pdev(vdev));
		peer = wlan_objmgr_get_peer_by_mac_n_vdev(
				psoc, pdev_id,
				bssid_mac,
				macaddr,
				WLAN_CRYPTO_ID);
		if (peer == NULL) {
			return QDF_STATUS_E_INVAL;
		}
		crypto_params = wlan_crypto_peer_get_comp_params(peer,
								&crypto_priv);
		wlan_objmgr_peer_release_ref(peer, WLAN_CRYPTO_ID);
		if (crypto_priv == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}
	}

	if (key_idx >= WLAN_CRYPTO_MAXKEYIDX) {
		uint8_t igtk_idx = key_idx - WLAN_CRYPTO_MAXKEYIDX;
		if (igtk_idx >= WLAN_CRYPTO_MAXIGTKKEYIDX) {
			qdf_print("%s[%d] Igtk key invalid keyid %d\n",
			__func__, __LINE__, igtk_idx);
			return QDF_STATUS_E_INVAL;
		}
		key = crypto_priv->igtk_key[igtk_idx];
		crypto_priv->igtk_key[igtk_idx] = NULL;
		if (key)
			key->valid = 0;
	} else {
		key = crypto_priv->key[key_idx];
		crypto_priv->key[key_idx] = NULL;
	}

	if (!key)
		return QDF_STATUS_E_INVAL;

	if (key->valid) {
		cipher_table = (struct wlan_crypto_cipher *)key->cipher_table;

		if (WLAN_CRYPTO_TX_OPS_DELKEY(psoc)) {
			WLAN_CRYPTO_TX_OPS_DELKEY(psoc)(vdev, key,
						macaddr, cipher_table->cipher);
		}
	}
	qdf_mem_free(key);

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_crypto_default_key - called by ucfg to set default tx key
 * @vdev: vdev
 * @mac_address: mac address of the peer for unicast key
 *            or broadcast address if group key need to made default.
 * @key_idx: key index to be made as default key
 * @unicast: is key was unicast or group key.
 *
 * This function gets called from ucfg to set default key
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_crypto_default_key(struct wlan_objmgr_vdev *vdev,
					uint8_t *macaddr,
					uint8_t key_idx,
					bool unicast){
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;
	struct wlan_crypto_key *key;
	struct wlan_objmgr_psoc *psoc;
	uint8_t bssid_mac[WLAN_ALEN];

	if (!vdev || !macaddr || (key_idx >= WLAN_CRYPTO_MAXKEYIDX)) {
		qdf_print("%s[%d] Invalid params vdev %pK, macaddr %pK"
				"keyidx %d\n", __func__, __LINE__,
				vdev, macaddr, key_idx);
		return QDF_STATUS_E_INVAL;
	}

	wlan_vdev_obj_lock(vdev);
	qdf_mem_copy(bssid_mac, wlan_vdev_mlme_get_macaddr(vdev), WLAN_ALEN);
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		wlan_vdev_obj_unlock(vdev);
		qdf_print("%s[%d] psoc NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}
	wlan_vdev_obj_unlock(vdev);

	if (qdf_is_macaddr_broadcast((struct qdf_mac_addr *)macaddr)) {
		crypto_params = wlan_crypto_vdev_get_comp_params(vdev,
								&crypto_priv);
		if (crypto_priv == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}

		key = crypto_priv->key[key_idx];
		if (!key)
			return QDF_STATUS_E_INVAL;
	} else {
		struct wlan_objmgr_peer *peer;
		uint8_t pdev_id;

		pdev_id = wlan_objmgr_pdev_get_pdev_id(
				wlan_vdev_get_pdev(vdev));
		peer = wlan_objmgr_get_peer_by_mac_n_vdev(
				psoc, pdev_id,
				bssid_mac,
				macaddr,
				WLAN_CRYPTO_ID);

		if (peer == NULL) {
			qdf_print("%s[%d] peer NULL\n", __func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}
		crypto_params = wlan_crypto_peer_get_comp_params(peer,
								&crypto_priv);
		wlan_objmgr_peer_release_ref(peer, WLAN_CRYPTO_ID);
		if (crypto_priv == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}

		key = crypto_priv->key[key_idx];
		if (!key)
			return QDF_STATUS_E_INVAL;
	}
	if (!key->valid)
		return QDF_STATUS_E_INVAL;

	if (WLAN_CRYPTO_TX_OPS_DEFAULTKEY(psoc)) {
		WLAN_CRYPTO_TX_OPS_DEFAULTKEY(psoc)(vdev, key_idx,
						macaddr);
	}
	crypto_priv->def_tx_keyid = key_idx;

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_crypto_encap - called by mgmt for encap the frame based on cipher
 * @vdev: vdev
 * @wbuf: wbuf
 * @macaddr: macaddr
 * @encapdone: is encapdone already or not.
 *
 * This function gets called from mgmt txrx to encap frame.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_crypto_encap(struct wlan_objmgr_vdev *vdev,
				qdf_nbuf_t wbuf,
				uint8_t *mac_addr,
				uint8_t encapdone){
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;
	struct wlan_crypto_key *key;
	QDF_STATUS status;
	struct wlan_crypto_cipher *cipher_table;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_peer *peer;
	uint8_t bssid_mac[WLAN_ALEN];
	uint8_t pdev_id;
	uint8_t hdrlen;
	enum QDF_OPMODE opmode;

	opmode = wlan_vdev_mlme_get_opmode(vdev);
	wlan_vdev_obj_lock(vdev);
	qdf_mem_copy(bssid_mac, wlan_vdev_mlme_get_macaddr(vdev), WLAN_ALEN);
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		wlan_vdev_obj_unlock(vdev);
		qdf_print("%s[%d] psoc NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}
	wlan_vdev_obj_unlock(vdev);

	pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	/* FILS Encap required only for (Re-)Assoc response */
	peer = wlan_objmgr_get_peer(psoc, pdev_id, mac_addr, WLAN_CRYPTO_ID);

	if (!wlan_crypto_is_data_protected((uint8_t *)qdf_nbuf_data(wbuf)) &&
	    peer && !wlan_crypto_get_peer_fils_aead(peer)) {
		wlan_objmgr_peer_release_ref(peer, WLAN_CRYPTO_ID);
		return QDF_STATUS_E_INVAL;
	}

	if (peer)
		wlan_objmgr_peer_release_ref(peer, WLAN_CRYPTO_ID);

	if (qdf_is_macaddr_group((struct qdf_mac_addr *)mac_addr)) {
		crypto_params = wlan_crypto_vdev_get_comp_params(vdev,
								&crypto_priv);
		if (crypto_priv == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}

		key = crypto_priv->key[crypto_priv->def_tx_keyid];
		if (!key)
			return QDF_STATUS_E_INVAL;

	} else {
		struct wlan_objmgr_peer *peer;
		uint8_t pdev_id;

		pdev_id = wlan_objmgr_pdev_get_pdev_id(
				wlan_vdev_get_pdev(vdev));
		peer = wlan_objmgr_get_peer_by_mac_n_vdev(psoc, pdev_id,
							  bssid_mac, mac_addr,
							  WLAN_CRYPTO_ID);

		if (peer == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}
		crypto_params = wlan_crypto_peer_get_comp_params(peer,
								&crypto_priv);
		wlan_objmgr_peer_release_ref(peer, WLAN_CRYPTO_ID);

		if (crypto_priv == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}

		key = crypto_priv->key[crypto_priv->def_tx_keyid];
		if (!key)
			return QDF_STATUS_E_INVAL;
	}
	if (opmode == QDF_MONITOR_MODE)
		hdrlen = ieee80211_hdrsize((uint8_t *)qdf_nbuf_data(wbuf));
	else
		hdrlen = ieee80211_hdrspace(wlan_vdev_get_pdev(vdev),
					    (uint8_t *)qdf_nbuf_data(wbuf));

	/* if tkip, is counter measures enabled, then drop the frame */
	cipher_table = (struct wlan_crypto_cipher *)key->cipher_table;
	status = cipher_table->encap(key, wbuf, encapdone,
				     hdrlen);

	return status;
}
qdf_export_symbol(wlan_crypto_encap);

/**
 * wlan_crypto_decap - called by mgmt for decap the frame based on cipher
 * @vdev: vdev
 * @wbuf: wbuf
 * @macaddr: macaddr
 * @tid: tid of the frame
 *
 * This function gets called from mgmt txrx to decap frame.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_crypto_decap(struct wlan_objmgr_vdev *vdev,
				qdf_nbuf_t wbuf,
				uint8_t *mac_addr,
				uint8_t tid){
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;
	struct wlan_crypto_key *key;
	QDF_STATUS status;
	struct wlan_crypto_cipher *cipher_table;
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_peer *peer;
	uint8_t bssid_mac[WLAN_ALEN];
	uint8_t keyid;
	uint8_t pdev_id;
	uint8_t hdrlen;
	enum QDF_OPMODE opmode;

	opmode = wlan_vdev_mlme_get_opmode(vdev);
	wlan_vdev_obj_lock(vdev);
	qdf_mem_copy(bssid_mac, wlan_vdev_mlme_get_macaddr(vdev), WLAN_ALEN);
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		wlan_vdev_obj_unlock(vdev);
		qdf_print("%s[%d] psoc NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}
	wlan_vdev_obj_unlock(vdev);

	if (opmode == QDF_MONITOR_MODE)
		hdrlen = ieee80211_hdrsize((uint8_t *)qdf_nbuf_data(wbuf));
	else
		hdrlen = ieee80211_hdrspace(wlan_vdev_get_pdev(vdev),
					    (uint8_t *)qdf_nbuf_data(wbuf));

	keyid = wlan_crypto_get_keyid((uint8_t *)qdf_nbuf_data(wbuf), hdrlen);

	if (keyid >= WLAN_CRYPTO_MAXKEYIDX)
		return QDF_STATUS_E_INVAL;

	pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	/* FILS Decap required only for (Re-)Assoc request */
	peer = wlan_objmgr_get_peer(psoc, pdev_id, mac_addr, WLAN_CRYPTO_ID);

	if (!wlan_crypto_is_data_protected((uint8_t *)qdf_nbuf_data(wbuf)) &&
	    peer && !wlan_crypto_get_peer_fils_aead(peer)) {
		wlan_objmgr_peer_release_ref(peer, WLAN_CRYPTO_ID);
		return QDF_STATUS_E_INVAL;
	}

	if (peer)
		wlan_objmgr_peer_release_ref(peer, WLAN_CRYPTO_ID);

	if (qdf_is_macaddr_group((struct qdf_mac_addr *)mac_addr)) {
		crypto_params = wlan_crypto_vdev_get_comp_params(vdev,
								&crypto_priv);
		if (crypto_priv == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}

		key = crypto_priv->key[keyid];
		if (!key)
			return QDF_STATUS_E_INVAL;

	} else {
		struct wlan_objmgr_peer *peer;
		uint8_t pdev_id;

		pdev_id = wlan_objmgr_pdev_get_pdev_id(
				wlan_vdev_get_pdev(vdev));
		peer = wlan_objmgr_get_peer_by_mac_n_vdev(
					psoc, pdev_id, bssid_mac,
					mac_addr, WLAN_CRYPTO_ID);
		if (peer == NULL) {
			qdf_print("%s[%d] peer NULL\n", __func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}

		crypto_params = wlan_crypto_peer_get_comp_params(peer,
								&crypto_priv);
		wlan_objmgr_peer_release_ref(peer, WLAN_CRYPTO_ID);

		if (crypto_priv == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}

		key = crypto_priv->key[keyid];
		if (!key)
			return QDF_STATUS_E_INVAL;
	}
	/* if tkip, is counter measures enabled, then drop the frame */
	cipher_table = (struct wlan_crypto_cipher *)key->cipher_table;
	status = cipher_table->decap(key, wbuf, tid, hdrlen);

	return status;
}
qdf_export_symbol(wlan_crypto_decap);
/**
 * wlan_crypto_enmic - called by mgmt for adding mic in frame based on cipher
 * @vdev: vdev
 * @wbuf: wbuf
 * @macaddr: macaddr
 * @encapdone: is encapdone already or not.
 *
 * This function gets called from mgmt txrx to adding mic to the frame.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_crypto_enmic(struct wlan_objmgr_vdev *vdev,
				qdf_nbuf_t wbuf,
				uint8_t *mac_addr,
				uint8_t encapdone){
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;
	struct wlan_crypto_key *key;
	QDF_STATUS status;
	struct wlan_crypto_cipher *cipher_table;
	struct wlan_objmgr_psoc *psoc;
	uint8_t bssid_mac[WLAN_ALEN];
	uint8_t hdrlen;
	enum QDF_OPMODE opmode;

	opmode = wlan_vdev_mlme_get_opmode(vdev);


	wlan_vdev_obj_lock(vdev);
	qdf_mem_copy(bssid_mac, wlan_vdev_mlme_get_macaddr(vdev), WLAN_ALEN);
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		wlan_vdev_obj_unlock(vdev);
		qdf_print("%s[%d] psoc NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}
	wlan_vdev_obj_unlock(vdev);

	if (qdf_is_macaddr_broadcast((struct qdf_mac_addr *)mac_addr)) {
		crypto_params = wlan_crypto_vdev_get_comp_params(vdev,
								&crypto_priv);
		if (crypto_priv == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}

		key = crypto_priv->key[crypto_priv->def_tx_keyid];
		if (!key)
			return QDF_STATUS_E_INVAL;

	} else {
		struct wlan_objmgr_peer *peer;
		uint8_t pdev_id;

		pdev_id = wlan_objmgr_pdev_get_pdev_id(
				wlan_vdev_get_pdev(vdev));
		peer = wlan_objmgr_get_peer_by_mac_n_vdev(
					psoc, pdev_id, bssid_mac,
					mac_addr, WLAN_CRYPTO_ID);
		if (peer == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}

		crypto_params = wlan_crypto_peer_get_comp_params(peer,
								&crypto_priv);
		wlan_objmgr_peer_release_ref(peer, WLAN_CRYPTO_ID);

		if (crypto_priv == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}

		key = crypto_priv->key[crypto_priv->def_tx_keyid];
		if (!key)
			return QDF_STATUS_E_INVAL;
	}
	if (opmode == QDF_MONITOR_MODE)
		hdrlen = ieee80211_hdrsize((uint8_t *)qdf_nbuf_data(wbuf));
	else
		hdrlen = ieee80211_hdrspace(wlan_vdev_get_pdev(vdev),
					    (uint8_t *)qdf_nbuf_data(wbuf));

	/* if tkip, is counter measures enabled, then drop the frame */
	cipher_table = (struct wlan_crypto_cipher *)key->cipher_table;
	status = cipher_table->enmic(key, wbuf, encapdone, hdrlen);

	return status;
}

/**
 * wlan_crypto_demic - called by mgmt for remove and check mic for
 *			                        the frame based on cipher
 * @vdev: vdev
 * @wbuf: wbuf
 * @macaddr: macaddr
 * @tid: tid of the frame
 * @keyid: keyid in the received frame
 * This function gets called from mgmt txrx to decap frame.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_crypto_demic(struct wlan_objmgr_vdev *vdev,
			     qdf_nbuf_t wbuf,
			     uint8_t *mac_addr,
			     uint8_t tid,
			     uint8_t keyid){
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;
	struct wlan_crypto_key *key;
	QDF_STATUS status;
	struct wlan_crypto_cipher *cipher_table;
	struct wlan_objmgr_psoc *psoc;
	uint8_t bssid_mac[WLAN_ALEN];
	uint8_t hdrlen;
	enum QDF_OPMODE opmode;

	opmode = wlan_vdev_mlme_get_opmode(vdev);

	if (opmode == QDF_MONITOR_MODE)
		hdrlen = ieee80211_hdrsize((uint8_t *)qdf_nbuf_data(wbuf));
	else
		hdrlen = ieee80211_hdrspace(wlan_vdev_get_pdev(vdev),
					    (uint8_t *)qdf_nbuf_data(wbuf));

	wlan_vdev_obj_lock(vdev);
	qdf_mem_copy(bssid_mac, wlan_vdev_mlme_get_macaddr(vdev), WLAN_ALEN);
	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		wlan_vdev_obj_unlock(vdev);
		qdf_print("%s[%d] psoc NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}
	wlan_vdev_obj_unlock(vdev);

	if (qdf_is_macaddr_broadcast((struct qdf_mac_addr *)mac_addr)) {
		crypto_params = wlan_crypto_vdev_get_comp_params(vdev,
								&crypto_priv);
		if (crypto_priv == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}

		key = crypto_priv->key[keyid];
		if (!key)
			return QDF_STATUS_E_INVAL;

	} else {
		struct wlan_objmgr_peer *peer;
		uint8_t pdev_id;

		pdev_id = wlan_objmgr_pdev_get_pdev_id(
				wlan_vdev_get_pdev(vdev));
		peer = wlan_objmgr_get_peer_by_mac_n_vdev(
					psoc, pdev_id, bssid_mac,
					mac_addr, WLAN_CRYPTO_ID);
		if (peer == NULL) {
			qdf_print("%s[%d] peer NULL\n", __func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}

		crypto_params = wlan_crypto_peer_get_comp_params(peer,
								&crypto_priv);
		wlan_objmgr_peer_release_ref(peer, WLAN_CRYPTO_ID);

		if (crypto_priv == NULL) {
			qdf_print("%s[%d] crypto_priv NULL\n",
							__func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}

		key = crypto_priv->key[keyid];
		if (!key)
			return QDF_STATUS_E_INVAL;
	}
	/* if tkip, is counter measures enabled, then drop the frame */
	cipher_table = (struct wlan_crypto_cipher *)key->cipher_table;
	status = cipher_table->demic(key, wbuf, tid, hdrlen);

	return status;
}

/**
 * wlan_crypto_vdev_is_pmf_enabled - called to check is pmf enabled in vdev
 * @vdev: vdev
 *
 * This function gets called to check is pmf enabled or not in vdev.
 *
 * Return: true or false
 */
bool wlan_crypto_vdev_is_pmf_enabled(struct wlan_objmgr_vdev *vdev)
{

	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *vdev_crypto_params;

	if (!vdev)
		return false;
	vdev_crypto_params = wlan_crypto_vdev_get_comp_params(vdev,
							&crypto_priv);
	if (crypto_priv == NULL) {
		qdf_print("%s[%d] crypto_priv NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}

	if ((vdev_crypto_params->rsn_caps &
					WLAN_CRYPTO_RSN_CAP_MFP_ENABLED)
		|| (vdev_crypto_params->rsn_caps &
					WLAN_CRYPTO_RSN_CAP_MFP_REQUIRED)) {
		return true;
	}

	return false;
}
/**
 * wlan_crypto_is_pmf_enabled - called by mgmt txrx to check is pmf enabled
 * @vdev: vdev
 * @peer: peer
 *
 * This function gets called by mgmt txrx to check is pmf enabled or not.
 *
 * Return: true or false
 */
bool wlan_crypto_is_pmf_enabled(struct wlan_objmgr_vdev *vdev,
				struct wlan_objmgr_peer *peer){

	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *vdev_crypto_params;
	struct wlan_crypto_params *peer_crypto_params;

	if (!vdev || !peer)
		return false;
	vdev_crypto_params = wlan_crypto_vdev_get_comp_params(vdev,
							&crypto_priv);
	if (crypto_priv == NULL) {
		qdf_print("%s[%d] crypto_priv NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}

	peer_crypto_params = wlan_crypto_peer_get_comp_params(peer,
							&crypto_priv);
	if (crypto_priv == NULL) {
		qdf_print("%s[%d] crypto_priv NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}
	if (((vdev_crypto_params->rsn_caps &
					WLAN_CRYPTO_RSN_CAP_MFP_ENABLED) &&
		(peer_crypto_params->rsn_caps &
					WLAN_CRYPTO_RSN_CAP_MFP_ENABLED))
		|| (vdev_crypto_params->rsn_caps &
					WLAN_CRYPTO_RSN_CAP_MFP_REQUIRED)) {
		return true;
	}

	return false;
}

static void wlan_crypto_gmac_pn_swap(uint8_t *a, uint8_t *b)
{
	a[0] = b[5];
	a[1] = b[4];
	a[2] = b[3];
	a[3] = b[2];
	a[4] = b[1];
	a[5] = b[0];
}

/**
 * wlan_crypto_add_mmie - called by mgmt txrx to add mmie in frame
 * @vdev: vdev
 * @bfrm:  frame starting pointer
 * @len:  length of the frame
 *
 * This function gets called by mgmt txrx to add mmie in frame
 *
 * Return: end of frame or NULL in case failure
 */
uint8_t *wlan_crypto_add_mmie(struct wlan_objmgr_vdev *vdev,
				uint8_t *bfrm,
				uint32_t len) {
	struct wlan_crypto_key *key;
	struct wlan_crypto_mmie *mmie;
	uint8_t *pn, *aad, *buf, *efrm, nounce[12];
	struct ieee80211_hdr *hdr;
	uint32_t i, hdrlen, mic_len, aad_len;
	uint8_t mic[16];
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;
	int32_t ret = -1;

	if (!bfrm) {
		qdf_print("%s[%d] frame is NULL\n", __func__, __LINE__);
		return NULL;
	}

	crypto_params = wlan_crypto_vdev_get_comp_params(vdev,
							&crypto_priv);
	if (crypto_priv == NULL) {
		qdf_print("%s[%d] crypto_priv NULL\n", __func__, __LINE__);
		return NULL;
	}

	if (crypto_priv->def_igtk_tx_keyid >= WLAN_CRYPTO_MAXIGTKKEYIDX) {
		qdf_print("%s[%d] igtk key invalid keyid %d \n",
			__func__, __LINE__, crypto_priv->def_igtk_tx_keyid);
		return NULL;
	}

	key = crypto_priv->igtk_key[crypto_priv->def_igtk_tx_keyid];
	if (!key) {
		qdf_print("%s[%d] No igtk key present\n", __func__, __LINE__);
		return NULL;
	}
	mic_len = (crypto_priv->igtk_key_type
			== WLAN_CRYPTO_CIPHER_AES_CMAC) ? 8 : 16;

	efrm = bfrm + len;
	aad_len = 20;
	hdrlen = sizeof(struct ieee80211_hdr);
	len += sizeof(struct wlan_crypto_mmie);

	mmie = (struct wlan_crypto_mmie *) efrm;
	qdf_mem_zero((unsigned char *)mmie, sizeof(*mmie));
	mmie->element_id = WLAN_ELEMID_MMIE;
	mmie->length = sizeof(*mmie) - 2;
	mmie->key_id = qdf_cpu_to_le16(key->keyix);

	mic_len = (crypto_priv->igtk_key_type
			== WLAN_CRYPTO_CIPHER_AES_CMAC) ? 8 : 16;
	if (mic_len == 8) {
		mmie->length -= 8;
		len -= 8;
	}
	/* PN = PN + 1 */
	pn = (uint8_t *)&key->keytsc;

	for (i = 0; i <= 5; i++) {
		pn[i]++;
		if (pn[i])
			break;
	}

	/* Copy IPN */
	qdf_mem_copy(mmie->sequence_number, pn, 6);

	hdr = (struct ieee80211_hdr *) bfrm;

	buf = qdf_mem_malloc(len - hdrlen + 20);
	if (!buf) {
		qdf_print("%s[%d] malloc failed\n", __func__, __LINE__);
		return NULL;
	}
	qdf_mem_zero(buf, len - hdrlen + 20);
	aad = buf;
	/* generate BIP AAD: FC(masked) || A1 || A2 || A3 */

	/* FC type/subtype */
	aad[0] = hdr->frame_control[0];
	/* Mask FC Retry, PwrMgt, MoreData flags to zero */
	aad[1] = (hdr->frame_control[1] & ~(WLAN_FC1_RETRY | WLAN_FC1_PWRMGT
						| WLAN_FC1_MOREDATA));
	/* A1 || A2 || A3 */
	qdf_mem_copy(aad + 2, hdr->addr1, WLAN_ALEN);
	qdf_mem_copy(aad + 8, hdr->addr2, WLAN_ALEN);
	qdf_mem_copy(aad + 14, hdr->addr3, WLAN_ALEN);
	qdf_mem_zero(mic, 16);

	/*
	 * MIC = AES-128-CMAC(IGTK, AAD || Management Frame Body || MMIE, 64)
	 */

	qdf_mem_copy(buf + aad_len, bfrm + hdrlen, len - hdrlen);
	if (crypto_priv->igtk_key_type == WLAN_CRYPTO_CIPHER_AES_CMAC) {

		ret = omac1_aes_128(key->keyval, buf,
					len + aad_len - hdrlen, mic);
		qdf_mem_copy(mmie->mic, mic, 8);

	} else if (crypto_priv->igtk_key_type
				== WLAN_CRYPTO_CIPHER_AES_CMAC_256) {

		ret = omac1_aes_256(key->keyval, buf,
					len + aad_len - hdrlen, mmie->mic);
	} else if ((crypto_priv->igtk_key_type == WLAN_CRYPTO_CIPHER_AES_GMAC)
			|| (crypto_priv->igtk_key_type
					== WLAN_CRYPTO_CIPHER_AES_GMAC_256)) {

		qdf_mem_copy(nounce, hdr->addr2, WLAN_ALEN);
		wlan_crypto_gmac_pn_swap(nounce + 6, pn);
		ret = wlan_crypto_aes_gmac(key->keyval, key->keylen, nounce,
					sizeof(nounce), buf,
					len + aad_len - hdrlen, mmie->mic);
	}
	qdf_mem_free(buf);
	if (ret < 0) {
		qdf_print("%s[%d] add mmie failed\n", __func__, __LINE__);
		return NULL;
	}

	return bfrm + len;
}

/**
 * wlan_crypto_is_mmie_valid - called by mgmt txrx to check mmie of the frame
 * @vdev: vdev
 * @frm:  frame starting pointer
 * @efrm: end of frame pointer
 *
 * This function gets called by mgmt txrx to check mmie of the frame
 *
 * Return: true or false
 */
bool wlan_crypto_is_mmie_valid(struct wlan_objmgr_vdev *vdev,
					uint8_t *frm,
					uint8_t *efrm){
	struct wlan_crypto_mmie   *mmie = NULL;
	uint8_t *ipn, *aad, *buf, mic[16], nounce[12];
	struct wlan_crypto_key *key;
	struct ieee80211_hdr *hdr;
	uint16_t mic_len, hdrlen, len;
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;
	uint8_t aad_len = 20;
	int32_t ret = -1;

	/* check if frame is illegal length */
	if (!frm || !efrm || (efrm < frm)
			|| ((efrm - frm) < sizeof(struct ieee80211_hdr))) {
		qdf_print("%s[%d] Invalid params\n", __func__, __LINE__);
		return false;
	}
	len = efrm - frm;
	crypto_priv = (struct wlan_crypto_comp_priv *)
				wlan_get_vdev_crypto_obj(vdev);
	if (crypto_priv == NULL) {
		qdf_print("%s[%d] crypto_priv NULL\n", __func__, __LINE__);
		return false;
	}

	crypto_params = &(crypto_priv->crypto_params);


	mic_len = (crypto_priv->igtk_key_type
			== WLAN_CRYPTO_CIPHER_AES_CMAC) ? 8 : 16;
	hdrlen = sizeof(struct ieee80211_hdr);

	if (mic_len == 8)
		mmie = (struct wlan_crypto_mmie *)(efrm - sizeof(*mmie) + 8);
	else
		mmie = (struct wlan_crypto_mmie *)(efrm - sizeof(*mmie));


	/* check Elem ID*/
	if ((mmie == NULL) || (mmie->element_id != WLAN_ELEMID_MMIE)) {
		qdf_print("%s[%d] IE is not MMIE\n", __func__, __LINE__);
		return false;
	}

	if (mmie->key_id >= (WLAN_CRYPTO_MAXKEYIDX +
				WLAN_CRYPTO_MAXIGTKKEYIDX) ||
				(mmie->key_id < WLAN_CRYPTO_MAXKEYIDX)) {
		qdf_print("%s[%d] keyid not valid\n", __func__, __LINE__);
		return false;
	}

	key = crypto_priv->igtk_key[mmie->key_id - WLAN_CRYPTO_MAXKEYIDX];
	if (!key) {
		qdf_print("%s[%d] No igtk key present\n", __func__, __LINE__);
		return false;
	}

	/* validate ipn */
	ipn = mmie->sequence_number;
	if (qdf_mem_cmp(ipn, key->keyrsc, 6) <= 0) {
		qdf_print("%s[%d] replay error\n", __func__, __LINE__);
		return false;
	}

	buf = qdf_mem_malloc(len - hdrlen + 20);
	if (!buf) {
		qdf_print("%s[%d] malloc failed\n", __func__, __LINE__);
		return false;
	}
	aad = buf;

	/* construct AAD */
	hdr = (struct ieee80211_hdr *)frm;
	/* generate BIP AAD: FC(masked) || A1 || A2 || A3 */

	/* FC type/subtype */
	aad[0] = hdr->frame_control[0];
	/* Mask FC Retry, PwrMgt, MoreData flags to zero */
	aad[1] = (hdr->frame_control[1] & ~(WLAN_FC1_RETRY | WLAN_FC1_PWRMGT
						| WLAN_FC1_MOREDATA));
	/* A1 || A2 || A3 */
	qdf_mem_copy(aad + 2, hdr->addr1, 3 * WLAN_ALEN);

	/*
	 * MIC = AES-128-CMAC(IGTK, AAD || Management Frame Body || MMIE, 64)
	 */
	qdf_mem_copy(buf + 20, frm + hdrlen, len - hdrlen);
	qdf_mem_zero(buf + (len - hdrlen + 20 - mic_len), mic_len);
	qdf_mem_zero(mic, 16);
	if (crypto_priv->igtk_key_type == WLAN_CRYPTO_CIPHER_AES_CMAC) {
		ret = omac1_aes_128(key->keyval, buf,
					len - hdrlen + aad_len, mic);
	} else if (crypto_priv->igtk_key_type
				== WLAN_CRYPTO_CIPHER_AES_CMAC_256) {
		ret = omac1_aes_256(key->keyval, buf,
					len + aad_len - hdrlen, mic);
	} else if ((crypto_priv->igtk_key_type == WLAN_CRYPTO_CIPHER_AES_GMAC)
			|| (crypto_priv->igtk_key_type
					== WLAN_CRYPTO_CIPHER_AES_GMAC_256)) {
		qdf_mem_copy(nounce, hdr->addr2, WLAN_ALEN);
		wlan_crypto_gmac_pn_swap(nounce + 6, ipn);
		ret = wlan_crypto_aes_gmac(key->keyval, key->keylen, nounce,
					sizeof(nounce), buf,
					len + aad_len - hdrlen, mic);
	}

	qdf_mem_free(buf);

	if (ret < 0) {
		qdf_print("%s[%d] genarate mmie failed\n", __func__, __LINE__);
		return false;
	}

	if (qdf_mem_cmp(mic, mmie->mic, mic_len) != 0) {
		qdf_print("%s[%d] mmie mismatch\n", __func__, __LINE__);
		/* MMIE MIC mismatch */
		return false;
	}

	/* Update the receive sequence number */
	qdf_mem_copy(key->keyrsc, ipn, 6);
	qdf_print("%s[%d] mmie matched\n", __func__, __LINE__);

	return true;
}


static int32_t wlan_crypto_wpa_cipher_to_suite(uint32_t cipher)
{
	int32_t status = -1;

	switch (cipher) {
	case WLAN_CRYPTO_CIPHER_TKIP:
		return WPA_CIPHER_SUITE_TKIP;
	case WLAN_CRYPTO_CIPHER_AES_CCM:
		return WPA_CIPHER_SUITE_CCMP;
	case WLAN_CRYPTO_CIPHER_NONE:
		return WPA_CIPHER_SUITE_NONE;
	}

	return status;
}

static int32_t wlan_crypto_rsn_cipher_to_suite(uint32_t cipher)
{
	int32_t status = -1;

	switch (cipher) {
	case WLAN_CRYPTO_CIPHER_TKIP:
		return RSN_CIPHER_SUITE_TKIP;
	case WLAN_CRYPTO_CIPHER_AES_CCM:
		return RSN_CIPHER_SUITE_CCMP;
	case WLAN_CRYPTO_CIPHER_AES_CCM_256:
		return RSN_CIPHER_SUITE_CCMP_256;
	case WLAN_CRYPTO_CIPHER_AES_GCM:
		return RSN_CIPHER_SUITE_GCMP;
	case WLAN_CRYPTO_CIPHER_AES_GCM_256:
		return RSN_CIPHER_SUITE_GCMP_256;
	case WLAN_CRYPTO_CIPHER_AES_CMAC:
		return RSN_CIPHER_SUITE_AES_CMAC;
	case WLAN_CRYPTO_CIPHER_AES_CMAC_256:
		return RSN_CIPHER_SUITE_BIP_CMAC_256;
	case WLAN_CRYPTO_CIPHER_AES_GMAC:
		return RSN_CIPHER_SUITE_BIP_GMAC_128;
	case WLAN_CRYPTO_CIPHER_AES_GMAC_256:
		return RSN_CIPHER_SUITE_BIP_GMAC_256;
	case WLAN_CRYPTO_CIPHER_NONE:
		return RSN_CIPHER_SUITE_NONE;
	}

	return status;
}

/*
 * Convert an RSN key management/authentication algorithm
 * to an internal code.
 */
static int32_t
wlan_crypto_rsn_keymgmt_to_suite(uint32_t keymgmt)
{
	int32_t status = -1;

	switch (keymgmt) {
	case WLAN_CRYPTO_KEY_MGMT_NONE:
		return RSN_AUTH_KEY_MGMT_NONE;
	case WLAN_CRYPTO_KEY_MGMT_IEEE8021X:
		return RSN_AUTH_KEY_MGMT_UNSPEC_802_1X;
	case WLAN_CRYPTO_KEY_MGMT_PSK:
		return RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X;
	case WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X:
		return RSN_AUTH_KEY_MGMT_FT_802_1X;
	case WLAN_CRYPTO_KEY_MGMT_FT_PSK:
		return RSN_AUTH_KEY_MGMT_FT_PSK;
	case WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SHA256:
		return RSN_AUTH_KEY_MGMT_802_1X_SHA256;
	case WLAN_CRYPTO_KEY_MGMT_PSK_SHA256:
		return RSN_AUTH_KEY_MGMT_PSK_SHA256;
	case WLAN_CRYPTO_KEY_MGMT_SAE:
		return RSN_AUTH_KEY_MGMT_SAE;
	case WLAN_CRYPTO_KEY_MGMT_FT_SAE:
		return RSN_AUTH_KEY_MGMT_FT_SAE;
	case WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B:
		return RSN_AUTH_KEY_MGMT_802_1X_SUITE_B;
	case WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B_192:
		return RSN_AUTH_KEY_MGMT_802_1X_SUITE_B_192;
	case WLAN_CRYPTO_KEY_MGMT_CCKM:
		return RSN_AUTH_KEY_MGMT_CCKM;
	case WLAN_CRYPTO_KEY_MGMT_OSEN:
		return RSN_AUTH_KEY_MGMT_OSEN;
	case WLAN_CRYPTO_KEY_MGMT_FILS_SHA256:
		return RSN_AUTH_KEY_MGMT_FILS_SHA256;
	case WLAN_CRYPTO_KEY_MGMT_FILS_SHA384:
		return RSN_AUTH_KEY_MGMT_FILS_SHA384;
	case WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA256:
		return RSN_AUTH_KEY_MGMT_FT_FILS_SHA256;
	case WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA384:
		return RSN_AUTH_KEY_MGMT_FT_FILS_SHA384;
	case WLAN_CRYPTO_KEY_MGMT_OWE:
		return RSN_AUTH_KEY_MGMT_OWE;
	case WLAN_CRYPTO_KEY_MGMT_DPP:
		return RSN_AUTH_KEY_MGMT_DPP;
	}

	return status;
}

/*
 * Convert an RSN key management/authentication algorithm
 * to an internal code.
 */
static int32_t
wlan_crypto_wpa_keymgmt_to_suite(uint32_t keymgmt)
{
	int32_t status = -1;

	switch (keymgmt) {
	case WLAN_CRYPTO_KEY_MGMT_NONE:
		return WPA_AUTH_KEY_MGMT_NONE;
	case WLAN_CRYPTO_KEY_MGMT_IEEE8021X:
		return WPA_AUTH_KEY_MGMT_UNSPEC_802_1X;
	case WLAN_CRYPTO_KEY_MGMT_PSK:
		return WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X;
	case WLAN_CRYPTO_KEY_MGMT_CCKM:
		return WPA_AUTH_KEY_MGMT_CCKM;
	}

	return status;
}
/**
 * Convert a WPA cipher selector OUI to an internal
 * cipher algorithm.  Where appropriate we also
 * record any key length.
 */
static int32_t wlan_crypto_wpa_suite_to_cipher(uint8_t *sel)
{
	uint32_t w = LE_READ_4(sel);
	int32_t status = -1;

	switch (w) {
	case WPA_CIPHER_SUITE_TKIP:
		return WLAN_CRYPTO_CIPHER_TKIP;
	case WPA_CIPHER_SUITE_CCMP:
		return WLAN_CRYPTO_CIPHER_AES_CCM;
	case WPA_CIPHER_SUITE_NONE:
		return WLAN_CRYPTO_CIPHER_NONE;
	}

	return status;
}

/*
 * Convert a WPA key management/authentication algorithm
 * to an internal code.
 */
static int32_t wlan_crypto_wpa_suite_to_keymgmt(uint8_t *sel)
{
	uint32_t w = LE_READ_4(sel);
	int32_t status = -1;

	switch (w) {
	case WPA_AUTH_KEY_MGMT_UNSPEC_802_1X:
		return WLAN_CRYPTO_KEY_MGMT_IEEE8021X;
	case WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X:
		return WLAN_CRYPTO_KEY_MGMT_PSK;
	case WPA_AUTH_KEY_MGMT_CCKM:
		return WLAN_CRYPTO_KEY_MGMT_CCKM;
	case WPA_AUTH_KEY_MGMT_NONE:
		return WLAN_CRYPTO_KEY_MGMT_NONE;
	}
	return status;
}

/*
 * Convert a RSN cipher selector OUI to an internal
 * cipher algorithm.  Where appropriate we also
 * record any key length.
 */
static int32_t wlan_crypto_rsn_suite_to_cipher(uint8_t *sel)
{
	uint32_t w = LE_READ_4(sel);
	int32_t status = -1;

	switch (w) {
	case RSN_CIPHER_SUITE_TKIP:
		return WLAN_CRYPTO_CIPHER_TKIP;
	case RSN_CIPHER_SUITE_CCMP:
		return WLAN_CRYPTO_CIPHER_AES_CCM;
	case RSN_CIPHER_SUITE_CCMP_256:
		return WLAN_CRYPTO_CIPHER_AES_CCM_256;
	case RSN_CIPHER_SUITE_GCMP:
		return WLAN_CRYPTO_CIPHER_AES_GCM;
	case RSN_CIPHER_SUITE_GCMP_256:
		return WLAN_CRYPTO_CIPHER_AES_GCM_256;
	case RSN_CIPHER_SUITE_AES_CMAC:
		return WLAN_CRYPTO_CIPHER_AES_CMAC;
	case RSN_CIPHER_SUITE_BIP_CMAC_256:
		return WLAN_CRYPTO_CIPHER_AES_CMAC_256;
	case RSN_CIPHER_SUITE_BIP_GMAC_128:
		return WLAN_CRYPTO_CIPHER_AES_GMAC;
	case RSN_CIPHER_SUITE_BIP_GMAC_256:
		return WLAN_CRYPTO_CIPHER_AES_GMAC_256;
	case RSN_CIPHER_SUITE_NONE:
		return WLAN_CRYPTO_CIPHER_NONE;
	}

	return status;
}
/*
 * Convert an RSN key management/authentication algorithm
 * to an internal code.
 */
static int32_t wlan_crypto_rsn_suite_to_keymgmt(uint8_t *sel)
{
	uint32_t w = LE_READ_4(sel);
	int32_t status = -1;

	switch (w) {
	case RSN_AUTH_KEY_MGMT_UNSPEC_802_1X:
		return WLAN_CRYPTO_KEY_MGMT_IEEE8021X;
	case RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X:
		return WLAN_CRYPTO_KEY_MGMT_PSK;
	case RSN_AUTH_KEY_MGMT_FT_802_1X:
		return WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X;
	case RSN_AUTH_KEY_MGMT_FT_PSK:
		return WLAN_CRYPTO_KEY_MGMT_FT_PSK;
	case RSN_AUTH_KEY_MGMT_802_1X_SHA256:
		return WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SHA256;
	case RSN_AUTH_KEY_MGMT_PSK_SHA256:
		return WLAN_CRYPTO_KEY_MGMT_PSK_SHA256;
	case RSN_AUTH_KEY_MGMT_SAE:
		return WLAN_CRYPTO_KEY_MGMT_SAE;
	case RSN_AUTH_KEY_MGMT_FT_SAE:
		return WLAN_CRYPTO_KEY_MGMT_FT_SAE;
	case RSN_AUTH_KEY_MGMT_802_1X_SUITE_B:
		return WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B;
	case RSN_AUTH_KEY_MGMT_802_1X_SUITE_B_192:
		return WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SUITE_B_192;
	case RSN_AUTH_KEY_MGMT_CCKM:
		return WLAN_CRYPTO_KEY_MGMT_CCKM;
	case RSN_AUTH_KEY_MGMT_OSEN:
		return WLAN_CRYPTO_KEY_MGMT_OSEN;
	case RSN_AUTH_KEY_MGMT_FILS_SHA256:
		return WLAN_CRYPTO_KEY_MGMT_FILS_SHA256;
	case RSN_AUTH_KEY_MGMT_FILS_SHA384:
		return WLAN_CRYPTO_KEY_MGMT_FILS_SHA384;
	case RSN_AUTH_KEY_MGMT_FT_FILS_SHA256:
		return WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA256;
	case RSN_AUTH_KEY_MGMT_FT_FILS_SHA384:
		return WLAN_CRYPTO_KEY_MGMT_FT_FILS_SHA384;
	case RSN_AUTH_KEY_MGMT_OWE:
		return WLAN_CRYPTO_KEY_MGMT_OWE;
	case RSN_AUTH_KEY_MGMT_DPP:
		return WLAN_CRYPTO_KEY_MGMT_DPP;
	}

	return status;
}

/**
 * wlan_crypto_wpaie_check - called by mlme to check the wpaie
 * @crypto params: crypto params
 * @iebuf: ie buffer
 *
 * This function gets called by mlme to check the contents of wpa is
 * matching with given crypto params
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_crypto_wpaie_check(struct wlan_crypto_params *crypto_params,
					uint8_t *frm){
	uint8_t len = frm[1];
	int32_t w;
	int n;

	/*
	 * Check the length once for fixed parts: OUI, type,
	 * version, mcast cipher, and 2 selector counts.
	 * Other, variable-length data, must be checked separately.
	 */
	RESET_AUTHMODE(crypto_params);
	SET_AUTHMODE(crypto_params, WLAN_CRYPTO_AUTH_WPA);

	if (len < 14)
		return QDF_STATUS_E_INVAL;

	frm += 6, len -= 4;

	w = LE_READ_2(frm);
	if (w != WPA_VERSION)
		return QDF_STATUS_E_INVAL;

	frm += 2, len -= 2;

	/* multicast/group cipher */
	RESET_MCAST_CIPHERS(crypto_params);
	w = wlan_crypto_wpa_suite_to_cipher(frm);
	if (w < 0)
		return QDF_STATUS_E_INVAL;
	SET_MCAST_CIPHER(crypto_params, w);
	frm += 4, len -= 4;

	/* unicast ciphers */
	n = LE_READ_2(frm);
	frm += 2, len -= 2;
	if (len < n*4+2)
		return QDF_STATUS_E_INVAL;

	RESET_UCAST_CIPHERS(crypto_params);
	for (; n > 0; n--) {
		w = wlan_crypto_wpa_suite_to_cipher(frm);
		if (w < 0)
			return QDF_STATUS_E_INVAL;
		SET_UCAST_CIPHER(crypto_params, w);
		frm += 4, len -= 4;
	}

	if (!crypto_params->ucastcipherset)
		return QDF_STATUS_E_INVAL;

	/* key management algorithms */
	n = LE_READ_2(frm);
	frm += 2, len -= 2;
	if (len < n*4)
		return QDF_STATUS_E_INVAL;

	w = 0;
	RESET_KEY_MGMT(crypto_params);
	for (; n > 0; n--) {
		w = wlan_crypto_wpa_suite_to_keymgmt(frm);
		if (w < 0)
			return QDF_STATUS_E_INVAL;
		SET_KEY_MGMT(crypto_params, w);
		frm += 4, len -= 4;
	}

	/* optional capabilities */
	if (len >= 2) {
		crypto_params->rsn_caps = LE_READ_2(frm);
		frm += 2, len -= 2;
	}

	return 0;
}

/**
 * wlan_crypto_rsnie_check - called by mlme to check the rsnie
 * @crypto params: crypto params
 * @iebuf: ie buffer
 *
 * This function gets called by mlme to check the contents of wpa is
 * matching with given crypto params
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_crypto_rsnie_check(struct wlan_crypto_params *crypto_params,
					uint8_t *frm){
	uint8_t len = frm[1];
	int32_t w;
	int n;

	/* Check the length once for fixed parts: OUI, type & version */
	if (len < 2)
		return QDF_STATUS_E_INVAL;

	/* initialize crypto params */
	qdf_mem_zero(crypto_params, sizeof(struct wlan_crypto_params));

	SET_AUTHMODE(crypto_params, WLAN_CRYPTO_AUTH_RSNA);

	frm += 2;
	/* NB: iswapoui already validated the OUI and type */
	w = LE_READ_2(frm);
	if (w != RSN_VERSION)
		return QDF_STATUS_E_INVAL;

	frm += 2, len -= 2;

	if (!len) {
		/* set defaults */
		/* default group cipher CCMP-128 */
		SET_MCAST_CIPHER(crypto_params, WLAN_CRYPTO_CIPHER_AES_CCM);
		/* default ucast cipher CCMP-128 */
		SET_UCAST_CIPHER(crypto_params, WLAN_CRYPTO_CIPHER_AES_CCM);
		/* default key mgmt 8021x */
		SET_KEY_MGMT(crypto_params, WLAN_CRYPTO_KEY_MGMT_IEEE8021X);
		return QDF_STATUS_SUCCESS;
	} else if (len < 4) {
		return QDF_STATUS_E_INVAL;
	}

	/* multicast/group cipher */
	w = wlan_crypto_rsn_suite_to_cipher(frm);
	if (w < 0)
		return QDF_STATUS_E_INVAL;
	else {
		SET_MCAST_CIPHER(crypto_params, w);
		frm += 4, len -= 4;
	}

	if (crypto_params->mcastcipherset == 0)
		return QDF_STATUS_E_INVAL;

	if (!len) {
		/* default ucast cipher CCMP-128 */
		SET_UCAST_CIPHER(crypto_params, WLAN_CRYPTO_CIPHER_AES_CCM);
		/* default key mgmt 8021x */
		SET_KEY_MGMT(crypto_params, WLAN_CRYPTO_KEY_MGMT_IEEE8021X);
		return QDF_STATUS_SUCCESS;
	} else if (len < 2) {
		return QDF_STATUS_E_INVAL;
	}

	/* unicast ciphers */
	n = LE_READ_2(frm);
	frm += 2, len -= 2;
	if (n) {
		if (len < n * 4)
			return QDF_STATUS_E_INVAL;

		for (; n > 0; n--) {
			w = wlan_crypto_rsn_suite_to_cipher(frm);
			if (w < 0)
				return QDF_STATUS_E_INVAL;
			SET_UCAST_CIPHER(crypto_params, w);
			frm += 4, len -= 4;
		}
	} else {
		/* default ucast cipher CCMP-128 */
		SET_UCAST_CIPHER(crypto_params, WLAN_CRYPTO_CIPHER_AES_CCM);
	}

	if (crypto_params->ucastcipherset == 0)
		return QDF_STATUS_E_INVAL;

	if (!len) {
		/* default key mgmt 8021x */
		SET_KEY_MGMT(crypto_params, WLAN_CRYPTO_KEY_MGMT_IEEE8021X);
		return QDF_STATUS_SUCCESS;
	} else if (len < 2) {
		return QDF_STATUS_E_INVAL;
	}

	/* key management algorithms */
	n = LE_READ_2(frm);
	frm += 2, len -= 2;

	if (n) {
		if (len < n * 4)
			return QDF_STATUS_E_INVAL;

		for (; n > 0; n--) {
			w = wlan_crypto_rsn_suite_to_keymgmt(frm);
			if (w < 0)
				return QDF_STATUS_E_INVAL;
			SET_KEY_MGMT(crypto_params, w);
			frm += 4, len -= 4;
		}
	} else {
		/* default key mgmt 8021x */
		SET_KEY_MGMT(crypto_params, WLAN_CRYPTO_KEY_MGMT_IEEE8021X);
	}

	if (crypto_params->key_mgmt == 0)
		return QDF_STATUS_E_INVAL;

	/* optional capabilities */
	if (len >= 2) {
		crypto_params->rsn_caps = LE_READ_2(frm);
		frm += 2, len -= 2;
	} else if (len && len < 2) {
		return QDF_STATUS_E_INVAL;
	}


	/* PMKID */
	if (len >= 2) {
		n = LE_READ_2(frm);
		frm += 2, len -= 2;
		if (n && len) {
			if (len >= n * PMKID_LEN)
				frm += (n * PMKID_LEN), len -= (n * PMKID_LEN);
			else
				return QDF_STATUS_E_INVAL;
		} else if (n && !len) {
			return QDF_STATUS_E_INVAL;
		}
		/*TODO: Save pmkid in params for further reference */
	}

	/* BIP */
	if (!len &&
	    (crypto_params->rsn_caps & WLAN_CRYPTO_RSN_CAP_MFP_ENABLED)) {
		/* when no BIP mentioned and MFP capable use CMAC as default*/
		SET_MGMT_CIPHER(crypto_params, WLAN_CRYPTO_CIPHER_AES_CMAC);
		return QDF_STATUS_SUCCESS;
	} else if (len >= 4) {
		w = wlan_crypto_rsn_suite_to_cipher(frm);
		frm += 4, len -= 4;
		SET_MGMT_CIPHER(crypto_params, w);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_crypto_build_wpaie - called by mlme to build wpaie
 * @vdev: vdev
 * @iebuf: ie buffer
 *
 * This function gets called by mlme to build wpaie from given vdev
 *
 * Return: end of buffer
 */
uint8_t *wlan_crypto_build_wpaie(struct wlan_objmgr_vdev *vdev,
					uint8_t *iebuf){
	uint8_t *frm = iebuf;
	uint8_t *selcnt;
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;

	if (!frm)
		return NULL;

	crypto_params = wlan_crypto_vdev_get_comp_params(vdev, &crypto_priv);

	if (!crypto_params)
		return NULL;

	*frm++ = WLAN_ELEMID_VENDOR;
	*frm++ = 0;
	WLAN_CRYPTO_ADDSELECTOR(frm, WPA_TYPE_OUI);
	WLAN_CRYPTO_ADDSHORT(frm, WPA_VERSION);


	/* multicast cipher */
	if (MCIPHER_IS_TKIP(crypto_params)) {
		WLAN_CRYPTO_ADDSELECTOR(frm,
					wlan_crypto_wpa_cipher_to_suite(
						WLAN_CRYPTO_CIPHER_TKIP));
	} else if (MCIPHER_IS_CCMP128(crypto_params)) {
		WLAN_CRYPTO_ADDSELECTOR(frm,
					wlan_crypto_wpa_cipher_to_suite(
						WLAN_CRYPTO_CIPHER_AES_CCM));
	}
	/* unicast cipher list */
	selcnt = frm;
	WLAN_CRYPTO_ADDSHORT(frm, 0);
	/* do not use CCMP unicast cipher in WPA mode */
	if (UCIPHER_IS_TKIP(crypto_params)) {
		selcnt[0]++;
		WLAN_CRYPTO_ADDSELECTOR(frm,
			 wlan_crypto_wpa_cipher_to_suite(
						WLAN_CRYPTO_CIPHER_TKIP));
	}
	if (UCIPHER_IS_CCMP128(crypto_params)) {
		selcnt[0]++;
		WLAN_CRYPTO_ADDSELECTOR(frm,
			wlan_crypto_wpa_cipher_to_suite(
						WLAN_CRYPTO_CIPHER_AES_CCM));
	}

	/* authenticator selector list */
	selcnt = frm;
	WLAN_CRYPTO_ADDSHORT(frm, 0);

	if (HAS_KEY_MGMT(crypto_params, WLAN_CRYPTO_KEY_MGMT_IEEE8021X)) {
		selcnt[0]++;
		WLAN_CRYPTO_ADDSELECTOR(frm,
			wlan_crypto_wpa_keymgmt_to_suite(
					WLAN_CRYPTO_KEY_MGMT_IEEE8021X));
	} else if (HAS_KEY_MGMT(crypto_params, WLAN_CRYPTO_KEY_MGMT_PSK)) {
		selcnt[0]++;
		WLAN_CRYPTO_ADDSELECTOR(frm,
			wlan_crypto_wpa_keymgmt_to_suite(
						WLAN_CRYPTO_KEY_MGMT_PSK));
	} else if (HAS_KEY_MGMT(crypto_params, WLAN_CRYPTO_KEY_MGMT_CCKM)) {
		selcnt[0]++;
		WLAN_CRYPTO_ADDSELECTOR(frm,
			wlan_crypto_wpa_keymgmt_to_suite(
						WLAN_CRYPTO_KEY_MGMT_CCKM));
	} else {
		selcnt[0]++;
		WLAN_CRYPTO_ADDSELECTOR(frm,
			wlan_crypto_wpa_keymgmt_to_suite(
						WLAN_CRYPTO_KEY_MGMT_NONE));
	}
	/* calculate element length */
	iebuf[1] = frm - iebuf - 2;

	return frm;
}

/**
 * wlan_crypto_build_rsnie - called by mlme to build rsnie
 * @vdev: vdev
 * @iebuf: ie buffer
 *
 * This function gets called by mlme to build rsnie from given vdev
 *
 * Return: end of buffer
 */
uint8_t *wlan_crypto_build_rsnie(struct wlan_objmgr_vdev *vdev,
				 uint8_t *iebuf){
	uint8_t *frm = iebuf;
	uint8_t *selcnt;
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;

	if (!frm) {
		return NULL;
	}

	crypto_params = wlan_crypto_vdev_get_comp_params(vdev, &crypto_priv);

	if (!crypto_params) {
		return NULL;
	}

	*frm++ = WLAN_ELEMID_RSN;
	*frm++ = 0;
	WLAN_CRYPTO_ADDSHORT(frm, RSN_VERSION);


	/* multicast cipher */
	if (MCIPHER_IS_TKIP(crypto_params)) {
		WLAN_CRYPTO_ADDSELECTOR(frm,
					wlan_crypto_rsn_cipher_to_suite(
					WLAN_CRYPTO_CIPHER_TKIP));
	} else if (MCIPHER_IS_CCMP128(crypto_params)) {
		WLAN_CRYPTO_ADDSELECTOR(frm,
					wlan_crypto_rsn_cipher_to_suite(
					WLAN_CRYPTO_CIPHER_AES_CCM));
	} else if (MCIPHER_IS_CCMP256(crypto_params)) {
		WLAN_CRYPTO_ADDSELECTOR(frm,
					wlan_crypto_rsn_cipher_to_suite(
					WLAN_CRYPTO_CIPHER_AES_CCM_256));
	} else if (MCIPHER_IS_GCMP128(crypto_params)) {
		WLAN_CRYPTO_ADDSELECTOR(frm,
					wlan_crypto_rsn_cipher_to_suite(
					WLAN_CRYPTO_CIPHER_AES_GCM));
	} else if (MCIPHER_IS_GCMP256(crypto_params)) {
		WLAN_CRYPTO_ADDSELECTOR(frm,
					wlan_crypto_rsn_cipher_to_suite(
					WLAN_CRYPTO_CIPHER_AES_GCM_256));
	}

	/* unicast cipher list */
	selcnt = frm;
	WLAN_CRYPTO_ADDSHORT(frm, 0);
	/* do not use CCMP unicast cipher in WPA mode */
	if (UCIPHER_IS_TKIP(crypto_params)) {
		selcnt[0]++;
		WLAN_CRYPTO_ADDSELECTOR(frm,
					wlan_crypto_rsn_cipher_to_suite(
						WLAN_CRYPTO_CIPHER_TKIP));
	}
	if (UCIPHER_IS_CCMP128(crypto_params)) {
		selcnt[0]++;
		WLAN_CRYPTO_ADDSELECTOR(frm,
					wlan_crypto_rsn_cipher_to_suite(
						WLAN_CRYPTO_CIPHER_AES_CCM));
	}
	if (UCIPHER_IS_CCMP256(crypto_params)) {
		selcnt[0]++;
		WLAN_CRYPTO_ADDSELECTOR(frm,
			wlan_crypto_rsn_cipher_to_suite(
					WLAN_CRYPTO_CIPHER_AES_CCM_256));
	}

	if (UCIPHER_IS_GCMP128(crypto_params)) {
		selcnt[0]++;
		WLAN_CRYPTO_ADDSELECTOR(frm,
			 wlan_crypto_rsn_cipher_to_suite(
					WLAN_CRYPTO_CIPHER_AES_GCM));
	}
	if (UCIPHER_IS_GCMP256(crypto_params)) {
		selcnt[0]++;
		WLAN_CRYPTO_ADDSELECTOR(frm,
			wlan_crypto_rsn_cipher_to_suite(
					WLAN_CRYPTO_CIPHER_AES_GCM_256));
	}


	/* authenticator selector list */
	selcnt = frm;
	WLAN_CRYPTO_ADDSHORT(frm, 0);
	if (HAS_KEY_MGMT(crypto_params, WLAN_CRYPTO_KEY_MGMT_CCKM)) {
		selcnt[0]++;
		WLAN_CRYPTO_ADDSELECTOR(frm,
			wlan_crypto_rsn_keymgmt_to_suite(
					WLAN_CRYPTO_KEY_MGMT_CCKM));
	} else {
		if (HAS_KEY_MGMT(crypto_params, WLAN_CRYPTO_KEY_MGMT_PSK)) {
			selcnt[0]++;
			WLAN_CRYPTO_ADDSELECTOR(frm,
				wlan_crypto_rsn_keymgmt_to_suite(
					WLAN_CRYPTO_KEY_MGMT_PSK));
		}
		if (HAS_KEY_MGMT(crypto_params,
					WLAN_CRYPTO_KEY_MGMT_IEEE8021X)) {
			selcnt[0]++;
			WLAN_CRYPTO_ADDSELECTOR(frm,
				wlan_crypto_rsn_keymgmt_to_suite(
					WLAN_CRYPTO_KEY_MGMT_IEEE8021X));
		}
		if (HAS_KEY_MGMT(crypto_params,
					WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X)) {
			selcnt[0]++;
			WLAN_CRYPTO_ADDSELECTOR(frm,
				wlan_crypto_rsn_keymgmt_to_suite(
					WLAN_CRYPTO_KEY_MGMT_FT_IEEE8021X));
		}
		if (HAS_KEY_MGMT(crypto_params, WLAN_CRYPTO_KEY_MGMT_FT_PSK)) {
			selcnt[0]++;
			WLAN_CRYPTO_ADDSELECTOR(frm,
				wlan_crypto_rsn_keymgmt_to_suite(
					WLAN_CRYPTO_KEY_MGMT_FT_PSK));
		}
		if (HAS_KEY_MGMT(crypto_params,
				WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SHA256)) {
			selcnt[0]++;
			WLAN_CRYPTO_ADDSELECTOR(frm,
				wlan_crypto_rsn_keymgmt_to_suite(
					WLAN_CRYPTO_KEY_MGMT_IEEE8021X_SHA256));
		}
		if (HAS_KEY_MGMT(crypto_params,
					WLAN_CRYPTO_KEY_MGMT_PSK_SHA256)) {
			selcnt[0]++;
			WLAN_CRYPTO_ADDSELECTOR(frm,
				wlan_crypto_rsn_keymgmt_to_suite(
					WLAN_CRYPTO_KEY_MGMT_PSK_SHA256));
		}
		if (HAS_KEY_MGMT(crypto_params, WLAN_CRYPTO_KEY_MGMT_OSEN)) {
			selcnt[0]++;
			WLAN_CRYPTO_ADDSELECTOR(frm,
				wlan_crypto_rsn_keymgmt_to_suite(
					WLAN_CRYPTO_KEY_MGMT_OSEN));
		}
	}

	WLAN_CRYPTO_ADDSHORT(frm, crypto_params->rsn_caps);
	/* optional capabilities */
	if (crypto_params->rsn_caps & WLAN_CRYPTO_RSN_CAP_MFP_ENABLED) {
		/* PMK list */
		WLAN_CRYPTO_ADDSHORT(frm, 0);
		if (HAS_MGMT_CIPHER(crypto_params,
						WLAN_CRYPTO_CIPHER_AES_CMAC)) {
			WLAN_CRYPTO_ADDSELECTOR(frm,
				 wlan_crypto_rsn_cipher_to_suite(
						WLAN_CRYPTO_CIPHER_AES_CMAC));
		}
		if (HAS_MGMT_CIPHER(crypto_params,
						WLAN_CRYPTO_CIPHER_AES_GMAC)) {
			WLAN_CRYPTO_ADDSELECTOR(frm,
				 wlan_crypto_rsn_cipher_to_suite(
						WLAN_CRYPTO_CIPHER_AES_GMAC));
		}
		if (HAS_MGMT_CIPHER(crypto_params,
					 WLAN_CRYPTO_CIPHER_AES_CMAC_256)) {
			WLAN_CRYPTO_ADDSELECTOR(frm,
				 wlan_crypto_rsn_cipher_to_suite(
					WLAN_CRYPTO_CIPHER_AES_CMAC_256));
		}

		if (HAS_MGMT_CIPHER(crypto_params,
					WLAN_CRYPTO_CIPHER_AES_GMAC_256)) {
			WLAN_CRYPTO_ADDSELECTOR(frm,
				 wlan_crypto_rsn_cipher_to_suite(
					WLAN_CRYPTO_CIPHER_AES_GMAC_256));
		}
	}

	/* calculate element length */
	iebuf[1] = frm - iebuf - 2;

	return frm;
}

bool wlan_crypto_rsn_info(struct wlan_objmgr_vdev *vdev,
				struct wlan_crypto_params *crypto_params){
	struct wlan_crypto_params *my_crypto_params;
	my_crypto_params = wlan_crypto_vdev_get_crypto_params(vdev);

	if (!my_crypto_params)
		return false;
	/*
	 * Check peer's pairwise ciphers.
	 * At least one must match with our unicast cipher
	 */
	if (!UCAST_CIPHER_MATCH(crypto_params, my_crypto_params))
		return false;
	/*
	 * Check peer's group cipher is our enabled multicast cipher.
	 */
	if (!MCAST_CIPHER_MATCH(crypto_params, my_crypto_params))
		return false;
	/*
	 * Check peer's key management class set (PSK or UNSPEC)
	 */
	if (!KEY_MGMTSET_MATCH(crypto_params, my_crypto_params))
		return false;

	return true;
}

/*
 * Convert an WAPI CIPHER suite to to an internal code.
 */
static int32_t wlan_crypto_wapi_suite_to_cipher(uint8_t *sel)
{
	uint32_t w = LE_READ_4(sel);
	int32_t status = -1;

	switch (w) {
	case (WLAN_WAPI_SEL(WLAN_CRYPTO_WAPI_SMS4_CIPHER)):
		return WLAN_CRYPTO_CIPHER_WAPI_SMS4;
	}

	return status;
}

/*
 * Convert an WAPI key management/authentication algorithm
 * to an internal code.
 */
static int32_t wlan_crypto_wapi_keymgmt(u_int8_t *sel)
{
	uint32_t w = LE_READ_4(sel);
	int32_t status = -1;

	switch (w) {
	case (WLAN_WAPI_SEL(WLAN_WAI_PSK)):
		return WLAN_CRYPTO_KEY_MGMT_WAPI_PSK;
	case (WLAN_WAPI_SEL(WLAN_WAI_CERT_OR_SMS4)):
		return WLAN_CRYPTO_KEY_MGMT_WAPI_CERT;
	}

	return status;
}
/**
 * wlan_crypto_wapiie_check - called by mlme to check the wapiie
 * @crypto params: crypto params
 * @iebuf: ie buffer
 *
 * This function gets called by mlme to check the contents of wapi is
 * matching with given crypto params
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS wlan_crypto_wapiie_check(struct wlan_crypto_params *crypto_params,
					uint8_t *frm)
{
	uint8_t len = frm[1];
	int32_t w;
	int n;

	/*
	 * Check the length once for fixed parts: OUI, type,
	 * version, mcast cipher, and 2 selector counts.
	 * Other, variable-length data, must be checked separately.
	 */
	RESET_AUTHMODE(crypto_params);
	SET_AUTHMODE(crypto_params, WLAN_CRYPTO_AUTH_WAPI);

	if (len < WLAN_CRYPTO_WAPI_IE_LEN)
		return QDF_STATUS_E_INVAL;


	frm += 2;

	w = LE_READ_2(frm);
	frm += 2, len -= 2;
	if (w != WAPI_VERSION)
		return QDF_STATUS_E_INVAL;

	n = LE_READ_2(frm);
	frm += 2, len -= 2;
	if (len < n*4+2)
		return QDF_STATUS_E_INVAL;

	RESET_KEY_MGMT(crypto_params);
	for (; n > 0; n--) {
		w = wlan_crypto_wapi_keymgmt(frm);
		if (w < 0)
			return QDF_STATUS_E_INVAL;

		SET_KEY_MGMT(crypto_params, w);
		frm += 4, len -= 4;
	}

	/* unicast ciphers */
	n = LE_READ_2(frm);
	frm += 2, len -= 2;
	if (len < n*4+2)
		return QDF_STATUS_E_INVAL;

	RESET_UCAST_CIPHERS(crypto_params);
	for (; n > 0; n--) {
		w = wlan_crypto_wapi_suite_to_cipher(frm);
		if (w < 0)
			return QDF_STATUS_E_INVAL;
		SET_UCAST_CIPHER(crypto_params, w);
		frm += 4, len -= 4;
	}

	if (!crypto_params->ucastcipherset)
		return QDF_STATUS_E_INVAL;

	/* multicast/group cipher */
	RESET_MCAST_CIPHERS(crypto_params);
	w = wlan_crypto_wapi_suite_to_cipher(frm);

	if (w < 0)
		return QDF_STATUS_E_INVAL;

	SET_MCAST_CIPHER(crypto_params, w);
	frm += 4, len -= 4;

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_crypto_build_wapiie - called by mlme to build wapi ie
 * @vdev: vdev
 * @iebuf: ie buffer
 *
 * This function gets called by mlme to build wapi ie from given vdev
 *
 * Return: end of buffer
 */
uint8_t *wlan_crypto_build_wapiie(struct wlan_objmgr_vdev *vdev,
				uint8_t *iebuf)
{
	uint8_t *frm;
	uint8_t *selcnt;
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;

	frm = iebuf;
	if (!frm) {
		qdf_print("%s[%d] ie buffer NULL\n", __func__, __LINE__);
		return NULL;
	}

	crypto_params = wlan_crypto_vdev_get_comp_params(vdev, &crypto_priv);

	if (!crypto_params) {
		qdf_print("%s[%d] crypto_params NULL\n", __func__, __LINE__);
		return NULL;
	}

	*frm++ = WLAN_ELEMID_WAPI;
	*frm++ = 0;

	WLAN_CRYPTO_ADDSHORT(frm, WAPI_VERSION);

	/* authenticator selector list */
	selcnt = frm;
	WLAN_CRYPTO_ADDSHORT(frm, 0);

	if (HAS_KEY_MGMT(crypto_params, WLAN_CRYPTO_KEY_MGMT_WAPI_PSK)) {
		selcnt[0]++;
		WLAN_CRYPTO_ADDSELECTOR(frm,
				WLAN_WAPI_SEL(WLAN_WAI_PSK));
	}

	if (HAS_KEY_MGMT(crypto_params, WLAN_CRYPTO_KEY_MGMT_WAPI_CERT)) {
		selcnt[0]++;
		WLAN_CRYPTO_ADDSELECTOR(frm,
				WLAN_WAPI_SEL(WLAN_WAI_CERT_OR_SMS4));
	}

	/* unicast cipher list */
	selcnt = frm;
	WLAN_CRYPTO_ADDSHORT(frm, 0);

	if (UCIPHER_IS_SMS4(crypto_params)) {
		selcnt[0]++;
		WLAN_CRYPTO_ADDSELECTOR(frm,
				WLAN_WAPI_SEL(WLAN_CRYPTO_WAPI_SMS4_CIPHER));
	}

	WLAN_CRYPTO_ADDSELECTOR(frm,
				WLAN_WAPI_SEL(WLAN_CRYPTO_WAPI_SMS4_CIPHER));

	/* optional capabilities */
	WLAN_CRYPTO_ADDSHORT(frm, crypto_params->rsn_caps);

	/* calculate element length */
	iebuf[1] = frm - iebuf - 2;

	return frm;

}

/**
 * wlan_crypto_pn_check - called by data patch for PN check
 * @vdev: vdev
 * @wbuf: wbuf
 *
 * This function gets called by data patch for PN check
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_crypto_pn_check(struct wlan_objmgr_vdev *vdev,
				qdf_nbuf_t wbuf){
	/* Need to check is there real requirement for this function
	 * as PN check is already handled in decap function.
	 */
	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_crypto_vdev_get_crypto_params - called by mlme to get crypto params
 * @vdev:vdev
 *
 * This function gets called by mlme to get crypto params
 *
 * Return: wlan_crypto_params or NULL in case of failure
 */
struct wlan_crypto_params *wlan_crypto_vdev_get_crypto_params(
						struct wlan_objmgr_vdev *vdev){
	struct wlan_crypto_comp_priv *crypto_priv;

	return wlan_crypto_vdev_get_comp_params(vdev, &crypto_priv);
}

/**
 * wlan_crypto_peer_get_crypto_params - called by mlme to get crypto params
 * @peer:peer
 *
 * This function gets called by mlme to get crypto params
 *
 * Return: wlan_crypto_params or NULL in case of failure
 */
struct wlan_crypto_params *wlan_crypto_peer_get_crypto_params(
						struct wlan_objmgr_peer *peer){
	struct wlan_crypto_comp_priv *crypto_priv;

	return wlan_crypto_peer_get_comp_params(peer, &crypto_priv);
}


QDF_STATUS wlan_crypto_set_peer_wep_keys(struct wlan_objmgr_vdev *vdev,
					struct wlan_objmgr_peer *peer)
{
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_comp_priv *sta_crypto_priv;
	struct wlan_crypto_params *crypto_params;
	struct wlan_crypto_key *key;
	struct wlan_crypto_key *sta_key;
	struct wlan_crypto_cipher *cipher_table;
	struct wlan_objmgr_psoc *psoc;
	uint8_t *mac_addr;
	int i;
	enum QDF_OPMODE opmode;

	if (!vdev)
		return QDF_STATUS_E_NULL_VALUE;

	if (!peer) {
		qdf_print("%s[%d] peer NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}

	opmode = wlan_vdev_mlme_get_opmode(vdev);
	psoc = wlan_vdev_get_psoc(vdev);

	if (!psoc) {
		qdf_print("%s[%d] psoc NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_peer_obj_lock(peer);
	mac_addr = wlan_peer_get_macaddr(peer);
	wlan_peer_obj_unlock(peer);

	crypto_params = wlan_crypto_vdev_get_comp_params(vdev,
							&crypto_priv);
	if (crypto_priv == NULL) {
		qdf_print("%s[%d] crypto_priv NULL\n", __func__, __LINE__);
		return QDF_STATUS_E_NULL_VALUE;
	}

	/* push only valid static WEP keys from vap */
	if (AUTH_IS_8021X(crypto_params))
		return QDF_STATUS_E_INVAL;

	if (opmode == QDF_STA_MODE) {
		peer = wlan_vdev_get_bsspeer(vdev);
		if (!peer) {
			qdf_print("%s[%d] peer NULL\n", __func__, __LINE__);
			return QDF_STATUS_E_INVAL;
		}
	}

	wlan_crypto_peer_get_comp_params(peer, &sta_crypto_priv);
	if (sta_crypto_priv == NULL) {
		qdf_print("%s[%d] sta priv is null\n", __func__, __LINE__);
		return QDF_STATUS_E_INVAL;
	}

	for (i = 0; i < WLAN_CRYPTO_MAXKEYIDX; i++) {
		if (crypto_priv->key[i]) {
			key = crypto_priv->key[i];
			if (!key || !key->valid)
				continue;

			cipher_table = (struct wlan_crypto_cipher *)
							key->cipher_table;

			if (cipher_table->cipher == WLAN_CRYPTO_CIPHER_WEP) {
				sta_key = qdf_mem_malloc(
						sizeof(struct wlan_crypto_key));
				if (!sta_key) {
					qdf_print("%s[%d] key alloc failed\n",
							__func__, __LINE__);
					return QDF_STATUS_E_NOMEM;
				}
				sta_crypto_priv->key[i] = sta_key;
				qdf_mem_copy(sta_key, key,
						sizeof(struct wlan_crypto_key));

				sta_key->flags &= ~WLAN_CRYPTO_KEY_DEFAULT;

				if (crypto_priv->def_tx_keyid == i) {
					sta_key->flags
						|= WLAN_CRYPTO_KEY_DEFAULT;
					sta_crypto_priv->def_tx_keyid =
						crypto_priv->def_tx_keyid;
				}
				/* setting the broadcast/multicast key for sta*/
				if (opmode == QDF_STA_MODE ||
						opmode == QDF_IBSS_MODE){
					if (WLAN_CRYPTO_TX_OPS_SETKEY(psoc)) {
						WLAN_CRYPTO_TX_OPS_SETKEY(psoc)(
							vdev, sta_key, mac_addr,
							cipher_table->cipher);
					}
				}

				/* setting unicast key */
				sta_key->flags &= ~WLAN_CRYPTO_KEY_GROUP;
				if (WLAN_CRYPTO_TX_OPS_SETKEY(psoc)) {
					WLAN_CRYPTO_TX_OPS_SETKEY(psoc)(vdev,
						sta_key, mac_addr,
						cipher_table->cipher);
				}
			}
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_crypto_register_crypto_rx_ops - set crypto_rx_ops
 * @crypto_rx_ops: crypto_rx_ops
 *
 * This function gets called by object manger to register crypto rx ops.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_crypto_register_crypto_rx_ops(
			struct wlan_lmac_if_crypto_rx_ops *crypto_rx_ops){
	crypto_rx_ops->crypto_encap      = wlan_crypto_encap;
	crypto_rx_ops->crypto_decap      = wlan_crypto_decap;
	crypto_rx_ops->crypto_enmic      = wlan_crypto_enmic;
	crypto_rx_ops->crypto_demic      = wlan_crypto_demic;
	crypto_rx_ops->set_peer_wep_keys = wlan_crypto_set_peer_wep_keys;

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_crypto_get_crypto_rx_ops - get crypto_rx_ops from psoc
 * @psoc: psoc
 *
 * This function gets called by umac to get the crypto_rx_ops
 *
 * Return: crypto_rx_ops
 */
struct wlan_lmac_if_crypto_rx_ops *wlan_crypto_get_crypto_rx_ops(
					struct wlan_objmgr_psoc *psoc)
{

	return &(psoc->soc_cb.rx_ops.crypto_rx_ops);
}
qdf_export_symbol(wlan_crypto_get_crypto_rx_ops);

/**
 * wlan_crypto_vdev_has_auth_mode - check authmode for vdev
 * @vdev: vdev
 * @authvalue: authvalue to be checked
 *
 * This function check is authvalue passed is set in vdev or not
 *
 * Return: true or false
 */
bool wlan_crypto_vdev_has_auth_mode(struct wlan_objmgr_vdev *vdev,
					wlan_crypto_auth_mode authvalue)
{
	return wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_AUTH_MODE)
			& authvalue;
}
qdf_export_symbol(wlan_crypto_vdev_has_auth_mode);

/**
 * wlan_crypto_peer_has_auth_mode - check authmode for peer
 * @peer: peer
 * @authvalue: authvalue to be checked
 *
 * This function check is authvalue passed is set in peer or not
 *
 * Return: true or false
 */
bool wlan_crypto_peer_has_auth_mode(struct wlan_objmgr_peer *peer,
					wlan_crypto_auth_mode authvalue)
{
	return wlan_crypto_get_peer_param(peer, WLAN_CRYPTO_PARAM_AUTH_MODE)
			& authvalue;
}
qdf_export_symbol(wlan_crypto_peer_has_auth_mode);

/**
 * wlan_crypto_vdev_has_ucastcipher - check ucastcipher for vdev
 * @vdev: vdev
 * @ucastcipher: ucastcipher to be checked
 *
 * This function check is ucastcipher passed is set in vdev or not
 *
 * Return: true or false
 */
bool wlan_crypto_vdev_has_ucastcipher(struct wlan_objmgr_vdev *vdev,
					wlan_crypto_cipher_type ucastcipher)
{
	return wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_UCAST_CIPHER)
			& ucastcipher;
}
qdf_export_symbol(wlan_crypto_vdev_has_ucastcipher);

/**
 * wlan_crypto_peer_has_ucastcipher - check ucastcipher for peer
 * @peer: peer
 * @ucastcipher: ucastcipher to be checked
 *
 * This function check is ucastcipher passed is set in peer or not
 *
 * Return: true or false
 */
bool wlan_crypto_peer_has_ucastcipher(struct wlan_objmgr_peer *peer,
					wlan_crypto_cipher_type ucastcipher)
{
	return wlan_crypto_get_peer_param(peer, WLAN_CRYPTO_PARAM_UCAST_CIPHER)
			& ucastcipher;
}
qdf_export_symbol(wlan_crypto_peer_has_ucastcipher);

/**
 * wlan_crypto_vdev_has_mcastcipher - check mcastcipher for vdev
 * @vdev: vdev
 * @mcastcipher: mcastcipher to be checked
 *
 * This function check is mcastcipher passed is set in vdev or not
 *
 * Return: true or false
 */
bool wlan_crypto_vdev_has_mcastcipher(struct wlan_objmgr_vdev *vdev,
					wlan_crypto_cipher_type mcastcipher)
{
	return wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_MCAST_CIPHER)
			& mcastcipher;
}
qdf_export_symbol(wlan_crypto_vdev_has_mcastcipher);

/**
 * wlan_crypto_peer_has_mcastcipher - check mcastcipher for peer
 * @peer: peer
 * @mcastcipher: mcastcipher to be checked
 *
 * This function check is mcastcipher passed is set in peer or not
 *
 * Return: true or false
 */
bool wlan_crypto_peer_has_mcastcipher(struct wlan_objmgr_peer *peer,
					wlan_crypto_cipher_type mcastcipher)
{
	return wlan_crypto_get_peer_param(peer, WLAN_CRYPTO_PARAM_UCAST_CIPHER)
			& mcastcipher;
}
qdf_export_symbol(wlan_crypto_peer_has_mcastcipher);

uint8_t wlan_crypto_get_peer_fils_aead(struct wlan_objmgr_peer *peer)
{
	struct wlan_crypto_comp_priv *crypto_priv = NULL;

	if (!peer) {
		qdf_print(FL("Invalid Input\n"));
		return 0;
	}

	crypto_priv = wlan_get_peer_crypto_obj(peer);
	if (!crypto_priv) {
		qdf_print(FL("crypto_priv NULL\n"));
		return 0;
	}

	return crypto_priv->fils_aead_set;
}

void
wlan_crypto_set_peer_fils_aead(struct wlan_objmgr_peer *peer, uint8_t value)
{
	struct wlan_crypto_comp_priv *crypto_priv = NULL;

	if (!peer) {
		qdf_print(FL("Invalid Input\n"));
		return;
	}

	crypto_priv = wlan_get_peer_crypto_obj(peer);
	if (!crypto_priv) {
		qdf_print(FL("crypto_priv NULL\n"));
		return;
	}

	crypto_priv->fils_aead_set = value;
}

/**
 * wlan_crypto_get_key_header - get header length
 * @key: key
 *
 * This function gets header length based on keytype
 *
 * Return: header length
 */
uint8_t wlan_crypto_get_key_header(struct wlan_crypto_key *key)
{
	struct wlan_crypto_cipher *cipher_table;

	cipher_table = (struct wlan_crypto_cipher *)key->cipher_table;
	if (cipher_table)
		return cipher_table->header;
	else
		return 0;
}

qdf_export_symbol(wlan_crypto_get_key_header);

/**
 * wlan_crypto_get_key_trailer - get cipher trailer length
 * @key: key
 *
 * This function gets cipher trailer length based on keytype
 *
 * Return: cipher trailer length
 */
uint8_t wlan_crypto_get_key_trailer(struct wlan_crypto_key *key)
{
	struct wlan_crypto_cipher *cipher_table;

	cipher_table = (struct wlan_crypto_cipher *)key->cipher_table;
	if (cipher_table)
		return cipher_table->trailer;
	else
		return 0;
}

qdf_export_symbol(wlan_crypto_get_key_trailer);

/**
 * wlan_crypto_get_key_miclen - get cipher miclen length
 * @key: key
 *
 * This function gets cipher miclen length based on keytype
 *
 * Return: cipher miclen length
 */
uint8_t wlan_crypto_get_key_miclen(struct wlan_crypto_key *key)
{
	struct wlan_crypto_cipher *cipher_table;

	cipher_table = (struct wlan_crypto_cipher *)key->cipher_table;
	if (cipher_table)
		return cipher_table->miclen;
	else
		return 0;
}

qdf_export_symbol(wlan_crypto_get_key_miclen);

/**
 * wlan_crypto_get_keyid - get keyid from frame
 * @data: frame
 *
 * This function parse frame and returns keyid
 *
 * Return: keyid
 */
uint16_t wlan_crypto_get_keyid(uint8_t *data, int hdrlen)
{
	struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)data;
	uint8_t *iv;

	if (hdr->frame_control[1] & WLAN_FC1_ISWEP) {
		iv = data + hdrlen;
		/*
		 * iv[3] is the Key ID octet in the CCMP/TKIP/WEP headers
		 * Bits 6–7 of the Key ID octet are for the Key ID subfield
		 */
		return ((iv[3] >> 6) & 0x3);
	} else {
		return WLAN_CRYPTO_KEYIX_NONE;
	}
}

qdf_export_symbol(wlan_crypto_get_keyid);

/**
 * crypto_plumb_peer_keys - called during radio reset
 * @vdev: vdev
 * @object: peer
 * @arg: psoc
 *
 * Restore unicast and persta hardware keys
 *
 * Return: void
 */
static void crypto_plumb_peer_keys(struct wlan_objmgr_vdev *vdev,
				   void *object, void *arg) {
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)object;
	struct wlan_objmgr_psoc *psoc = (struct wlan_objmgr_psoc *)arg;
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;
	struct wlan_crypto_key *key = NULL;
	int i;

	if ((NULL == peer) || (NULL == vdev) || (NULL == psoc)) {
		QDF_TRACE(QDF_MODULE_ID_CRYPTO, QDF_TRACE_LEVEL_ERROR,
			  "%s[%d] Peer or vdev or psoc objects are null!\n",
			  __func__, __LINE__);
		return;
	}

	crypto_params = wlan_crypto_peer_get_comp_params(peer,
							 &crypto_priv);

	if (!crypto_priv) {
		QDF_TRACE(QDF_MODULE_ID_CRYPTO, QDF_TRACE_LEVEL_ERROR,
			  "%s[%d] crypto_priv NULL\n",
			  __func__, __LINE__);
		return;
	}

	for (i = 0; i < WLAN_CRYPTO_MAXKEYIDX; i++) {
		key = crypto_priv->key[i];
		if (key && key->valid) {
			if (WLAN_CRYPTO_TX_OPS_SETKEY(psoc)) {
				WLAN_CRYPTO_TX_OPS_SETKEY(psoc)
					(
					 vdev,
					 key,
					 wlan_peer_get_macaddr(peer),
					 wlan_crypto_get_key_type(key)
					);
			}
		}
	}
}

/**
 * wlan_crypto_restore_keys - called during radio reset
 * @vdev: vdev
 *
 * Clear and restore keycache, needed for some DA chipsets which put
 * random values in keycache when phy reset is triggered
 *
 * Return: void
 */
void wlan_crypto_restore_keys(struct wlan_objmgr_vdev *vdev)
{
	int i;
	struct wlan_crypto_comp_priv *crypto_priv;
	struct wlan_crypto_params *crypto_params;
	struct wlan_crypto_key *key;
	uint8_t macaddr[WLAN_ALEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;

	pdev = wlan_vdev_get_pdev(vdev);
	psoc = wlan_vdev_get_psoc(vdev);
	if (NULL == pdev) {
		QDF_TRACE(QDF_MODULE_ID_CRYPTO, QDF_TRACE_LEVEL_ERROR,
			  "%s[%d] pdev is NULL\n",
			  __func__, __LINE__);
		return;
	}
	if (NULL == psoc) {
		QDF_TRACE(QDF_MODULE_ID_CRYPTO, QDF_TRACE_LEVEL_ERROR,
			  "%s[%d] psoc is NULL\n",
			  __func__, __LINE__);
		return;
	}

	/* TBD: QWRAP key restore*/
	/* crypto is on */
	if (wlan_vdev_mlme_feat_cap_get(vdev, WLAN_VDEV_F_PRIVACY)) {
		/* restore static shared keys */
		for (i = 0; i < WLAN_CRYPTO_MAXKEYIDX; i++) {
			crypto_params = wlan_crypto_vdev_get_comp_params
				(
				 vdev,
				 &crypto_priv
				);
			if (!crypto_priv) {
				QDF_TRACE(QDF_MODULE_ID_CRYPTO,
					  QDF_TRACE_LEVEL_ERROR,
					  "%s[%d] crypto_priv is NULL\n",
					  __func__, __LINE__);
				return;
			}
			key = crypto_priv->key[i];
			if (key && key->valid) {
				if (WLAN_CRYPTO_TX_OPS_SETKEY(psoc)) {
					WLAN_CRYPTO_TX_OPS_SETKEY(psoc)
						(
						 vdev,
						 key,
						 macaddr,
						 wlan_crypto_get_key_type(key)
						 );
				}
			}
		}

		wlan_objmgr_iterate_peerobj_list(vdev,
						 crypto_plumb_peer_keys,
						 psoc,
						 WLAN_CRYPTO_ID);
	}
}
