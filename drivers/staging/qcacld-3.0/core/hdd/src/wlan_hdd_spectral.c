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
 * DOC: wlan_hdd_spectral_scan.c
 *
 * WLAN Host Device Driver Spectral Scan Implementation
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <net/cfg80211.h>
#include <ani_global.h>
#include "wlan_hdd_main.h"
#include "wlan_hdd_spectralscan.h"
#include "spectral_scan_api.h"
#include "wlan_nlink_srv.h"
#include "spectral_scan_fmt.h"
#ifdef CNSS_GENL
#include <net/cnss_nl.h>
#endif

static const struct nla_policy spectral_scan_policy[
		QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_PERIOD] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PRIORITY] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_SIZE] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_GC_ENA] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RESTART_ENA] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NOISE_FLOOR_REF] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_INIT_DELAY] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NB_TONE_THR] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_STR_BIN_THR] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_WB_RPT_MODE] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_RPT_MODE] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_THR] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PWR_FORMAT] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RPT_MODE] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BIN_SCALE] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DBM_ADJ] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_CHN_MASK] = {
							.type = NLA_U32},
	[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COOKIE] = {.type = NLA_U32},
};

/**
 * __wlan_hdd_cfg80211_spectral_scan_start() - start spectral scan
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function starts spectral scan
 *
 * Return: 0 on success and errno on failure
 */
static int __wlan_hdd_cfg80211_spectral_scan_start(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int ret;
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX + 1];
	struct vdev_spectral_configure_params config_req;
	QDF_STATUS status;
	struct vdev_spectral_enable_params ss_req;
	uint64_t cookie;
	struct sk_buff *skb;

	ENTER_DEV(dev);

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}
	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	if (wlan_hdd_validate_session_id(adapter->sessionId)) {
		hdd_err("invalid session id: %d", adapter->sessionId);
		return -EINVAL;
	}
	/* initialize config parameters*/
	config_req = hdd_ctx->ss_config;

	if (hdd_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_MAX,
			  data, data_len, spectral_scan_policy)) {
		hdd_err("Invalid Spectral Scan config ATTR");
		return -EINVAL;
	}
	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT])
		config_req.count = nla_get_u32(tb
			[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_COUNT]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_PERIOD])
		config_req.period = nla_get_u32(tb
		[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_SCAN_PERIOD]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PRIORITY])
		config_req.spectral_pri = nla_get_u32(tb
			[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PRIORITY]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_SIZE])
		config_req.fft_size = nla_get_u32(tb
			[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_FFT_SIZE]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_GC_ENA])
		config_req.gc_enable = nla_get_u32(tb
			[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_GC_ENA]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RESTART_ENA])
		config_req.restart_enable = nla_get_u32(tb
		   [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RESTART_ENA]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NOISE_FLOOR_REF])
		config_req.noise_floor_ref = nla_get_u32(tb
		   [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NOISE_FLOOR_REF]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_INIT_DELAY])
		config_req.init_delay = nla_get_u32(tb
		   [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_INIT_DELAY]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NB_TONE_THR])
		config_req.nb_tone_thr = nla_get_u32(tb
		   [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_NB_TONE_THR]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_STR_BIN_THR])
		config_req.str_bin_thr = nla_get_u32(tb
		   [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_STR_BIN_THR]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_WB_RPT_MODE])
		config_req.wb_rpt_mode = nla_get_u32(tb
		   [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_WB_RPT_MODE]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_RPT_MODE])
		config_req.rssi_rpt_mode = nla_get_u32(tb
		   [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_RPT_MODE]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_THR])
		config_req.rssi_thr = nla_get_u32(tb
		   [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RSSI_THR]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PWR_FORMAT])
		config_req.pwr_format = nla_get_u32(tb
		   [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_PWR_FORMAT]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RPT_MODE])
		config_req.rpt_mode = nla_get_u32(tb
		   [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_RPT_MODE]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BIN_SCALE])
		config_req.bin_scale = nla_get_u32(tb
		   [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_BIN_SCALE]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DBM_ADJ])
		config_req.dBm_adj = nla_get_u32(tb
		   [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_DBM_ADJ]);

	if (tb[QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_CHN_MASK])
		config_req.chn_mask = nla_get_u32(tb
		   [QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_CONFIG_CHN_MASK]);

	/* update hdd_contxt spectral scan config */
	hdd_ctx->ss_config = config_req;

	config_req.vdev_id = adapter->sessionId;

	status = sme_spectral_scan_config(&config_req);
	if (QDF_STATUS_SUCCESS != status)
		return -EINVAL;

	ss_req.vdev_id = adapter->sessionId;
	ss_req.active = 1;
	ss_req.active_valid = 1;
	ss_req.enabled = 1;
	ss_req.enabled_valid = 1;

	status = sme_start_spectral_scan(&ss_req);
	if (QDF_STATUS_SUCCESS != status)
		return -EINVAL;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(wiphy, sizeof(u64) +
		NLA_HDRLEN + NLMSG_HDRLEN);
	if (!skb) {
		hdd_err(" reply skb alloc failed");
		return -ENOMEM;
	}

	cookie = 0;
	if (hdd_wlan_nla_put_u64(skb, QCA_WLAN_VENDOR_ATTR_SPECTRAL_SCAN_COOKIE,
					cookie)) {
		kfree_skb(skb);
		return -EINVAL;
	}

	cfg80211_vendor_cmd_reply(skb);

	return 0;
}

/**
 * wlan_hdd_cfg80211_spectral_scan_start() - start spectral scan
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function starts spectral scan
 *
 * Return: 0 on success and errno on failure
 */
int wlan_hdd_cfg80211_spectral_scan_start(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_spectral_scan_start(
				wiphy, wdev, data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_spectral_scan_stop() - stop spectral scan
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function stops spectral scan
 *
 * Return: 0 on success and errno on failure
 */
static int __wlan_hdd_cfg80211_spectral_scan_stop(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	struct vdev_spectral_enable_params ss_req;
	QDF_STATUS status;
	int ret_val;
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	ret_val = wlan_hdd_validate_context(hdd_ctx);
	if (ret_val)
		return ret_val;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	ss_req.vdev_id = adapter->sessionId;
	ss_req.active = 0;
	ss_req.active_valid = 1;
	ss_req.enabled = 0;
	ss_req.enabled_valid = 1;

	status = sme_start_spectral_scan(&ss_req);
	if (QDF_STATUS_SUCCESS != status)
		return -EINVAL;

	return 0;
}

/**
 * wlan_hdd_cfg80211_spectral_scan_stop() - stop spectral scan
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function stops spectral scan
 *
 * Return: 0 on success and errno on failure
 */
int wlan_hdd_cfg80211_spectral_scan_stop(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_spectral_scan_stop(
				wiphy, wdev, data, data_len);

	cds_ssr_unprotect(__func__);

	return ret;
}

static void send_spectral_scan_reg_rsp_msg(hdd_context_t *hdd_ctx)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	struct spectral_scan_msg *rsp_msg;
	int err;

	skb = alloc_skb(NLMSG_SPACE(sizeof(struct spectral_scan_msg)),
				GFP_KERNEL);
	if (skb == NULL)
		return;

	nlh = (struct nlmsghdr *)skb->data;
	nlh->nlmsg_pid = 0;
	nlh->nlmsg_flags = 0;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_type = WLAN_NL_MSG_SPECTRAL_SCAN;

	rsp_msg = NLMSG_DATA(nlh);
	rsp_msg->msg_type = SPECTRAL_SCAN_REGISTER_RSP;
	rsp_msg->pid = hdd_ctx->sscan_pid;

	nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct spectral_scan_msg));
	skb_put(skb, NLMSG_SPACE(sizeof(struct spectral_scan_msg)));

	hdd_info("sending App Reg Response to process pid %d",
			hdd_ctx->sscan_pid);

#ifdef CNSS_GENL
	err = nl_srv_ucast(skb, hdd_ctx->sscan_pid, MSG_DONTWAIT,
			WLAN_NL_MSG_SPECTRAL_SCAN, CLD80211_MCGRP_OEM_MSGS);
#else
	err = nl_srv_ucast(skb, hdd_ctx->sscan_pid, MSG_DONTWAIT);
#endif
	if (err < 0)
		hdd_err("SPECTRAL: failed to send to spectral scan reg"
			" response");
}

#ifdef CNSS_GENL
/**
 * __spectral_scan_msg_handler() - API to handle spectral scan
 * command
 * @data: Data received
 * @data_len: length of the data received
 * @ctx: Pointer to stored context
 * @pid: Process ID
 *
 * API to handle spectral scan commands from user space
 *
 * Return: None
 */
static void __spectral_scan_msg_handler(const void *data, int data_len,
					void *ctx, int pid)
{
	struct spectral_scan_msg *ss_msg = NULL;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_MAX + 1];
	hdd_context_t *hdd_ctx;
	int ret;

	hdd_ctx = (hdd_context_t *)cds_get_context(QDF_MODULE_ID_HDD);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return;

	if (hdd_nla_parse(tb, CLD80211_ATTR_MAX, data, data_len, NULL)) {
		hdd_err("nla parse fails");
		return;
	}

	if (!tb[CLD80211_ATTR_DATA]) {
		hdd_err("attr VENDOR_DATA fails");
		return;
	}
	ss_msg = (struct spectral_scan_msg *)nla_data(tb[CLD80211_ATTR_DATA]);

	if (!ss_msg) {
		hdd_err("data NULL");
		return;
	}

	switch (ss_msg->msg_type) {
	case SPECTRAL_SCAN_REGISTER_REQ:
		hdd_ctx->sscan_pid = ss_msg->pid;
		hdd_debug("spectral scan application registered, pid=%d",
				 hdd_ctx->sscan_pid);
		send_spectral_scan_reg_rsp_msg(hdd_ctx);
		break;
	default:
		hdd_warn("invalid message type %d", ss_msg->msg_type);
		break;
	}
}

static void spectral_scan_msg_handler(const void *data, int data_len,
					void *ctx, int pid)
{
	cds_ssr_protect(__func__);
	__spectral_scan_msg_handler(data, data_len, ctx, pid);
	cds_ssr_unprotect(__func__);
}

int spectral_scan_activate_service(void)
{
	register_cld_cmd_cb(WLAN_NL_MSG_SPECTRAL_SCAN,
				spectral_scan_msg_handler, NULL);
	return 0;
}

int spectral_scan_deactivate_service(void)
{
	deregister_cld_cmd_cb(WLAN_NL_MSG_SPECTRAL_SCAN);
	return 0;
}

#else
static int spectral_scan_msg_callback(struct sk_buff *skb)
{
	struct nlmsghdr *nlh;
	uint8_t *msg;
	struct spectral_scan_msg *ss_msg;
	hdd_context_t *hdd_ctx;

	nlh = (struct nlmsghdr *)skb->data;
	if (!nlh) {
		hdd_err("Netlink header null");
		return -EINVAL;
	}

	msg = NLMSG_DATA(nlh);

	ss_msg = (struct spectral_scan_msg *)msg;
	switch (ss_msg->msg_type) {
	case SPECTRAL_SCAN_REGISTER_REQ:
		hdd_ctx = (hdd_context_t *)cds_get_context(QDF_MODULE_ID_HDD);
		if (hdd_ctx != NULL) {
			hdd_ctx->sscan_pid = nlh->nlmsg_pid;
			hdd_info("spectral scan application registered, pid=%d",
				 hdd_ctx->sscan_pid);
			send_spectral_scan_reg_rsp_msg(hdd_ctx);
		} else {
			hdd_err("failed to get hdd context");
		}
		break;
	default:
		hdd_info("invalid message type %d", ss_msg->msg_type);
		break;
	}

	return 0;
}

int spectral_scan_activate_service(void)
{
	int ret;

	/* Register the msg handler for msgs addressed to
	 * WLAN_NL_MSG_SPECTRAL_SCAN
	 */
	ret = nl_srv_register(WLAN_NL_MSG_SPECTRAL_SCAN,
				spectral_scan_msg_callback);
	if (ret)
		hdd_err("Spectral Scan Registration failed");

	return ret;
}

int spectral_scan_deactivate_service(void)
{
	int ret;

	/*
	 * Unregister the msg handler for msgs addressed to
	 * WLAN_NL_MSG_SPECTRAL_SCAN
	 */
	ret = nl_srv_unregister(WLAN_NL_MSG_SPECTRAL_SCAN,
				spectral_scan_msg_callback);
	if (ret)
		hdd_err("Spectral Scan Unregistration failed");

	return ret;
}

#endif

/**
 * hdd_init_spectral_scan() - Initialize spectral scan config parameters
 *
 * This function initialize spectral scan configuration parameters
 * with default values.
 * @hdd_ctx: HDD context pointer
 *
 * Return - None
 */
void hdd_init_spectral_scan(hdd_context_t *hdd_ctx)
{
	struct vdev_spectral_configure_params *params;

	params = &(hdd_ctx->ss_config);

	params->vdev_id = 0;
	params->count = SPECTRAL_SCAN_COUNT_DEFAULT;
	params->period = SPECTRAL_SCAN_PERIOD_DEFAULT;
	params->spectral_pri = SPECTRAL_SCAN_PRIORITY_DEFAULT;
	params->fft_size = SPECTRAL_SCAN_FFT_SIZE_DEFAULT;
	params->gc_enable = SPECTRAL_SCAN_GC_ENA_DEFAULT;
	params->restart_enable = SPECTRAL_SCAN_RESTART_ENA_DEFAULT;
	params->noise_floor_ref = SPECTRAL_SCAN_NOISE_FLOOR_REF_DEFAULT;
	params->init_delay = SPECTRAL_SCAN_INIT_DELAY_DEFAULT;
	params->nb_tone_thr = SPECTRAL_SCAN_NB_TONE_THR_DEFAULT;
	params->str_bin_thr = SPECTRAL_SCAN_STR_BIN_THR_DEFAULT;
	params->wb_rpt_mode = SPECTRAL_SCAN_WB_RPT_MODE_DEFAULT;
	params->rssi_rpt_mode = SPECTRAL_SCAN_RSSI_RPT_MODE_DEFAULT;
	params->rssi_thr = SPECTRAL_SCAN_RSSI_THR_DEFAULT;
	params->pwr_format = SPECTRAL_SCAN_PWR_FORMAT_DEFAULT;
	params->rpt_mode = SPECTRAL_SCAN_RPT_MODE_DEFAULT;
	params->bin_scale = SPECTRAL_SCAN_BIN_SCALE_DEFAULT;
	params->dBm_adj = SPECTRAL_SCAN_DBM_ADJ_DEFAULT;
	params->chn_mask = SPECTRAL_SCAN_CHN_MASK_DEFAULT;
}

/**
 * spectral_scan_callback() - send spectral scan SAMP message to user space
 * @samp_msg: spectral scan results in SAMP message format
 *
 * Return - None
 */
void spectral_scan_callback(void *context, struct spectral_samp_msg   *samp_msg)
{
	struct sk_buff *spectral_skb;
	struct nlmsghdr *nlh;
	int err;
	struct spectral_samp_msg_info *samp_msg_info;
	hdd_context_t *hdd_ctx = (hdd_context_t *)context;
	int i;

	spectral_skb = alloc_skb(NLMSG_SPACE(MAX_SPECTRAL_PAYLOAD), GFP_KERNEL);
	if (spectral_skb == NULL)
		return;

	nlh = (struct nlmsghdr *)spectral_skb->data;
	nlh->nlmsg_pid = 0;
	nlh->nlmsg_flags = 0;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_type = WLAN_NL_MSG_SPECTRAL_SCAN;

	nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct spectral_samp_msg_info));
	skb_put(spectral_skb, NLMSG_SPACE(sizeof(
					struct spectral_samp_msg_info)));

	samp_msg_info = NLMSG_DATA(nlh);
	samp_msg_info->signature          = samp_msg->signature;
	samp_msg_info->freq               = samp_msg->freq;
	samp_msg_info->vhtop_ch_freq_seg1 = samp_msg->vhtop_ch_freq_seg1;
	samp_msg_info->vhtop_ch_freq_seg2 = samp_msg->vhtop_ch_freq_seg2;
	samp_msg_info->freq_loading       = samp_msg->freq_loading;
	samp_msg_info->dcs_enabled        = samp_msg->dcs_enabled;
	samp_msg_info->int_type           = samp_msg->int_type;
	for (i = 0; i < ETH_ALEN; i++)
		samp_msg_info->macaddr[i] = samp_msg->macaddr[i];
	samp_msg_info->samp_data.spectral_data_len =
				samp_msg->samp_data.spectral_data_len;
	samp_msg_info->samp_data.spectral_data_len_sec80 =
				samp_msg->samp_data.spectral_data_len_sec80;
	samp_msg_info->samp_data.spectral_rssi =
				samp_msg->samp_data.spectral_rssi;
	samp_msg_info->samp_data.spectral_rssi_sec80 =
				samp_msg->samp_data.spectral_rssi_sec80;
	samp_msg_info->samp_data.spectral_combined_rssi =
				samp_msg->samp_data.spectral_combined_rssi;
	samp_msg_info->samp_data.spectral_upper_rssi =
				samp_msg->samp_data.spectral_upper_rssi;
	samp_msg_info->samp_data.spectral_lower_rssi =
				samp_msg->samp_data.spectral_lower_rssi;
	for (i = 0; i < MAX_SPECTRAL_CHAINS; i++) {
		samp_msg_info->samp_data.spectral_chain_ctl_rssi[i] =
			samp_msg->samp_data.spectral_chain_ctl_rssi[i];
		samp_msg_info->samp_data.spectral_chain_ext_rssi[i] =
			samp_msg->samp_data.spectral_chain_ext_rssi[i];
	}
	samp_msg_info->samp_data.spectral_max_scale =
				samp_msg->samp_data.spectral_max_scale;
	samp_msg_info->samp_data.spectral_bwinfo =
				samp_msg->samp_data.spectral_bwinfo;
	samp_msg_info->samp_data.spectral_tstamp =
				samp_msg->samp_data.spectral_tstamp;
	samp_msg_info->samp_data.spectral_max_index =
				samp_msg->samp_data.spectral_max_index;
	samp_msg_info->samp_data.spectral_max_index_sec80 =
				samp_msg->samp_data.spectral_max_index_sec80;
	samp_msg_info->samp_data.spectral_max_mag =
				samp_msg->samp_data.spectral_max_mag;
	samp_msg_info->samp_data.spectral_max_mag_sec80 =
				samp_msg->samp_data.spectral_max_mag_sec80;
	samp_msg_info->samp_data.spectral_max_exp =
				samp_msg->samp_data.spectral_max_exp;
	samp_msg_info->samp_data.spectral_last_tstamp =
				samp_msg->samp_data.spectral_last_tstamp;
	samp_msg_info->samp_data.spectral_upper_max_index =
				samp_msg->samp_data.spectral_upper_max_index;
	samp_msg_info->samp_data.spectral_lower_max_index =
				samp_msg->samp_data.spectral_lower_max_index;
	samp_msg_info->samp_data.spectral_nb_upper =
				samp_msg->samp_data.spectral_nb_upper;
	samp_msg_info->samp_data.spectral_nb_lower =
				samp_msg->samp_data.spectral_nb_lower;
	samp_msg_info->samp_data.bin_pwr_count =
				samp_msg->samp_data.bin_pwr_count;
	samp_msg_info->samp_data.lb_edge_extrabins =
				samp_msg->samp_data.lb_edge_extrabins;
	samp_msg_info->samp_data.rb_edge_extrabins =
				samp_msg->samp_data.rb_edge_extrabins;
	samp_msg_info->samp_data.bin_pwr_count_sec80 =
				samp_msg->samp_data.bin_pwr_count_sec80;
	for (i = 0; i < MAX_NUM_BINS; i++) {
		samp_msg_info->samp_data.bin_pwr[i] =
				samp_msg->samp_data.bin_pwr[i];
		samp_msg_info->samp_data.bin_pwr_sec80[i] =
				samp_msg->samp_data.bin_pwr_sec80[i];
	}
	samp_msg_info->samp_data.interf_list.count =
				samp_msg->samp_data.interf_list.count;
	for (i = 0; i < MAX_INTERF; i++) {
		samp_msg_info->samp_data.interf_list.interf[i].interf_type =
			samp_msg->samp_data.interf_list.interf[i].interf_type;
		samp_msg_info->samp_data.interf_list.interf[i].interf_min_freq =
			samp_msg->samp_data.interf_list.interf[i].
							interf_min_freq;
		samp_msg_info->samp_data.interf_list.interf[i].interf_max_freq =
			samp_msg->samp_data.interf_list.interf[i].
							interf_max_freq;
	}
	samp_msg_info->samp_data.noise_floor =
				samp_msg->samp_data.noise_floor;
	samp_msg_info->samp_data.noise_floor_sec80 =
				samp_msg->samp_data.noise_floor_sec80;
	samp_msg_info->samp_data.ch_width =
				samp_msg->samp_data.ch_width;

#ifdef CNSS_GENL
	err = nl_srv_ucast(spectral_skb, hdd_ctx->sscan_pid, MSG_DONTWAIT,
			WLAN_NL_MSG_SPECTRAL_SCAN, CLD80211_MCGRP_OEM_MSGS);
#else
	err = nl_srv_ucast(spectral_skb, hdd_ctx->sscan_pid, MSG_DONTWAIT);
#endif
	if (err < 0)
		hdd_err("SPECTRAL : failed to send to spectral scan app");
}

/**
 * hdd_register_spectral_scan_cb() - register callback function for sending
 *      spectral scan results to user space.
 *
 * @hdd_ctx: HDD context pointer
 * @cb: callback function
 *
 * Return - None
 */
void hdd_register_spectral_scan_cb(hdd_context_t *hdd_ctx,
				void (*cb)(void *, struct spectral_samp_msg *))
{
	QDF_STATUS status;

	status = sme_spectral_scan_register_callback(hdd_ctx->hHal, cb);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Failed to register spectral scan callback");
}
