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

/**
 *
 * @file  wlan_hdd_p2p.c
 *
 * @brief WLAN Host Device Driver implementation for P2P commands interface
 *
 */

#include <wlan_hdd_includes.h>
#include <wlan_hdd_hostapd.h>
#include <net/cfg80211.h>
#include "sme_api.h"
#include "sme_qos_api.h"
#include "wlan_hdd_p2p.h"
#include "sap_api.h"
#include "wlan_hdd_main.h"
#include "qdf_trace.h"
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <net/ieee80211_radiotap.h>
#include "wlan_hdd_tdls.h"
#include "wlan_hdd_trace.h"
#include "qdf_types.h"
#include "qdf_trace.h"
#include "cds_sched.h"
#include "cds_concurrency.h"
#include "cds_utils.h"
#include "wlan_hdd_request_manager.h"

/* Ms to Time Unit Micro Sec */
#define MS_TO_TU_MUS(x)   ((x) * 1024)
#define MAX_MUS_VAL       (INT_MAX / 1024)

static uint8_t *hdd_get_action_string(uint16_t MsgType)
{
	switch (MsgType) {
		CASE_RETURN_STRING(SIR_MAC_ACTION_SPECTRUM_MGMT);
		CASE_RETURN_STRING(SIR_MAC_ACTION_QOS_MGMT);
		CASE_RETURN_STRING(SIR_MAC_ACTION_DLP);
		CASE_RETURN_STRING(SIR_MAC_ACTION_PUBLIC_USAGE);
		CASE_RETURN_STRING(SIR_MAC_ACTION_RRM);
		CASE_RETURN_STRING(SIR_MAC_ACTION_FAST_BSS_TRNST);
		CASE_RETURN_STRING(SIR_MAC_ACTION_HT);
		CASE_RETURN_STRING(SIR_MAC_ACTION_SA_QUERY);
		CASE_RETURN_STRING(SIR_MAC_ACTION_PROT_DUAL_PUB);
		CASE_RETURN_STRING(SIR_MAC_ACTION_WNM);
		CASE_RETURN_STRING(SIR_MAC_ACTION_UNPROT_WNM);
		CASE_RETURN_STRING(SIR_MAC_ACTION_TDLS);
		CASE_RETURN_STRING(SIR_MAC_ACITON_MESH);
		CASE_RETURN_STRING(SIR_MAC_ACTION_MHF);
		CASE_RETURN_STRING(SIR_MAC_SELF_PROTECTED);
		CASE_RETURN_STRING(SIR_MAC_ACTION_WME);
		CASE_RETURN_STRING(SIR_MAC_ACTION_VHT);
	default:
		return "UNKNOWN";
	}
}

#ifdef WLAN_FEATURE_P2P_DEBUG
#define MAX_P2P_ACTION_FRAME_TYPE 9
const char *p2p_action_frame_type[] = { "GO Negotiation Request",
					"GO Negotiation Response",
					"GO Negotiation Confirmation",
					"P2P Invitation Request",
					"P2P Invitation Response",
					"Device Discoverability Request",
					"Device Discoverability Response",
					"Provision Discovery Request",
					"Provision Discovery Response"};

/* We no need to protect this variable since
 * there is no chance of race to condition
 * and also not make any complicating the code
 * just for debugging log
 */
enum p2p_connection_status global_p2p_connection_status = P2P_NOT_ACTIVE;

#endif
#define MAX_TDLS_ACTION_FRAME_TYPE 11
const char *tdls_action_frame_type[] = { "TDLS Setup Request",
					 "TDLS Setup Response",
					 "TDLS Setup Confirm",
					 "TDLS Teardown",
					 "TDLS Peer Traffic Indication",
					 "TDLS Channel Switch Request",
					 "TDLS Channel Switch Response",
					 "TDLS Peer PSM Request",
					 "TDLS Peer PSM Response",
					 "TDLS Peer Traffic Response",
					 "TDLS Discovery Request"};

static bool wlan_hdd_is_type_p2p_action(const u8 *buf, uint32_t len)
{
	const u8 *ouiPtr;

	if (len < WLAN_HDD_PUBLIC_ACTION_FRAME_SUB_TYPE_OFFSET + 1)
		return false;

	if (buf[WLAN_HDD_PUBLIC_ACTION_FRAME_CATEGORY_OFFSET] !=
	    WLAN_HDD_PUBLIC_ACTION_FRAME)
		return false;

	if (buf[WLAN_HDD_PUBLIC_ACTION_FRAME_ACTION_OFFSET] !=
	    WLAN_HDD_VENDOR_SPECIFIC_ACTION)
		return false;

	ouiPtr = &buf[WLAN_HDD_PUBLIC_ACTION_FRAME_OUI_OFFSET];

	if (WPA_GET_BE24(ouiPtr) != WLAN_HDD_WFA_OUI)
		return false;

	if (buf[WLAN_HDD_PUBLIC_ACTION_FRAME_OUI_TYPE_OFFSET] !=
	    WLAN_HDD_WFA_P2P_OUI_TYPE)
		return false;

	return true;
}

static bool hdd_p2p_is_action_type_rsp(const u8 *buf, uint32_t len)
{
	enum action_frm_type actionFrmType;

	if (wlan_hdd_is_type_p2p_action(buf, len)) {
		actionFrmType =
			buf[WLAN_HDD_PUBLIC_ACTION_FRAME_SUB_TYPE_OFFSET];
		if (actionFrmType != WLAN_HDD_INVITATION_REQ
		    && actionFrmType != WLAN_HDD_GO_NEG_REQ
		    && actionFrmType != WLAN_HDD_DEV_DIS_REQ
		    && actionFrmType != WLAN_HDD_PROV_DIS_REQ)
			return true;
	}

	return false;
}

/**
 * hdd_is_p2p_go_cnf_frame() - function to if the frame type is go neg cnf
 * @buf: pointer to frame
 * @len: frame length
 *
 * This function is used to check if the given frame is GO negotiation
 * confirmation frame.
 *
 * Return: true if the frame is go negotiation confirmation otherwise false
 */
static bool hdd_is_p2p_go_cnf_frame(const u8 *buf, uint32_t len)
{
	if (wlan_hdd_is_type_p2p_action(buf, len) &&
			buf[WLAN_HDD_PUBLIC_ACTION_FRAME_SUB_TYPE_OFFSET] ==
			WLAN_HDD_GO_NEG_CNF)
		return true;
	else
		return false;
}

struct random_mac_priv {
	bool set_random_addr;
};

/**
 * hdd_random_mac_callback() - Callback invoked from wmi layer
 * @set_random_addr: Status of random mac filter set operation
 * @context: Context passed while registring callback
 *
 * This function is invoked from wmi layer to give the status of
 * random mac filter set operation by firmware.
 *
 * Return: None
 */
static void hdd_random_mac_callback(bool set_random_addr, void *context)
{
	struct hdd_request *request;
	struct random_mac_priv *priv;

	request = hdd_request_get(context);
	if (!request) {
		hdd_err("invalid request");
		return;
	}

	priv = hdd_request_priv(request);
	priv->set_random_addr = set_random_addr;

	hdd_request_complete(request);
	hdd_request_put(request);
}

/**
 * hdd_set_random_mac() - Invoke sme api to set random mac filter
 * @adapter: Pointer to adapter
 * @random_mac_addr: Mac addr filter to be set
 * @freq: Channel frequency
 *
 * Return: If set is successful return true else return false
 */
static bool hdd_set_random_mac(hdd_adapter_t *adapter,
			       uint8_t *random_mac_addr,
			       uint32_t freq)
{
	hdd_context_t *hdd_ctx;
	QDF_STATUS sme_status;
	unsigned long rc;
	void *cookie;
	bool status = false;
	struct hdd_request     *request;
	struct random_mac_priv *priv;
	static const struct hdd_request_params params = {
		.priv_size = sizeof(*priv),
		.timeout_ms = WLAN_WAIT_TIME_SET_RND,
	};

	ENTER();
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (wlan_hdd_validate_context(hdd_ctx)) {
		hdd_err("Invalid hdd ctx");
		return false;
	}

	request = hdd_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return false;
	}

	cookie = hdd_request_cookie(request);

	sme_status = sme_set_random_mac(hdd_ctx->hHal, hdd_random_mac_callback,
				     adapter->sessionId, random_mac_addr, freq,
				     cookie);

	if (sme_status != QDF_STATUS_SUCCESS) {
		hdd_err("Unable to set random mac");
	} else {
		rc = hdd_request_wait_for_response(request);
		if (rc) {
			hdd_err("SME timed out while setting random mac");
		} else {
			priv = hdd_request_priv(request);
			status = priv->set_random_addr;
		}
	}

	hdd_request_put(request);
	EXIT();

	return status;
}

/**
 * hdd_clear_random_mac() - Invoke sme api to clear random mac filter
 * @adapter: Pointer to adapter
 * @random_mac_addr: Mac addr filter to be cleared
 * @freq: Channel frequency
 *
 * Return: If clear is successful return true else return false
 */
static bool hdd_clear_random_mac(hdd_adapter_t *adapter,
				 uint8_t *random_mac_addr,
				 uint32_t freq)
{
	hdd_context_t *hdd_ctx;
	QDF_STATUS status;

	ENTER();
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (wlan_hdd_validate_context(hdd_ctx)) {
		hdd_err("Invalid hdd ctx");
		return false;
	}

	status = sme_clear_random_mac(hdd_ctx->hHal, adapter->sessionId,
				      random_mac_addr, freq);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Unable to clear random mac");
		return false;
	}

	EXIT();
	return true;
}

bool hdd_check_random_mac(hdd_adapter_t *adapter, uint8_t *random_mac_addr)
{
	uint32_t i = 0;

	spin_lock(&adapter->random_mac_lock);
	for (i = 0; i < MAX_RANDOM_MAC_ADDRS; i++) {
		if ((adapter->random_mac[i].in_use) &&
		    (!qdf_mem_cmp(adapter->random_mac[i].addr, random_mac_addr,
			     QDF_MAC_ADDR_SIZE))) {
			spin_unlock(&adapter->random_mac_lock);
			return true;
		}
	}
	spin_unlock(&adapter->random_mac_lock);
	return false;
}

/**
 * find_action_frame_cookie() - Checks for action cookie in cookie list
 * @cookie_list: List of cookies
 * @cookie: Cookie to be searched
 *
 * Return: If search is successful return pointer to action_frame_cookie
 * object in which cookie item is encapsulated.
 */
static struct action_frame_cookie *find_action_frame_cookie(
						struct list_head *cookie_list,
						uint64_t cookie)
{
	struct action_frame_cookie *action_cookie = NULL;
	struct list_head *temp = NULL;

	list_for_each(temp, cookie_list) {
		action_cookie = list_entry(temp, struct action_frame_cookie,
					   cookie_node);
		if (action_cookie->cookie == cookie)
			return action_cookie;
	}

	return NULL;
}

/**
 * allocate_action_frame_cookie() - Allocate and add action cookie to
 * given list
 * @cookie_list: List of cookies
 * @cookie: Cookie to be added
 *
 * Return: If allocation and addition is successful return pointer to
 * action_frame_cookie object in which cookie item is encapsulated.
 */
static struct action_frame_cookie *allocate_action_frame_cookie(
						struct list_head *cookie_list,
						uint64_t cookie)
{
	struct action_frame_cookie *action_cookie = NULL;

	action_cookie = qdf_mem_malloc(sizeof(*action_cookie));
	if (!action_cookie)
		return NULL;

	action_cookie->cookie = cookie;
	list_add(&action_cookie->cookie_node, cookie_list);

	return action_cookie;
}

/**
 * delete_action_frame_cookie() - Delete the cookie from given list
 * @cookie_list: List of cookies
 * @cookie: Cookie to be deleted
 *
 * This function deletes the cookie item from given list and corresponding
 * object in which it is encapsulated.
 *
 * Return: None
 */
static void delete_action_frame_cookie(
				struct action_frame_cookie *action_cookie)
{
	list_del(&action_cookie->cookie_node);
	qdf_mem_free(action_cookie);
}

/**
 * append_action_frame_cookie() - Append action cookie to given list
 * @cookie_list: List of cookies
 * @cookie: Cookie to be append
 *
 * This is a wrapper function which invokes allocate_action_frame_cookie
 * if the cookie to be added is not duplicate
 *
 * Return: 0 - for successful case
 *         -EALREADY - if cookie is duplicate
 *         -ENOMEM - if allocation is failed
 */
static int32_t append_action_frame_cookie(struct list_head *cookie_list,
					  uint64_t cookie)
{
	struct action_frame_cookie *action_cookie = NULL;

	/*
	 * There should be no mac entry with empty cookie list,
	 * check and ignore if duplicate
	 */
	action_cookie = find_action_frame_cookie(cookie_list, cookie);
	if (action_cookie)
		/* random mac address is already programmed */
		return -EALREADY;

	/* insert new cookie in cookie list */
	action_cookie = allocate_action_frame_cookie(cookie_list, cookie);
	if (!action_cookie)
		return -ENOMEM;

	return 0;
}

/**
 * hdd_set_action_frame_random_mac() - Store action frame cookie
 * @adapter: Pointer to adapter
 * @random_mac_addr: Mac addr for cookie
 * @cookie: Cookie to be stored
 * @freq: Channel frequency
 *
 * This function is used to create cookie list and append the cookies
 * to same for corresponding random mac addr. If this cookie is the first
 * item in the list then random mac filter is set.
 *
 * Return: 0 - for success else negative value
 */
static int32_t hdd_set_action_frame_random_mac(hdd_adapter_t *adapter,
					       uint8_t *random_mac_addr,
					       uint64_t cookie, uint32_t freq)
{
	uint32_t i = 0;
	uint32_t first_unused = MAX_RANDOM_MAC_ADDRS;
	struct action_frame_cookie *action_cookie = NULL;
	int32_t append_ret = 0;

	if (!cookie) {
		hdd_err("Invalid cookie");
		return -EINVAL;
	}

	hdd_info("mac_addr: " MAC_ADDRESS_STR " && cookie = %llu && freq = %u",
			MAC_ADDR_ARRAY(random_mac_addr), cookie, freq);

	spin_lock(&adapter->random_mac_lock);
	/*
	 * Following loop checks whether random mac entry is already
	 * present, if present get the index of matched entry else
	 * get the first unused slot to store this new random mac
	 */
	for (i = 0; i < MAX_RANDOM_MAC_ADDRS; i++) {
		if (!adapter->random_mac[i].in_use) {
			if (first_unused == MAX_RANDOM_MAC_ADDRS)
				first_unused = i;
			continue;
		}

		if (!qdf_mem_cmp(adapter->random_mac[i].addr, random_mac_addr,
				 QDF_MAC_ADDR_SIZE))
			break;
	}

	if (i != MAX_RANDOM_MAC_ADDRS) {
		append_ret = append_action_frame_cookie(
					&adapter->random_mac[i].cookie_list,
					cookie);
		spin_unlock(&adapter->random_mac_lock);

		if (append_ret == -ENOMEM) {
			hdd_err("No Sufficient memory for cookie");
			return append_ret;
		}

		return 0;
	}

	if (first_unused == MAX_RANDOM_MAC_ADDRS) {
		spin_unlock(&adapter->random_mac_lock);
		hdd_err("Reached the limit of Max random addresses");
		return -EBUSY;
	}

	/* get the first unused buf and store new random mac */
	i = first_unused;

	INIT_LIST_HEAD(&adapter->random_mac[i].cookie_list);
	action_cookie = allocate_action_frame_cookie(
					&adapter->random_mac[i].cookie_list,
					cookie);
	if (!action_cookie) {
		spin_unlock(&adapter->random_mac_lock);
		hdd_err("No Sufficient memory for cookie");
		return -ENOMEM;
	}
	qdf_mem_copy(adapter->random_mac[i].addr, random_mac_addr,
		     QDF_MAC_ADDR_SIZE);
	adapter->random_mac[i].in_use = true;
	adapter->random_mac[i].freq = freq;
	spin_unlock(&adapter->random_mac_lock);
	/* Program random mac_addr */
	if (!hdd_set_random_mac(adapter, adapter->random_mac[i].addr, freq)) {
		spin_lock(&adapter->random_mac_lock);
		/* clear the cookie */
		delete_action_frame_cookie(action_cookie);
		adapter->random_mac[i].in_use = false;
		spin_unlock(&adapter->random_mac_lock);
		hdd_err("random mac filter set failed for: "MAC_ADDRESS_STR,
				MAC_ADDR_ARRAY(adapter->random_mac[i].addr));
		return -EFAULT;
	}

	return 0;
}

/**
 * hdd_reset_action_frame_random_mac() - Delete action frame cookie with
 * given random mac addr
 * @adapter: Pointer to adapter
 * @random_mac_addr: Mac addr for cookie
 * @cookie: Cookie to be deleted
 *
 * This function is used to delete the cookie from the cookie list
 * corresponding to given random mac addr.If cookie list is empty after
 * deleting, it will clear random mac filter.
 *
 * Return: 0 - for success else negative value
 */
static int32_t hdd_reset_action_frame_random_mac(hdd_adapter_t *adapter,
						 uint8_t *random_mac_addr,
						 uint64_t cookie)
{
	uint32_t i = 0;
	uint32_t freq = 0;
	struct action_frame_cookie *action_cookie = NULL;

	if (!cookie) {
		hdd_err("Invalid cookie");
		return -EINVAL;
	}

	hdd_info("mac_addr: " MAC_ADDRESS_STR " && cookie = %llu",
			MAC_ADDR_ARRAY(random_mac_addr), cookie);

	spin_lock(&adapter->random_mac_lock);
	for (i = 0; i < MAX_RANDOM_MAC_ADDRS; i++) {
		if ((adapter->random_mac[i].in_use) &&
		    (!qdf_mem_cmp(adapter->random_mac[i].addr,
			     random_mac_addr, QDF_MAC_ADDR_SIZE)))
			break;
	}

	if (i == MAX_RANDOM_MAC_ADDRS) {
		/*
		 * trying to delete cookie of random mac-addr
		 * for which entry is not present
		 */
		spin_unlock(&adapter->random_mac_lock);
		return -EINVAL;
	}

	action_cookie = find_action_frame_cookie(
					&adapter->random_mac[i].cookie_list,
					cookie);

	if (!action_cookie) {
		spin_unlock(&adapter->random_mac_lock);
		hdd_info("No cookie matches");
		return 0;
	}

	delete_action_frame_cookie(action_cookie);
	if (list_empty(&adapter->random_mac[i].cookie_list)) {
		adapter->random_mac[i].in_use = false;
		freq = adapter->random_mac[i].freq;
		spin_unlock(&adapter->random_mac_lock);
		hdd_clear_random_mac(adapter, random_mac_addr,
				     freq);
		hdd_info("Deleted random mac_addr: "MAC_ADDRESS_STR,
			 MAC_ADDR_ARRAY(random_mac_addr));
		return 0;
	}

	spin_unlock(&adapter->random_mac_lock);
	return 0;
}

/**
 * hdd_delete_action_frame_cookie() - Delete action frame cookie
 * @adapter: Pointer to adapter
 * @cookie: Cookie to be deleted
 *
 * This function parses the cookie list of each random mac addr until the
 * specified cookie is found and then deletes it. If cookie list is empty
 * after deleting, it will clear random mac filter.
 *
 * Return: 0 - for success else negative value
 */
static int32_t hdd_delete_action_frame_cookie(hdd_adapter_t *adapter,
					      uint64_t cookie)
{
	uint32_t i = 0;
	struct action_frame_cookie *action_cookie = NULL;
	uint32_t freq = 0;

	hdd_debug("Delete cookie = %llu", cookie);

	spin_lock(&adapter->random_mac_lock);
	for (i = 0; i < MAX_RANDOM_MAC_ADDRS; i++) {
		if (!adapter->random_mac[i].in_use)
			continue;

		action_cookie = find_action_frame_cookie(
					&adapter->random_mac[i].cookie_list,
					cookie);

		if (!action_cookie)
			continue;

		delete_action_frame_cookie(action_cookie);

		if (list_empty(&adapter->random_mac[i].cookie_list)) {
			adapter->random_mac[i].in_use = false;
			freq = adapter->random_mac[i].freq;
			spin_unlock(&adapter->random_mac_lock);
			hdd_clear_random_mac(adapter,
					     adapter->random_mac[i].addr,
					     freq);
			hdd_info("Deleted random addr "MAC_ADDRESS_STR,
				 MAC_ADDR_ARRAY(adapter->random_mac[i].addr));
			return 0;
		}
		spin_unlock(&adapter->random_mac_lock);
		return 0;
	}

	spin_unlock(&adapter->random_mac_lock);
	return 0;
}

/**
 * hdd_delete_all_action_frame_cookies() - Delete all action frame cookies
 * @adapter: Pointer to adapter
 *
 * This function deletes all the cookie lists of each random mac addr and
 * clears the corresponding random mac filters.
 *
 * Return: 0 - for success else negative value
 */
static void hdd_delete_all_action_frame_cookies(hdd_adapter_t *adapter)
{
	uint32_t i = 0;
	struct action_frame_cookie *action_cookie = NULL;
	struct list_head *n;
	struct list_head *temp;
	uint32_t freq = 0;

	spin_lock(&adapter->random_mac_lock);

	for (i = 0; i < MAX_RANDOM_MAC_ADDRS; i++) {
		if (!adapter->random_mac[i].in_use)
			continue;

		/* empty the list and clear random addr */
		list_for_each_safe(temp, n,
				   &adapter->random_mac[i].cookie_list) {
			action_cookie = list_entry(temp,
						   struct action_frame_cookie,
						   cookie_node);
			list_del(temp);
			qdf_mem_free(action_cookie);
		}

		adapter->random_mac[i].in_use = false;
		freq = adapter->random_mac[i].freq;
		spin_unlock(&adapter->random_mac_lock);
		hdd_clear_random_mac(adapter, adapter->random_mac[i].addr,
				     freq);
		hdd_info("Deleted random addr " MAC_ADDRESS_STR,
			 MAC_ADDR_ARRAY(adapter->random_mac[i].addr));
		spin_lock(&adapter->random_mac_lock);
	}

	spin_unlock(&adapter->random_mac_lock);
}
static
QDF_STATUS wlan_hdd_remain_on_channel_callback(tHalHandle hHal, void *pCtx,
			QDF_STATUS status, uint32_t scan_id)
{
	hdd_adapter_t *pAdapter = (hdd_adapter_t *) pCtx;
	hdd_cfg80211_state_t *cfgState = WLAN_HDD_GET_CFG_STATE_PTR(pAdapter);
	hdd_remain_on_chan_ctx_t *pRemainChanCtx;
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(pAdapter);
	enum rem_on_channel_request_type req_type;

	if (!hdd_ctx) {
		hdd_err("Invalid HDD context");
		return QDF_STATUS_E_FAILURE;
	}

	mutex_lock(&cfgState->remain_on_chan_ctx_lock);
	pRemainChanCtx = cfgState->remain_on_chan_ctx;

	if (pRemainChanCtx == NULL) {
		mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
		hdd_warn("No Rem on channel pending for which Rsp is received");
		return QDF_STATUS_SUCCESS;
	}
	if (pRemainChanCtx->scan_id != scan_id) {
		mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
		hdd_warn("RoC scan id is not matching");
		return QDF_STATUS_SUCCESS;
	}
	hdd_debug("Received remain on channel rsp");
	if (qdf_mc_timer_stop(&pRemainChanCtx->hdd_remain_on_chan_timer)
			!= QDF_STATUS_SUCCESS)
		hdd_warn("Failed to stop hdd_remain_on_chan_timer");
	if (qdf_mc_timer_destroy(&pRemainChanCtx->hdd_remain_on_chan_timer)
			!= QDF_STATUS_SUCCESS)
		hdd_warn("Failed to destroy hdd_remain_on_chan_timer");
	cfgState->remain_on_chan_ctx = NULL;
	/*
	 * Resetting the roc in progress early ensures that the subsequent
	 * roc requests are immediately processed without being queued
	 */
	pAdapter->is_roc_inprogress = false;
	qdf_runtime_pm_allow_suspend(&hdd_ctx->runtime_context.roc);
	/*
	 * If the allow suspend is done later, the scheduled roc wil prevent
	 * the system from going into suspend and immediately this logic
	 * will allow the system to go to suspend breaking the exising logic.
	 * Basically, the system must not go into suspend while roc is in
	 * progress.
	 */
	hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_ROC);

	if (REMAIN_ON_CHANNEL_REQUEST == pRemainChanCtx->rem_on_chan_request) {
		if (cfgState->buf)
			hdd_debug("Yet to rcv an ack for one of the tx pkt");

		cfg80211_remain_on_channel_expired(
			pRemainChanCtx->dev->
			ieee80211_ptr,
			(u64)pRemainChanCtx->id,
			&pRemainChanCtx->chan,
			GFP_KERNEL);
		pAdapter->last_roc_ts =
			(uint64_t)qdf_mc_timer_get_system_time();
	}
	req_type = pRemainChanCtx->rem_on_chan_request;
	mutex_unlock(&cfgState->remain_on_chan_ctx_lock);

	if ((QDF_STA_MODE == pAdapter->device_mode) ||
	    (QDF_P2P_CLIENT_MODE == pAdapter->device_mode) ||
	    (QDF_P2P_DEVICE_MODE == pAdapter->device_mode)
	    ) {
		uint8_t sessionId = pAdapter->sessionId;

		if (REMAIN_ON_CHANNEL_REQUEST == req_type) {
			sme_deregister_mgmt_frame(hHal, sessionId,
						  (SIR_MAC_MGMT_FRAME << 2) |
						  (SIR_MAC_MGMT_PROBE_REQ << 4),
						  NULL, 0);
		}
		hdd_delete_all_action_frame_cookies(pAdapter);
	} else if ((QDF_SAP_MODE == pAdapter->device_mode) ||
		   (QDF_P2P_GO_MODE == pAdapter->device_mode)
		   ) {
		wlansap_de_register_mgmt_frame(
			WLAN_HDD_GET_SAP_CTX_PTR(pAdapter),
			(SIR_MAC_MGMT_FRAME << 2) |
			(SIR_MAC_MGMT_PROBE_REQ << 4),
			 NULL, 0);

	}

	mutex_lock(&cfgState->remain_on_chan_ctx_lock);
	if (pRemainChanCtx) {
		if (pRemainChanCtx->action_pkt_buff.frame_ptr != NULL
		    && pRemainChanCtx->action_pkt_buff.frame_length != 0) {
			qdf_mem_free(pRemainChanCtx->action_pkt_buff.frame_ptr);
			pRemainChanCtx->action_pkt_buff.frame_ptr = NULL;
			pRemainChanCtx->action_pkt_buff.frame_length = 0;
		}
		qdf_idr_remove(&hdd_ctx->p2p_idr, pRemainChanCtx->id);
	}
	qdf_mem_free(pRemainChanCtx);
	mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
	complete(&pAdapter->cancel_rem_on_chan_var);
	if (QDF_STATUS_SUCCESS != status)
		complete(&pAdapter->rem_on_chan_ready_event);

	/* If we schedule work queue to start new RoC before completing
	 * cancel_rem_on_chan_var then the work queue may immediately get
	 * scheduled and update cfgState->remain_on_chan_ctx which is referred
	 * in mgmt_tx. Due to this update the the mgmt_tx may extend the roc
	 * which was already completed. This will lead to mgmt tx failure.
	 * Always schedule below work queue only after completing the
	 * cancel_rem_on_chan_var event.
	 */
	schedule_delayed_work(&hdd_ctx->roc_req_work, 0);

	return QDF_STATUS_SUCCESS;
}

void wlan_hdd_cancel_existing_remain_on_channel(hdd_adapter_t *pAdapter)
{
	hdd_cfg80211_state_t *cfgState = WLAN_HDD_GET_CFG_STATE_PTR(pAdapter);
	hdd_remain_on_chan_ctx_t *pRemainChanCtx;
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(pAdapter);
	unsigned long rc;
	uint32_t roc_scan_id;

	mutex_lock(&cfgState->remain_on_chan_ctx_lock);
	if (cfgState->remain_on_chan_ctx != NULL) {
		hdd_debug("Cancel Existing Remain on Channel");

		if (QDF_TIMER_STATE_RUNNING == qdf_mc_timer_get_current_state(
		    &cfgState->remain_on_chan_ctx->hdd_remain_on_chan_timer)) {
			if (qdf_mc_timer_stop(&cfgState->remain_on_chan_ctx->
				hdd_remain_on_chan_timer) != QDF_STATUS_SUCCESS)
				hdd_warn("Failed to stop hdd_remain_on_chan_timer");
		}

		pRemainChanCtx = cfgState->remain_on_chan_ctx;
		if (pRemainChanCtx->hdd_remain_on_chan_cancel_in_progress ==
			true) {
			mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
			hdd_warn("ROC timer cancellation in progress wait for completion");
			rc = wait_for_completion_timeout(&pAdapter->
							 cancel_rem_on_chan_var,
							 msecs_to_jiffies
								 (WAIT_CANCEL_REM_CHAN));
			if (!rc)
				hdd_err("wait on cancel_rem_on_chan_var timed out");

			return;
		}
		pRemainChanCtx->hdd_remain_on_chan_cancel_in_progress = true;
		roc_scan_id = pRemainChanCtx->scan_id;
		mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
		/* Wait till remain on channel ready indication before
		 * issuing cancel remain on channel request, otherwise
		 * if remain on channel not received and if the driver issues
		 * cancel remain on channel then lim will be in unknown state.
		 */
		rc = wait_for_completion_timeout(&pAdapter->
						 rem_on_chan_ready_event,
						 msecs_to_jiffies
							 (WAIT_REM_CHAN_READY));
		if (!rc) {
			hdd_err("timeout waiting for remain on channel ready indication");
			cds_flush_logs(WLAN_LOG_TYPE_FATAL,
				WLAN_LOG_INDICATOR_HOST_DRIVER,
				WLAN_LOG_REASON_HDD_TIME_OUT,
				true, false);
		}

		INIT_COMPLETION(pAdapter->cancel_rem_on_chan_var);

		/* Issue abort remain on chan request to sme.
		 * The remain on channel callback will make sure the
		 * remain_on_chan expired event is sent.
		 */
		if ((QDF_STA_MODE == pAdapter->device_mode) ||
		    (QDF_P2P_CLIENT_MODE == pAdapter->device_mode) ||
		    (QDF_P2P_DEVICE_MODE == pAdapter->device_mode)
		    ) {
			hdd_delete_all_action_frame_cookies(pAdapter);
			sme_cancel_remain_on_channel(WLAN_HDD_GET_HAL_CTX
							     (pAdapter),
				pAdapter->sessionId, roc_scan_id);
		} else if ((QDF_SAP_MODE == pAdapter->device_mode)
			   || (QDF_P2P_GO_MODE == pAdapter->device_mode)
			   ) {
			wlansap_cancel_remain_on_channel(
				WLAN_HDD_GET_SAP_CTX_PTR(pAdapter), roc_scan_id);
		}

		rc = wait_for_completion_timeout(&pAdapter->
						 cancel_rem_on_chan_var,
						 msecs_to_jiffies
							 (WAIT_CANCEL_REM_CHAN));

		if (!rc)
			hdd_err("timeout for cancel ROC ready indication");

		qdf_runtime_pm_allow_suspend(&hdd_ctx->runtime_context.roc);
		hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_ROC);
	} else
		mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
}

int wlan_hdd_check_remain_on_channel(hdd_adapter_t *pAdapter)
{
	int status = 0;
	hdd_cfg80211_state_t *cfgState = WLAN_HDD_GET_CFG_STATE_PTR(pAdapter);

	if (QDF_P2P_GO_MODE != pAdapter->device_mode) {
		/* Cancel Existing Remain On Channel */
		/* If no action frame is pending */
		if (cfgState->remain_on_chan_ctx != NULL) {
			/* Check whether Action Frame is pending or not */
			if (cfgState->buf == NULL) {
				wlan_hdd_cancel_existing_remain_on_channel
					(pAdapter);
			} else {
				hdd_debug("Cannot Cancel Existing Remain on Channel");
				status = -EBUSY;
			}
		}
	}
	return status;
}

/**
 * wlan_hdd_cancel_pending_roc() - Cancel pending roc
 * @adapter: HDD adapter
 *
 * Cancels any pending remain on channel request
 *
 * Return: None
 */
static void wlan_hdd_cancel_pending_roc(hdd_adapter_t *adapter)
{
	hdd_remain_on_chan_ctx_t *roc_ctx;
	unsigned long rc;
	hdd_cfg80211_state_t *cfg_state = WLAN_HDD_GET_CFG_STATE_PTR(adapter);
	uint32_t roc_scan_id;

	hdd_debug("ROC completion is not received !!!");

	mutex_lock(&cfg_state->remain_on_chan_ctx_lock);
	roc_ctx = cfg_state->remain_on_chan_ctx;

	if (!roc_ctx) {
		mutex_unlock(&cfg_state->remain_on_chan_ctx_lock);
		hdd_debug("roc_ctx is NULL, No pending RoC");
		return;
	}

	if (roc_ctx->hdd_remain_on_chan_cancel_in_progress) {
		mutex_unlock(&cfg_state->remain_on_chan_ctx_lock);
		hdd_debug("roc cancel already in progress");
		/*
		 * Since a cancel roc is already issued and is
		 * in progress, we need not send another
		 * cancel roc again. Instead we can just wait
		 * for cancel roc completion
		 */
		goto wait;
	}
	roc_scan_id = roc_ctx->scan_id;
	mutex_unlock(&cfg_state->remain_on_chan_ctx_lock);

	if (adapter->device_mode == QDF_P2P_GO_MODE) {
		void *sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter);

		wlansap_cancel_remain_on_channel(sap_ctx, roc_scan_id);
	} else if (adapter->device_mode == QDF_P2P_CLIENT_MODE ||
		   adapter->device_mode == QDF_P2P_DEVICE_MODE) {
		hdd_delete_all_action_frame_cookies(adapter);
		sme_cancel_remain_on_channel(WLAN_HDD_GET_HAL_CTX
				(adapter),
				adapter->sessionId, roc_scan_id);
	}

wait:
	rc = wait_for_completion_timeout(&adapter->cancel_rem_on_chan_var,
			msecs_to_jiffies
			(WAIT_CANCEL_REM_CHAN));
	if (!rc) {
		hdd_err("Timeout occurred while waiting for RoC Cancellation");
		mutex_lock(&cfg_state->remain_on_chan_ctx_lock);
		roc_ctx = cfg_state->remain_on_chan_ctx;
		if (roc_ctx != NULL) {
			cfg_state->remain_on_chan_ctx = NULL;
			if (qdf_mc_timer_stop(&roc_ctx->
				hdd_remain_on_chan_timer)
				!= QDF_STATUS_SUCCESS)
				hdd_err("Failed to stop hdd_remain_on_chan_timer");
			if (qdf_mc_timer_destroy(
				&roc_ctx->hdd_remain_on_chan_timer)
				!= QDF_STATUS_SUCCESS)
				hdd_err("Failed to destroy hdd_remain_on_chan_timer");
			if (roc_ctx->action_pkt_buff.frame_ptr != NULL
				&& roc_ctx->action_pkt_buff.frame_length != 0) {
				qdf_mem_free(
					roc_ctx->action_pkt_buff.frame_ptr);
				roc_ctx->action_pkt_buff.frame_ptr = NULL;
				roc_ctx->action_pkt_buff.frame_length = 0;
			}
			qdf_mem_free(roc_ctx);
			adapter->is_roc_inprogress = false;
		}
		mutex_unlock(&cfg_state->remain_on_chan_ctx_lock);
	}
}

/* Clean up RoC context at hdd_stop_adapter*/
void wlan_hdd_cleanup_remain_on_channel_ctx(hdd_adapter_t *pAdapter)
{
	uint8_t retry = 0;
	hdd_cfg80211_state_t *cfgState = WLAN_HDD_GET_CFG_STATE_PTR(pAdapter);

	mutex_lock(&cfgState->remain_on_chan_ctx_lock);
	while (pAdapter->is_roc_inprogress) {
		mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
		hdd_debug("ROC in progress for session %d!!!",
			pAdapter->sessionId);
		msleep(500);
		if (retry++ > 3) {
			wlan_hdd_cancel_pending_roc(pAdapter);
			/* hold the lock before break from the loop */
			mutex_lock(&cfgState->remain_on_chan_ctx_lock);
			break;
		}
		mutex_lock(&cfgState->remain_on_chan_ctx_lock);
	} /* end of while */
	mutex_unlock(&cfgState->remain_on_chan_ctx_lock);

}

static void wlan_hdd_remain_on_chan_timeout(void *data)
{
	hdd_adapter_t *pAdapter = (hdd_adapter_t *) data;
	hdd_remain_on_chan_ctx_t *pRemainChanCtx;
	hdd_cfg80211_state_t *cfgState;
	hdd_context_t *hdd_ctx;
	uint32_t roc_scan_id;

	if ((NULL == pAdapter) ||
	    (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)) {
		hdd_err("pAdapter is invalid %pK !!!", pAdapter);
		return;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(pAdapter);
	cfgState = WLAN_HDD_GET_CFG_STATE_PTR(pAdapter);
	mutex_lock(&cfgState->remain_on_chan_ctx_lock);
	pRemainChanCtx = cfgState->remain_on_chan_ctx;

	if (NULL == pRemainChanCtx) {
		mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
		hdd_err("No Remain on channel is pending");
		return;
	}

	if (true == pRemainChanCtx->hdd_remain_on_chan_cancel_in_progress) {
		mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
		hdd_err("Cancellation already in progress");
		return;
	}

	pRemainChanCtx->hdd_remain_on_chan_cancel_in_progress = true;
	roc_scan_id = pRemainChanCtx->scan_id;
	mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
	hdd_debug("Cancel Remain on Channel on timeout");

	if ((QDF_STA_MODE == pAdapter->device_mode) ||
	    (QDF_P2P_CLIENT_MODE == pAdapter->device_mode) ||
	    (QDF_P2P_DEVICE_MODE == pAdapter->device_mode)) {
		hdd_delete_all_action_frame_cookies(pAdapter);
		sme_cancel_remain_on_channel(WLAN_HDD_GET_HAL_CTX(pAdapter),
			pAdapter->sessionId, roc_scan_id);
	} else if ((QDF_SAP_MODE == pAdapter->device_mode) ||
			(QDF_P2P_GO_MODE == pAdapter->device_mode)) {
		void *sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(pAdapter);

		wlansap_cancel_remain_on_channel(sap_ctx, roc_scan_id);
	}

	hdd_tdls_notify_p2p_roc(hdd_ctx, P2P_ROC_END);
	qdf_runtime_pm_allow_suspend(&hdd_ctx->runtime_context.roc);

	hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_ROC);
}

static int wlan_hdd_execute_remain_on_channel(hdd_adapter_t *pAdapter,
		      hdd_remain_on_chan_ctx_t *pRemainChanCtx)
{
	hdd_cfg80211_state_t *cfgState = WLAN_HDD_GET_CFG_STATE_PTR(pAdapter);
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	hdd_adapter_list_node_t *pAdapterNode = NULL, *pNext = NULL;
	hdd_adapter_t *pAdapter_temp;
	QDF_STATUS status;
	bool isGoPresent = false;
	unsigned int duration;


	mutex_lock(&cfgState->remain_on_chan_ctx_lock);
	if (pAdapter->is_roc_inprogress == true) {
		mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
		hdd_err("remain on channel request is in execution");
		return -EBUSY;
	}

	cfgState->remain_on_chan_ctx = pRemainChanCtx;
	cfgState->current_freq = pRemainChanCtx->chan.center_freq;
	pAdapter->is_roc_inprogress = true;
	pRemainChanCtx->is_recd_roc_ready = false;
	mutex_unlock(&cfgState->remain_on_chan_ctx_lock);

	/* Initialize Remain on chan timer */
	qdf_status =
		qdf_mc_timer_init(&pRemainChanCtx->hdd_remain_on_chan_timer,
				  QDF_TIMER_TYPE_SW,
				  wlan_hdd_remain_on_chan_timeout, pAdapter);
	if (qdf_status != QDF_STATUS_SUCCESS) {
		hdd_err("Not able to initialize remain_on_chan timer");
		mutex_lock(&cfgState->remain_on_chan_ctx_lock);
		cfgState->remain_on_chan_ctx = NULL;
		pAdapter->is_roc_inprogress = false;
		mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
		qdf_mem_free(pRemainChanCtx);
		return -EINVAL;
	}

	status = hdd_get_front_adapter(pHddCtx, &pAdapterNode);
	while (NULL != pAdapterNode && QDF_STATUS_SUCCESS == status) {
		pAdapter_temp = pAdapterNode->pAdapter;
		if (pAdapter_temp->device_mode == QDF_P2P_GO_MODE)
			isGoPresent = true;
		status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext);
		pAdapterNode = pNext;
	}

	/* Extending duration for proactive extension logic for RoC */
	duration = pRemainChanCtx->duration;
	if (duration < HDD_P2P_MAX_ROC_DURATION) {
		if (isGoPresent == true)
			duration *= P2P_ROC_DURATION_MULTIPLIER_GO_PRESENT;
		else
			duration *= P2P_ROC_DURATION_MULTIPLIER_GO_ABSENT;

		/* this is to protect too huge value if some customers
		 * give a higher value from supplicant
		 */
		if (duration > HDD_P2P_MAX_ROC_DURATION)
			duration = HDD_P2P_MAX_ROC_DURATION;
	}

	hdd_prevent_suspend(WIFI_POWER_EVENT_WAKELOCK_ROC);
	qdf_runtime_pm_prevent_suspend(&pHddCtx->runtime_context.roc);
	INIT_COMPLETION(pAdapter->rem_on_chan_ready_event);

	/* call sme API to start remain on channel. */
	if ((QDF_STA_MODE == pAdapter->device_mode) ||
	    (QDF_P2P_CLIENT_MODE == pAdapter->device_mode) ||
	    (QDF_P2P_DEVICE_MODE == pAdapter->device_mode)
	    ) {
		uint8_t sessionId = pAdapter->sessionId;
		/* call sme API to start remain on channel. */

		if (QDF_STATUS_SUCCESS != sme_remain_on_channel(
				WLAN_HDD_GET_HAL_CTX(pAdapter),
				sessionId,
				pRemainChanCtx->chan.hw_value, duration,
				wlan_hdd_remain_on_channel_callback,
				pAdapter,
				(pRemainChanCtx->rem_on_chan_request ==
				 REMAIN_ON_CHANNEL_REQUEST) ? true : false,
				 &pRemainChanCtx->scan_id)) {
			hdd_err("sme_remain_on_channel failed");
			mutex_lock(&cfgState->remain_on_chan_ctx_lock);
			pAdapter->is_roc_inprogress = false;
			pRemainChanCtx = cfgState->remain_on_chan_ctx;
			hdd_debug("Freeing ROC ctx cfgState->remain_on_chan_ctx=%pK",
				 cfgState->remain_on_chan_ctx);
			if (pRemainChanCtx) {
				if (qdf_mc_timer_destroy(
					&pRemainChanCtx->
						hdd_remain_on_chan_timer)
						!= QDF_STATUS_SUCCESS)
					hdd_err("Failed to destroy hdd_remain_on_chan_timer");
				qdf_mem_free(pRemainChanCtx);
				cfgState->remain_on_chan_ctx = NULL;
			}
			mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
			qdf_runtime_pm_allow_suspend(&pHddCtx->runtime_context.
						     roc);
			hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_ROC);
			return -EINVAL;
		}

		mutex_lock(&cfgState->remain_on_chan_ctx_lock);
		pRemainChanCtx = cfgState->remain_on_chan_ctx;
		if ((pRemainChanCtx) && (REMAIN_ON_CHANNEL_REQUEST ==
		    pRemainChanCtx->rem_on_chan_request)) {
			mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
			if (QDF_STATUS_SUCCESS != sme_register_mgmt_frame(
			    WLAN_HDD_GET_HAL_CTX(pAdapter), sessionId,
			    (SIR_MAC_MGMT_FRAME << 2) |
			    (SIR_MAC_MGMT_PROBE_REQ << 4), NULL, 0))
				hdd_err("sme_register_mgmt_frame failed");
		} else {
			mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
		}
	} else if ((QDF_SAP_MODE == pAdapter->device_mode) ||
		   (QDF_P2P_GO_MODE == pAdapter->device_mode)) {
		/* call sme API to start remain on channel. */
		if (QDF_STATUS_SUCCESS != wlansap_remain_on_channel(
			    WLAN_HDD_GET_SAP_CTX_PTR(pAdapter),
			    pRemainChanCtx->chan.hw_value,
			    duration, wlan_hdd_remain_on_channel_callback,
			    pAdapter, &pRemainChanCtx->scan_id)) {
			hdd_err("wlansap_remain_on_channel failed");
			mutex_lock(&cfgState->remain_on_chan_ctx_lock);
			pAdapter->is_roc_inprogress = false;
			pRemainChanCtx = cfgState->remain_on_chan_ctx;
			hdd_debug("Freeing ROC ctx cfgState->remain_on_chan_ctx=%pK",
				 cfgState->remain_on_chan_ctx);
			if (pRemainChanCtx) {
				if (qdf_mc_timer_destroy(
					&pRemainChanCtx->
					hdd_remain_on_chan_timer)
					!= QDF_STATUS_SUCCESS)
					hdd_err("Failed to destroy hdd_remain_on_chan_timer");
				qdf_mem_free(pRemainChanCtx);
				cfgState->remain_on_chan_ctx = NULL;
			}
			mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
			qdf_runtime_pm_allow_suspend(&pHddCtx->runtime_context.
						     roc);
			hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_ROC);
			return -EINVAL;
		}

		if (QDF_STATUS_SUCCESS != wlansap_register_mgmt_frame(
			WLAN_HDD_GET_SAP_CTX_PTR(pAdapter),
			(SIR_MAC_MGMT_FRAME << 2) |
			(SIR_MAC_MGMT_PROBE_REQ << 4), NULL, 0)) {
			hdd_err("wlansap_register_mgmt_frame return fail");
			wlansap_cancel_remain_on_channel(
				WLAN_HDD_GET_SAP_CTX_PTR(pAdapter),
				pRemainChanCtx->scan_id);
			qdf_runtime_pm_allow_suspend(&pHddCtx->runtime_context.
						     roc);
			hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_ROC);
			return -EINVAL;
		}

	}
	hdd_tdls_notify_p2p_roc(pHddCtx, P2P_ROC_START);
	return 0;
}

/**
 * wlan_hdd_roc_request_enqueue() - enqueue remain on channel request
 * @adapter: Pointer to the adapter
 * @remain_chan_ctx: Pointer to the remain on channel context
 *
 * Return: 0 on success, error number otherwise
 */
static int wlan_hdd_roc_request_enqueue(hdd_adapter_t *adapter,
			hdd_remain_on_chan_ctx_t *remain_chan_ctx)
{
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	hdd_roc_req_t *hdd_roc_req;
	QDF_STATUS status;

	/*
	 * "Driver is busy" OR "there is already RoC request inside the queue"
	 * so enqueue this RoC Request and execute sequentially later.
	 */

	hdd_roc_req = qdf_mem_malloc(sizeof(*hdd_roc_req));

	if (NULL == hdd_roc_req) {
		hdd_err("malloc failed for roc req context");
		return -ENOMEM;
	}

	hdd_roc_req->pAdapter = adapter;
	hdd_roc_req->pRemainChanCtx = remain_chan_ctx;

	/* Enqueue this RoC request */
	qdf_spin_lock(&hdd_ctx->hdd_roc_req_q_lock);
	status = qdf_list_insert_back(&hdd_ctx->hdd_roc_req_q,
					&hdd_roc_req->node);
	qdf_spin_unlock(&hdd_ctx->hdd_roc_req_q_lock);

	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Not able to enqueue RoC Req context");
		qdf_mem_free(hdd_roc_req);
		return -EINVAL;
	}

	return 0;
}

/**
 * wlan_hdd_indicate_roc_drop() - Indicate roc drop to userspace
 * @adapter: HDD adapter
 * @ctx: Remain on channel context
 *
 * Send remain on channel ready and cancel event for the queued
 * roc that is being dropped. This will ensure that the userspace
 * will send more roc requests. If this drop is not indicated to
 * userspace, subsequent roc will not be sent to the driver since
 * the userspace times out waiting for the remain on channel ready
 * event.
 *
 * Return: None
 */
static void wlan_hdd_indicate_roc_drop(hdd_adapter_t *adapter,
				       hdd_remain_on_chan_ctx_t *ctx)
{
	hdd_debug("indicate roc drop to userspace");
	cfg80211_ready_on_channel(
			adapter->dev->ieee80211_ptr,
			(u64)ctx->id,
			&ctx->chan,
			ctx->duration, GFP_KERNEL);

	cfg80211_remain_on_channel_expired(
			ctx->dev->ieee80211_ptr,
			(u64)ctx->id,
			&ctx->chan,
			GFP_KERNEL);
}

/**
 * wlan_hdd_roc_request_dequeue() - dequeue remain on channel request
 * @work: Pointer to work queue struct
 *
 * Return: none
 */
void wlan_hdd_roc_request_dequeue(struct work_struct *work)
{
	QDF_STATUS status;
	int ret = 0;
	hdd_roc_req_t *hdd_roc_req;
	hdd_context_t *hdd_ctx =
			container_of(work, hdd_context_t, roc_req_work.work);

	hdd_debug("going to dequeue roc");

	if (0 != (wlan_hdd_validate_context(hdd_ctx)))
		return;

	/*
	 * The queued roc requests is dequeued and processed one at a time.
	 * Callback 'wlan_hdd_remain_on_channel_callback' ensures
	 * that any pending roc in the queue will be scheduled
	 * on the current roc completion by scheduling the work queue.
	 */
	qdf_spin_lock(&hdd_ctx->hdd_roc_req_q_lock);
	if (list_empty(&hdd_ctx->hdd_roc_req_q.anchor)) {
		qdf_spin_unlock(&hdd_ctx->hdd_roc_req_q_lock);
		return;
	}
	status = qdf_list_remove_front(&hdd_ctx->hdd_roc_req_q,
			(qdf_list_node_t **) &hdd_roc_req);
	qdf_spin_unlock(&hdd_ctx->hdd_roc_req_q_lock);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_debug("unable to remove roc element from list");
		return;
	}
	ret = wlan_hdd_execute_remain_on_channel(
			hdd_roc_req->pAdapter,
			hdd_roc_req->pRemainChanCtx);
	if (ret == -EBUSY) {
		hdd_err("dropping RoC request");
		wlan_hdd_indicate_roc_drop(hdd_roc_req->pAdapter,
					   hdd_roc_req->pRemainChanCtx);
		qdf_mem_free(hdd_roc_req->pRemainChanCtx);
	}
	qdf_mem_free(hdd_roc_req);
}

/**
 * wlan_hdd_is_roc_in_progress_for_other_adapters() - Check if roc is in
 *	progress for another adapter
 * @hdd_ctx: HDD context
 * @cur_adapter: current adapter
 *
 * Roc requests are serialized per adapter. This means that simultaneous
 * roc requests on multiple adapters are not supported. This function checks
 * and returns if there is an roc being executed on another adapter.
 *
 * Return: true if roc is ongoing for another adapter, false otherwise.
 */
static bool
wlan_hdd_is_roc_in_progress_for_other_adapters(hdd_context_t *hdd_ctx,
						hdd_adapter_t *cur_adapter)
{
	hdd_adapter_list_node_t *adapter_node = NULL, *next = NULL;
	hdd_adapter_t *adapter;
	QDF_STATUS qdf_status;

	qdf_status = hdd_get_front_adapter(hdd_ctx, &adapter_node);

	while ((NULL != adapter_node) && (QDF_STATUS_SUCCESS == qdf_status)) {
		adapter = adapter_node->pAdapter;
		if (cur_adapter != adapter) {
			if (adapter->is_roc_inprogress)
				return true;
		}

		qdf_status = hdd_get_next_adapter(hdd_ctx, adapter_node, &next);
		adapter_node = next;
	}

	return false;
}

/**
 * wlan_hdd_is_roc_req_queued_by_other_adapters() - Check if an roc req is
 *	queued by another adapter
 * @hdd_ctx: HDD context
 * @cur_adapter: current adapter
 *
 * Roc requests are serialized per adapter. This means that simultaneous
 * roc requests on multiple adapters are not supported. This function checks
 * and returns if there is an roc request queued by another adapter.
 *
 * Return: true if roc is queued by another adapter, false otherwise.
 */
static bool
wlan_hdd_is_roc_req_queued_by_other_adapters(hdd_context_t *hdd_ctx,
					     hdd_adapter_t *cur_adapter)
{
	qdf_list_node_t *node = NULL, *next_node = NULL;
	hdd_roc_req_t *roc_req;

	qdf_spin_lock(&hdd_ctx->hdd_roc_req_q_lock);
	if (list_empty(&hdd_ctx->hdd_roc_req_q.anchor)) {
		qdf_spin_unlock(&hdd_ctx->hdd_roc_req_q_lock);
		return false;
	}
	if (QDF_STATUS_SUCCESS != qdf_list_peek_front(&hdd_ctx->hdd_roc_req_q,
						      &next_node)) {
		qdf_spin_unlock(&hdd_ctx->hdd_roc_req_q_lock);
		hdd_err("Unable to peek roc element from list");
		return false;
	}

	do {
		node = next_node;
		roc_req = qdf_container_of(node, hdd_roc_req_t, node);
		if (roc_req->pAdapter != cur_adapter) {
			qdf_spin_unlock(&hdd_ctx->hdd_roc_req_q_lock);
			return true;
		}

	} while (QDF_STATUS_SUCCESS  == qdf_list_peek_next(
							&hdd_ctx->hdd_roc_req_q,
							node, &next_node));
	qdf_spin_unlock(&hdd_ctx->hdd_roc_req_q_lock);

	return false;
}

static int wlan_hdd_request_remain_on_channel(struct wiphy *wiphy,
					      struct net_device *dev,
					      struct ieee80211_channel *chan,
					      unsigned int duration,
					      u64 *cookie,
					      enum rem_on_channel_request_type
					      request_type)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx;
	hdd_remain_on_chan_ctx_t *pRemainChanCtx;
	bool isBusy = false;
	uint32_t size = 0;
	hdd_adapter_t *sta_adapter;
	int ret;
	int status = 0;
	int roc_id;

	hdd_debug("Device_mode %s(%d)",
		   hdd_device_mode_to_string(pAdapter->device_mode),
		   pAdapter->device_mode);
	hdd_debug("chan(hw_val)0x%x chan(centerfreq) %d, duration %d",
		 chan->hw_value, chan->center_freq, duration);

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	ret = wlan_hdd_validate_context(pHddCtx);
	if (0 != ret)
		return ret;

	if ((wlan_hdd_is_roc_in_progress_for_other_adapters(pHddCtx, pAdapter))
	   || (wlan_hdd_is_roc_req_queued_by_other_adapters(pHddCtx, pAdapter))
		) {
		hdd_debug("ROC in progress or queued for another adapter");
		return -EAGAIN;
	}
	if (cds_is_connection_in_progress(NULL, NULL)) {
		hdd_debug("Connection is in progress");
		if (request_type == OFF_CHANNEL_ACTION_TX) {
			hdd_debug("Reject Offchannel action frame tx as conection in progress");
			return -EAGAIN;
		}
		isBusy = true;
	}
	pRemainChanCtx = qdf_mem_malloc(sizeof(hdd_remain_on_chan_ctx_t));
	if (NULL == pRemainChanCtx) {
		hdd_err("Not able to allocate memory for Channel context");
		return -ENOMEM;
	}

	if (QDF_STATUS_SUCCESS != qdf_idr_alloc(&pHddCtx->p2p_idr,
		pRemainChanCtx, &roc_id)) {
		hdd_err("alloc id fail");
		qdf_mem_free(pRemainChanCtx);
		return -EFAULT;
	}
	qdf_mem_copy(&pRemainChanCtx->chan, chan,
		     sizeof(struct ieee80211_channel));
	pRemainChanCtx->duration = duration;
	pRemainChanCtx->dev = dev;
	*cookie = (u64)roc_id;
	pRemainChanCtx->id = roc_id;
	pRemainChanCtx->rem_on_chan_request = request_type;
	pRemainChanCtx->action_pkt_buff.freq = 0;
	pRemainChanCtx->action_pkt_buff.frame_ptr = NULL;
	pRemainChanCtx->action_pkt_buff.frame_length = 0;
	pRemainChanCtx->hdd_remain_on_chan_cancel_in_progress = false;
	if (REMAIN_ON_CHANNEL_REQUEST == request_type) {
		sta_adapter = hdd_get_adapter(pHddCtx, QDF_STA_MODE);
		if ((NULL != sta_adapter) &&
			hdd_conn_is_connected(
				WLAN_HDD_GET_STATION_CTX_PTR(sta_adapter))) {
			if (pAdapter->last_roc_ts != 0 &&
				(((uint64_t)qdf_mc_timer_get_system_time() -
					 pAdapter->last_roc_ts) <
				pHddCtx->config->p2p_listen_defer_interval)) {
			if (pRemainChanCtx->duration > HDD_P2P_MAX_ROC_DURATION)
				pRemainChanCtx->duration =
						HDD_P2P_MAX_ROC_DURATION;

			wlan_hdd_roc_request_enqueue(pAdapter, pRemainChanCtx);
			schedule_delayed_work(&pHddCtx->roc_req_work,
			msecs_to_jiffies(
				pHddCtx->config->p2p_listen_defer_interval));
			hdd_debug("Defer interval is %hu, pAdapter %pK",
				pHddCtx->config->p2p_listen_defer_interval,
				pAdapter);
			return 0;
			}
		}
	}

	qdf_spin_lock(&pHddCtx->hdd_roc_req_q_lock);
	size = qdf_list_size(&(pHddCtx->hdd_roc_req_q));
	qdf_spin_unlock(&pHddCtx->hdd_roc_req_q_lock);
	if ((isBusy == false) && (!size)) {
		status = wlan_hdd_execute_remain_on_channel(pAdapter,
							    pRemainChanCtx);
		if (status == -EBUSY) {
			if (wlan_hdd_roc_request_enqueue(pAdapter,
							 pRemainChanCtx)) {
				qdf_mem_free(pRemainChanCtx);
				return -EAGAIN;
			}
		}
		return 0;
	}
	if (wlan_hdd_roc_request_enqueue(pAdapter, pRemainChanCtx)) {
		qdf_mem_free(pRemainChanCtx);
		return -EAGAIN;
	}

	/*
	 * If a connection is not in progress (isBusy), before scheduling
	 * the work queue it is necessary to check if a roc in in progress
	 * or not because: if an roc is in progress, the dequeued roc
	 * that will be processed will be dropped. To ensure that this new
	 * roc request is not dropped, it is suggested to check if an roc
	 * is in progress or not. The existing roc completion will provide
	 * the trigger to dequeue the next roc request.
	 */
	if (isBusy == false && pAdapter->is_roc_inprogress == false) {
		hdd_debug("scheduling delayed work: no connection/roc active");
		schedule_delayed_work(&pHddCtx->roc_req_work, 0);
	}
	return 0;
}

static int __wlan_hdd_cfg80211_remain_on_channel(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 struct ieee80211_channel *chan,
						 unsigned int duration,
						 u64 *cookie)
{
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx;
	int ret;

	ENTER();

	hdd_ctx = WLAN_HDD_GET_CTX(pAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	if (wlan_hdd_validate_session_id(pAdapter->sessionId)) {
		hdd_err("invalid session id: %d", pAdapter->sessionId);
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_REMAIN_ON_CHANNEL,
			 pAdapter->sessionId, REMAIN_ON_CHANNEL_REQUEST));

	ret = wlan_hdd_request_remain_on_channel(wiphy, dev, chan,
						  duration, cookie,
						  REMAIN_ON_CHANNEL_REQUEST);
	EXIT();
	return ret;
}

int wlan_hdd_cfg80211_remain_on_channel(struct wiphy *wiphy,
					struct wireless_dev *wdev,
					struct ieee80211_channel *chan,
					unsigned int duration, u64 *cookie)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_remain_on_channel(wiphy,
						    wdev,
						    chan,
						    duration, cookie);
	cds_ssr_unprotect(__func__);

	return ret;
}

void hdd_remain_chan_ready_handler(hdd_adapter_t *pAdapter,
	uint32_t scan_id)
{
	hdd_cfg80211_state_t *cfgState = NULL;
	hdd_remain_on_chan_ctx_t *pRemainChanCtx = NULL;
	QDF_STATUS status;

	if (NULL == pAdapter) {
		hdd_err("pAdapter is NULL");
		return;
	}
	cfgState = WLAN_HDD_GET_CFG_STATE_PTR(pAdapter);
	hdd_debug("Ready on chan ind %d", scan_id);

	pAdapter->start_roc_ts = (uint64_t)qdf_mc_timer_get_system_time();
	mutex_lock(&cfgState->remain_on_chan_ctx_lock);
	pRemainChanCtx = cfgState->remain_on_chan_ctx;
	if (pRemainChanCtx != NULL) {
		pRemainChanCtx->is_recd_roc_ready = true;
		MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
				 TRACE_CODE_HDD_REMAINCHANREADYHANDLER,
				 pAdapter->sessionId,
				 pRemainChanCtx->duration));
		/* start timer for actual duration */
		if (QDF_TIMER_STATE_RUNNING ==
			qdf_mc_timer_get_current_state(
				&pRemainChanCtx->hdd_remain_on_chan_timer)) {
			hdd_debug("Timer Started before ready event!!!");
			if (qdf_mc_timer_stop(&pRemainChanCtx->
						hdd_remain_on_chan_timer)
					!= QDF_STATUS_SUCCESS)
				hdd_err("Failed to stop hdd_remain_on_chan_timer");
		}
		status =
			qdf_mc_timer_start(&pRemainChanCtx->
					   hdd_remain_on_chan_timer,
					   (pRemainChanCtx->duration +
					    COMPLETE_EVENT_PROPOGATE_TIME));
		if (status != QDF_STATUS_SUCCESS)
			hdd_err("Remain on Channel timer start failed");

		if (REMAIN_ON_CHANNEL_REQUEST ==
		    pRemainChanCtx->rem_on_chan_request) {
			cfg80211_ready_on_channel(
				pAdapter->dev->
				ieee80211_ptr,
				(u64)pRemainChanCtx->id,
				&pRemainChanCtx->chan,
				pRemainChanCtx->
				duration, GFP_KERNEL);
		} else if (OFF_CHANNEL_ACTION_TX ==
			   pRemainChanCtx->rem_on_chan_request) {
			complete(&pAdapter->offchannel_tx_event);
		}
		/* Check for cached action frame */
		if (pRemainChanCtx->action_pkt_buff.frame_length != 0) {
			hdd_debug("Sent cached action frame to supplicant");
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
			cfg80211_rx_mgmt(pAdapter->dev->ieee80211_ptr,
				pRemainChanCtx->action_pkt_buff.freq, 0,
				pRemainChanCtx->action_pkt_buff.frame_ptr,
				pRemainChanCtx->action_pkt_buff.frame_length,
				NL80211_RXMGMT_FLAG_ANSWERED);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0))
			cfg80211_rx_mgmt(pAdapter->dev->ieee80211_ptr,
				pRemainChanCtx->action_pkt_buff.freq, 0,
				pRemainChanCtx->action_pkt_buff.frame_ptr,
				pRemainChanCtx->action_pkt_buff.frame_length,
				NL80211_RXMGMT_FLAG_ANSWERED, GFP_ATOMIC);
#else
			cfg80211_rx_mgmt(pAdapter->dev->ieee80211_ptr,
					 pRemainChanCtx->action_pkt_buff.freq,
					 0,
					 pRemainChanCtx->action_pkt_buff.
					 frame_ptr,
					 pRemainChanCtx->action_pkt_buff.
					 frame_length, GFP_ATOMIC);
#endif /* LINUX_VERSION_CODE */

			qdf_mem_free(pRemainChanCtx->action_pkt_buff.frame_ptr);
			pRemainChanCtx->action_pkt_buff.frame_length = 0;
			pRemainChanCtx->action_pkt_buff.freq = 0;
			pRemainChanCtx->action_pkt_buff.frame_ptr = NULL;
		}
		complete(&pAdapter->rem_on_chan_ready_event);
	} else {
		hdd_debug("No Pending Remain on channel Request");
	}
	mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
}

static int
__wlan_hdd_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
					     struct wireless_dev *wdev,
					     u64 cookie)
{
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_cfg80211_state_t *cfgState = WLAN_HDD_GET_CFG_STATE_PTR(pAdapter);
	hdd_remain_on_chan_ctx_t *pRemainChanCtx;
	hdd_remain_on_chan_ctx_t *cur_ctx;
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	int status;
	int qdf_status;
	unsigned long rc;
	qdf_list_node_t *tmp, *q;
	hdd_roc_req_t *curr_roc_req;
	uint32_t roc_scan_id;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	if (wlan_hdd_validate_session_id(pAdapter->sessionId)) {
		hdd_err("invalid session id: %d", pAdapter->sessionId);
		return -EINVAL;
	}

	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	if (QDF_STATUS_SUCCESS != qdf_idr_find(&pHddCtx->p2p_idr,
		(int32_t)cookie, (void **)&cur_ctx)) {
		hdd_debug("failed to find cookie, 0x%llx", cookie);
		return -EFAULT;
	}

	qdf_spin_lock(&pHddCtx->hdd_roc_req_q_lock);
	list_for_each_safe(tmp, q, &pHddCtx->hdd_roc_req_q.anchor) {
		curr_roc_req = list_entry(tmp, hdd_roc_req_t, node);
		if (curr_roc_req->pRemainChanCtx == cur_ctx) {
			qdf_status = qdf_list_remove_node(&pHddCtx->hdd_roc_req_q,
						      (qdf_list_node_t *)
						      curr_roc_req);
			qdf_spin_unlock(&pHddCtx->hdd_roc_req_q_lock);
			if (qdf_status == QDF_STATUS_SUCCESS) {
				qdf_mem_free(curr_roc_req->pRemainChanCtx);
				qdf_mem_free(curr_roc_req);
			}
			return 0;
		}
	}
	qdf_spin_unlock(&pHddCtx->hdd_roc_req_q_lock);
	/* FIXME cancel currently running remain on chan.
	 * Need to check cookie and cancel accordingly
	 */
	mutex_lock(&cfgState->remain_on_chan_ctx_lock);
	pRemainChanCtx = cfgState->remain_on_chan_ctx;

	if (pRemainChanCtx) {
		hdd_debug("action_id = %x, roc id = %x, cookie = %08llx",
				cfgState->action_id, pRemainChanCtx->id,
				cookie);

		if (pRemainChanCtx->id == (int32_t)cookie) {
			/* request to cancel on-going roc */
			if (cfgState->buf) {
				/* Tx frame pending */
				if (cfgState->action_id != (int32_t)cookie) {
					hdd_debug("Cookie matched with RoC cookie but not with tx cookie, indicate expired event for roc");
					/* RoC was extended to accomodate the tx frame */
					if (REMAIN_ON_CHANNEL_REQUEST ==
							pRemainChanCtx->
							rem_on_chan_request) {
					cfg80211_remain_on_channel_expired(
							pRemainChanCtx->dev->
							ieee80211_ptr,
							(u64)pRemainChanCtx->id,
							&pRemainChanCtx->chan,
							GFP_KERNEL);
					}
					pRemainChanCtx->rem_on_chan_request =
						OFF_CHANNEL_ACTION_TX;
					pRemainChanCtx->id =
						cfgState->action_id;
					mutex_unlock(&cfgState->
						remain_on_chan_ctx_lock);
					return 0;
				}
			}
		} else if (cfgState->buf && cfgState->action_id ==
				(int32_t)cookie) {
			mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
			hdd_debug("Cookie not matched with RoC cookie but matched with tx cookie, cleanup action frame");
			/*free the buf and return 0*/
			hdd_cleanup_actionframe(pHddCtx, pAdapter);
			return 0;
		} else {
			mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
			hdd_debug("No matching cookie");
			return -EINVAL;
		}
	} else {
		mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
		hdd_debug("RoC context is NULL, return success");
		return 0;
	}

	if (NULL != cfgState->remain_on_chan_ctx) {
		if (qdf_mc_timer_stop(&cfgState->remain_on_chan_ctx->
					hdd_remain_on_chan_timer)
				!= QDF_STATUS_SUCCESS)
			hdd_err("Failed to stop hdd_remain_on_chan_timer");
		if (true ==
		    pRemainChanCtx->hdd_remain_on_chan_cancel_in_progress) {
			mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
			hdd_debug("ROC timer cancellation in progress, wait for completion");
			rc = wait_for_completion_timeout(&pAdapter->
							 cancel_rem_on_chan_var,
							 msecs_to_jiffies
								 (WAIT_CANCEL_REM_CHAN));
			if (!rc)
				hdd_err("wait on cancel_rem_on_chan_var timed out");

			return 0;
		}
		pRemainChanCtx->hdd_remain_on_chan_cancel_in_progress = true;
	}
	roc_scan_id = pRemainChanCtx->scan_id;
	mutex_unlock(&cfgState->remain_on_chan_ctx_lock);

	/* wait until remain on channel ready event received
	 * for already issued remain on channel request
	 */
	rc = wait_for_completion_timeout(&pAdapter->rem_on_chan_ready_event,
					 msecs_to_jiffies(WAIT_REM_CHAN_READY));
	if (!rc) {
		hdd_err("timeout waiting for remain on channel ready indication");

		if (cds_is_driver_recovering()) {
			hdd_err("Recovery in Progress. State: 0x%x Ignore!!!",
				 cds_get_driver_state());
			return -EAGAIN;
		}
		cds_flush_logs(WLAN_LOG_TYPE_FATAL,
			WLAN_LOG_INDICATOR_HOST_DRIVER,
			WLAN_LOG_REASON_HDD_TIME_OUT,
			true, false);
	}
	INIT_COMPLETION(pAdapter->cancel_rem_on_chan_var);
	/* Issue abort remain on chan request to sme.
	 * The remain on channel callback will make sure the remain_on_chan
	 * expired event is sent.
	 */
	if ((QDF_STA_MODE == pAdapter->device_mode) ||
	    (QDF_P2P_CLIENT_MODE == pAdapter->device_mode) ||
	    (QDF_P2P_DEVICE_MODE == pAdapter->device_mode)
	    ) {
		uint8_t sessionId = pAdapter->sessionId;

		hdd_delete_all_action_frame_cookies(pAdapter);
		sme_cancel_remain_on_channel(WLAN_HDD_GET_HAL_CTX(pAdapter),
			sessionId, roc_scan_id);
	} else if ((QDF_SAP_MODE == pAdapter->device_mode) ||
		   (QDF_P2P_GO_MODE == pAdapter->device_mode)
		   ) {
		wlansap_cancel_remain_on_channel(
			WLAN_HDD_GET_SAP_CTX_PTR(pAdapter), roc_scan_id);

	} else {
		hdd_err("Invalid device_mode %s(%d)",
			hdd_device_mode_to_string(pAdapter->device_mode),
			pAdapter->device_mode);
		return -EIO;
	}
	rc = wait_for_completion_timeout(&pAdapter->cancel_rem_on_chan_var,
					 msecs_to_jiffies
						 (WAIT_CANCEL_REM_CHAN));
	if (!rc)
		hdd_err("wait on cancel_rem_on_chan_var timed out");

	hdd_allow_suspend(WIFI_POWER_EVENT_WAKELOCK_ROC);

	EXIT();
	return 0;
}

int wlan_hdd_cfg80211_cancel_remain_on_channel(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       u64 cookie)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_cancel_remain_on_channel(wiphy,
							   wdev,
							   cookie);
	cds_ssr_unprotect(__func__);

	return ret;
}

static int __wlan_hdd_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
			      struct ieee80211_channel *chan, bool offchan,
			      unsigned int wait,
			      const u8 *buf, size_t len, bool no_cck,
			      bool dont_wait_for_ack, u64 *cookie)
{
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_cfg80211_state_t *cfgState = WLAN_HDD_GET_CFG_STATE_PTR(pAdapter);
	hdd_remain_on_chan_ctx_t *pRemainChanCtx;
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	uint16_t extendedWait = 0;
	uint8_t type;
	uint8_t subType;
	enum action_frm_type actionFrmType;
	bool noack = 0;
	int status;
	unsigned long rc;
	hdd_adapter_t *goAdapter;
	uint16_t current_freq;
	uint8_t home_ch = 0;
	bool enb_random_mac = false;
	uint32_t mgmt_hdr_len = sizeof(struct ieee80211_hdr_3addr);
	QDF_STATUS qdf_status;
	int32_t mgmt_id;

	ENTER();

	if (len < mgmt_hdr_len + 1) {
		hdd_err("Invalid Length");
		return -EINVAL;
	}

	type = WLAN_HDD_GET_TYPE_FRM_FC(buf[0]);
	subType = WLAN_HDD_GET_SUBTYPE_FRM_FC(buf[0]);

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	if (wlan_hdd_validate_session_id(pAdapter->sessionId)) {
		hdd_err("invalid session id: %d", pAdapter->sessionId);
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_ACTION, pAdapter->sessionId,
			 pAdapter->device_mode));
	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	hdd_debug("Device_mode %s(%d) type: %d, wait: %d, offchan: %d",
		   hdd_device_mode_to_string(pAdapter->device_mode),
		   pAdapter->device_mode, type, wait, offchan);

	/*
	 * When frame to be transmitted is auth mgmt, then trigger
	 * sme_send_mgmt_tx to send auth frame
	 */
	if ((pAdapter->device_mode == QDF_STA_MODE) &&
	    (type == SIR_MAC_MGMT_FRAME &&
	    subType == SIR_MAC_MGMT_AUTH)) {
		qdf_status = sme_send_mgmt_tx(WLAN_HDD_GET_HAL_CTX(pAdapter),
					      pAdapter->sessionId, buf, len);

		if (QDF_IS_STATUS_SUCCESS(qdf_status))
			return 0;
		else
			return -EINVAL;
	}

	if (type == SIR_MAC_MGMT_FRAME && subType == SIR_MAC_MGMT_ACTION &&
	    len > IEEE80211_MIN_ACTION_SIZE)
		hdd_debug("category: %d, actionId: %d",
			  buf[WLAN_HDD_PUBLIC_ACTION_FRAME_BODY_OFFSET +
			  WLAN_HDD_PUBLIC_ACTION_FRAME_CATEGORY_OFFSET],
			  buf[WLAN_HDD_PUBLIC_ACTION_FRAME_BODY_OFFSET +
			  WLAN_HDD_PUBLIC_ACTION_FRAME_ACTION_OFFSET]);

#ifdef WLAN_FEATURE_P2P_DEBUG
	if ((type == SIR_MAC_MGMT_FRAME) &&
	    (subType == SIR_MAC_MGMT_ACTION) &&
	    wlan_hdd_is_type_p2p_action(&buf
				[WLAN_HDD_PUBLIC_ACTION_FRAME_BODY_OFFSET],
				len - mgmt_hdr_len)) {
		actionFrmType = buf[WLAN_HDD_PUBLIC_ACTION_FRAME_TYPE_OFFSET];
		if (actionFrmType >= MAX_P2P_ACTION_FRAME_TYPE) {
			hdd_debug("[P2P] unknown[%d] ---> OTA to " MAC_ADDRESS_STR,
				actionFrmType,
				MAC_ADDR_ARRAY(&buf
					       [WLAN_HDD_80211_FRM_DA_OFFSET]));
		} else {
			hdd_debug("[P2P] %s ---> OTA to "
			       MAC_ADDRESS_STR,
			       p2p_action_frame_type[actionFrmType],
			       MAC_ADDR_ARRAY(&buf
					      [WLAN_HDD_80211_FRM_DA_OFFSET]));
			if ((actionFrmType == WLAN_HDD_PROV_DIS_REQ)
			    && (global_p2p_connection_status == P2P_NOT_ACTIVE)) {
				global_p2p_connection_status = P2P_GO_NEG_PROCESS;
				hdd_debug("[P2P State]Inactive state to GO negotiation progress state");
			} else if ((actionFrmType == WLAN_HDD_GO_NEG_CNF) &&
				   (global_p2p_connection_status ==
				    P2P_GO_NEG_PROCESS)) {
				global_p2p_connection_status =
					P2P_GO_NEG_COMPLETED;
				hdd_debug("[P2P State]GO nego progress to GO nego completed state");
			}
		}
	}
#endif

	noack = dont_wait_for_ack;

	/* If the wait is coming as 0 with off channel set */
	/* then set the wait to 200 ms */
	if (offchan && !wait) {
		wait = ACTION_FRAME_DEFAULT_WAIT;
		mutex_lock(&cfgState->remain_on_chan_ctx_lock);
		if (cfgState->remain_on_chan_ctx) {
			uint64_t current_time =
				(uint64_t)qdf_mc_timer_get_system_time();
			int remaining_roc_time =
				((int) cfgState->remain_on_chan_ctx->duration -
				(current_time - pAdapter->start_roc_ts));

			if (remaining_roc_time > ACTION_FRAME_DEFAULT_WAIT)
				wait = remaining_roc_time;
		}
		mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
	}

	if ((QDF_STA_MODE == pAdapter->device_mode) &&
		(type == SIR_MAC_MGMT_FRAME &&
		subType == SIR_MAC_MGMT_PROBE_RSP)) {
			/* Drop Probe response received
			 * from supplicant in sta mode
			 */
			goto err_rem_channel;
	}

	/* Call sme API to send out a action frame. */
	/* OR can we send it directly through data path?? */
	/* After tx completion send tx status back. */
	if ((QDF_SAP_MODE == pAdapter->device_mode) ||
	    (QDF_P2P_GO_MODE == pAdapter->device_mode)
	    ) {
		if (type == SIR_MAC_MGMT_FRAME) {
			if (subType == SIR_MAC_MGMT_PROBE_RSP) {
				/* Drop Probe response recieved from supplicant,
				 * as for GO and SAP PE itself
				 * sends probe response
				 */
				goto err_rem_channel;
			} else if ((subType == SIR_MAC_MGMT_DISASSOC) ||
				   (subType == SIR_MAC_MGMT_DEAUTH)) {
				/* During EAP failure or P2P Group Remove
				 * supplicant is sending del_station command
				 * to driver. From del_station function,
				 * Driver will send deauth frame top2p client.
				 * No need to send disassoc frame from here.
				 * so Drop the frame here and send tx indication
				 * back to supplicant.
				 */
				uint8_t dstMac[ETH_ALEN] = { 0 };

				memcpy(&dstMac,
				       &buf[WLAN_HDD_80211_FRM_DA_OFFSET],
				       ETH_ALEN);
				hdd_debug("Deauth/Disassoc received for STA:"
					 MAC_ADDRESS_STR,
					 MAC_ADDR_ARRAY(dstMac));
				goto err_rem_channel;
			}
		}
	}

	if (NULL != cfgState->buf) {
		if (!noack) {
			hdd_warn("Previous P2P Action frame packet pending");
			if (!hdd_is_p2p_go_cnf_frame(buf, len))
				hdd_cleanup_actionframe(pAdapter->pHddCtx,
						pAdapter);
			else {
				hdd_cleanup_actionframe_no_wait(
						pAdapter->pHddCtx, pAdapter);
			}
		} else {
			hdd_err("Pending Action frame packet return EBUSY");
			return -EBUSY;
		}
	}

	if (subType == SIR_MAC_MGMT_ACTION) {
		hdd_debug("Action frame tx request : %s",
			   hdd_get_action_string(buf
						 [WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET]));
	}

	if ((pAdapter->device_mode == QDF_SAP_MODE) &&
	    (test_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags))) {
		home_ch = pAdapter->sessionCtx.ap.operatingChannel;
	} else if ((pAdapter->device_mode == QDF_STA_MODE) &&
		   (pAdapter->sessionCtx.station.conn_info.connState ==
				eConnectionState_Associated)) {
		home_ch =
			pAdapter->sessionCtx.station.conn_info.operationChannel;
	} else {
		goAdapter = hdd_get_adapter(pAdapter->pHddCtx, QDF_P2P_GO_MODE);
		if (goAdapter &&
		    (test_bit(SOFTAP_BSS_STARTED, &goAdapter->event_flags)))
			home_ch = goAdapter->sessionCtx.ap.operatingChannel;
	}

	if (chan &&
	    (ieee80211_frequency_to_channel(chan->center_freq) ==
	     home_ch)) {
		/* if adapter is already on requested ch, no need for ROC */
		wait = 0;
		hdd_debug("Adapter already on requested ch. No ROC needed");
		goto send_frame;
	}

	if (offchan && wait && chan) {
		int status;
		enum rem_on_channel_request_type req_type = OFF_CHANNEL_ACTION_TX;
		/* In case of P2P Client mode if we are already */
		/* on the same channel then send the frame directly */

		mutex_lock(&cfgState->remain_on_chan_ctx_lock);
		pRemainChanCtx = cfgState->remain_on_chan_ctx;
		if ((type == SIR_MAC_MGMT_FRAME) &&
		    (subType == SIR_MAC_MGMT_ACTION) &&
		    hdd_p2p_is_action_type_rsp(&buf
			[WLAN_HDD_PUBLIC_ACTION_FRAME_BODY_OFFSET],
			len - mgmt_hdr_len)
		    && cfgState->remain_on_chan_ctx
		    && cfgState->current_freq == chan->center_freq) {
			if (QDF_TIMER_STATE_RUNNING ==
			    qdf_mc_timer_get_current_state(&cfgState->
						   remain_on_chan_ctx->
						   hdd_remain_on_chan_timer)) {

				/* In the latest wpa_supplicant, the wait time
				 * for go negotiation response is set to 100ms,
				 * due to which, there could be a possibility
				 * that, if the go negotaition confirmation
				 * frame is not received within 100 msec, ROC
				 * would be timeout and resulting in connection
				 * failures as the device will not be on the
				 * listen channel anymore to receive the conf
				 * frame. Also wpa_supplicant has set the wait
				 * to 50msec for go negotiation confirmation,
				 * invitation response and prov discovery rsp
				 * frames. So increase the wait time for all
				 * these frames.
				 */
				actionFrmType = buf
				[WLAN_HDD_PUBLIC_ACTION_FRAME_TYPE_OFFSET];
				if (actionFrmType == WLAN_HDD_GO_NEG_RESP ||
				    actionFrmType == WLAN_HDD_PROV_DIS_RESP)
					wait = wait + ACTION_FRAME_RSP_WAIT;
				else if (actionFrmType ==
					 WLAN_HDD_GO_NEG_CNF ||
					 actionFrmType ==
					 WLAN_HDD_INVITATION_RESP)
					wait = wait + ACTION_FRAME_ACK_WAIT;

				hdd_debug("Extending the wait time %d for actionFrmType=%d",
					 wait, actionFrmType);

				if (qdf_mc_timer_stop(&cfgState->
						remain_on_chan_ctx->
						hdd_remain_on_chan_timer)
						!= QDF_STATUS_SUCCESS)
					hdd_warn("Failed to stop hdd_remain_on_chan_timer");
				status =
					qdf_mc_timer_start(&cfgState->
							   remain_on_chan_ctx->
							   hdd_remain_on_chan_timer,
							   wait);
				if (status != QDF_STATUS_SUCCESS)
					hdd_warn("Remain on Channel timer start failed");

				mutex_unlock(&cfgState->
					     remain_on_chan_ctx_lock);
				goto send_frame;
			} else {
				if (pRemainChanCtx->
				    hdd_remain_on_chan_cancel_in_progress ==
				    true) {
					mutex_unlock(&cfgState->
						     remain_on_chan_ctx_lock);
					hdd_debug("action frame tx: waiting for completion of ROC ");

					rc = wait_for_completion_timeout
						     (&pAdapter->cancel_rem_on_chan_var,
						     msecs_to_jiffies
							     (WAIT_CANCEL_REM_CHAN));
					if (!rc)
						hdd_warn("wait on cancel_rem_on_chan_var timed out");
				} else
					mutex_unlock(&cfgState->
						     remain_on_chan_ctx_lock);
			}
		} else
			mutex_unlock(&cfgState->remain_on_chan_ctx_lock);

		mutex_lock(&cfgState->remain_on_chan_ctx_lock);
		pRemainChanCtx = cfgState->remain_on_chan_ctx;

		/* At this point if remain_on_chan_ctx exists but timer not
		 * running means that roc workqueue requested a new RoC and it
		 * is in progress. So wait for Ready on channel indication
		 */
		if ((pRemainChanCtx) &&
			(QDF_TIMER_STATE_RUNNING !=
			 qdf_mc_timer_get_current_state(
				 &pRemainChanCtx->hdd_remain_on_chan_timer))) {
			if (!pRemainChanCtx->is_recd_roc_ready) {
				mutex_unlock(
				    &cfgState->remain_on_chan_ctx_lock);
				hdd_debug("remain_on_chan_ctx exists but RoC timer not running. wait for ready on channel");
				rc = wait_for_completion_timeout(&pAdapter->
						rem_on_chan_ready_event,
						msecs_to_jiffies
						(WAIT_REM_CHAN_READY));
				if (!rc)
					hdd_err("timeout waiting for remain on channel ready indication");
			} else {
				/* Timer expired and posted msg to mc thread
				 * but is not yet processed clean the roc ctx
				 * and send response to upper layer.
				 */
				mutex_unlock(
				    &cfgState->remain_on_chan_ctx_lock);
				INIT_COMPLETION(pAdapter->
						cancel_rem_on_chan_var);
				rc = wait_for_completion_timeout(&pAdapter->
						cancel_rem_on_chan_var,
						msecs_to_jiffies(
							WAIT_CANCEL_REM_CHAN));
				if (!rc) {
					hdd_err("Timeout waiting for cancel ROC indication");
					goto err_rem_channel;
				}
			}
			mutex_lock(&cfgState->remain_on_chan_ctx_lock);
			pRemainChanCtx = cfgState->remain_on_chan_ctx;
		}

		if ((pRemainChanCtx != NULL) &&
			(cfgState->current_freq == chan->center_freq)) {
			mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
			hdd_debug("action frame: extending the wait time");
			extendedWait = (uint16_t) wait;
			goto send_frame;
		}

		mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
		INIT_COMPLETION(pAdapter->offchannel_tx_event);

		status = wlan_hdd_request_remain_on_channel(wiphy, dev, chan,
							    wait, cookie,
							    req_type);
		if (0 != status) {
			if ((-EBUSY == status) &&
			    (cfgState->current_freq == chan->center_freq)) {
				goto send_frame;
			}
			goto err_rem_channel;
		}
		/* This will extend timer in LIM when sending Any action frame
		 * It will cover remain on channel timer till next action frame
		 * in rx direction.
		 */
		extendedWait = (uint16_t) wait;
		/* Wait for driver to be ready on the requested channel */
		rc = wait_for_completion_timeout(&pAdapter->offchannel_tx_event,
						 msecs_to_jiffies
							 (WAIT_CHANGE_CHANNEL_FOR_OFFCHANNEL_TX));
		if (!rc) {
			hdd_err("wait on offchannel_tx_event timed out");
			goto err_rem_channel;
		}
	} else if (offchan) {
		/* Check before sending action frame
		 * whether we already remain on channel
		 */
		if (NULL == cfgState->remain_on_chan_ctx)
			goto err_rem_channel;
	}
send_frame:

	if (!noack) {
		cfgState->buf = qdf_mem_malloc(len);    /* buf; */
		if (cfgState->buf == NULL)
			return -ENOMEM;

		cfgState->len = len;

		qdf_mem_copy(cfgState->buf, buf, len);

		mutex_lock(&cfgState->remain_on_chan_ctx_lock);

		if (cfgState->remain_on_chan_ctx) {
			cfgState->action_id =
				cfgState->remain_on_chan_ctx->id;
			*cookie = cfgState->action_id;
		} else {
			if (QDF_STATUS_SUCCESS != qdf_idr_alloc(
					&pHddCtx->p2p_idr,
					cfgState->buf, &mgmt_id)) {
				hdd_err("alloc id fail");
				goto err;
			}

			*cookie = (u64)mgmt_id;
			cfgState->action_id = mgmt_id;
		}

		mutex_unlock(&cfgState->remain_on_chan_ctx_lock);
	}

	/*
	 * Firmware needs channel information for action frames
	 * which are not sent on the current operating channel of VDEV
	 */
	if ((QDF_P2P_DEVICE_MODE == pAdapter->device_mode) ||
		(QDF_P2P_CLIENT_MODE == pAdapter->device_mode) ||
		(QDF_P2P_GO_MODE == pAdapter->device_mode)) {
		if (chan && (chan->center_freq != 0))
			current_freq = chan->center_freq;
		else
			current_freq = cfgState->current_freq;
	} else {
		current_freq = 0;
	}

	INIT_COMPLETION(pAdapter->tx_action_cnf_event);

	if ((QDF_STA_MODE == pAdapter->device_mode) ||
	    (QDF_P2P_CLIENT_MODE == pAdapter->device_mode) ||
	    (QDF_P2P_DEVICE_MODE == pAdapter->device_mode)) {
		uint8_t sessionId = pAdapter->sessionId;

		if ((type == SIR_MAC_MGMT_FRAME) &&
		    (subType == SIR_MAC_MGMT_ACTION) &&
		     wlan_hdd_is_type_p2p_action(&buf
				[WLAN_HDD_PUBLIC_ACTION_FRAME_BODY_OFFSET],
				len - mgmt_hdr_len)) {
			actionFrmType =
				buf[WLAN_HDD_PUBLIC_ACTION_FRAME_TYPE_OFFSET];
			hdd_debug("Tx Action Frame %u", actionFrmType);
			if (actionFrmType == WLAN_HDD_PROV_DIS_REQ) {
				cfgState->actionFrmState =
					HDD_PD_REQ_ACK_PENDING;
				hdd_debug("HDD_PD_REQ_ACK_PENDING");
			} else if (actionFrmType == WLAN_HDD_GO_NEG_REQ) {
				cfgState->actionFrmState =
					HDD_GO_NEG_REQ_ACK_PENDING;
				hdd_debug("HDD_GO_NEG_REQ_ACK_PENDING");
			}
		}

		if (qdf_mem_cmp((uint8_t *)(&buf[WLAN_HDD_80211_FRM_SA_OFFSET]),
		    &pAdapter->macAddressCurrent, QDF_MAC_ADDR_SIZE)) {
			hdd_info("%s: action frame sa is randomized with mac: "
				 MAC_ADDRESS_STR, __func__,
				 MAC_ADDR_ARRAY((uint8_t *)
				 (&buf[WLAN_HDD_80211_FRM_SA_OFFSET])));
			enb_random_mac = true;
		}

		if (enb_random_mac && !noack && chan && chan->center_freq)
			hdd_set_action_frame_random_mac(pAdapter,
				(uint8_t *)(&buf[WLAN_HDD_80211_FRM_SA_OFFSET]),
				*cookie, chan->center_freq);

		if (QDF_STATUS_SUCCESS !=
		    sme_send_action(WLAN_HDD_GET_HAL_CTX(pAdapter),
				    sessionId, buf, len, extendedWait, noack,
				    current_freq)) {
			hdd_err("sme_send_action returned fail");
			goto err;
		}
	} else if (QDF_SAP_MODE == pAdapter->device_mode ||
		   QDF_P2P_GO_MODE == pAdapter->device_mode) {
		if (QDF_STATUS_SUCCESS !=
		    wlansap_send_action(WLAN_HDD_GET_SAP_CTX_PTR(pAdapter),
					buf, len, 0, current_freq)) {
			hdd_err("wlansap_send_action returned fail");
			goto err;
		}
	}

	return 0;
err:
	if (!noack) {
		if (enb_random_mac && chan && chan->center_freq &&
			((pAdapter->device_mode == QDF_STA_MODE) ||
			(pAdapter->device_mode == QDF_P2P_CLIENT_MODE) ||
			(pAdapter->device_mode == QDF_P2P_DEVICE_MODE)))
			hdd_reset_action_frame_random_mac(pAdapter,
				(uint8_t *)(&buf[WLAN_HDD_80211_FRM_SA_OFFSET]),
				*cookie);

		hdd_send_action_cnf(pAdapter, false);
	}
	return 0;
err_rem_channel:
	if (QDF_STATUS_SUCCESS != qdf_idr_alloc(&pHddCtx->p2p_idr,
		cfgState, &mgmt_id))
		mgmt_id = 0;

	*cookie = (u64)mgmt_id;
	cfg80211_mgmt_tx_status(
		pAdapter->dev->ieee80211_ptr,
		*cookie, buf, len, false, GFP_KERNEL);
	qdf_idr_remove(&pHddCtx->p2p_idr, mgmt_id);

	EXIT();
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
int wlan_hdd_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
		     struct cfg80211_mgmt_tx_params *params, u64 *cookie)
#else
int wlan_hdd_mgmt_tx(struct wiphy *wiphy, struct wireless_dev *wdev,
		     struct ieee80211_channel *chan, bool offchan,
		     unsigned int wait,
		     const u8 *buf, size_t len, bool no_cck,
		     bool dont_wait_for_ack, u64 *cookie)
#endif /* LINUX_VERSION_CODE */
{
	int ret;

	cds_ssr_protect(__func__);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
	ret = __wlan_hdd_mgmt_tx(wiphy, wdev, params->chan, params->offchan,
				 params->wait, params->buf, params->len,
				 params->no_cck, params->dont_wait_for_ack,
				 cookie);
#else
	ret = __wlan_hdd_mgmt_tx(wiphy, wdev, chan, offchan,
				 wait, buf, len, no_cck,
				 dont_wait_for_ack, cookie);
#endif /* LINUX_VERSION_CODE */
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * hdd_wlan_delete_mgmt_tx_cookie() - Wrapper to delete action frame cookie
 * @wdev: Pointer to wireless device
 * @cookie: Cookie to be deleted
 *
 * This is a wrapper function which actually invokes the hdd api to delete
 * cookie based on the device mode of adapter.
 *
 * Return: 0 - for success else negative value
 */
static int hdd_wlan_delete_mgmt_tx_cookie(struct wireless_dev *wdev,
				   u64 cookie)
{
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	if ((adapter->device_mode == QDF_STA_MODE) ||
	    (adapter->device_mode == QDF_P2P_CLIENT_MODE) ||
	    (adapter->device_mode == QDF_P2P_DEVICE_MODE)) {
		hdd_delete_action_frame_cookie(adapter, cookie);
	}

	return 0;
}

static int __wlan_hdd_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
						   struct wireless_dev *wdev,
						   u64 cookie)
{
	hdd_wlan_delete_mgmt_tx_cookie(wdev, cookie);
	return wlan_hdd_cfg80211_cancel_remain_on_channel(wiphy, wdev, cookie);
}

int wlan_hdd_cfg80211_mgmt_tx_cancel_wait(struct wiphy *wiphy,
					  struct wireless_dev *wdev, u64 cookie)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_mgmt_tx_cancel_wait(wiphy, wdev, cookie);
	cds_ssr_unprotect(__func__);

	return ret;
}

void hdd_send_action_cnf(hdd_adapter_t *pAdapter, bool actionSendSuccess)
{
	hdd_cfg80211_state_t *cfgState = WLAN_HDD_GET_CFG_STATE_PTR(pAdapter);
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(pAdapter);

	cfgState->actionFrmState = HDD_IDLE;

	if (NULL == cfgState->buf) {
		qdf_idr_remove(&hdd_ctx->p2p_idr, cfgState->action_id);
		return;
	}

	if (cfgState->is_go_neg_ack_received) {

		cfgState->is_go_neg_ack_received = 0;
		/* Sometimes its possible that host may receive the ack for GO
		 * negotiation req after sending go negotaition confirmation,
		 * in such case drop the ack received for the go negotiation
		 * request, so that supplicant waits for the confirmation ack
		 * from firmware.
		 */
		hdd_debug("Drop the pending ack received in cfgState->actionFrmState %d",
				cfgState->actionFrmState);
		qdf_idr_remove(&hdd_ctx->p2p_idr, cfgState->action_id);
		return;
	}

	hdd_debug("Send Action cnf, actionSendSuccess %d",
		actionSendSuccess);
	/*
	 * buf is the same pointer it passed us to send. Since we are sending
	 * it through control path, we use different buffers.
	 * In case of mac80211, they just push it to the skb and pass the same
	 * data while sending tx ack status.
	 */
	cfg80211_mgmt_tx_status(
		pAdapter->dev->ieee80211_ptr,
		(u64)cfgState->action_id,
		cfgState->buf, cfgState->len,
		actionSendSuccess, GFP_KERNEL);

	qdf_idr_remove(&hdd_ctx->p2p_idr, cfgState->action_id);
	qdf_mem_free(cfgState->buf);
	cfgState->buf = NULL;

	complete(&pAdapter->tx_action_cnf_event);
}

/**
 * hdd_send_action_cnf_cb - action confirmation callback
 * @session_id: SME session ID
 * @tx_completed: ack status
 *
 * This function invokes hdd_sendActionCnf to update ack status to
 * supplicant.
 */
void hdd_send_action_cnf_cb(uint32_t session_id, bool tx_completed)
{
	hdd_context_t *hdd_ctx;
	hdd_adapter_t *adapter;

	ENTER();

	/* Get the HDD context.*/
	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (0 != wlan_hdd_validate_context(hdd_ctx))
		return;

	adapter = hdd_get_adapter_by_sme_session_id(hdd_ctx, session_id);
	if (NULL == adapter) {
		hdd_err("adapter not found");
		return;
	}

	if (WLAN_HDD_ADAPTER_MAGIC != adapter->magic) {
		hdd_err("adapter has invalid magic");
		return;
	}

	hdd_send_action_cnf(adapter, tx_completed);
}

/**
 * hdd_set_p2p_noa
 *
 ***FUNCTION:
 * This function is called from hdd_hostapd_ioctl function when Driver
 * get P2P_SET_NOA comand from wpa_supplicant using private ioctl
 *
 ***LOGIC:
 * Fill NoA Struct According to P2P Power save Option and Pass it to SME layer
 *
 ***ASSUMPTIONS:
 *
 *
 ***NOTE:
 *
 * @param dev          Pointer to net device structure
 * @param command      Pointer to command
 *
 * @return Status
 */

int hdd_set_p2p_noa(struct net_device *dev, uint8_t *command)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
	tP2pPsConfig NoA;
	int count, duration, interval;
	char *param;
	int ret;

	param = strnchr(command, strlen(command), ' ');
	if (param == NULL) {
		hdd_err("strnchr failed to find delimeter");
		return -EINVAL;
	}
	param++;
	ret = sscanf(param, "%d %d %d", &count, &interval, &duration);
	if (ret != 3) {
		hdd_err("P2P_SET GO NoA: fail to read params, ret=%d",
			ret);
		return -EINVAL;
	}
	if (count < 0 || interval < 0 || duration < 0 ||
	    interval > MAX_MUS_VAL || duration > MAX_MUS_VAL) {
		hdd_err("Invalid NOA parameters");
		return -EINVAL;
	}
	hdd_debug("P2P_SET GO NoA: count=%d interval=%d duration=%d",
		count, interval, duration);
	duration = MS_TO_TU_MUS(duration);
	/* PS Selection
	 * Periodic NoA (2)
	 * Single NOA   (4)
	 */
	NoA.opp_ps = 0;
	NoA.ctWindow = 0;
	if (count == 1) {
		NoA.duration = 0;
		NoA.single_noa_duration = duration;
		NoA.psSelection = P2P_POWER_SAVE_TYPE_SINGLE_NOA;
	} else {
		NoA.duration = duration;
		NoA.single_noa_duration = 0;
		NoA.psSelection = P2P_POWER_SAVE_TYPE_PERIODIC_NOA;
	}
	NoA.interval = MS_TO_TU_MUS(interval);
	NoA.count = count;
	NoA.sessionid = pAdapter->sessionId;

	hdd_debug("P2P_PS_ATTR:oppPS %d ctWindow %d duration %d "
		  "interval %d count %d single noa duration %d "
		  "PsSelection %x", NoA.opp_ps,
		  NoA.ctWindow, NoA.duration, NoA.interval,
		  NoA.count, NoA.single_noa_duration, NoA.psSelection);

	sme_p2p_set_ps(hHal, &NoA);
	return 0;
}

/**
 * hdd_set_p2p_opps
 *
 ***FUNCTION:
 * This function is called from hdd_hostapd_ioctl function when Driver
 * get P2P_SET_PS comand from wpa_supplicant using private ioctl
 *
 ***LOGIC:
 * Fill NoA Struct According to P2P Power save Option and Pass it to SME layer
 *
 ***ASSUMPTIONS:
 *
 *
 ***NOTE:
 *
 * @param  dev         Pointer to net device structure
 * @param  command     Pointer to command
 *
 * @return Status
 */

int hdd_set_p2p_opps(struct net_device *dev, uint8_t *command)
{
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	tHalHandle handle = WLAN_HDD_GET_HAL_CTX(adapter);
	tP2pPsConfig noa;
	char *param;
	int legacy_ps, opp_ps, ctwindow;
	int ret;

	param = strnchr(command, strlen(command), ' ');
	if (param == NULL) {
		hdd_err("strnchr failed to find delimiter");
		return -EINVAL;
	}
	param++;
	ret = sscanf(param, "%d %d %d", &legacy_ps, &opp_ps, &ctwindow);
	if (ret != 3) {
		hdd_err("P2P_SET GO PS: fail to read params, ret=%d", ret);
		return -EINVAL;
	}

	if ((opp_ps != -1) && (opp_ps != 0) && (opp_ps != 1)) {
		hdd_err("Invalid opp_ps value:%d", opp_ps);
		return -EINVAL;
	}

	/* P2P spec: 3.3.2 Power Management and discovery:
	 *     CTWindow should be at least 10 TU.
	 * P2P spec: Table 27 - CTWindow and OppPS Parameters field format:
	 *     CTWindow and OppPS Parameters together is 8 bits.
	 *     CTWindow uses 7 bits (0-6, Bit 7 is for OppPS)
	 * 0 indicates that there shall be no CTWindow
	 */
	if ((ctwindow != -1) && (ctwindow != 0) &&
	    (!((ctwindow >= 10) && (ctwindow <= 127)))) {
		hdd_err("Invalid CT window value:%d", ctwindow);
		return -EINVAL;
	}

	hdd_debug("P2P_SET GO PS: legacy_ps=%d opp_ps=%d ctwindow=%d",
		  legacy_ps, opp_ps, ctwindow);

	/* PS Selection
	 * Opportunistic Power Save (1)
	 */

	/* From wpa_cli user need to use separate command to set ctWindow and
	 * Opps when user want to set ctWindow during that time other parameters
	 * values are coming from wpa_supplicant as -1.
	 * Example : User want to set ctWindow with 30 then wpa_cli command :
	 * P2P_SET ctwindow 30
	 * Command Received at hdd_hostapd_ioctl is as below:
	 * P2P_SET_PS -1 -1 30 (legacy_ps = -1, opp_ps = -1, ctwindow = 30)
	 *
	 * e.g., 1: P2P_SET_PS 1 1 30
	 * Driver sets the Opps and CTwindow as 30 and send it to FW.
	 * e.g., 2: P2P_SET_PS 1 -1 15
	 * Driver caches the CTwindow value but not send the command to FW.
	 * e.g., 3: P2P_SET_PS 1 1 -1
	 * Driver sends the command to FW with Opps enabled and CT window as
	 * 15 (last cached CTWindow value).
	 * (or) : P2P_SET_PS 1 1 20
	 * Driver sends the command to FW with opps enabled and CT window
	 * as 20.
	 *
	 * legacy_ps param remains unused until required in the future.
	 */
	if (ctwindow != -1)
		adapter->ctw = ctwindow;

	/* Send command to FW when OppPS is either enabled(1)/disbaled(0) */
	if (opp_ps != -1) {
		adapter->ops = opp_ps;
		noa.opp_ps = adapter->ops;
		noa.ctWindow = adapter->ctw;
		noa.duration = 0;
		noa.single_noa_duration = 0;
		noa.interval = 0;
		noa.count = 0;
		noa.psSelection = P2P_POWER_SAVE_TYPE_OPPORTUNISTIC;
		noa.sessionid = adapter->sessionId;

		hdd_debug("P2P_PS_ATTR: oppPS %d ctWindow %d duration %d interval %d count %d single noa duration %d PsSelection %x",
			noa.opp_ps, noa.ctWindow,
			noa.duration, noa.interval, noa.count,
			noa.single_noa_duration,
			noa.psSelection);

		sme_p2p_set_ps(handle, &noa);
	}

	return 0;
}

int hdd_set_p2p_ps(struct net_device *dev, void *msgData)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tP2pPsConfig NoA;
	struct p2p_app_set_ps *pappNoA = (struct p2p_app_set_ps *) msgData;

	NoA.opp_ps = pappNoA->opp_ps;
	NoA.ctWindow = pappNoA->ctWindow;
	NoA.duration = pappNoA->duration;
	NoA.interval = pappNoA->interval;
	NoA.count = pappNoA->count;
	NoA.single_noa_duration = pappNoA->single_noa_duration;
	NoA.psSelection = pappNoA->psSelection;
	NoA.sessionid = pAdapter->sessionId;

	sme_p2p_set_ps(hHal, &NoA);
	return status;
}

static uint8_t wlan_hdd_get_session_type(enum nl80211_iftype type)
{
	switch (type) {
	case NL80211_IFTYPE_AP:
		return QDF_SAP_MODE;
	case NL80211_IFTYPE_P2P_GO:
		return QDF_P2P_GO_MODE;
	case NL80211_IFTYPE_P2P_CLIENT:
		return QDF_P2P_CLIENT_MODE;
	case NL80211_IFTYPE_STATION:
		return QDF_STA_MODE;
	default:
		return QDF_STA_MODE;
	}
}

/**
 * wlan_hdd_allow_sap_add() - check to add new sap interface
 * @hdd_ctx: pointer to hdd context
 * @name: name of the new interface
 * @sap_dev: output pointer to hold existing interface
 *
 * Return: If able to add interface return true else false
 */
static bool
wlan_hdd_allow_sap_add(hdd_context_t *hdd_ctx,
		       const char *name,
		       struct wireless_dev **sap_dev)
{
	hdd_adapter_list_node_t *adapter_node = NULL, *next = NULL;
	QDF_STATUS status;
	hdd_adapter_t *adapter;

	*sap_dev = NULL;
	status = hdd_get_front_adapter(hdd_ctx, &adapter_node);
	while (adapter_node && QDF_IS_STATUS_SUCCESS(status)) {
		adapter = adapter_node->pAdapter;
		if (adapter && adapter->device_mode == QDF_SAP_MODE &&
		    test_bit(NET_DEVICE_REGISTERED, &adapter->event_flags) &&
		    adapter->dev &&
		    !strncmp(adapter->dev->name, name, IFNAMSIZ)) {
			beacon_data_t *beacon = adapter->sessionCtx.ap.beacon;

			hdd_debug("iface already registered");
			if (beacon) {
				adapter->sessionCtx.ap.beacon = NULL;
				qdf_mem_free(beacon);
			}
			if (adapter->dev->ieee80211_ptr) {
				*sap_dev = adapter->dev->ieee80211_ptr;
				return false;
			}

			hdd_err("ieee80211_ptr points to NULL");
			return false;
		}

		status = hdd_get_next_adapter(hdd_ctx, adapter_node, &next);
		adapter_node = next;
	}

	return true;
}

/**
 * __wlan_hdd_add_virtual_intf() - Add virtual interface
 * @wiphy: wiphy pointer
 * @name: User-visible name of the interface
 * @name_assign_type: the name of assign type of the netdev
 * @nl80211_iftype: (virtual) interface types
 * @flags: moniter configuraiton flags (not used)
 * @vif_params: virtual interface parameters (not used)
 *
 * Return: the pointer of wireless dev, otherwise ERR_PTR.
 */
static
struct wireless_dev *__wlan_hdd_add_virtual_intf(struct wiphy *wiphy,
						 const char *name,
						 unsigned char name_assign_type,
						 enum nl80211_iftype type,
						 u32 *flags,
						 struct vif_params *params)
{
	hdd_context_t *pHddCtx = (hdd_context_t *) wiphy_priv(wiphy);
	hdd_adapter_t *pAdapter = NULL;
	hdd_scaninfo_t *scan_info = NULL;
	int ret;
	uint8_t session_type;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return ERR_PTR(-EINVAL);
	}

	ret = wlan_hdd_validate_context(pHddCtx);
	if (0 != ret)
		return ERR_PTR(ret);

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_ADD_VIRTUAL_INTF, NO_SESSION, type));
	/*
	 * Allow addition multiple interfaces for QDF_P2P_GO_MODE,
	 * QDF_SAP_MODE, QDF_P2P_CLIENT_MODE and QDF_STA_MODE
	 * session type.
	 */
	session_type = wlan_hdd_get_session_type(type);
	if (hdd_get_adapter(pHddCtx, session_type) != NULL
	    && QDF_SAP_MODE != session_type
	    && QDF_P2P_GO_MODE != session_type
	    && QDF_P2P_CLIENT_MODE != session_type
	    && QDF_STA_MODE != session_type) {
		hdd_err("Interface type %d already exists. Two interfaces of same type are not supported currently.",
			type);
		return ERR_PTR(-EINVAL);
	}

	pAdapter = hdd_get_adapter(pHddCtx, QDF_STA_MODE);
	if ((pAdapter != NULL) &&
		!(wlan_hdd_validate_session_id(pAdapter->sessionId))) {
		scan_info = &pAdapter->scan_info;
		if (scan_info->mScanPending) {
			hdd_abort_mac_scan(pHddCtx, pAdapter->sessionId,
					   INVALID_SCAN_ID,
					   eCSR_SCAN_ABORT_DEFAULT);
			hdd_debug("Abort Scan while adding virtual interface");
		}
	}

	if (session_type == QDF_SAP_MODE) {
		struct wireless_dev *sap_dev;
		bool allow_add_sap = wlan_hdd_allow_sap_add(pHddCtx, name,
							    &sap_dev);
		if (!allow_add_sap) {
			if (sap_dev)
				return sap_dev;

			return ERR_PTR(-EINVAL);
		}
	}

	pAdapter = NULL;
	if (pHddCtx->config->isP2pDeviceAddrAdministrated &&
	    ((NL80211_IFTYPE_P2P_GO == type) ||
	     (NL80211_IFTYPE_P2P_CLIENT == type))) {
		/*
		 * Generate the P2P Interface Address. this address must be
		 * different from the P2P Device Address.
		 */
		struct qdf_mac_addr p2pDeviceAddress =
						pHddCtx->p2pDeviceAddress;
		p2pDeviceAddress.bytes[4] ^= 0x80;
		pAdapter = hdd_open_adapter(pHddCtx,
					    session_type,
					    name, p2pDeviceAddress.bytes,
					    name_assign_type,
					    true);
	} else {
		pAdapter = hdd_open_adapter(pHddCtx,
					    session_type,
					    name,
					    wlan_hdd_get_intf_addr(
								pHddCtx,
								session_type),
					    name_assign_type,
					    true);
	}

	if (NULL == pAdapter) {
		hdd_err("hdd_open_adapter failed");
		return ERR_PTR(-ENOSPC);
	}

	/*
	 * Add interface can be requested from the upper layer at any time
	 * check the statemachine for modules state and if they are closed
	 * open the modules.
	 */
	ret = hdd_wlan_start_modules(pHddCtx, pAdapter, false);
	if (ret) {
		hdd_err("Failed to start the wlan_modules");
		goto close_adapter;
	}

	/*
	 * Once the support for session creation/deletion from
	 * hdd_hostapd_open/hdd_host_stop is in place.
	 * The support for starting adapter from here can be removed.
	 */
	if (NL80211_IFTYPE_AP == type || (NL80211_IFTYPE_P2P_GO == type)) {
		ret = hdd_start_adapter(pAdapter);
		if (ret) {
			hdd_err("Failed to start %s", name);
			goto stop_modules;
		}
	}

	if (pHddCtx->rps)
		hdd_send_rps_ind(pAdapter);

	EXIT();
	return pAdapter->dev->ieee80211_ptr;

stop_modules:
	/*
	 * Find if any iface is up. If there is not iface which is up
	 * start the timer to close the modules
	 */
	if (hdd_check_for_opened_interfaces(pHddCtx)) {
		hdd_debug("Closing all modules from the add_virt_iface");
		qdf_sched_delayed_work(&pHddCtx->iface_idle_work,
				       pHddCtx->config->iface_change_wait_time);
		hdd_prevent_suspend_timeout(
			pHddCtx->config->iface_change_wait_time,
			WIFI_POWER_EVENT_WAKELOCK_IFACE_CHANGE_TIMER);
	} else
		hdd_debug("Other interfaces are still up dont close modules!");

close_adapter:
	hdd_close_adapter(pHddCtx, pAdapter, true);

	return ERR_PTR(-EINVAL);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)
struct wireless_dev *wlan_hdd_add_virtual_intf(struct wiphy *wiphy,
					       const char *name,
					       unsigned char name_assign_type,
					       enum nl80211_iftype type,
					       struct vif_params *params)
{
	struct wireless_dev *wdev;

	cds_ssr_protect(__func__);
	wdev = __wlan_hdd_add_virtual_intf(wiphy, name, name_assign_type,
					   type, &params->flags, params);
	cds_ssr_unprotect(__func__);

	return wdev;
}
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)) || defined(WITH_BACKPORTS)
/**
 * wlan_hdd_add_virtual_intf() - Add virtual interface wrapper
 * @wiphy: wiphy pointer
 * @name: User-visible name of the interface
 * @name_assign_type: the name of assign type of the netdev
 * @nl80211_iftype: (virtual) interface types
 * @flags: monitor mode configuration flags (not used)
 * @vif_params: virtual interface parameters (not used)
 *
 * Return: the pointer of wireless dev, otherwise ERR_PTR.
 */
struct wireless_dev *wlan_hdd_add_virtual_intf(struct wiphy *wiphy,
					       const char *name,
					       unsigned char name_assign_type,
					       enum nl80211_iftype type,
					       u32 *flags,
					       struct vif_params *params)
{
	struct wireless_dev *wdev;

	cds_ssr_protect(__func__);
	wdev = __wlan_hdd_add_virtual_intf(wiphy, name, name_assign_type,
					   type, flags, params);
	cds_ssr_unprotect(__func__);
	return wdev;

}
#else
/**
 * wlan_hdd_add_virtual_intf() - Add virtual interface wrapper
 * @wiphy: wiphy pointer
 * @name: User-visible name of the interface
 * @nl80211_iftype: (virtual) interface types
 * @flags: monitor mode configuration flags (not used)
 * @vif_params: virtual interface parameters (not used)
 *
 * Return: the pointer of wireless dev, otherwise ERR_PTR.
 */
struct wireless_dev *wlan_hdd_add_virtual_intf(struct wiphy *wiphy,
					       const char *name,
					       enum nl80211_iftype type,
					       u32 *flags,
					       struct vif_params *params)
{
	struct wireless_dev *wdev;
	unsigned char name_assign_type = 0;

	cds_ssr_protect(__func__);
	wdev = __wlan_hdd_add_virtual_intf(wiphy, name, name_assign_type,
					   type, flags, params);
	cds_ssr_unprotect(__func__);
	return wdev;

}
#endif

int __wlan_hdd_del_virtual_intf(struct wiphy *wiphy, struct wireless_dev *wdev)
{
	struct net_device *dev = wdev->netdev;
	hdd_context_t *pHddCtx = (hdd_context_t *) wiphy_priv(wiphy);
	hdd_adapter_t *pVirtAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	int status;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	/*
	 * Clear SOFTAP_INIT_DONE flag to mark SAP unload, so that we do
	 * not restart SAP after SSR as SAP is already stopped from user space.
	 */
	clear_bit(SOFTAP_INIT_DONE, &pVirtAdapter->event_flags);

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_DEL_VIRTUAL_INTF,
			 pVirtAdapter->sessionId, pVirtAdapter->device_mode));
	hdd_debug("Device_mode %s(%d)",
		   hdd_device_mode_to_string(pVirtAdapter->device_mode),
		   pVirtAdapter->device_mode);

	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	/* check state machine state and kickstart modules if they are closed */
	status = hdd_wlan_start_modules(pHddCtx, pVirtAdapter, false);
	if (status)
		return status;

	if (pVirtAdapter->device_mode == QDF_SAP_MODE &&
	    wlan_sap_is_pre_cac_active(pHddCtx->hHal)) {
		hdd_clean_up_pre_cac_interface(pHddCtx);
	} else {
		wlan_hdd_release_intf_addr(pHddCtx,
					 pVirtAdapter->macAddressCurrent.bytes);
		hdd_stop_adapter(pHddCtx, pVirtAdapter, true);
		hdd_close_adapter(pHddCtx, pVirtAdapter, true);
	}

	EXIT();

	return 0;
}

int wlan_hdd_del_virtual_intf(struct wiphy *wiphy, struct wireless_dev *wdev)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_del_virtual_intf(wiphy, wdev);
	cds_ssr_unprotect(__func__);

	return ret;
}

#ifdef WLAN_FEATURE_P2P_DEBUG
/*
 * wlan_hdd_p2p_action_debug() - Log P2P state and update global status
 * @actionFrmType: action frame type
 * @macFrom: peer mac address
 *
 * return: void
 */
static void wlan_hdd_p2p_action_debug(enum action_frm_type actionFrmType,
					uint8_t *macFrom)
{
	if (actionFrmType >= MAX_P2P_ACTION_FRAME_TYPE) {
		hdd_debug("[P2P] unknown[%d] <--- OTA from " MAC_ADDRESS_STR,
			actionFrmType, MAC_ADDR_ARRAY(macFrom));
	} else {
		hdd_debug("[P2P] %s <--- OTA from " MAC_ADDRESS_STR,
			p2p_action_frame_type[actionFrmType],
			MAC_ADDR_ARRAY(macFrom));
		if ((actionFrmType == WLAN_HDD_PROV_DIS_REQ)
		    && (global_p2p_connection_status == P2P_NOT_ACTIVE)) {
			global_p2p_connection_status = P2P_GO_NEG_PROCESS;
			hdd_debug("[P2P State]Inactive state to GO negotiation progress state");
		} else
		if ((actionFrmType == WLAN_HDD_GO_NEG_CNF)
		    && (global_p2p_connection_status == P2P_GO_NEG_PROCESS)) {
			global_p2p_connection_status = P2P_GO_NEG_COMPLETED;
			hdd_debug("[P2P State]GO negotiation progress to GO negotiation completed state");
		} else
		if ((actionFrmType == WLAN_HDD_INVITATION_REQ)
		    && (global_p2p_connection_status == P2P_NOT_ACTIVE)) {
			global_p2p_connection_status = P2P_GO_NEG_COMPLETED;
			hdd_debug("[P2P State]Inactive state to GO negotiation completed state Autonomous GO formation");
		}
	}
}
#else
/*
 * wlan_hdd_p2p_action_debug() - dummy
 * @actionFrmType: action frame type
 * @macFrom: peer mac address
 *
 * return: void
 */
static void wlan_hdd_p2p_action_debug(enum action_frm_type actionFrmType,
					uint8_t *macFrom)
{

}
#endif

static inline bool is_non_probe_req_mgmt_frame(uint8_t type, uint8_t sub_type)
{
	return type == SIR_MAC_MGMT_FRAME &&
		sub_type != SIR_MAC_MGMT_PROBE_REQ;
}

static hdd_adapter_t *get_adapter_from_broadcast_ocb(hdd_context_t *hdd_ctx,
						     uint8_t *broadcast)
{
	hdd_adapter_t *adapter;

	adapter = hdd_get_adapter(hdd_ctx, QDF_OCB_MODE);
	if (!adapter) {
		hdd_err("Dropping the frame. No frame with BCST as destination");
		return NULL;
	}
	if (broadcast) {
		hdd_debug("Set broadcast to true");
		*broadcast = 1;
	}

	return adapter;
}

static hdd_adapter_t *get_adapter_from_mac_addr(hdd_context_t *hdd_ctx,
						uint8_t *dest_addr)
{
	hdd_adapter_t *adapter;

	adapter = hdd_get_adapter_by_macaddr(hdd_ctx, dest_addr);

	return (adapter) ? adapter :
		hdd_get_adapter_by_rand_macaddr(hdd_ctx, dest_addr);
}

static inline bool is_mgmt_non_broadcast_frame(uint8_t type,
					       uint8_t sub_type,
					       uint8_t broadcast)
{
	return (type == SIR_MAC_MGMT_FRAME) &&
		(sub_type == SIR_MAC_MGMT_ACTION) && !broadcast;
}

static inline bool is_public_action_frame(uint8_t *pb_frames,
					  uint32_t frame_len)
{
	if (frame_len <= WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET) {
		hdd_debug("Not a public action frame len: %d", frame_len);
		return false;
	}

	return (pb_frames[WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET] ==
		WLAN_HDD_PUBLIC_ACTION_FRAME);
}

static inline bool is_p2p_action_frame(uint8_t *pb_frames,
				       uint32_t frame_len)
{
	if (frame_len <= (WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET +
	    SIR_MAC_P2P_OUI_SIZE + 2)) {
		hdd_debug("Not a p2p action frame len: %d", frame_len);
		return false;
	}

	if ((pb_frames[WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET + 1]
		 != SIR_MAC_ACTION_VENDOR_SPECIFIC))
		return false;

	return !qdf_mem_cmp(&pb_frames
			    [WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET + 2],
			    SIR_MAC_P2P_OUI, SIR_MAC_P2P_OUI_SIZE);
}

static inline bool is_tdls_action_frame(uint8_t *pb_frames,
					uint32_t frame_len)
{
	if (frame_len <= WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET) {
		hdd_debug("Not a TDLS action frame len: %d", frame_len);
		return false;
	}

	return (pb_frames[WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET] ==
		    WLAN_HDD_TDLS_ACTION_FRAME);
}

static inline bool is_tdls_disc_resp_frame(uint8_t *pb_frames,
					   uint32_t frame_len)
{
	if (frame_len <= WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET + 1) {
		hdd_debug("Not a TDLS disc resp frame len: %d", frame_len);
		return false;
	}

	return (pb_frames[WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET + 1] ==
		WLAN_HDD_PUBLIC_ACTION_TDLS_DISC_RESP);
}

static inline bool is_qos_action_frame(uint8_t *pb_frames, uint32_t frame_len)
{
	if (frame_len <= WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET + 1) {
		hdd_debug("Not a QOS frame len: %d", frame_len);
		return false;
	}

	return ((pb_frames[WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET] ==
		 WLAN_HDD_QOS_ACTION_FRAME) &&
		(pb_frames[WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET + 1] ==
		 WLAN_HDD_QOS_MAP_CONFIGURE));
}

static inline bool is_extend_roc_timer_for_action(enum action_frm_type
						  frm_type)
{
	switch (frm_type) {
	case WLAN_HDD_GO_NEG_REQ:
	case WLAN_HDD_GO_NEG_RESP:
	case WLAN_HDD_INVITATION_REQ:
	case WLAN_HDD_DEV_DIS_REQ:
	case WLAN_HDD_PROV_DIS_REQ:
		return true;
	default:
		return false;
	}
}

static inline int get_roc_extension_time(enum action_frm_type frm_type)
{
	switch (frm_type) {
	case WLAN_HDD_GO_NEG_REQ:
	case WLAN_HDD_GO_NEG_RESP:
		return 2 * ACTION_FRAME_DEFAULT_WAIT;
	default:
		return ACTION_FRAME_DEFAULT_WAIT;
	}
}

static int extend_roc_timer(hdd_remain_on_chan_ctx_t *rem_chan_ctx,
			    uint16_t extend_time)
{
	QDF_STATUS status;

	if (!rem_chan_ctx) {
		hdd_err("Extend ROC timer failed");
		return -EINVAL;
	}

	if (QDF_TIMER_STATE_RUNNING !=
	    qdf_mc_timer_get_current_state(
		    &rem_chan_ctx->hdd_remain_on_chan_timer)) {
		hdd_debug("Rcvd action frame after timer expired");
		return -EFAULT;
	}

	qdf_mc_timer_stop(&rem_chan_ctx->hdd_remain_on_chan_timer);
	status = qdf_mc_timer_start(&rem_chan_ctx->hdd_remain_on_chan_timer,
				    extend_time);
	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("Remain on Channel timer start failed");
		return -EFAULT;
	}

	return 0;
}

static void buffer_p2p_action_frame(hdd_remain_on_chan_ctx_t *rem_chan_ctx,
				    uint8_t *pb_frames, uint32_t frm_len,
				    uint16_t freq)
{
	if (!rem_chan_ctx) {
		hdd_err("Buffer P2P action frame failed");
		return;
	}

	if (rem_chan_ctx->action_pkt_buff.frame_length != 0) {
		hdd_warn("Frames are pending. dropping frame !!!");
		return;
	}

	rem_chan_ctx->action_pkt_buff.frame_length = frm_len;
	rem_chan_ctx->action_pkt_buff.freq = freq;
	rem_chan_ctx->action_pkt_buff.frame_ptr =
		qdf_mem_malloc(frm_len);
	qdf_mem_copy(rem_chan_ctx->action_pkt_buff.frame_ptr, pb_frames,
		     frm_len);
	hdd_debug("Action Pkt Cached successfully !!!");
}

static inline bool is_rx_action_resp_while_ack_pending(
	enum p2p_action_frame_state frame_state,
	enum action_frm_type frm_type)
{
	return (frm_type == WLAN_HDD_PROV_DIS_RESP &&
		frame_state == HDD_PD_REQ_ACK_PENDING) ||
		(frm_type == WLAN_HDD_GO_NEG_RESP &&
		 frame_state == HDD_GO_NEG_REQ_ACK_PENDING);
}

static bool process_rx_p2p_action_frame(hdd_adapter_t *adapter,
					uint8_t *pb_frames,
					hdd_cfg80211_state_t *cfg_state,
					uint32_t frm_len, uint16_t freq)
{
	uint8_t *macFrom;
	enum action_frm_type frm_type;
	uint16_t extend_time;
	hdd_remain_on_chan_ctx_t *rem_chan_ctx = NULL;

	macFrom = &pb_frames[WLAN_HDD_80211_PEER_ADDR_OFFSET];
	frm_type = pb_frames[WLAN_HDD_PUBLIC_ACTION_FRAME_TYPE_OFFSET];

	hdd_debug("Rx Action Frame %u", frm_type);
	wlan_hdd_p2p_action_debug(frm_type, macFrom);

	mutex_lock(&cfg_state->remain_on_chan_ctx_lock);
	rem_chan_ctx = cfg_state->remain_on_chan_ctx;
	if (rem_chan_ctx) {
		if (is_extend_roc_timer_for_action(frm_type)) {
			extend_time = get_roc_extension_time(frm_type);
			hdd_debug("Extend RoC timer on reception of Action Frame %d",
				  extend_time);
			if (completion_done(
				&adapter->rem_on_chan_ready_event)) {
				extend_roc_timer(rem_chan_ctx, extend_time);
			} else {
				hdd_err("Buffer packet");
				buffer_p2p_action_frame(rem_chan_ctx,
							pb_frames,
							frm_len, freq);
				mutex_unlock(&cfg_state->
					     remain_on_chan_ctx_lock);
				return false;
			}
		}
	}
	mutex_unlock(&cfg_state->remain_on_chan_ctx_lock);

	if (is_rx_action_resp_while_ack_pending(cfg_state->actionFrmState,
						frm_type)) {
		hdd_debug("ACK_PENDING and But received RESP for Action frame ");
		cfg_state->is_go_neg_ack_received = 1;
		hdd_send_action_cnf(adapter, true);
	}

	return true;
}

static void log_rx_tdls_action_frame(uint8_t *pb_frames, uint32_t frame_len)
{
	enum action_frm_type frm_type;

	if (frame_len <= WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET + 1) {
		hdd_debug("Not a TDLS action frame len: %d", frame_len);
		return;
	}

	frm_type = pb_frames[WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET + 1];
	if (frm_type >= MAX_TDLS_ACTION_FRAME_TYPE) {
		hdd_debug("[TDLS] unknown[%d] <--- OTA",
			  frm_type);
	} else {
		hdd_debug("[TDLS] %s <--- OTA",
			  tdls_action_frame_type[frm_type]);
	}

	cds_tdls_tx_rx_mgmt_event(SIR_MAC_ACTION_TDLS, SIR_MAC_ACTION_RX,
				  SIR_MAC_MGMT_ACTION, frm_type,
				  &pb_frames[WLAN_HDD_80211_PEER_ADDR_OFFSET]);
}

#ifdef FEATURE_WLAN_TDLS
static void process_tdls_rx_action_frame(hdd_adapter_t *adapter,
					 uint8_t *peer_addr, int8_t rx_rssi)
{
	process_rx_tdls_disc_resp_frame(adapter, peer_addr, rx_rssi);
}
#else
static void process_tdls_rx_action_frame(hdd_adapter_t *adapter,
					 uint8_t *peer_addr, int8_t rx_rssi)
{
}
#endif

static bool process_rx_public_action_frame(hdd_adapter_t *adapter,
					   uint8_t *pb_frames,
					   hdd_cfg80211_state_t *cfg_state,
					   enum action_frm_type frm_type,
					   uint32_t frm_len, uint16_t freq,
					   int8_t rx_rssi)
{
	if (!adapter || !pb_frames || !cfg_state) {
		hdd_err("Invalid parameter");
		return false;
	}

	if (!is_public_action_frame(pb_frames, frm_len))
		goto done;

	if (is_p2p_action_frame(pb_frames, frm_len)) {
		hdd_debug("Process P2P action frame");
		return process_rx_p2p_action_frame(adapter, pb_frames,
						   cfg_state, frm_len, freq);
	}

	if (is_tdls_disc_resp_frame(pb_frames, frm_len)) {
		uint8_t *peer_addr;

		peer_addr = &pb_frames[WLAN_HDD_80211_PEER_ADDR_OFFSET];
		process_tdls_rx_action_frame(adapter, peer_addr, rx_rssi);
	}

done:
	return true;
}

static uint16_t get_rx_frame_freq_from_chan(uint32_t rx_chan)
{
	if (rx_chan <= MAX_NO_OF_2_4_CHANNELS)
		return ieee80211_channel_to_frequency(rx_chan,
						      HDD_NL80211_BAND_2GHZ);

	return ieee80211_channel_to_frequency(rx_chan,
						HDD_NL80211_BAND_5GHZ);
}

static void indicate_rx_mgmt_over_nl80211(hdd_adapter_t *adapter,
					  uint32_t frm_len,
					  uint8_t *pb_frames, uint16_t freq,
					  int8_t rx_rssi)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
	cfg80211_rx_mgmt(adapter->dev->ieee80211_ptr,
			 freq, rx_rssi * 100, pb_frames,
			 frm_len, NL80211_RXMGMT_FLAG_ANSWERED);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0))
	cfg80211_rx_mgmt(adapter->dev->ieee80211_ptr,
			 freq, rx_rssi * 100, pb_frames,
			 frm_len, NL80211_RXMGMT_FLAG_ANSWERED,
			 GFP_ATOMIC);
#else
	cfg80211_rx_mgmt(adapter->dev->ieee80211_ptr, freq,
			 rx_rssi * 100,
			 pb_frames, frm_len, GFP_ATOMIC);
#endif /* LINUX_VERSION_CODE */
}

void __hdd_indicate_mgmt_frame(hdd_adapter_t *adapter, uint32_t frm_len,
			       uint8_t *pb_frames, uint8_t frame_type,
			       uint32_t rx_chan, int8_t rx_rssi)
{
	uint16_t freq;
	uint8_t type = 0;
	uint8_t sub_type = 0;
	enum action_frm_type frm_type;
	hdd_cfg80211_state_t *cfg_state;
	hdd_context_t *hdd_ctx;
	uint8_t broadcast = 0;
	uint8_t *dest_addr;

	hdd_debug("Frame Type = %d Frame Length = %d",
		  frame_type, frm_len);

	if (NULL == adapter) {
		hdd_err("adapter is NULL");
		return;
	}
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	if (0 == frm_len) {
		hdd_err("Frame Length is Invalid ZERO");
		return;
	}

	if (NULL == pb_frames) {
		hdd_err("pb_frames is NULL");
		return;
	}

	type = WLAN_HDD_GET_TYPE_FRM_FC(pb_frames[0]);
	sub_type = WLAN_HDD_GET_SUBTYPE_FRM_FC(pb_frames[0]);
	hdd_debug("Frame Type = %d subType = %d", type, sub_type);

	if (is_non_probe_req_mgmt_frame(type, sub_type) &&
	    !qdf_is_macaddr_broadcast(
	     (struct qdf_mac_addr *)&pb_frames[WLAN_HDD_80211_FRM_DA_OFFSET])) {
		if (frm_len <= WLAN_HDD_80211_FRM_DA_OFFSET +
		    QDF_MAC_ADDR_SIZE) {
			hdd_err("Cannot parse dest mac addr len: %d", frm_len);
			return;
		}
		dest_addr = &pb_frames[WLAN_HDD_80211_FRM_DA_OFFSET];

		if (qdf_is_macaddr_broadcast((struct qdf_mac_addr *)
					     dest_addr)) {
			hdd_debug("Check if rx mgmt OCB frames");
			adapter = get_adapter_from_broadcast_ocb(hdd_ctx,
								 &broadcast);
		} else {
			adapter = get_adapter_from_mac_addr(hdd_ctx,
							    dest_addr);
		}
		if (!adapter) {
			hdd_err("Cannot get adapter for RX mgmt frame");
			return;
		}
	}

	if (!adapter->dev) {
		hdd_err("adapter->dev is NULL");
		return;
	}

	if (WLAN_HDD_ADAPTER_MAGIC != adapter->magic) {
		hdd_err("adapter has invalid magic");
		return;
	}

	freq = get_rx_frame_freq_from_chan(rx_chan);

	cfg_state = WLAN_HDD_GET_CFG_STATE_PTR(adapter);

	if (!is_mgmt_non_broadcast_frame(type, sub_type, broadcast))
		goto indicate;

	if (is_public_action_frame(pb_frames, frm_len)) {
		bool processed;

		processed = process_rx_public_action_frame(adapter, pb_frames,
							   cfg_state, frm_type,
							   frm_len, freq,
							   rx_rssi);
		if (!processed) {
			hdd_err("Unable to process rx public action frame");
			return;
		}
	}

	if (is_tdls_action_frame(pb_frames, frm_len))
		log_rx_tdls_action_frame(pb_frames, frm_len);

	if (is_qos_action_frame(pb_frames, frm_len))
		sme_update_dsc_pto_up_mapping(hdd_ctx->hHal,
					      adapter->hddWmmDscpToUpMap,
					      adapter->sessionId);

indicate:
	hdd_debug("Indicate Frame over NL80211 sessionid : %d, idx :%d",
		  adapter->sessionId, adapter->dev->ifindex);

	indicate_rx_mgmt_over_nl80211(adapter, frm_len, pb_frames,
				      freq, rx_rssi);
}

