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
 * DOC: wlan_hdd_apf.c
 *
 * Android Packet Filter support and implementation
 */

#include "wlan_hdd_apf.h"
#include "qca_vendor.h"
#include "wlan_hdd_request_manager.h"

/*
 * define short names for the global vendor params
 * used by __wlan_hdd_cfg80211_apf_offload()
 */
#define APF_INVALID \
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_INVALID
#define APF_SUBCMD \
	QCA_WLAN_VENDOR_ATTR_SET_RESET_PACKET_FILTER
#define APF_VERSION \
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_VERSION
#define APF_FILTER_ID \
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_ID
#define APF_PACKET_SIZE \
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_SIZE
#define APF_CURRENT_OFFSET \
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_CURRENT_OFFSET
#define APF_PROGRAM \
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_PROGRAM
#define APF_PROG_LEN \
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_PROG_LENGTH
#define APF_MAX \
	QCA_WLAN_VENDOR_ATTR_PACKET_FILTER_MAX

static const struct nla_policy
wlan_hdd_apf_offload_policy[APF_MAX + 1] = {
	[APF_SUBCMD] = {.type = NLA_U32},
	[APF_VERSION] = {.type = NLA_U32},
	[APF_FILTER_ID] = {.type = NLA_U32},
	[APF_PACKET_SIZE] = {.type = NLA_U32},
	[APF_CURRENT_OFFSET] = {.type = NLA_U32},
	[APF_PROGRAM] = {.type = NLA_BINARY,
			 .len = MAX_APF_MEMORY_LEN},
	[APF_PROG_LEN] = {.type = NLA_U32},
};

void hdd_apf_context_init(hdd_adapter_t *adapter)
{
	qdf_event_create(&adapter->apf_context.qdf_apf_event);
	qdf_spinlock_create(&adapter->apf_context.lock);
	adapter->apf_context.apf_enabled = true;
}

void hdd_apf_context_destroy(hdd_adapter_t *adapter)
{
	qdf_event_destroy(&adapter->apf_context.qdf_apf_event);
	qdf_spinlock_destroy(&adapter->apf_context.lock);
	qdf_mem_zero(&adapter->apf_context,
		     sizeof(struct hdd_apf_context));
}

void hdd_get_apf_capabilities_cb(void *cookie,
				 struct sir_apf_get_offload *data)
{
	struct sir_apf_get_offload *apf_caps_resp;
	struct hdd_request *request;

	ENTER();

	if (!data) {
		hdd_err("Event response is NULL!");
		return;
	}

	request = hdd_request_get(cookie);
	if (!request) {
		hdd_debug("Obsolete request");
		return;
	}

	apf_caps_resp = hdd_request_priv(request);
	*apf_caps_resp = *data;
	hdd_request_complete(request);
	hdd_request_put(request);

	EXIT();
}

/**
 * hdd_post_get_apf_capabilities_rsp() - Callback function to APF Offload
 * @hdd_context: hdd_context
 * @apf_get_offload: struct for get offload
 *
 * Return: 0 on success, error number otherwise.
 */
static int
hdd_post_get_apf_capabilities_rsp(hdd_context_t *hdd_ctx,
				  struct sir_apf_get_offload *apf_get_offload)
{
	struct sk_buff *skb;
	uint32_t nl_buf_len;

	ENTER();

	nl_buf_len = NLMSG_HDRLEN;
	nl_buf_len +=
		(sizeof(apf_get_offload->max_bytes_for_apf_inst) + NLA_HDRLEN) +
		(sizeof(apf_get_offload->apf_version) + NLA_HDRLEN);

	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy, nl_buf_len);
	if (!skb) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		return -ENOMEM;
	}

	hdd_ctx->apf_version = apf_get_offload->apf_version;
	hdd_debug("APF Version: %u APF max bytes: %u",
			apf_get_offload->apf_version,
			apf_get_offload->max_bytes_for_apf_inst);

	if (nla_put_u32(skb, APF_PACKET_SIZE,
			apf_get_offload->max_bytes_for_apf_inst) ||
	    nla_put_u32(skb, APF_VERSION, apf_get_offload->apf_version)) {
		hdd_err("nla put failure");
		goto nla_put_failure;
	}

	cfg80211_vendor_cmd_reply(skb);
	EXIT();
	return 0;

nla_put_failure:
	kfree_skb(skb);
	return -EINVAL;
}

/**
 * hdd_get_apf_capabilities - Get APF Capabilities
 * @hdd_ctx: Hdd context
 *
 * Return: 0 on success, errno on failure
 */
static int hdd_get_apf_capabilities(hdd_context_t *hdd_ctx)
{
	static struct sir_apf_get_offload *apf_caps_resp;
	struct hdd_request *request;
	QDF_STATUS status;
	void *cookie;
	int ret;
	static const struct hdd_request_params params = {
		.priv_size = sizeof(*apf_caps_resp),
		.timeout_ms = WLAN_WAIT_TIME_APF_GET_CAPS,
	};

	ENTER();

	request = hdd_request_alloc(&params);
	if (!request) {
		hdd_err("Request allocation failure");
		return -ENOMEM;
	}

	cookie = hdd_request_cookie(request);

	status = sme_get_apf_capabilities(hdd_ctx->hHal, cookie);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Unable to retrieve APF caps");
		ret = -EINVAL;
		goto cleanup;
	}

	ret = hdd_request_wait_for_response(request);
	if (ret) {
		hdd_err("Target response timed out");
		goto cleanup;
	}

	apf_caps_resp = hdd_request_priv(request);
	ret = hdd_post_get_apf_capabilities_rsp(hdd_ctx, apf_caps_resp);
	if (ret)
		hdd_err("Failed to post get apf capabilities");

cleanup:
	hdd_request_put(request);
	EXIT();

	return ret;
}

/**
 * hdd_set_reset_apf_offload - Post set/reset apf to SME
 * @hdd_ctx: Hdd context
 * @tb: Length of @data
 * @adapter: pointer to adapter struct
 *
 * Return: 0 on success; errno on failure
 */
static int hdd_set_reset_apf_offload(hdd_context_t *hdd_ctx,
				     struct nlattr **tb,
				     hdd_adapter_t *adapter)
{
	struct sir_apf_set_offload *apf_set_offload;
	QDF_STATUS status;
	int prog_len;
	int ret = 0;
	bool apf_enabled = false;

	ENTER();

	if (!hdd_conn_is_connected(
	    WLAN_HDD_GET_STATION_CTX_PTR(adapter))) {
		hdd_err("Not in Connected state!");
		return -ENOTSUPP;
	}

	apf_set_offload = qdf_mem_malloc(sizeof(*apf_set_offload));
	if (apf_set_offload == NULL) {
		hdd_err("qdf_mem_malloc failed for apf_set_offload");
		return -ENOMEM;
	}

	/* Parse and fetch apf packet size */
	if (!tb[APF_PACKET_SIZE]) {
		hdd_err("attr apf packet size failed");
		ret = -EINVAL;
		goto fail;
	}
	apf_set_offload->total_length = nla_get_u32(tb[APF_PACKET_SIZE]);

	if (!apf_set_offload->total_length) {
		hdd_debug("APF reset packet filter received");
		apf_enabled = false;
		goto post_sme;
	}

	/* Parse and fetch apf program */
	if (!tb[APF_PROGRAM]) {
		hdd_err("attr apf program failed");
		ret = -EINVAL;
		goto fail;
	}

	prog_len = nla_len(tb[APF_PROGRAM]);
	apf_set_offload->program = qdf_mem_malloc(sizeof(uint8_t) * prog_len);

	if (apf_set_offload->program == NULL) {
		hdd_err("qdf_mem_malloc failed for apf offload program");
		ret = -ENOMEM;
		goto fail;
	}

	apf_set_offload->current_length = prog_len;
	nla_memcpy(apf_set_offload->program, tb[APF_PROGRAM], prog_len);
	apf_set_offload->session_id = adapter->sessionId;

	hdd_debug("APF set instructions");
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
			   apf_set_offload->program, prog_len);

	/* Parse and fetch filter Id */
	if (!tb[APF_FILTER_ID]) {
		hdd_err("attr filter id failed");
		ret = -EINVAL;
		goto fail;
	}
	apf_set_offload->filter_id = nla_get_u32(tb[APF_FILTER_ID]);

	/* Parse and fetch current offset */
	if (!tb[APF_CURRENT_OFFSET]) {
		hdd_err("attr current offset failed");
		ret = -EINVAL;
		goto fail;
	}
	apf_set_offload->current_offset = nla_get_u32(tb[APF_CURRENT_OFFSET]);
	apf_enabled = true;

post_sme:
	hdd_debug("Posting APF SET/RESET to SME, session_id: %d APF Version: %d filter ID: %d total_length: %d current_length: %d current offset: %d",
			apf_set_offload->session_id,
			apf_set_offload->version,
			apf_set_offload->filter_id,
			apf_set_offload->total_length,
			apf_set_offload->current_length,
			apf_set_offload->current_offset);

	status = sme_set_apf_instructions(hdd_ctx->hHal, apf_set_offload);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("sme_set_apf_instructions failed(err=%d)", status);
		ret = -EINVAL;
		goto fail;
	}
	EXIT();

fail:
	if (apf_set_offload->current_length)
		qdf_mem_free(apf_set_offload->program);
	qdf_mem_free(apf_set_offload);

	if (!ret)
		hdd_ctx->apf_enabled_v2 = true;

	return ret;
}

/**
 * hdd_enable_disable_apf - Enable or Disable the APF interpreter
 * @adapter: HDD Adapter
 * @apf_enable: true: Enable APF Int., false: disable APF Int.
 *
 * Return: 0 on success, errno on failure
 */
static int
hdd_enable_disable_apf(hdd_adapter_t *adapter, bool apf_enable)
{
	QDF_STATUS status;

	ENTER();

	status = sme_set_apf_enable_disable(WLAN_HDD_GET_HAL_CTX(adapter),
					    adapter->sessionId, apf_enable);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Unable to post sme apf enable/disable message (status-%d)",
				status);
		return -EINVAL;
	}

	adapter->apf_context.apf_enabled = apf_enable;

	EXIT();
	return 0;
}

/**
 * hdd_apf_write_memory - Write into the apf work memory
 * @hdd_ctx: Hdd context
 * @tb: list of attributes
 * @session_id: Session id
 *
 * This function writes code/data into the APF work memory and
 * provides program length that is passed on to the interpreter.
 *
 * Return: 0 on success, errno on failure
 */
static int
hdd_apf_write_memory(hdd_adapter_t *adapter, struct nlattr **tb)
{
	struct wmi_apf_write_memory_params write_mem_params = {0};
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	QDF_STATUS status;
	int ret = 0;

	ENTER();

	write_mem_params.vdev_id = adapter->sessionId;

	if (adapter->apf_context.apf_enabled) {
		hdd_err("Cannot get/set when APF interpreter is enabled");
		return -EINVAL;
	}

	/* Read program length */
	if (!tb[APF_PROG_LEN]) {
		hdd_err("attr program length failed");
		return -EINVAL;
	}
	write_mem_params.program_len = nla_get_u32(tb[APF_PROG_LEN]);

	/* Read APF work memory offset */
	if (!tb[APF_CURRENT_OFFSET]) {
		hdd_err("attr apf packet size failed");
		return -EINVAL;
	}
	write_mem_params.addr_offset = nla_get_u32(tb[APF_CURRENT_OFFSET]);

	/* Parse and fetch apf program */
	if (!tb[APF_PROGRAM]) {
		hdd_err("attr apf program failed");
		return -EINVAL;
	}

	write_mem_params.length = nla_len(tb[APF_PROGRAM]);
	if (!write_mem_params.length) {
		hdd_err("Program attr with empty data");
		return -EINVAL;
	}

	write_mem_params.buf = qdf_mem_malloc(sizeof(uint8_t)
						* write_mem_params.length);
	if (write_mem_params.buf == NULL) {
		hdd_err("failed to alloc mem for apf write mem operation");
		return -EINVAL;
	}
	nla_memcpy(write_mem_params.buf, tb[APF_PROGRAM],
		   write_mem_params.length);

	write_mem_params.apf_version = hdd_ctx->apf_version;

	status = sme_apf_write_work_memory(WLAN_HDD_GET_HAL_CTX(adapter),
					   &write_mem_params);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Unable to retrieve APF caps");
		ret = -EINVAL;
	}

	if (write_mem_params.buf)
		qdf_mem_free(write_mem_params.buf);

	EXIT();
	return ret;
}

void
hdd_apf_read_memory_callback(void *hdd_context,
			     struct wmi_apf_read_memory_resp_event_params
								*read_mem_evt)
{
	hdd_context_t *hdd_ctx = hdd_context;
	hdd_adapter_t *adapter;
	struct hdd_apf_context *context;
	uint8_t *buf_ptr;
	uint32_t pkt_offset;
	ENTER();

	if (wlan_hdd_validate_context(hdd_ctx) || !read_mem_evt) {
		hdd_err("HDD context is invalid or event buf(%pK) is null",
			read_mem_evt);
		return;
	}

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, read_mem_evt->vdev_id);
	if (hdd_validate_adapter(adapter)) {
		hdd_err("Adapter is invalid");
		return;
	}
	context = &adapter->apf_context;

	if (context->magic != APF_CONTEXT_MAGIC) {
		hdd_err("Caller timed out or corrupt magic, simply return");
		return;
	}

	if (read_mem_evt->offset <  context->offset) {
		hdd_err("Offset in read event(%d) smaller than offset in request(%d)!",
					read_mem_evt->offset, context->offset);
		return;
	}

	/*
	 * offset in the event is relative to the APF work memory.
	 * Calculate the packet offset, which gives us the relative
	 * location in the buffer to start copy into.
	 */
	pkt_offset = read_mem_evt->offset - context->offset;

	if (context->buf_len < pkt_offset + read_mem_evt->length) {
		hdd_err("Read chunk exceeding allocated space");
		return;
	}
	buf_ptr = context->buf + pkt_offset;

	qdf_mem_copy(buf_ptr, read_mem_evt->data, read_mem_evt->length);

	if (!read_mem_evt->more_data) {
		/* Release the caller after last event, clear magic */
		context->magic = 0;
		qdf_event_set(&context->qdf_apf_event);
	}

	EXIT();
}

/**
 * hdd_apf_read_memory - Read part of the apf work memory
 * @adapter: HDD Adapter
 * @tb: list of attributes
 *
 * Return: 0 on success, errno on failure
 */
static int hdd_apf_read_memory(hdd_adapter_t *adapter, struct nlattr **tb)
{
	struct wmi_apf_read_memory_params read_mem_params = {0};
	struct hdd_apf_context *context = &adapter->apf_context;
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	QDF_STATUS status;
	unsigned long nl_buf_len = NLMSG_HDRLEN;
	int ret = 0;
	struct sk_buff *skb = NULL;
	uint8_t *bufptr;

	ENTER();

	read_mem_params.vdev_id = adapter->sessionId;

	/* Read APF work memory offset */
	if (!tb[APF_CURRENT_OFFSET]) {
		hdd_err("attr apf memory offset failed");
		return -EINVAL;
	}
	read_mem_params.addr_offset = nla_get_u32(tb[APF_CURRENT_OFFSET]);

	/* Read length */
	if (!tb[APF_PACKET_SIZE]) {
		hdd_err("attr apf packet size failed");
		return -EINVAL;
	}
	read_mem_params.length = nla_get_u32(tb[APF_PACKET_SIZE]);
	if (!read_mem_params.length) {
		hdd_err("apf read length cannot be zero!");
		return -EINVAL;
	}
	bufptr = qdf_mem_malloc(read_mem_params.length);
	if (bufptr == NULL) {
		hdd_err("alloc failed for cumulative event buffer");
		return -ENOMEM;
	}

	if (context->apf_enabled) {
		hdd_err("Cannot get/set while interpreter is enabled");
		return -EINVAL;
	}

	qdf_event_reset(&context->qdf_apf_event);
	context->offset = read_mem_params.addr_offset;

	context->buf = bufptr;
	context->buf_len = read_mem_params.length;
	context->magic = APF_CONTEXT_MAGIC;

	status = sme_apf_read_work_memory(WLAN_HDD_GET_HAL_CTX(adapter),
					  &read_mem_params);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Unable to post sme APF read memory message (status-%d)",
				status);
		ret = -EINVAL;
		goto fail;
	}

	/* request was sent -- wait for the response */
	status = qdf_wait_for_event_completion(&context->qdf_apf_event,
					       WLAN_WAIT_TIME_APF_READ_MEM);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Target response timed out");
		context->magic = 0;
		ret = -ETIMEDOUT;
		goto fail;
	}

	nl_buf_len += sizeof(uint32_t) + NLA_HDRLEN;
	nl_buf_len += context->buf_len + NLA_HDRLEN;

	skb = cfg80211_vendor_cmd_alloc_reply_skb(hdd_ctx->wiphy, nl_buf_len);
	if (!skb) {
		hdd_err("cfg80211_vendor_cmd_alloc_reply_skb failed");
		ret = -ENOMEM;
		goto fail;
	}

	if (nla_put_u32(skb, APF_SUBCMD, QCA_WLAN_READ_PACKET_FILTER) ||
	    nla_put(skb, APF_PROGRAM, read_mem_params.length, context->buf)) {
		hdd_err("put fail");
		kfree_skb(skb);
		ret = -EINVAL;
		goto fail;
	}

	cfg80211_vendor_cmd_reply(skb);
fail:
	if (context->buf) {
		qdf_mem_free(context->buf);
		context->buf = NULL;
	}

	EXIT();
	return ret;
}


/**
 * wlan_hdd_cfg80211_apf_offload() - Set/Reset to APF Offload
 * @wiphy:    wiphy structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of @data
 *
 * Return: 0 on success; errno on failure
 */
static int
__wlan_hdd_cfg80211_apf_offload(struct wiphy *wiphy,
				struct wireless_dev *wdev,
				const void *data, int data_len)
{
	hdd_context_t *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	hdd_adapter_t *adapter =  WLAN_HDD_GET_PRIV_PTR(dev);
	struct nlattr *tb[APF_MAX + 1];
	int ret_val = 0, apf_subcmd;
	struct hdd_apf_context *context = &adapter->apf_context;

	ENTER();

	ret_val = wlan_hdd_validate_context(hdd_ctx);
	if (ret_val)
		return ret_val;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	if (!hdd_ctx->apf_supported) {
		hdd_err("APF is not supported or disabled through INI");
		return -ENOTSUPP;
	}

	if (!(adapter->device_mode == QDF_STA_MODE ||
	      adapter->device_mode == QDF_P2P_CLIENT_MODE)) {
			hdd_err("APF only supported in STA or P2P CLI modes!");
			return -ENOTSUPP;
	}

	if (hdd_nla_parse(tb, APF_MAX, data, data_len,
			  wlan_hdd_apf_offload_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[APF_SUBCMD]) {
		hdd_err("attr apf sub-command failed");
		return -EINVAL;
	}
	apf_subcmd = nla_get_u32(tb[APF_SUBCMD]);

	/* Do not allow simultaneous new APF commands on the same adapter */
	qdf_spin_lock(&context->lock);
	if (context->cmd_in_progress) {
		qdf_spin_unlock(&context->lock);
		hdd_err("Another cmd in progress for same session!");
		return -EAGAIN;
	}
	context->cmd_in_progress = true;
	qdf_spin_unlock(&context->lock);

	switch (apf_subcmd) {
	/* Legacy APF sub-commands */
	case QCA_WLAN_SET_PACKET_FILTER:
		ret_val = hdd_set_reset_apf_offload(hdd_ctx, tb,
						    adapter);
		break;
	case QCA_WLAN_GET_PACKET_FILTER:
		ret_val = hdd_get_apf_capabilities(hdd_ctx);
		break;

	/* APF 3.0 sub-commands */
	case QCA_WLAN_WRITE_PACKET_FILTER:
		ret_val = hdd_apf_write_memory(adapter, tb);
		break;
	case QCA_WLAN_READ_PACKET_FILTER:
		ret_val = hdd_apf_read_memory(adapter, tb);
		break;
	case QCA_WLAN_ENABLE_PACKET_FILTER:
		ret_val = hdd_enable_disable_apf(adapter, true);
		break;
	case QCA_WLAN_DISABLE_PACKET_FILTER:
		ret_val = hdd_enable_disable_apf(adapter, false);
		break;
	default:
		hdd_err("Unknown APF Sub-command: %d", apf_subcmd);
		ret_val = -ENOTSUPP;
	}

	qdf_spin_lock(&context->lock);
	context->cmd_in_progress = false;
	qdf_spin_unlock(&context->lock);

	return ret_val;
}

/**
 * wlan_hdd_cfg80211_apf_offload() - SSR Wrapper to APF Offload
 * @wiphy:    wiphy structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of @data
 *
 * Return: 0 on success; errno on failure
 */

int wlan_hdd_cfg80211_apf_offload(struct wiphy *wiphy,
				  struct wireless_dev *wdev,
				  const void *data, int data_len)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_apf_offload(wiphy, wdev, data, data_len);
	cds_ssr_unprotect(__func__);

	return ret;
}
