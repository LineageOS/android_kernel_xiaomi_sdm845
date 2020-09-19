/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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

/*
 * DOC: sme_rrm.c
 *
 * Implementation for SME RRM APIs
 */

#include "ani_global.h"
#include "sme_inside.h"
#include "sme_api.h"
#include "cfg_api.h"

#ifdef FEATURE_WLAN_DIAG_SUPPORT
#include "host_diag_core_event.h"
#include "host_diag_core_log.h"
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

#include "csr_inside_api.h"

#include "rrm_global.h"
#include <wlan_scan_ucfg_api.h>
#include <wlan_scan_utils_api.h>
#include <wlan_utility.h>

/* Roam score for a neighbor AP will be calculated based on the below
 * definitions. The calculated roam score will be used to select the
 * roamable candidate from neighbor AP list
 */
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_REACHABILITY             0
/* When we support 11r over the DS, this should have a non-zero value */
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_SECURITY                 10
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_KEY_SCOPE                20
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_SPECTRUM_MGMT 0
/* Not used */
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_QOS           5
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_APSD          3
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_RRM           8
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_DELAYED_BA    0
/* We dont support delayed BA */
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_IMMEDIATE_BA  3
#define RRM_ROAM_SCORE_NEIGHBOR_REPORT_MOBILITY_DOMAIN          30

#ifdef FEATURE_WLAN_ESE
#define RRM_ROAM_SCORE_NEIGHBOR_IAPP_LIST                       30
#endif

uint64_t rrm_scan_timer;

/**
 * rrm_ll_purge_neighbor_cache() -Purges all the entries in the neighbor cache
 *
 * @pMac: Pointer to the Hal Handle.
 * @pList: Pointer the List that should be purged.
 *
 * This function purges all the entries in the neighbor cache and frees up all
 * the internal nodes
 *
 * Return: void
 */
static void rrm_ll_purge_neighbor_cache(tpAniSirGlobal pMac,
	tDblLinkList *pList)
{
	tListElem *pEntry;
	tRrmNeighborReportDesc *pNeighborReportDesc;

	csr_ll_lock(pList);
	while ((pEntry = csr_ll_remove_head(pList, LL_ACCESS_NOLOCK)) != NULL) {
		pNeighborReportDesc =
			GET_BASE_ADDR(pEntry, tRrmNeighborReportDesc, List);
		qdf_mem_free(pNeighborReportDesc->pNeighborBssDescription);
		qdf_mem_free(pNeighborReportDesc);
	}
	csr_ll_unlock(pList);
}

/**
 * rrm_indicate_neighbor_report_result() -calls the callback registered for
 *                                                      neighbor report
 * @pMac: Pointer to the Hal Handle.
 * @qdf_status - QDF_STATUS_SUCCESS/QDF_STATUS_FAILURE based on whether a valid
 *                       report is received or neighbor timer expired
 *
 * This function calls the callback register by the caller while requesting for
 * neighbor report. This function gets invoked if a neighbor report is received
 * from an AP or neighbor response wait timer expires.
 *
 * Return: void
 */
static void rrm_indicate_neighbor_report_result(tpAniSirGlobal pMac,
						QDF_STATUS qdf_status)
{
	NeighborReportRspCallback callback;
	void *callbackContext;

	/* Reset the neighbor response pending status */
	pMac->rrm.rrmSmeContext[DEFAULT_RRM_IDX].
		neighborReqControlInfo.isNeighborRspPending = false;

	/* Stop the timer if it is already running.
	 *  The timer should be running only in the SUCCESS case.
	 */
	if (QDF_TIMER_STATE_RUNNING ==
	    qdf_mc_timer_get_current_state(&pMac->rrm.
					   rrmSmeContext[DEFAULT_RRM_IDX].
					   neighborReqControlInfo.
					   neighborRspWaitTimer)) {
		sme_debug("No entry in neighbor report cache");
		qdf_mc_timer_stop(&pMac->rrm.rrmSmeContext[DEFAULT_RRM_IDX].
				  neighborReqControlInfo.neighborRspWaitTimer);
	}
	callback =
		pMac->rrm.rrmSmeContext[DEFAULT_RRM_IDX].neighborReqControlInfo.
		neighborRspCallbackInfo.neighborRspCallback;
	callbackContext =
		pMac->rrm.rrmSmeContext[DEFAULT_RRM_IDX].neighborReqControlInfo.
		neighborRspCallbackInfo.neighborRspCallbackContext;

	/* Reset the callback and the callback context before calling the
	 * callback. It is very likely that there may be a registration in
	 * callback itself.
	 */
	pMac->rrm.rrmSmeContext[DEFAULT_RRM_IDX].neighborReqControlInfo.
		neighborRspCallbackInfo.neighborRspCallback = NULL;
	pMac->rrm.rrmSmeContext[DEFAULT_RRM_IDX].neighborReqControlInfo.
		neighborRspCallbackInfo.neighborRspCallbackContext = NULL;

	/* Call the callback with the status received from caller */
	if (callback)
		callback(callbackContext, qdf_status);
}

/**
 * sme_RrmBeaconReportXmitInd () - Send beacon report
 * @mac_ctx  Pointer to mac context
 * @measurement_index: Measurement index
 * @result_arr scan results
 * @msrmnt_status flag to indicate that the measurement is done.
 * @bss_count  bss count
 *
 * Create and send the beacon report Xmit ind message to PE.
 *
 * Return: status
 */

static QDF_STATUS
sme_rrm_send_beacon_report_xmit_ind(tpAniSirGlobal mac_ctx,
	uint8_t measurement_index, tCsrScanResultInfo **result_arr,
	uint8_t msrmnt_status, uint8_t bss_count)
{
	tpSirBssDescription bss_desc = NULL;
	tpSirBeaconReportXmitInd beacon_rep;
	uint16_t length;
	uint32_t size;
	uint8_t  i = 0, j = 0, counter = 0;
	tCsrScanResultInfo *cur_result = NULL;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tpRrmSMEContext rrm_ctx =
		&mac_ctx->rrm.rrmSmeContext[measurement_index];
	tpSirBssDescription bss_desc_to_free[SIR_BCN_REPORT_MAX_BSS_DESC] = {0};

	if (NULL == result_arr && !msrmnt_status) {
		sme_err("Beacon report xmit Ind to PE Failed");
		return QDF_STATUS_E_FAILURE;
	}

	if (result_arr)
		cur_result = result_arr[j];

	do {
		length = sizeof(tSirBeaconReportXmitInd);
		beacon_rep = qdf_mem_malloc(length);
		if (NULL == beacon_rep) {
			sme_err("Unable to allocate memory for beacon report");
			return QDF_STATUS_E_NOMEM;
		}
		beacon_rep->messageType = eWNI_SME_BEACON_REPORT_RESP_XMIT_IND;
		beacon_rep->length = length;
		beacon_rep->measurement_idx = measurement_index;
		beacon_rep->uDialogToken = rrm_ctx->token;
		beacon_rep->duration = rrm_ctx->duration[0];
		beacon_rep->regClass = rrm_ctx->regClass;
		qdf_mem_copy(beacon_rep->bssId, rrm_ctx->sessionBssId.bytes,
			QDF_MAC_ADDR_SIZE);

		i = 0;
		while (cur_result) {
			bss_desc = &cur_result->BssDescriptor;
			if (bss_desc == NULL)
				break;
			size =  bss_desc->length + sizeof(bss_desc->length);
			beacon_rep->pBssDescription[i] = qdf_mem_malloc(size);
			if (NULL ==
				beacon_rep->pBssDescription[i])
				break;
			qdf_mem_copy(beacon_rep->pBssDescription[i],
				bss_desc, size);
			bss_desc_to_free[i] =
				beacon_rep->pBssDescription[i];
			sme_debug("RRM Result Bssid = " MAC_ADDRESS_STR
				" chan= %d, rssi = -%d",
				MAC_ADDR_ARRAY(
				beacon_rep->pBssDescription[i]->bssId),
				beacon_rep->pBssDescription[i]->channelId,
				beacon_rep->pBssDescription[i]->rssi * (-1));
			beacon_rep->numBssDesc++;
			if (++i >= SIR_BCN_REPORT_MAX_BSS_DESC)
				break;
			if (i + j >= bss_count)
				break;
			cur_result =
				result_arr[j + i];
		}

		j += i;
		if (!result_arr || (cur_result == NULL)
			|| (j >= bss_count)) {
			cur_result = NULL;
			sme_debug("Reached to  max/last BSS in cur_result list");
		} else {
			cur_result = result_arr[j];
			sme_debug("Move to the next BSS set in cur_result list");
		}
		beacon_rep->fMeasureDone =
			(cur_result) ? false : msrmnt_status;
		sme_debug("SME Sending BcnRepXmit to PE numBss %d i %d j %d",
			beacon_rep->numBssDesc, i, j);
		status = umac_send_mb_message_to_mac(beacon_rep);
		if (status != QDF_STATUS_SUCCESS)
			for (counter = 0; counter < i; ++counter)
				qdf_mem_free(bss_desc_to_free[counter]);
	} while (cur_result);

	return status;
}

#ifdef FEATURE_WLAN_ESE
/**
 * sme_ese_send_beacon_req_scan_results () - Send beacon report
 * @mac_ctx  Pointer to mac context
 * @measurement_index: Measurement request index
 * @session_id - session id
 * @result_arr scan results
 * @msrmnt_status flag to indicate that the measurement is done.
 * @bss_count  number of bss found
 *
 * This function sends up the scan results received as a part of
 * beacon request scanning.
 * This function is called after receiving the scan results per channel
 * Due to the limitation on the size of the IWEVCUSTOM buffer, we send
 * 3 BSSIDs of beacon report information in one custom event;
 *
 * Return: status
 */
static QDF_STATUS sme_ese_send_beacon_req_scan_results(
	tpAniSirGlobal mac_ctx, uint8_t measurement_index,
	uint32_t session_id, uint8_t channel,
	tCsrScanResultInfo **result_arr,
	uint8_t msrmnt_status, uint8_t bss_count)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	QDF_STATUS fill_ie_status;
	tpSirBssDescription bss_desc = NULL;
	uint32_t ie_len = 0;
	uint32_t out_ie_len = 0;
	uint8_t bss_counter = 0;
	tCsrScanResultInfo *cur_result = NULL;
	tpRrmSMEContext rrm_ctx =
		&mac_ctx->rrm.rrmSmeContext[measurement_index];
	struct csr_roam_info *roam_info;
	tSirEseBcnReportRsp bcn_rpt_rsp;
	tpSirEseBcnReportRsp bcn_report = &bcn_rpt_rsp;
	tpCsrEseBeaconReqParams cur_meas_req = NULL;
	uint8_t i = 0, j = 0;
	tBcnReportFields *bcn_rpt_fields;

	if (NULL == rrm_ctx) {
		sme_err("rrm_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	if (NULL == result_arr && !msrmnt_status) {
		sme_err("Beacon report xmit Ind to HDD Failed");
		return QDF_STATUS_E_FAILURE;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;

	if (result_arr)
		cur_result = result_arr[bss_counter];

	do {
		cur_meas_req = NULL;
		/* memset bcn_rpt_rsp for each iteration */
		qdf_mem_zero(&bcn_rpt_rsp, sizeof(bcn_rpt_rsp));

		for (i = 0; i < rrm_ctx->eseBcnReqInfo.numBcnReqIe; i++) {
			if (rrm_ctx->eseBcnReqInfo.bcnReq[i].channel ==
				channel) {
				cur_meas_req =
					&rrm_ctx->eseBcnReqInfo.bcnReq[i];
				break;
			}
		}
		if (NULL != cur_meas_req)
			bcn_report->measurementToken =
				cur_meas_req->measurementToken;
		sme_debug("Channel: %d MeasToken: %d", channel,
			bcn_report->measurementToken);

		j = 0;
		while (cur_result) {
			bss_desc = &cur_result->BssDescriptor;
			if (NULL == bss_desc) {
				cur_result = NULL;
				break;
			}
			ie_len = GET_IE_LEN_IN_BSS(bss_desc->length);
			bcn_rpt_fields =
				&bcn_report->bcnRepBssInfo[j].bcnReportFields;
			bcn_rpt_fields->ChanNum =
				bss_desc->channelId;
			bcn_report->bcnRepBssInfo[j].bcnReportFields.Spare = 0;
			if (NULL != cur_meas_req)
				bcn_rpt_fields->MeasDuration =
					cur_meas_req->measurementDuration;
			bcn_rpt_fields->PhyType = bss_desc->nwType;
			bcn_rpt_fields->RecvSigPower = bss_desc->rssi;
			bcn_rpt_fields->ParentTsf = bss_desc->parentTSF;
			bcn_rpt_fields->TargetTsf[0] = bss_desc->timeStamp[0];
			bcn_rpt_fields->TargetTsf[1] = bss_desc->timeStamp[1];
			bcn_rpt_fields->BcnInterval = bss_desc->beaconInterval;
			bcn_rpt_fields->CapabilityInfo =
				bss_desc->capabilityInfo;

			qdf_mem_copy(bcn_rpt_fields->Bssid,
				bss_desc->bssId, sizeof(tSirMacAddr));
				fill_ie_status =
					sir_beacon_ie_ese_bcn_report(mac_ctx,
						(uint8_t *) bss_desc->ieFields,
						ie_len,
						&(bcn_report->bcnRepBssInfo[j].
						pBuf),
						&out_ie_len);
			if (QDF_STATUS_E_FAILURE == fill_ie_status)
				continue;
			bcn_report->bcnRepBssInfo[j].ieLen = out_ie_len;

			sme_debug("Bssid"MAC_ADDRESS_STR" Channel: %d Rssi: %d",
				MAC_ADDR_ARRAY(bss_desc->bssId),
				bss_desc->channelId, (-1) * bss_desc->rssi);
			bcn_report->numBss++;
			if (++j >= SIR_BCN_REPORT_MAX_BSS_DESC)
				break;
			if ((bss_counter + j) >= bss_count)
				break;
			cur_result = result_arr[bss_counter + j];
		}

		bss_counter += j;
		if (!result_arr || !cur_result || (bss_counter >= bss_count)) {
			cur_result = NULL;
			sme_err("Reached to the max/last BSS in cur_result list");
		} else {
			cur_result = result_arr[bss_counter];
			sme_err("Move to the next BSS set in cur_result list");
		}

		bcn_report->flag =
			(msrmnt_status << 1) | ((cur_result) ? true : false);

		sme_debug("SME Sending BcnRep to HDD numBss: %d j: %d bss_counter: %d flag: %d",
			bcn_report->numBss, j, bss_counter,
			bcn_report->flag);

		roam_info->pEseBcnReportRsp = bcn_report;
		status = csr_roam_call_callback(mac_ctx, session_id, roam_info,
						0, eCSR_ROAM_ESE_BCN_REPORT_IND,
						0);

		/* Free the memory allocated to IE */
		for (i = 0; i < j; i++)
			if (bcn_report->bcnRepBssInfo[i].pBuf)
				qdf_mem_free(bcn_report->bcnRepBssInfo[i].pBuf);
	} while (cur_result);
	qdf_mem_free(roam_info);
	return status;
}

static inline
void sme_reset_ese_bcn_req_in_progress(tpRrmSMEContext sme_rrm_ctx)
{
	if (sme_rrm_ctx)
		sme_rrm_ctx->eseBcnReqInProgress = false;
}

#else

static inline
void sme_reset_ese_bcn_req_in_progress(tpRrmSMEContext sme_rrm_ctx)
{}
#endif /* FEATURE_WLAN_ESE */

/**
 * sme_rrm_send_scan_result() - to get scan result and send the beacon report
 * @mac_ctx: pointer to mac context
 * @measurement_index: Measurement request number
 * @num_chan: number of channels
 * @chan_list: list of channels to fetch the result from
 * @measurementdone: Flag to indicate measurement done or no
 *
 * This function is called to get the scan result from CSR and send the beacon
 * report xmit ind message to PE
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS sme_rrm_send_scan_result(tpAniSirGlobal mac_ctx,
					   uint8_t measurement_index,
					   uint8_t num_chan,
					   uint8_t *chan_list,
					   uint8_t measurementdone)
{
	mac_handle_t mac_handle = MAC_HANDLE(mac_ctx);
	tCsrScanResultFilter filter;
	tScanResultHandle result_handle;
	tCsrScanResultInfo *scan_results, *next_result;
	tCsrScanResultInfo **scanresults_arr = NULL;
	struct scan_result_list *result_list;
	QDF_STATUS status;
	uint8_t num_scan_results, counter = 0;
	tpRrmSMEContext rrm_ctx =
		&mac_ctx->rrm.rrmSmeContext[measurement_index];
	uint32_t session_id;
	struct csr_roam_info *roam_info = NULL;
	tSirScanType scan_type;
	struct csr_roam_session *session;

	qdf_mem_zero(&filter, sizeof(filter));
	filter.BSSIDs.numOfBSSIDs = 1;
	filter.BSSIDs.bssid = (struct qdf_mac_addr *)&rrm_ctx->bssId;

	if (rrm_ctx->ssId.length) {
		filter.SSIDs.SSIDList =
			(tCsrSSIDInfo *) qdf_mem_malloc(sizeof(tCsrSSIDInfo));
		if (filter.SSIDs.SSIDList == NULL) {
			sme_err("qdf_mem_malloc failed");
			return QDF_STATUS_E_NOMEM;
		}

		filter.SSIDs.SSIDList->SSID.length =
			rrm_ctx->ssId.length;
		qdf_mem_copy(filter.SSIDs.SSIDList->SSID.ssId,
				rrm_ctx->ssId.ssId, rrm_ctx->ssId.length);
		filter.SSIDs.numOfSSIDs = 1;
	} else {
		filter.SSIDs.numOfSSIDs = 0;
	}

	filter.ChannelInfo.numOfChannels = num_chan;
	filter.ChannelInfo.ChannelList = chan_list;
	filter.fMeasurement = true;

	if (eRRM_MSG_SOURCE_ESE_UPLOAD == rrm_ctx->msgSource ||
	    eRRM_MSG_SOURCE_LEGACY_ESE == rrm_ctx->msgSource)
		scan_type = rrm_ctx->measMode[rrm_ctx->currentIndex];
	else
		scan_type = rrm_ctx->measMode[0];

	if (scan_type == eSIR_BEACON_TABLE)
		filter.age_threshold =
			ucfg_scan_get_aging_time(mac_ctx->psoc);

	/*
	 * In case this is beacon report request from last AP (before roaming)
	 * following call to csr_roam_get_session_id_from_bssid will fail,
	 * hence use current session ID instead of one stored in SME rrm context
	 */
	if (QDF_STATUS_E_FAILURE == csr_roam_get_session_id_from_bssid(mac_ctx,
			&rrm_ctx->sessionBssId, &session_id)) {
		sme_debug("BSSID mismatch, using current session_id");
		session_id = mac_ctx->roam.roamSession->sessionId;
	}
	status = sme_scan_get_result(mac_handle, (uint8_t)session_id,
				     &filter, &result_handle);

	if (filter.SSIDs.SSIDList)
		qdf_mem_free(filter.SSIDs.SSIDList);

	sme_debug("RRM Measurement Done %d for index:%d", measurementdone,
		  measurement_index);
	if (NULL == result_handle) {
		/*
		 * no scan results
		 * Spec. doesn't say anything about such condition
		 * Since section 7.4.6.2 (IEEE802.11k-2008) says-rrm report
		 * frame should contain one or more report IEs. It probably
		 * means dont send any respose if no matching BSS found.
		 * Moreover, there is no flag or field in measurement report
		 * IE(7.3.2.22) OR beacon report IE(7.3.2.22.6) that can be set
		 * to indicate no BSS found on a given channel. If we finished
		 * measurement on all the channels, we still need to send a
		 * xmit indication with moreToFollow set to MEASURMENT_DONE so
		 * that PE can clean any context allocated.
		 */
		if (!measurementdone)
			return status;
#ifdef FEATURE_WLAN_ESE
		if (eRRM_MSG_SOURCE_ESE_UPLOAD == rrm_ctx->msgSource)
			status = sme_ese_send_beacon_req_scan_results(mac_ctx,
					measurement_index, session_id,
					chan_list[0], NULL,
					measurementdone, 0);
		else
#endif /* FEATURE_WLAN_ESE */
			status = sme_rrm_send_beacon_report_xmit_ind(mac_ctx,
						measurement_index, NULL,
						measurementdone, 0);
		return status;
	}
	scan_results = sme_scan_result_get_first(mac_handle, result_handle);
	if (NULL == scan_results && measurementdone) {
#ifdef FEATURE_WLAN_ESE
		if (eRRM_MSG_SOURCE_ESE_UPLOAD == rrm_ctx->msgSource) {
			status = sme_ese_send_beacon_req_scan_results(mac_ctx,
					measurement_index, session_id,
					chan_list[0], NULL,
					measurementdone, 0);
		} else
#endif /* FEATURE_WLAN_ESE */
			status = sme_rrm_send_beacon_report_xmit_ind(mac_ctx,
						measurement_index, NULL,
						measurementdone, 0);
	}

	result_list = (struct scan_result_list *)result_handle;
	num_scan_results = csr_ll_count(&result_list->List);
	if (!num_scan_results) {
		sme_err("num_scan_results is %d", num_scan_results);
		status = QDF_STATUS_E_FAILURE;
		goto rrm_send_scan_results_done;
	}

	sme_debug("num_scan_results %d", num_scan_results);
	scanresults_arr = qdf_mem_malloc(num_scan_results *
					 sizeof(next_result));
	if (!scanresults_arr) {
		sme_err("Failed to allocate scanresults_arr");
		status = QDF_STATUS_E_NOMEM;
		goto rrm_send_scan_results_done;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (NULL == roam_info) {
		sme_err("malloc failed");
		status = QDF_STATUS_E_NOMEM;
		goto rrm_send_scan_results_done;
	}

	session = CSR_GET_SESSION(mac_ctx, session_id);
	if ((!session) ||  (!csr_is_conn_state_connected_infra(
	    mac_ctx, session_id)) ||
	    (NULL == session->pConnectBssDesc)) {
		sme_err("Invaild session");
		status = QDF_STATUS_E_FAILURE;
		goto rrm_send_scan_results_done;
	}


	while (scan_results) {
		/*
		 * In passive scan, sta listens beacon. Connected AP beacon
		 * is offloaded to firmware. Firmware will discard
		 * connected AP beacon except that special IE exists.
		 * Connected AP beacon will not be sent to host. Hence, timer
		 * of connected AP in scan results is not updated and can
		 * not meet "pScanResult->timer >= RRM_scan_timer".
		 */
		uint8_t is_conn_bss_found = false;

		if ((scan_type == eSIR_PASSIVE_SCAN) &&
		     (!qdf_mem_cmp(scan_results->BssDescriptor.bssId,
		      session->pConnectBssDesc->bssId,
		      sizeof(struct qdf_mac_addr)))) {
			is_conn_bss_found = true;
			sme_debug("Connected BSS in scan results");
		}
		next_result = sme_scan_result_get_next(mac_handle,
						       result_handle);
		sme_debug("Scan res timer:%lu, rrm scan timer:%llu",
				scan_results->timer, rrm_scan_timer);
		if ((scan_results->timer >= rrm_scan_timer) ||
		    (is_conn_bss_found == true)) {
			roam_info->pBssDesc = &scan_results->BssDescriptor;
			csr_roam_call_callback(mac_ctx, session_id, roam_info,
						0, eCSR_ROAM_UPDATE_SCAN_RESULT,
						eCSR_ROAM_RESULT_NONE);
			scanresults_arr[counter++] = scan_results;
		}
		scan_results = next_result;
		if (counter >= num_scan_results)
			break;
	}
	/*
	 * The beacon report should be sent whether the counter is zero or
	 * non-zero. There might be a few scan results in the cache but not
	 * actually are a result of this scan. During that scenario, the
	 * counter will be zero. The report should be sent and LIM will further
	 * cleanup the RRM to accept the further incoming requests
	 * In case the counter is Zero, the pScanResultsArr will be NULL.
	 * The next level routine does a check for the measurementDone to
	 * determine whether to send a report or not.
	 */
	sme_debug("Number of BSS Desc with RRM Scan %d", counter);
	if (counter || measurementdone) {
#ifdef FEATURE_WLAN_ESE
		if (eRRM_MSG_SOURCE_ESE_UPLOAD == rrm_ctx->msgSource)
			status = sme_ese_send_beacon_req_scan_results(mac_ctx,
					measurement_index, session_id,
					chan_list[0], scanresults_arr,
					measurementdone, counter);
		else
#endif /* FEATURE_WLAN_ESE */
			status = sme_rrm_send_beacon_report_xmit_ind(mac_ctx,
					measurement_index, scanresults_arr,
					measurementdone, counter);
	}

rrm_send_scan_results_done:
	if (scanresults_arr)
		qdf_mem_free(scanresults_arr);
	qdf_mem_free(roam_info);
	sme_scan_result_purge(result_handle);

	return status;
}


/**
 * sme_rrm_scan_request_callback() -Sends the beacon report xmit to PE
 * @mac_ctx: Pointer to mac context
 * @pSmeRrmContext: SME rrm context for measurement request
 * @sessionId: session id
 * @scanId: Scan ID.
 * @status: CSR Status.
 *
 * The sme module calls this callback function once it finish the scan request
 * and this function send the beacon report xmit to PE and starts a timer of
 * random interval to issue next request.
 *
 * Return : 0 for success, non zero for failure
 */
static QDF_STATUS sme_rrm_scan_request_callback(tpAniSirGlobal pMac,
						tpRrmSMEContext pSmeRrmContext,
						uint8_t sessionId,
						uint32_t scanId,
						eCsrScanStatus status)
{
	uint16_t interval;
	uint32_t time_tick;
	QDF_STATUS qdf_status;
	uint32_t session_id;
	bool valid_result = true;
	uint8_t ch_idx, num_chan;

	/*
	 * RRM scan response received after roaming to different AP.
	 * Post message to PE for rrm cleanup.
	 */
	qdf_status = csr_roam_get_session_id_from_bssid(pMac,
						&pSmeRrmContext->sessionBssId,
						&session_id);
	if (qdf_status == QDF_STATUS_E_FAILURE) {
		sme_debug("Cleanup RRM context due to STA roaming");
		valid_result = false;
	}

	if (!pSmeRrmContext->channelList.ChannelList) {
		sme_err("[802.11 RRM]: Global freq list is null");
		pSmeRrmContext->channelList.numOfChannels = 0;
		sme_reset_ese_bcn_req_in_progress(pSmeRrmContext);
		return QDF_STATUS_E_FAILURE;
	}

	/* if any more channels are pending, start a timer of a random value
	 * within randomization interval.
	 */
	ch_idx = pSmeRrmContext->currentIndex;
	num_chan = pSmeRrmContext->channelList.numOfChannels;
	if (((ch_idx + 1) < num_chan) && valid_result) {
		if (QDF_TIMER_STATE_RUNNING ==
		    qdf_mc_timer_get_current_state(
				&pSmeRrmContext->IterMeasTimer)) {
			/*
			 * Measurement random timer is already running, this
			 * should not happen because the driver doesn't support
			 * multiple measurements simultaneously. Also for
			 * multiple measurements on a single report, the
			 * channels in op class should be appended to the global
			 * channel list
			 */
			sme_err("[802.11 RRM]: meas timer is already running");
			sme_rrm_send_scan_result(
				pMac, pSmeRrmContext->measurement_idx, 1,
				&pSmeRrmContext->channelList.
				ChannelList[ch_idx], true);
			qdf_mem_free(pSmeRrmContext->channelList.ChannelList);
			pSmeRrmContext->channelList.ChannelList = NULL;
			pSmeRrmContext->channelList.numOfChannels = 0;
			sme_reset_ese_bcn_req_in_progress(pSmeRrmContext);
			return QDF_STATUS_E_FAILURE;
		}

		sme_rrm_send_scan_result(pMac, pSmeRrmContext->measurement_idx,
					 1, &pSmeRrmContext->channelList.
					 ChannelList[ch_idx], false);

		/* Advance the current index. */
		pSmeRrmContext->currentIndex++;
		/* start the timer to issue next request. */
		/* From timer tick get a random number within 10ms and max
		 * randmization interval.
		 */
		time_tick = qdf_mc_timer_get_system_ticks();
		interval =
			time_tick % (pSmeRrmContext->randnIntvl - 10 + 1) + 10;

		sme_debug("Set timer for interval %d ", interval);
		qdf_status = qdf_mc_timer_start(&pSmeRrmContext->IterMeasTimer,
						interval);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			qdf_mem_free(pSmeRrmContext->channelList.ChannelList);
			pSmeRrmContext->channelList.ChannelList = NULL;
			pSmeRrmContext->channelList.numOfChannels = 0;
			sme_reset_ese_bcn_req_in_progress(pSmeRrmContext);
		}

	} else {
		/* Done with the measurement. Clean up all context and send a
		 * message to PE with measurement done flag set.
		 */
		sme_rrm_send_scan_result(pMac, pSmeRrmContext->measurement_idx,
					 1, &pSmeRrmContext->channelList.
					 ChannelList[ch_idx], true);
		qdf_mem_free(pSmeRrmContext->channelList.ChannelList);
		pSmeRrmContext->channelList.ChannelList = NULL;
		pSmeRrmContext->channelList.numOfChannels = 0;
		sme_reset_ese_bcn_req_in_progress(pSmeRrmContext);
	}

	return QDF_STATUS_SUCCESS;
}

static void sme_rrm_scan_event_callback(struct wlan_objmgr_vdev *vdev,
			struct scan_event *event, void *arg)
{
	tpAniSirGlobal mac_ctx;
	tpRrmSMEContext smerrmctx;
	uint32_t scan_id;
	uint8_t session_id, i;
	eCsrScanStatus scan_status = eCSR_SCAN_FAILURE;
	bool success = false;
	session_id = wlan_vdev_get_id(vdev);
	scan_id = event->scan_id;

	mac_ctx = (tpAniSirGlobal)arg;
	if (!mac_ctx) {
		sme_err("invalid mac_ctx");
		return;
	}

	qdf_mtrace(QDF_MODULE_ID_SCAN, QDF_MODULE_ID_SME, event->type,
		   event->vdev_id, event->scan_id);

	if (!util_is_scan_completed(event, &success))
		return;

	if (success)
		scan_status = eCSR_SCAN_SUCCESS;

	for (i = 0; i < MAX_MEASUREMENT_REQUEST; i++) {
		smerrmctx = &mac_ctx->rrm.rrmSmeContext[i];
		if (smerrmctx->scan_id == scan_id)
			break;

		if (i == (MAX_MEASUREMENT_REQUEST - 1))
			return;
	}

	sme_debug("Scan completed for scan_id:%d measurement_idx:%d",
		  scan_id, smerrmctx->measurement_idx);
	sme_rrm_scan_request_callback(mac_ctx, smerrmctx, session_id,
				      scan_id, scan_status);
}


/**
 * sme_rrm_issue_scan_req() - To issue rrm scan request
 * @mac_ctx: pointer to mac context
 * @idx: Measurement Index
 *
 * This routine is called to issue rrm scan request
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
sme_rrm_issue_scan_req(tpAniSirGlobal mac_ctx, uint8_t idx)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpRrmSMEContext sme_rrm_ctx = &mac_ctx->rrm.rrmSmeContext[idx];
	uint32_t session_id;
	tSirScanType scan_type;
	uint8_t *chan_list;
	uint8_t ch_idx;

	status = csr_roam_get_session_id_from_bssid(mac_ctx,
			&sme_rrm_ctx->sessionBssId, &session_id);
	if (status != QDF_STATUS_SUCCESS) {
		sme_err("sme session ID not found for bssid= "MAC_ADDRESS_STR,
			MAC_ADDR_ARRAY(sme_rrm_ctx->sessionBssId.bytes));
		status = QDF_STATUS_E_FAILURE;
		goto send_ind;
	}

	if ((sme_rrm_ctx->currentIndex) >=
			sme_rrm_ctx->channelList.numOfChannels) {
		sme_rrm_send_beacon_report_xmit_ind(mac_ctx, idx, NULL,
						    true, 0);
		sme_debug("done with the complete ch lt. finish and fee now");
		goto free_ch_lst;
	}

	if (eRRM_MSG_SOURCE_ESE_UPLOAD == sme_rrm_ctx->msgSource ||
		eRRM_MSG_SOURCE_LEGACY_ESE == sme_rrm_ctx->msgSource)
		scan_type = sme_rrm_ctx->measMode[sme_rrm_ctx->currentIndex];
	else
		scan_type = sme_rrm_ctx->measMode[0];

	if ((eSIR_ACTIVE_SCAN == scan_type) ||
	    (eSIR_PASSIVE_SCAN == scan_type)) {
		uint32_t max_chan_time;
		uint64_t current_time;
		struct scan_start_request *req;
		struct wlan_objmgr_vdev *vdev;
		uint32_t chan_num;

		req = qdf_mem_malloc(sizeof(*req));
		if (!req) {
			sme_debug("Failed to allocate memory");
			status = QDF_STATUS_E_NOMEM;
			goto send_ind;
		}

		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
						mac_ctx->psoc,
						session_id,
						WLAN_LEGACY_SME_ID);
		if (!vdev) {
			sme_err("VDEV is null %d", session_id);
			status = QDF_STATUS_E_INVAL;
			qdf_mem_free(req);
			goto send_ind;
		}
		ucfg_scan_init_default_params(vdev, req);
		req->scan_req.scan_id = ucfg_scan_get_scan_id(mac_ctx->psoc);
		sme_rrm_ctx->scan_id = req->scan_req.scan_id;

		sme_debug("RRM_SCN: rrm_idx:%d scan_id:%d",
			  sme_rrm_ctx->measurement_idx, sme_rrm_ctx->scan_id);
		req->scan_req.scan_f_passive =
				(scan_type == eSIR_ACTIVE_SCAN) ? false : true;
		req->scan_req.vdev_id = wlan_vdev_get_id(vdev);
		req->scan_req.scan_req_id = sme_rrm_ctx->req_id;
		qdf_mem_copy(&req->scan_req.bssid_list[0], sme_rrm_ctx->bssId,
				QDF_MAC_ADDR_SIZE);
		req->scan_req.num_bssid = 1;
		if (sme_rrm_ctx->ssId.length) {
			req->scan_req.num_ssids = 1;
			qdf_mem_copy(&req->scan_req.ssid[0].ssid,
					sme_rrm_ctx->ssId.ssId,
					sme_rrm_ctx->ssId.length);
			req->scan_req.ssid[0].length = sme_rrm_ctx->ssId.length;
		}

		/*
		 * set min and max channel time
		 * sme_rrm_ctx->duration; Dont use min timeout.
		 */
		if (eRRM_MSG_SOURCE_ESE_UPLOAD == sme_rrm_ctx->msgSource ||
			eRRM_MSG_SOURCE_LEGACY_ESE == sme_rrm_ctx->msgSource)
			max_chan_time =
			      sme_rrm_ctx->duration[sme_rrm_ctx->currentIndex];
		else
			max_chan_time = sme_rrm_ctx->duration[0];

		/*
		 * Use max_chan_time if max_chan_time is more than def value
		 * depending on type of scan.
		 */
		if (req->scan_req.scan_f_passive) {
			if (max_chan_time > req->scan_req.dwell_time_passive)
				req->scan_req.dwell_time_passive =
								max_chan_time;
			sme_debug("Passive Max Dwell Time(%d)",
				  req->scan_req.dwell_time_passive);
		} else {
			if (max_chan_time > req->scan_req.dwell_time_active)
				req->scan_req.dwell_time_active = max_chan_time;
			sme_debug("Active Max Dwell Time(%d)",
				  req->scan_req.dwell_time_active);
		}

		req->scan_req.adaptive_dwell_time_mode = SCAN_DWELL_MODE_STATIC;
		/*
		 * For RRM scans timing is very important especially when the
		 * request is for limited channels. There is no need for
		 * firmware to rest for about 100-200 ms on the home channel.
		 * Instead, it can start the scan right away which will make the
		 * host to respond with the beacon report as quickly as
		 * possible. Ensure that the scan requests are not back to back
		 * and hence there is a check to see if the requests are atleast
		 * 1 second apart.
		 */
		current_time = (uint64_t)qdf_mc_timer_get_system_time();
		sme_debug("prev scan triggered before %llu ms, totalchannels %d",
				current_time - rrm_scan_timer,
				sme_rrm_ctx->channelList.numOfChannels);
		if ((abs(current_time - rrm_scan_timer) > 1000) &&
				(sme_rrm_ctx->channelList.numOfChannels == 1)) {
			req->scan_req.max_rest_time = 1;
			req->scan_req.min_rest_time = 1;
			req->scan_req.idle_time = 1;
		}

		rrm_scan_timer = (uint64_t)qdf_mc_timer_get_system_time();

		/* set requestType to full scan */
		req->scan_req.chan_list.num_chan = 1;
		chan_num = sme_rrm_ctx->channelList.ChannelList[
			   sme_rrm_ctx->currentIndex];
		req->scan_req.chan_list.chan[0].freq =
			wlan_chan_to_freq(chan_num);
		sme_debug("Duration %d On channel %d freq %d",
				req->scan_req.dwell_time_active,
				chan_num,
				req->scan_req.chan_list.chan[0].freq);
		status = ucfg_scan_start(req);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		if (QDF_IS_STATUS_ERROR(status))
			goto send_ind;

		return status;
	} else if (eSIR_BEACON_TABLE == scan_type) {
		/*
		 * In beacon table mode, scan results are taken directly from
		 * scan cache without issuing any scan request. So, it is not
		 * proper to update rrm_scan_timer with latest time and hence
		 * made it to zero to satisfy
		 * pScanResult->timer >= rrm_scan_timer
		 */
		rrm_scan_timer = 0;
		chan_list = sme_rrm_ctx->channelList.ChannelList;
		if (!chan_list) {
			sme_err("[802.11 RRM]: Global channel list is null");
			sme_reset_ese_bcn_req_in_progress(sme_rrm_ctx);
			status = QDF_STATUS_E_FAILURE;
			goto send_ind;
		}

		ch_idx = sme_rrm_ctx->currentIndex;
		for (; ch_idx < sme_rrm_ctx->channelList.numOfChannels; ch_idx++) {

			if ((ch_idx + 1) <
			    sme_rrm_ctx->channelList.numOfChannels) {
				sme_rrm_send_scan_result(mac_ctx, idx, 1,
					&sme_rrm_ctx->channelList.ChannelList[
					ch_idx], false);
				/* Advance the current index. */
				sme_rrm_ctx->currentIndex++;
			} else {
				/*
				 * Done with the measurement. Clean up all context and
				 * send a message to PE with measurement done flag set.
				 */
				sme_rrm_send_scan_result(mac_ctx, idx, 1,
					&sme_rrm_ctx->channelList.ChannelList[
					ch_idx], true);
				sme_reset_ese_bcn_req_in_progress(sme_rrm_ctx);
				goto free_ch_lst;
			}
		}
	}

	sme_err("Unknown beacon report req mode(%d)", scan_type);
	/*
	 * Indicate measurement completion to PE
	 * If this is not done, pCurrentReq pointer will not be freed
	 * and PE will not handle subsequent Beacon requests
	 */
send_ind:
	sme_rrm_send_beacon_report_xmit_ind(mac_ctx, idx, NULL, true, 0);
free_ch_lst:
	qdf_mem_free(sme_rrm_ctx->channelList.ChannelList);
	sme_rrm_ctx->channelList.ChannelList = NULL;
	return status;
}

static QDF_STATUS sme_rrm_fill_scan_channels(uint8_t *country,
					     tpRrmSMEContext sme_rrm_context,
					     uint8_t reg_class,
					     uint32_t num_channels)
{
	uint32_t num_chan = 0;
	uint32_t i;

	/* List all the channels in the requested RC */
	wlan_reg_dmn_print_channels_in_opclass(country, reg_class);

	for (i = 0; i < num_channels; i++) {
		if (wlan_reg_dmn_get_opclass_from_channel(country,
			sme_rrm_context->channelList.ChannelList[i],
			BWALL) ==
			reg_class) {
			sme_rrm_context->channelList.
			ChannelList[num_chan] =
			sme_rrm_context->channelList.ChannelList[i];
			num_chan++;
		}
	}
	sme_rrm_context->channelList.numOfChannels = num_chan;
	if (sme_rrm_context->channelList.numOfChannels == 0) {
		qdf_mem_free(sme_rrm_context->channelList.ChannelList);
		sme_rrm_context->channelList.ChannelList = NULL;
		sme_err("No channels populated with requested operation class and current country, Hence abort the rrm operation");
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * sme_rrm_process_beacon_report_req_ind() -Process beacon report request
 * @pMac:- Global Mac structure
 * @pMsgBuf:- a pointer to a buffer that maps to various structures base
 *                  on the message type.The beginning of the buffer can always
 *                  map to tSirSmeRsp.
 *
 * This is called to process the Beacon
 * report request from peer AP forwarded through PE .
 *
 * Return : QDF_STATUS_SUCCESS - Validation is successful.
 */
QDF_STATUS sme_rrm_process_beacon_report_req_ind(tpAniSirGlobal pMac,
						void *pMsgBuf)
{
	tpSirBeaconReportReqInd pBeaconReq = (tpSirBeaconReportReqInd) pMsgBuf;
	tpRrmSMEContext pSmeRrmContext;
	uint32_t len = 0, i = 0, j = 0;
	uint8_t country[WNI_CFG_COUNTRY_CODE_LEN];
	uint32_t session_id;
	struct csr_roam_session *session;
	QDF_STATUS status;
	uint8_t local_num_channel, local_bcn_chan_freq;
	uint8_t *local_rrm_freq_list;
	tRrmPEContext rrm_context;
	bool chan_valid = true;

	pSmeRrmContext = &pMac->rrm.rrmSmeContext[pBeaconReq->measurement_idx];
	rrm_context = pMac->rrm.rrmPEContext;

	status = csr_roam_get_session_id_from_bssid(pMac, (struct qdf_mac_addr *)
						    pBeaconReq->bssId,
						    &session_id);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("sme session ID not found for bssid");
		goto cleanup;
	}

	session = CSR_GET_SESSION(pMac, session_id);
	if (!session) {
		sme_err("Invalid session id %d", session_id);
		status = QDF_STATUS_E_FAILURE;
		goto cleanup;
	}

	qdf_mem_zero(country, WNI_CFG_COUNTRY_CODE_LEN);
	if (session->connectedProfile.country_code[0])
		qdf_mem_copy(country, session->connectedProfile.country_code,
			     WNI_CFG_COUNTRY_CODE_LEN);
	else
		country[2] = OP_CLASS_GLOBAL;


	sme_debug("RRM SCN: Index:%d Request Reg class %d, AP's country code %c%c 0x%x Channel %d",
		  pBeaconReq->measurement_idx,
		  pBeaconReq->channelInfo.regulatoryClass,
		  country[0], country[1], country[2],
		  pBeaconReq->channelInfo.channelNum);

	if (pBeaconReq->channelList.numChannels >
	    SIR_ESE_MAX_MEAS_IE_REQS) {
		sme_err("Beacon report request numChannels:%u exceeds max num channels",
			pBeaconReq->channelList.numChannels);
		status = QDF_STATUS_E_INVAL;
		goto cleanup;
	}

	/* section 11.10.8.1 (IEEE Std 802.11k-2008) */
	/* channel 0 and 255 has special meaning. */
	if ((pBeaconReq->channelInfo.channelNum == 0) ||
	    ((pBeaconReq->channelInfo.channelNum == 255)
	     && (pBeaconReq->channelList.numChannels == 0))) {
		/* Add all the channel in the regulatory domain. */
		wlan_cfg_get_str_len(pMac, WNI_CFG_VALID_CHANNEL_LIST, &len);
		if (pSmeRrmContext->channelList.ChannelList) {
			qdf_mem_free(pSmeRrmContext->channelList.ChannelList);
			pSmeRrmContext->channelList.ChannelList = NULL;
		}
		pSmeRrmContext->channelList.ChannelList = qdf_mem_malloc(len);
		if (!pSmeRrmContext->channelList.ChannelList) {
			status = QDF_STATUS_E_NOMEM;
			goto cleanup;
		}

		csr_get_cfg_valid_channels(pMac, pSmeRrmContext->channelList.
					   ChannelList, &len);

		if (pBeaconReq->channelInfo.regulatoryClass) {
			if (sme_rrm_fill_scan_channels(country, pSmeRrmContext,
						       pBeaconReq->channelInfo.
						       regulatoryClass, len) !=
			    QDF_STATUS_SUCCESS)
				goto cleanup;
		} else {
			pSmeRrmContext->channelList.numOfChannels = len;
		}
	} else {
		len = 0;
		pSmeRrmContext->channelList.numOfChannels = 0;

		/* If valid channel is present. We first Measure on the given
		 * channel and if there are additional channels present in
		 * APchannelreport, measure on these also.
		 */
		if (pBeaconReq->channelInfo.channelNum != 255)
			len = 1;

		len += pBeaconReq->channelList.numChannels;

		if (pSmeRrmContext->channelList.ChannelList) {
			qdf_mem_free(pSmeRrmContext->channelList.ChannelList);
			pSmeRrmContext->channelList.ChannelList = NULL;
		}
		pSmeRrmContext->channelList.ChannelList = qdf_mem_malloc(len);
		if (!pSmeRrmContext->channelList.ChannelList) {
			status = QDF_STATUS_E_NOMEM;
			goto cleanup;
		}

		if (pBeaconReq->channelInfo.channelNum != 255) {
			if (csr_roam_is_channel_valid
				    (pMac, pBeaconReq->channelInfo.channelNum))
				pSmeRrmContext->channelList.
				ChannelList[pSmeRrmContext->channelList.
					    numOfChannels++] =
					pBeaconReq->channelInfo.channelNum;
			else
				sme_err("Invalid channel: %d",
					pBeaconReq->channelInfo.channelNum);
		}

		for (i = 0; i < pBeaconReq->channelList.numChannels; i++) {
			if (csr_roam_is_channel_valid(pMac, pBeaconReq->
					channelList.channelNumber[i])) {
				pSmeRrmContext->channelList.
					ChannelList[pSmeRrmContext->channelList.
				numOfChannels] = pBeaconReq->channelList.
					channelNumber[i];
				pSmeRrmContext->channelList.numOfChannels++;
			}
		}
	}

	local_rrm_freq_list = pSmeRrmContext->channelList.ChannelList;
	local_num_channel = 0;
	for (i = 0; i < pSmeRrmContext->channelList.numOfChannels; i++) {
		local_bcn_chan_freq = local_rrm_freq_list[i];
		chan_valid = true;

		if (pBeaconReq->measurement_idx > 0) {
			for (j = 0; j < rrm_context.beacon_rpt_chan_num; j++) {
				if (rrm_context.beacon_rpt_chan_list[j] ==
				    local_bcn_chan_freq) {
				/*
				 * Ignore this channel, As this is already
				 * included in previous request
				 */
					chan_valid = false;
					break;
				}
			}
		}

		if (chan_valid) {
			rrm_context.
			beacon_rpt_chan_list[rrm_context.beacon_rpt_chan_num] =
							local_bcn_chan_freq;
			rrm_context.beacon_rpt_chan_num++;

			if (rrm_context.beacon_rpt_chan_num >=
			    MAX_NUM_CHANNELS) {
			    /* this should never happen */
				sme_err("Reset beacon_rpt_chan_num : %d",
					rrm_context.beacon_rpt_chan_num);
				rrm_context.beacon_rpt_chan_num = 0;
			}
			local_rrm_freq_list[local_num_channel] =
							local_bcn_chan_freq;
			local_num_channel++;
		}
	}

	if (local_num_channel == 0)
		goto cleanup;

	pSmeRrmContext->channelList.numOfChannels = local_num_channel;

	/* Copy session bssid */
	qdf_mem_copy(pSmeRrmContext->sessionBssId.bytes, pBeaconReq->bssId,
		     sizeof(tSirMacAddr));

	/* copy measurement bssid */
	qdf_mem_copy(pSmeRrmContext->bssId, pBeaconReq->macaddrBssid,
		     sizeof(tSirMacAddr));

	/* Copy ssid */
	qdf_mem_copy(&pSmeRrmContext->ssId, &pBeaconReq->ssId,
		     sizeof(tAniSSID));

	pSmeRrmContext->token = pBeaconReq->uDialogToken;
	pSmeRrmContext->regClass = pBeaconReq->channelInfo.regulatoryClass;
	pSmeRrmContext->randnIntvl =
		QDF_MAX(pBeaconReq->randomizationInterval,
			pMac->rrm.rrmConfig.max_randn_interval);
	pSmeRrmContext->currentIndex = 0;
	pSmeRrmContext->msgSource = pBeaconReq->msgSource;
	qdf_mem_copy((uint8_t *) &pSmeRrmContext->measMode,
		     (uint8_t *) &pBeaconReq->fMeasurementtype,
		     SIR_ESE_MAX_MEAS_IE_REQS);
	qdf_mem_copy((uint8_t *) &pSmeRrmContext->duration,
		     (uint8_t *) &pBeaconReq->measurementDuration,
		     SIR_ESE_MAX_MEAS_IE_REQS);

	sme_debug("token: %d randnIntvl: %d msgSource: %d",
		pSmeRrmContext->token, pSmeRrmContext->randnIntvl,
		pSmeRrmContext->msgSource);

	return sme_rrm_issue_scan_req(pMac, pBeaconReq->measurement_idx);

cleanup:
	if (pBeaconReq->msgSource == eRRM_MSG_SOURCE_11K) {
		/* Copy session bssid */
		qdf_mem_copy(pSmeRrmContext->sessionBssId.bytes,
			     pBeaconReq->bssId, sizeof(tSirMacAddr));

		/* copy measurement bssid */
		qdf_mem_copy(pSmeRrmContext->bssId, pBeaconReq->macaddrBssid,
			     sizeof(tSirMacAddr));

		pSmeRrmContext->token = pBeaconReq->uDialogToken;
		pSmeRrmContext->regClass =
				pBeaconReq->channelInfo.regulatoryClass;
		pSmeRrmContext->randnIntvl =
			QDF_MAX(pBeaconReq->randomizationInterval,
				pMac->rrm.rrmConfig.max_randn_interval);

		sme_rrm_send_beacon_report_xmit_ind(pMac,
						    pBeaconReq->measurement_idx,
						    NULL, true, 0);
	}

	return status;
}

/**
 * sme_rrm_neighbor_report_request() - This is API can be used to trigger a
 *        Neighbor report from the peer.
 * @sessionId: session identifier on which the request should be made.
 * @pNeighborReq: a pointer to a neighbor report request.
 *
 * This is API can be used to trigger a  Neighbor report from the peer.
 *
 * Return: QDF_STATUS_SUCCESS - Validation is successful.
 */
QDF_STATUS sme_rrm_neighbor_report_request(tpAniSirGlobal pMac, uint8_t
					sessionId, tpRrmNeighborReq
					pNeighborReq,
					tpRrmNeighborRspCallbackInfo
					callbackInfo)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpSirNeighborReportReqInd pMsg;
	struct csr_roam_session *pSession;

	sme_debug("Request to send Neighbor report request received ");
	if (!CSR_IS_SESSION_VALID(pMac, sessionId)) {
		sme_err("Invalid session %d", sessionId);
		return QDF_STATUS_E_INVAL;
	}
	pSession = CSR_GET_SESSION(pMac, sessionId);

	/* If already a report is pending, return failure */
	if (true ==
	    pMac->rrm.rrmSmeContext[0].neighborReqControlInfo.
	    isNeighborRspPending) {
		sme_err("Neighbor request already pending.. Not allowed");
		return QDF_STATUS_E_AGAIN;
	}

	pMsg = qdf_mem_malloc(sizeof(tSirNeighborReportReqInd));
	if (NULL == pMsg) {
		sme_err("Unable to allocate memory for Neighbor request");
		return QDF_STATUS_E_NOMEM;
	}

	rrm_ll_purge_neighbor_cache(pMac,
			    &pMac->rrm.rrmSmeContext[0].neighborReportCache);

	pMsg->messageType = eWNI_SME_NEIGHBOR_REPORT_REQ_IND;
	pMsg->length = sizeof(tSirNeighborReportReqInd);
	qdf_mem_copy(&pMsg->bssId, &pSession->connectedProfile.bssid,
		     sizeof(tSirMacAddr));
	pMsg->noSSID = pNeighborReq->no_ssid;
	qdf_mem_copy(&pMsg->ucSSID, &pNeighborReq->ssid, sizeof(tSirMacSSid));

	status = umac_send_mb_message_to_mac(pMsg);
	if (status != QDF_STATUS_SUCCESS)
		return QDF_STATUS_E_FAILURE;

	/* Neighbor report request message sent successfully to PE.
	 * Now register the callbacks
	 */
	pMac->rrm.rrmSmeContext[0].neighborReqControlInfo.
		neighborRspCallbackInfo.neighborRspCallback =
		callbackInfo->neighborRspCallback;
	pMac->rrm.rrmSmeContext[0].neighborReqControlInfo.
		neighborRspCallbackInfo.neighborRspCallbackContext =
		callbackInfo->neighborRspCallbackContext;
	pMac->rrm.rrmSmeContext[0].neighborReqControlInfo.isNeighborRspPending =
		true;

	/* Start neighbor response wait timer now */
	qdf_mc_timer_start(&pMac->rrm.rrmSmeContext[0].neighborReqControlInfo.
			   neighborRspWaitTimer, callbackInfo->timeout);

	return QDF_STATUS_SUCCESS;
}

/**
 * rrm_calculate_neighbor_ap_roam_score() - caclulates roam score
 * @mac_ctx:                mac global context
 * @pNeighborReportDesc:    Neighbor BSS Descriptor node for which roam score
 *                          should be calculated
 *
 * This API is called while handling individual neighbor reports from the APs
 * neighbor AP report to calculate the cumulative roam score before storing it
 * in neighbor cache.
 *
 * Return: void
 */
static void
rrm_calculate_neighbor_ap_roam_score(tpAniSirGlobal mac_ctx,
				tpRrmNeighborReportDesc nbr_report_desc)
{
	tpSirNeighborBssDescripton nbr_bss_desc;
	uint32_t roam_score = 0;
#ifdef FEATURE_WLAN_ESE
	uint8_t session_id;
#endif
	if (NULL == nbr_report_desc) {
		QDF_ASSERT(0);
		return;
	}

	if (NULL == nbr_report_desc->pNeighborBssDescription) {
		QDF_ASSERT(0);
		return;
	}

	nbr_bss_desc = nbr_report_desc->pNeighborBssDescription;
	if (!nbr_bss_desc->bssidInfo.rrmInfo.fMobilityDomain)
		goto check_11r_assoc;

	roam_score += RRM_ROAM_SCORE_NEIGHBOR_REPORT_MOBILITY_DOMAIN;
	if (!nbr_bss_desc->bssidInfo.rrmInfo.fSameSecurityMode)
		goto check_11r_assoc;

	roam_score += RRM_ROAM_SCORE_NEIGHBOR_REPORT_SECURITY;
	if (!nbr_bss_desc->bssidInfo.rrmInfo.fSameAuthenticator)
		goto check_11r_assoc;

	roam_score += RRM_ROAM_SCORE_NEIGHBOR_REPORT_KEY_SCOPE;
	if (!nbr_bss_desc->bssidInfo.rrmInfo.fCapRadioMeasurement)
		goto check_11r_assoc;

	roam_score += RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_RRM;
	if (nbr_bss_desc->bssidInfo.rrmInfo.fCapSpectrumMeasurement)
		roam_score +=
			RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_SPECTRUM_MGMT;

	if (nbr_bss_desc->bssidInfo.rrmInfo.fCapQos)
		roam_score += RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_QOS;

	if (nbr_bss_desc->bssidInfo.rrmInfo.fCapApsd)
		roam_score += RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_APSD;

	if (nbr_bss_desc->bssidInfo.rrmInfo.fCapDelayedBlockAck)
		roam_score +=
			RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_DELAYED_BA;

	if (nbr_bss_desc->bssidInfo.rrmInfo.fCapImmediateBlockAck)
		roam_score +=
			RRM_ROAM_SCORE_NEIGHBOR_REPORT_CAPABILITY_IMMEDIATE_BA;

	if (nbr_bss_desc->bssidInfo.rrmInfo.fApPreauthReachable)
		roam_score += RRM_ROAM_SCORE_NEIGHBOR_REPORT_REACHABILITY;

check_11r_assoc:
#ifdef FEATURE_WLAN_ESE
	session_id = nbr_report_desc->sessionId;
	/* It has come in the report so its the best score */
	if (csr_neighbor_roam_is11r_assoc(mac_ctx, session_id) == false) {
		/* IAPP Route so lets make use of this info save all AP, as the
		 * list does not come all the time. Save and reuse till the next
		 * AP List comes to us. Even save our own MAC address. Will be
		 * useful next time around.
		 */
		roam_score += RRM_ROAM_SCORE_NEIGHBOR_IAPP_LIST;
	}
#endif
	nbr_report_desc->roamScore = roam_score;
}

/**
 * rrm_store_neighbor_rpt_by_roam_score()-store Neighbor BSS descriptor
 * @pMac: Pointer to mac context
 * @pNeighborReportDesc - Neighbor BSS Descriptor node to be stored in cache
 * @index: RRM sme context index
 *
 * This API is called to store a given
 * Neighbor BSS descriptor to the neighbor cache. This function
 * stores the neighbor BSS descriptors in such a way that descriptors
 * are sorted by roamScore in descending order
 *
 * Return: void.
 */
static void rrm_store_neighbor_rpt_by_roam_score(tpAniSirGlobal pMac,
				tpRrmNeighborReportDesc pNeighborReportDesc,
				uint8_t index)
{
	tpRrmSMEContext pSmeRrmContext = &pMac->rrm.rrmSmeContext[0];
	tListElem *pEntry;
	tRrmNeighborReportDesc *pTempNeighborReportDesc;

	if (NULL == pNeighborReportDesc) {
		QDF_ASSERT(0);
		return;
	}
	if (NULL == pNeighborReportDesc->pNeighborBssDescription) {
		QDF_ASSERT(0);
		return;
	}

	if (csr_ll_is_list_empty
		    (&pSmeRrmContext->neighborReportCache, LL_ACCESS_LOCK)) {
		sme_err("Neighbor report cache is empty.. Adding a entry now");
		/* Neighbor list cache is empty. Insert this entry
		 * in the tail
		 */
		csr_ll_insert_tail(&pSmeRrmContext->neighborReportCache,
				   &pNeighborReportDesc->List, LL_ACCESS_LOCK);
		return;
	}
	/* Should store the neighbor BSS description in the order
	 * sorted by roamScore in descending order. APs with highest
	 * roamScore should be the 1st entry in the list
	 */
	pEntry = csr_ll_peek_head(&pSmeRrmContext->neighborReportCache,
				LL_ACCESS_LOCK);
	while (pEntry != NULL) {
		pTempNeighborReportDesc = GET_BASE_ADDR(pEntry,
					tRrmNeighborReportDesc, List);
		if (pTempNeighborReportDesc->roamScore <
				pNeighborReportDesc->roamScore)
			break;
		pEntry = csr_ll_next(&pSmeRrmContext->
				neighborReportCache, pEntry, LL_ACCESS_LOCK);
		}

	if (pEntry)
		/* This BSS roamscore is better than something in the
		 * list. Insert this before that one
		 */
		csr_ll_insert_entry(&pSmeRrmContext->neighborReportCache,
					pEntry, &pNeighborReportDesc->List,
					LL_ACCESS_LOCK);
	else
		/* All the entries in the list has a better roam Score
		 * than this one. Insert this at the last
		 */
		csr_ll_insert_tail(&pSmeRrmContext->neighborReportCache,
					&pNeighborReportDesc->List,
					LL_ACCESS_LOCK);
}

/**
 * sme_rrm_process_neighbor_report() -Process the Neighbor report received
 *                                                     from PE
 * @pMac - Global MAC structure
 * @pMsgBuf - a pointer to a buffer that maps to various structures base
 *                  on the message type.
 *                  The beginning of the buffer can always map to tSirSmeRsp.
 * This is called to process the Neighbor report received from PE.
 *
 * Return: QDF_STATUS_SUCCESS - Validation is successful
 */
static QDF_STATUS sme_rrm_process_neighbor_report(tpAniSirGlobal pMac,
						  void *pMsgBuf)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tpSirNeighborReportInd pNeighborRpt = (tpSirNeighborReportInd) pMsgBuf;
	tpRrmNeighborReportDesc pNeighborReportDesc;
	uint8_t i = 0;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;

	/* Purge the cache on reception of unsolicited neighbor report */
	if (!pMac->rrm.rrmSmeContext[pNeighborRpt->measurement_idx].
	    neighborReqControlInfo.isNeighborRspPending)
		rrm_ll_purge_neighbor_cache(pMac,
		    &pMac->rrm.rrmSmeContext[pNeighborRpt->measurement_idx].
		    neighborReportCache);

	for (i = 0; i < pNeighborRpt->numNeighborReports; i++) {
		pNeighborReportDesc =
			qdf_mem_malloc(sizeof(tRrmNeighborReportDesc));
		if (NULL == pNeighborReportDesc) {
			sme_err("Failed to alloc memory for RRM report desc");
			status = QDF_STATUS_E_NOMEM;
			goto end;

		}

		pNeighborReportDesc->pNeighborBssDescription =
			qdf_mem_malloc(sizeof(tSirNeighborBssDescription));
		if (NULL == pNeighborReportDesc->pNeighborBssDescription) {
			sme_err("Failed to alloc mem for RRM BSS Description");
			qdf_mem_free(pNeighborReportDesc);
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		qdf_mem_copy(pNeighborReportDesc->pNeighborBssDescription,
			     &pNeighborRpt->sNeighborBssDescription[i],
			     sizeof(tSirNeighborBssDescription));

		sme_debug("Received neighbor report with Neighbor BSSID: "
			MAC_ADDRESS_STR,
			MAC_ADDR_ARRAY(
			       pNeighborRpt->sNeighborBssDescription[i].bssId));

		rrm_calculate_neighbor_ap_roam_score(pMac, pNeighborReportDesc);

		if (pNeighborReportDesc->roamScore > 0) {
			rrm_store_neighbor_rpt_by_roam_score(pMac,
				     pNeighborReportDesc,
				     pNeighborRpt->measurement_idx);
		} else {
			sme_err("Roam score of BSSID  " MAC_ADDRESS_STR
				" is 0, Ignoring..",
				MAC_ADDR_ARRAY(pNeighborRpt->
					       sNeighborBssDescription[i].
					       bssId));

			qdf_mem_free(
				pNeighborReportDesc->pNeighborBssDescription);
			qdf_mem_free(pNeighborReportDesc);
		}
	}
end:

	if (!csr_ll_count(
		&pMac->rrm.rrmSmeContext[pNeighborRpt->measurement_idx].
		neighborReportCache))
		qdf_status = QDF_STATUS_E_FAILURE;

	rrm_indicate_neighbor_report_result(pMac, qdf_status);

	return status;
}

/**
 * sme_rrm_msg_processor()-Process RRM message
 * @pMac - Pointer to the global MAC parameter structure.
 * @msg_type - the type of msg passed by PE as defined in wni_api.h
 * @pMsgBuf - a pointer to a buffer that maps to various structures base
 *                  on the message type.
 *                  The beginning of the buffer can always map to tSirSmeRsp.
 * sme_process_msg() calls this function for the
 * messages that are handled by SME RRM module.
 *
 * Return: QDF_STATUS_SUCCESS - Validation is successful.
 */
QDF_STATUS sme_rrm_msg_processor(tpAniSirGlobal pMac, uint16_t msg_type,
				 void *pMsgBuf)
{
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  FL(" Msg = %d for RRM measurement"), msg_type);

	/* switch on the msg type & make the state transition accordingly */
	switch (msg_type) {
	case eWNI_SME_NEIGHBOR_REPORT_IND:
		sme_rrm_process_neighbor_report(pMac, pMsgBuf);
		break;

	case eWNI_SME_BEACON_REPORT_REQ_IND:
		sme_rrm_process_beacon_report_req_ind(pMac, pMsgBuf);
		break;

	default:
		/* err msg */
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("sme_rrm_msg_processor:unknown msg type = %d"),
			  msg_type);

		break;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * rrm_iter_meas_timer_handle() - Timer handler to handlet the timeout
 * @data - Timer data.
 *
 * Timer handler to handlet the timeout condition when a specific BT
 * stop event does not come back, in which case to restore back the
 * heartbeat timer.
 *
 * Return: NULL
 */
static void rrm_iter_meas_timer_handle(void *data)
{
	tpAniSirGlobal pMac;
	mac_handle_t mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	tpRrmSMEContext sme_rrm_ctx = (tpRrmSMEContext)data;

	pMac = MAC_CONTEXT(mac_handle);
	if (!pMac) {
		sme_err("Mac ctx is NULL");
		return;
	}

	sme_debug("Randomization timer expired...send on next channel");
	/* Issue a scan req for next channel. */
	sme_rrm_issue_scan_req(pMac, sme_rrm_ctx->measurement_idx);
}

/**
 * rrm_neighbor_rsp_timeout_handler() - Timer handler to handlet the timeout
 * @pMac - The handle returned by mac_open.
 *
 * Timer handler to handle the timeout condition when a neighbor request is sent
 * and no neighbor response is received from the AP
 *
 * Return: NULL
 */
static void rrm_neighbor_rsp_timeout_handler(void *userData)
{
	tpAniSirGlobal pMac = (tpAniSirGlobal) userData;

	sme_warn("Neighbor Response timed out");
	rrm_indicate_neighbor_report_result(pMac, QDF_STATUS_E_FAILURE);
}

/**
 * rrm_open() - Initialze all RRM module
 * @pMac: The handle returned by mac_open.
 *
 * Initialze all RRM module.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS rrm_open(tpAniSirGlobal pMac)
{

	QDF_STATUS qdf_status;
	tpRrmSMEContext pSmeRrmContext;
	QDF_STATUS qdf_ret_status = QDF_STATUS_SUCCESS;
	uint8_t i;

	pMac->rrm.rrmConfig.max_randn_interval = 50;        /* ms */

	for (i = 0; i < MAX_MEASUREMENT_REQUEST; i++) {
		pSmeRrmContext = &pMac->rrm.rrmSmeContext[i];
		qdf_status = qdf_mc_timer_init(&pSmeRrmContext->IterMeasTimer,
					       QDF_TIMER_TYPE_SW,
					       rrm_iter_meas_timer_handle,
						(void *)pSmeRrmContext);

		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			sme_err("rrm_open: Fail to init timer");
			return QDF_STATUS_E_FAILURE;
		}

		qdf_status = qdf_mc_timer_init(
				&pSmeRrmContext->neighborReqControlInfo.
				neighborRspWaitTimer, QDF_TIMER_TYPE_SW,
				rrm_neighbor_rsp_timeout_handler,
				(void *)pMac);

		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			sme_err("rrm_open: Fail to init neighbor rsp timer");
			return QDF_STATUS_E_FAILURE;
		}

		pSmeRrmContext->measurement_idx = i;
		pSmeRrmContext->neighborReqControlInfo.isNeighborRspPending =
				false;

		qdf_ret_status =
			csr_ll_open(&pSmeRrmContext->neighborReportCache);
		if (QDF_STATUS_SUCCESS != qdf_ret_status) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				  "rrm_open: Fail to open neighbor cache result");
			return QDF_STATUS_E_FAILURE;
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * rrm_close() - Release all RRM modules and their resources.
 * @pMac - The handle returned by mac_open.
 *
 * Release all RRM modules and their resources.
 *
 * Return: QDF_STATUS
 *           QDF_STATUS_E_FAILURE  success
 *           QDF_STATUS_SUCCESS  failure
 */

QDF_STATUS rrm_close(tpAniSirGlobal pMac)
{

	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	tpRrmSMEContext pSmeRrmContext;
	uint8_t i;

	for (i = 0; i < MAX_MEASUREMENT_REQUEST; i++) {
		pSmeRrmContext = &pMac->rrm.rrmSmeContext[i];
		if (QDF_TIMER_STATE_RUNNING ==
		    qdf_mc_timer_get_current_state(&pSmeRrmContext->IterMeasTimer)) {
			qdf_status = qdf_mc_timer_stop(
					&pSmeRrmContext->IterMeasTimer);
			if (QDF_IS_STATUS_ERROR(qdf_status))
				sme_err("Timer stop fail");
		}

		if (pSmeRrmContext->channelList.ChannelList) {
			qdf_mem_free(pSmeRrmContext->channelList.ChannelList);
			pSmeRrmContext->channelList.ChannelList = NULL;
			pSmeRrmContext->channelList.numOfChannels = 0;
		}

		qdf_status =
			qdf_mc_timer_destroy(&pSmeRrmContext->IterMeasTimer);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			sme_err("Fail to destroy timer");

		if (QDF_TIMER_STATE_RUNNING ==
		    qdf_mc_timer_get_current_state(&pSmeRrmContext->
						   neighborReqControlInfo.
						   neighborRspWaitTimer)) {
			qdf_status = qdf_mc_timer_stop(&pSmeRrmContext->
						neighborReqControlInfo.
						  neighborRspWaitTimer);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status))
				sme_err("Timer stop fail");
		}

		qdf_status = qdf_mc_timer_destroy(&pSmeRrmContext->
						  neighborReqControlInfo.
						  neighborRspWaitTimer);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			sme_err("Fail to destroy timer");

		rrm_ll_purge_neighbor_cache(pMac,
				   &pSmeRrmContext->neighborReportCache);
		csr_ll_close(&pSmeRrmContext->neighborReportCache);
	}

	return qdf_status;

}

/**
 * rrm_change_default_config_param() - Changing default config param to new
 * @pMac - The handle returned by mac_open.
 * param  pRrmConfig - pointer to new rrm configs.
 *
 * Return: QDF_STATUS
 *           QDF_STATUS_SUCCESS  success
 */
QDF_STATUS rrm_change_default_config_param(tpAniSirGlobal pMac,
					   struct rrm_config_param *rrm_config)
{
	qdf_mem_copy(&pMac->rrm.rrmConfig, rrm_config,
		     sizeof(struct rrm_config_param));

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS rrm_start(tpAniSirGlobal mac_ctx)
{
	tpRrmSMEContext smerrmctx;
	wlan_scan_requester req_id;
	uint8_t i;

	/* Register with scan component */
	req_id = ucfg_scan_register_requester(mac_ctx->psoc,
					      "RRM",
					      sme_rrm_scan_event_callback,
					      mac_ctx);

	for (i = 0; i < MAX_MEASUREMENT_REQUEST; i++) {
		smerrmctx = &mac_ctx->rrm.rrmSmeContext[i];
		smerrmctx->req_id = req_id;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS rrm_stop(tpAniSirGlobal mac_ctx)
{
	tpRrmSMEContext smerrmctx;
	wlan_scan_requester req_id;
	uint8_t i;

	for (i = 0; i < MAX_MEASUREMENT_REQUEST; i++) {
		smerrmctx = &mac_ctx->rrm.rrmSmeContext[i];
		req_id = smerrmctx->req_id;
		smerrmctx->req_id = 0;
	}
	ucfg_scan_unregister_requester(mac_ctx->psoc, req_id);

	return QDF_STATUS_SUCCESS;
}
