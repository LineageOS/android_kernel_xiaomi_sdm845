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

#include <sir_common.h>
#include <ani_global.h>
#include <csr_inside_api.h>
#include <csr_neighbor_roam.h>
#include <sir_api.h>

/* Initialize the FT context. */
void sme_ft_open(tHalHandle hHal, uint32_t sessionId)
{
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (NULL != pSession) {
		/* Clean up the context */
		qdf_mem_zero(&pSession->ftSmeContext, sizeof(tftSMEContext));

		pSession->ftSmeContext.pUsrCtx =
			qdf_mem_malloc(sizeof(tFTRoamCallbackUsrCtx));

		if (NULL == pSession->ftSmeContext.pUsrCtx) {
			sme_err("Memory allocation failure");
			return;
		}
		pSession->ftSmeContext.pUsrCtx->pMac = pMac;
		pSession->ftSmeContext.pUsrCtx->sessionId = sessionId;

		status =
			qdf_mc_timer_init(&pSession->ftSmeContext.
					  preAuthReassocIntvlTimer,
					  QDF_TIMER_TYPE_SW,
				sme_preauth_reassoc_intvl_timer_callback,
					(void *)pSession->ftSmeContext.pUsrCtx);

		if (QDF_STATUS_SUCCESS != status) {
			sme_err("Preauth Reassoc interval Timer allocation failed");
			qdf_mem_free(pSession->ftSmeContext.pUsrCtx);
			pSession->ftSmeContext.pUsrCtx = NULL;
			return;
		}
	}
}

/* Cleanup the SME FT Global context. */
void sme_ft_close(tHalHandle hHal, uint32_t sessionId)
{
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
	struct csr_roam_session *pSession = NULL;

	/* Clear the FT Context */
	sme_ft_reset(hHal, sessionId);

	pSession = CSR_GET_SESSION(pMac, sessionId);
	if (NULL != pSession) {
		/* check if the timer is running */
		if (QDF_TIMER_STATE_RUNNING ==
		    qdf_mc_timer_get_current_state(&pSession->ftSmeContext.
						   preAuthReassocIntvlTimer)) {
			qdf_mc_timer_stop(&pSession->ftSmeContext.
					  preAuthReassocIntvlTimer);
		}

		qdf_mc_timer_destroy(&pSession->ftSmeContext.
					preAuthReassocIntvlTimer);

		if (pSession->ftSmeContext.pUsrCtx != NULL) {
			qdf_mem_free(pSession->ftSmeContext.pUsrCtx);
			pSession->ftSmeContext.pUsrCtx = NULL;
		}
	}
}

void sme_set_ft_pre_auth_state(tHalHandle hHal, uint32_t sessionId, bool state)
{
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (pSession)
		pSession->ftSmeContext.setFTPreAuthState = state;
}

bool sme_get_ft_pre_auth_state(tHalHandle hHal, uint32_t sessionId)
{
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (pSession)
		return pSession->ftSmeContext.setFTPreAuthState;

	return false;
}

/**
 * sme_set_ft_ies() - to set FT IEs
 * @hal_ptr: pointer to HAL
 * @session_id: sme session id
 * @ft_ies: pointer to FT IEs
 * @ft_ies_length: length of FT IEs
 *
 * Each time the supplicant sends down the FT IEs to the driver. This function
 * is called in SME. This function packages and sends the FT IEs to PE.
 *
 * Return: none
 */
void sme_set_ft_ies(tHalHandle hal_ptr, uint32_t session_id,
		const uint8_t *ft_ies, uint16_t ft_ies_length)
{
	tpAniSirGlobal mac_ctx = PMAC_STRUCT(hal_ptr);
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (NULL == session || NULL == ft_ies) {
		sme_err("ft ies or session is NULL");
		return;
	}

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (!(QDF_IS_STATUS_SUCCESS(status)))
		return;

	sme_debug("FT IEs Req is received in state %d",
		  session->ftSmeContext.FTState);

	/* Global Station FT State */
	switch (session->ftSmeContext.FTState) {
	case eFT_START_READY:
	case eFT_AUTH_REQ_READY:
		sme_debug("ft_ies_length: %d", ft_ies_length);
		if ((session->ftSmeContext.auth_ft_ies) &&
			(session->ftSmeContext.auth_ft_ies_length)) {
			/* Free the one we recvd last from supplicant */
			qdf_mem_free(session->ftSmeContext.auth_ft_ies);
			session->ftSmeContext.auth_ft_ies_length = 0;
			session->ftSmeContext.auth_ft_ies = NULL;
		}
		ft_ies_length = QDF_MIN(ft_ies_length, MAX_FTIE_SIZE);
		/* Save the FT IEs */
		session->ftSmeContext.auth_ft_ies =
					qdf_mem_malloc(ft_ies_length);
		if (NULL == session->ftSmeContext.auth_ft_ies) {
			sme_err("Mem alloc failed for auth_ft_ies");
			sme_release_global_lock(&mac_ctx->sme);
			return;
		}
		session->ftSmeContext.auth_ft_ies_length = ft_ies_length;
		qdf_mem_copy((uint8_t *)session->ftSmeContext.auth_ft_ies,
				ft_ies, ft_ies_length);
		session->ftSmeContext.FTState = eFT_AUTH_REQ_READY;
		break;

	case eFT_AUTH_COMPLETE:
		/*
		 * We will need to re-start preauth. If we received FT
		 * IEs in eFT_PRE_AUTH_DONE state, it implies there was
		 * a rekey in our pre-auth state. Hence this implies we
		 * need Pre-auth again. OK now inform SME we have no
		 * pre-auth list. Delete the pre-auth node locally. Set
		 * your self back to restart pre-auth
		 */
		sme_debug("Preauth done & rcving AUTHREQ in state %d",
			  session->ftSmeContext.FTState);
		sme_debug("Unhandled reception of FT IES in state %d",
			  session->ftSmeContext.FTState);
		break;

	case eFT_REASSOC_REQ_WAIT:
		/*
		 * We are done with pre-auth, hence now waiting for
		 * reassoc req. This is the new FT Roaming in place At
		 * this juncture we'r ready to start sending Reassoc req
		 */

		ft_ies_length = QDF_MIN(ft_ies_length, MAX_FTIE_SIZE);

		sme_debug("New Reassoc Req: %pK in state %d",
			ft_ies, session->ftSmeContext.FTState);
		if ((session->ftSmeContext.reassoc_ft_ies) &&
			(session->ftSmeContext.reassoc_ft_ies_length)) {
			/* Free the one we recvd last from supplicant */
			qdf_mem_free(session->ftSmeContext.reassoc_ft_ies);
			session->ftSmeContext.reassoc_ft_ies_length = 0;
		}
		/* Save the FT IEs */
		session->ftSmeContext.reassoc_ft_ies =
					qdf_mem_malloc(ft_ies_length);
		if (NULL == session->ftSmeContext.reassoc_ft_ies) {
			sme_err("Mem alloc fail for reassoc_ft_ie");
			sme_release_global_lock(&mac_ctx->sme);
			return;
		}
		session->ftSmeContext.reassoc_ft_ies_length =
							ft_ies_length;
		qdf_mem_copy((uint8_t *)session->ftSmeContext.reassoc_ft_ies,
				ft_ies, ft_ies_length);

		session->ftSmeContext.FTState = eFT_SET_KEY_WAIT;
		sme_debug("ft_ies_length: %d state: %d", ft_ies_length,
			  session->ftSmeContext.FTState);

		break;

	default:
		sme_warn("Unhandled state: %d", session->ftSmeContext.FTState);
		break;
	}
	sme_release_global_lock(&mac_ctx->sme);
}

/**
 * sme_ft_send_update_key_ind() - To send key update indication for FT session
 * @mac: pointer to MAC context
 * @session_id: sme session id
 * @ftkey_info: FT key information
 *
 * To send key update indication for FT session
 *
 * Return: QDF_STATUS
 */
static
QDF_STATUS sme_ft_send_update_key_ind(tpAniSirGlobal mac, uint32_t session_id,
				      tCsrRoamSetKey *ftkey_info)
{
	tSirFTUpdateKeyInfo *msg;
	uint16_t msglen;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tSirKeyMaterial *keymaterial = NULL;
	tAniEdType ed_type;

	sme_debug("keyLength: %d", ftkey_info->keyLength);

	if (ftkey_info->keyLength > CSR_MAX_KEY_LEN) {
		sme_err("invalid keyLength: %d", ftkey_info->keyLength);
		return QDF_STATUS_E_FAILURE;
	}
	msglen  = sizeof(tSirFTUpdateKeyInfo);

	msg = qdf_mem_malloc(msglen);
	if (NULL == msg)
		return QDF_STATUS_E_NOMEM;

	msg->messageType = eWNI_SME_FT_UPDATE_KEY;
	msg->length = msglen;

	keymaterial = &msg->keyMaterial;
	keymaterial->length = ftkey_info->keyLength;
	ed_type = csr_translate_encrypt_type_to_ed_type(ftkey_info->encType);
	keymaterial->edType = ed_type;
	keymaterial->numKeys = 1;
	keymaterial->key[0].keyId = ftkey_info->keyId;
	keymaterial->key[0].unicast = (uint8_t) true;
	keymaterial->key[0].keyDirection = ftkey_info->keyDirection;

	qdf_mem_copy(&keymaterial->key[0].keyRsc,
			ftkey_info->keyRsc, CSR_MAX_RSC_LEN);
	keymaterial->key[0].paeRole = ftkey_info->paeRole;
	keymaterial->key[0].keyLength = ftkey_info->keyLength;

	if (ftkey_info->keyLength)
		qdf_mem_copy(&keymaterial->key[0].key, ftkey_info->Key,
				ftkey_info->keyLength);

	qdf_copy_macaddr(&msg->bssid, &ftkey_info->peerMac);
	msg->smeSessionId = session_id;
	sme_debug("BSSID = " MAC_ADDRESS_STR, MAC_ADDR_ARRAY(msg->bssid.bytes));
	status = umac_send_mb_message_to_mac(msg);

	return status;
}

bool sme_get_ftptk_state(tHalHandle hHal, uint32_t sessionId)
{
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("pSession is NULL");
		return false;
	}
	return pSession->ftSmeContext.setFTPTKState;
}

void sme_set_ftptk_state(tHalHandle hHal, uint32_t sessionId, bool state)
{
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);

	if (!pSession) {
		sme_err("pSession is NULL");
		return;
	}
	pSession->ftSmeContext.setFTPTKState = state;
}

QDF_STATUS sme_ft_update_key(tHalHandle hHal, uint32_t sessionId,
			     tCsrRoamSetKey *pFTKeyInfo)
{
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!pSession) {
		sme_err("pSession is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (pFTKeyInfo == NULL) {
		sme_err("pFTKeyInfo is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	status = sme_acquire_global_lock(&pMac->sme);
	if (!(QDF_IS_STATUS_SUCCESS(status)))
		return QDF_STATUS_E_FAILURE;

	sme_debug("sme_ft_update_key is received in state %d",
		  pSession->ftSmeContext.FTState);

	/* Global Station FT State */
	switch (pSession->ftSmeContext.FTState) {
	case eFT_SET_KEY_WAIT:
		if (sme_get_ft_pre_auth_state(hHal, sessionId) == true) {
			status = sme_ft_send_update_key_ind(pMac, sessionId,
								pFTKeyInfo);
			if (status != 0) {
				sme_err("Key set failure: %d", status);
				pSession->ftSmeContext.setFTPTKState = false;
				status = QDF_STATUS_FT_PREAUTH_KEY_FAILED;
			} else {
				pSession->ftSmeContext.setFTPTKState = true;
				status = QDF_STATUS_FT_PREAUTH_KEY_SUCCESS;
				sme_debug("Key set success");
			}
			sme_set_ft_pre_auth_state(hHal, sessionId, false);
		}

		pSession->ftSmeContext.FTState = eFT_START_READY;
		sme_debug("state changed to %d status %d",
			  pSession->ftSmeContext.FTState, status);
		break;

	default:
		sme_debug("Unhandled state:%d", pSession->ftSmeContext.FTState);
		status = QDF_STATUS_E_FAILURE;
		break;
	}
	sme_release_global_lock(&pMac->sme);

	return status;
}

/*
 * HDD Interface to SME. SME now sends the Auth 2 and RIC IEs up to the
 * supplicant. The supplicant will then proceed to send down the
 * Reassoc Req.
 */
void sme_get_ft_pre_auth_response(tHalHandle hHal, uint32_t sessionId,
				  uint8_t *ft_ies, uint32_t ft_ies_ip_len,
				  uint16_t *ft_ies_length)
{
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!pSession) {
		sme_err("pSession is NULL");
		return;
	}

	*ft_ies_length = 0;

	status = sme_acquire_global_lock(&pMac->sme);
	if (!(QDF_IS_STATUS_SUCCESS(status)))
		return;

	/* All or nothing - proceed only if both BSSID and FT IE fit */
	if ((QDF_MAC_ADDR_SIZE +
	     pSession->ftSmeContext.psavedFTPreAuthRsp->ft_ies_length) >
	    ft_ies_ip_len) {
		sme_release_global_lock(&pMac->sme);
		return;
	}
	/* hdd needs to pack the bssid also along with the */
	/* auth response to supplicant */
	qdf_mem_copy(ft_ies, pSession->ftSmeContext.preAuthbssId,
		     QDF_MAC_ADDR_SIZE);

	/* Copy the auth resp FTIEs */
	qdf_mem_copy(&(ft_ies[QDF_MAC_ADDR_SIZE]),
		     pSession->ftSmeContext.psavedFTPreAuthRsp->ft_ies,
		     pSession->ftSmeContext.psavedFTPreAuthRsp->ft_ies_length);

	*ft_ies_length = QDF_MAC_ADDR_SIZE +
		pSession->ftSmeContext.psavedFTPreAuthRsp->ft_ies_length;

	pSession->ftSmeContext.FTState = eFT_REASSOC_REQ_WAIT;

	sme_debug("Filled auth resp: %d", *ft_ies_length);
	sme_release_global_lock(&pMac->sme);
}

/*
 * SME now sends the RIC IEs up to the supplicant.
 * The supplicant will then proceed to send down the
 * Reassoc Req.
 */
void sme_get_rici_es(tHalHandle hHal, uint32_t sessionId, uint8_t *ric_ies,
		     uint32_t ric_ies_ip_len, uint32_t *ric_ies_length)
{
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
	struct csr_roam_session *pSession = CSR_GET_SESSION(pMac, sessionId);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!pSession) {
		sme_err("pSession is NULL");
		return;
	}

	*ric_ies_length = 0;

	status = sme_acquire_global_lock(&pMac->sme);
	if (!(QDF_IS_STATUS_SUCCESS(status)))
		return;

	/* All or nothing */
	if (pSession->ftSmeContext.psavedFTPreAuthRsp->ric_ies_length >
	    ric_ies_ip_len) {
		sme_release_global_lock(&pMac->sme);
		return;
	}

	qdf_mem_copy(ric_ies,
		     pSession->ftSmeContext.psavedFTPreAuthRsp->ric_ies,
		     pSession->ftSmeContext.psavedFTPreAuthRsp->ric_ies_length);

	*ric_ies_length =
		pSession->ftSmeContext.psavedFTPreAuthRsp->ric_ies_length;

	sme_debug("Filled ric ies: %d", *ric_ies_length);

	sme_release_global_lock(&pMac->sme);
}

/*
 * Timer callback for the timer that is started between the preauth completion
 * and reassoc request to the PE. In this interval, it is expected that the
 * pre-auth response and RIC IEs are passed up to the WPA supplicant and
 * received back the necessary FTIEs required to be sent in the reassoc request
 */
void sme_preauth_reassoc_intvl_timer_callback(void *context)
{
	tFTRoamCallbackUsrCtx *pUsrCtx = (tFTRoamCallbackUsrCtx *) context;

	if (pUsrCtx)
		csr_neighbor_roam_request_handoff(pUsrCtx->pMac,
						  pUsrCtx->sessionId);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#ifdef FEATURE_WLAN_ESE
static void sme_reset_esecckm_info(struct csr_roam_session *session)
{
	qdf_mem_zero(&session->eseCckmInfo, sizeof(session->eseCckmInfo));
}
#else
static void sme_reset_esecckm_info(struct csr_roam_session *session)
{
}
#endif
void sme_reset_key(mac_handle_t mac_handle, uint32_t vdev_id)
{
	tpAniSirGlobal mac = PMAC_STRUCT(mac_handle);
	struct csr_roam_session *session = NULL;

	if (!mac) {
		sme_err("mac is NULL");
		return;
	}

	session = CSR_GET_SESSION(mac, vdev_id);
	if (!session)
		return;
	qdf_mem_zero(&session->psk_pmk, sizeof(session->psk_pmk));
	session->pmk_len = 0;
	sme_reset_esecckm_info(session);
}
#endif
/* Reset the FT context. */
void sme_ft_reset(tHalHandle hHal, uint32_t sessionId)
{
	tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
	struct csr_roam_session *pSession = NULL;

	if (pMac == NULL) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("pMac is NULL"));
		return;
	}

	pSession = CSR_GET_SESSION(pMac, sessionId);
	if (NULL != pSession) {
		if (pSession->ftSmeContext.auth_ft_ies != NULL) {
			qdf_mem_free(pSession->ftSmeContext.auth_ft_ies);
			pSession->ftSmeContext.auth_ft_ies = NULL;
		}
		pSession->ftSmeContext.auth_ft_ies_length = 0;

		if (pSession->ftSmeContext.reassoc_ft_ies != NULL) {
			qdf_mem_free(pSession->ftSmeContext.reassoc_ft_ies);
			pSession->ftSmeContext.reassoc_ft_ies = NULL;
		}
		pSession->ftSmeContext.reassoc_ft_ies_length = 0;

		if (pSession->ftSmeContext.psavedFTPreAuthRsp != NULL) {
			qdf_mem_free(pSession->ftSmeContext.psavedFTPreAuthRsp);
			pSession->ftSmeContext.psavedFTPreAuthRsp = NULL;
		}
		pSession->ftSmeContext.setFTPreAuthState = false;
		pSession->ftSmeContext.setFTPTKState = false;

		qdf_mem_zero(pSession->ftSmeContext.preAuthbssId,
			     QDF_MAC_ADDR_SIZE);
		pSession->ftSmeContext.FTState = eFT_START_READY;
	}
}

/* End of File */
