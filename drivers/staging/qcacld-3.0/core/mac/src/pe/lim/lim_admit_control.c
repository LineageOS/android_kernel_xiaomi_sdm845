/*
 * Copyright (c) 2011-2018 The Linux Foundation. All rights reserved.
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
 * This file contains TSPEC and STA admit control related functions
 * NOTE: applies only to AP builds
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "sys_def.h"
#include "lim_api.h"
#include "cfg_api.h"             /* wlan_cfg_get_int() */
#include "lim_trace.h"
#include "lim_send_sme_rsp_messages.h"
#include "lim_types.h"
#include "lim_admit_control.h"

#define ADMIT_CONTROL_LOGLEVEL        LOGD
#define ADMIT_CONTROL_POLICY_LOGLEVEL LOGD

/* total available bandwidth in bps in each phy mode
 * these should be defined in hal or dph - replace these later
 */
#define LIM_TOTAL_BW_11A   54000000
#define LIM_MIN_BW_11A     6000000
#define LIM_TOTAL_BW_11B   11000000
#define LIM_MIN_BW_11B     1000000
#define LIM_TOTAL_BW_11G   LIM_TOTAL_BW_11A
#define LIM_MIN_BW_11G     LIM_MIN_BW_11B

/* conversion factors */
#define LIM_CONVERT_SIZE_BITS(numBytes) ((numBytes) * 8)
#define LIM_CONVERT_RATE_MBPS(rate)     ((rate)/1000000)

/* ------------------------------------------------------------------------------ */
/* local protos */

static QDF_STATUS
lim_calculate_svc_int(tpAniSirGlobal, tSirMacTspecIE *, uint32_t *);
static QDF_STATUS
lim_validate_tspec_edca(tpAniSirGlobal, tSirMacTspecIE *, tpPESession);
static QDF_STATUS
lim_validate_tspec(tpAniSirGlobal, tSirMacTspecIE *, tpPESession);
static void
lim_compute_mean_bw_used(tpAniSirGlobal, uint32_t *, uint32_t, tpLimTspecInfo,
			 tpPESession);
static void lim_get_available_bw(tpAniSirGlobal, uint32_t *, uint32_t *, uint32_t,
				 uint32_t);
static QDF_STATUS lim_admit_policy_oversubscription(tpAniSirGlobal,
						       tSirMacTspecIE *,
						       tpLimAdmitPolicyInfo,
						       tpLimTspecInfo,
						       tpPESession);
static QDF_STATUS lim_tspec_find_by_sta_addr(tpAniSirGlobal, uint8_t *,
						tSirMacTspecIE *, tpLimTspecInfo,
						tpLimTspecInfo *);
static QDF_STATUS lim_validate_access_policy(tpAniSirGlobal, uint8_t, uint16_t,
						tpPESession);

/** -------------------------------------------------------------
   \fn lim_calculate_svc_int
   \brief TSPEC validation and servcie interval determination
   \param     tpAniSirGlobal    pMac
   \param         tSirMacTspecIE *pTspec
   \param         uint32_t            *pSvcInt
   \return QDF_STATUS - status of the comparison
   -------------------------------------------------------------*/

static QDF_STATUS
lim_calculate_svc_int(tpAniSirGlobal pMac,
		      tSirMacTspecIE *pTspec, uint32_t *pSvcInt)
{
	uint32_t msduSz, dataRate;
	*pSvcInt = 0;

	/* if a service interval is already specified, we are done */
	if ((pTspec->minSvcInterval != 0) || (pTspec->maxSvcInterval != 0)) {
		*pSvcInt = (pTspec->maxSvcInterval != 0)
			   ? pTspec->maxSvcInterval : pTspec->minSvcInterval;
		return QDF_STATUS_SUCCESS;
	}

	/* Masking off the fixed bits according to definition of MSDU size
	 * in IEEE 802.11-2007 spec (section 7.3.2.30). Nominal MSDU size
	 * is defined as:  Bit[0:14]=Size, Bit[15]=Fixed
	 */
	if (pTspec->nomMsduSz != 0)
		msduSz = (pTspec->nomMsduSz & 0x7fff);
	else if (pTspec->maxMsduSz != 0)
		msduSz = pTspec->maxMsduSz;
	else {
		pe_err("MsduSize not specified");
		return QDF_STATUS_E_FAILURE;
	}

	/* need to calculate a reasonable service interval
	 * this is simply the msduSz/meanDataRate
	 */
	if (pTspec->meanDataRate != 0)
		dataRate = pTspec->meanDataRate;
	else if (pTspec->peakDataRate != 0)
		dataRate = pTspec->peakDataRate;
	else if (pTspec->minDataRate != 0)
		dataRate = pTspec->minDataRate;
	else {
		pe_err("DataRate not specified");
		return QDF_STATUS_E_FAILURE;
	}

	*pSvcInt =
		LIM_CONVERT_SIZE_BITS(msduSz) / LIM_CONVERT_RATE_MBPS(dataRate);
	return QDF_STATUS_E_FAILURE;
}

/**
 * lim_validate_tspec_edca() - Validate the parameters
 * @mac_ctx: Global MAC context
 * @tspec:   Pointer to the TSPEC
 * @session_entry: Session Entry
 *
 * validate the parameters in the edca tspec
 * mandatory fields are derived from 11e Annex I (Table I.1)
 *
 * Return: Status
 **/
static QDF_STATUS
lim_validate_tspec_edca(tpAniSirGlobal mac_ctx,
			tSirMacTspecIE *tspec, tpPESession session_entry)
{
	uint32_t max_phy_rate, min_phy_rate;
	uint32_t phy_mode;
	QDF_STATUS retval = QDF_STATUS_SUCCESS;

	lim_get_phy_mode(mac_ctx, &phy_mode, session_entry);

	lim_get_available_bw(mac_ctx, &max_phy_rate, &min_phy_rate, phy_mode,
			     1 /* bandwidth mult factor */);
	/* mandatory fields are derived from 11e Annex I (Table I.1) */
	if ((tspec->nomMsduSz == 0) ||
	    (tspec->meanDataRate == 0) ||
	    (tspec->surplusBw == 0) ||
	    (tspec->minPhyRate == 0) ||
	    (tspec->minPhyRate > max_phy_rate)) {
		pe_warn("Invalid EDCA Tspec: NomMsdu: %d meanDataRate: %d surplusBw: %d min_phy_rate: %d",
			tspec->nomMsduSz, tspec->meanDataRate,
			tspec->surplusBw, tspec->minPhyRate);
		retval = QDF_STATUS_E_FAILURE;
	}

	pe_debug("return status: %d", retval);
	return retval;
}

/** -------------------------------------------------------------
   \fn lim_validate_tspec
   \brief validate the offered tspec
   \param   tpAniSirGlobal pMac
   \param         tSirMacTspecIE *pTspec
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

static QDF_STATUS
lim_validate_tspec(tpAniSirGlobal pMac,
		   tSirMacTspecIE *pTspec, tpPESession psessionEntry)
{
	QDF_STATUS retval = QDF_STATUS_SUCCESS;

	switch (pTspec->tsinfo.traffic.accessPolicy) {
	case SIR_MAC_ACCESSPOLICY_EDCA:
		retval = lim_validate_tspec_edca(pMac, pTspec, psessionEntry);
#ifdef WLAN_DEBUG
		if (retval != QDF_STATUS_SUCCESS)
			pe_warn("EDCA tspec invalid");
#endif
			break;

	case SIR_MAC_ACCESSPOLICY_HCCA:
	case SIR_MAC_ACCESSPOLICY_BOTH:
	/* TBD: should we support hybrid tspec as well?? for now, just fall through */
	default:
		pe_warn("AccessType: %d not supported",
			pTspec->tsinfo.traffic.accessPolicy);
		retval = QDF_STATUS_E_FAILURE;
		break;
	}
	return retval;
}

/* ----------------------------------------------------------------------------- */
/* Admit Control Policy */

/** -------------------------------------------------------------
   \fn lim_compute_mean_bw_used
   \brief determime the used/allocated bandwidth
   \param   tpAniSirGlobal pMac
   \param       uint32_t              *pBw
   \param       uint32_t               phyMode
   \param       tpLimTspecInfo    pTspecInfo
   \return void
   -------------------------------------------------------------*/

static void
lim_compute_mean_bw_used(tpAniSirGlobal pMac,
			 uint32_t *pBw,
			 uint32_t phyMode,
			 tpLimTspecInfo pTspecInfo, tpPESession psessionEntry)
{
	uint32_t ctspec;
	*pBw = 0;
	for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecInfo++) {
		if (pTspecInfo->inuse) {
			tpDphHashNode pSta =
				dph_get_hash_entry(pMac, pTspecInfo->assocId,
						   &psessionEntry->dph.dphHashTable);
			if (pSta == NULL) {
				/* maybe we should delete the tspec?? */
				pe_err("Tspec: %d assocId: %d dphNode not found",
					ctspec, pTspecInfo->assocId);
				continue;
			}
			*pBw += pTspecInfo->tspec.meanDataRate;
		}
	}
}

/** -------------------------------------------------------------
   \fn lim_get_available_bw
   \brief based on the phy mode and the bw_factor, determine the total bandwidth that
       can be supported
   \param   tpAniSirGlobal pMac
   \param       uint32_t              *pMaxBw
   \param       uint32_t              *pMinBw
   \param       uint32_t               phyMode
   \param       uint32_t               bw_factor
   \return void
   -------------------------------------------------------------*/

static void
lim_get_available_bw(tpAniSirGlobal pMac,
		     uint32_t *pMaxBw,
		     uint32_t *pMinBw, uint32_t phyMode, uint32_t bw_factor)
{
	switch (phyMode) {
	case WNI_CFG_PHY_MODE_11B:
		*pMaxBw = LIM_TOTAL_BW_11B;
		*pMinBw = LIM_MIN_BW_11B;
		break;

	case WNI_CFG_PHY_MODE_11A:
		*pMaxBw = LIM_TOTAL_BW_11A;
		*pMinBw = LIM_MIN_BW_11A;
		break;

	case WNI_CFG_PHY_MODE_11G:
	case WNI_CFG_PHY_MODE_NONE:
	default:
		*pMaxBw = LIM_TOTAL_BW_11G;
		*pMinBw = LIM_MIN_BW_11G;
		break;
	}
	*pMaxBw *= bw_factor;
}

/**
 * lim_admit_policy_oversubscription() - Admission control policy
 * @mac_ctx:        Global MAC Context
 * @tspec:          Pointer to the tspec
 * @admit_policy:   Admission policy
 * @tspec_info:     TSPEC information
 * @session_entry:  Session Entry
 *
 * simple admission control policy based on oversubscription
 * if the total bandwidth of all admitted tspec's exceeds (factor * phy-bw) then
 * reject the tspec, else admit it. The phy-bw is the peak available bw in the
 * current phy mode. The 'factor' is the configured oversubscription factor.
 *
 * Return: Status
 **/
static QDF_STATUS
lim_admit_policy_oversubscription(tpAniSirGlobal mac_ctx,
				  tSirMacTspecIE *tspec,
				  tpLimAdmitPolicyInfo admit_policy,
				  tpLimTspecInfo tspec_info,
				  tpPESession session_entry)
{
	uint32_t totalbw, minbw, usedbw;
	uint32_t phy_mode;

	/* determine total bandwidth used so far */
	lim_get_phy_mode(mac_ctx, &phy_mode, session_entry);

	lim_compute_mean_bw_used(mac_ctx, &usedbw, phy_mode,
			tspec_info, session_entry);

	/* determine how much bw is available based on the current phy mode */
	lim_get_available_bw(mac_ctx, &totalbw, &minbw, phy_mode,
			     admit_policy->bw_factor);

	if (usedbw > totalbw)   /* this can't possibly happen */
		return QDF_STATUS_E_FAILURE;

	if ((totalbw - usedbw) < tspec->meanDataRate) {
		pe_warn("Total BW: %d Used: %d Tspec request: %d not possible",
			totalbw, usedbw, tspec->meanDataRate);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_admit_policy
   \brief determine the current admit control policy and apply it for the offered tspec
   \param   tpAniSirGlobal pMac
   \param         tSirMacTspecIE   *pTspec
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

static QDF_STATUS lim_admit_policy(tpAniSirGlobal pMac,
				      tSirMacTspecIE *pTspec,
				      tpPESession psessionEntry)
{
	QDF_STATUS retval = QDF_STATUS_E_FAILURE;
	tpLimAdmitPolicyInfo pAdmitPolicy = &pMac->lim.admitPolicyInfo;

	switch (pAdmitPolicy->type) {
	case WNI_CFG_ADMIT_POLICY_ADMIT_ALL:
		retval = QDF_STATUS_SUCCESS;
		break;

	case WNI_CFG_ADMIT_POLICY_BW_FACTOR:
		retval = lim_admit_policy_oversubscription(pMac, pTspec,
							   &pMac->lim.
							   admitPolicyInfo,
							   &pMac->lim.tspecInfo[0],
							   psessionEntry);
#ifdef WLAN_DEBUG
		if (retval != QDF_STATUS_SUCCESS)
			pe_err("rejected by BWFactor policy");
#endif
			break;

	case WNI_CFG_ADMIT_POLICY_REJECT_ALL:
		retval = QDF_STATUS_E_FAILURE;
		break;

	default:
		retval = QDF_STATUS_SUCCESS;
		pe_warn("Admit Policy: %d unknown, admitting all traffic",
			pAdmitPolicy->type);
		break;
	}
	return retval;
}

/** -------------------------------------------------------------
   \fn lim_tspec_delete
   \brief delete the specified tspec
   \param   tpAniSirGlobal pMac
   \param     tpLimTspecInfo pInfo
   \return void
   -------------------------------------------------------------*/

/* ----------------------------------------------------------------------------- */
/* delete the specified tspec */
static void lim_tspec_delete(tpAniSirGlobal pMac, tpLimTspecInfo pInfo)
{
	if (pInfo == NULL)
		return;
	/* pierre */
	pe_debug("tspec entry: %d delete tspec: %pK", pInfo->idx, pInfo);
	pInfo->inuse = 0;

	return;
}

/** -------------------------------------------------------------
   \fn lim_tspec_find_by_sta_addr
   \brief Send halMsg_AddTs to HAL
   \param   tpAniSirGlobal pMac
   \param   \param       uint8_t               *pAddr
   \param       tSirMacTspecIE    *pTspecIE
   \param       tpLimTspecInfo    pTspecList
   \param       tpLimTspecInfo   *ppInfo
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

/* find the specified tspec in the list */
static QDF_STATUS
lim_tspec_find_by_sta_addr(tpAniSirGlobal pMac,
			   uint8_t *pAddr,
			   tSirMacTspecIE *pTspecIE,
			   tpLimTspecInfo pTspecList, tpLimTspecInfo *ppInfo)
{
	int ctspec;

	*ppInfo = NULL;

	for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecList++) {
		if ((pTspecList->inuse)
		    &&
		    (!qdf_mem_cmp
			     (pAddr, pTspecList->staAddr, sizeof(pTspecList->staAddr)))
		    &&
		    (!qdf_mem_cmp
			     ((uint8_t *) pTspecIE, (uint8_t *) &pTspecList->tspec,
			     sizeof(tSirMacTspecIE)))) {
			*ppInfo = pTspecList;
			return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
}

/** -------------------------------------------------------------
   \fn lim_tspec_find_by_assoc_id
   \brief find tspec with matchin staid and Tspec
   \param   tpAniSirGlobal pMac
   \param       uint32_t               staid
   \param       tSirMacTspecIE    *pTspecIE
   \param       tpLimTspecInfo    pTspecList
   \param       tpLimTspecInfo   *ppInfo
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

QDF_STATUS
lim_tspec_find_by_assoc_id(tpAniSirGlobal pMac,
			   uint16_t assocId,
			   tSirMacTspecIE *pTspecIE,
			   tpLimTspecInfo pTspecList, tpLimTspecInfo *ppInfo)
{
	int ctspec;

	*ppInfo = NULL;

	pe_debug("Trying to find tspec entry for assocId: %d pTsInfo->traffic.direction: %d pTsInfo->traffic.tsid: %d",
		assocId, pTspecIE->tsinfo.traffic.direction,
		pTspecIE->tsinfo.traffic.tsid);

	for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecList++) {
		if ((pTspecList->inuse)
		    && (assocId == pTspecList->assocId)
		    &&
		    (!qdf_mem_cmp
			     ((uint8_t *) pTspecIE, (uint8_t *) &pTspecList->tspec,
			     sizeof(tSirMacTspecIE)))) {
			*ppInfo = pTspecList;
			return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
}

/** -------------------------------------------------------------
   \fn lim_find_tspec
   \brief finding a TSPEC entry with assocId, tsinfo.direction and tsinfo.tsid
   \param    uint16_t               assocId
   \param     tpAniSirGlobal    pMac
   \param     tSirMacTSInfo   *pTsInfo
   \param         tpLimTspecInfo    pTspecList
   \param         tpLimTspecInfo   *ppInfo
   \return QDF_STATUS - status of the comparison
   -------------------------------------------------------------*/

static QDF_STATUS
lim_find_tspec(tpAniSirGlobal pMac,
	       uint16_t assocId,
	       tSirMacTSInfo *pTsInfo,
	       tpLimTspecInfo pTspecList, tpLimTspecInfo *ppInfo)
{
	int ctspec;

	*ppInfo = NULL;

	pe_debug("Trying to find tspec entry for assocId: %d pTsInfo->traffic.direction: %d pTsInfo->traffic.tsid: %d",
		assocId, pTsInfo->traffic.direction, pTsInfo->traffic.tsid);

	for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecList++) {
		if ((pTspecList->inuse)
		    && (assocId == pTspecList->assocId)
		    && (pTsInfo->traffic.direction ==
			pTspecList->tspec.tsinfo.traffic.direction)
		    && (pTsInfo->traffic.tsid ==
			pTspecList->tspec.tsinfo.traffic.tsid)) {
			*ppInfo = pTspecList;
			return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
}

/** -------------------------------------------------------------
   \fn lim_tspec_add
   \brief add or update the specified tspec to the tspec list
   \param tpAniSirGlobal    pMac
   \param uint8_t               *pAddr
   \param uint16_t               assocId
   \param tSirMacTspecIE   *pTspec
   \param uint32_t               interval
   \param tpLimTspecInfo   *ppInfo

   \return QDF_STATUS - status of the comparison
   -------------------------------------------------------------*/

QDF_STATUS lim_tspec_add(tpAniSirGlobal pMac,
			    uint8_t *pAddr,
			    uint16_t assocId,
			    tSirMacTspecIE *pTspec,
			    uint32_t interval, tpLimTspecInfo *ppInfo)
{
	tpLimTspecInfo pTspecList = &pMac->lim.tspecInfo[0];
	*ppInfo = NULL;

	/* validate the assocId */
	if (assocId >= pMac->lim.maxStation) {
		pe_err("Invalid assocId 0x%x", assocId);
		return QDF_STATUS_E_FAILURE;
	}
	/* decide whether to add/update */
	{
		*ppInfo = NULL;

		if (QDF_STATUS_SUCCESS ==
		    lim_find_tspec(pMac, assocId, &pTspec->tsinfo, pTspecList,
				   ppInfo)) {
			/* update this entry. */
			pe_debug("updating TSPEC table entry: %d",
				(*ppInfo)->idx);
		} else {
			/* We didn't find one to update. So find a free slot in the
			 * LIM TSPEC list and add this new entry
			 */
			uint8_t ctspec = 0;

			for (ctspec = 0, pTspecList = &pMac->lim.tspecInfo[0];
			     ctspec < LIM_NUM_TSPEC_MAX;
			     ctspec++, pTspecList++) {
				if (!pTspecList->inuse) {
					pe_debug("Found free slot in TSPEC list. Add to TSPEC table entry: %d",
						ctspec);
					break;
				}
			}

			if (ctspec >= LIM_NUM_TSPEC_MAX)
				return QDF_STATUS_E_FAILURE;

			/* Record the new index entry */
			pTspecList->idx = ctspec;
		}
	}

	/* update the tspec info */
	pTspecList->tspec = *pTspec;
	pTspecList->assocId = assocId;
	qdf_mem_copy(pTspecList->staAddr, pAddr, sizeof(pTspecList->staAddr));

	/* for edca tspec's, we are all done */
	if (pTspec->tsinfo.traffic.accessPolicy == SIR_MAC_ACCESSPOLICY_EDCA) {
		pTspecList->inuse = 1;
		*ppInfo = pTspecList;
		pe_debug("added entry for EDCA AccessPolicy");
		return QDF_STATUS_SUCCESS;
	}

	/*
	 * for hcca tspec's, must set the parameterized bit in the queues
	 * the 'ts' bit in the queue data structure indicates that the queue is
	 * parameterized (hcca). When the schedule is written this bit is used
	 * in the tsid field (bit 3) and the other three bits (0-2) are simply
	 * filled in as the user priority (or qid). This applies only to uplink
	 * polls where the qos control field must contain the tsid specified in the
	 * tspec.
	 */
	pTspecList->inuse = 1;
	*ppInfo = pTspecList;
	pe_debug("added entry for HCCA AccessPolicy");
	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_validate_access_policy
   \brief Validates Access policy
   \param   tpAniSirGlobal pMac
   \param       uint8_t              accessPolicy
   \param       uint16_t             assocId
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

static QDF_STATUS
lim_validate_access_policy(tpAniSirGlobal pMac,
			   uint8_t accessPolicy,
			   uint16_t assocId, tpPESession psessionEntry)
{
	QDF_STATUS retval = QDF_STATUS_E_FAILURE;
	tpDphHashNode pSta =
		dph_get_hash_entry(pMac, assocId, &psessionEntry->dph.dphHashTable);

	if ((pSta == NULL) || (!pSta->valid)) {
		pe_err("invalid station address passed");
		return QDF_STATUS_E_FAILURE;
	}

	switch (accessPolicy) {
	case SIR_MAC_ACCESSPOLICY_EDCA:
		if (pSta->wmeEnabled || pSta->lleEnabled)
			retval = QDF_STATUS_SUCCESS;
		break;

	case SIR_MAC_ACCESSPOLICY_HCCA:
	case SIR_MAC_ACCESSPOLICY_BOTH:
	default:
		pe_err("Invalid accessPolicy: %d",
			       accessPolicy);
		break;
	}

	if (retval != QDF_STATUS_SUCCESS)
		pe_warn("accPol: %d staId: %d lle: %d wme: %d wsm: %d",
			accessPolicy, pSta->staIndex, pSta->lleEnabled,
			pSta->wmeEnabled, pSta->wsmEnabled);

	return retval;
}

/**
 * lim_admit_control_add_ts() -        Check if STA can be admitted
 * @pMac:               Global MAC context
 * @pAddr:              Address
 * @pAddts:             ADD TS
 * @pQos:               QOS fields
 * @assocId:            Association ID
 * @alloc:              Allocate bandwidth for this tspec
 * @pSch:               Schedule IE
 * @pTspecIdx:          TSPEC index
 * @psessionEntry:      PE Session Entry
 *
 * Determine if STA with the specified TSPEC can be admitted. If it can,
 * a schedule element is provided
 *
 * Return: status
 **/
QDF_STATUS lim_admit_control_add_ts(tpAniSirGlobal pMac, uint8_t *pAddr,
		tSirAddtsReqInfo *pAddts, tSirMacQosCapabilityStaIE *pQos,
		uint16_t assocId, uint8_t alloc, tSirMacScheduleIE *pSch,
		uint8_t *pTspecIdx, tpPESession psessionEntry)
{
	tpLimTspecInfo pTspecInfo;
	QDF_STATUS retval;
	uint32_t svcInterval;
	(void)pQos;

	/* TBD: modify tspec as needed */
	/* EDCA: need to fill in the medium time and the minimum phy rate */
	/* to be consistent with the desired traffic parameters. */

	pe_debug("tsid: %d directn: %d start: %d intvl: %d accPolicy: %d up: %d",
		pAddts->tspec.tsinfo.traffic.tsid,
		pAddts->tspec.tsinfo.traffic.direction,
		pAddts->tspec.svcStartTime, pAddts->tspec.minSvcInterval,
		pAddts->tspec.tsinfo.traffic.accessPolicy,
		pAddts->tspec.tsinfo.traffic.userPrio);

	/* check for duplicate tspec */
	retval = (alloc)
		 ? lim_tspec_find_by_assoc_id(pMac, assocId, &pAddts->tspec,
					      &pMac->lim.tspecInfo[0], &pTspecInfo)
		 : lim_tspec_find_by_sta_addr(pMac, pAddr, &pAddts->tspec,
					      &pMac->lim.tspecInfo[0], &pTspecInfo);

	if (retval == QDF_STATUS_SUCCESS) {
		pe_err("duplicate tspec index: %d", pTspecInfo->idx);
		return QDF_STATUS_E_FAILURE;
	}
	/* check that the tspec's are well formed and acceptable */
	if (lim_validate_tspec(pMac, &pAddts->tspec, psessionEntry) !=
	    QDF_STATUS_SUCCESS) {
		pe_warn("tspec validation failed");
		return QDF_STATUS_E_FAILURE;
	}
	/* determine a service interval for the tspec */
	if (lim_calculate_svc_int(pMac, &pAddts->tspec, &svcInterval) !=
	    QDF_STATUS_SUCCESS) {
		pe_warn("SvcInt calculate failed");
		return QDF_STATUS_E_FAILURE;
	}
	/* determine if the tspec can be admitted or not based on current policy */
	if (lim_admit_policy(pMac, &pAddts->tspec, psessionEntry) != QDF_STATUS_SUCCESS) {
		pe_warn("tspec rejected by admit control policy");
		return QDF_STATUS_E_FAILURE;
	}
	/* fill in a schedule if requested */
	if (pSch != NULL) {
		qdf_mem_zero((uint8_t *) pSch, sizeof(*pSch));
		pSch->svcStartTime = pAddts->tspec.svcStartTime;
		pSch->svcInterval = svcInterval;
		pSch->maxSvcDuration = (uint16_t) pSch->svcInterval;    /* use SP = SI */
		pSch->specInterval = 0x1000;    /* fixed for now: TBD */

		pSch->info.direction = pAddts->tspec.tsinfo.traffic.direction;
		pSch->info.tsid = pAddts->tspec.tsinfo.traffic.tsid;
		pSch->info.aggregation = 0;     /* no support for aggregation for now: TBD */
	}
	/* if no allocation is requested, done */
	if (!alloc)
		return QDF_STATUS_SUCCESS;

	/* check that we are in the proper mode to deal with the tspec type */
	if (lim_validate_access_policy
		    (pMac, (uint8_t) pAddts->tspec.tsinfo.traffic.accessPolicy, assocId,
		    psessionEntry) != QDF_STATUS_SUCCESS) {
		pe_warn("AccessPolicy: %d is not valid in current mode",
			pAddts->tspec.tsinfo.traffic.accessPolicy);
		return QDF_STATUS_E_FAILURE;
	}
	/* add tspec to list */
	if (lim_tspec_add
		    (pMac, pAddr, assocId, &pAddts->tspec, svcInterval, &pTspecInfo)
	    != QDF_STATUS_SUCCESS) {
		pe_err("no space in tspec list");
		return QDF_STATUS_E_FAILURE;
	}
	/* passing lim tspec table index to the caller */
	*pTspecIdx = pTspecInfo->idx;

	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_admit_control_delete_ts
   \brief Delete the specified Tspec for the specified STA
   \param   tpAniSirGlobal pMac
   \param       uint16_t               assocId
   \param       tSirMacTSInfo    *pTsInfo
   \param       uint8_t               *pTsStatus
   \param       uint8_t             *ptspecIdx
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

QDF_STATUS
lim_admit_control_delete_ts(tpAniSirGlobal pMac,
			    uint16_t assocId,
			    tSirMacTSInfo *pTsInfo,
			    uint8_t *pTsStatus, uint8_t *ptspecIdx)
{
	tpLimTspecInfo pTspecInfo = NULL;

	if (pTsStatus != NULL)
		*pTsStatus = 0;

	if (lim_find_tspec
		    (pMac, assocId, pTsInfo, &pMac->lim.tspecInfo[0],
		    &pTspecInfo) == QDF_STATUS_SUCCESS) {
		if (pTspecInfo != NULL) {
			pe_debug("Tspec entry: %d found", pTspecInfo->idx);

			*ptspecIdx = pTspecInfo->idx;
			lim_tspec_delete(pMac, pTspecInfo);
			return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
}

/** -------------------------------------------------------------
   \fn lim_admit_control_delete_sta
   \brief Delete all TSPEC for the specified STA
   \param   tpAniSirGlobal pMac
   \param     uint16_t assocId
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

QDF_STATUS lim_admit_control_delete_sta(tpAniSirGlobal pMac, uint16_t assocId)
{
	tpLimTspecInfo pTspecInfo = &pMac->lim.tspecInfo[0];
	int ctspec;

	for (ctspec = 0; ctspec < LIM_NUM_TSPEC_MAX; ctspec++, pTspecInfo++) {
		if (assocId == pTspecInfo->assocId) {
			lim_tspec_delete(pMac, pTspecInfo);
			pe_debug("Deleting TSPEC: %d for assocId: %d", ctspec,
				assocId);
		}
	}
	pe_debug("assocId: %d done", assocId);

	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_admit_control_init
   \brief init tspec table
   \param   tpAniSirGlobal pMac
   \return QDF_STATUS - status
   -------------------------------------------------------------*/
QDF_STATUS lim_admit_control_init(tpAniSirGlobal pMac)
{
	qdf_mem_zero(pMac->lim.tspecInfo,
		    LIM_NUM_TSPEC_MAX * sizeof(tLimTspecInfo));
	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_update_admit_policy
   \brief Set the admit control policy based on CFG parameters
   \param   tpAniSirGlobal pMac
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

QDF_STATUS lim_update_admit_policy(tpAniSirGlobal pMac)
{
	uint32_t val;

	if (wlan_cfg_get_int(pMac, WNI_CFG_ADMIT_POLICY, &val) != QDF_STATUS_SUCCESS) {
		pe_err("Unable to get CFG_ADMIT_POLICY");
		return QDF_STATUS_E_FAILURE;
	}
	pMac->lim.admitPolicyInfo.type = (uint8_t) val;
	if (wlan_cfg_get_int(pMac, WNI_CFG_ADMIT_BWFACTOR, &val) != QDF_STATUS_SUCCESS) {
		pe_err("Unable to get CFG_ADMIT_BWFACTOR");
		return QDF_STATUS_E_FAILURE;
	}
	pMac->lim.admitPolicyInfo.bw_factor = (uint8_t) val;

	pe_debug("LIM: AdmitPolicy: %d bw_factor: %d",
		       pMac->lim.admitPolicyInfo.type,
		       pMac->lim.admitPolicyInfo.bw_factor);

	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_send_hal_msg_add_ts
   \brief Send halMsg_AddTs to HAL
   \param   tpAniSirGlobal pMac
   \param     uint16_t        staIdx
   \param     uint8_t         tspecIdx
   \param       tSirMacTspecIE tspecIE
   \param       tSirTclasInfo   *tclasInfo
   \param       uint8_t           tclasProc
   \param       uint16_t          tsm_interval
   \return QDF_STATUS - status
   -------------------------------------------------------------*/
#ifdef FEATURE_WLAN_ESE
QDF_STATUS
lim_send_hal_msg_add_ts(tpAniSirGlobal pMac,
			uint16_t staIdx,
			uint8_t tspecIdx,
			tSirMacTspecIE tspecIE,
			uint8_t sessionId, uint16_t tsm_interval)
#else
QDF_STATUS
lim_send_hal_msg_add_ts(tpAniSirGlobal pMac,
			uint16_t staIdx,
			uint8_t tspecIdx, tSirMacTspecIE tspecIE, uint8_t sessionId)
#endif
{
	struct scheduler_msg msg = {0};
	tpAddTsParams pAddTsParam;

	tpPESession psessionEntry = pe_find_session_by_session_id(pMac, sessionId);

	if (psessionEntry == NULL) {
		pe_err("Unable to get Session for session Id: %d",
			sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	pAddTsParam = qdf_mem_malloc(sizeof(tAddTsParams));
	if (NULL == pAddTsParam) {
		pe_err("AllocateMemory() failed");
		return QDF_STATUS_E_NOMEM;
	}

	pAddTsParam->staIdx = staIdx;
	pAddTsParam->tspecIdx = tspecIdx;
	qdf_mem_copy(&pAddTsParam->tspec, &tspecIE, sizeof(tSirMacTspecIE));
	pAddTsParam->sessionId = sessionId;
	pAddTsParam->sme_session_id = psessionEntry->smeSessionId;

#ifdef FEATURE_WLAN_ESE
	pAddTsParam->tsm_interval = tsm_interval;
#endif
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	if (pMac->roam.configParam.isRoamOffloadEnabled &&
	    psessionEntry->is11Rconnection)
		pAddTsParam->setRICparams = 1;
#endif

	msg.type = WMA_ADD_TS_REQ;
	msg.bodyptr = pAddTsParam;
	msg.bodyval = 0;

	/* We need to defer any incoming messages until we get a
	 * WMA_ADD_TS_RSP from HAL.
	 */
	SET_LIM_PROCESS_DEFD_MESGS(pMac, false);
	MTRACE(mac_trace_msg_tx(pMac, sessionId, msg.type));

	if (QDF_STATUS_SUCCESS != wma_post_ctrl_msg(pMac, &msg)) {
		pe_warn("wma_post_ctrl_msg() failed");
		SET_LIM_PROCESS_DEFD_MESGS(pMac, true);
		qdf_mem_free(pAddTsParam);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/** -------------------------------------------------------------
   \fn lim_send_hal_msg_del_ts
   \brief Send halMsg_AddTs to HAL
   \param   tpAniSirGlobal pMac
   \param     uint16_t        staIdx
   \param     uint8_t         tspecIdx
   \param     tSirAddtsReqInfo addts
   \return QDF_STATUS - status
   -------------------------------------------------------------*/

QDF_STATUS
lim_send_hal_msg_del_ts(tpAniSirGlobal pMac,
			uint16_t staIdx,
			uint8_t tspecIdx,
			tSirDeltsReqInfo delts, uint8_t sessionId, uint8_t *bssId)
{
	struct scheduler_msg msg = {0};
	tpDelTsParams pDelTsParam;
	tpPESession psessionEntry = NULL;

	pDelTsParam = qdf_mem_malloc(sizeof(tDelTsParams));
	if (NULL == pDelTsParam) {
		pe_err("AllocateMemory() failed");
		return QDF_STATUS_E_NOMEM;
	}

	msg.type = WMA_DEL_TS_REQ;
	msg.bodyptr = pDelTsParam;
	msg.bodyval = 0;

	/* filling message parameters. */
	pDelTsParam->staIdx = staIdx;
	pDelTsParam->tspecIdx = tspecIdx;
	qdf_mem_copy(&pDelTsParam->bssId, bssId, sizeof(tSirMacAddr));

	psessionEntry = pe_find_session_by_session_id(pMac, sessionId);
	if (psessionEntry == NULL) {
		pe_err("Session does Not exist with given sessionId: %d",
			       sessionId);
		goto err;
	}
	pDelTsParam->sessionId = psessionEntry->smeSessionId;
	pDelTsParam->userPrio = delts.wmeTspecPresent ?
			delts.tspec.tsinfo.traffic.userPrio :
			delts.tsinfo.traffic.userPrio;

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	if (pMac->roam.configParam.isRoamOffloadEnabled &&
	    psessionEntry->is11Rconnection) {
		qdf_mem_copy(&pDelTsParam->delTsInfo, &delts,
			     sizeof(tSirDeltsReqInfo));
		pDelTsParam->setRICparams = 1;
	}
#endif
	MTRACE(mac_trace_msg_tx(pMac, sessionId, msg.type));

	if (QDF_STATUS_SUCCESS != wma_post_ctrl_msg(pMac, &msg)) {
		pe_warn("wma_post_ctrl_msg() failed");
		goto err;
	}
	return QDF_STATUS_SUCCESS;

err:
	qdf_mem_free(pDelTsParam);
	return QDF_STATUS_E_FAILURE;
}

/** -------------------------------------------------------------
   \fn     lim_process_hal_add_ts_rsp
   \brief  This function process the WMA_ADD_TS_RSP from HAL.
 \       If response is successful, then send back SME_ADDTS_RSP.
 \       Otherwise, send DELTS action frame to peer and then
 \       then send back SME_ADDTS_RSP.
 \
   \param  tpAniSirGlobal  pMac
   \param  struct scheduler_msg *limMsg
   -------------------------------------------------------------*/
void lim_process_hal_add_ts_rsp(tpAniSirGlobal pMac,
				struct scheduler_msg *limMsg)
{
	tpAddTsParams pAddTsRspMsg = NULL;
	tpDphHashNode pSta = NULL;
	uint16_t assocId = 0;
	tSirMacAddr peerMacAddr;
	uint8_t rspReqd = 1;
	tpPESession psessionEntry = NULL;

	/* Need to process all the deferred messages enqueued
	 * since sending the WMA_ADD_TS_REQ.
	 */
	SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

	if (NULL == limMsg->bodyptr) {
		pe_err("Received WMA_ADD_TS_RSP with NULL");
		goto end;
	}

	pAddTsRspMsg = (tpAddTsParams) (limMsg->bodyptr);

	/* 090803: Use pe_find_session_by_session_id() to obtain the PE session context */
	/* from the sessionId in the Rsp Msg from HAL */
	psessionEntry = pe_find_session_by_session_id(pMac, pAddTsRspMsg->sessionId);

	if (psessionEntry == NULL) {
		pe_err("Session does Not exist with given sessionId: %d",
			       pAddTsRspMsg->sessionId);
		lim_send_sme_addts_rsp(pMac, rspReqd, eSIR_SME_ADDTS_RSP_FAILED,
				       psessionEntry, pAddTsRspMsg->tspec,
				       pMac->lim.gLimAddtsReq.sessionId,
				       pMac->lim.gLimAddtsReq.transactionId);
		goto end;
	}

	if (pAddTsRspMsg->status == QDF_STATUS_SUCCESS) {
		pe_debug("Received successful ADDTS response from HAL");
		/* Use the smesessionId and smetransactionId from the PE session context */
		lim_send_sme_addts_rsp(pMac, rspReqd, eSIR_SME_SUCCESS,
				       psessionEntry, pAddTsRspMsg->tspec,
				       psessionEntry->smeSessionId,
				       psessionEntry->transactionId);
		goto end;
	} else {
		pe_debug("Received failure ADDTS response from HAL");
		/* Send DELTS action frame to AP */
		/* 090803: Get peer MAC addr from session */
		sir_copy_mac_addr(peerMacAddr, psessionEntry->bssId);

		/* 090803: Add the SME Session ID */
		lim_send_delts_req_action_frame(pMac, peerMacAddr, rspReqd,
						&pAddTsRspMsg->tspec.tsinfo,
						&pAddTsRspMsg->tspec, psessionEntry);

		/* Delete TSPEC */
		/* 090803: Pull the hash table from the session */
		pSta = dph_lookup_assoc_id(pMac, pAddTsRspMsg->staIdx, &assocId,
					   &psessionEntry->dph.dphHashTable);
		if (pSta != NULL)
			lim_admit_control_delete_ts(pMac, assocId,
						    &pAddTsRspMsg->tspec.tsinfo,
						    NULL,
						    (uint8_t *) &pAddTsRspMsg->
						    tspecIdx);

		/* Send SME_ADDTS_RSP */
		/* 090803: Use the smesessionId and smetransactionId from the PE session context */
		lim_send_sme_addts_rsp(pMac, rspReqd, eSIR_SME_ADDTS_RSP_FAILED,
				       psessionEntry, pAddTsRspMsg->tspec,
				       psessionEntry->smeSessionId,
				       psessionEntry->transactionId);
		goto end;
	}

end:
	if (pAddTsRspMsg != NULL)
		qdf_mem_free(pAddTsRspMsg);
	return;
}
