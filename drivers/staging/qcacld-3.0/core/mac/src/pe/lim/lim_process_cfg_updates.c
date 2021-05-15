/*
 * Copyright (c) 2012-2018, 2020 The Linux Foundation. All rights reserved.
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
 * This file lim_process_cfg_updates.cc contains the utility functions
 * to handle various CFG parameter update events
 * Author:        Chandra Modumudi
 * Date:          01/20/03
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#include "ani_global.h"

#include "wni_cfg.h"
#include "sir_mac_prot_def.h"
#include "cfg_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_prop_exts_utils.h"
#include "sch_api.h"
#include "rrm_api.h"

static void lim_update_config(tpAniSirGlobal pMac, tpPESession psessionEntry);

/** -------------------------------------------------------------
   \fn lim_set_cfg_protection
   \brief sets lim global cfg cache from the config.
   \param      tpAniSirGlobal    pMac
   \return      None
   -------------------------------------------------------------*/
void lim_set_cfg_protection(tpAniSirGlobal pMac, tpPESession pesessionEntry)
{
	uint32_t val = 0;

	if (pesessionEntry != NULL && LIM_IS_AP_ROLE(pesessionEntry)) {
		if (pesessionEntry->gLimProtectionControl ==
		    WNI_CFG_FORCE_POLICY_PROTECTION_DISABLE)
			qdf_mem_zero((void *)&pesessionEntry->cfgProtection,
				    sizeof(tCfgProtection));
		else {
			pe_debug("frm11a = %d, from11b = %d, frm11g = %d, "
				   "ht20 = %d, nongf = %d, lsigTxop = %d, "
				   "rifs = %d, obss = %d",
				pesessionEntry->cfgProtection.fromlla,
				pesessionEntry->cfgProtection.fromllb,
				pesessionEntry->cfgProtection.fromllg,
				pesessionEntry->cfgProtection.ht20,
				pesessionEntry->cfgProtection.nonGf,
				pesessionEntry->cfgProtection.lsigTxop,
				pesessionEntry->cfgProtection.rifs,
				pesessionEntry->cfgProtection.obss);
		}
	} else {
		if (wlan_cfg_get_int(pMac, WNI_CFG_FORCE_POLICY_PROTECTION, &val)
		    != QDF_STATUS_SUCCESS) {
			pe_err("reading WNI_CFG_FORCE_POLICY_PROTECTION cfg failed");
			return;
		} else
			pMac->lim.gLimProtectionControl = (uint8_t) val;

		if (wlan_cfg_get_int(pMac, WNI_CFG_PROTECTION_ENABLED, &val) !=
		    QDF_STATUS_SUCCESS) {
			pe_err("reading protection cfg failed");
			return;
		}

		if (pMac->lim.gLimProtectionControl ==
		    WNI_CFG_FORCE_POLICY_PROTECTION_DISABLE)
			qdf_mem_zero((void *)&pMac->lim.cfgProtection,
				    sizeof(tCfgProtection));
		else {
			pMac->lim.cfgProtection.fromlla =
				(val >> WNI_CFG_PROTECTION_ENABLED_FROM_llA) & 1;
			pMac->lim.cfgProtection.fromllb =
				(val >> WNI_CFG_PROTECTION_ENABLED_FROM_llB) & 1;
			pMac->lim.cfgProtection.fromllg =
				(val >> WNI_CFG_PROTECTION_ENABLED_FROM_llG) & 1;
			pMac->lim.cfgProtection.ht20 =
				(val >> WNI_CFG_PROTECTION_ENABLED_HT_20) & 1;
			pMac->lim.cfgProtection.nonGf =
				(val >> WNI_CFG_PROTECTION_ENABLED_NON_GF) & 1;
			pMac->lim.cfgProtection.lsigTxop =
				(val >> WNI_CFG_PROTECTION_ENABLED_LSIG_TXOP) & 1;
			pMac->lim.cfgProtection.rifs =
				(val >> WNI_CFG_PROTECTION_ENABLED_RIFS) & 1;
			pMac->lim.cfgProtection.obss =
				(val >> WNI_CFG_PROTECTION_ENABLED_OBSS) & 1;

		}
	}
}

/**
 * lim_handle_param_update()
 *
 ***FUNCTION:
 * This function is use to post a message whenever need indicate
 * there is update of config parameter.
 *
 ***PARAMS:
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 *
 * @param  pMac  - Pointer to Global MAC structure
 * @param  cfgId - ID of CFG parameter that got updated
 * @return None
 */
void lim_handle_param_update(tpAniSirGlobal pMac, eUpdateIEsType cfgId)
{
	struct scheduler_msg msg = { 0 };
	QDF_STATUS status;

	pe_debug("Handling CFG parameter id %X update", cfgId);

	switch (cfgId) {
	case eUPDATE_IE_PROBE_BCN:
	{
		msg.type = SIR_LIM_UPDATE_BEACON;
		status = lim_post_msg_api(pMac, &msg);

#ifdef WLAN_DEBUG
		if (status != QDF_STATUS_SUCCESS)
			pe_err("Failed lim_post_msg_api %u", status);
#endif
			break;
	}
	default:
		break;
	}
}

/**
 * lim_handle_cf_gparam_update()
 *
 ***FUNCTION:
 * This function is called by lim_process_messages() to
 * whenever SIR_CFG_PARAM_UPDATE_IND message is posted
 * to LIM (due to a set operation on a CFG parameter).
 *
 ***PARAMS:
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 *
 * @param  pMac  - Pointer to Global MAC structure
 * @param  cfgId - ID of CFG parameter that got updated
 * @return None
 */

void lim_handle_cf_gparam_update(tpAniSirGlobal pMac, uint32_t cfgId)
{
	uint32_t val1, val2;
	uint16_t val16;
	tSirMacHTParametersInfo *pAmpduParamInfo;

	switch (cfgId) {
	case WNI_CFG_ASSOCIATION_FAILURE_TIMEOUT:
		if (pMac->lim.gLimMlmState != eLIM_MLM_WT_ASSOC_RSP_STATE) {
			/* 'Change' timer for future activations */
			lim_deactivate_and_change_timer(pMac,
							eLIM_ASSOC_FAIL_TIMER);
		}

		break;

	case WNI_CFG_PROTECTION_ENABLED:
		lim_set_cfg_protection(pMac, NULL);
		break;

	case WNI_CFG_MPDU_DENSITY:
		if (wlan_cfg_get_int(pMac, WNI_CFG_HT_AMPDU_PARAMS, &val1) !=
		    QDF_STATUS_SUCCESS) {
			pe_err("could not retrieve HT AMPDU Param CFG");
			break;
		}
		if (wlan_cfg_get_int(pMac, WNI_CFG_MPDU_DENSITY, &val2) !=
		    QDF_STATUS_SUCCESS) {
			pe_err("could not retrieve MPDU Density CFG");
			break;
		}
		val16 = (uint16_t) val1;
		pAmpduParamInfo = (tSirMacHTParametersInfo *) &val16;
		pAmpduParamInfo->mpduDensity = (uint8_t) val2;
#ifdef WLAN_DEBUG
		if (cfg_set_int
			    (pMac, WNI_CFG_HT_AMPDU_PARAMS,
			    *(uint8_t *) pAmpduParamInfo) != QDF_STATUS_SUCCESS)
			pe_err("could not update HT AMPDU Param CFG");
#endif
			break;
	case WNI_CFG_MAX_RX_AMPDU_FACTOR:
		if (wlan_cfg_get_int(pMac, WNI_CFG_HT_AMPDU_PARAMS, &val1) !=
		    QDF_STATUS_SUCCESS) {
			pe_err("could not retrieve HT AMPDU Param CFG");
			break;
		}
		if (wlan_cfg_get_int(pMac, WNI_CFG_MAX_RX_AMPDU_FACTOR, &val2) !=
		    QDF_STATUS_SUCCESS) {
			pe_err("could not retrieve AMPDU Factor CFG");
			break;
		}
		val16 = (uint16_t) val1;
		pAmpduParamInfo = (tSirMacHTParametersInfo *) &val16;
		pAmpduParamInfo->maxRxAMPDUFactor = (uint8_t) val2;
#ifdef WLAN_DEBUG
		if (cfg_set_int
			    (pMac, WNI_CFG_HT_AMPDU_PARAMS,
			    *(uint8_t *) pAmpduParamInfo) != QDF_STATUS_SUCCESS)
			pe_err("could not update HT AMPDU Param CFG");
#endif
			break;

	case WNI_CFG_DOT11_MODE:
		if (wlan_cfg_get_int(pMac, WNI_CFG_DOT11_MODE, &val1) !=
		    QDF_STATUS_SUCCESS) {
			pe_err("could not retrieve Dot11 Mode CFG");
			break;
		}
		break;

	case WNI_CFG_ASSOC_STA_LIMIT:
		if (wlan_cfg_get_int(pMac, WNI_CFG_ASSOC_STA_LIMIT, &val1) !=
		    QDF_STATUS_SUCCESS) {
			pe_err("Unable to get WNI_CFG_ASSOC_STA_LIMIT");
			break;
		}
		pMac->lim.gLimAssocStaLimit = (uint16_t) val1;
		break;
	default:
		break;
	}
} /*** end lim_handle_cf_gparam_update() ***/

/**
 * lim_apply_configuration()
 *
 ***FUNCTION:
 * This function is called to apply the configured parameters
 * before joining or reassociating with a BSS or starting a BSS.
 *
 ***PARAMS:
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 *
 * @param  pMac  - Pointer to Global MAC structure
 * @return None
 */

void lim_apply_configuration(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
	uint32_t val = 0, phyMode;

	psessionEntry->limSentCapsChangeNtf = false;

	lim_get_phy_mode(pMac, &phyMode, psessionEntry);

	lim_update_config(pMac, psessionEntry);

	lim_get_short_slot_from_phy_mode(pMac, psessionEntry, phyMode,
					 &psessionEntry->shortSlotTimeSupported);

	lim_set_cfg_protection(pMac, psessionEntry);

	/* Added for BT - AMP Support */
	if (LIM_IS_AP_ROLE(psessionEntry) ||
	    LIM_IS_IBSS_ROLE(psessionEntry)) {
		/* This check is required to ensure the beacon generation is not done
		   as a part of join request for a BT-AMP station */

		if (psessionEntry->statypeForBss == STA_ENTRY_SELF) {
			sch_set_beacon_interval(pMac, psessionEntry);
			sch_set_fixed_beacon_fields(pMac, psessionEntry);
		}
	}

	if (wlan_cfg_get_int(pMac, WNI_CFG_SCAN_IN_POWERSAVE, &val) !=
	    QDF_STATUS_SUCCESS) {
		pe_err("could not retrieve WNI_CFG_SCAN_IN_POWERSAVE");
		return;
	}
} /*** end lim_apply_configuration() ***/

/**
 * lim_update_config
 *
 * FUNCTION:
 * Update the local state from CFG database
 * (This used to be dphUpdateConfig)
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param None
 * @return None
 */

static void lim_update_config(tpAniSirGlobal pMac, tpPESession psessionEntry)
{
	uint32_t val;

	if (wlan_cfg_get_int(pMac, WNI_CFG_SHORT_PREAMBLE, &val) != QDF_STATUS_SUCCESS)
		pe_err("cfg get short preamble failed");
	psessionEntry->beaconParams.fShortPreamble = (val) ? 1 : 0;

	/* In STA case this parameter is filled during the join request */
	if (LIM_IS_AP_ROLE(psessionEntry) ||
	    LIM_IS_IBSS_ROLE(psessionEntry)) {
		if (wlan_cfg_get_int(pMac, WNI_CFG_WME_ENABLED, &val) !=
		    QDF_STATUS_SUCCESS)
			pe_err("cfg get wme enabled failed");
		psessionEntry->limWmeEnabled = (val) ? 1 : 0;
	}

	if (wlan_cfg_get_int(pMac, WNI_CFG_WSM_ENABLED, &val) != QDF_STATUS_SUCCESS)
		pe_err("cfg get wsm enabled failed");
	psessionEntry->limWsmEnabled = (val) ? 1 : 0;

	if ((!psessionEntry->limWmeEnabled) && (psessionEntry->limWsmEnabled)) {
		pe_err("Can't enable WSM without WME");
		psessionEntry->limWsmEnabled = 0;
	}
	/* In STA , this parameter is filled during the join request */
	if (LIM_IS_AP_ROLE(psessionEntry) || LIM_IS_IBSS_ROLE(psessionEntry)) {
		if (wlan_cfg_get_int(pMac, WNI_CFG_QOS_ENABLED, &val) !=
		    QDF_STATUS_SUCCESS)
			pe_err("cfg get qos enabled failed");
		psessionEntry->limQosEnabled = (val) ? 1 : 0;
	}
	if (wlan_cfg_get_int(pMac, WNI_CFG_HCF_ENABLED, &val) != QDF_STATUS_SUCCESS)
		pe_err("cfg get hcf enabled failed");
	psessionEntry->limHcfEnabled = (val) ? 1 : 0;

	/* AP: WSM should enable HCF as well, for STA enable WSM only after */
	/* association response is received */
	if (psessionEntry->limWsmEnabled && LIM_IS_AP_ROLE(psessionEntry))
		psessionEntry->limHcfEnabled = 1;

	if (wlan_cfg_get_int(pMac, WNI_CFG_11D_ENABLED, &val) != QDF_STATUS_SUCCESS)
		pe_err("cfg get 11d enabled failed");
	psessionEntry->lim11dEnabled = (val) ? 1 : 0;

	if (wlan_cfg_get_int(pMac, WNI_CFG_ASSOC_STA_LIMIT, &val) != QDF_STATUS_SUCCESS) {
		pe_err("cfg get assoc sta limit failed");
	}
	pMac->lim.gLimAssocStaLimit = (uint16_t) val;

	pe_debug("Updated Lim shadow state based on CFG");
}
