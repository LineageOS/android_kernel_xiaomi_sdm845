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
 * DOC:  wlan_hdd_hostapd.c
 *
 * WLAN Host Device Driver implementation
 */

/* Include Files */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/wireless.h>
#include <linux/semaphore.h>
#include <linux/compat.h>
#include <cdp_txrx_stats.h>
#include <cds_api.h>
#include <cds_sched.h>
#include <linux/etherdevice.h>
#include <wlan_hdd_includes.h>
#include <qc_sap_ioctl.h>
#include <wlan_hdd_hostapd.h>
#include <sap_api.h>
#include <sap_internal.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <wlan_hdd_main.h>
#include <wlan_hdd_ioctl.h>
#include <wlan_hdd_stats.h>
#include <linux/netdevice.h>
#include <linux/rtnetlink.h>
#include <linux/mmc/sdio_func.h>
#include "wlan_hdd_p2p.h"
#include <wlan_hdd_ipa.h>
#include "cfg_api.h"
#include "wni_cfg.h"
#include "wlan_hdd_misc.h"
#include <cds_utils.h>
#include "pld_common.h"
#include "wlan_hdd_regulatory.h"

#include "wma.h"
#ifdef WLAN_DEBUG
#include "wma_api.h"
#endif
#include "wlan_hdd_trace.h"
#include "qdf_types.h"
#include "qdf_trace.h"
#include "wlan_hdd_cfg.h"
#include "cds_concurrency.h"
#include "wlan_hdd_tsf.h"
#include "wlan_hdd_green_ap.h"
#include "ol_rx_fwd.h"

#define    IS_UP(_dev) \
	(((_dev)->flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))
#define    IS_UP_AUTO(_ic) \
	(IS_UP((_ic)->ic_dev) && (_ic)->ic_roaming == IEEE80211_ROAMING_AUTO)
#define WE_WLAN_VERSION     1
#define WE_GET_STA_INFO_SIZE 30
/* WEXT limitation: MAX allowed buf len for any *
 * IW_PRIV_TYPE_CHAR is 2Kbytes *
 */
#define WE_SAP_MAX_STA_INFO 0x7FF

#define RC_2_RATE_IDX(_rc)        ((_rc) & 0x7)
#define HT_RC_2_STREAMS(_rc)    ((((_rc) & 0x78) >> 3) + 1)
#define RC_2_RATE_IDX_11AC(_rc)        ((_rc) & 0xf)
#define HT_RC_2_STREAMS_11AC(_rc)    ((((_rc) & 0x30) >> 4) + 1)

#define SAP_24GHZ_CH_COUNT (14)
#define ACS_SCAN_EXPIRY_TIMEOUT_S 4

/*
 * 11B, 11G Rate table include Basic rate and Extended rate
 * The IDX field is the rate index
 * The HI field is the rate when RSSI is strong or being ignored
 * (in this case we report actual rate)
 * The MID field is the rate when RSSI is moderate
 * (in this case we cap 11b rates at 5.5 and 11g rates at 24)
 * The LO field is the rate when RSSI is low
 * (in this case we don't report rates, actual current rate used)
 */
static const struct index_data_rate_type supported_data_rate[] = {
	/* IDX     HI  HM  LM LO (RSSI-based index */
	{2,   { 10,  10, 10, 0} },
	{4,   { 20,  20, 10, 0} },
	{11,  { 55,  20, 10, 0} },
	{12,  { 60,  55, 20, 0} },
	{18,  { 90,  55, 20, 0} },
	{22,  {110,  55, 20, 0} },
	{24,  {120,  90, 60, 0} },
	{36,  {180, 120, 60, 0} },
	{44,  {220, 180, 60, 0} },
	{48,  {240, 180, 90, 0} },
	{66,  {330, 180, 90, 0} },
	{72,  {360, 240, 90, 0} },
	{96,  {480, 240, 120, 0} },
	{108, {540, 240, 120, 0} }
};

/* MCS Based rate table */
/* HT MCS parameters with Nss = 1 */
static const struct index_data_rate_type supported_mcs_rate_nss1[] = {
	/* MCS  L20   L40   S20  S40 */
	{0,  { 65,  135,  72,  150} },
	{1,  { 130, 270,  144, 300} },
	{2,  { 195, 405,  217, 450} },
	{3,  { 260, 540,  289, 600} },
	{4,  { 390, 810,  433, 900} },
	{5,  { 520, 1080, 578, 1200} },
	{6,  { 585, 1215, 650, 1350} },
	{7,  { 650, 1350, 722, 1500} }
};

/* HT MCS parameters with Nss = 2 */
static const struct index_data_rate_type supported_mcs_rate_nss2[] = {
	/* MCS  L20    L40   S20   S40 */
	{0,  {130,  270,  144,  300} },
	{1,  {260,  540,  289,  600} },
	{2,  {390,  810,  433,  900} },
	{3,  {520,  1080, 578,  1200} },
	{4,  {780,  1620, 867,  1800} },
	{5,  {1040, 2160, 1156, 2400} },
	{6,  {1170, 2430, 1300, 2700} },
	{7,  {1300, 2700, 1444, 3000} }
};

/* MCS Based VHT rate table */
/* MCS parameters with Nss = 1*/
static const struct index_vht_data_rate_type supported_vht_mcs_rate_nss1[] = {
	/* MCS  L80    S80     L40   S40    L20   S40*/
	{0,  {293,  325},  {135,  150},  {65,   72} },
	{1,  {585,  650},  {270,  300},  {130,  144} },
	{2,  {878,  975},  {405,  450},  {195,  217} },
	{3,  {1170, 1300}, {540,  600},  {260,  289} },
	{4,  {1755, 1950}, {810,  900},  {390,  433} },
	{5,  {2340, 2600}, {1080, 1200}, {520,  578} },
	{6,  {2633, 2925}, {1215, 1350}, {585,  650} },
	{7,  {2925, 3250}, {1350, 1500}, {650,  722} },
	{8,  {3510, 3900}, {1620, 1800}, {780,  867} },
	{9,  {3900, 4333}, {1800, 2000}, {780,  867} }
};

/*MCS parameters with Nss = 2*/
static const struct index_vht_data_rate_type supported_vht_mcs_rate_nss2[] = {
	/* MCS  L80    S80     L40   S40    L20   S40*/
	{0,  {585,  650},  {270,  300},  {130,  144} },
	{1,  {1170, 1300}, {540,  600},  {260,  289} },
	{2,  {1755, 1950}, {810,  900},  {390,  433} },
	{3,  {2340, 2600}, {1080, 1200}, {520,  578} },
	{4,  {3510, 3900}, {1620, 1800}, {780,  867} },
	{5,  {4680, 5200}, {2160, 2400}, {1040, 1156} },
	{6,  {5265, 5850}, {2430, 2700}, {1170, 1300} },
	{7,  {5850, 6500}, {2700, 3000}, {1300, 1444} },
	{8,  {7020, 7800}, {3240, 3600}, {1560, 1733} },
	{9,  {7800, 8667}, {3600, 4000}, {1560, 1733} }
};

/* Function definitions */

/**
 * hdd_sap_context_init() - Initialize SAP context.
 * @hdd_ctx:	HDD context.
 *
 * Initialize SAP context.
 *
 * Return: 0 on success.
 */
int hdd_sap_context_init(hdd_context_t *hdd_ctx)
{
	qdf_wake_lock_create(&hdd_ctx->sap_dfs_wakelock, "sap_dfs_wakelock");
	atomic_set(&hdd_ctx->sap_dfs_ref_cnt, 0);

	mutex_init(&hdd_ctx->sap_lock);
	qdf_wake_lock_create(&hdd_ctx->sap_wake_lock, "qcom_sap_wakelock");
	qdf_spinlock_create(&hdd_ctx->sap_update_info_lock);

	qdf_atomic_init(&hdd_ctx->dfs_radar_found);

	return 0;
}

/**
 * hdd_hostapd_channel_allow_suspend() - allow suspend in a channel.
 * Called when, 1. bss stopped, 2. channel switch
 *
 * @pAdapter: pointer to hdd adapter
 * @channel: current channel
 *
 * Return: None
 */
static void hdd_hostapd_channel_allow_suspend(hdd_adapter_t *pAdapter,
					      uint8_t channel)
{

	hdd_context_t *pHddCtx = (hdd_context_t *) (pAdapter->pHddCtx);
	hdd_hostapd_state_t *pHostapdState =
		WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);

	hdd_debug("bssState: %d, channel: %d, dfs_ref_cnt: %d",
	       pHostapdState->bssState, channel,
	       atomic_read(&pHddCtx->sap_dfs_ref_cnt));

	/* Return if BSS is already stopped */
	if (pHostapdState->bssState == BSS_STOP)
		return;

	if (CHANNEL_STATE_DFS != cds_get_channel_state(channel))
		return;

	/* Release wakelock when no more DFS channels are used */
	if (atomic_dec_and_test(&pHddCtx->sap_dfs_ref_cnt)) {
		hdd_err("DFS: allowing suspend (chan: %d)", channel);
		qdf_wake_lock_release(&pHddCtx->sap_dfs_wakelock,
				      WIFI_POWER_EVENT_WAKELOCK_DFS);
		qdf_runtime_pm_allow_suspend(&pHddCtx->runtime_context.dfs);

	}
}

/**
 * hdd_hostapd_channel_prevent_suspend() - prevent suspend in a channel.
 * Called when, 1. bss started, 2. channel switch
 *
 * @pAdapter: pointer to hdd adapter
 * @channel: current channel
 *
 * Return - None
 */
static void hdd_hostapd_channel_prevent_suspend(hdd_adapter_t *pAdapter,
						uint8_t channel)
{
	hdd_context_t *pHddCtx = (hdd_context_t *) (pAdapter->pHddCtx);
	hdd_hostapd_state_t *pHostapdState =
		WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);

	hdd_debug("bssState: %d, channel: %d, dfs_ref_cnt: %d",
	       pHostapdState->bssState, channel,
	       atomic_read(&pHddCtx->sap_dfs_ref_cnt));

	/* Return if BSS is already started && wakelock is acquired */
	if ((pHostapdState->bssState == BSS_START) &&
		(atomic_read(&pHddCtx->sap_dfs_ref_cnt) >= 1))
		return;

	if (CHANNEL_STATE_DFS != cds_get_channel_state(channel))
		return;

	/* Acquire wakelock if we have at least one DFS channel in use */
	if (atomic_inc_return(&pHddCtx->sap_dfs_ref_cnt) == 1) {
		hdd_err("DFS: preventing suspend (chan: %d)", channel);
		qdf_runtime_pm_prevent_suspend(&pHddCtx->runtime_context.dfs);
		qdf_wake_lock_acquire(&pHddCtx->sap_dfs_wakelock,
				      WIFI_POWER_EVENT_WAKELOCK_DFS);
	}
}

/**
 * hdd_sap_context_destroy() - Destroy SAP context
 *
 * @hdd_ctx:	HDD context.
 *
 * Destroy SAP context.
 *
 * Return: None
 */
void hdd_sap_context_destroy(hdd_context_t *hdd_ctx)
{
	if (atomic_read(&hdd_ctx->sap_dfs_ref_cnt)) {
		qdf_wake_lock_release(&hdd_ctx->sap_dfs_wakelock,
				      WIFI_POWER_EVENT_WAKELOCK_DRIVER_EXIT);

		atomic_set(&hdd_ctx->sap_dfs_ref_cnt, 0);
		hdd_debug("DFS: Allowing suspend");
	}

	qdf_wake_lock_destroy(&hdd_ctx->sap_dfs_wakelock);

	mutex_destroy(&hdd_ctx->sap_lock);
	qdf_wake_lock_destroy(&hdd_ctx->sap_wake_lock);

	qdf_spinlock_destroy(&hdd_ctx->sap_update_info_lock);

}

/**
 * __hdd_hostapd_open() - hdd open function for hostapd interface
 * This is called in response to ifconfig up
 * @dev: pointer to net_device structure
 *
 * Return - 0 for success non-zero for failure
 */
static int __hdd_hostapd_open(struct net_device *dev)
{
	hdd_adapter_t *pAdapter = netdev_priv(dev);
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(pAdapter);
	int ret;

	ENTER_DEV(dev);

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_HOSTAPD_OPEN_REQUEST, NO_SESSION, 0));

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;
	/*
	 * Check statemachine state and also stop iface change timer if running
	 */
	ret = hdd_wlan_start_modules(hdd_ctx, pAdapter, false);
	if (ret) {
		hdd_err("Failed to start WLAN modules return");
		return ret;
	}

	if (!test_bit(SME_SESSION_OPENED, &pAdapter->event_flags)) {
		ret = hdd_start_adapter(pAdapter);
		if (ret) {
			hdd_err("Failed to start adapter :%d",
				pAdapter->device_mode);
			return ret;
		}
	}

	set_bit(DEVICE_IFACE_OPENED, &pAdapter->event_flags);
	/* Enable all Tx queues */
	hdd_info("Enabling queues");
	wlan_hdd_netif_queue_control(pAdapter,
				   WLAN_START_ALL_NETIF_QUEUE_N_CARRIER,
				   WLAN_CONTROL_PATH);
	EXIT();
	return 0;
}

/**
 * hdd_hostapd_open() - SSR wrapper for __hdd_hostapd_open
 * @dev: pointer to net device
 *
 * Return: 0 on success, error number otherwise
 */
static int hdd_hostapd_open(struct net_device *dev)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __hdd_hostapd_open(dev);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __hdd_hostapd_stop() - hdd stop function for hostapd interface
 * This is called in response to ifconfig down
 *
 * @dev: pointer to net_device structure
 *
 * Return - 0 for success non-zero for failure
 */
static int __hdd_hostapd_stop(struct net_device *dev)
{
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ptSapContext sap_ctx = adapter->sessionCtx.ap.sapContext;
	int ret;

	ENTER_DEV(dev);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret) {
		set_bit(DOWN_DURING_SSR, &adapter->event_flags);
		return ret;
	}

	if (!sap_ctx) {
		hdd_err("invalid sap ctx: %pK", sap_ctx);
		return -ENODEV;
	}

	hdd_stop_adapter(hdd_ctx, adapter, true);

	clear_bit(DEVICE_IFACE_OPENED, &adapter->event_flags);

	if (!hdd_is_cli_iface_up(hdd_ctx))
		sme_scan_flush_result(hdd_ctx->hHal);

	/* Stop all tx queues */
	hdd_info("Disabling queues");
	wlan_hdd_netif_queue_control(adapter,
				     WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
				     WLAN_CONTROL_PATH);

	EXIT();
	return 0;
}

/**
 * hdd_hostapd_stop() - SSR wrapper for__hdd_hostapd_stop
 * @dev: pointer to net_device
 *
 * This is called in response to ifconfig down
 *
 * Return: 0 on success, error number otherwise
 */
int hdd_hostapd_stop(struct net_device *dev)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __hdd_hostapd_stop(dev);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __hdd_hostapd_uninit() - hdd uninit function
 * This is called during the netdev unregister to uninitialize all data
 * associated with the device.
 *
 * @dev: pointer to net_device structure
 *
 * Return: None
 */
static void __hdd_hostapd_uninit(struct net_device *dev)
{
	hdd_adapter_t *adapter = netdev_priv(dev);
	hdd_context_t *hdd_ctx;

	ENTER_DEV(dev);

	if (WLAN_HDD_ADAPTER_MAGIC != adapter->magic) {
		hdd_err("Invalid magic");
		return;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (NULL == hdd_ctx) {
		hdd_err("NULL hdd_ctx");
		return;
	}

	hdd_deinit_adapter(hdd_ctx, adapter, true);

	/* after uninit our adapter structure will no longer be valid */
	adapter->dev = NULL;
	adapter->magic = 0;

	EXIT();
}

/**
 * hdd_hostapd_uninit() - SSR wrapper for __hdd_hostapd_uninit
 * @dev: pointer to net_device
 *
 * Return: 0 on success, error number otherwise
 */
static void hdd_hostapd_uninit(struct net_device *dev)
{
	cds_ssr_protect(__func__);
	__hdd_hostapd_uninit(dev);
	cds_ssr_unprotect(__func__);
}

/**
 * __hdd_hostapd_change_mtu() - change mtu
 * @dev: pointer to net_device
 * @new_mtu: new mtu
 *
 * Return: 0 on success, error number otherwise
 */
static int __hdd_hostapd_change_mtu(struct net_device *dev, int new_mtu)
{
	ENTER_DEV(dev);

	return 0;
}

/**
 * hdd_hostapd_change_mtu() - SSR wrapper for __hdd_hostapd_change_mtu
 * @dev: pointer to net_device
 * @new_mtu: new mtu
 *
 * Return: 0 on success, error number otherwise
 */
static int hdd_hostapd_change_mtu(struct net_device *dev, int new_mtu)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __hdd_hostapd_change_mtu(dev, new_mtu);
	cds_ssr_unprotect(__func__);

	return ret;
}

#ifdef QCA_HT_2040_COEX
QDF_STATUS hdd_set_sap_ht2040_mode(hdd_adapter_t *pHostapdAdapter,
				   uint8_t channel_type)
{
	QDF_STATUS qdf_ret_status = QDF_STATUS_E_FAILURE;
	void *hHal = NULL;

	hdd_debug("change HT20/40 mode");

	if (QDF_SAP_MODE == pHostapdAdapter->device_mode) {
		hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
		if (NULL == hHal) {
			hdd_err("Hal ctx is null");
			return QDF_STATUS_E_FAULT;
		}
		qdf_ret_status =
			sme_set_ht2040_mode(hHal, pHostapdAdapter->sessionId,
					channel_type, true);
		if (qdf_ret_status == QDF_STATUS_E_FAILURE) {
			hdd_err("Failed to change HT20/40 mode");
			return QDF_STATUS_E_FAILURE;
		}
	}
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * __hdd_hostapd_set_mac_address() -
 * This function sets the user specified mac address using
 * the command ifconfig wlanX hw ether <mac address>.
 *
 * @dev: pointer to the net device.
 * @addr: pointer to the sockaddr.
 *
 * Return: 0 for success, non zero for failure
 */
static int __hdd_hostapd_set_mac_address(struct net_device *dev, void *addr)
{
	struct sockaddr *psta_mac_addr = addr;
	hdd_adapter_t *adapter, *adapter_temp;
	hdd_context_t *hdd_ctx;
	int ret = 0;
	struct qdf_mac_addr mac_addr;

	ENTER_DEV(dev);

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	qdf_mem_copy(&mac_addr, psta_mac_addr->sa_data, QDF_MAC_ADDR_SIZE);

	adapter_temp = hdd_get_adapter_by_macaddr(hdd_ctx, mac_addr.bytes);
	if (adapter_temp) {
		if (!qdf_str_cmp(adapter_temp->dev->name, dev->name))
			return 0;
		hdd_err("%s adapter exist with same address " MAC_ADDRESS_STR,
			adapter_temp->dev->name,
			MAC_ADDR_ARRAY(mac_addr.bytes));
		return -EINVAL;
	}

	if (qdf_is_macaddr_zero(&mac_addr)) {
		hdd_err("MAC is all zero");
		return -EINVAL;
	}

	if (qdf_is_macaddr_broadcast(&mac_addr)) {
		hdd_err("MAC is Broadcast");
		return -EINVAL;
	}

	if (ETHER_IS_MULTICAST(psta_mac_addr->sa_data)) {
		hdd_err("MAC is Multicast");
		return -EINVAL;
	}

	hdd_info("Changing MAC to " MAC_ADDRESS_STR " of interface %s ",
		 MAC_ADDR_ARRAY(mac_addr.bytes),
		 dev->name);
	memcpy(dev->dev_addr, psta_mac_addr->sa_data, ETH_ALEN);
	EXIT();
	return 0;
}

/**
 * hdd_hostapd_set_mac_address() - set mac address
 * @dev: pointer to net_device
 * @addr: mac address
 *
 * Return: 0 on success, error number otherwise
 */
static int hdd_hostapd_set_mac_address(struct net_device *dev, void *addr)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __hdd_hostapd_set_mac_address(dev, addr);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * hdd_hostapd_inactivity_timer_cb() - Inactivity timeout handler
 * @context: Context registered with qdf_mc_timer_init()
 *
 * This is the callback function registered with qdf_mc_timer_init()
 * to handle the AP inactivity timer. The @context registered is the
 * struct net_device associated with the interface.  When this
 * function is called it means the AP inactivity timer has fired, and
 * this function in turn indicates the timeout to userspace.
 */

static void hdd_hostapd_inactivity_timer_cb(void *context)
{
	struct net_device *dev = (struct net_device *)context;
	uint8_t we_custom_event[64];
	union iwreq_data wrqu;
#ifdef DISABLE_CONCURRENCY_AUTOSAVE
	QDF_STATUS qdf_status;
	hdd_adapter_t *pHostapdAdapter;
	hdd_ap_ctx_t *pHddApCtx;
#endif /* DISABLE_CONCURRENCY_AUTOSAVE */

	/* event_name space-delimiter driver_module_name
	 * Format of the event is "AUTO-SHUT.indication" " " "module_name"
	 */
	char *autoShutEvent = "AUTO-SHUT.indication" " " KBUILD_MODNAME;

	/* For the NULL at the end */
	int event_len = strlen(autoShutEvent) + 1;

	ENTER_DEV(dev);

#ifdef DISABLE_CONCURRENCY_AUTOSAVE
	if (cds_concurrent_open_sessions_running()) {
		/*
		 * This timer routine is going to be called only when AP
		 * persona is up.
		 * If there are concurrent sessions running we do not want
		 * to shut down the Bss.Instead we run the timer again so
		 * that if Autosave is enabled next time and other session
		   was down only then we bring down AP
		 */
		pHostapdAdapter = netdev_priv(dev);
		if (WLAN_HDD_ADAPTER_MAGIC != pHostapdAdapter->magic) {
			hdd_err("invalid adapter: %pK", pHostapdAdapter);
			return;
		}
		pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
		qdf_status =
			qdf_mc_timer_start(&pHddApCtx->hdd_ap_inactivity_timer,
					(WLAN_HDD_GET_CTX(pHostapdAdapter))->
					config->nAPAutoShutOff * 1000);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			hdd_err("Failed to init AP inactivity timer");

		EXIT();
		return;
	}
#endif /* DISABLE_CONCURRENCY_AUTOSAVE */
	if (event_len > sizeof(we_custom_event)) {
		hdd_err("Event len exceeds event buffer size: %d", event_len);
		return;
	}
	memset(&we_custom_event, '\0', sizeof(we_custom_event));
	memcpy(&we_custom_event, autoShutEvent, event_len);

	memset(&wrqu, 0, sizeof(wrqu));
	wrqu.data.length = event_len;

	hdd_debug("Shutting down AP interface due to inactivity");
	wireless_send_event(dev, IWEVCUSTOM, &wrqu, (char *)we_custom_event);

	EXIT();
}

static void hdd_clear_all_sta(hdd_adapter_t *pHostapdAdapter,
			      void *usrDataForCallback)
{
	uint8_t staId = 0;
	struct net_device *dev;
	struct tagCsrDelStaParams del_sta_params;

	dev = (struct net_device *)usrDataForCallback;

	hdd_debug("Clearing all the STA entry....");
	for (staId = 0; staId < WLAN_MAX_STA_COUNT; staId++) {
		if (pHostapdAdapter->aStaInfo[staId].isUsed &&
		    (staId !=
		     (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uBCStaId)) {
			wlansap_populate_del_sta_params(
				&pHostapdAdapter->aStaInfo[staId].macAddrSTA.
				bytes[0], eSIR_MAC_DEAUTH_LEAVING_BSS_REASON,
				(SIR_MAC_MGMT_DISASSOC >> 4), &del_sta_params);

			/* Disconnect all the stations */
			hdd_softap_sta_disassoc(pHostapdAdapter,
						&del_sta_params);
		}
	}
}

static int hdd_stop_bss_link(hdd_adapter_t *pHostapdAdapter,
			     void *usrDataForCallback)
{
	struct net_device *dev;
	hdd_context_t *pHddCtx = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	dev = (struct net_device *)usrDataForCallback;
	ENTER();

	pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	if (test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags)) {
		status = wlansap_stop_bss(
			WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter));
		if (QDF_IS_STATUS_SUCCESS(status))
			hdd_debug("Deleting SAP/P2P link!!!!!!");

		clear_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags);
		cds_decr_session_set_pcl(pHostapdAdapter->device_mode,
					     pHostapdAdapter->sessionId);
	}
	EXIT();
	return (status == QDF_STATUS_SUCCESS) ? 0 : -EBUSY;
}

/**
 * hdd_issue_stored_joinreq() - This function will trigger stations's
 *                              cached connect request to proceed.
 * @hdd_ctx: pointer to hdd context.
 * @sta_adater: pointer to station adapter.
 *
 * This function will call SME to release station's stored/cached connect
 * request to proceed.
 *
 * Return: none.
 */
static void hdd_issue_stored_joinreq(hdd_adapter_t *sta_adapter,
		hdd_context_t *hdd_ctx)
{
	tHalHandle hal_handle;
	uint32_t roam_id = INVALID_ROAM_ID;

	if (NULL == sta_adapter) {
		hdd_err("Invalid station adapter, ignore issueing join req");
		return;
	}
	hal_handle = WLAN_HDD_GET_HAL_CTX(sta_adapter);

	if (true ==  cds_is_sta_connection_pending()) {
		MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
				TRACE_CODE_HDD_ISSUE_JOIN_REQ,
				sta_adapter->sessionId, 0));
		if (QDF_STATUS_SUCCESS !=
			sme_issue_stored_joinreq(hal_handle,
				&roam_id,
				sta_adapter->sessionId)) {
			/* change back to NotAssociated */
			hdd_conn_set_connection_state(sta_adapter,
				eConnectionState_NotConnected);
		}
		cds_change_sta_conn_pending_status(false);
	}
}

/**
 * hdd_chan_change_notify() - Function to notify hostapd about channel change
 * @hostapd_adapter:	hostapd adapter
 * @dev:		Net device structure
 * @chan_change:	New channel change parameters
 * @legacy_phymode:	is the phymode legacy
 *
 * This function is used to notify hostapd about the channel change
 *
 * Return: Success on intimating userspace
 *
 */
QDF_STATUS hdd_chan_change_notify(hdd_adapter_t *adapter,
		struct net_device *dev,
		struct hdd_chan_change_params chan_change,
		bool legacy_phymode)
{
	struct ieee80211_channel *chan;
	struct cfg80211_chan_def chandef;
	enum nl80211_channel_type channel_type;
	uint32_t freq;
	tHalHandle  hal = WLAN_HDD_GET_HAL_CTX(adapter);

	if (NULL == hal) {
		hdd_err("hal is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	hdd_debug("chan:%d width:%d sec_ch_offset:%d seg0:%d seg1:%d",
		chan_change.chan, chan_change.chan_params.ch_width,
		chan_change.chan_params.sec_ch_offset,
		chan_change.chan_params.center_freq_seg0,
		chan_change.chan_params.center_freq_seg1);

	freq = cds_chan_to_freq(chan_change.chan);

	chan = ieee80211_get_channel(adapter->wdev.wiphy, freq);

	if (!chan) {
		hdd_err("Invalid input frequency for channel conversion");
		return QDF_STATUS_E_FAILURE;
	}
	if (legacy_phymode) {
		channel_type = NL80211_CHAN_NO_HT;
	} else {
		switch (chan_change.chan_params.sec_ch_offset) {
		case PHY_SINGLE_CHANNEL_CENTERED:
			channel_type = NL80211_CHAN_HT20;
			break;
		case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
			channel_type = NL80211_CHAN_HT40MINUS;
			break;
		case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
			channel_type = NL80211_CHAN_HT40PLUS;
			break;
		default:
			channel_type = NL80211_CHAN_NO_HT;
			break;
		}
	}

	cfg80211_chandef_create(&chandef, chan, channel_type);

	/* cfg80211_chandef_create() does update of width and center_freq1
	 * only for NL80211_CHAN_NO_HT, NL80211_CHAN_HT20, NL80211_CHAN_HT40PLUS
	 * and NL80211_CHAN_HT40MINUS.
	 */
	if (chan_change.chan_params.ch_width == CH_WIDTH_80MHZ)
		chandef.width = NL80211_CHAN_WIDTH_80;
	else if (chan_change.chan_params.ch_width == CH_WIDTH_80P80MHZ)
		chandef.width = NL80211_CHAN_WIDTH_80P80;
	else if (chan_change.chan_params.ch_width == CH_WIDTH_160MHZ)
		chandef.width = NL80211_CHAN_WIDTH_160;

	if ((chan_change.chan_params.ch_width == CH_WIDTH_80MHZ) ||
	    (chan_change.chan_params.ch_width == CH_WIDTH_80P80MHZ) ||
	    (chan_change.chan_params.ch_width == CH_WIDTH_160MHZ)) {
		if (chan_change.chan_params.center_freq_seg0)
			chandef.center_freq1 = cds_chan_to_freq(
				chan_change.chan_params.center_freq_seg0);

		if (chan_change.chan_params.center_freq_seg1)
			chandef.center_freq2 = cds_chan_to_freq(
				chan_change.chan_params.center_freq_seg1);
	}

	hdd_debug("notify: chan:%d width:%d freq1:%d freq2:%d",
		chandef.chan->center_freq, chandef.width, chandef.center_freq1,
		chandef.center_freq2);

	cfg80211_ch_switch_notify(dev, &chandef);

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_send_radar_event() - Function to send radar events to user space
 * @hdd_context:	HDD context
 * @event:		Type of radar event
 * @dfs_info:		Structure containing DFS channel and country
 * @wdev:		Wireless device structure
 *
 * This function is used to send radar events such as CAC start, CAC
 * end etc., to userspace
 *
 * Return: Success on sending notifying userspace
 *
 */
static QDF_STATUS hdd_send_radar_event(hdd_context_t *hdd_context,
				       eSapHddEvent event,
				       struct wlan_dfs_info dfs_info,
				       struct wireless_dev *wdev)
{

	struct sk_buff *vendor_event;
	enum qca_nl80211_vendor_subcmds_index index;
	uint32_t freq, ret;
	uint32_t data_size;

	if (!hdd_context) {
		hdd_err("HDD context is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	freq = cds_chan_to_freq(dfs_info.channel);

	switch (event) {
	case eSAP_DFS_CAC_START:
		index =
		    QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_STARTED_INDEX;
		data_size = sizeof(uint32_t);
		break;
	case eSAP_DFS_CAC_END:
		index =
		    QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_CAC_FINISHED_INDEX;
		data_size = sizeof(uint32_t);
		break;
	case eSAP_DFS_RADAR_DETECT:
		index =
		    QCA_NL80211_VENDOR_SUBCMD_DFS_OFFLOAD_RADAR_DETECTED_INDEX;
		data_size = sizeof(uint32_t);
		break;
	default:
		return QDF_STATUS_E_FAILURE;
	}

	vendor_event = cfg80211_vendor_event_alloc(hdd_context->wiphy,
			wdev,
			data_size + NLMSG_HDRLEN,
			index,
			GFP_KERNEL);
	if (!vendor_event) {
		hdd_err("cfg80211_vendor_event_alloc failed for %d", index);
		return QDF_STATUS_E_FAILURE;
	}

	ret = nla_put_u32(vendor_event, NL80211_ATTR_WIPHY_FREQ, freq);

	if (ret) {
		hdd_err("NL80211_ATTR_WIPHY_FREQ put fail");
		kfree_skb(vendor_event);
		return QDF_STATUS_E_FAILURE;
	}

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);
	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_send_conditional_chan_switch_status() - Send conditional channel switch
 * status
 * @hdd_ctx: HDD context
 * @wdev: Wireless device structure
 * @status: Status of conditional channel switch
 * (0: Success, Non-zero: Failure)
 *
 * Sends the status of conditional channel switch to user space. This is named
 * conditional channel switch because the SAP will move to the provided channel
 * after some condition (pre-cac) is met.
 *
 * Return: None
 */
static void hdd_send_conditional_chan_switch_status(hdd_context_t *hdd_ctx,
						struct wireless_dev *wdev,
						bool status)
{
	struct sk_buff *event;

	ENTER_DEV(wdev->netdev);

	if (!hdd_ctx) {
		hdd_err("Invalid HDD context pointer");
		return;
	}

	event = cfg80211_vendor_event_alloc(hdd_ctx->wiphy,
		  wdev, sizeof(uint32_t) + NLMSG_HDRLEN,
		  QCA_NL80211_VENDOR_SUBCMD_SAP_CONDITIONAL_CHAN_SWITCH_INDEX,
		  GFP_KERNEL);
	if (!event) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	if (nla_put_u32(event,
			QCA_WLAN_VENDOR_ATTR_SAP_CONDITIONAL_CHAN_SWITCH_STATUS,
			status)) {
		hdd_err("nla put failed");
		kfree_skb(event);
		return;
	}

	cfg80211_vendor_event(event, GFP_KERNEL);
}

/**
 * wlan_hdd_set_pre_cac_complete_status() - Set pre cac complete status
 * @ap_adapter: AP adapter
 * @status: Status which can be true or false
 *
 * Sets the status of pre cac i.e., whether it is complete or not
 *
 * Return: Zero on success, non-zero on failure
 */
static int wlan_hdd_set_pre_cac_complete_status(hdd_adapter_t *ap_adapter,
		bool status)
{
	QDF_STATUS ret;

	ret = wlan_sap_set_pre_cac_complete_status(
			WLAN_HDD_GET_SAP_CTX_PTR(ap_adapter), status);
	if (QDF_IS_STATUS_ERROR(ret))
		return -EINVAL;

	return 0;
}

/**
 * __wlan_hdd_sap_pre_cac_failure() - Process the pre cac failure
 * @data: AP adapter
 *
 * Deletes the pre cac adapter
 *
 * Return: None
 */
static void __wlan_hdd_sap_pre_cac_failure(void *data)
{
	hdd_adapter_t *adapter;
	hdd_context_t *hdd_ctx;

	ENTER();

	adapter = (hdd_adapter_t *) data;
	if (!adapter ||
	    adapter->magic != WLAN_HDD_ADAPTER_MAGIC) {
		hdd_err("SAP Pre CAC adapter invalid");
		return;
	}

	hdd_ctx = (hdd_context_t *) (adapter->pHddCtx);
	if (wlan_hdd_validate_context(hdd_ctx)) {
		hdd_err("HDD context is null");
		return;
	}

	wlan_hdd_release_intf_addr(hdd_ctx,
				   adapter->macAddressCurrent.bytes);
	hdd_stop_adapter(hdd_ctx, adapter, true);
	hdd_close_adapter(hdd_ctx, adapter, false);
}

/**
 * wlan_hdd_sap_pre_cac_failure() - Process the pre cac failure
 * @data: AP adapter
 *
 * Deletes the pre cac adapter
 *
 * Return: None
 */
void wlan_hdd_sap_pre_cac_failure(void *data)
{
	cds_ssr_protect(__func__);
	__wlan_hdd_sap_pre_cac_failure(data);
	cds_ssr_unprotect(__func__);
}

/**
 * wlan_hdd_sap_pre_cac_success() - Process the pre cac result
 * @data: AP adapter
 *
 * Deletes the pre cac adapter and moves the existing SAP to the pre cac
 * channel
 *
 * Return: None
 */
static void wlan_hdd_sap_pre_cac_success(void *data)
{
	hdd_adapter_t *pHostapdAdapter, *ap_adapter;
	int i;
	hdd_context_t *hdd_ctx;

	ENTER();

	pHostapdAdapter = (hdd_adapter_t *) data;
	if (!pHostapdAdapter) {
		hdd_err("AP adapter is NULL");
		return;
	}

	hdd_ctx = (hdd_context_t *) (pHostapdAdapter->pHddCtx);
	if (!hdd_ctx) {
		hdd_err("HDD context is null");
		return;
	}

	cds_ssr_protect(__func__);
	wlan_hdd_release_intf_addr(hdd_ctx,
				   pHostapdAdapter->macAddressCurrent.bytes);
	hdd_stop_adapter(hdd_ctx, pHostapdAdapter, true);
	hdd_close_adapter(hdd_ctx, pHostapdAdapter, false);
	cds_ssr_unprotect(__func__);

	/* Prepare to switch AP from 2.4GHz channel to the pre CAC channel */
	ap_adapter = hdd_get_adapter(hdd_ctx, QDF_SAP_MODE);
	if (!ap_adapter) {
		hdd_err("failed to get SAP adapter, no restart on pre CAC channel");
		return;
	}

	/*
	 * Setting of the pre cac complete status will ensure that on channel
	 * switch to the pre CAC DFS channel, there is no CAC again.
	 */
	wlan_hdd_set_pre_cac_complete_status(ap_adapter, true);
	i = hdd_softap_set_channel_change(ap_adapter->dev,
			ap_adapter->pre_cac_chan,
			CH_WIDTH_MAX);
	if (0 != i) {
		hdd_err("failed to change channel");
		wlan_hdd_set_pre_cac_complete_status(ap_adapter, false);
	}
}

#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
/**
 * hdd_handle_acs_scan_event() - handle acs scan event for SAP
 * @sap_event: tpSap_Event
 * @adapter: hdd_adapter_t for SAP
 *
 * The function is to handle the eSAP_ACS_SCAN_SUCCESS_EVENT event.
 * It will update scan result to cfg80211 and start a timer to flush the
 * cached acs scan result.
 *
 * Return: QDF_STATUS_SUCCESS on success,
 *      other value on failure
 */
static QDF_STATUS hdd_handle_acs_scan_event(tpSap_Event sap_event,
		hdd_adapter_t *adapter)
{
	hdd_context_t *hdd_ctx;
	struct sap_acs_scan_complete_event *comp_evt;
	QDF_STATUS qdf_status;
	int chan_list_size = 0;
	uint8_t *tmp_last_acs_channel_list = NULL;

	hdd_ctx = (hdd_context_t *)(adapter->pHddCtx);
	if (!hdd_ctx) {
		hdd_err("HDD context is null");
		return QDF_STATUS_E_FAILURE;
	}
	comp_evt = &sap_event->sapevt.sap_acs_scan_comp;
	hdd_ctx->skip_acs_scan_status = eSAP_SKIP_ACS_SCAN;
	if (comp_evt->num_of_channels && comp_evt->channellist) {
		chan_list_size = comp_evt->num_of_channels *
			sizeof(comp_evt->channellist[0]);
		tmp_last_acs_channel_list = qdf_mem_malloc(
			chan_list_size);
	}
	qdf_spin_lock(&hdd_ctx->acs_skip_lock);
	qdf_mem_free(hdd_ctx->last_acs_channel_list);
	hdd_ctx->last_acs_channel_list = tmp_last_acs_channel_list;
	hdd_ctx->num_of_channels = 0;
	/* cache the previous ACS scan channel list .
	 * If the following OBSS scan chan list is covered by ACS chan list,
	 * we can skip OBSS Scan to save SAP starting total time.
	 */
	if (comp_evt->num_of_channels && comp_evt->channellist) {
		if (hdd_ctx->last_acs_channel_list) {
			qdf_mem_copy(hdd_ctx->last_acs_channel_list,
				comp_evt->channellist,
				chan_list_size);
			hdd_ctx->num_of_channels = comp_evt->num_of_channels;
		}
	}
	qdf_spin_unlock(&hdd_ctx->acs_skip_lock);
	/* Update ACS scan result to cfg80211. Then OBSS scan can reuse the
	 * scan result.
	 */
	if (wlan_hdd_cfg80211_update_bss(hdd_ctx->wiphy, adapter, 0))
		hdd_debug("NO SCAN result");

	hdd_debug("Reusing Last ACS scan result for %d sec",
		ACS_SCAN_EXPIRY_TIMEOUT_S);
	qdf_mc_timer_stop(&hdd_ctx->skip_acs_scan_timer);
	qdf_status = qdf_mc_timer_start(&hdd_ctx->skip_acs_scan_timer,
			ACS_SCAN_EXPIRY_TIMEOUT_S * 1000);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		hdd_err("Failed to start ACS scan expiry timer");
	return QDF_STATUS_SUCCESS;
}
#else
static QDF_STATUS hdd_handle_acs_scan_event(tpSap_Event sap_event,
		hdd_adapter_t *adapter)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * hdd_update_scan_result() - Update the scan result to CFG80211
 * @sap_event: SAP event
 * @adapter: adapter for SAP
 *
 * The function is to update the scan result to CFG80211
 *
 * Return: QDF_STATUS_SUCCESS on success,
 *      other value on failure
 */
static QDF_STATUS hdd_update_scan_result(tpSap_Event sap_event,
		hdd_adapter_t *adapter)
{
	struct cfg80211_bss *bss_status;

	if (NULL != sap_event->sapevt.bss_desc) {
		bss_status = wlan_hdd_cfg80211_inform_bss_frame(
			adapter,
			sap_event->sapevt.bss_desc);
		if (NULL == bss_status) {
			hdd_info("UPDATE_SCAN_RESULT returned NULL");
			return QDF_STATUS_E_FAILURE;
		}
		cfg80211_put_bss(
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 9, 0)) || defined(WITH_BACKPORTS)
				(WLAN_HDD_GET_CTX(adapter))->wiphy,
#endif
				bss_status);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * get_max_rate_vht() - calculate max rate for VHT mode
 * @nss: num of streams
 * @ch_width: channel width
 * @sgi: short gi
 * @vht_mcs_map: vht mcs map
 *
 * This function calculate max rate for VHT mode
 *
 * Return: max rate
 */
static int get_max_rate_vht(int nss, int ch_width, int sgi, int vht_mcs_map)
{
	const struct index_vht_data_rate_type *supported_vht_mcs_rate;
	enum data_rate_11ac_max_mcs vht_max_mcs;
	int maxrate = 0;
	int maxidx;

	if (nss == 1) {
		supported_vht_mcs_rate = supported_vht_mcs_rate_nss1;
	} else if (nss == 2) {
		supported_vht_mcs_rate = supported_vht_mcs_rate_nss2;
	} else {
		/* Not Supported */
		hdd_err("nss %d not supported", nss);
		return maxrate;
	}

	vht_max_mcs =
		(enum data_rate_11ac_max_mcs)
		(vht_mcs_map & DATA_RATE_11AC_MCS_MASK);

	if (vht_max_mcs == DATA_RATE_11AC_MAX_MCS_7) {
		maxidx = 7;
	} else if (vht_max_mcs == DATA_RATE_11AC_MAX_MCS_8) {
		maxidx = 8;
	} else if (vht_max_mcs == DATA_RATE_11AC_MAX_MCS_9) {
		if (ch_width == eHT_CHANNEL_WIDTH_20MHZ)
			/* MCS9 is not valid for VHT20 when nss=1,2 */
			maxidx = 8;
		else
			maxidx = 9;
	} else {
		hdd_err("vht mcs map %x not supported",
				vht_mcs_map & DATA_RATE_11AC_MCS_MASK);
		return maxrate;
	}

	if (ch_width == eHT_CHANNEL_WIDTH_20MHZ) {
		maxrate =
		supported_vht_mcs_rate[maxidx].supported_VHT20_rate[sgi];
	} else if (ch_width == eHT_CHANNEL_WIDTH_40MHZ) {
		maxrate =
		supported_vht_mcs_rate[maxidx].supported_VHT40_rate[sgi];
	} else if (ch_width == eHT_CHANNEL_WIDTH_80MHZ) {
		maxrate =
		supported_vht_mcs_rate[maxidx].supported_VHT80_rate[sgi];
	} else {
		hdd_err("ch_width %d not supported", ch_width);
		return maxrate;
	}

	return maxrate;
}

/**
 * calculate_max_phy_rate() - calcuate maximum phy rate (100kbps)
 * @mode: phymode: Legacy, 11a/b/g, HT, VHT
 * @nss: num of stream (maximum num is 2)
 * @ch_width: channel width
 * @sgi: short gi enabled or not
 * @supp_idx: max supported idx
 * @ext_idx: max extended idx
 * @ht_mcs_idx: max mcs index for HT
 * @vht_mcs_map: mcs map for VHT
 *
 * return: maximum phy rate in 100kbps
 */
static int calcuate_max_phy_rate(int mode, int nss, int ch_width,
		int sgi, int supp_idx, int ext_idx, int ht_mcs_idx,
		int vht_mcs_map)
{
	const struct index_data_rate_type *supported_mcs_rate;
	int maxidx = 12; /*default 6M mode*/
	int maxrate = 0, tmprate;
	int i;

	/* check supported rates */
	if (supp_idx != 0xff && maxidx < supp_idx)
		maxidx = supp_idx;

	/* check extended rates */
	if (ext_idx != 0xff && maxidx < ext_idx)
		maxidx = ext_idx;

	for (i = 0; i < QDF_ARRAY_SIZE(supported_data_rate); i++) {
		if (supported_data_rate[i].beacon_rate_index == maxidx)
			maxrate = supported_data_rate[i].supported_rate[0];
	}

	if (mode == SIR_SME_PHY_MODE_HT) {
		/* check for HT Mode */
		maxidx = ht_mcs_idx;
		if (nss == 1) {
			supported_mcs_rate = supported_mcs_rate_nss1;
		} else if (nss == 2) {
			supported_mcs_rate = supported_mcs_rate_nss2;
		} else {
			/* Not Supported */
			hdd_err("nss %d not supported", nss);
			return maxrate;
		}

		if (ch_width == eHT_CHANNEL_WIDTH_20MHZ) {
			tmprate = sgi ?
				supported_mcs_rate[maxidx].supported_rate[2] :
				supported_mcs_rate[maxidx].supported_rate[0];
		} else if (ch_width == eHT_CHANNEL_WIDTH_40MHZ) {
			tmprate = sgi ?
				supported_mcs_rate[maxidx].supported_rate[3] :
				supported_mcs_rate[maxidx].supported_rate[1];
		} else {
			hdd_err("invalid mode %d ch_width %d",
					mode, ch_width);
			return maxrate;
		}

		if (maxrate < tmprate)
			maxrate = tmprate;
	}

	if (mode == SIR_SME_PHY_MODE_VHT) {
		/* check for VHT Mode */
		tmprate = get_max_rate_vht(nss, ch_width, sgi, vht_mcs_map);
		if (maxrate < tmprate)
			maxrate = tmprate;
	}

	return maxrate;
}

/**
 * hdd_convert_dot11mode_from_phymode() - get dot11 mode from phymode
 * @phymode: phymode of sta associated to SAP
 *
 * The function is to convert the phymode to corresponding dot11 mode
 *
 * Return: dot11mode.
 */


static int hdd_convert_dot11mode_from_phymode(int phymode)
{

	switch (phymode) {

	case MODE_11A:
		return QCA_WLAN_802_11_MODE_11A;

	case MODE_11B:
		return QCA_WLAN_802_11_MODE_11B;

	case MODE_11G:
	case MODE_11GONLY:
		return QCA_WLAN_802_11_MODE_11G;

	case MODE_11NA_HT20:
	case MODE_11NG_HT20:
	case MODE_11NA_HT40:
	case MODE_11NG_HT40:
		return QCA_WLAN_802_11_MODE_11N;

	case MODE_11AC_VHT20:
	case MODE_11AC_VHT40:
	case MODE_11AC_VHT80:
	case MODE_11AC_VHT20_2G:
	case MODE_11AC_VHT40_2G:
	case MODE_11AC_VHT80_2G:
#ifdef CONFIG_160MHZ_SUPPORT
	case MODE_11AC_VHT80_80:
	case MODE_11AC_VHT160:
#endif
		return QCA_WLAN_802_11_MODE_11AC;

	default:
		return QCA_WLAN_802_11_MODE_INVALID;
	}

}

/**
 * hdd_fill_station_info() - fill stainfo once connected
 * @stainfo: peer stainfo associate to SAP
 * @event: associate/reassociate event received
 *
 * The function is to update rate stats to stainfo
 *
 * Return: None.
 */
static void hdd_fill_station_info(hdd_adapter_t *pHostapdAdapter,
				  tSap_StationAssocReassocCompleteEvent *event)
{
	hdd_station_info_t *stainfo;
	uint8_t i = 0, oldest_disassoc_sta_idx = WLAN_MAX_STA_COUNT + 1;
	qdf_time_t oldest_disassoc_sta_ts = 0;


	if (event->staId >= WLAN_MAX_STA_COUNT) {
		hdd_err("invalid sta id");
		return;
	}

	stainfo = &pHostapdAdapter->aStaInfo[event->staId];
	if (!stainfo) {
		hdd_err("invalid stainfo");
		return;
	}

	qdf_mem_copy(&stainfo->capability, &event->capability_info,
		     sizeof(uint16_t));
	stainfo->freq = cds_chan_to_freq(event->chan_info.chan_id);
	stainfo->dot11_mode =
		hdd_convert_dot11mode_from_phymode(event->chan_info.info);
	stainfo->nss = event->chan_info.nss;
	stainfo->rate_flags = event->chan_info.rate_flags;
	stainfo->ampdu = event->ampdu;
	stainfo->sgi_enable = event->sgi_enable;
	stainfo->tx_stbc = event->tx_stbc;
	stainfo->rx_stbc = event->rx_stbc;
	stainfo->ch_width = event->ch_width;
	stainfo->mode = event->mode;
	stainfo->max_supp_idx = event->max_supp_idx;
	stainfo->max_ext_idx = event->max_ext_idx;
	stainfo->max_mcs_idx = event->max_mcs_idx;
	stainfo->rx_mcs_map = event->rx_mcs_map;
	stainfo->tx_mcs_map = event->tx_mcs_map;
	stainfo->assoc_ts = qdf_system_ticks();
	stainfo->max_phy_rate =
		calcuate_max_phy_rate(stainfo->mode,
				stainfo->nss,
				stainfo->ch_width,
				stainfo->sgi_enable,
				stainfo->max_supp_idx,
				stainfo->max_ext_idx,
				stainfo->max_mcs_idx,
				stainfo->rx_mcs_map);
	/* expect max_phy_rate report in kbps */
	stainfo->max_phy_rate *= 100;

	if (event->vht_caps.present) {
		stainfo->vht_present = true;
		hdd_copy_vht_caps(&stainfo->vht_caps, &event->vht_caps);
	}
	if (event->ht_caps.present) {
		stainfo->ht_present = true;
		hdd_copy_ht_caps(&stainfo->ht_caps, &event->ht_caps);
	}

	/* Initialize DHCP info */
	stainfo->dhcp_phase = DHCP_PHASE_ACK;
	stainfo->dhcp_nego_status = DHCP_NEGO_STOP;

	while (i < WLAN_MAX_STA_COUNT) {
		if (!qdf_mem_cmp(pHostapdAdapter->
				 cache_sta_info[i].macAddrSTA.bytes,
				 event->staMac.bytes,
				 QDF_MAC_ADDR_SIZE)) {
			break;
		}
		i++;
	}

	if (i == WLAN_MAX_STA_COUNT) {
		i = 0;
		while (i < WLAN_MAX_STA_COUNT) {
			if (pHostapdAdapter->cache_sta_info[i].isUsed != TRUE)
				break;

			if (pHostapdAdapter->
					cache_sta_info[i].disassoc_ts &&
			    (!oldest_disassoc_sta_ts ||
			    (qdf_system_time_after(
					oldest_disassoc_sta_ts,
					pHostapdAdapter->
					cache_sta_info[i].disassoc_ts)))) {
				oldest_disassoc_sta_ts =
					pHostapdAdapter->
						cache_sta_info[i].disassoc_ts;
				oldest_disassoc_sta_idx = i;
			}
			i++;
		}
	}

	if ((i == WLAN_MAX_STA_COUNT) && oldest_disassoc_sta_ts) {
		hdd_debug("reached max cached staid, removing oldest stainfo");
		i = oldest_disassoc_sta_idx;
	}
	if (i < WLAN_MAX_STA_COUNT) {
		qdf_mem_zero(&pHostapdAdapter->cache_sta_info[i],
			     sizeof(*stainfo));
		qdf_mem_copy(&pHostapdAdapter->cache_sta_info[i],
				     stainfo, sizeof(hdd_station_info_t));

	} else {
		hdd_debug("reached max staid, stainfo can't be cached");
	}

	hdd_debug("cap %d %d %d %d %d %d %d %d %d %x %d",
			stainfo->ampdu,
			stainfo->sgi_enable,
			stainfo->tx_stbc,
			stainfo->rx_stbc,
			stainfo->isQosEnabled,
			stainfo->ch_width,
			stainfo->mode,
			event->wmmEnabled,
			event->chan_info.nss,
			event->chan_info.rate_flags,
			stainfo->max_phy_rate);
	hdd_debug("rate info %d %d %d %d %d",
			stainfo->max_supp_idx,
			stainfo->max_ext_idx,
			stainfo->max_mcs_idx,
			stainfo->rx_mcs_map,
			stainfo->tx_mcs_map);
}

/**
 * hdd_stop_sap_due_to_invalid_channel() - to stop sap in case of invalid chnl
 * @work: pointer to work structure
 *
 * Let's say SAP detected RADAR and trying to select the new channel and if no
 * valid channel is found due to none of the channels are available or
 * regulatory restriction then SAP needs to be stopped. so SAP state-machine
 * will create a work to stop the bss
 *
 * stop bss has to happen through worker thread because radar indication comes
 * from FW through mc thread or main host thread and if same thread is used to
 * do stopbss then waiting for stopbss to finish operation will halt mc thread
 * to freeze which will trigger stopbss timeout. Instead worker thread can do
 * the stopbss operation while mc thread waits for stopbss to finish.
 *
 * Return: none
 */
static void
hdd_stop_sap_due_to_invalid_channel(struct work_struct *work)
{
	/*
	 * Extract the adapter from work structure. sap_stop_bss_work
	 * is part of adapter context.
	 */
	hdd_adapter_t *sap_adapter = container_of(work, hdd_adapter_t,
						  sap_stop_bss_work);
	cds_ssr_protect(__func__);
	if (sap_adapter == NULL) {
	    cds_err("sap_adapter is NULL, no work needed");
	    cds_ssr_unprotect(__func__);
	    return;
	}
	hdd_debug("work started for sap session[%d]", sap_adapter->sessionId);
	wlan_hdd_stop_sap(sap_adapter);
	wlansap_set_invalid_session(WLAN_HDD_GET_SAP_CTX_PTR(sap_adapter));
	wlansap_cleanup_cac_timer(WLAN_HDD_GET_SAP_CTX_PTR(sap_adapter));
	hdd_debug("work finished for sap");
	cds_ssr_unprotect(__func__);
}

QDF_STATUS hdd_softap_set_peer_authorized(hdd_adapter_t *adapter,
					  struct qdf_mac_addr *peer_mac)
{
	QDF_STATUS status;
	hdd_context_t *hdd_ctx;

	if (hdd_validate_adapter(adapter)) {
		hdd_err("Invalid adapter");
		return QDF_STATUS_E_INVAL;
	}

	hdd_ctx = (hdd_context_t *) (adapter->pHddCtx);
	if (wlan_hdd_validate_context(hdd_ctx)) {
		hdd_err("Invalid HDD Context");
		return QDF_STATUS_E_INVAL;
	}

	status = hdd_softap_change_sta_state(adapter, peer_mac,
					     OL_TXRX_PEER_STATE_AUTH);

	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_debug("Not able to change TL state to AUTHENTICATED");
		return status;
	}

	status = wlan_hdd_send_sta_authorized_event(adapter, hdd_ctx, peer_mac);
	if (QDF_IS_STATUS_ERROR(status))
		hdd_debug("Failed to sent STA authorized event");

	return status;
}

QDF_STATUS hdd_hostapd_sap_event_cb(tpSap_Event pSapEvent,
				    void *usrDataForCallback)
{
	hdd_adapter_t *pHostapdAdapter;
	hdd_ap_ctx_t *pHddApCtx;
	hdd_hostapd_state_t *pHostapdState;
	struct net_device *dev;
	eSapHddEvent sapEvent;
	union iwreq_data wrqu;
	uint8_t *we_custom_event_generic = NULL;
	int we_event = 0;
	int i = 0;
	uint8_t staId;
	QDF_STATUS qdf_status;
	bool bWPSState;
	bool bAuthRequired = true;
	tpSap_AssocMacAddr pAssocStasArray = NULL;
	char unknownSTAEvent[IW_CUSTOM_MAX + 1];
	char maxAssocExceededEvent[IW_CUSTOM_MAX + 1];
	uint8_t we_custom_start_event[64];
	char *startBssEvent;
	hdd_context_t *pHddCtx;
	hdd_scaninfo_t *pScanInfo = NULL;
	struct iw_michaelmicfailure msg;
	uint8_t ignoreCAC = 0;
	struct hdd_config *cfg = NULL;
	struct wlan_dfs_info dfs_info;
	uint8_t cc_len = WLAN_SVC_COUNTRY_CODE_LEN;
	hdd_adapter_t *con_sap_adapter;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct hdd_chan_change_params chan_change;
	tSap_StationAssocReassocCompleteEvent *event;
	tSap_StationSetKeyCompleteEvent *key_complete;
	int ret = 0;
	struct ch_params_s sap_ch_param = {0};
	eCsrPhyMode phy_mode;
	bool legacy_phymode;
	tSap_StationDisassocCompleteEvent *disassoc_comp;
	hdd_station_info_t *stainfo, *cache_stainfo;
	cds_context_type *cds_ctx;
	hdd_adapter_t *sta_adapter;
	tsap_Config_t *sap_config;

	dev = (struct net_device *)usrDataForCallback;
	if (!dev) {
		hdd_err("usrDataForCallback is null");
		return QDF_STATUS_E_FAILURE;
	}

	pHostapdAdapter = netdev_priv(dev);

	if ((NULL == pHostapdAdapter) ||
	    (WLAN_HDD_ADAPTER_MAGIC != pHostapdAdapter->magic)) {
		hdd_err("invalid adapter or adapter has invalid magic");
		return QDF_STATUS_E_FAILURE;
	}

	pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);
	pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);

	if (!pSapEvent) {
		hdd_err("pSapEvent is null");
		return QDF_STATUS_E_FAILURE;
	}

	sapEvent = pSapEvent->sapHddEventCode;
	memset(&wrqu, '\0', sizeof(wrqu));
	pHddCtx = (hdd_context_t *) (pHostapdAdapter->pHddCtx);

	if (!pHddCtx) {
		hdd_err("HDD context is null");
		return QDF_STATUS_E_FAILURE;
	}

	cfg = pHddCtx->config;

	if (!cfg) {
		hdd_err("HDD config is null");
		return QDF_STATUS_E_FAILURE;
	}

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		hdd_err("QDF context is null");
		return QDF_STATUS_E_FAILURE;
	}


	dfs_info.channel = pHddApCtx->operatingChannel;
	sme_get_country_code(pHddCtx->hHal, dfs_info.country_code, &cc_len);
	sap_config = &pHostapdAdapter->sessionCtx.ap.sapConfig;

	switch (sapEvent) {
	case eSAP_START_BSS_EVENT:
		hdd_debug("BSS status = %s, channel = %u, bc sta Id = %d",
		       pSapEvent->sapevt.sapStartBssCompleteEvent.
		       status ? "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS",
		       pSapEvent->sapevt.sapStartBssCompleteEvent.
		       operatingChannel,
		       pSapEvent->sapevt.sapStartBssCompleteEvent.staId);

		pHostapdAdapter->sessionId =
			pSapEvent->sapevt.sapStartBssCompleteEvent.sessionId;

		sap_config->channel =
			pSapEvent->sapevt.sapStartBssCompleteEvent.
			operatingChannel;

		sap_config->ch_params.ch_width =
			pSapEvent->sapevt.sapStartBssCompleteEvent.ch_width;

		cds_set_channel_params(sap_config->channel, 0,
				       &sap_config->ch_params);

		pHostapdState->qdf_status =
			pSapEvent->sapevt.sapStartBssCompleteEvent.status;

		qdf_atomic_set(&pHddCtx->dfs_radar_found, 0);

		status = qdf_event_set(&cds_ctx->channel_switch_complete);

		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("set event failed");
			goto stopbss;
		}

		wlansap_get_dfs_ignore_cac(pHddCtx->hHal, &ignoreCAC);

		/* DFS requirement: DO NOT transmit during CAC. */
		if ((CHANNEL_STATE_DFS !=
			cds_get_channel_state(pHddApCtx->operatingChannel))
			|| ignoreCAC
			|| pHddCtx->dev_dfs_cac_status == DFS_CAC_ALREADY_DONE)
			pHddApCtx->dfs_cac_block_tx = false;
		else
			pHddApCtx->dfs_cac_block_tx = true;

		hdd_debug("The value of dfs_cac_block_tx[%d] for ApCtx[%pK]:%d",
				pHddApCtx->dfs_cac_block_tx, pHddApCtx,
				pHostapdAdapter->sessionId);

		if (pHostapdState->qdf_status) {
			hdd_err("startbss event failed!!");
			/*
			 * Make sure to set the event before proceeding
			 * for error handling otherwise caller thread will
			 * wait till 10 secs and no other connection will
			 * go through before that.
			 */
			pHostapdState->bssState = BSS_STOP;
			qdf_event_set(&pHostapdState->qdf_event);
			goto stopbss;
		} else {
			sme_ch_avoid_update_req(pHddCtx->hHal);

			pHddApCtx->uBCStaId =
				pSapEvent->sapevt.sapStartBssCompleteEvent.staId;

			hdd_register_tx_flow_control(pHostapdAdapter,
				hdd_softap_tx_resume_timer_expired_handler,
				hdd_softap_tx_resume_cb,
				hdd_tx_flow_control_is_pause);

			/* @@@ need wep logic here to set privacy bit */
			qdf_status =
				hdd_softap_register_bc_sta(pHostapdAdapter,
							   pHddApCtx->uPrivacy);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
				hdd_warn("Failed to register BC STA %d",
				       qdf_status);
				hdd_stop_bss_link(pHostapdAdapter,
						  usrDataForCallback);
			}
		}

		if (hdd_ipa_is_enabled(pHddCtx)) {
			status = hdd_ipa_wlan_evt(pHostapdAdapter,
					pHddApCtx->uBCStaId,
					HDD_IPA_AP_CONNECT,
					pHostapdAdapter->dev->dev_addr);
			if (status)
				hdd_err("WLAN_AP_CONNECT event failed");
		}

		if (0 !=
		    (WLAN_HDD_GET_CTX(pHostapdAdapter))->config->
		     nAPAutoShutOff) {
			/* AP Inactivity timer init and start */
			qdf_status =
				qdf_mc_timer_init(&pHddApCtx->
						  hdd_ap_inactivity_timer,
						  QDF_TIMER_TYPE_SW,
						  hdd_hostapd_inactivity_timer_cb,
						  dev);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status))
				hdd_err("Failed to init inactivity timer");

			qdf_status =
				qdf_mc_timer_start(&pHddApCtx->
						   hdd_ap_inactivity_timer,
						   (WLAN_HDD_GET_CTX
						    (pHostapdAdapter))->config->
						    nAPAutoShutOff * 1000);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status))
				hdd_err("Failed to init inactivity timer");

		}
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
		wlan_hdd_auto_shutdown_enable(pHddCtx, true);
#endif
		pHddApCtx->operatingChannel =
			pSapEvent->sapevt.sapStartBssCompleteEvent.operatingChannel;

		hdd_hostapd_channel_prevent_suspend(pHostapdAdapter,
						    pHddApCtx->
						    operatingChannel);

		pHostapdState->bssState = BSS_START;

		/* Set default key index */
		QDF_TRACE(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
			"%s: default key index %hu", __func__,
			pHddApCtx->wep_def_key_idx);

		sme_roam_set_default_key_index(
			WLAN_HDD_GET_HAL_CTX(pHostapdAdapter),
			pHostapdAdapter->sessionId,
			pHddApCtx->wep_def_key_idx);

		/* Set group key / WEP key every time when BSS is restarted */
		if (pHddApCtx->groupKey.keyLength) {
			status = wlansap_set_key_sta(
				WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
				&pHddApCtx->groupKey);
			if (!QDF_IS_STATUS_SUCCESS(status))
				hdd_err("wlansap_set_key_sta failed");
		} else {
			for (i = 0; i < CSR_MAX_NUM_KEY; i++) {
				if (!pHddApCtx->wepKey[i].keyLength)
					continue;

				status = wlansap_set_key_sta(
					WLAN_HDD_GET_SAP_CTX_PTR
						(pHostapdAdapter),
					&pHddApCtx->wepKey[i]);
				if (!QDF_IS_STATUS_SUCCESS(status))
					hdd_err("set_key failed idx: %d", i);
			}
		}

		/* Fill the params for sending IWEVCUSTOM Event
		 * with SOFTAP.enabled
		 */
		startBssEvent = "SOFTAP.enabled";
		memset(&we_custom_start_event, '\0',
		       sizeof(we_custom_start_event));
		memcpy(&we_custom_start_event, startBssEvent,
		       strlen(startBssEvent));
		memset(&wrqu, 0, sizeof(wrqu));
		wrqu.data.length = strlen(startBssEvent);
		we_event = IWEVCUSTOM;
		we_custom_event_generic = we_custom_start_event;
		cds_dump_concurrency_info();

		sta_adapter = hdd_get_adapter(pHddCtx, QDF_STA_MODE);
		if (sta_adapter != NULL) {
			hdd_debug("check for SAP restart");
			cds_check_concurrent_intf_and_restart_sap(sta_adapter);
		}

		if (cds_is_hw_mode_change_after_vdev_up()) {
			hdd_debug("check for possible hw mode change");
			status = cds_set_hw_mode_on_channel_switch(
					pHostapdAdapter->sessionId);
			if (QDF_IS_STATUS_ERROR(status))
				hdd_debug("set hw mode change not done");
			cds_set_do_hw_mode_change_flag(false);
		}
		/*
		 * set this event at the very end because once this events
		 * get set, caller thread is waiting to do further processing.
		 * so once this event gets set, current worker thread might get
		 * pre-empted by caller thread.
		 */
		qdf_status = qdf_event_set(&pHostapdState->qdf_event);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			hdd_err("qdf_event_set failed! status: %d", qdf_status);
			goto stopbss;
		}
		break;          /* Event will be sent after Switch-Case stmt */

	case eSAP_STOP_BSS_EVENT:
		hdd_debug("BSS stop status = %s",
		       pSapEvent->sapevt.sapStopBssCompleteEvent.
		       status ? "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS");

		hdd_hostapd_channel_allow_suspend(pHostapdAdapter,
						  pHddApCtx->operatingChannel);

		/* Free up Channel List incase if it is set */
		sap_cleanup_channel_list(
			WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter));

		/* Invalidate the channel info. */
		pHddApCtx->operatingChannel = 0;

		/* reset the dfs_cac_status and dfs_cac_block_tx flag only when
		 * the last BSS is stopped
		 */
		con_sap_adapter = hdd_get_con_sap_adapter(pHostapdAdapter, true);
		if (!con_sap_adapter) {
			pHddApCtx->dfs_cac_block_tx = true;
			pHddCtx->dev_dfs_cac_status = DFS_CAC_NEVER_DONE;
		}
		if (pHddCtx->config->conc_custom_rule2 &&
			(QDF_P2P_GO_MODE == pHostapdAdapter->device_mode)) {
			hdd_adapter_t *sta_adapter = hdd_get_adapter(pHddCtx,
					QDF_STA_MODE);
			hdd_debug("P2PGO is going down now");
			hdd_issue_stored_joinreq(sta_adapter, pHddCtx);
		}

		hdd_debug("bss_stop_reason=%d", pHddApCtx->bss_stop_reason);
		if ((BSS_STOP_DUE_TO_MCC_SCC_SWITCH !=
			pHddApCtx->bss_stop_reason) &&
		    (BSS_STOP_DUE_TO_VENDOR_CONFIG_CHAN !=
			pHddApCtx->bss_stop_reason)) {
			/*
			 * when MCC to SCC switching or vendor subcmd
			 * setting sap config channel happens, key storage
			 * should not be cleared due to hostapd will not
			 * repopulate the original keys
			 */
			pHddApCtx->groupKey.keyLength = 0;
			for (i = 0; i < CSR_MAX_NUM_KEY; i++)
				pHddApCtx->wepKey[i].keyLength = 0;
		}

		/* clear the reason code in case BSS is stopped
		 * in another place
		 */
		pHddApCtx->bss_stop_reason = BSS_STOP_REASON_INVALID;
		goto stopbss;

	case eSAP_DFS_CAC_START:
		wlan_hdd_send_svc_nlink_msg(pHddCtx->radio_index,
					WLAN_SVC_DFS_CAC_START_IND,
					    &dfs_info,
					    sizeof(struct wlan_dfs_info));
		pHddCtx->dev_dfs_cac_status = DFS_CAC_IN_PROGRESS;
		if (QDF_STATUS_SUCCESS !=
			hdd_send_radar_event(pHddCtx, eSAP_DFS_CAC_START,
				dfs_info, &pHostapdAdapter->wdev)) {
			hdd_err("Unable to indicate CAC start NL event");
		} else {
			hdd_debug("Sent CAC start to user space");
		}

		qdf_atomic_set(&pHddCtx->dfs_radar_found, 0);
		break;
	case eSAP_DFS_CAC_INTERRUPTED:
		/*
		 * The CAC timer did not run completely and a radar was detected
		 * during the CAC time. This new state will keep the tx path
		 * blocked since we do not want any transmission on the DFS
		 * channel. CAC end will only be reported here since the user
		 * space applications are waiting on CAC end for their state
		 * management.
		 */
		if (QDF_STATUS_SUCCESS !=
			hdd_send_radar_event(pHddCtx, eSAP_DFS_CAC_END,
				dfs_info, &pHostapdAdapter->wdev)) {
			hdd_err("Unable to indicate CAC end (interrupted) event");
		} else {
			hdd_debug("Sent CAC end (interrupted) to user space");
		}
		break;
	case eSAP_DFS_CAC_END:
		wlan_hdd_send_svc_nlink_msg(pHddCtx->radio_index,
					WLAN_SVC_DFS_CAC_END_IND,
					    &dfs_info,
					    sizeof(struct wlan_dfs_info));
		pHddApCtx->dfs_cac_block_tx = false;
		pHddCtx->dev_dfs_cac_status = DFS_CAC_ALREADY_DONE;
		if (QDF_STATUS_SUCCESS !=
			hdd_send_radar_event(pHddCtx, eSAP_DFS_CAC_END,
				dfs_info, &pHostapdAdapter->wdev)) {
			hdd_err("Unable to indicate CAC end NL event");
		} else {
			hdd_debug("Sent CAC end to user space");
		}
		break;
	case eSAP_DFS_RADAR_DETECT:
		wlan_hdd_send_svc_nlink_msg(pHddCtx->radio_index,
					WLAN_SVC_DFS_RADAR_DETECT_IND,
					    &dfs_info,
					    sizeof(struct wlan_dfs_info));
		pHddCtx->dev_dfs_cac_status = DFS_CAC_NEVER_DONE;
		if (QDF_STATUS_SUCCESS !=
			hdd_send_radar_event(pHddCtx, eSAP_DFS_RADAR_DETECT,
				dfs_info, &pHostapdAdapter->wdev)) {
			hdd_err("Unable to indicate Radar detect NL event");
		} else {
			hdd_debug("Sent radar detected to user space");
		}
		break;
	case eSAP_DFS_RADAR_DETECT_DURING_PRE_CAC:
		hdd_debug("notification for radar detect during pre cac:%d",
			pHostapdAdapter->sessionId);
		hdd_send_conditional_chan_switch_status(pHddCtx,
			&pHostapdAdapter->wdev, false);
		pHddCtx->dev_dfs_cac_status = DFS_CAC_NEVER_DONE;
		qdf_create_work(0, &pHddCtx->sap_pre_cac_work,
				wlan_hdd_sap_pre_cac_failure,
				(void *)pHostapdAdapter);
		qdf_sched_work(0, &pHddCtx->sap_pre_cac_work);
		break;
	case eSAP_DFS_PRE_CAC_END:
		hdd_debug("pre cac end notification received:%d",
			pHostapdAdapter->sessionId);
		hdd_send_conditional_chan_switch_status(pHddCtx,
			&pHostapdAdapter->wdev, true);
		pHddApCtx->dfs_cac_block_tx = false;
		pHddCtx->dev_dfs_cac_status = DFS_CAC_ALREADY_DONE;

		qdf_create_work(0, &pHddCtx->sap_pre_cac_work,
				wlan_hdd_sap_pre_cac_success,
				(void *)pHostapdAdapter);
		qdf_sched_work(0, &pHddCtx->sap_pre_cac_work);
		break;
	case eSAP_DFS_NO_AVAILABLE_CHANNEL:
		wlan_hdd_send_svc_nlink_msg
			(pHddCtx->radio_index,
			WLAN_SVC_DFS_ALL_CHANNEL_UNAVAIL_IND, &dfs_info,
			sizeof(struct wlan_dfs_info));
		break;

	case eSAP_STA_SET_KEY_EVENT:
		/* TODO:
		 * forward the message to hostapd once implementation
		 * is done for now just print
		 */

		key_complete = &pSapEvent->sapevt.sapStationSetKeyCompleteEvent;
		hdd_debug("SET Key: configured status = %s",
			  key_complete->status ?
			  "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS");

		if (QDF_IS_STATUS_SUCCESS(key_complete->status)) {
			hdd_softap_set_peer_authorized(pHostapdAdapter,
						&key_complete->peerMacAddr);
		}

		return QDF_STATUS_SUCCESS;
	case eSAP_STA_MIC_FAILURE_EVENT:
	{
		memset(&msg, '\0', sizeof(msg));
		msg.src_addr.sa_family = ARPHRD_ETHER;
		memcpy(msg.src_addr.sa_data,
		       &pSapEvent->sapevt.sapStationMICFailureEvent.
		       staMac, QDF_MAC_ADDR_SIZE);
		hdd_debug("MIC MAC " MAC_ADDRESS_STR,
		       MAC_ADDR_ARRAY(msg.src_addr.sa_data));
		if (pSapEvent->sapevt.sapStationMICFailureEvent.
		    multicast == true)
			msg.flags = IW_MICFAILURE_GROUP;
		else
			msg.flags = IW_MICFAILURE_PAIRWISE;
		memset(&wrqu, 0, sizeof(wrqu));
		wrqu.data.length = sizeof(msg);
		we_event = IWEVMICHAELMICFAILURE;
		we_custom_event_generic = (uint8_t *) &msg;
	}
		/* inform mic failure to nl80211 */
		cfg80211_michael_mic_failure(dev,
					     pSapEvent->
					     sapevt.sapStationMICFailureEvent.
					     staMac.bytes,
					     ((pSapEvent->sapevt.
					       sapStationMICFailureEvent.
					       multicast ==
					       true) ?
					      NL80211_KEYTYPE_GROUP :
					      NL80211_KEYTYPE_PAIRWISE),
					     pSapEvent->sapevt.
					     sapStationMICFailureEvent.keyId,
					     pSapEvent->sapevt.
					     sapStationMICFailureEvent.TSC,
					     GFP_KERNEL);
		break;

	case eSAP_STA_ASSOC_EVENT:
	case eSAP_STA_REASSOC_EVENT:
		event = &pSapEvent->sapevt.sapStationAssocReassocCompleteEvent;
		if (eSAP_STATUS_FAILURE == event->status) {
			hdd_notice("assoc failure: " MAC_ADDRESS_STR,
				   MAC_ADDR_ARRAY(wrqu.addr.sa_data));
			break;
		}

		wrqu.addr.sa_family = ARPHRD_ETHER;
		memcpy(wrqu.addr.sa_data,
		       &event->staMac, QDF_MAC_ADDR_SIZE);
		hdd_notice(" associated " MAC_ADDRESS_STR,
		       MAC_ADDR_ARRAY(wrqu.addr.sa_data));
		we_event = IWEVREGISTERED;

		wlansap_get_wps_state(
			WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
			&bWPSState);

		if ((eCSR_ENCRYPT_TYPE_NONE == pHddApCtx->ucEncryptType) ||
		    (eCSR_ENCRYPT_TYPE_WEP40_STATICKEY ==
		     pHddApCtx->ucEncryptType)
		    || (eCSR_ENCRYPT_TYPE_WEP104_STATICKEY ==
			pHddApCtx->ucEncryptType)) {
			bAuthRequired = false;
		}

		if (bAuthRequired || bWPSState == true) {
			qdf_status = hdd_softap_register_sta(
						pHostapdAdapter,
						true,
						pHddApCtx->uPrivacy,
						event->staId, 0, 0,
						(struct qdf_mac_addr *)
						wrqu.addr.sa_data,
						event->wmmEnabled);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status))
				hdd_err("Failed to register STA %d "
					  MAC_ADDRESS_STR "", qdf_status,
				       MAC_ADDR_ARRAY(wrqu.addr.sa_data));
		} else {
			qdf_status = hdd_softap_register_sta(
						pHostapdAdapter,
						false,
						pHddApCtx->uPrivacy,
						event->staId, 0, 0,
						(struct qdf_mac_addr *)
						wrqu.addr.sa_data,
						event->wmmEnabled);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status))
				hdd_err("Failed to register STA %d "
					  MAC_ADDRESS_STR "", qdf_status,
				       MAC_ADDR_ARRAY(wrqu.addr.sa_data));
		}

		if (QDF_IS_STATUS_SUCCESS(qdf_status))
			hdd_fill_station_info(pHostapdAdapter, event);

		pHostapdAdapter->aStaInfo[event->staId].ecsa_capable =
			event->ecsa_capable;

		if (hdd_ipa_is_enabled(pHddCtx)) {
			status = hdd_ipa_wlan_evt(pHostapdAdapter,
					event->staId, HDD_IPA_CLIENT_CONNECT_EX,
					event->staMac.bytes);
			if (status)
				hdd_err("WLAN_CLIENT_CONNECT_EX event failed");
		}

		DPTRACE(qdf_dp_trace_mgmt_pkt(QDF_DP_TRACE_MGMT_PACKET_RECORD,
			pHostapdAdapter->sessionId,
			QDF_PROTO_TYPE_MGMT, QDF_PROTO_MGMT_ASSOC));

#ifdef MSM_PLATFORM
		/* start timer in sap/p2p_go */
		if (pHddApCtx->bApActive == false) {
			spin_lock_bh(&pHddCtx->bus_bw_lock);
			pHostapdAdapter->prev_tx_packets =
				pHostapdAdapter->stats.tx_packets;
			pHostapdAdapter->prev_rx_packets =
				pHostapdAdapter->stats.rx_packets;
			ol_get_intra_bss_fwd_pkts_count(
				pHostapdAdapter->sessionId,
				&pHostapdAdapter->prev_fwd_tx_packets,
				&pHostapdAdapter->prev_fwd_rx_packets);
			spin_unlock_bh(&pHddCtx->bus_bw_lock);
			hdd_bus_bw_compute_timer_start(pHddCtx);
		}
#endif
		pHddApCtx->bApActive = true;
		/* Stop AP inactivity timer */
		if (pHddApCtx->hdd_ap_inactivity_timer.state ==
		    QDF_TIMER_STATE_RUNNING) {
			qdf_status =
				qdf_mc_timer_stop(&pHddApCtx->
						  hdd_ap_inactivity_timer);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status))
				hdd_err("Failed to start inactivity timer");
		}
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
		wlan_hdd_auto_shutdown_enable(pHddCtx, false);
#endif
		cds_host_diag_log_work(&pHddCtx->sap_wake_lock,
				       HDD_SAP_WAKE_LOCK_DURATION,
				       WIFI_POWER_EVENT_WAKELOCK_SAP);
		qdf_wake_lock_timeout_acquire(&pHddCtx->sap_wake_lock,
					      HDD_SAP_WAKE_LOCK_DURATION);
		{
			struct station_info *sta_info;
			uint16_t iesLen = event->iesLen;

			sta_info = qdf_mem_malloc(sizeof(*sta_info));
			if (!sta_info) {
				hdd_err("Failed to allocate station info");
				return QDF_STATUS_E_FAILURE;
			}
			if (iesLen <= MAX_ASSOC_IND_IE_LEN) {
				sta_info->assoc_req_ies =
					(const u8 *)&event->ies[0];
				sta_info->assoc_req_ies_len = iesLen;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)) && !defined(WITH_BACKPORTS)
				/*
				 * After Kernel 4.0, it's no longer need to set
				 * STATION_INFO_ASSOC_REQ_IES flag, as it
				 * changed to use assoc_req_ies_len length to
				 * check the existance of request IE.
				 */
				sta_info->filled |= STATION_INFO_ASSOC_REQ_IES;
#endif
				cfg80211_new_sta(dev,
					(const u8 *)&event->staMac.bytes[0],
					sta_info, GFP_KERNEL);
			} else {
				hdd_err("Assoc Ie length is too long");
			}
			qdf_mem_free(sta_info);
		}
		pScanInfo = &pHostapdAdapter->scan_info;
		/* Lets abort scan to ensure smooth authentication for client */
		if ((pScanInfo != NULL) && pScanInfo->mScanPending) {
			hdd_abort_mac_scan(pHddCtx, pHostapdAdapter->sessionId,
					   INVALID_SCAN_ID,
					   eCSR_SCAN_ABORT_DEFAULT);
		}
		if (pHostapdAdapter->device_mode == QDF_P2P_GO_MODE) {
			/* send peer status indication to oem app */
			hdd_send_peer_status_ind_to_oem_app(&event->staMac,
					ePeerConnected,
					event->timingMeasCap,
					pHostapdAdapter->sessionId,
					&event->chan_info,
					pHostapdAdapter->device_mode);
		}
		hdd_green_ap_add_sta(pHddCtx);
		break;

	case eSAP_STA_DISASSOC_EVENT:
		disassoc_comp =
			&pSapEvent->sapevt.sapStationDisassocCompleteEvent;
		memcpy(wrqu.addr.sa_data,
		       &disassoc_comp->staMac, QDF_MAC_ADDR_SIZE);

		cache_stainfo = hdd_get_stainfo(pHostapdAdapter->cache_sta_info,
					  disassoc_comp->staMac);
		if (cache_stainfo) {
			/* Cache the disassoc info */
			cache_stainfo->rssi = disassoc_comp->rssi;
			cache_stainfo->tx_rate = disassoc_comp->tx_rate;
			cache_stainfo->rx_rate = disassoc_comp->rx_rate;
			cache_stainfo->rx_mc_bc_cnt =
						disassoc_comp->rx_mc_bc_cnt;
			cache_stainfo->reason_code = disassoc_comp->reason_code;
			cache_stainfo->disassoc_ts = qdf_system_ticks();
		}
		hdd_notice(" disassociated " MAC_ADDRESS_STR,
				MAC_ADDR_ARRAY(wrqu.addr.sa_data));

		qdf_status = qdf_event_set(&pHostapdState->qdf_sta_disassoc_event);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			hdd_err("Station Deauth event Set failed");

		if (pSapEvent->sapevt.sapStationDisassocCompleteEvent.reason ==
		    eSAP_USR_INITATED_DISASSOC)
			hdd_debug(" User initiated disassociation");
		else
			hdd_debug(" MAC initiated disassociation");
		we_event = IWEVEXPIRED;
		qdf_status =
			hdd_softap_get_sta_id(pHostapdAdapter,
					      &pSapEvent->sapevt.
					      sapStationDisassocCompleteEvent.staMac,
					      &staId);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			hdd_err("Failed to find sta id status: %d", qdf_status);
			return QDF_STATUS_E_FAILURE;
		}

		DPTRACE(qdf_dp_trace_mgmt_pkt(QDF_DP_TRACE_MGMT_PACKET_RECORD,
			pHostapdAdapter->sessionId,
			QDF_PROTO_TYPE_MGMT, QDF_PROTO_MGMT_DISASSOC));

		stainfo = hdd_get_stainfo(pHostapdAdapter->aStaInfo,
					  disassoc_comp->staMac);
		if (stainfo) {
			/* Send DHCP STOP indication to FW */
			stainfo->dhcp_phase = DHCP_PHASE_ACK;
			if (stainfo->dhcp_nego_status ==
						DHCP_NEGO_IN_PROGRESS)
				hdd_post_dhcp_ind(pHostapdAdapter, staId,
						WMA_DHCP_STOP_IND);
			stainfo->dhcp_nego_status = DHCP_NEGO_STOP;
		}
		hdd_softap_deregister_sta(pHostapdAdapter, staId);

		pHddApCtx->bApActive = false;
		spin_lock_bh(&pHostapdAdapter->staInfo_lock);
		for (i = 0; i < WLAN_MAX_STA_COUNT; i++) {
			if (pHostapdAdapter->aStaInfo[i].isUsed
			    && i !=
			    (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->
			    uBCStaId) {
				pHddApCtx->bApActive = true;
				break;
			}
		}
		spin_unlock_bh(&pHostapdAdapter->staInfo_lock);

		/* Start AP inactivity timer if no stations associated */
		if ((0 !=
		     (WLAN_HDD_GET_CTX(pHostapdAdapter))->config->
		     nAPAutoShutOff)) {
			if (pHddApCtx->bApActive == false) {
				if (pHddApCtx->hdd_ap_inactivity_timer.state ==
				    QDF_TIMER_STATE_STOPPED) {
					qdf_status =
						qdf_mc_timer_start(&pHddApCtx->
								   hdd_ap_inactivity_timer,
								   (WLAN_HDD_GET_CTX
								    (pHostapdAdapter))->
								   config->
								   nAPAutoShutOff *
								   1000);
					if (!QDF_IS_STATUS_SUCCESS(qdf_status))
						hdd_err("Failed to init AP inactivity timer");
				} else
					QDF_ASSERT
						(qdf_mc_timer_get_current_state
							(&pHddApCtx->
							hdd_ap_inactivity_timer) ==
						QDF_TIMER_STATE_STOPPED);
			}
		}
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
		wlan_hdd_auto_shutdown_enable(pHddCtx, true);
#endif

		cds_host_diag_log_work(&pHddCtx->sap_wake_lock,
				       HDD_SAP_WAKE_LOCK_DURATION,
				       WIFI_POWER_EVENT_WAKELOCK_SAP);
		qdf_wake_lock_timeout_acquire(&pHddCtx->sap_wake_lock,
			 HDD_SAP_CLIENT_DISCONNECT_WAKE_LOCK_DURATION);
		cfg80211_del_sta(dev,
				 (const u8 *)&pSapEvent->sapevt.
				 sapStationDisassocCompleteEvent.staMac.
				 bytes[0], GFP_KERNEL);

		/* Update the beacon Interval if it is P2P GO */
		qdf_status = cds_change_mcc_go_beacon_interval(pHostapdAdapter);
		if (QDF_STATUS_SUCCESS != qdf_status) {
			hdd_err("Failed to update Beacon interval status: %d",
				qdf_status);
		}
		if (pHostapdAdapter->device_mode == QDF_P2P_GO_MODE) {
			/* send peer status indication to oem app */
			hdd_send_peer_status_ind_to_oem_app(&pSapEvent->sapevt.
						sapStationDisassocCompleteEvent.
						staMac, ePeerDisconnected,
						0,
						pHostapdAdapter->sessionId,
						NULL,
						pHostapdAdapter->device_mode);
		}
#ifdef MSM_PLATFORM
		/*stop timer in sap/p2p_go */
		if (pHddApCtx->bApActive == false) {
			spin_lock_bh(&pHddCtx->bus_bw_lock);
			pHostapdAdapter->prev_tx_packets = 0;
			pHostapdAdapter->prev_rx_packets = 0;
			pHostapdAdapter->prev_fwd_tx_packets = 0;
			pHostapdAdapter->prev_fwd_rx_packets = 0;
			spin_unlock_bh(&pHddCtx->bus_bw_lock);
			hdd_bus_bw_compute_timer_try_stop(pHddCtx);
		}
#endif
		hdd_green_ap_del_sta(pHddCtx);
		break;

	case eSAP_WPS_PBC_PROBE_REQ_EVENT:
	{
		static const char *message =
			"MLMEWPSPBCPROBEREQ.indication";
		union iwreq_data wreq;

		down(&pHddApCtx->semWpsPBCOverlapInd);
		pHddApCtx->WPSPBCProbeReq.probeReqIELen =
			pSapEvent->sapevt.sapPBCProbeReqEvent.
			WPSPBCProbeReq.probeReqIELen;

		qdf_mem_copy(pHddApCtx->WPSPBCProbeReq.probeReqIE,
			     pSapEvent->sapevt.sapPBCProbeReqEvent.
			     WPSPBCProbeReq.probeReqIE,
			     pHddApCtx->WPSPBCProbeReq.probeReqIELen);

		qdf_copy_macaddr(&pHddApCtx->WPSPBCProbeReq.peer_macaddr,
			     &pSapEvent->sapevt.sapPBCProbeReqEvent.
			     WPSPBCProbeReq.peer_macaddr);
		hdd_debug("WPS PBC probe req " MAC_ADDRESS_STR,
		       MAC_ADDR_ARRAY(pHddApCtx->WPSPBCProbeReq.
				      peer_macaddr.bytes));
		memset(&wreq, 0, sizeof(wreq));
		wreq.data.length = strlen(message);
		wireless_send_event(dev, IWEVCUSTOM, &wreq,
				    (char *)message);

		return QDF_STATUS_SUCCESS;
	}
	case eSAP_ASSOC_STA_CALLBACK_EVENT:
		pAssocStasArray =
			pSapEvent->sapevt.sapAssocStaListEvent.pAssocStas;
		if (pSapEvent->sapevt.sapAssocStaListEvent.noOfAssocSta != 0) {
			for (i = 0;
			     i <
			     pSapEvent->sapevt.sapAssocStaListEvent.
			     noOfAssocSta; i++) {
				hdd_notice("Associated Sta Num %d:assocId=%d, staId=%d, staMac="
				       MAC_ADDRESS_STR, i + 1,
				       pAssocStasArray->assocId,
				       pAssocStasArray->staId,
				       MAC_ADDR_ARRAY(pAssocStasArray->staMac.
						      bytes));
				pAssocStasArray++;
			}
		}
		qdf_mem_free(pSapEvent->sapevt.sapAssocStaListEvent.pAssocStas);
		pSapEvent->sapevt.sapAssocStaListEvent.pAssocStas = NULL;
		return QDF_STATUS_SUCCESS;
	case eSAP_REMAIN_CHAN_READY:
		hdd_remain_chan_ready_handler(pHostapdAdapter,
			pSapEvent->sapevt.sap_roc_ind.scan_id);
		return QDF_STATUS_SUCCESS;
	case eSAP_UNKNOWN_STA_JOIN:
		snprintf(unknownSTAEvent, IW_CUSTOM_MAX,
			 "JOIN_UNKNOWN_STA-%02x:%02x:%02x:%02x:%02x:%02x",
			 pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[0],
			 pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[1],
			 pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[2],
			 pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[3],
			 pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[4],
			 pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[5]);
		we_event = IWEVCUSTOM;  /* Discovered a new node (AP mode). */
		wrqu.data.pointer = unknownSTAEvent;
		wrqu.data.length = strlen(unknownSTAEvent);
		we_custom_event_generic = (uint8_t *) unknownSTAEvent;
		hdd_err("%s", unknownSTAEvent);
		break;

	case eSAP_MAX_ASSOC_EXCEEDED:
		snprintf(maxAssocExceededEvent, IW_CUSTOM_MAX,
			 "Peer %02x:%02x:%02x:%02x:%02x:%02x denied"
			 " assoc due to Maximum Mobile Hotspot connections reached. Please disconnect"
			 " one or more devices to enable the new device connection",
			 pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[0],
			 pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[1],
			 pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[2],
			 pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[3],
			 pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[4],
			 pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.
			 bytes[5]);
		we_event = IWEVCUSTOM;  /* Discovered a new node (AP mode). */
		wrqu.data.pointer = maxAssocExceededEvent;
		wrqu.data.length = strlen(maxAssocExceededEvent);
		we_custom_event_generic = (uint8_t *) maxAssocExceededEvent;
		hdd_debug("%s", maxAssocExceededEvent);
		break;
	case eSAP_STA_ASSOC_IND:
		return QDF_STATUS_SUCCESS;

	case eSAP_DISCONNECT_ALL_P2P_CLIENT:
		hdd_debug(" Disconnecting all the P2P Clients....");
		hdd_clear_all_sta(pHostapdAdapter, usrDataForCallback);
		return QDF_STATUS_SUCCESS;

	case eSAP_MAC_TRIG_STOP_BSS_EVENT:
		qdf_status =
			hdd_stop_bss_link(pHostapdAdapter, usrDataForCallback);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			hdd_warn("hdd_stop_bss_link failed %d",
			       qdf_status);
		}
		return QDF_STATUS_SUCCESS;

	case eSAP_CHANNEL_CHANGE_EVENT:
		hdd_debug("Received eSAP_CHANNEL_CHANGE_EVENT event");
		if (pHostapdState->bssState != BSS_STOP) {
			/* Prevent suspend for new channel */
			hdd_hostapd_channel_prevent_suspend(pHostapdAdapter,
				pSapEvent->sapevt.sap_ch_selected.pri_ch);
			/* Allow suspend for old channel */
			hdd_hostapd_channel_allow_suspend(pHostapdAdapter,
				pHddApCtx->operatingChannel);
		}
		/*
		 * SME/PE is already updated for new operation channel.
		 * So update HDD layer also here. This resolves issue
		 * in AP-AP mode where AP1 channel is changed due to RADAR
		 * then CAC is going on and START_BSS on new channel has
		 * not come to HDD. At this case if AP2 is start it needs
		 * current operation channel for MCC DFS restiction
		 */
		pHddApCtx->operatingChannel =
			pSapEvent->sapevt.sap_ch_selected.pri_ch;
		pHddApCtx->sapConfig.acs_cfg.pri_ch =
			pSapEvent->sapevt.sap_ch_selected.pri_ch;
		pHddApCtx->sapConfig.acs_cfg.ht_sec_ch =
			pSapEvent->sapevt.sap_ch_selected.ht_sec_ch;
		pHddApCtx->sapConfig.acs_cfg.vht_seg0_center_ch =
			pSapEvent->sapevt.sap_ch_selected.vht_seg0_center_ch;
		pHddApCtx->sapConfig.acs_cfg.vht_seg1_center_ch =
			pSapEvent->sapevt.sap_ch_selected.vht_seg1_center_ch;
		pHddApCtx->sapConfig.acs_cfg.ch_width =
			pSapEvent->sapevt.sap_ch_selected.ch_width;

		sap_ch_param.ch_width =
			pSapEvent->sapevt.sap_ch_selected.ch_width;
		sap_ch_param.center_freq_seg0 =
			pSapEvent->sapevt.sap_ch_selected.vht_seg0_center_ch;
		sap_ch_param.center_freq_seg1 =
			pSapEvent->sapevt.sap_ch_selected.vht_seg1_center_ch;
		cds_set_channel_params(pSapEvent->sapevt.sap_ch_selected.pri_ch,
			pSapEvent->sapevt.sap_ch_selected.ht_sec_ch,
			&sap_ch_param);
		phy_mode = wlan_sap_get_phymode(
				WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter));
		switch (phy_mode) {
		case eCSR_DOT11_MODE_11n:
		case eCSR_DOT11_MODE_11n_ONLY:
		case eCSR_DOT11_MODE_11ac:
		case eCSR_DOT11_MODE_11ac_ONLY:
			legacy_phymode = false;
			break;
		default:
			legacy_phymode = true;
			break;
		}
		chan_change.chan =
			  pSapEvent->sapevt.sap_ch_selected.pri_ch;
		chan_change.chan_params.ch_width =
			  pSapEvent->sapevt.sap_ch_selected.ch_width;
		chan_change.chan_params.sec_ch_offset =
			  sap_ch_param.sec_ch_offset;
		chan_change.chan_params.center_freq_seg0 =
			  pSapEvent->sapevt.sap_ch_selected.vht_seg0_center_ch;
		chan_change.chan_params.center_freq_seg1 =
			  pSapEvent->sapevt.sap_ch_selected.vht_seg1_center_ch;

		return hdd_chan_change_notify(pHostapdAdapter, dev,
						chan_change, legacy_phymode);

	case eSAP_ACS_SCAN_SUCCESS_EVENT:
		return hdd_handle_acs_scan_event(pSapEvent, pHostapdAdapter);

	case eSAP_DFS_NOL_GET:
		hdd_debug("Received eSAP_DFS_NOL_GET event");

		/* get the dfs nol from PLD */
		ret = pld_wlan_get_dfs_nol(pHddCtx->parent_dev,
					   pSapEvent->sapevt.sapDfsNolInfo.
					   pDfsList,
					   pSapEvent->sapevt.sapDfsNolInfo.
					   sDfsList);

		if (ret > 0) {
			hdd_debug("Get %d bytes of dfs nol from PLD", ret);
			return QDF_STATUS_SUCCESS;
		}
		hdd_debug("No dfs nol entry in PLD, ret: %d", ret);
		return QDF_STATUS_E_FAULT;

	case eSAP_DFS_NOL_SET:
		hdd_debug("Received eSAP_DFS_NOL_SET event");

		/* set the dfs nol to PLD */
		ret = pld_wlan_set_dfs_nol(pHddCtx->parent_dev,
					   pSapEvent->sapevt.sapDfsNolInfo.
					   pDfsList,
					   pSapEvent->sapevt.sapDfsNolInfo.
					   sDfsList);

		if (ret) {
			hdd_debug("Failed to set dfs nol - ret: %d", ret);
		} else {
			hdd_debug(" Set %d bytes dfs nol to PLD",
				 pSapEvent->sapevt.sapDfsNolInfo.sDfsList);
		}
		return QDF_STATUS_SUCCESS;
	case eSAP_ACS_CHANNEL_SELECTED:
		hdd_debug("ACS Completed for wlan%d",
					pHostapdAdapter->dev->ifindex);
		clear_bit(ACS_PENDING, &pHostapdAdapter->event_flags);
		clear_bit(ACS_IN_PROGRESS, &pHddCtx->g_event_flags);
		pHddApCtx->sapConfig.acs_cfg.pri_ch =
			pSapEvent->sapevt.sap_ch_selected.pri_ch;
		pHddApCtx->sapConfig.acs_cfg.ht_sec_ch =
			pSapEvent->sapevt.sap_ch_selected.ht_sec_ch;
		pHddApCtx->sapConfig.acs_cfg.vht_seg0_center_ch =
			pSapEvent->sapevt.sap_ch_selected.vht_seg0_center_ch;
		pHddApCtx->sapConfig.acs_cfg.vht_seg1_center_ch =
			pSapEvent->sapevt.sap_ch_selected.vht_seg1_center_ch;
		pHddApCtx->sapConfig.acs_cfg.ch_width =
			pSapEvent->sapevt.sap_ch_selected.ch_width;
		wlan_hdd_cfg80211_acs_ch_select_evt(pHostapdAdapter);
		qdf_atomic_set(
			&pHostapdAdapter->sessionCtx.ap.acs_in_progress, 0);
		return QDF_STATUS_SUCCESS;
	case eSAP_ECSA_CHANGE_CHAN_IND:
		hdd_debug("Channel change indication from peer for channel %d",
				pSapEvent->sapevt.sap_chan_cng_ind.new_chan);
		if (hdd_softap_set_channel_change(dev,
			 pSapEvent->sapevt.sap_chan_cng_ind.new_chan,
			 CH_WIDTH_MAX))
			return QDF_STATUS_E_FAILURE;
		else
			return QDF_STATUS_SUCCESS;
	case eSAP_UPDATE_SCAN_RESULT:
		return hdd_update_scan_result(pSapEvent,
					pHostapdAdapter);
	case eSAP_STOP_BSS_DUE_TO_NO_CHNL:
		hdd_debug("Stop sap session[%d]",
			  pHostapdAdapter->sessionId);
		INIT_WORK(&pHostapdAdapter->sap_stop_bss_work,
			  hdd_stop_sap_due_to_invalid_channel);
		schedule_work(&pHostapdAdapter->sap_stop_bss_work);
		return QDF_STATUS_SUCCESS;
	default:
		hdd_debug("SAP message is not handled");
		goto stopbss;
		return QDF_STATUS_SUCCESS;
	}
	wireless_send_event(dev, we_event, &wrqu,
			    (char *)we_custom_event_generic);

	return QDF_STATUS_SUCCESS;

stopbss:
	{
		uint8_t we_custom_event[64];
		char *stopBssEvent = "STOP-BSS.response";       /* 17 */
		int event_len = strlen(stopBssEvent);

		hdd_debug("BSS stop status = %s",
		       pSapEvent->sapevt.sapStopBssCompleteEvent.status ?
		       "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS");

		/* Change the BSS state now since,
		 * as we are shutting things down,
		 * we don't want interfaces to become re-enabled
		 */
		pHostapdState->bssState = BSS_STOP;

		if (0 !=
		    (WLAN_HDD_GET_CTX(pHostapdAdapter))->config->
		    nAPAutoShutOff) {
			if (QDF_TIMER_STATE_RUNNING ==
			    pHddApCtx->hdd_ap_inactivity_timer.state) {
				qdf_status =
					qdf_mc_timer_stop(&pHddApCtx->
							  hdd_ap_inactivity_timer);
				if (!QDF_IS_STATUS_SUCCESS(qdf_status))
					hdd_err("Failed to stop AP inactivity timer");
			}

			qdf_status =
				qdf_mc_timer_destroy(&pHddApCtx->
						hdd_ap_inactivity_timer);
			if (!QDF_IS_STATUS_SUCCESS(qdf_status))
				hdd_err("Failed to Destroy AP inactivity timer");
		}
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
		wlan_hdd_auto_shutdown_enable(pHddCtx, true);
#endif

		/* Stop the pkts from n/w stack as we are going to free all of
		 * the TX WMM queues for all STAID's
		 */
		hdd_info("Disabling queues");
		wlan_hdd_netif_queue_control(pHostapdAdapter,
					WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
					WLAN_CONTROL_PATH);

		/* reclaim all resources allocated to the BSS */
		qdf_status = hdd_softap_stop_bss(pHostapdAdapter);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			hdd_warn("hdd_softap_stop_bss failed %d",
			       qdf_status);
			if (hdd_ipa_is_enabled(pHddCtx)) {
				hdd_ipa_uc_disconnect_ap(pHostapdAdapter);
				hdd_ipa_clean_adapter_iface(pHostapdAdapter);
			}
		}

		/* notify userspace that the BSS has stopped */
		memset(&we_custom_event, '\0', sizeof(we_custom_event));
		memcpy(&we_custom_event, stopBssEvent, event_len);
		memset(&wrqu, 0, sizeof(wrqu));
		wrqu.data.length = event_len;
		we_event = IWEVCUSTOM;
		we_custom_event_generic = we_custom_event;
		wireless_send_event(dev, we_event, &wrqu,
				    (char *)we_custom_event_generic);

		/* once the event is set, structure dev/pHostapdAdapter should
		 * not be touched since they are now subject to being deleted
		 * by another thread
		 */
		if (eSAP_STOP_BSS_EVENT == sapEvent)
			qdf_event_set(&pHostapdState->qdf_stop_bss_event);

		cds_dump_concurrency_info();
	}
	return QDF_STATUS_SUCCESS;
}

int hdd_softap_unpack_ie(tHalHandle halHandle,
			 eCsrEncryptionType *pEncryptType,
			 eCsrEncryptionType *mcEncryptType,
			 eCsrAuthType *pAuthType,
			 bool *pMFPCapable,
			 bool *pMFPRequired,
			 uint16_t gen_ie_len, uint8_t *gen_ie)
{
	uint32_t ret;
	uint8_t *pRsnIe;
	uint16_t RSNIeLen;
	tDot11fIERSN dot11RSNIE = {0};
	tDot11fIEWPA dot11WPAIE = {0};

	if (NULL == halHandle) {
		hdd_err("Error haHandle returned NULL");
		return -EINVAL;
	}
	/* Validity checks */
	if ((gen_ie_len < QDF_MIN(DOT11F_IE_RSN_MIN_LEN, DOT11F_IE_WPA_MIN_LEN))
	    || (gen_ie_len >
		QDF_MAX(DOT11F_IE_RSN_MAX_LEN, DOT11F_IE_WPA_MAX_LEN)))
		return -EINVAL;
	/* Type check */
	if (gen_ie[0] == DOT11F_EID_RSN) {
		/* Validity checks */
		if ((gen_ie_len < DOT11F_IE_RSN_MIN_LEN) ||
		    (gen_ie_len > DOT11F_IE_RSN_MAX_LEN)) {
			return QDF_STATUS_E_FAILURE;
		}
		/* Skip past the EID byte and length byte */
		pRsnIe = gen_ie + 2;
		RSNIeLen = gen_ie_len - 2;
		/* Unpack the RSN IE */
		memset(&dot11RSNIE, 0, sizeof(tDot11fIERSN));
		ret = sme_unpack_rsn_ie(halHandle, pRsnIe, RSNIeLen,
					&dot11RSNIE, false);
		if (!DOT11F_SUCCEEDED(ret)) {
			hdd_err("unpack failed, ret: 0x%x", ret);
			return -EINVAL;
		}
		/* Copy out the encryption and authentication types */
		hdd_debug("pairwise cipher suite count: %d",
		       dot11RSNIE.pwise_cipher_suite_count);
		hdd_debug("authentication suite count: %d",
		       dot11RSNIE.akm_suite_cnt);
		/* Here we have followed the apple base code,
		 * but probably I suspect we can do something different
		 * dot11RSNIE.akm_suite_cnt
		 * Just translate the FIRST one
		 */
		*pAuthType =
		    hdd_translate_rsn_to_csr_auth_type(dot11RSNIE.akm_suite[0]);
		/* dot11RSNIE.pwise_cipher_suite_count */
		*pEncryptType =
			hdd_translate_rsn_to_csr_encryption_type(dot11RSNIE.
								 pwise_cipher_suites[0]);
		/* dot11RSNIE.gp_cipher_suite_count */
		*mcEncryptType =
			hdd_translate_rsn_to_csr_encryption_type(dot11RSNIE.
								 gp_cipher_suite);
		/* Set the PMKSA ID Cache for this interface */
		*pMFPCapable = 0 != (dot11RSNIE.RSN_Cap[0] & 0x80);
		*pMFPRequired = 0 != (dot11RSNIE.RSN_Cap[0] & 0x40);
	} else if (gen_ie[0] == DOT11F_EID_WPA) {
		/* Validity checks */
		if ((gen_ie_len < DOT11F_IE_WPA_MIN_LEN) ||
		    (gen_ie_len > DOT11F_IE_WPA_MAX_LEN)) {
			return QDF_STATUS_E_FAILURE;
		}
		/* Skip past the EID byte and length byte and 4 byte WiFi OUI */
		pRsnIe = gen_ie + 2 + 4;
		RSNIeLen = gen_ie_len - (2 + 4);
		/* Unpack the WPA IE */
		memset(&dot11WPAIE, 0, sizeof(tDot11fIEWPA));
		ret = dot11f_unpack_ie_wpa((tpAniSirGlobal) halHandle,
				     pRsnIe, RSNIeLen, &dot11WPAIE, false);
		if (DOT11F_FAILED(ret)) {
			hdd_err("unpack failed, ret: 0x%x", ret);
			return -EINVAL;
		}
		/* Copy out the encryption and authentication types */
		hdd_debug("WPA unicast cipher suite count: %d",
		       dot11WPAIE.unicast_cipher_count);
		hdd_debug("WPA authentication suite count: %d",
		       dot11WPAIE.auth_suite_count);
		/* dot11WPAIE.auth_suite_count */
		/* Just translate the FIRST one */
		*pAuthType =
			hdd_translate_wpa_to_csr_auth_type(dot11WPAIE.auth_suites[0]);
		/* dot11WPAIE.unicast_cipher_count */
		*pEncryptType =
			hdd_translate_wpa_to_csr_encryption_type(dot11WPAIE.
								 unicast_ciphers[0]);
		/* dot11WPAIE.unicast_cipher_count */
		*mcEncryptType =
			hdd_translate_wpa_to_csr_encryption_type(dot11WPAIE.
								 multicast_cipher);
		*pMFPCapable = false;
		*pMFPRequired = false;
	} else {
		hdd_err("gen_ie[0]: %d", gen_ie[0]);
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_softap_set_channel_change() -
 * This function to support SAP channel change with CSA IE
 * set in the beacons.
 *
 * @dev: pointer to the net device.
 * @target_channel: target channel number.
 * @target_bw: Target bandwidth to move.
 * If no bandwidth is specified, the value is CH_WIDTH_MAX
 *
 * Return: 0 for success, non zero for failure
 */
int hdd_softap_set_channel_change(struct net_device *dev, int target_channel,
				 enum phy_ch_width target_bw)
{
	QDF_STATUS status;
	int ret = 0;
	hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
	hdd_context_t *pHddCtx = NULL;
	hdd_adapter_t *sta_adapter;
	hdd_station_ctx_t *sta_ctx;
	cds_context_type *cds_ctx;

	pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(pHddCtx);
	if (ret)
		return ret;

	ret = hdd_validate_channel_and_bandwidth(pHostapdAdapter,
						target_channel, target_bw);
	if (ret) {
		hdd_err("Invalid CH and BW combo");
		return ret;
	}

	sta_adapter = hdd_get_adapter(pHddCtx, QDF_STA_MODE);
	/*
	 * conc_custom_rule1:
	 * Force SCC for SAP + STA
	 * if STA is already connected then we shouldn't allow
	 * channel switch in SAP interface.
	 */
	if (sta_adapter && pHddCtx->config->conc_custom_rule1) {
		sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(sta_adapter);
		if (hdd_conn_is_connected(sta_ctx)) {
			hdd_err("Channel switch not allowed after STA connection with conc_custom_rule1 enabled");
			return -EBUSY;
		}
	}
	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_FATAL,
				"%s: Trying to open CDS without a PreOpen",
				__func__);
		return -EINVAL;
	}
	/*
	 * Set the dfs_radar_found flag to mimic channel change
	 * when a radar is found. This will enable synchronizing
	 * SAP and HDD states similar to that of radar indication.
	 * Suspend the netif queues to stop queuing Tx frames
	 * from upper layers.  netif queues will be resumed
	 * once the channel change is completed and SAP will
	 * post eSAP_START_BSS_EVENT success event to HDD.
	 */
	if (qdf_atomic_inc_return(&pHddCtx->dfs_radar_found) > 1) {
		hdd_err("Channel switch in progress!!");
		return -EBUSY;
	}

	status = qdf_event_reset(&cds_ctx->channel_switch_complete);

	if (!QDF_IS_STATUS_SUCCESS(status)) {
		cds_err("clear event failed");
		return -EINVAL;
	}
	/*
	 * Post the Channel Change request to SAP.
	 */
	status = wlansap_set_channel_change_with_csa(
		WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
		(uint32_t)target_channel,
		target_bw, !(pHddCtx->config->sta_sap_scc_on_lte_coex_chan));

	if (QDF_STATUS_SUCCESS != status) {
		hdd_err("SAP set channel failed for channel: %d, bw: %d",
		       target_channel, target_bw);
		/*
		 * If channel change command fails then clear the
		 * radar found flag and also restart the netif
		 * queues.
		 */
		qdf_atomic_set(&pHddCtx->dfs_radar_found, 0);

		ret = -EINVAL;
	}

	return ret;
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
/**
 * hdd_sap_restart_with_channel_switch() - SAP channel change with E/CSA
 * @ap_adapter: HDD adapter
 * @target_channel: Channel to which switch must happen
 * @target_bw: Bandwidth of the target channel
 *
 * Invokes the necessary API to perform channel switch for the SAP or GO
 *
 * Return: None
 */
void hdd_sap_restart_with_channel_switch(hdd_adapter_t *ap_adapter,
					uint32_t target_channel,
					uint32_t target_bw)
{
	struct net_device *dev = ap_adapter->dev;
	int ret;

	ENTER();

	if (!dev) {
		hdd_err("Invalid dev pointer");
		return;
	}

	ret = hdd_softap_set_channel_change(dev, target_channel, target_bw);
	if (ret) {
		hdd_err("channel switch failed");
		return;
	}
}
#endif

static int __iw_softap_set_ini_cfg(struct net_device *dev,
				   struct iw_request_info *info,
				   union iwreq_data *wrqu,
				   char *extra)
{
	QDF_STATUS status;
	int errno;
	hdd_adapter_t *adapter;
	hdd_context_t *hdd_ctx;
	char *value;
	size_t len;

	ENTER_DEV(dev);

	adapter = netdev_priv(dev);
	errno = hdd_validate_adapter(adapter);
	if (errno)
		return errno;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	/* ensure null termination */
	len = min_t(size_t, wrqu->data.length, QCSAP_IOCTL_MAX_STR_LEN);
	value = qdf_mem_malloc(len + 1);
	if (!value)
		return -ENOMEM;

	qdf_mem_copy(value, extra, len);
	hdd_debug("Received data %s", value);
	status = hdd_execute_global_config_command(hdd_ctx, value);
	qdf_mem_free(value);

	EXIT();

	return qdf_status_to_os_return(status);
}

int
static iw_softap_set_ini_cfg(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_set_ini_cfg(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

static int hdd_sap_get_chan_width(hdd_adapter_t *adapter, int *value)
{
	void *cds_ctx;
	hdd_hostapd_state_t *hostapdstate;

	ENTER();
	hostapdstate = WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);

	if (hostapdstate->bssState != BSS_START) {
		*value = -EINVAL;
		return -EINVAL;
	}

	cds_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter);

	*value = wlansap_get_chan_width(cds_ctx);
	hdd_debug("chan_width = %d", *value);

	return 0;
}

int
static __iw_softap_get_ini_cfg(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx;
	int ret = 0;

	ENTER_DEV(dev);

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	ret = wlan_hdd_validate_context(pHddCtx);
	if (ret != 0)
		return ret;

	hdd_debug("Printing CLD global INI Config");
	hdd_cfg_get_global_config(pHddCtx, extra, QCSAP_IOCTL_MAX_STR_LEN);
	wrqu->data.length = strlen(extra) + 1;

	return 0;
}

int
static iw_softap_get_ini_cfg(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_get_ini_cfg(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * iw_softap_set_two_ints_getnone() - Generic "set two integer" ioctl handler
 * @dev: device upon which the ioctl was received
 * @info: ioctl request information
 * @wrqu: ioctl request data
 * @extra: ioctl extra data
 *
 * Return: 0 on success, non-zero on error
 */
static int __iw_softap_set_two_ints_getnone(struct net_device *dev,
					    struct iw_request_info *info,
					    union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	int ret;
	int *value = (int *)extra;
	int sub_cmd = value[0];
	hdd_context_t *hdd_ctx;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		goto out;

	switch (sub_cmd) {
#ifdef WLAN_DEBUG
	case QCSAP_IOCTL_SET_FW_CRASH_INJECT:
		hdd_err("WE_SET_FW_CRASH_INJECT: %d %d",
		       value[1], value[2]);
		if (!hdd_ctx->config->crash_inject_enabled) {
			hdd_err("Crash Inject ini disabled");
			return 0;
		}
		ret = wma_cli_set2_command(adapter->sessionId,
					   GEN_PARAM_CRASH_INJECT,
					   value[1], value[2],
					   GEN_CMD);
		break;
#endif
	case QCSAP_IOCTL_DUMP_DP_TRACE_LEVEL:
		hdd_debug("WE_DUMP_DP_TRACE: %d %d",
		       value[1], value[2]);
		if (value[1] == DUMP_DP_TRACE)
			qdf_dp_trace_dump_all(value[2]);
		else if (value[1] == ENABLE_DP_TRACE_LIVE_MODE)
			qdf_dp_trace_enable_live_mode();
		else if (value[1] == CLEAR_DP_TRACE_BUFFER)
			qdf_dp_trace_clear_buffer();
		else if (value[1] == DISABLE_DP_TRACE_LIVE_MODE)
			qdf_dp_trace_disable_live_mode();
		break;
	case QCSAP_ENABLE_FW_PROFILE:
		hdd_debug("QCSAP_ENABLE_FW_PROFILE: %d %d",
		       value[1], value[2]);
		ret = wma_cli_set2_command(adapter->sessionId,
				 WMI_WLAN_PROFILE_ENABLE_PROFILE_ID_CMDID,
					value[1], value[2], DBG_CMD);
		break;
	case QCSAP_SET_FW_PROFILE_HIST_INTVL:
		hdd_debug("QCSAP_SET_FW_PROFILE_HIST_INTVL: %d %d",
		       value[1], value[2]);
		ret = wma_cli_set2_command(adapter->sessionId,
					WMI_WLAN_PROFILE_SET_HIST_INTVL_CMDID,
					value[1], value[2], DBG_CMD);
	default:
		hdd_err("Invalid IOCTL command: %d", sub_cmd);
		break;
	}

out:
	return ret;
}

static int iw_softap_set_two_ints_getnone(struct net_device *dev,
					  struct iw_request_info *info,
					  union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_set_two_ints_getnone(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

static void print_mac_list(struct qdf_mac_addr *macList, uint8_t size)
{
	int i;
	uint8_t *macArray;

	for (i = 0; i < size; i++) {
		macArray = (macList + i)->bytes;
		pr_info("ACL entry %i - %02x:%02x:%02x:%02x:%02x:%02x\n",
			i, MAC_ADDR_ARRAY(macArray));
	}
}

static QDF_STATUS hdd_print_acl(hdd_adapter_t *pHostapdAdapter)
{
	eSapMacAddrACL acl_mode;
	struct qdf_mac_addr MacList[MAX_ACL_MAC_ADDRESS];
	uint8_t listnum;
	void *p_cds_gctx = NULL;

	p_cds_gctx = WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter);
	qdf_mem_zero(&MacList[0], sizeof(MacList));
	if (QDF_STATUS_SUCCESS == wlansap_get_acl_mode(p_cds_gctx, &acl_mode)) {
		pr_info("******** ACL MODE *********\n");
		switch (acl_mode) {
		case eSAP_ACCEPT_UNLESS_DENIED:
			pr_info("ACL Mode = ACCEPT_UNLESS_DENIED\n");
			break;
		case eSAP_DENY_UNLESS_ACCEPTED:
			pr_info("ACL Mode = DENY_UNLESS_ACCEPTED\n");
			break;
		case eSAP_SUPPORT_ACCEPT_AND_DENY:
			pr_info("ACL Mode = ACCEPT_AND_DENY\n");
			break;
		case eSAP_ALLOW_ALL:
			pr_info("ACL Mode = ALLOW_ALL\n");
			break;
		default:
			pr_info("Invalid SAP ACL Mode = %d\n", acl_mode);
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		return QDF_STATUS_E_FAILURE;
	}

	if (QDF_STATUS_SUCCESS == wlansap_get_acl_accept_list(p_cds_gctx,
							      &MacList[0],
							      &listnum)) {
		pr_info("******* WHITE LIST ***********\n");
		if (listnum <= MAX_ACL_MAC_ADDRESS)
			print_mac_list(&MacList[0], listnum);
	} else {
		return QDF_STATUS_E_FAILURE;
	}

	if (QDF_STATUS_SUCCESS == wlansap_get_acl_deny_list(p_cds_gctx,
							    &MacList[0],
							    &listnum)) {
		pr_info("******* BLACK LIST ***********\n");
		if (listnum <= MAX_ACL_MAC_ADDRESS)
			print_mac_list(&MacList[0], listnum);
	} else {
		return QDF_STATUS_E_FAILURE;
	}
	return QDF_STATUS_SUCCESS;
}

int
static __iw_softap_setparam(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
	tHalHandle hHal;
	int *value = (int *)extra;
	int sub_cmd = value[0];
	int set_value = value[1];
	QDF_STATUS status;
	int ret = 0;
	hdd_context_t *hdd_ctx;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return -EINVAL;

	hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
	if (!hHal) {
		hdd_err("Hal ctx is null");
		return -EINVAL;
	}

	switch (sub_cmd) {
	case QCASAP_SET_RADAR_DBG:
		hdd_debug("QCASAP_SET_RADAR_DBG called with: value: %d",
		       set_value);
		wlan_sap_enable_phy_error_logs(hHal, (bool) set_value);
		break;

	case QCSAP_PARAM_CLR_ACL:
		if (QDF_STATUS_SUCCESS != wlansap_clear_acl(
		    WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter))) {
			ret = -EIO;
		}
		break;

	case QCSAP_PARAM_ACL_MODE:
		if ((eSAP_ALLOW_ALL < (eSapMacAddrACL) set_value) ||
		    (eSAP_ACCEPT_UNLESS_DENIED > (eSapMacAddrACL) set_value)) {
			hdd_err("Invalid ACL Mode value: %d", set_value);
			ret = -EINVAL;
		} else {
			wlansap_set_mode(
				WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
				set_value);
		}
		break;

	case QCSAP_PARAM_SET_CHANNEL_CHANGE:
		if ((QDF_SAP_MODE == pHostapdAdapter->device_mode) ||
		   (QDF_P2P_GO_MODE == pHostapdAdapter->device_mode)) {
			hdd_debug("SET Channel Change to new channel= %d",
			       set_value);
			ret = hdd_softap_set_channel_change(dev, set_value,
								CH_WIDTH_MAX);
		} else {
			hdd_err("Channel Change Failed, Device in test mode");
			ret = -EINVAL;
		}
		break;
	case QCSAP_PARAM_CONC_SYSTEM_PREF:
		hdd_debug("New preference: %d", set_value);
		if (!((set_value >= CFG_CONC_SYSTEM_PREF_MIN) &&
				(set_value <= CFG_CONC_SYSTEM_PREF_MAX))) {
			hdd_err("Invalid system preference: %d", set_value);
			return -EINVAL;
		}
		cds_set_cur_conc_system_pref(set_value);
		break;
	case QCSAP_PARAM_MAX_ASSOC:
		if (WNI_CFG_ASSOC_STA_LIMIT_STAMIN > set_value) {
			hdd_err("Invalid setMaxAssoc value %d",
			       set_value);
			ret = -EINVAL;
		} else {
			if (WNI_CFG_ASSOC_STA_LIMIT_STAMAX < set_value) {
				hdd_warn("setMaxAssoc %d > max allowed %d.",
				       set_value,
				       WNI_CFG_ASSOC_STA_LIMIT_STAMAX);
				hdd_warn("Setting it to max allowed and continuing");
				set_value = WNI_CFG_ASSOC_STA_LIMIT_STAMAX;
			}
			status = sme_cfg_set_int(hHal, WNI_CFG_ASSOC_STA_LIMIT,
					set_value);
			if (status != QDF_STATUS_SUCCESS) {
				hdd_err("setMaxAssoc failure, status: %d",
				       status);
				ret = -EIO;
			}
		}
		break;

	case QCSAP_PARAM_HIDE_SSID:
	{
		QDF_STATUS status;

		status = sme_update_session_param(hHal,
				pHostapdAdapter->sessionId,
				SIR_PARAM_SSID_HIDDEN, set_value);
		if (QDF_STATUS_SUCCESS != status) {
			hdd_err("QCSAP_PARAM_HIDE_SSID failed");
			return -EIO;
		}
		break;
	}
	case QCSAP_PARAM_SET_MC_RATE:
	{
		tSirRateUpdateInd rateUpdate = {0};
		struct hdd_config *pConfig = hdd_ctx->config;

		hdd_debug("MC Target rate %d", set_value);
		qdf_copy_macaddr(&rateUpdate.bssid,
				 &pHostapdAdapter->macAddressCurrent);
		rateUpdate.nss = (pConfig->enable2x2 == 0) ? 0 : 1;
		rateUpdate.dev_mode = pHostapdAdapter->device_mode;
		rateUpdate.mcastDataRate24GHz = set_value;
		rateUpdate.mcastDataRate24GHzTxFlag = 1;
		rateUpdate.mcastDataRate5GHz = set_value;
		rateUpdate.bcastDataRate = -1;
		status = sme_send_rate_update_ind(hHal, &rateUpdate);
		if (QDF_STATUS_SUCCESS != status) {
			hdd_err("SET_MC_RATE failed");
			ret = -1;
		}
		break;
	}

	case QCSAP_PARAM_SET_TXRX_FW_STATS:
	{
		hdd_debug("QCSAP_PARAM_SET_TXRX_FW_STATS val %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMA_VDEV_TXRX_FWSTATS_ENABLE_CMDID,
					  set_value, VDEV_CMD);
		break;
	}
	/* Firmware debug log */
	case QCSAP_DBGLOG_LOG_LEVEL:
	{
		hdd_debug("QCSAP_DBGLOG_LOG_LEVEL val %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_DBGLOG_LOG_LEVEL,
					  set_value, DBG_CMD);
		break;
	}

	case QCSAP_DBGLOG_VAP_ENABLE:
	{
		hdd_debug("QCSAP_DBGLOG_VAP_ENABLE val %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_DBGLOG_VAP_ENABLE,
					  set_value, DBG_CMD);
		break;
	}

	case QCSAP_DBGLOG_VAP_DISABLE:
	{
		hdd_debug("QCSAP_DBGLOG_VAP_DISABLE val %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_DBGLOG_VAP_DISABLE,
					  set_value, DBG_CMD);
		break;
	}

	case QCSAP_DBGLOG_MODULE_ENABLE:
	{
		hdd_debug("QCSAP_DBGLOG_MODULE_ENABLE val %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_DBGLOG_MODULE_ENABLE,
					  set_value, DBG_CMD);
		break;
	}

	case QCSAP_DBGLOG_MODULE_DISABLE:
	{
		hdd_debug("QCSAP_DBGLOG_MODULE_DISABLE val %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_DBGLOG_MODULE_DISABLE,
					  set_value, DBG_CMD);
		break;
	}

	case QCSAP_DBGLOG_MOD_LOG_LEVEL:
	{
		hdd_debug("QCSAP_DBGLOG_MOD_LOG_LEVEL val %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_DBGLOG_MOD_LOG_LEVEL,
					  set_value, DBG_CMD);
		break;
	}

	case QCSAP_DBGLOG_TYPE:
	{
		hdd_debug("QCSAP_DBGLOG_TYPE val %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_DBGLOG_TYPE,
					  set_value, DBG_CMD);
		break;
	}
	case QCSAP_DBGLOG_REPORT_ENABLE:
	{
		hdd_debug("QCSAP_DBGLOG_REPORT_ENABLE val %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_DBGLOG_REPORT_ENABLE,
					  set_value, DBG_CMD);
		break;
	}
	case QCSAP_PARAM_SET_MCC_CHANNEL_LATENCY:
	{
		cds_set_mcc_latency(pHostapdAdapter, set_value);
		break;
	}

	case QCSAP_PARAM_SET_MCC_CHANNEL_QUOTA:
	{
		hdd_debug("iwpriv cmd to set MCC quota value %dms",
		       set_value);
		ret = cds_go_set_mcc_p2p_quota(pHostapdAdapter,
						    set_value);
		break;
	}

	case QCASAP_TXRX_FWSTATS_RESET:
	{
		hdd_debug("WE_TXRX_FWSTATS_RESET val %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMA_VDEV_TXRX_FWSTATS_RESET_CMDID,
					  set_value, VDEV_CMD);
		break;
	}

	case QCSAP_PARAM_RTSCTS:
	{
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_VDEV_PARAM_ENABLE_RTSCTS,
					  set_value, VDEV_CMD);
		if (ret) {
			hdd_err("FAILED TO SET RTSCTS at SAP");
			ret = -EIO;
		}
		break;
	}
	case QCASAP_SET_11N_RATE:
	{
		uint8_t preamble = 0, nss = 0, rix = 0;
		tsap_Config_t *pConfig =
			&pHostapdAdapter->sessionCtx.ap.sapConfig;

		hdd_debug("SET_HT_RATE val %d", set_value);

		if (set_value != 0xff) {
			rix = RC_2_RATE_IDX(set_value);
			if (set_value & 0x80) {
				if (pConfig->SapHw_mode ==
				    eCSR_DOT11_MODE_11b
				    || pConfig->SapHw_mode ==
				    eCSR_DOT11_MODE_11b_ONLY
				    || pConfig->SapHw_mode ==
				    eCSR_DOT11_MODE_11g
				    || pConfig->SapHw_mode ==
				    eCSR_DOT11_MODE_11g_ONLY
				    || pConfig->SapHw_mode ==
				    eCSR_DOT11_MODE_abg
				    || pConfig->SapHw_mode ==
				    eCSR_DOT11_MODE_11a) {
					hdd_err("Not valid mode for HT");
					ret = -EIO;
					break;
				}
				preamble = WMI_RATE_PREAMBLE_HT;
				nss = HT_RC_2_STREAMS(set_value) - 1;
			} else if (set_value & 0x10) {
				if (pConfig->SapHw_mode ==
				    eCSR_DOT11_MODE_11a) {
					hdd_err("Not valid for cck");
					ret = -EIO;
					break;
				}
				preamble = WMI_RATE_PREAMBLE_CCK;
				/* Enable Short preamble always
				 * for CCK except 1mbps
				 */
				if (rix != 0x3)
					rix |= 0x4;
			} else {
				if (pConfig->SapHw_mode ==
				    eCSR_DOT11_MODE_11b
				    || pConfig->SapHw_mode ==
				    eCSR_DOT11_MODE_11b_ONLY) {
					hdd_err("Not valid for OFDM");
					ret = -EIO;
					break;
				}
				preamble = WMI_RATE_PREAMBLE_OFDM;
			}
			set_value = (preamble << 6) | (nss << 4) | rix;
		}
		hdd_debug("SET_HT_RATE val %d rix %d preamble %x nss %d",
		       set_value, rix, preamble, nss);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_VDEV_PARAM_FIXED_RATE,
					  set_value, VDEV_CMD);
		break;
	}

	case QCASAP_SET_VHT_RATE:
	{
		uint8_t preamble = 0, nss = 0, rix = 0;
		tsap_Config_t *pConfig =
			&pHostapdAdapter->sessionCtx.ap.sapConfig;

		if (pConfig->SapHw_mode != eCSR_DOT11_MODE_11ac &&
		    pConfig->SapHw_mode != eCSR_DOT11_MODE_11ac_ONLY) {
			hdd_err("SET_VHT_RATE error: SapHw_mode= 0x%x, ch: %d",
			       pConfig->SapHw_mode, pConfig->channel);
			ret = -EIO;
			break;
		}

		if (set_value != 0xff) {
		rix = RC_2_RATE_IDX_11AC(set_value);
		preamble = WMI_RATE_PREAMBLE_VHT;
		nss = HT_RC_2_STREAMS_11AC(set_value) - 1;

			set_value = (preamble << 6) | (nss << 4) | rix;
		}
		hdd_debug("SET_VHT_RATE val %d rix %d preamble %x nss %d",
		       set_value, rix, preamble, nss);

		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_VDEV_PARAM_FIXED_RATE,
					  set_value, VDEV_CMD);
		break;
	}

	case QCASAP_SHORT_GI:
	{
		hdd_debug("QCASAP_SET_SHORT_GI val %d", set_value);

		/* same as 40MHZ */
		ret = sme_update_ht_config(hHal, pHostapdAdapter->sessionId,
					   WNI_CFG_HT_CAP_INFO_SHORT_GI_20MHZ,
					   set_value);
		if (ret)
			hdd_err("Failed to set ShortGI value ret: %d", ret);
		break;
	}

	case QCSAP_SET_AMPDU:
	{
		hdd_debug("QCSAP_SET_AMPDU %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  GEN_VDEV_PARAM_AMPDU,
					  set_value, GEN_CMD);
		break;
	}

	case QCSAP_SET_AMSDU:
	{
		hdd_debug("QCSAP_SET_AMSDU %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  GEN_VDEV_PARAM_AMSDU,
					  set_value, GEN_CMD);
		break;
	}
	case QCSAP_GTX_HT_MCS:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_HT_MCS %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_VDEV_PARAM_GTX_HT_MCS,
					  set_value, GTX_CMD);
		break;
	}

	case QCSAP_GTX_VHT_MCS:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_VHT_MCS %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_VDEV_PARAM_GTX_VHT_MCS,
						set_value, GTX_CMD);
		break;
	}

	case QCSAP_GTX_USRCFG:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_USR_CFG %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_VDEV_PARAM_GTX_USR_CFG,
					  set_value, GTX_CMD);
		break;
	}

	case QCSAP_GTX_THRE:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_THRE %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_VDEV_PARAM_GTX_THRE,
					  set_value, GTX_CMD);
		break;
	}

	case QCSAP_GTX_MARGIN:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_MARGIN %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_VDEV_PARAM_GTX_MARGIN,
					  set_value, GTX_CMD);
		break;
	}

	case QCSAP_GTX_STEP:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_STEP %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_VDEV_PARAM_GTX_STEP,
					  set_value, GTX_CMD);
		break;
	}

	case QCSAP_GTX_MINTPC:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_MINTPC %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_VDEV_PARAM_GTX_MINTPC,
					  set_value, GTX_CMD);
		break;
	}

	case QCSAP_GTX_BWMASK:
	{
		hdd_debug("WMI_VDEV_PARAM_GTX_BWMASK %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_VDEV_PARAM_GTX_BW_MASK,
					  set_value, GTX_CMD);
		break;
	}

	case QCASAP_SET_TM_LEVEL:
	{
		hdd_debug("Set Thermal Mitigation Level %d", set_value);
		(void)sme_set_thermal_level(hHal, set_value);
		break;
	}

	case QCASAP_SET_DFS_IGNORE_CAC:
	{
		hdd_debug("Set Dfs ignore CAC  %d", set_value);

		if (pHostapdAdapter->device_mode != QDF_SAP_MODE)
			return -EINVAL;

		ret = wlansap_set_dfs_ignore_cac(hHal, set_value);
		break;
	}

	case QCASAP_SET_DFS_TARGET_CHNL:
	{
		hdd_debug("Set Dfs target channel  %d", set_value);

		if (pHostapdAdapter->device_mode != QDF_SAP_MODE)
			return -EINVAL;

		ret = wlansap_set_dfs_target_chnl(hHal, set_value);
		break;
	}

	case QCASAP_SET_DFS_NOL:
		wlansap_set_dfs_nol(
			WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
			(eSapDfsNolType) set_value);
		break;

	case QCASAP_SET_RADAR_CMD:
	{
		hdd_context_t *pHddCtx =
			WLAN_HDD_GET_CTX(pHostapdAdapter);
		uint8_t ch =
			(WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->
			operatingChannel;
		bool isDfsch;
		int32_t dfs_radar_found;

		isDfsch = (CHANNEL_STATE_DFS ==
			   cds_get_channel_state(ch));

		hdd_debug("Set QCASAP_SET_RADAR_CMD val %d", set_value);

		dfs_radar_found = qdf_atomic_read(&pHddCtx->dfs_radar_found);
		if (!dfs_radar_found && isDfsch) {
			ret = wma_cli_set_command(pHostapdAdapter->sessionId,
						  WMA_VDEV_DFS_CONTROL_CMDID,
						  set_value, VDEV_CMD);
		} else {
			hdd_err("Ignore, radar_found: %d,  dfs_channel: %d",
				dfs_radar_found, isDfsch);
		}
		break;
	}
	case QCASAP_TX_CHAINMASK_CMD:
	{
		hdd_debug("QCASAP_TX_CHAINMASK_CMD val %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_PDEV_PARAM_TX_CHAIN_MASK,
					  set_value, PDEV_CMD);
		ret = hdd_set_antenna_mode(pHostapdAdapter, hdd_ctx, set_value);
		break;
	}

	case QCASAP_RX_CHAINMASK_CMD:
	{
		hdd_debug("QCASAP_RX_CHAINMASK_CMD val %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_PDEV_PARAM_RX_CHAIN_MASK,
					  set_value, PDEV_CMD);
		ret = hdd_set_antenna_mode(pHostapdAdapter, hdd_ctx, set_value);
		break;
	}

	case QCASAP_NSS_CMD:
	{
		hdd_debug("QCASAP_NSS_CMD val %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					  WMI_VDEV_PARAM_NSS,
					  set_value, VDEV_CMD);
		break;
	}

	case QCSAP_IPA_UC_STAT:
	{
		/* If input value is non-zero get stats */
		switch (set_value) {
		case 1:
			hdd_ipa_uc_stat(pHostapdAdapter);
			break;
		case 2:
			hdd_ipa_uc_info(hdd_ctx);
			break;
		case 3:
			hdd_ipa_uc_rt_debug_host_dump(hdd_ctx);
			break;
		case 4:
			hdd_ipa_dump_info(hdd_ctx);
			break;
		default:
			/* place holder for stats clean up
			 * Stats clean not implemented yet on FW and IPA
			 */
			break;
		}
		return ret;
	}

	case QCASAP_SET_PHYMODE:
	{
		hdd_context_t *phddctx =
			WLAN_HDD_GET_CTX(pHostapdAdapter);

		ret =
			wlan_hdd_update_phymode(dev, hHal, set_value,
						phddctx);
		break;
	}
	case QCASAP_DUMP_STATS:
	{
		hdd_debug("QCASAP_DUMP_STATS val %d", set_value);
		ret = hdd_wlan_dump_stats(pHostapdAdapter, set_value);
		break;
	}
	case QCASAP_CLEAR_STATS:
	{
		hdd_context_t *hdd_ctx =
			WLAN_HDD_GET_CTX(pHostapdAdapter);
		hdd_debug("QCASAP_CLEAR_STATS val %d", set_value);
		switch (set_value) {
		case WLAN_HDD_STATS:
			memset(&pHostapdAdapter->stats, 0,
						sizeof(pHostapdAdapter->stats));
			memset(&pHostapdAdapter->hdd_stats, 0,
					sizeof(pHostapdAdapter->hdd_stats));
			break;
		case WLAN_TXRX_HIST_STATS:
			wlan_hdd_clear_tx_rx_histogram(hdd_ctx);
			break;
		case WLAN_HDD_NETIF_OPER_HISTORY:
			wlan_hdd_clear_netif_queue_history(hdd_ctx);
			break;
		case WLAN_HIF_STATS:
			hdd_clear_hif_stats();
			break;
		default:
			if (ol_txrx_clear_stats(set_value) ==
						QDF_STATUS_E_INVAL) {
				hdd_display_stats_help();
				ret = EINVAL;
			}
		}
		break;
	}
	case QCSAP_START_FW_PROFILING:
		hdd_debug("QCSAP_START_FW_PROFILING %d", set_value);
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
					WMI_WLAN_PROFILE_TRIGGER_CMDID,
					set_value, DBG_CMD);
		break;
	case QCASAP_PARAM_LDPC:
		ret = hdd_set_ldpc(pHostapdAdapter, set_value);
		break;
	case QCASAP_PARAM_TX_STBC:
		ret = hdd_set_tx_stbc(pHostapdAdapter, set_value);
		break;
	case QCASAP_PARAM_RX_STBC:
		ret = hdd_set_rx_stbc(pHostapdAdapter, set_value);
		break;
	case QCSAP_SET_DEFAULT_AMPDU:
		hdd_debug("QCSAP_SET_DEFAULT_AMPDU val %d", set_value);
		ret = wma_cli_set_command((int)pHostapdAdapter->sessionId,
					(int)WMI_PDEV_PARAM_MAX_MPDUS_IN_AMPDU,
					set_value, PDEV_CMD);
		break;

	case QCSAP_ENABLE_RTS_BURSTING:
		hdd_debug("QCSAP_ENABLE_RTS_BURSTING val %d", set_value);
		ret = wma_cli_set_command((int)pHostapdAdapter->sessionId,
					(int)WMI_PDEV_PARAM_ENABLE_RTS_SIFS_BURSTING,
					set_value, PDEV_CMD);
		break;
	default:
		hdd_err("Invalid setparam command %d value %d",
		       sub_cmd, set_value);
		ret = -EINVAL;
		break;
	}
	EXIT();
	return ret;
}

/**
 * __iw_softap_get_three() - return three value to upper layer.
 * @dev: pointer of net_device of this wireless card
 * @info: meta data about Request sent
 * @wrqu: include request info
 * @extra: buf used for in/out
 *
 * Return: execute result
 */
static int __iw_softap_get_three(struct net_device *dev,
					struct iw_request_info *info,
					union iwreq_data *wrqu, char *extra)
{
	uint32_t *value = (uint32_t *)extra;
	uint32_t sub_cmd = value[0];
	int ret = 0; /* success */
	struct hdd_context_s *hdd_ctx;
	struct hdd_adapter_s *adapter = WLAN_HDD_GET_PRIV_PTR(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	switch (sub_cmd) {
	case QCSAP_GET_TSF:
		ret = hdd_indicate_tsf(adapter, value, 3);
		break;
	default:
		hdd_err("Invalid getparam command: %d", sub_cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}


/**
 * iw_softap_get_three() - return three value to upper layer.
 *
 * @dev: pointer of net_device of this wireless card
 * @info: meta data about Request sent
 * @wrqu: include request info
 * @extra: buf used for in/Output
 *
 * Return: execute result
 */
static int iw_softap_get_three(struct net_device *dev,
					struct iw_request_info *info,
					union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_get_three(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

int
static iw_softap_setparam(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_setparam(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

int
static __iw_softap_getparam(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
	int *value = (int *)extra;
	int sub_cmd = value[0];
	QDF_STATUS status;
	int ret;
	hdd_context_t *hdd_ctx;
	uint8_t nol[QDF_MAX_NUM_CHAN];
	uint32_t nol_len = 0;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	switch (sub_cmd) {
	case QCSAP_PARAM_MAX_ASSOC:
		status = sme_cfg_get_int(hHal, WNI_CFG_ASSOC_STA_LIMIT,
					(uint32_t *) value);
		if (QDF_STATUS_SUCCESS != status) {
			hdd_err("get WNI_CFG_ASSOC_STA_LIMIT failed status: %d",
				status);
			ret = -EIO;
		}
		break;

	case QCSAP_PARAM_GET_WLAN_DBG:
	{
		qdf_trace_display();
		*value = 0;
		break;
	}

	case QCSAP_PARAM_RTSCTS:
	{
		*value = wma_cli_get_command(pHostapdAdapter->sessionId,
					     WMI_VDEV_PARAM_ENABLE_RTSCTS,
					     VDEV_CMD);
		break;
	}

	case QCASAP_SHORT_GI:
	{
		*value = (int)sme_get_ht_config(hHal,
						pHostapdAdapter->
						sessionId,
						WNI_CFG_HT_CAP_INFO_SHORT_GI_20MHZ);
		break;
	}

	case QCSAP_GTX_HT_MCS:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_HT_MCS");
		*value = wma_cli_get_command(pHostapdAdapter->sessionId,
					     WMI_VDEV_PARAM_GTX_HT_MCS,
					     GTX_CMD);
		break;
	}

	case QCSAP_GTX_VHT_MCS:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_VHT_MCS");
		*value = wma_cli_get_command(pHostapdAdapter->sessionId,
					     WMI_VDEV_PARAM_GTX_VHT_MCS,
					     GTX_CMD);
		break;
	}

	case QCSAP_GTX_USRCFG:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_USR_CFG");
		*value = wma_cli_get_command(pHostapdAdapter->sessionId,
					     WMI_VDEV_PARAM_GTX_USR_CFG,
					     GTX_CMD);
		break;
	}

	case QCSAP_GTX_THRE:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_THRE");
		*value = wma_cli_get_command(pHostapdAdapter->sessionId,
					     WMI_VDEV_PARAM_GTX_THRE,
					     GTX_CMD);
		break;
	}

	case QCSAP_GTX_MARGIN:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_MARGIN");
		*value = wma_cli_get_command(pHostapdAdapter->sessionId,
					     WMI_VDEV_PARAM_GTX_MARGIN,
					     GTX_CMD);
		break;
	}

	case QCSAP_GTX_STEP:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_STEP");
		*value = wma_cli_get_command(pHostapdAdapter->sessionId,
					     WMI_VDEV_PARAM_GTX_STEP,
					     GTX_CMD);
		break;
	}

	case QCSAP_GTX_MINTPC:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_MINTPC");
		*value = wma_cli_get_command(pHostapdAdapter->sessionId,
					     WMI_VDEV_PARAM_GTX_MINTPC,
					     GTX_CMD);
		break;
	}

	case QCSAP_GTX_BWMASK:
	{
		hdd_debug("GET WMI_VDEV_PARAM_GTX_BW_MASK");
		*value = wma_cli_get_command(pHostapdAdapter->sessionId,
					     WMI_VDEV_PARAM_GTX_BW_MASK,
					     GTX_CMD);
		break;
	}

	case QCASAP_GET_DFS_NOL:
	{
		wlansap_get_dfs_nol(
			WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
			nol, &nol_len);
	}
	break;

	case QCSAP_GET_ACL:
	{
		hdd_debug("QCSAP_GET_ACL");
		if (hdd_print_acl(pHostapdAdapter) !=
		    QDF_STATUS_SUCCESS) {
			hdd_err("QCSAP_GET_ACL returned Error: not completed");
		}
		*value = 0;
		break;
	}

	case QCASAP_TX_CHAINMASK_CMD:
	{
		hdd_debug("QCASAP_TX_CHAINMASK_CMD");
		*value = wma_cli_get_command(pHostapdAdapter->sessionId,
					     WMI_PDEV_PARAM_TX_CHAIN_MASK,
					     PDEV_CMD);
		break;
	}

	case QCASAP_RX_CHAINMASK_CMD:
	{
		hdd_debug("QCASAP_RX_CHAINMASK_CMD");
		*value = wma_cli_get_command(pHostapdAdapter->sessionId,
					     WMI_PDEV_PARAM_RX_CHAIN_MASK,
					     PDEV_CMD);
		break;
	}

	case QCASAP_NSS_CMD:
	{
		hdd_debug("QCASAP_NSS_CMD");
		*value = wma_cli_get_command(pHostapdAdapter->sessionId,
					     WMI_VDEV_PARAM_NSS,
					     VDEV_CMD);
		break;
	}
	case QCSAP_CAP_TSF:
		ret = hdd_capture_tsf(pHostapdAdapter, (uint32_t *)value, 1);
		break;
	case QCASAP_GET_TEMP_CMD:
	{
		hdd_debug("QCASAP_GET_TEMP_CMD");
		ret = wlan_hdd_get_temperature(pHostapdAdapter, value);
		break;
	}
	case QCSAP_GET_FW_PROFILE_DATA:
		hdd_debug("QCSAP_GET_FW_PROFILE_DATA");
		ret = wma_cli_set_command(pHostapdAdapter->sessionId,
				WMI_WLAN_PROFILE_GET_PROFILE_DATA_CMDID,
				0, DBG_CMD);
		break;
	case QCASAP_PARAM_LDPC:
	{
		ret = hdd_get_ldpc(pHostapdAdapter, value);
		break;
	}
	case QCASAP_PARAM_TX_STBC:
	{
		ret = hdd_get_tx_stbc(pHostapdAdapter, value);
		break;
	}
	case QCASAP_PARAM_RX_STBC:
	{
		ret = hdd_get_rx_stbc(pHostapdAdapter, value);
		break;
	}
	case QCSAP_PARAM_CHAN_WIDTH:
	{
		ret = hdd_sap_get_chan_width(pHostapdAdapter, value);
		break;
	}
	default:
		hdd_err("Invalid getparam command: %d", sub_cmd);
		ret = -EINVAL;
		break;

	}
	EXIT();
	return ret;
}

int
static iw_softap_getparam(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_getparam(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

/* Usage:
 * BLACK_LIST  = 0
 * WHITE_LIST  = 1
 * ADD MAC = 0
 * REMOVE MAC  = 1
 *
 * mac addr will be accepted as a 6 octet with each octet inputted in hex
 * for e.g. 00:0a:f5:11:22:33 is represented as 0x00 0x0a 0xf5 0x11 0x22 0x33
 * while using this ioctl
 *
 *  Syntax:
 *  iwpriv softap.0 modify_acl
 *  <6 octet mac addr> <list type> <cmd type>
 *
 * Examples:
 * eg 1. to add a mac addr 00:0a:f5:89:89:90 to the black list
 * iwpriv softap.0 modify_acl 0x00 0x0a 0xf5 0x89 0x89 0x90 0 0
 * eg 2. to delete a mac addr 00:0a:f5:89:89:90 from white list
 * iwpriv softap.0 modify_acl 0x00 0x0a 0xf5 0x89 0x89 0x90 1 1
 */
static
int __iw_softap_modify_acl(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
	uint8_t *value = (uint8_t *) extra;
	uint8_t pPeerStaMac[QDF_MAC_ADDR_SIZE];
	int listType, cmd, i;
	int ret;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	hdd_context_t *hdd_ctx;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	for (i = 0; i < QDF_MAC_ADDR_SIZE; i++)
		pPeerStaMac[i] = *(value + i);

	listType = (int)(*(value + i));
	i++;
	cmd = (int)(*(value + i));

	hdd_debug("Modify ACL mac:" MAC_ADDRESS_STR " type: %d cmd: %d",
	       MAC_ADDR_ARRAY(pPeerStaMac), listType, cmd);

	qdf_status = wlansap_modify_acl(
		WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
		pPeerStaMac, (eSapACLType) listType, (eSapACLCmdType) cmd);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_err("Modify ACL failed");
		ret = -EIO;
	}
	EXIT();
	return ret;
}

static
int iw_softap_modify_acl(struct net_device *dev,
			 struct iw_request_info *info,
			 union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_modify_acl(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

int
static __iw_softap_getchannel(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
	hdd_context_t *hdd_ctx;
	int *value = (int *)extra;
	int ret;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	*value = 0;
	if (test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags))
		*value = (WLAN_HDD_GET_AP_CTX_PTR(
					pHostapdAdapter))->operatingChannel;
	EXIT();
	return 0;
}

int
static iw_softap_getchannel(struct net_device *dev,
			    struct iw_request_info *info,
			    union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_getchannel(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

int
static __iw_softap_set_max_tx_power(struct net_device *dev,
				    struct iw_request_info *info,
				    union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
	hdd_context_t *hdd_ctx;
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
	int *value = (int *)extra;
	int set_value;
	int ret;
	struct qdf_mac_addr bssid = QDF_MAC_ADDR_BROADCAST_INITIALIZER;
	struct qdf_mac_addr selfMac = QDF_MAC_ADDR_BROADCAST_INITIALIZER;

	ENTER_DEV(dev);

	if (NULL == value)
		return -ENOMEM;

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	/* Assign correct slef MAC address */
	qdf_copy_macaddr(&bssid, &pHostapdAdapter->macAddressCurrent);
	qdf_copy_macaddr(&selfMac, &pHostapdAdapter->macAddressCurrent);

	set_value = value[0];
	if (QDF_STATUS_SUCCESS !=
	    sme_set_max_tx_power(hHal, bssid, selfMac, set_value)) {
		hdd_err("Setting maximum tx power failed");
		return -EIO;
	}
	EXIT();
	return 0;
}

int
static iw_softap_set_max_tx_power(struct net_device *dev,
				  struct iw_request_info *info,
				  union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_set_max_tx_power(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

int
static __iw_softap_set_pktlog(struct net_device *dev,
				    struct iw_request_info *info,
				    union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pHostapdAdapter = netdev_priv(dev);
	hdd_context_t *hdd_ctx;
	int *value = (int *)extra;

	ENTER_DEV(dev);

	if (NULL == value)
		return -ENOMEM;

	if (wrqu->data.length < 1 || wrqu->data.length > 2) {
		hdd_err("pktlog: either 1 or 2 parameters are required");
		return -EINVAL;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	return hdd_process_pktlog_command(hdd_ctx, value[0], value[1]);
}

int
static iw_softap_set_pktlog(struct net_device *dev,
				  struct iw_request_info *info,
				  union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_set_pktlog(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}


int
static __iw_softap_set_tx_power(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
	hdd_context_t *hdd_ctx;
	int *value = (int *)extra;
	int set_value;
	struct qdf_mac_addr bssid;
	int ret;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	if (NULL == value)
		return -ENOMEM;

	qdf_copy_macaddr(&bssid, &pHostapdAdapter->macAddressCurrent);

	set_value = value[0];
	if (QDF_STATUS_SUCCESS !=
	    sme_set_tx_power(hHal, pHostapdAdapter->sessionId, bssid,
			     pHostapdAdapter->device_mode, set_value)) {
		hdd_err("Setting tx power failed");
		return -EIO;
	}
	EXIT();
	return 0;
}

int
static iw_softap_set_tx_power(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_set_tx_power(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

#define IS_BROADCAST_MAC(x) (((x[0] & x[1] & x[2] & x[3] & x[4] & x[5]) == 0xff) ? 1 : 0)

int
static __iw_softap_getassoc_stamacaddr(struct net_device *dev,
				       struct iw_request_info *info,
				       union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
	hdd_station_info_t *pStaInfo = pHostapdAdapter->aStaInfo;
	hdd_context_t *hdd_ctx;
	char *buf;
	int cnt = 0;
	int left;
	int ret;
	/* maclist_index must be u32 to match userspace */
	u32 maclist_index;

	ENTER_DEV(dev);

	/*
	 * NOTE WELL: this is a "get" ioctl but it uses an even ioctl
	 * number, and even numbered iocts are supposed to have "set"
	 * semantics.  Hence the wireless extensions support in the kernel
	 * won't correctly copy the result to userspace, so the ioctl
	 * handler itself must copy the data.  Output format is 32-bit
	 * record length, followed by 0 or more 6-byte STA MAC addresses.
	 *
	 * Further note that due to the incorrect semantics, the "iwpriv"
	 * userspace application is unable to correctly invoke this API,
	 * hence it is not registered in the hostapd_private_args.  This
	 * API can only be invoked by directly invoking the ioctl() system
	 * call.
	 */

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	/* make sure userspace allocated a reasonable buffer size */
	if (wrqu->data.length < sizeof(maclist_index)) {
		hdd_err("invalid userspace buffer");
		return -EINVAL;
	}

	/* allocate local buffer to build the response */
	buf = qdf_mem_malloc(wrqu->data.length);
	if (!buf) {
		hdd_err("failed to allocate response buffer");
		return -ENOMEM;
	}

	/* start indexing beyond where the record count will be written */
	maclist_index = sizeof(maclist_index);
	left = wrqu->data.length - maclist_index;

	spin_lock_bh(&pHostapdAdapter->staInfo_lock);
	while ((cnt < WLAN_MAX_STA_COUNT) && (left >= QDF_MAC_ADDR_SIZE)) {
		if ((pStaInfo[cnt].isUsed) &&
		    (!IS_BROADCAST_MAC(pStaInfo[cnt].macAddrSTA.bytes))) {
			memcpy(&buf[maclist_index], &(pStaInfo[cnt].macAddrSTA),
			       QDF_MAC_ADDR_SIZE);
			maclist_index += QDF_MAC_ADDR_SIZE;
			left -= QDF_MAC_ADDR_SIZE;
		}
		cnt++;
	}
	spin_unlock_bh(&pHostapdAdapter->staInfo_lock);

	*((u32 *) buf) = maclist_index;
	wrqu->data.length = maclist_index;
	if (copy_to_user(wrqu->data.pointer, buf, maclist_index)) {
		hdd_err("failed to copy response to user buffer");
		ret = -EFAULT;
	}
	qdf_mem_free(buf);
	EXIT();
	return ret;
}

int
static iw_softap_getassoc_stamacaddr(struct net_device *dev,
				     struct iw_request_info *info,
				     union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_getassoc_stamacaddr(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

/* Usage:
 * mac addr will be accepted as a 6 octet with each octet inputted in hex
 * for e.g. 00:0a:f5:11:22:33 is represented as 0x00 0x0a 0xf5 0x11 0x22 0x33
 * while using this ioctl
 *
 * Syntax:
 * iwpriv softap.0 disassoc_sta <6 octet mac address>
 *
 * e.g.
 * disassociate sta with mac addr 00:0a:f5:11:22:33 from softap
 * iwpriv softap.0 disassoc_sta 0x00 0x0a 0xf5 0x11 0x22 0x33
 */

int
static __iw_softap_disassoc_sta(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
	hdd_context_t *hdd_ctx;
	uint8_t *peerMacAddr;
	int ret;
	struct tagCsrDelStaParams del_sta_params;

	ENTER_DEV(dev);

	if (!capable(CAP_NET_ADMIN)) {
		hdd_err("permission check failed");
		return -EPERM;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	/* iwpriv tool or framework calls this ioctl with
	 * data passed in extra (less than 16 octets);
	 */
	peerMacAddr = (uint8_t *) (extra);

	hdd_debug("data " MAC_ADDRESS_STR,
	       MAC_ADDR_ARRAY(peerMacAddr));
	wlansap_populate_del_sta_params(peerMacAddr,
			eSIR_MAC_DEAUTH_LEAVING_BSS_REASON,
			(SIR_MAC_MGMT_DISASSOC >> 4),
			&del_sta_params);
	hdd_softap_sta_disassoc(pHostapdAdapter, &del_sta_params);

	EXIT();
	return 0;
}

int
static iw_softap_disassoc_sta(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_disassoc_sta(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * iw_get_char_setnone() - Generic "get char" private ioctl handler
 * @dev: device upon which the ioctl was received
 * @info: ioctl request information
 * @wrqu: ioctl request data
 * @extra: ioctl extra data
 *
 * Return: 0 on success, non-zero on error
 */
static int __iw_get_char_setnone(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	int ret;
	int sub_cmd = wrqu->data.flags;
	hdd_context_t *hdd_ctx;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret != 0)
		return ret;

	switch (sub_cmd) {
	case QCSAP_GET_STATS:
		hdd_wlan_get_stats(adapter, &(wrqu->data.length),
					extra, WE_MAX_STR_LEN);
		break;
	case QCSAP_LIST_FW_PROFILE:
		hdd_wlan_list_fw_profile(&(wrqu->data.length),
					extra, WE_MAX_STR_LEN);
		break;
	}

	EXIT();
	return ret;
}

static int iw_get_char_setnone(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_get_char_setnone(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

static int __iw_get_channel_list(struct net_device *dev,
					struct iw_request_info *info,
					union iwreq_data *wrqu, char *extra)
{
	uint32_t num_channels = 0;
	uint8_t i = 0;
	uint8_t band_start_channel = CHAN_ENUM_1;
	uint8_t band_end_channel = CHAN_ENUM_184;
	hdd_adapter_t *hostapd_adapter = (netdev_priv(dev));
	tHalHandle hal = WLAN_HDD_GET_HAL_CTX(hostapd_adapter);
	struct channel_list_info *channel_list =
					(struct channel_list_info *) extra;
	tSirRFBand cur_band = SIR_BAND_ALL;
	hdd_context_t *hdd_ctx;
	int ret;
	bool is_dfs_mode_enabled = false;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(hostapd_adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	if (QDF_STATUS_SUCCESS != sme_get_freq_band(hal, &cur_band)) {
		hdd_err("not able get the current frequency band");
		return -EIO;
	}
	wrqu->data.length = sizeof(struct channel_list_info);

	if (SIR_BAND_2_4_GHZ == cur_band) {
		band_start_channel = CHAN_ENUM_1;
		band_end_channel = CHAN_ENUM_14;
	} else if (SIR_BAND_5_GHZ == cur_band) {
		band_start_channel = CHAN_ENUM_36;
		band_end_channel = CHAN_ENUM_184;
	}
	if (cur_band != SIR_BAND_2_4_GHZ) {
		if (hdd_ctx->config->dot11p_mode)
			band_end_channel = CHAN_ENUM_184;
		else
			band_end_channel = CHAN_ENUM_173;
	}

	if (hostapd_adapter->device_mode == QDF_STA_MODE &&
	    hdd_ctx->config->enableDFSChnlScan) {
		is_dfs_mode_enabled = true;
	} else if (hostapd_adapter->device_mode == QDF_SAP_MODE &&
		   hdd_ctx->config->enableDFSMasterCap) {
		is_dfs_mode_enabled = true;
	}

	hdd_debug("curBand = %d, StartChannel = %hu, EndChannel = %hu is_dfs_mode_enabled  = %d ",
			cur_band, band_start_channel, band_end_channel,
			is_dfs_mode_enabled);

	for (i = band_start_channel; i <= band_end_channel; i++) {
		if ((CHANNEL_STATE_ENABLE == CDS_CHANNEL_STATE(i)) ||
			(is_dfs_mode_enabled &&
		     CHANNEL_STATE_DFS == CDS_CHANNEL_STATE(i))) {
			channel_list->channels[num_channels] =
				CDS_CHANNEL_NUM(i);
			num_channels++;
		}
	}

	hdd_debug("number of channels %d", num_channels);

	channel_list->num_channels = num_channels;
	EXIT();

	return 0;
}

int iw_get_channel_list(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_get_channel_list(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

static
int __iw_get_genie(struct net_device *dev,
		   struct iw_request_info *info,
		   union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
	hdd_context_t *hdd_ctx;
	int ret;
	QDF_STATUS status;
	uint32_t length = DOT11F_IE_RSN_MAX_LEN;
	uint8_t genIeBytes[DOT11F_IE_RSN_MAX_LEN];

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	/*
	 * Actually retrieve the RSN IE from CSR.
	 * (We previously sent it down in the CSR Roam Profile.)
	 */
	status = wlan_sap_getstation_ie_information(
		WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
		&length, genIeBytes);
	if (status == QDF_STATUS_SUCCESS) {
		wrqu->data.length = length;
		if (length > DOT11F_IE_RSN_MAX_LEN) {
			hdd_err("Invalid buffer length: %d", length);
			return -E2BIG;
		}
		qdf_mem_copy(extra, genIeBytes, length);
		hdd_debug(" RSN IE of %d bytes returned",
				wrqu->data.length);
	} else {
		wrqu->data.length = 0;
		hdd_debug(" RSN IE failed to populate");
	}

	EXIT();
	return 0;
}

static
int iw_get_genie(struct net_device *dev,
		 struct iw_request_info *info,
		 union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_get_genie(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

static
int __iw_get_wpspbc_probe_req_ies(struct net_device *dev,
				  struct iw_request_info *info,
				  union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
	struct sap_wpspbc_probe_reqies WPSPBCProbeReqIEs;
	hdd_ap_ctx_t *pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
	hdd_context_t *hdd_ctx;
	int ret;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	hdd_debug("get_WPSPBCProbeReqIEs ioctl");
	memset((void *)&WPSPBCProbeReqIEs, 0, sizeof(WPSPBCProbeReqIEs));

	WPSPBCProbeReqIEs.probeReqIELen =
		pHddApCtx->WPSPBCProbeReq.probeReqIELen;
	qdf_mem_copy(&WPSPBCProbeReqIEs.probeReqIE,
		     pHddApCtx->WPSPBCProbeReq.probeReqIE,
		     WPSPBCProbeReqIEs.probeReqIELen);
	qdf_copy_macaddr(&WPSPBCProbeReqIEs.macaddr,
			 &pHddApCtx->WPSPBCProbeReq.peer_macaddr);
	if (copy_to_user(wrqu->data.pointer,
			 (void *)&WPSPBCProbeReqIEs,
			 sizeof(WPSPBCProbeReqIEs))) {
		hdd_err("failed to copy data to user buffer");
		return -EFAULT;
	}
	wrqu->data.length = 12 + WPSPBCProbeReqIEs.probeReqIELen;
	hdd_debug("Macaddress : " MAC_ADDRESS_STR,
	       MAC_ADDR_ARRAY(WPSPBCProbeReqIEs.macaddr.bytes));
	up(&pHddApCtx->semWpsPBCOverlapInd);
	EXIT();
	return 0;
}

static
int iw_get_wpspbc_probe_req_ies(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_get_wpspbc_probe_req_ies(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __iw_set_auth_hostap() -
 * This function sets the auth type received from the wpa_supplicant.
 *
 * @dev: pointer to the net device.
 * @info: pointer to the iw_request_info.
 * @wrqu: pointer to the iwreq_data.
 * @extra: pointer to the data.
 *
 * Return: 0 for success, non zero for failure
 */
static
int __iw_set_auth_hostap(struct net_device *dev, struct iw_request_info *info,
		       union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
	hdd_context_t *hdd_ctx;
	int ret;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	switch (wrqu->param.flags & IW_AUTH_INDEX) {
	case IW_AUTH_TKIP_COUNTERMEASURES:
	{
		if (wrqu->param.value) {
			hdd_debug("Counter Measure started %d",
				 wrqu->param.value);
			pWextState->mTKIPCounterMeasures =
				TKIP_COUNTER_MEASURE_STARTED;
		} else {
			hdd_debug("Counter Measure stopped=%d",
				 wrqu->param.value);
			pWextState->mTKIPCounterMeasures =
				TKIP_COUNTER_MEASURE_STOPED;
		}

		hdd_softap_tkip_mic_fail_counter_measure(pAdapter,
							 wrqu->param.
							 value);
	}
	break;

	default:

		hdd_warn("called with unsupported auth type %d",
		       wrqu->param.flags & IW_AUTH_INDEX);
		break;
	}

	EXIT();
	return 0;
}

/**
 * iw_set_auth_hostap() - Wrapper function to protect __iw_set_auth_hostap
 *			from the SSR.
 *
 * @dev - Pointer to the net device.
 * @info - Pointer to the iw_request_info.
 * @wrqu - Pointer to the iwreq_data.
 * @extra - Pointer to the data.
 *
 * Return: 0 for success, non zero for failure.
 */
static int
iw_set_auth_hostap(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_set_auth_hostap(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __iw_set_ap_encodeext() - set ap encode
 *
 * @dev - Pointer to the net device.
 * @info - Pointer to the iw_request_info.
 * @wrqu - Pointer to the iwreq_data.
 * @extra - Pointer to the data.
 *
 * Return: 0 for success, non zero for failure.
 */
static int __iw_set_ap_encodeext(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
	hdd_ap_ctx_t *pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
	hdd_context_t *hdd_ctx;
	int ret;
	QDF_STATUS vstatus;
	struct iw_encode_ext *ext = (struct iw_encode_ext *)extra;
	int key_index;
	struct iw_point *encoding = &wrqu->encoding;
	tCsrRoamSetKey setKey;
	int i;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	key_index = encoding->flags & IW_ENCODE_INDEX;

	if (key_index > 0) {
		/*Convert from 1-based to 0-based keying */
		key_index--;
	}

	if (!ext->key_len || ext->key_len > CSR_MAX_KEY_LEN)
		return -EINVAL;

	qdf_mem_zero(&setKey, sizeof(tCsrRoamSetKey));

	setKey.keyId = key_index;
	setKey.keyLength = ext->key_len;

	qdf_mem_copy(&setKey.Key[0], ext->key, ext->key_len);

	if (ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
		/*Key direction for group is RX only */
		setKey.keyDirection = eSIR_RX_ONLY;
		qdf_set_macaddr_broadcast(&setKey.peerMac);
	} else {

		setKey.keyDirection = eSIR_TX_RX;
		qdf_mem_copy(setKey.peerMac.bytes, ext->addr.sa_data,
			     QDF_MAC_ADDR_SIZE);
	}
	if (ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY) {
		setKey.keyDirection = eSIR_TX_DEFAULT;
		qdf_mem_copy(setKey.peerMac.bytes, ext->addr.sa_data,
			     QDF_MAC_ADDR_SIZE);
	}

	/*For supplicant pae role is zero */
	setKey.paeRole = 0;

	switch (ext->alg) {
	case IW_ENCODE_ALG_NONE:
		setKey.encType = eCSR_ENCRYPT_TYPE_NONE;
		break;

	case IW_ENCODE_ALG_WEP:
		setKey.encType = (ext->key_len == 5) ? eCSR_ENCRYPT_TYPE_WEP40 :
					eCSR_ENCRYPT_TYPE_WEP104;
		pHddApCtx->uPrivacy = 1;
		hdd_debug("uPrivacy=%d", pHddApCtx->uPrivacy);
		break;

	case IW_ENCODE_ALG_TKIP:
	{
		uint8_t *pKey = &setKey.Key[0];

		setKey.encType = eCSR_ENCRYPT_TYPE_TKIP;

		qdf_mem_zero(pKey, CSR_MAX_KEY_LEN);

		/* Supplicant sends the 32bytes key in this order
		 *
		 * |--------------|----------|----------|
		 * |   Tk1        |TX-MIC    |  RX Mic  |
		 * |--------------|----------|----------|
		 * <---16bytes---><--8bytes--><--8bytes-->
		 *
		 */
		/* Sme expects the 32 bytes key to be in the below order
		 *
		 * |--------------|----------|----------|
		 * |   Tk1        |RX-MIC    |  TX Mic  |
		 * |--------------|----------|----------|
		 * <---16bytes---><--8bytes--><--8bytes-->
		 */
		/* Copy the Temporal Key 1 (TK1) */
		qdf_mem_copy(pKey, ext->key, 16);

		/*Copy the rx mic first */
		qdf_mem_copy(&pKey[16], &ext->key[24], 8);

		/*Copy the tx mic */
		qdf_mem_copy(&pKey[24], &ext->key[16], 8);

	}
	break;

	case IW_ENCODE_ALG_CCMP:
		setKey.encType = eCSR_ENCRYPT_TYPE_AES;
		break;

	default:
		setKey.encType = eCSR_ENCRYPT_TYPE_NONE;
		break;
	}

	hdd_debug(":EncryptionType:%d key_len:%d, KeyId:%d",
	       setKey.encType, setKey.keyLength, setKey.keyId);
	for (i = 0; i < ext->key_len; i++)
		hdd_debug("%02x", setKey.Key[i]);

	vstatus = wlansap_set_key_sta(
		WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter), &setKey);

	if (vstatus != QDF_STATUS_SUCCESS) {
		hdd_err("wlansap_set_key_sta failed, status: %d",
		       vstatus);
		ret = -EINVAL;
	}
	EXIT();
	return ret;
}

/**
 * iw_set_ap_encodeext() - Wrapper function to protect __iw_set_ap_encodeext
 *			from the SSR.
 *
 * @dev - Pointer to the net device.
 * @info - Pointer to the iw_request_info.
 * @wrqu - Pointer to the iwreq_data.
 * @extra - Pointer to the data.
 *
 * Return: 0 for success, non zero for failure.
 */
static int iw_set_ap_encodeext(struct net_device *dev,
				struct iw_request_info *info,
				union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_set_ap_encodeext(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __iw_set_ap_mlme() - set ap mlme
 * @dev: pointer to net_device
 * @info: pointer to iw_request_info
 * @wrqu; pointer to iwreq_data
 * @extra: extra
 *
 * Return; 0 on success, error number otherwise
 */
static int __iw_set_ap_mlme(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra)
{
	ENTER_DEV(dev);
	return 0;
}

/**
 * iw_set_ap_mlme() - SSR wrapper for __iw_set_ap_mlme
 * @dev: pointer to net_device
 * @info: pointer to iw_request_info
 * @wrqu; pointer to iwreq_data
 * @extra: extra
 *
 * Return; 0 on success, error number otherwise
 */
static int iw_set_ap_mlme(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu,
			  char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_set_ap_mlme(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __iw_get_ap_rts_threshold() - get ap rts threshold
 * @dev - Pointer to the net device.
 * @info - Pointer to the iw_request_info.
 * @wrqu - Pointer to the iwreq_data.
 * @extra - Pointer to the data.
 *
 * Return: 0 for success, non zero for failure.
 */
static int __iw_get_ap_rts_threshold(struct net_device *dev,
				   struct iw_request_info *info,
				   union iwreq_data *wrqu, char *extra) {

	hdd_adapter_t *pHostapdAdapter = netdev_priv(dev);
	int ret;
	hdd_context_t *hdd_ctx;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;
	ret = hdd_wlan_get_rts_threshold(pHostapdAdapter, wrqu);

	return ret;
}

/**
 * iw_get_ap_rts_threshold() - Wrapper function to protect
 *			__iw_get_ap_rts_threshold from the SSR.
 * @dev - Pointer to the net device.
 * @info - Pointer to the iw_request_info.
 * @wrqu - Pointer to the iwreq_data.
 * @extra - Pointer to the data.
 *
 * Return: 0 for success, non zero for failure.
 */
static int iw_get_ap_rts_threshold(struct net_device *dev,
				   struct iw_request_info *info,
				   union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_get_ap_rts_threshold(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __iw_get_ap_frag_threshold() - get ap fragmentation threshold
 * @dev - Pointer to the net device.
 * @info - Pointer to the iw_request_info.
 * @wrqu - Pointer to the iwreq_data.
 * @extra - Pointer to the data.
 *
 * Return: 0 for success, non zero for failure.
 */
static int __iw_get_ap_frag_threshold(struct net_device *dev,
				    struct iw_request_info *info,
				    union iwreq_data *wrqu, char *extra) {

	hdd_adapter_t *pHostapdAdapter = netdev_priv(dev);
	hdd_context_t *hdd_ctx;
	int ret = 0;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	ret = hdd_wlan_get_frag_threshold(pHostapdAdapter, wrqu);

	return ret;
}

/**
 * iw_get_ap_frag_threshold() - Wrapper function to protect
 *			__iw_get_ap_frag_threshold from the SSR.
 * @dev - Pointer to the net device.
 * @info - Pointer to the iw_request_info.
 * @wrqu - Pointer to the iwreq_data.
 * @extra - Pointer to the data.
 *
 * Return: 0 for success, non zero for failure.
 */
static int iw_get_ap_frag_threshold(struct net_device *dev,
				    struct iw_request_info *info,
				    union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_get_ap_frag_threshold(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __iw_get_ap_freq() - get ap frequency
 * @dev - Pointer to the net device.
 * @info - Pointer to the iw_request_info.
 * @wrqu - Pointer to the iwreq_data.
 * @extra - Pointer to the data.
 *
 * Return: 0 for success, non zero for failure.
 */
static int __iw_get_ap_freq(struct net_device *dev,
			  struct iw_request_info *info, struct iw_freq *fwrq,
			  char *extra) {
	uint32_t status = false, channel = 0, freq = 0;
	hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
	hdd_context_t *hdd_ctx;
	tHalHandle hHal;
	hdd_hostapd_state_t *pHostapdState;
	hdd_ap_ctx_t *pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
	int ret;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);
	hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);

	if (pHostapdState->bssState == BSS_STOP) {
		if (sme_cfg_get_int(hHal, WNI_CFG_CURRENT_CHANNEL, &channel)
		    != QDF_STATUS_SUCCESS) {
			return -EIO;
		}
		status = hdd_wlan_get_freq(channel, &freq);
		if (0 == status) {
			/* Set Exponent parameter as 6 (MHZ) in struct
			 * iw_freq * iwlist & iwconfig command
			 * shows frequency into proper
			 * format (2.412 GHz instead of 246.2 MHz)
			 */
			fwrq->m = freq;
			fwrq->e = MHZ;
		}
	} else {
		channel = pHddApCtx->operatingChannel;
		status = hdd_wlan_get_freq(channel, &freq);
		if (0 == status) {
			/* Set Exponent parameter as 6 (MHZ) in struct iw_freq
			 * iwlist & iwconfig command shows frequency into proper
			 * format (2.412 GHz instead of 246.2 MHz)
			 */
			fwrq->m = freq;
			fwrq->e = MHZ;
		}
	}

	EXIT();
	return 0;
}

/**
 * iw_get_ap_freq() - Wrapper function to protect
 *                    __iw_get_ap_freq from the SSR.
 * @dev - Pointer to the net device.
 * @info - Pointer to the iw_request_info.
 * @wrqu - Pointer to the iwreq_data.
 * @extra - Pointer to the data.
 *
 * Return: 0 for success, non zero for failure.
 */
static int iw_get_ap_freq(struct net_device *dev,
			  struct iw_request_info *info,
			  struct iw_freq *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_get_ap_freq(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

static int
__iw_softap_stopbss(struct net_device *dev,
		    struct iw_request_info *info,
		    union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	hdd_context_t *hdd_ctx;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	status = wlan_hdd_validate_context(hdd_ctx);
	if (0 != status)
		return status;

	if (test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags)) {
		hdd_hostapd_state_t *pHostapdState =
			WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);

		qdf_event_reset(&pHostapdState->qdf_stop_bss_event);
		status = wlansap_stop_bss(
			WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter));
		if (QDF_IS_STATUS_SUCCESS(status)) {
			qdf_status =
				qdf_wait_for_event_completion(&pHostapdState->
					qdf_stop_bss_event,
					SME_CMD_TIMEOUT_VALUE);

			if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
				hdd_err("wait for single_event failed!!");
				QDF_ASSERT(0);
			}
		}
		clear_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags);
		cds_decr_session_set_pcl(pHostapdAdapter->device_mode,
					     pHostapdAdapter->sessionId);
	}
	EXIT();
	return (status == QDF_STATUS_SUCCESS) ? 0 : -EBUSY;
}

static int iw_softap_stopbss(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu,
			     char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_stopbss(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

static int
__iw_softap_version(struct net_device *dev,
		    struct iw_request_info *info,
		    union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pHostapdAdapter = netdev_priv(dev);
	hdd_context_t *hdd_ctx;
	int ret;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	wrqu->data.length = hdd_wlan_get_version(hdd_ctx, WE_MAX_STR_LEN,
						 extra);
	EXIT();
	return 0;
}

static int iw_softap_version(struct net_device *dev,
			     struct iw_request_info *info,
			     union iwreq_data *wrqu,
			     char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_version(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

static int hdd_softap_get_sta_info(hdd_adapter_t *adapter,
				   uint8_t *buf,
				   int size)
{
	int i;
	int written;
	uint8_t bc_sta_id;

	ENTER();

	bc_sta_id = WLAN_HDD_GET_AP_CTX_PTR(adapter)->uBCStaId;

	written = scnprintf(buf, size, "\nstaId staAddress\n");
	for (i = 0; i < WLAN_MAX_STA_COUNT; i++) {
		hdd_station_info_t *sta = &adapter->aStaInfo[i];

		if (written >= size - 1)
			break;

		if (!sta->isUsed)
			continue;

		if (i == bc_sta_id)
			continue;

		written += scnprintf(buf + written, size - written,
				     "%5d .%02x:%02x:%02x:%02x:%02x:%02x \t ecsa=%d\n",
				     sta->ucSTAId,
				     sta->macAddrSTA.bytes[0],
				     sta->macAddrSTA.bytes[1],
				     sta->macAddrSTA.bytes[2],
				     sta->macAddrSTA.bytes[3],
				     sta->macAddrSTA.bytes[4],
				     sta->macAddrSTA.bytes[5],
				     sta->ecsa_capable);
	}

	EXIT();

	return 0;
}

static int __iw_softap_get_sta_info(struct net_device *dev,
				    struct iw_request_info *info,
				    union iwreq_data *wrqu, char *extra)
{
	int errno;
	hdd_adapter_t *adapter;
	hdd_context_t *hdd_ctx;

	ENTER_DEV(dev);

	adapter = netdev_priv(dev);
	errno = hdd_validate_adapter(adapter);
	if (errno)
		return errno;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	errno = hdd_softap_get_sta_info(adapter, extra, WE_SAP_MAX_STA_INFO);
	if (errno) {
		hdd_err("Failed to get sta info; errno:%d", errno);
		return errno;
	}

	wrqu->data.length = strlen(extra);

	EXIT();

	return 0;
}

static int iw_softap_get_sta_info(struct net_device *dev,
				  struct iw_request_info *info,
				  union iwreq_data *wrqu,
				  char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_softap_get_sta_info(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __iw_set_ap_genie() - set ap wpa/rsn ie
 *
 * @dev - Pointer to the net device.
 * @info - Pointer to the iw_request_info.
 * @wrqu - Pointer to the iwreq_data.
 * @extra - Pointer to the data.
 *
 * Return: 0 for success, non zero for failure.
 */
static int __iw_set_ap_genie(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu, char *extra) {

	hdd_adapter_t *pHostapdAdapter = netdev_priv(dev);
	hdd_context_t *hdd_ctx;
	QDF_STATUS qdf_ret_status = QDF_STATUS_SUCCESS;
	uint8_t *genie = (uint8_t *)extra;
	int ret;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (0 != ret)
		return ret;

	if (!wrqu->data.length) {
		EXIT();
		return 0;
	}

	if (wrqu->data.length > DOT11F_IE_RSN_MAX_LEN) {
		hdd_err("WPARSN Ie input length is more than max[%d]",
			wrqu->data.length);
		return QDF_STATUS_E_INVAL;
	}

	switch (genie[0]) {
	case DOT11F_EID_WPA:
	case DOT11F_EID_RSN:
		if ((WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uPrivacy == 0) {
			hdd_softap_deregister_bc_sta(pHostapdAdapter);
			hdd_softap_register_bc_sta(pHostapdAdapter, 1);
		}
		(WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uPrivacy = 1;
		qdf_ret_status = wlansap_set_wparsn_ies(
			WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
			 genie, wrqu->data.length);
		break;

	default:
		hdd_err("Set UNKNOWN IE %X", genie[0]);
		qdf_ret_status = 0;
	}

	EXIT();
	return qdf_ret_status;
}

/**
 * iw_set_ap_genie() - Wrapper function to protect __iw_set_ap_genie
 *                      from the SSR.
 *
 * @dev - Pointer to the net device.
 * @info - Pointer to the iw_request_info.
 * @wrqu - Pointer to the iwreq_data.
 * @extra - Pointer to the data.
 *
 * Return: 0 for success, non zero for failure.
 */
static int
iw_set_ap_genie(struct net_device *dev,
		struct iw_request_info *info,
		union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_set_ap_genie(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

static
int __iw_get_softap_linkspeed(struct net_device *dev,
			      struct iw_request_info *info,
			      union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
	hdd_context_t *hdd_ctx;
	char *pLinkSpeed = (char *)extra;
	uint32_t link_speed = 0;
	int len = sizeof(uint32_t) + 1;
	struct qdf_mac_addr macAddress;
	char pmacAddress[MAC_ADDRESS_STR_LEN + 1];
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	int rc, errno, i;

	ENTER_DEV(dev);

	hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	errno = wlan_hdd_validate_context(hdd_ctx);
	if (errno)
		return errno;

	hdd_debug("wrqu->data.length(%d)", wrqu->data.length);

	/* Linkspeed is allowed only for P2P mode */
	if (pHostapdAdapter->device_mode != QDF_P2P_GO_MODE) {
		hdd_err("Link Speed is not allowed in Device mode %s(%d)",
			hdd_device_mode_to_string(
				pHostapdAdapter->device_mode),
			pHostapdAdapter->device_mode);
		return -ENOTSUPP;
	}

	if (wrqu->data.length >= MAC_ADDRESS_STR_LEN - 1) {
		if (copy_from_user((void *)pmacAddress,
				   wrqu->data.pointer, MAC_ADDRESS_STR_LEN)) {
			hdd_err("failed to copy data to user buffer");
			return -EFAULT;
		}
		pmacAddress[MAC_ADDRESS_STR_LEN - 1] = '\0';

		if (!mac_pton(pmacAddress, macAddress.bytes)) {
			hdd_err("String to Hex conversion Failed");
			return -EINVAL;
		}
	}
	/* If no mac address is passed and/or its length is less than 17,
	 * link speed for first connected client will be returned.
	 */
	if (wrqu->data.length < 17 || !QDF_IS_STATUS_SUCCESS(status)) {
		for (i = 0; i < WLAN_MAX_STA_COUNT; i++) {
			if (pHostapdAdapter->aStaInfo[i].isUsed &&
			    (!qdf_is_macaddr_broadcast
				  (&pHostapdAdapter->aStaInfo[i].macAddrSTA))) {
				qdf_copy_macaddr(
					&macAddress,
					&pHostapdAdapter->aStaInfo[i].
					 macAddrSTA);
				status = QDF_STATUS_SUCCESS;
				break;
			}
		}
	}
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("Invalid peer macaddress");
		return -EINVAL;
	}
	rc = wlan_hdd_get_linkspeed_for_peermac(pHostapdAdapter, &macAddress,
						&link_speed);
	if (rc) {
		hdd_err("Unable to retrieve SME linkspeed: %d", errno);
		return rc;
	}

	/* linkspeed in units of 500 kbps */
	link_speed = link_speed / 500;
	wrqu->data.length = len;
	rc = snprintf(pLinkSpeed, len, "%u", link_speed);
	if ((rc < 0) || (rc >= len)) {
		/* encoding or length error? */
		hdd_err("Unable to encode link speed");
		return -EIO;
	}
	EXIT();
	return 0;
}

static int
iw_get_softap_linkspeed(struct net_device *dev,
			struct iw_request_info *info,
			union iwreq_data *wrqu,
			char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_get_softap_linkspeed(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}



/**
 * __iw_get_peer_rssi() - get station's rssi
 * @dev: net device
 * @info: iwpriv request information
 * @wrqu: iwpriv command parameter
 * @extra
 *
 * This function will call wlan_hdd_get_peer_rssi
 * to get rssi
 *
 * Return: 0 on success, otherwise error value
 */
static int
__iw_get_peer_rssi(struct net_device *dev, struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	hdd_adapter_t *adapter = netdev_priv(dev);
	hdd_context_t *hddctx;
	char macaddrarray[MAC_ADDRESS_STR_LEN];
	struct qdf_mac_addr macaddress = QDF_MAC_ADDR_BROADCAST_INITIALIZER;
	int ret;
	char *rssi_info_output = extra;
	struct sir_peer_info *rssi_info;
	int i;
	int buf;
	int length;

	ENTER();

	hddctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hddctx);
	if (ret != 0)
		return ret;

	hdd_debug("wrqu->data.length= %d", wrqu->data.length);

	if (wrqu->data.length >= MAC_ADDRESS_STR_LEN - 1) {

		if (copy_from_user(macaddrarray,
			wrqu->data.pointer, MAC_ADDRESS_STR_LEN - 1)) {

			hdd_info("failed to copy data from user buffer");
			return -EFAULT;
		}

		macaddrarray[MAC_ADDRESS_STR_LEN - 1] = '\0';
		hdd_debug("%s", macaddrarray);

		if (!mac_pton(macaddrarray, macaddress.bytes))
			hdd_err("String to Hex conversion Failed");
	}

	ret = wlan_hdd_get_peer_rssi(adapter, &macaddress);
	if (ret) {
		hdd_err("Unable to retrieve peer rssi: %d", ret);
		return ret;
	}
	/*
	 * The iwpriv tool default print is before mac addr and rssi.
	 * Add '\n' before first rssi item to align the first rssi item
	 * with others
	 *
	 * wlan     getRSSI:
	 * [macaddr1] [rssi1]
	 * [macaddr2] [rssi2]
	 * [macaddr3] [rssi3]
	 */
	length = scnprintf(rssi_info_output, WE_MAX_STR_LEN, "\n");
	rssi_info = &adapter->peer_sta_info.info[0];
	for (i = 0; i < adapter->peer_sta_info.sta_num; i++) {
		buf = scnprintf
			(
			rssi_info_output + length, WE_MAX_STR_LEN - length,
			"[%pM] [%d]\n",
			rssi_info[i].peer_macaddr.bytes,
			rssi_info[i].rssi
			);
		length += buf;
	}
	wrqu->data.length = length + 1;

	EXIT();

	return 0;
}

/**
 * iw_get_peer_rssi() - get station's rssi
 * @dev: net device
 * @info: iwpriv request information
 * @wrqu: iwpriv command parameter
 * @extra
 *
 * This function will call __iw_get_peer_rssi
 *
 * Return: 0 on success, otherwise error value
 */
static int
iw_get_peer_rssi(struct net_device *dev, struct iw_request_info *info,
			union iwreq_data *wrqu, char *extra)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __iw_get_peer_rssi(dev, info, wrqu, extra);
	cds_ssr_unprotect(__func__);

	return ret;
}

static const iw_handler hostapd_handler[] = {
	(iw_handler) NULL,      /* SIOCSIWCOMMIT */
	(iw_handler) NULL,      /* SIOCGIWNAME */
	(iw_handler) NULL,      /* SIOCSIWNWID */
	(iw_handler) NULL,      /* SIOCGIWNWID */
	(iw_handler) NULL,      /* SIOCSIWFREQ */
	(iw_handler) iw_get_ap_freq,            /* SIOCGIWFREQ */
	(iw_handler) NULL,      /* SIOCSIWMODE */
	(iw_handler) NULL,       /* SIOCGIWMODE */
	(iw_handler) NULL,      /* SIOCSIWSENS */
	(iw_handler) NULL,      /* SIOCGIWSENS */
	(iw_handler) NULL,      /* SIOCSIWRANGE */
	(iw_handler) NULL,      /* SIOCGIWRANGE */
	(iw_handler) NULL,      /* SIOCSIWPRIV */
	(iw_handler) NULL,      /* SIOCGIWPRIV */
	(iw_handler) NULL,      /* SIOCSIWSTATS */
	(iw_handler) NULL,      /* SIOCGIWSTATS */
	(iw_handler) NULL,      /* SIOCSIWSPY */
	(iw_handler) NULL,      /* SIOCGIWSPY */
	(iw_handler) NULL,      /* SIOCSIWTHRSPY */
	(iw_handler) NULL,      /* SIOCGIWTHRSPY */
	(iw_handler) NULL,      /* SIOCSIWAP */
	(iw_handler) NULL,      /* SIOCGIWAP */
	(iw_handler) iw_set_ap_mlme,            /* SIOCSIWMLME */
	(iw_handler) NULL,      /* SIOCGIWAPLIST */
	(iw_handler) NULL,      /* SIOCSIWSCAN */
	(iw_handler) NULL,      /* SIOCGIWSCAN */
	(iw_handler) NULL,      /* SIOCSIWESSID */
	(iw_handler) NULL,      /* SIOCGIWESSID */
	(iw_handler) NULL,      /* SIOCSIWNICKN */
	(iw_handler) NULL,      /* SIOCGIWNICKN */
	(iw_handler) NULL,      /* -- hole -- */
	(iw_handler) NULL,      /* -- hole -- */
	(iw_handler) NULL,      /* SIOCSIWRATE */
	(iw_handler) NULL,      /* SIOCGIWRATE */
	(iw_handler) NULL,      /* SIOCSIWRTS */
	(iw_handler) iw_get_ap_rts_threshold,           /* SIOCGIWRTS */
	(iw_handler) NULL,      /* SIOCSIWFRAG */
	(iw_handler) iw_get_ap_frag_threshold,          /* SIOCGIWFRAG */
	(iw_handler) NULL,      /* SIOCSIWTXPOW */
	(iw_handler) NULL,      /* SIOCGIWTXPOW */
	(iw_handler) NULL,      /* SIOCSIWRETRY */
	(iw_handler) NULL,      /* SIOCGIWRETRY */
	(iw_handler) NULL,      /* SIOCSIWENCODE */
	(iw_handler) NULL,      /* SIOCGIWENCODE */
	(iw_handler) NULL,      /* SIOCSIWPOWER */
	(iw_handler) NULL,      /* SIOCGIWPOWER */
	(iw_handler) NULL,      /* -- hole -- */
	(iw_handler) NULL,      /* -- hole -- */
	(iw_handler) iw_set_ap_genie,           /* SIOCSIWGENIE */
	(iw_handler) NULL,      /* SIOCGIWGENIE */
	(iw_handler) iw_set_auth_hostap,        /* SIOCSIWAUTH */
	(iw_handler) NULL,      /* SIOCGIWAUTH */
	(iw_handler) iw_set_ap_encodeext,       /* SIOCSIWENCODEEXT */
	(iw_handler) NULL,      /* SIOCGIWENCODEEXT */
	(iw_handler) NULL,      /* SIOCSIWPMKSA */
};

/*
 * Note that the following ioctls were defined with semantics which
 * cannot be handled by the "iwpriv" userspace application and hence
 * they are not included in the hostapd_private_args array
 *     QCSAP_IOCTL_ASSOC_STA_MACADDR
 */

static const struct iw_priv_args hostapd_private_args[] = {
	{
		QCSAP_IOCTL_SETPARAM,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "setparam"
	}, {
		QCSAP_IOCTL_SETPARAM,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, ""
	}, {
		QCSAP_PARAM_MAX_ASSOC,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,
		"setMaxAssoc"
	}, {
		QCSAP_PARAM_HIDE_SSID,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "hideSSID"
	}, {
		QCSAP_PARAM_SET_MC_RATE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setMcRate"
	},
	{
		QCSAP_PARAM_SET_TXRX_FW_STATS,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,
		"txrx_fw_stats"
	}, {
		QCSAP_PARAM_SET_MCC_CHANNEL_LATENCY,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,
		"setMccLatency"
	}, {
		QCSAP_PARAM_SET_MCC_CHANNEL_QUOTA,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,
		"setMccQuota"
	}, {
		QCSAP_PARAM_SET_CHANNEL_CHANGE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,
		"setChanChange"
	}, {
		QCSAP_PARAM_CONC_SYSTEM_PREF,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,
		"setConcSysPref"
	},
	/* Sub-cmds DBGLOG specific commands */
	{
		QCSAP_DBGLOG_LOG_LEVEL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "dl_loglevel"
	}, {
		QCSAP_DBGLOG_VAP_ENABLE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dl_vapon"
	}, {
		QCSAP_DBGLOG_VAP_DISABLE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "dl_vapoff"
	}, {
		QCSAP_DBGLOG_MODULE_ENABLE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dl_modon"
	}, {
		QCSAP_DBGLOG_MODULE_DISABLE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "dl_modoff"
	}, {
		QCSAP_DBGLOG_MOD_LOG_LEVEL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "dl_mod_loglevel"
	}, {
		QCSAP_DBGLOG_TYPE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dl_type"
	}, {
		QCSAP_DBGLOG_REPORT_ENABLE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "dl_report"
	}, {
		QCASAP_TXRX_FWSTATS_RESET,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "txrx_fw_st_rst"
	}, {
		QCSAP_PARAM_RTSCTS,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "enablertscts"
	}, {
		QCASAP_SET_11N_RATE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set11NRates"
	}, {
		QCASAP_SET_VHT_RATE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set11ACRates"
	}, {
		QCASAP_SHORT_GI,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "enable_short_gi"
	}, {
		QCSAP_SET_AMPDU,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ampdu"
	}, {
		QCSAP_SET_AMSDU,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "amsdu"
	}, {
		QCSAP_GTX_HT_MCS,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "gtxHTMcs"
	}, {
		QCSAP_GTX_VHT_MCS,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "gtxVHTMcs"
	}, {
		QCSAP_GTX_USRCFG,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "gtxUsrCfg"
	}, {
		QCSAP_GTX_THRE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "gtxThre"
	}, {
		QCSAP_GTX_MARGIN,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "gtxMargin"
	}, {
		QCSAP_GTX_STEP,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "gtxStep"
	}, {
		QCSAP_GTX_MINTPC,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "gtxMinTpc"
	}, {
		QCSAP_GTX_BWMASK,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "gtxBWMask"
	}, {
		QCSAP_PARAM_CLR_ACL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "setClearAcl"
	}, {
		QCSAP_PARAM_ACL_MODE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setAclMode"
	},
	{
		QCASAP_SET_TM_LEVEL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "setTmLevel"
	}, {
		QCASAP_SET_DFS_IGNORE_CAC,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "setDfsIgnoreCAC"
	}, {
		QCASAP_SET_DFS_NOL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "setdfsnol"
	}, {
		QCASAP_SET_DFS_TARGET_CHNL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "setNextChnl"
	}, {
		QCASAP_SET_RADAR_CMD,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setRadar"
	},
	{
		QCSAP_IPA_UC_STAT,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "ipaucstat"
	},
	{
		QCASAP_TX_CHAINMASK_CMD,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set_txchainmask"
	}, {
		QCASAP_RX_CHAINMASK_CMD,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set_rxchainmask"
	}, {
		QCASAP_NSS_CMD,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_nss"
	}, {
		QCASAP_SET_PHYMODE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "setphymode"
	}, {
		QCASAP_DUMP_STATS,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "dumpStats"
	}, {
		QCASAP_CLEAR_STATS,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "clearStats"
	}, {
		QCSAP_START_FW_PROFILING,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "startProfile"
	}, {
		QCASAP_PARAM_LDPC,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set_ldpc"
	}, {
		QCASAP_PARAM_TX_STBC,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set_tx_stbc"
	}, {
		QCASAP_PARAM_RX_STBC,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "set_rx_stbc"
	}, {
		QCSAP_IOCTL_GETPARAM, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getparam"
	}, {
		QCSAP_IOCTL_GETPARAM, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, ""
	}, {
		QCSAP_PARAM_MAX_ASSOC, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getMaxAssoc"
	}, {
		QCSAP_PARAM_GET_WLAN_DBG, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getwlandbg"
	}, {
		QCSAP_GTX_BWMASK, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxBWMask"
	}, {
		QCSAP_GTX_MINTPC, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxMinTpc"
	}, {
		QCSAP_GTX_STEP, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxStep"
	}, {
		QCSAP_GTX_MARGIN, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxMargin"
	}, {
		QCSAP_GTX_THRE, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxThre"
	}, {
		QCSAP_GTX_USRCFG, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxUsrCfg"
	}, {
		QCSAP_GTX_VHT_MCS, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxVHTMcs"
	}, {
		QCSAP_GTX_HT_MCS, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_gtxHTMcs"
	}, {
		QCASAP_SHORT_GI, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_short_gi"
	}, {
		QCSAP_PARAM_RTSCTS, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rtscts"
	}, {
		QCASAP_GET_DFS_NOL, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getdfsnol"
	}, {
		QCSAP_GET_ACL, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_acl_list"
	}, {
		QCASAP_PARAM_LDPC, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_ldpc"
	}, {
		QCASAP_PARAM_TX_STBC, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_tx_stbc"
	}, {
		QCASAP_PARAM_RX_STBC, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_rx_stbc"
	}, {
		QCSAP_PARAM_CHAN_WIDTH, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_chwidth"
	}, {
		QCASAP_TX_CHAINMASK_CMD, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_txchainmask"
	}, {
		QCASAP_RX_CHAINMASK_CMD, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_rxchainmask"
	}, {
		QCASAP_NSS_CMD, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"get_nss"
	}, {
		QCSAP_CAP_TSF, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		"cap_tsf"
	}, {
		QCSAP_IOCTL_SET_NONE_GET_THREE, 0, IW_PRIV_TYPE_INT |
		IW_PRIV_SIZE_FIXED | 3,    ""
	}, {
		QCSAP_GET_TSF, 0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3,
		"get_tsf"
	}, {
		QCASAP_GET_TEMP_CMD, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_temp"
	}, {
		QCSAP_GET_FW_PROFILE_DATA, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getProfileData"
	}, {
		QCSAP_IOCTL_GET_STAWPAIE,
		0, IW_PRIV_TYPE_BYTE | DOT11F_IE_RSN_MAX_LEN,
		"get_staWPAIE"
	}, {
		QCSAP_IOCTL_STOPBSS, IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED, 0,
		"stopbss"
	}, {
		QCSAP_IOCTL_VERSION, 0, IW_PRIV_TYPE_CHAR | WE_MAX_STR_LEN,
		"version"
	}, {
		QCSAP_IOCTL_GET_STA_INFO, 0,
		IW_PRIV_TYPE_CHAR | WE_SAP_MAX_STA_INFO, "get_sta_info"
	}, {
		QCSAP_IOCTL_GET_WPS_PBC_PROBE_REQ_IES,
		IW_PRIV_TYPE_BYTE |
		sizeof(struct sap_wpspbc_probe_reqies) |
		IW_PRIV_SIZE_FIXED, 0, "getProbeReqIEs"
	}
	, {
		QCSAP_IOCTL_GET_CHANNEL, 0,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getchannel"
	}
	, {
		QCSAP_IOCTL_DISASSOC_STA,
		IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 6, 0,
		"disassoc_sta"
	}
	/* handler for main ioctl */
	, {
		QCSAP_PRIV_GET_CHAR_SET_NONE, 0,
		IW_PRIV_TYPE_CHAR | WE_MAX_STR_LEN, ""
	}
	/* handler for sub-ioctl */
	, {
		QCSAP_GET_STATS, 0,
		IW_PRIV_TYPE_CHAR | WE_MAX_STR_LEN, "getStats"
	}
	, {
		QCSAP_LIST_FW_PROFILE, 0,
		IW_PRIV_TYPE_CHAR | WE_MAX_STR_LEN, "listProfile"
	}
	, {
		QCSAP_IOCTL_PRIV_GET_SOFTAP_LINK_SPEED,
		IW_PRIV_TYPE_CHAR | 18,
		IW_PRIV_TYPE_CHAR | 5, "getLinkSpeed"
	}
	, {
		QCSAP_IOCTL_PRIV_GET_RSSI,
		IW_PRIV_TYPE_CHAR | 18,
		IW_PRIV_TYPE_CHAR | WE_MAX_STR_LEN, "getRSSI"
	}
	, {
		QCSAP_IOCTL_PRIV_SET_THREE_INT_GET_NONE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, ""
	}
	,
	/* handlers for sub-ioctl */
	{
		WE_SET_WLAN_DBG,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "setwlandbg"
	}
	,
	/* handlers for sub-ioctl */
	{
		WE_SET_DP_TRACE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "set_dp_trace"
	}
	,
	/* handlers for main ioctl */
	{
		QCSAP_IOCTL_PRIV_SET_VAR_INT_GET_NONE,
		IW_PRIV_TYPE_INT | MAX_VAR_ARGS, 0, ""
	}
	, {
		WE_P2P_NOA_CMD, IW_PRIV_TYPE_INT | MAX_VAR_ARGS, 0, "SetP2pPs"
	}
	, {
		WE_UNIT_TEST_CMD, IW_PRIV_TYPE_INT | MAX_VAR_ARGS, 0,
		"setUnitTestCmd"
	}
	,
	/* handlers for main ioctl */
	{
		QCSAP_IOCTL_MODIFY_ACL,
		IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 8, 0, "modify_acl"
	}
	,
	/* handlers for main ioctl */
	{
		QCSAP_IOCTL_GET_CHANNEL_LIST,
		0,
		IW_PRIV_TYPE_BYTE | sizeof(struct channel_list_info),
		"getChannelList"
	}
	,
	/* handlers for main ioctl */
	{
		QCSAP_IOCTL_SET_TX_POWER,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setTxPower"
	}
	,
	/* handlers for main ioctl */
	{
		QCSAP_IOCTL_SET_MAX_TX_POWER,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0, "setTxMaxPower"
	}
	,
	{
		QCSAP_IOCTL_SET_PKTLOG,
		IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
		0, "pktlog"
	}
	,
	/* Set HDD CFG Ini param */
	{
		QCSAP_IOCTL_SET_INI_CFG,
		IW_PRIV_TYPE_CHAR | QCSAP_IOCTL_MAX_STR_LEN, 0, "setConfig"
	}
	,
	/* Get HDD CFG Ini param */
	{
		QCSAP_IOCTL_GET_INI_CFG,
		0, IW_PRIV_TYPE_CHAR | QCSAP_IOCTL_MAX_STR_LEN, "getConfig"
	}
	,
	/* handlers for main ioctl */
	{
	/* handlers for main ioctl */
		QCSAP_IOCTL_SET_TWO_INT_GET_NONE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, ""
	}
	,
	/* handlers for sub-ioctl */
#ifdef WLAN_DEBUG
	{
		QCSAP_IOCTL_SET_FW_CRASH_INJECT,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		0, "crash_inject"
	}
	,
#endif
	{
		QCASAP_SET_RADAR_DBG,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0,  "setRadarDbg"
	}
	,
	/* dump dp trace - descriptor or dp trace records */
	{
		QCSAP_IOCTL_DUMP_DP_TRACE_LEVEL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		0, "dump_dp_trace"
	}
	,
	{
		QCSAP_ENABLE_FW_PROFILE,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		0, "enableProfile"
	}
	,
	{
		QCSAP_SET_FW_PROFILE_HIST_INTVL,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
		0, "set_hist_intvl"
	}
	,
	{	QCSAP_SET_DEFAULT_AMPDU,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0,
		"def_ampdu"
	}
	,
	{	QCSAP_ENABLE_RTS_BURSTING,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		0,
		"rts_bursting"
	}
	,
};

static const iw_handler hostapd_private[] = {
	/* set priv ioctl */
	[QCSAP_IOCTL_SETPARAM - SIOCIWFIRSTPRIV] = iw_softap_setparam,
	/* get priv ioctl */
	[QCSAP_IOCTL_GETPARAM - SIOCIWFIRSTPRIV] = iw_softap_getparam,
	[QCSAP_IOCTL_SET_NONE_GET_THREE - SIOCIWFIRSTPRIV] =
							iw_softap_get_three,
	/* get station genIE */
	[QCSAP_IOCTL_GET_STAWPAIE - SIOCIWFIRSTPRIV] = iw_get_genie,
	/* stop bss */
	[QCSAP_IOCTL_STOPBSS - SIOCIWFIRSTPRIV] = iw_softap_stopbss,
	/* get driver version */
	[QCSAP_IOCTL_VERSION - SIOCIWFIRSTPRIV] = iw_softap_version,
	[QCSAP_IOCTL_GET_WPS_PBC_PROBE_REQ_IES - SIOCIWFIRSTPRIV] =
		iw_get_wpspbc_probe_req_ies,
	[QCSAP_IOCTL_GET_CHANNEL - SIOCIWFIRSTPRIV] =
		iw_softap_getchannel,
	[QCSAP_IOCTL_ASSOC_STA_MACADDR - SIOCIWFIRSTPRIV] =
		iw_softap_getassoc_stamacaddr,
	[QCSAP_IOCTL_DISASSOC_STA - SIOCIWFIRSTPRIV] =
		iw_softap_disassoc_sta,
	[QCSAP_PRIV_GET_CHAR_SET_NONE - SIOCIWFIRSTPRIV] =
		iw_get_char_setnone,
	[QCSAP_IOCTL_PRIV_SET_THREE_INT_GET_NONE -
	 SIOCIWFIRSTPRIV] =
		iw_set_three_ints_getnone,
	[QCSAP_IOCTL_PRIV_SET_VAR_INT_GET_NONE -
	 SIOCIWFIRSTPRIV] =
		iw_set_var_ints_getnone,
	[QCSAP_IOCTL_MODIFY_ACL - SIOCIWFIRSTPRIV] =
		iw_softap_modify_acl,
	[QCSAP_IOCTL_GET_CHANNEL_LIST - SIOCIWFIRSTPRIV] =
		iw_get_channel_list,
	[QCSAP_IOCTL_GET_STA_INFO - SIOCIWFIRSTPRIV] =
		iw_softap_get_sta_info,
	[QCSAP_IOCTL_PRIV_GET_SOFTAP_LINK_SPEED -
	 SIOCIWFIRSTPRIV] =
		iw_get_softap_linkspeed,
	[QCSAP_IOCTL_PRIV_GET_RSSI - SIOCIWFIRSTPRIV] =
		iw_get_peer_rssi,
	[QCSAP_IOCTL_SET_TX_POWER - SIOCIWFIRSTPRIV] =
		iw_softap_set_tx_power,
	[QCSAP_IOCTL_SET_MAX_TX_POWER - SIOCIWFIRSTPRIV] =
		iw_softap_set_max_tx_power,
	[QCSAP_IOCTL_SET_PKTLOG - SIOCIWFIRSTPRIV] =
		iw_softap_set_pktlog,
	[QCSAP_IOCTL_SET_INI_CFG - SIOCIWFIRSTPRIV] =
		iw_softap_set_ini_cfg,
	[QCSAP_IOCTL_GET_INI_CFG - SIOCIWFIRSTPRIV] =
		iw_softap_get_ini_cfg,
	[QCSAP_IOCTL_SET_TWO_INT_GET_NONE - SIOCIWFIRSTPRIV] =
		iw_softap_set_two_ints_getnone,
};

const struct iw_handler_def hostapd_handler_def = {
	.num_standard = QDF_ARRAY_SIZE(hostapd_handler),
	.num_private = QDF_ARRAY_SIZE(hostapd_private),
	.num_private_args = QDF_ARRAY_SIZE(hostapd_private_args),
	.standard = (iw_handler *) hostapd_handler,
	.private = (iw_handler *) hostapd_private,
	.private_args = hostapd_private_args,
	.get_wireless_stats = NULL,
};

const struct net_device_ops net_ops_struct = {
	.ndo_open = hdd_hostapd_open,
	.ndo_stop = hdd_hostapd_stop,
	.ndo_uninit = hdd_hostapd_uninit,
	.ndo_start_xmit = hdd_softap_hard_start_xmit,
	.ndo_tx_timeout = hdd_softap_tx_timeout,
	.ndo_get_stats = hdd_get_stats,
	.ndo_set_mac_address = hdd_hostapd_set_mac_address,
	.ndo_do_ioctl = hdd_ioctl,
	.ndo_change_mtu = hdd_hostapd_change_mtu,
	.ndo_select_queue = hdd_hostapd_select_queue,
};

static int hdd_set_hostapd(hdd_adapter_t *pAdapter)
{
	return QDF_STATUS_SUCCESS;
}

void hdd_set_ap_ops(struct net_device *pWlanHostapdDev)
{
	pWlanHostapdDev->netdev_ops = &net_ops_struct;
}

QDF_STATUS hdd_init_ap_mode(hdd_adapter_t *pAdapter, bool reinit)
{
	hdd_hostapd_state_t *phostapdBuf;
	struct net_device *dev = pAdapter->dev;
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	QDF_STATUS status;
	QDF_STATUS qdf_status;
	v_CONTEXT_t p_cds_context = (WLAN_HDD_GET_CTX(pAdapter))->pcds_context;
	v_CONTEXT_t sapContext = NULL;
	int ret;
	enum tQDF_ADAPTER_MODE mode;
	uint32_t session_id = CSR_SESSION_ID_INVALID;
	enum dfs_mode acs_dfs_mode;

	ENTER();

	hdd_info("SSR in progress: %d", reinit);
	qdf_atomic_init(&pAdapter->sessionCtx.ap.acs_in_progress);
	if (reinit)
		sapContext = pAdapter->sessionCtx.ap.sapContext;
	else {
		sapContext = wlansap_open(p_cds_context);
		if (sapContext == NULL) {
			hdd_err("wlansap_open failed!!");
			return QDF_STATUS_E_FAULT;
		}

		pAdapter->sessionCtx.ap.sapContext = sapContext;
		pAdapter->sessionCtx.ap.sapConfig.channel =
			pHddCtx->acs_policy.acs_channel;
		acs_dfs_mode = pHddCtx->acs_policy.acs_dfs_mode;
		pAdapter->sessionCtx.ap.sapConfig.acs_dfs_mode =
			wlan_hdd_get_dfs_mode(acs_dfs_mode);
	}

	if (pAdapter->device_mode == QDF_P2P_GO_MODE) {
		mode = QDF_P2P_GO_MODE;
	} else if (pAdapter->device_mode == QDF_SAP_MODE) {
		mode = QDF_SAP_MODE;
	} else {
		hdd_err("Invalid mode for AP: %d", pAdapter->device_mode);
		return QDF_STATUS_E_FAULT;
	}

	status = wlansap_start(sapContext, hdd_hostapd_sap_event_cb, mode,
			pAdapter->macAddressCurrent.bytes,
			&session_id, pAdapter->dev);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("wlansap_start failed!!");
		wlansap_close(sapContext);
		pAdapter->sessionCtx.ap.sapContext = NULL;
		return status;
	}
	pAdapter->sessionId = session_id;
	set_bit(SME_SESSION_OPENED, &pAdapter->event_flags);

	/* Allocate the Wireless Extensions state structure */
	phostapdBuf = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);

	sme_set_curr_device_mode(pHddCtx->hHal, pAdapter->device_mode);

	/* Zero the memory.  This zeros the profile structure. */
	memset(phostapdBuf, 0, sizeof(hdd_hostapd_state_t));

	/* Set up the pointer to the Wireless Extensions state structure */
	/* NOP */
	status = hdd_set_hostapd(pAdapter);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("hdd_set_hostapd failed!!");
		wlansap_close(sapContext);
		pAdapter->sessionCtx.ap.sapContext = NULL;
		return status;
	}

	qdf_status = qdf_event_create(&phostapdBuf->qdf_event);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_err("Hostapd HDD qdf event init failed!!");
		wlansap_close(sapContext);
		pAdapter->sessionCtx.ap.sapContext = NULL;
		return qdf_status;
	}

	qdf_status = qdf_event_create(&phostapdBuf->qdf_stop_bss_event);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_err("Hostapd HDD stop bss event init failed!!");
		wlansap_close(sapContext);
		pAdapter->sessionCtx.ap.sapContext = NULL;
		return qdf_status;
	}

	qdf_status = qdf_event_create(&phostapdBuf->qdf_sta_disassoc_event);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_err("Hostapd HDD sta disassoc event init failed!!");
		wlansap_close(sapContext);
		pAdapter->sessionCtx.ap.sapContext = NULL;
		return qdf_status;
	}


	sema_init(&(WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->semWpsPBCOverlapInd, 1);

	/* Register as a wireless device */
	dev->wireless_handlers = (struct iw_handler_def *)&hostapd_handler_def;

	/* Initialize the data path module */
	status = hdd_softap_init_tx_rx(pAdapter);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("hdd_softap_init_tx_rx failed");

	status = hdd_wmm_adapter_init(pAdapter);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("hdd_wmm_adapter_init() failed code: %08d [x%08x]",
		       status, status);
		goto error_wmm_init;
	}

	set_bit(WMM_INIT_DONE, &pAdapter->event_flags);

	ret = wma_cli_set_command(pAdapter->sessionId,
				  WMI_PDEV_PARAM_BURST_ENABLE,
				  HDD_ENABLE_SIFS_BURST_DEFAULT,
				  PDEV_CMD);

	if (0 != ret)
		hdd_err("WMI_PDEV_PARAM_BURST_ENABLE set failed: %d", ret);

	if (!reinit) {
		pAdapter->sessionCtx.ap.sapConfig.acs_cfg.acs_mode = false;
		wlan_hdd_undo_acs(pAdapter);
		qdf_mem_zero(&pAdapter->sessionCtx.ap.sapConfig.acs_cfg,
			     sizeof(struct sap_acs_cfg));
	}

	/* rcpi info initialization */
	qdf_mem_zero(&pAdapter->rcpi, sizeof(pAdapter->rcpi));

	return status;

error_wmm_init:
	hdd_softap_deinit_tx_rx(pAdapter);
	wlansap_close(sapContext);
	pAdapter->sessionCtx.ap.sapContext = NULL;
	EXIT();
	return status;
}

/**
 * hdd_wlan_create_ap_dev() - create an AP-mode device
 * @pHddCtx: Global HDD context
 * @macAddr: MAC address to assign to the interface
 * @name_assign_type: the name of assign type of the netdev
 * @iface_name: User-visible name of the interface
 *
 * This function will allocate a Linux net_device and configuration it
 * for an AP mode of operation.  Note that the device is NOT actually
 * registered with the kernel at this time.
 *
 * Return: A pointer to the private data portion of the net_device if
 * the allocation and initialization was successful, NULL otherwise.
 */
hdd_adapter_t *hdd_wlan_create_ap_dev(hdd_context_t *pHddCtx,
				      tSirMacAddr macAddr,
				      unsigned char name_assign_type,
				      uint8_t *iface_name)
{
	struct net_device *pWlanHostapdDev = NULL;
	hdd_adapter_t *pHostapdAdapter = NULL;
	QDF_STATUS qdf_status;

	hdd_debug("iface_name = %s", iface_name);

	pWlanHostapdDev = alloc_netdev_mq(sizeof(hdd_adapter_t), iface_name,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)) || defined(WITH_BACKPORTS)
					  name_assign_type,
#endif
					  ether_setup, NUM_TX_QUEUES);

	if (pWlanHostapdDev != NULL) {
		pHostapdAdapter = netdev_priv(pWlanHostapdDev);

		/* Init the net_device structure */
		ether_setup(pWlanHostapdDev);

		/* Initialize the adapter context to zeros. */
		qdf_mem_zero(pHostapdAdapter, sizeof(hdd_adapter_t));
		pHostapdAdapter->dev = pWlanHostapdDev;
		pHostapdAdapter->pHddCtx = pHddCtx;
		pHostapdAdapter->magic = WLAN_HDD_ADAPTER_MAGIC;
		pHostapdAdapter->sessionId = HDD_SESSION_ID_INVALID;

		hdd_debug("pWlanHostapdDev = %pK, pHostapdAdapter = %pK, concurrency_mode=0x%x",
		       pWlanHostapdDev,
		       pHostapdAdapter,
		       (int)cds_get_concurrency_mode());

		/* Init the net_device structure */
		strlcpy(pWlanHostapdDev->name, (const char *)iface_name,
			IFNAMSIZ);

		hdd_set_ap_ops(pHostapdAdapter->dev);

		pWlanHostapdDev->watchdog_timeo = HDD_TX_TIMEOUT;
		pWlanHostapdDev->mtu = HDD_DEFAULT_MTU;
		pWlanHostapdDev->tx_queue_len = HDD_NETDEV_TX_QUEUE_LEN;

		if (pHddCtx->config->enable_ip_tcp_udp_checksum_offload)
			pWlanHostapdDev->features |=
				NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM;
		pWlanHostapdDev->features |= NETIF_F_RXCSUM;

		qdf_mem_copy(pWlanHostapdDev->dev_addr, (void *)macAddr,
			     sizeof(tSirMacAddr));
		qdf_mem_copy(pHostapdAdapter->macAddressCurrent.bytes,
			     (void *)macAddr, sizeof(tSirMacAddr));

		pHostapdAdapter->offloads_configured = false;
		hdd_dev_setup_destructor(pWlanHostapdDev);
		pWlanHostapdDev->ieee80211_ptr = &pHostapdAdapter->wdev;
		pHostapdAdapter->wdev.wiphy = pHddCtx->wiphy;
		pHostapdAdapter->wdev.netdev = pWlanHostapdDev;
		hdd_set_tso_flags(pHddCtx, pWlanHostapdDev);

		SET_NETDEV_DEV(pWlanHostapdDev, pHddCtx->parent_dev);
		spin_lock_init(&pHostapdAdapter->pause_map_lock);
		pHostapdAdapter->start_time =
			pHostapdAdapter->last_time = qdf_system_ticks();

		qdf_status = qdf_event_create(
				&pHostapdAdapter->qdf_session_open_event);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			hdd_err("failed to create session open QDF event!");
			free_netdev(pHostapdAdapter->dev);
			return NULL;
		}

		qdf_status = qdf_event_create(
				&pHostapdAdapter->qdf_session_close_event);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
			hdd_err("failed to create session close QDF event!");
			free_netdev(pHostapdAdapter->dev);
			return NULL;
		}

	}
	return pHostapdAdapter;
}

/**
 * hdd_unregister_hostapd() - unregister hostapd
 * @pAdapter: Pointer to hostapd adapter
 * @rtnl_held: true if rtnl lock held; false otherwise
 *
 * Return: QDF_STATUS enumaration
 */
int hdd_unregister_hostapd(hdd_adapter_t *pAdapter, bool rtnl_held)
{
	QDF_STATUS status;
	void *sapContext = WLAN_HDD_GET_SAP_CTX_PTR(pAdapter);

	ENTER();

	hdd_softap_deinit_tx_rx(pAdapter);

	/* if we are being called during driver unload,
	 * then the dev has already been invalidated.
	 * if we are being called at other times, then we can
	 * detach the wireless device handlers
	 */
	if (pAdapter->dev) {
		if (rtnl_held)
			pAdapter->dev->wireless_handlers = NULL;
		else {
			rtnl_lock();
			pAdapter->dev->wireless_handlers = NULL;
			rtnl_unlock();
		}
	}

	if (!test_bit(DEVICE_IFACE_OPENED, &pAdapter->event_flags)) {
		hdd_debug("iface is not opened");
		return 0;
	}

	status = wlansap_stop(sapContext);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Failed:wlansap_stop");

	status = wlansap_close(sapContext);
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("Failed:WLANSAP_close");
	pAdapter->sessionCtx.ap.sapContext = NULL;

	EXIT();
	return 0;
}

/**
 * wlan_hdd_rate_is_11g() - check if rate is 11g rate or not
 * @rate: Rate to be checked
 *
 * Return: true if rate if 11g else false
 */
static bool wlan_hdd_rate_is_11g(u8 rate)
{
	static const u8 gRateArray[8] = {12, 18, 24, 36, 48, 72,
					 96, 108}; /* actual rate * 2 */
	u8 i;

	for (i = 0; i < 8; i++) {
		if (rate == gRateArray[i])
			return true;
	}
	return false;
}

#ifdef QCA_HT_2040_COEX
/**
 * wlan_hdd_get_sap_obss() - Get SAP OBSS enable config based on HT_CAPAB IE
 * @pHostapdAdapter: Pointer to hostapd adapter
 *
 * Return: HT support channel width config value
 */
static bool wlan_hdd_get_sap_obss(hdd_adapter_t *pHostapdAdapter)
{
	uint32_t ret;
	uint8_t *ie = NULL;
	tDot11fIEHTCaps dot11_ht_cap_ie = {0};
	uint8_t ht_cap_ie[DOT11F_IE_HTCAPS_MAX_LEN];
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	beacon_data_t *beacon = pHostapdAdapter->sessionCtx.ap.beacon;

	ie = wlan_hdd_cfg80211_get_ie_ptr(beacon->tail, beacon->tail_len,
						WLAN_EID_HT_CAPABILITY);
	if (ie && ie[1]) {
		qdf_mem_copy(ht_cap_ie, &ie[2], DOT11F_IE_HTCAPS_MAX_LEN);
		ret = dot11f_unpack_ie_ht_caps((tpAniSirGlobal)hdd_ctx->hHal,
					       ht_cap_ie, ie[1],
					       &dot11_ht_cap_ie, false);
		if (DOT11F_FAILED(ret)) {
			hdd_err("unpack failed, ret: 0x%x", ret);
			return false;
		}
		return dot11_ht_cap_ie.supportedChannelWidthSet;
	}

	return false;
}
#else
static bool wlan_hdd_get_sap_obss(hdd_adapter_t *pHostapdAdapter)
{
	return false;
}
#endif
/**
 * wlan_hdd_set_channel() - set channel in sap mode
 * @wiphy: Pointer to wiphy structure
 * @dev: Pointer to net_device structure
 * @chandef: Pointer to channel definition structure
 * @channel_type: Channel type
 *
 * Return: 0 for success non-zero for failure
 */
int wlan_hdd_set_channel(struct wiphy *wiphy,
				struct net_device *dev,
				struct cfg80211_chan_def *chandef,
				enum nl80211_channel_type channel_type)
{
	hdd_adapter_t *pAdapter = NULL;
	uint32_t num_ch = 0;
	int channel = 0;
	int channel_seg2 = 0;
	hdd_context_t *pHddCtx;
	int status;

	tSmeConfigParams *sme_config;
	tsap_Config_t *sap_config;

	ENTER();


	if (NULL == dev) {
		hdd_err("Called with dev = NULL");
		return -ENODEV;
	}
	pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_SET_CHANNEL,
			 pAdapter->sessionId, channel_type));

	hdd_debug("Device_mode %s(%d)  freq = %d",
	       hdd_device_mode_to_string(pAdapter->device_mode),
	       pAdapter->device_mode, chandef->chan->center_freq);

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	status = wlan_hdd_validate_context(pHddCtx);
	if (status)
		return status;


	/*
	 * Do freq to chan conversion
	 * TODO: for 11a
	 */

	channel = ieee80211_frequency_to_channel(chandef->chan->center_freq);

	if (NL80211_CHAN_WIDTH_80P80 == chandef->width ||
	    NL80211_CHAN_WIDTH_160 == chandef->width) {
		if (chandef->center_freq2)
			channel_seg2 = ieee80211_frequency_to_channel(
					chandef->center_freq2);
		else
			hdd_err("Invalid center_freq2");
	}

	/* Check freq range */
	if ((WNI_CFG_CURRENT_CHANNEL_STAMIN > channel) ||
	    (WNI_CFG_CURRENT_CHANNEL_STAMAX < channel)) {
		hdd_err("Channel: %d is outside valid range from %d to %d",
		       channel, WNI_CFG_CURRENT_CHANNEL_STAMIN,
		       WNI_CFG_CURRENT_CHANNEL_STAMAX);
		return -EINVAL;
	}

	/* Check freq range */

	if ((WNI_CFG_CURRENT_CHANNEL_STAMIN > channel_seg2) ||
	    (WNI_CFG_CURRENT_CHANNEL_STAMAX < channel_seg2)) {
		hdd_err("Channel: %d is outside valid range from %d to %d",
		       channel_seg2, WNI_CFG_CURRENT_CHANNEL_STAMIN,
		       WNI_CFG_CURRENT_CHANNEL_STAMAX);
		return -EINVAL;
	}

	num_ch = WNI_CFG_VALID_CHANNEL_LIST_LEN;

	if ((QDF_SAP_MODE != pAdapter->device_mode) &&
	    (QDF_P2P_GO_MODE != pAdapter->device_mode)) {
		if (QDF_STATUS_SUCCESS !=
		    wlan_hdd_validate_operation_channel(pAdapter, channel)) {
			hdd_err("Invalid Channel: %d", channel);
			return -EINVAL;
		}
		hdd_debug("set channel to [%d] for device mode %s(%d)",
		       channel,
		       hdd_device_mode_to_string(pAdapter->device_mode),
		       pAdapter->device_mode);
	}

	if ((pAdapter->device_mode == QDF_STA_MODE)
	    || (pAdapter->device_mode == QDF_P2P_CLIENT_MODE)
	    ) {
		hdd_wext_state_t *pWextState =
			WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter);
		tCsrRoamProfile *pRoamProfile = &pWextState->roamProfile;
		hdd_station_ctx_t *pHddStaCtx =
			WLAN_HDD_GET_STATION_CTX_PTR(pAdapter);

		if (eConnectionState_IbssConnected ==
		    pHddStaCtx->conn_info.connState) {
			/* Link is up then return cant set channel */
			hdd_err("IBSS Associated, can't set the channel");
			return -EINVAL;
		}

		num_ch = pRoamProfile->ChannelInfo.numOfChannels = 1;
		pHddStaCtx->conn_info.operationChannel = channel;
		pRoamProfile->ChannelInfo.ChannelList =
			&pHddStaCtx->conn_info.operationChannel;
	} else if ((pAdapter->device_mode == QDF_SAP_MODE)
		   || (pAdapter->device_mode == QDF_P2P_GO_MODE)
		   ) {
		sap_config = &((WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->sapConfig);
		if (QDF_P2P_GO_MODE == pAdapter->device_mode) {
			if (QDF_STATUS_SUCCESS !=
			    wlan_hdd_validate_operation_channel(pAdapter,
								channel)) {
				hdd_err("Invalid Channel: %d", channel);
				return -EINVAL;
			}
			sap_config->channel = channel;
			sap_config->ch_params.center_freq_seg1 = channel_seg2;
		} else {
			/* set channel to what hostapd configured */
			if (QDF_STATUS_SUCCESS !=
				wlan_hdd_validate_operation_channel(pAdapter,
								channel)) {
				hdd_err("Invalid Channel: %d", channel);
				return -EINVAL;
			}

			sap_config->channel = channel;
			sap_config->ch_params.center_freq_seg1 = channel_seg2;

			sme_config = qdf_mem_malloc(sizeof(*sme_config));

			if (!sme_config) {
				hdd_err("Unable to allocate memory for smeconfig!");
				return -ENOMEM;
			}
			sme_get_config_param(pHddCtx->hHal, sme_config);
			switch (channel_type) {
			case NL80211_CHAN_HT20:
			case NL80211_CHAN_NO_HT:
				sme_config->csrConfig.obssEnabled = false;
				sap_config->sec_ch = 0;
				break;
			case NL80211_CHAN_HT40MINUS:
				sap_config->sec_ch = sap_config->channel - 4;
				break;
			case NL80211_CHAN_HT40PLUS:
				sap_config->sec_ch = sap_config->channel + 4;
				break;
			default:
				hdd_err("Error!!! Invalid HT20/40 mode !");
				qdf_mem_free(sme_config);
				return -EINVAL;
			}
			sme_config->csrConfig.obssEnabled =
				wlan_hdd_get_sap_obss(pAdapter);

			sme_update_config(pHddCtx->hHal, sme_config);
			qdf_mem_free(sme_config);
		}
	} else {
		hdd_err("Invalid device mode failed to set valid channel");
		return -EINVAL;
	}
	EXIT();
	return status;
}

/**
 * wlan_hdd_check_11gmode() - check for 11g mode
 * @pIe: Pointer to IE
 * @require_ht: Pointer to require ht
 * @require_vht: Pointer to require vht
 * @pCheckRatesfor11g: Pointer to check rates for 11g mode
 * @pSapHw_mode: SAP HW mode
 *
 * Check for 11g rate and set proper 11g only mode
 *
 * Return: none
 */
static void wlan_hdd_check_11gmode(u8 *pIe, u8 *require_ht, u8 *require_vht,
				   u8 *pCheckRatesfor11g,
				   eCsrPhyMode *pSapHw_mode)
{
	u8 i, num_rates = pIe[0];

	pIe += 1;
	for (i = 0; i < num_rates; i++) {
		if (*pCheckRatesfor11g
		    && (true == wlan_hdd_rate_is_11g(pIe[i] & RATE_MASK))) {
			/* If rate set have 11g rate than change the mode
			 * to 11G
			 */
			*pSapHw_mode = eCSR_DOT11_MODE_11g;
			if (pIe[i] & BASIC_RATE_MASK) {
				/* If we have 11g rate as  basic rate, it
				 * means mode is 11g only mode.
				 */
				*pSapHw_mode = eCSR_DOT11_MODE_11g_ONLY;
				*pCheckRatesfor11g = false;
			}
		} else {
			if ((BASIC_RATE_MASK |
				WLAN_BSS_MEMBERSHIP_SELECTOR_HT_PHY) == pIe[i])
				*require_ht = true;
			else if ((BASIC_RATE_MASK |
				WLAN_BSS_MEMBERSHIP_SELECTOR_VHT_PHY) == pIe[i])
				*require_vht = true;
		}
	}
}


/**
 * wlan_hdd_add_hostapd_conf_vsie() - configure Vendor IE in sap mode
 * @pHostapdAdapter: Pointer to hostapd adapter
 * @genie: Pointer to Vendor IE
 * @total_ielen: Pointer to store total ie length
 *
 * Return: none
 */
static void wlan_hdd_add_hostapd_conf_vsie(hdd_adapter_t *pHostapdAdapter,
					   uint8_t *genie,
					   uint16_t *total_ielen)
{
	beacon_data_t *pBeacon = pHostapdAdapter->sessionCtx.ap.beacon;
	int left = pBeacon->tail_len;
	uint8_t *ptr = pBeacon->tail;
	uint8_t elem_id, elem_len;
	uint16_t ielen = 0;
	bool skip_ie;

	if (NULL == ptr || 0 == left)
		return;

	while (left >= 2) {
		elem_id = ptr[0];
		elem_len = ptr[1];
		left -= 2;
		if (elem_len > left) {
			hdd_err("**Invalid IEs eid: %d elem_len: %d left: %d**",
				elem_id, elem_len, left);
			return;
		}
		if (IE_EID_VENDOR == elem_id) {
			/*
			 * skipping the Vendor IE's which we don't want to
			 * include or it will be included by existing code.
			 */
			if (elem_len >= WPS_OUI_TYPE_SIZE &&
			    (!qdf_mem_cmp(&ptr[2], WHITELIST_OUI_TYPE,
					  WPA_OUI_TYPE_SIZE) ||
			     !qdf_mem_cmp(&ptr[2], BLACKLIST_OUI_TYPE,
					  WPA_OUI_TYPE_SIZE) ||
			     !qdf_mem_cmp(&ptr[2], "\x00\x50\xf2\x02",
					  WPA_OUI_TYPE_SIZE) ||
			     !qdf_mem_cmp(&ptr[2], WPA_OUI_TYPE,
					  WPA_OUI_TYPE_SIZE)))
				skip_ie = true;
			else
				skip_ie = false;

			if (!skip_ie) {
				ielen = ptr[1] + 2;
				if ((*total_ielen + ielen) <= MAX_GENIE_LEN) {
					qdf_mem_copy(&genie[*total_ielen], ptr,
						     ielen);
					*total_ielen += ielen;
				} else {
					hdd_err("IE Length is too big IEs eid: %d elem_len: %d total_ie_lent: %d",
					       elem_id, elem_len, *total_ielen);
				}
			}
		}

		left -= elem_len;
		ptr += (elem_len + 2);
	}
}

/**
 * wlan_hdd_add_extra_ie() - add extra ies in beacon
 * @pHostapdAdapter: Pointer to hostapd adapter
 * @genie: Pointer to extra ie
 * @total_ielen: Pointer to store total ie length
 * @temp_ie_id: ID of extra ie
 *
 * Return: none
 */
static void wlan_hdd_add_extra_ie(hdd_adapter_t *pHostapdAdapter,
				  uint8_t *genie, uint16_t *total_ielen,
				  uint8_t temp_ie_id)
{
	beacon_data_t *pBeacon = pHostapdAdapter->sessionCtx.ap.beacon;
	int left = pBeacon->tail_len;
	uint8_t *ptr = pBeacon->tail;
	uint8_t elem_id, elem_len;
	uint16_t ielen = 0;

	if (NULL == ptr || 0 == left)
		return;

	while (left >= 2) {
		elem_id = ptr[0];
		elem_len = ptr[1];
		left -= 2;
		if (elem_len > left) {
			hdd_err("**Invalid IEs eid: %d elem_len: %d left: %d**",
			       elem_id, elem_len, left);
			return;
		}

		if (temp_ie_id == elem_id) {
			ielen = ptr[1] + 2;
			if ((*total_ielen + ielen) <= MAX_GENIE_LEN) {
				qdf_mem_copy(&genie[*total_ielen], ptr, ielen);
				*total_ielen += ielen;
			} else {
				hdd_err("IE Length is too big IEs eid: %d elem_len: %d total_ie_len: %d",
				       elem_id, elem_len, *total_ielen);
			}
		}

		left -= elem_len;
		ptr += (elem_len + 2);
	}
}

/**
 * wlan_hdd_cfg80211_alloc_new_beacon() - alloc beacon in ap mode
 * @pAdapter: Pointer to hostapd adapter
 * @ppBeacon: Pointer to pointer to beacon data
 * @params: Pointer to beacon parameters
 * @dtim_period: DTIM period
 *
 * Return: 0 for success non-zero for failure
 */
static int
wlan_hdd_cfg80211_alloc_new_beacon(hdd_adapter_t *pAdapter,
				   beacon_data_t **ppBeacon,
				   struct cfg80211_beacon_data *params,
				   int dtim_period)
{
	int size;
	beacon_data_t *beacon = NULL;
	beacon_data_t *old = NULL;
	int head_len, tail_len, proberesp_ies_len, assocresp_ies_len;
	const u8 *head, *tail, *proberesp_ies, *assocresp_ies;

	ENTER();
	if (params->head && !params->head_len) {
		hdd_err("head_len is NULL");
		return -EINVAL;
	}

	old = pAdapter->sessionCtx.ap.beacon;

	if (!params->head && !old) {
		hdd_err("session: %d old and new heads points to NULL",
		       pAdapter->sessionId);
		return -EINVAL;
	}

	if (params->head) {
		head_len = params->head_len;
		head = params->head;
	} else {
		head_len = old->head_len;
		head = old->head;
	}

	if (params->tail || !old) {
		tail_len = params->tail_len;
		tail = params->tail;
	} else {
		tail_len = old->tail_len;
		tail = old->tail;
	}

	if (params->proberesp_ies || !old) {
		proberesp_ies_len = params->proberesp_ies_len;
		proberesp_ies = params->proberesp_ies;
	} else {
		proberesp_ies_len = old->proberesp_ies_len;
		proberesp_ies = old->proberesp_ies;
	}

	if (params->assocresp_ies || !old) {
		assocresp_ies_len = params->assocresp_ies_len;
		assocresp_ies = params->assocresp_ies;
	} else {
		assocresp_ies_len = old->assocresp_ies_len;
		assocresp_ies = old->assocresp_ies;
	}

	size = sizeof(beacon_data_t) + head_len + tail_len +
		proberesp_ies_len + assocresp_ies_len;

	beacon = qdf_mem_malloc(size);

	if (beacon == NULL) {
		hdd_err("Mem allocation for beacon failed");
		return -ENOMEM;
	}
	if (dtim_period)
		beacon->dtim_period = dtim_period;
	else if (old)
		beacon->dtim_period = old->dtim_period;
	/* -----------------------------------------------
	 * | head | tail | proberesp_ies | assocresp_ies |
	 * -----------------------------------------------
	 */
	beacon->head = ((u8 *) beacon) + sizeof(beacon_data_t);
	beacon->tail = beacon->head + head_len;
	beacon->proberesp_ies = beacon->tail + tail_len;
	beacon->assocresp_ies = beacon->proberesp_ies + proberesp_ies_len;

	beacon->head_len = head_len;
	beacon->tail_len = tail_len;
	beacon->proberesp_ies_len = proberesp_ies_len;
	beacon->assocresp_ies_len = assocresp_ies_len;

	if (head && head_len)
		memcpy(beacon->head, head, head_len);
	if (tail && tail_len)
		memcpy(beacon->tail, tail, tail_len);
	if (proberesp_ies && proberesp_ies_len)
		memcpy(beacon->proberesp_ies, proberesp_ies, proberesp_ies_len);
	if (assocresp_ies && assocresp_ies_len)
		memcpy(beacon->assocresp_ies, assocresp_ies, assocresp_ies_len);

	*ppBeacon = beacon;

	pAdapter->sessionCtx.ap.beacon = NULL;
	qdf_mem_free(old);

	return 0;

}

#ifdef QCA_HT_2040_COEX
static void wlan_hdd_add_sap_obss_scan_ie(
	hdd_adapter_t *hostapd_adapter, uint8_t *ie_buf, uint16_t *ie_len)
{
	if (QDF_SAP_MODE == hostapd_adapter->device_mode) {
		if (wlan_hdd_get_sap_obss(hostapd_adapter))
			wlan_hdd_add_extra_ie(hostapd_adapter, ie_buf, ie_len,
					WLAN_EID_OVERLAP_BSS_SCAN_PARAM);
	}
}
#else
static void wlan_hdd_add_sap_obss_scan_ie(
	hdd_adapter_t *hostapd_adapter, uint8_t *ie_buf, uint16_t *ie_len)
{
}
#endif

/**
 * wlan_hdd_cfg80211_update_apies() - update ap mode ies
 * @adapter: Pointer to hostapd adapter
 *
 * Return: 0 for success non-zero for failure
 */
int wlan_hdd_cfg80211_update_apies(hdd_adapter_t *adapter)
{
	uint8_t *genie;
	uint16_t total_ielen = 0;
	int ret = 0;
	tsap_Config_t *pConfig;
	tSirUpdateIE updateIE;
	beacon_data_t *beacon = NULL;
	uint16_t proberesp_ies_len;
	uint8_t *proberesp_ies = NULL;

	pConfig = &adapter->sessionCtx.ap.sapConfig;
	beacon = adapter->sessionCtx.ap.beacon;
	if (!beacon) {
		hdd_err("Beacon is NULL !");
		return -EINVAL;
	}

	genie = qdf_mem_malloc(MAX_GENIE_LEN);

	if (genie == NULL)
		return -ENOMEM;

	wlan_hdd_add_extra_ie(adapter, genie, &total_ielen,
			      WLAN_EID_VHT_TX_POWER_ENVELOPE);

	/* Extract and add the extended capabilities and interworking IE */
	wlan_hdd_add_extra_ie(adapter, genie, &total_ielen,
			      WLAN_EID_EXT_CAPABILITY);

	wlan_hdd_add_extra_ie(adapter, genie, &total_ielen,
			      WLAN_EID_INTERWORKING);

#ifdef FEATURE_WLAN_WAPI
	if (QDF_SAP_MODE == adapter->device_mode) {
		wlan_hdd_add_extra_ie(adapter, genie, &total_ielen,
				      WLAN_EID_WAPI);
	}
#endif

	wlan_hdd_add_hostapd_conf_vsie(adapter, genie,
				       &total_ielen);

	wlan_hdd_add_sap_obss_scan_ie(adapter, genie, &total_ielen);

	qdf_copy_macaddr(&updateIE.bssid, &adapter->macAddressCurrent);
	updateIE.smeSessionId = adapter->sessionId;

	if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
		updateIE.ieBufferlength = total_ielen;
		updateIE.pAdditionIEBuffer = genie;
		updateIE.append = false;
		updateIE.notify = true;
		if (sme_update_add_ie(WLAN_HDD_GET_HAL_CTX(adapter),
				      &updateIE,
				      eUPDATE_IE_PROBE_BCN) ==
		    QDF_STATUS_E_FAILURE) {
			hdd_err("Could not pass on Add Ie probe beacon data");
			ret = -EINVAL;
			goto done;
		}
		wlansap_reset_sap_config_add_ie(pConfig, eUPDATE_IE_PROBE_BCN);
	} else {
		wlansap_update_sap_config_add_ie(pConfig,
						 genie,
						 total_ielen,
						 eUPDATE_IE_PROBE_BCN);
	}

	/* Added for Probe Response IE */
	proberesp_ies = qdf_mem_malloc(beacon->proberesp_ies_len +
				      MAX_GENIE_LEN);
	if (proberesp_ies == NULL) {
		hdd_err("mem alloc failed for probe resp ies, size: %d",
			beacon->proberesp_ies_len + MAX_GENIE_LEN);
		ret = -EINVAL;
		goto done;
	}
	qdf_mem_copy(proberesp_ies, beacon->proberesp_ies,
		    beacon->proberesp_ies_len);
	proberesp_ies_len = beacon->proberesp_ies_len;

	wlan_hdd_add_sap_obss_scan_ie(adapter, proberesp_ies,
				     &proberesp_ies_len);

	if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
		updateIE.ieBufferlength = proberesp_ies_len;
		updateIE.pAdditionIEBuffer = proberesp_ies;
		updateIE.append = false;
		updateIE.notify = false;
		if (sme_update_add_ie(WLAN_HDD_GET_HAL_CTX(adapter),
				      &updateIE,
				      eUPDATE_IE_PROBE_RESP) ==
		    QDF_STATUS_E_FAILURE) {
			hdd_err("Could not pass on PROBE_RESP add Ie data");
			ret = -EINVAL;
			goto done;
		}
		wlansap_reset_sap_config_add_ie(pConfig, eUPDATE_IE_PROBE_RESP);
	} else {
		wlansap_update_sap_config_add_ie(pConfig,
						 proberesp_ies,
						 proberesp_ies_len,
						 eUPDATE_IE_PROBE_RESP);
	}

	/* Assoc resp Add ie Data */
	if (test_bit(SOFTAP_BSS_STARTED, &adapter->event_flags)) {
		updateIE.ieBufferlength = beacon->assocresp_ies_len;
		updateIE.pAdditionIEBuffer = (uint8_t *) beacon->assocresp_ies;
		updateIE.append = false;
		updateIE.notify = false;
		if (sme_update_add_ie(WLAN_HDD_GET_HAL_CTX(adapter),
				      &updateIE,
				      eUPDATE_IE_ASSOC_RESP) ==
		    QDF_STATUS_E_FAILURE) {
			hdd_err("Could not pass on Add Ie Assoc Response data");
			ret = -EINVAL;
			goto done;
		}
		wlansap_reset_sap_config_add_ie(pConfig, eUPDATE_IE_ASSOC_RESP);
	} else {
		wlansap_update_sap_config_add_ie(pConfig,
						 beacon->assocresp_ies,
						 beacon->assocresp_ies_len,
						 eUPDATE_IE_ASSOC_RESP);
	}

done:
	qdf_mem_free(genie);
	qdf_mem_free(proberesp_ies);
	return ret;
}

/**
 * wlan_hdd_set_sap_hwmode() - set sap hw mode
 * @pHostapdAdapter: Pointer to hostapd adapter
 *
 * Return: none
 */
static void wlan_hdd_set_sap_hwmode(hdd_adapter_t *pHostapdAdapter)
{
	tsap_Config_t *pConfig = &pHostapdAdapter->sessionCtx.ap.sapConfig;
	beacon_data_t *pBeacon = pHostapdAdapter->sessionCtx.ap.beacon;
	struct ieee80211_mgmt *pMgmt_frame =
		(struct ieee80211_mgmt *)pBeacon->head;
	u8 checkRatesfor11g = true;
	u8 require_ht = false, require_vht = false;
	u8 *pIe = NULL;

	pConfig->SapHw_mode = eCSR_DOT11_MODE_11b;

	pIe = wlan_hdd_cfg80211_get_ie_ptr(&pMgmt_frame->u.beacon.variable[0],
					   pBeacon->head_len,
					   WLAN_EID_SUPP_RATES);
	if (pIe != NULL) {
		pIe += 1;
		wlan_hdd_check_11gmode(pIe, &require_ht, &require_vht,
			&checkRatesfor11g, &pConfig->SapHw_mode);
	}

	pIe = wlan_hdd_cfg80211_get_ie_ptr(pBeacon->tail, pBeacon->tail_len,
					   WLAN_EID_EXT_SUPP_RATES);
	if (pIe != NULL) {
		pIe += 1;
		wlan_hdd_check_11gmode(pIe, &require_ht, &require_vht,
			&checkRatesfor11g, &pConfig->SapHw_mode);
	}

	if (pConfig->channel > 14)
		pConfig->SapHw_mode = eCSR_DOT11_MODE_11a;

	pIe = wlan_hdd_cfg80211_get_ie_ptr(pBeacon->tail, pBeacon->tail_len,
					   WLAN_EID_HT_CAPABILITY);

	if (pIe) {
		pConfig->SapHw_mode = eCSR_DOT11_MODE_11n;
		if (require_ht)
			pConfig->SapHw_mode = eCSR_DOT11_MODE_11n_ONLY;
	}

	pIe = wlan_hdd_cfg80211_get_ie_ptr(pBeacon->tail, pBeacon->tail_len,
					   WLAN_EID_VHT_CAPABILITY);

	if (pIe) {
		pConfig->SapHw_mode = eCSR_DOT11_MODE_11ac;
		if (require_vht)
			pConfig->SapHw_mode = eCSR_DOT11_MODE_11ac_ONLY;
	}
}

/**
 * wlan_hdd_config_acs() - config ACS needed parameters
 * @hdd_ctx: HDD context
 * @adapter: Adapter pointer
 *
 * This function get ACS related INI paramters and populated
 * sap config and smeConfig for ACS needed configurations.
 *
 * Return: The QDF_STATUS code associated with performing the operation.
 */
QDF_STATUS wlan_hdd_config_acs(hdd_context_t *hdd_ctx, hdd_adapter_t *adapter)
{
	tsap_Config_t *sap_config;
	struct hdd_config *ini_config;
	tHalHandle hal;

	hal = WLAN_HDD_GET_HAL_CTX(adapter);
	sap_config = &adapter->sessionCtx.ap.sapConfig;
	ini_config = hdd_ctx->config;

	sap_config->enOverLapCh = !!hdd_ctx->config->gEnableOverLapCh;

#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
	hdd_debug("HDD_ACS_SKIP_STATUS = %d",
						hdd_ctx->skip_acs_scan_status);
	if (hdd_ctx->skip_acs_scan_status == eSAP_SKIP_ACS_SCAN) {
		hdd_adapter_t *con_sap_adapter;
		tsap_Config_t *con_sap_config = NULL;

		con_sap_adapter = hdd_get_con_sap_adapter(adapter, false);

		if (con_sap_adapter)
			con_sap_config =
				&con_sap_adapter->sessionCtx.ap.sapConfig;

		sap_config->acs_cfg.skip_scan_status = eSAP_DO_NEW_ACS_SCAN;

		if (con_sap_config &&
			con_sap_config->acs_cfg.acs_mode == true &&
			hdd_ctx->skip_acs_scan_status == eSAP_SKIP_ACS_SCAN &&
			con_sap_config->acs_cfg.hw_mode ==
						sap_config->acs_cfg.hw_mode) {
			uint8_t con_sap_st_ch, con_sap_end_ch;
			uint8_t cur_sap_st_ch, cur_sap_end_ch;
			uint8_t bandStartChannel, bandEndChannel;

			con_sap_st_ch =
					con_sap_config->acs_cfg.start_ch;
			con_sap_end_ch =
					con_sap_config->acs_cfg.end_ch;
			cur_sap_st_ch = sap_config->acs_cfg.start_ch;
			cur_sap_end_ch = sap_config->acs_cfg.end_ch;

			wlansap_extend_to_acs_range(
					&cur_sap_st_ch, &cur_sap_end_ch,
					&bandStartChannel, &bandEndChannel);

			wlansap_extend_to_acs_range(
					&con_sap_st_ch, &con_sap_end_ch,
					&bandStartChannel, &bandEndChannel);

			if (con_sap_st_ch <= cur_sap_st_ch &&
					con_sap_end_ch >= cur_sap_end_ch) {
				sap_config->acs_cfg.skip_scan_status =
							eSAP_SKIP_ACS_SCAN;

			} else if (con_sap_st_ch >= cur_sap_st_ch &&
					con_sap_end_ch >= cur_sap_end_ch) {
				sap_config->acs_cfg.skip_scan_status =
							eSAP_DO_PAR_ACS_SCAN;

				sap_config->acs_cfg.skip_scan_range1_stch =
							cur_sap_st_ch;
				sap_config->acs_cfg.skip_scan_range1_endch =
							con_sap_st_ch - 1;
				sap_config->acs_cfg.skip_scan_range2_stch =
							0;
				sap_config->acs_cfg.skip_scan_range2_endch =
							0;

			} else if (con_sap_st_ch <= cur_sap_st_ch &&
				con_sap_end_ch <= cur_sap_end_ch) {
				sap_config->acs_cfg.skip_scan_status =
							eSAP_DO_PAR_ACS_SCAN;

				sap_config->acs_cfg.skip_scan_range1_stch =
							con_sap_end_ch + 1;
				sap_config->acs_cfg.skip_scan_range1_endch =
							cur_sap_end_ch;
				sap_config->acs_cfg.skip_scan_range2_stch =
							0;
				sap_config->acs_cfg.skip_scan_range2_endch =
							0;

			} else if (con_sap_st_ch >= cur_sap_st_ch &&
				con_sap_end_ch <= cur_sap_end_ch) {
				sap_config->acs_cfg.skip_scan_status =
							eSAP_DO_PAR_ACS_SCAN;

				sap_config->acs_cfg.skip_scan_range1_stch =
							cur_sap_st_ch;
				sap_config->acs_cfg.skip_scan_range1_endch =
							con_sap_st_ch - 1;
				sap_config->acs_cfg.skip_scan_range2_stch =
							con_sap_end_ch;
				sap_config->acs_cfg.skip_scan_range2_endch =
							cur_sap_end_ch + 1;

			} else
				sap_config->acs_cfg.skip_scan_status =
							eSAP_DO_NEW_ACS_SCAN;


			hdd_debug(FL(
				"SecAP ACS Skip=%d, ACS CH RANGE=%d-%d, %d-%d"),
				sap_config->acs_cfg.skip_scan_status,
				sap_config->acs_cfg.skip_scan_range1_stch,
				sap_config->acs_cfg.skip_scan_range1_endch,
				sap_config->acs_cfg.skip_scan_range2_stch,
				sap_config->acs_cfg.skip_scan_range2_endch);
		}
	}
#endif

	return QDF_STATUS_SUCCESS;
}

/**
 * wlan_hdd_setup_driver_overrides : Overrides SAP / P2P GO Params
 * @adapter: pointer to adapter struct
 *
 * This function overrides SAP / P2P Go configuration based on driver INI
 * parameters for 11AC override and ACS. This overrides are done to support
 * android legacy configuration method.
 *
 * NOTE: Non android platform supports concurrency and these overrides shall
 * not be used. Also future driver based overrides shall be consolidated in this
 * function only. Avoid random overrides in other location based on ini.
 *
 * Return: 0 for Success or Negative error codes.
 */
static int wlan_hdd_setup_driver_overrides(hdd_adapter_t *ap_adapter)
{
	tsap_Config_t *sap_cfg = &ap_adapter->sessionCtx.ap.sapConfig;
	hdd_context_t *hdd_ctx = WLAN_HDD_GET_CTX(ap_adapter);

	/* Fixed channel 11AC override:
	 * 11AC override in qcacld is introduced for following reasons:
	 * 1. P2P GO also follows start_bss and since p2p GO could not be
	 *    configured to setup VHT channel width in wpa_supplicant
	 * 2. Android UI does not provide advanced configuration options for SAP
	 *
	 * Default override enabled (for android). MDM shall disable this in ini
	 */
	/*
	 * sub_20 MHz channel width is incompatible with 11AC rates, hence do
	 * not allow 11AC rates or more than 20 MHz channel width when
	 * enable_sub_20_channel_width is non zero
	 */
	if (!hdd_ctx->config->enable_sub_20_channel_width &&
			(sap_cfg->SapHw_mode == eCSR_DOT11_MODE_11n ||
			sap_cfg->SapHw_mode == eCSR_DOT11_MODE_11ac ||
			sap_cfg->SapHw_mode == eCSR_DOT11_MODE_11ac_ONLY) &&
			((ap_adapter->device_mode == QDF_SAP_MODE &&
			!hdd_ctx->config->sap_force_11n_for_11ac &&
			hdd_ctx->config->sap_11ac_override) ||
			(ap_adapter->device_mode == QDF_P2P_GO_MODE &&
			!hdd_ctx->config->go_force_11n_for_11ac &&
			hdd_ctx->config->go_11ac_override))) {
		hdd_debug("** Driver force 11AC override for SAP/Go **");

		/* 11n only shall not be overridden since it may be on purpose*/
		if (sap_cfg->SapHw_mode == eCSR_DOT11_MODE_11n)
			sap_cfg->SapHw_mode = eCSR_DOT11_MODE_11ac;

		if (sap_cfg->channel >= 36) {
			sap_cfg->ch_width_orig =
					hdd_ctx->config->vhtChannelWidth;
		} else {
			/*
			 * Allow 40 Mhz in 2.4 Ghz only if indicated by
			 * supplicant after OBSS scan and if 2.4 Ghz channel
			 * bonding is set in INI
			 */
			if (sap_cfg->ch_width_orig >= eHT_CHANNEL_WIDTH_40MHZ &&
			   hdd_ctx->config->nChannelBondingMode24GHz)
				sap_cfg->ch_width_orig =
					eHT_CHANNEL_WIDTH_40MHZ;
			else
				sap_cfg->ch_width_orig =
					eHT_CHANNEL_WIDTH_20MHZ;
		}
	}
	sap_cfg->ch_params.ch_width = sap_cfg->ch_width_orig;
	cds_set_channel_params(sap_cfg->channel,
				sap_cfg->sec_ch, &sap_cfg->ch_params);

	return 0;
}

#ifdef WLAN_FEATURE_UDP_RESPONSE_OFFLOAD
/**
 * wlan_hdd_set_udp_resp_offload() - get specific udp and response udp info from
 * ini file
 * @padapter: hdd adapter pointer
 * @enable: enable or disable the specific udp and response behaviour
 *
 * This function reads specific udp and response udp related info from ini file,
 * these configurations will be sent to fw through wmi.
 *
 * Return: 0 on success, otherwise error value
 */
static int wlan_hdd_set_udp_resp_offload(hdd_adapter_t *padapter, bool enable)
{
	hdd_context_t *phddctx = WLAN_HDD_GET_CTX(padapter);
	struct hdd_config *pcfg_ini = phddctx->config;
	struct udp_resp_offload udp_resp_cmd_info;
	QDF_STATUS status;
	uint8_t udp_payload_filter_len;
	uint8_t udp_response_payload_len;

	hdd_debug("udp_resp_offload enable flag is %d", enable);

	/* prepare the request to send to SME */
	if ((enable == true) &&
	    (pcfg_ini->udp_resp_offload_support)) {
		if (pcfg_ini->response_payload[0] != '\0') {
			udp_resp_cmd_info.vdev_id = padapter->sessionId;
			udp_resp_cmd_info.enable = 1;
			udp_resp_cmd_info.dest_port =
					pcfg_ini->dest_port;

			udp_payload_filter_len =
				strlen(pcfg_ini->payload_filter);
			hdd_debug("payload_filter[%s]",
				pcfg_ini->payload_filter);
			udp_response_payload_len =
				strlen(pcfg_ini->response_payload);
			hdd_debug("response_payload[%s]",
				pcfg_ini->response_payload);

			qdf_mem_copy(udp_resp_cmd_info.udp_payload_filter,
					pcfg_ini->payload_filter,
					udp_payload_filter_len + 1);

			qdf_mem_copy(udp_resp_cmd_info.udp_response_payload,
					pcfg_ini->response_payload,
					udp_response_payload_len + 1);

			status = sme_set_udp_resp_offload(&udp_resp_cmd_info);
			if (QDF_STATUS_E_FAILURE == status) {
				hdd_err("sme_set_udp_resp_offload failure!");
				return -EIO;
			}
			hdd_debug("sme_set_udp_resp_offload success!");
		}
	} else {
		udp_resp_cmd_info.vdev_id = padapter->sessionId;
		udp_resp_cmd_info.enable = 0;
		udp_resp_cmd_info.dest_port = 0;
		udp_resp_cmd_info.udp_payload_filter[0] = '\0';
		udp_resp_cmd_info.udp_response_payload[0] = '\0';
		status = sme_set_udp_resp_offload(&udp_resp_cmd_info);
		if (QDF_STATUS_E_FAILURE == status) {
			hdd_err("sme_set_udp_resp_offload failure!");
			return -EIO;
		}
		hdd_debug("sme_set_udp_resp_offload success!");
	}
	return 0;
}
#else
static inline int wlan_hdd_set_udp_resp_offload(hdd_adapter_t *padapter,
				bool enable)
{
	return 0;
}
#endif

static void hdd_check_and_disconnect_sta_on_invalid_channel(
		hdd_context_t *hdd_ctx)
{
	hdd_adapter_t *sta_adapter;
	uint8_t sta_chan;

	sta_chan = hdd_get_operating_channel(hdd_ctx, QDF_STA_MODE);

	if (!sta_chan) {
		hdd_err("STA not connected");
		return;
	}

	hdd_err("STA connected on chan %d", sta_chan);

	if (sme_is_channel_valid(hdd_ctx->hHal, sta_chan)) {
		hdd_err("STA connected on chan %d and it is valid", sta_chan);
		return;
	}

	sta_adapter = hdd_get_adapter(hdd_ctx, QDF_STA_MODE);

	if (!sta_adapter) {
		hdd_err("STA adapter does not exist");
		return;
	}

	hdd_err("chan %d not valid, issue disconnect", sta_chan);
	/* Issue Disconnect request */
	wlan_hdd_disconnect(sta_adapter, eCSR_DISCONNECT_REASON_DEAUTH);
}

/**
 * wlan_hdd_get_wiphy_channel() - Get wiphy channel
 * @wiphy: Pointer to wiphy structure
 * @freq: Frequency of the channel for which the wiphy hw value is required
 *
 * Return: wiphy channel for valid frequency else return NULL
 */
static struct ieee80211_channel *wlan_hdd_get_wiphy_channel(
						struct wiphy *wiphy,
						uint32_t freq)
{
	uint32_t band_num, channel_num;
	struct ieee80211_channel *wiphy_channel = NULL;

	for (band_num = 0; band_num < HDD_NUM_NL80211_BANDS; band_num++) {
		for (channel_num = 0; channel_num <
				wiphy->bands[band_num]->n_channels;
				channel_num++) {
			wiphy_channel = &(wiphy->bands[band_num]->
							channels[channel_num]);
			if (wiphy_channel->center_freq == freq)
				return wiphy_channel;
		}
	}
	return wiphy_channel;
}

int wlan_hdd_restore_channels(hdd_context_t *hdd_ctx)
{
	struct hdd_cache_channels *cache_chann;
	struct wiphy *wiphy;
	int freq, status, rf_channel;
	int i;
	struct ieee80211_channel *wiphy_channel = NULL;

	ENTER();

	if (!hdd_ctx) {
		hdd_err("HDD Context is NULL");
		return -EINVAL;
	}

	wiphy = hdd_ctx->wiphy;
	if (!wiphy) {
		hdd_err("Wiphy is NULL");
		return -EINVAL;
	}

	qdf_mutex_acquire(&hdd_ctx->cache_channel_lock);

	cache_chann = hdd_ctx->original_channels;

	if (!cache_chann || !cache_chann->num_channels) {
		qdf_mutex_release(&hdd_ctx->cache_channel_lock);
		hdd_err("channel list is NULL or num channels are zero");
		return -EINVAL;
	}

	for (i = 0; i < cache_chann->num_channels; i++) {
		status = hdd_wlan_get_freq(
				cache_chann->channel_info[i].channel_num,
				&freq);
		if (status)
			continue;

		wiphy_channel = wlan_hdd_get_wiphy_channel(wiphy, freq);
		if (!wiphy_channel)
			continue;
		rf_channel = wiphy_channel->hw_value;
		/*
		 * Restore the orginal states of the channels
		 * only if we have cached non zero values
		 */
		if (cache_chann->channel_info[i].reg_status)
			cds_set_channel_state(rf_channel,
					      cache_chann->
						channel_info[i].reg_status);

		if (cache_chann->channel_info[i].wiphy_status && wiphy_channel)
			wiphy_channel->flags =
				cache_chann->channel_info[i].wiphy_status;
	}

	qdf_mutex_release(&hdd_ctx->cache_channel_lock);

	status = sme_update_channel_list(hdd_ctx->hHal);
	if (status)
		hdd_err("Can't Restore channel list");
	EXIT();

	return 0;
}

int wlan_hdd_disable_channels(hdd_context_t *hdd_ctx)
{
	struct hdd_cache_channels *cache_chann;
	struct wiphy *wiphy;
	int freq, status, rf_channel;
	int i;
	struct ieee80211_channel *wiphy_channel = NULL;

	ENTER();

	if (!hdd_ctx) {
		hdd_err("HDD Context is NULL");
		return -EINVAL;
	}

	wiphy = hdd_ctx->wiphy;
	if (!wiphy) {
		hdd_err("Wiphy is NULL");
		return -EINVAL;
	}

	qdf_mutex_acquire(&hdd_ctx->cache_channel_lock);
	cache_chann = hdd_ctx->original_channels;

	if (!cache_chann || !cache_chann->num_channels) {
		qdf_mutex_release(&hdd_ctx->cache_channel_lock);
		hdd_err("channel list is NULL or num channels are zero");
		return -EINVAL;
	}

	for (i = 0; i < cache_chann->num_channels; i++) {
		status = hdd_wlan_get_freq(
				cache_chann->channel_info[i].channel_num,
				&freq);
		if (status)
			continue;
		wiphy_channel = wlan_hdd_get_wiphy_channel(wiphy, freq);
		if (!wiphy_channel)
			continue;
		rf_channel = wiphy_channel->hw_value;
		/*
		 * Cache the current states of
		 * the channels
		 */
		cache_chann->channel_info[i].reg_status =
						cds_get_channel_state(
							rf_channel);
		cache_chann->channel_info[i].wiphy_status =
							wiphy_channel->flags;
		hdd_debug("Disable channel %d reg_stat %d wiphy_stat 0x%x",
			  cache_chann->channel_info[i].channel_num,
			  cache_chann->channel_info[i].reg_status,
			  wiphy_channel->flags);

		cds_set_channel_state(rf_channel, CHANNEL_STATE_DISABLE);
		wiphy_channel->flags |= IEEE80211_CHAN_DISABLED;
	}

	qdf_mutex_release(&hdd_ctx->cache_channel_lock);
	status = sme_update_channel_list(hdd_ctx->hHal);

	EXIT();
	return status;
}

/**
 * wlan_hdd_cfg80211_start_bss() - start bss
 * @pHostapdAdapter: Pointer to hostapd adapter
 * @params: Pointer to start bss beacon parameters
 * @ssid: Pointer ssid
 * @ssid_len: Length of ssid
 * @hidden_ssid: Hidden SSID parameter
 * @check_for_concurrency: Flag to indicate if check for concurrency is needed
 *
 * Return: 0 for success non-zero for failure
 */
int wlan_hdd_cfg80211_start_bss(hdd_adapter_t *pHostapdAdapter,
				       struct cfg80211_beacon_data *params,
				       const u8 *ssid, size_t ssid_len,
				       enum nl80211_hidden_ssid hidden_ssid,
				       bool check_for_concurrency)
{
	tsap_Config_t *pConfig;
	beacon_data_t *pBeacon = NULL;
	struct ieee80211_mgmt *pMgmt_frame;
	uint8_t *pIe = NULL;
	uint16_t capab_info;
	eCsrAuthType RSNAuthType;
	eCsrEncryptionType RSNEncryptType;
	eCsrEncryptionType mcRSNEncryptType;
	int status = QDF_STATUS_SUCCESS, ret;
	int qdf_status = QDF_STATUS_SUCCESS;
	tpWLAN_SAPEventCB pSapEventCallback;
	hdd_hostapd_state_t *pHostapdState;
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
	int32_t i;
	struct hdd_config *iniConfig;
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);
	tSmeConfigParams *sme_config;
	bool MFPCapable = false;
	bool MFPRequired = false;
	uint16_t prev_rsn_length = 0;
	enum dfs_mode mode;
	bool disable_fw_tdls_state = false;
	uint8_t ignore_cac = 0;
	hdd_adapter_t *sta_adapter;

	ENTER();

	disable_fw_tdls_state = true;
	wlan_hdd_check_conc_and_update_tdls_state(pHddCtx,
						  disable_fw_tdls_state);

	if (cds_is_hw_mode_change_in_progress()) {
		status = qdf_wait_for_connection_update();
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("qdf wait for event failed!!");
			ret = -EINVAL;
			goto ret_status;
		}
	}

	/*
	 * For STA+SAP concurrency support from GUI, first STA connection gets
	 * triggered and while it is in progress, SAP start also comes up.
	 * Once STA association is successful, STA connect event is sent to
	 * kernel which gets queued in kernel workqueue and supplicant won't
	 * process M1 received from AP and send M2 until this NL80211_CONNECT
	 * event is received. Workqueue is not scheduled as RTNL lock is already
	 * taken by hostapd thread which has issued start_bss command to driver.
	 * Driver cannot complete start_bss as the pending command at the head
	 * of the SME command pending list is hw_mode_update for STA session
	 * which cannot be processed as SME is in WAITforKey state for STA
	 * interface. The start_bss command for SAP interface is queued behind
	 * the hw_mode_update command and so it cannot be processed until
	 * hw_mode_update command is processed. This is causing a deadlock so
	 * disconnect the STA interface first if connection or key exchange is
	 * in progress and then start SAP interface.
	 */
	sta_adapter = hdd_get_sta_connection_in_progress(pHddCtx);
	if (sta_adapter) {
		hdd_debug("Disconnecting STA with session id: %d",
			  sta_adapter->sessionId);
		wlan_hdd_disconnect(sta_adapter, eCSR_DISCONNECT_REASON_DEAUTH);
	}

	/*
	 * Reject start bss if reassoc in progress on any adapter.
	 * sme_is_any_session_in_middle_of_roaming is for LFR2 and
	 * hdd_is_roaming_in_progress is for LFR3
	 */
	if (sme_is_any_session_in_middle_of_roaming(hHal) ||
	    hdd_is_roaming_in_progress(pHddCtx)) {
		hdd_info("Reassociation in progress");
		ret = -EINVAL;
		goto ret_status;
	}

	/*
	 * Disable Roaming on all adapters before starting bss
	 */
	wlan_hdd_disable_roaming(pHostapdAdapter);

	sme_config = qdf_mem_malloc(sizeof(tSmeConfigParams));
	if (!sme_config) {
		hdd_err("failed to allocate memory");
		ret = -EINVAL;
		goto enable_roaming;
	}

	iniConfig = pHddCtx->config;
	pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);

	clear_bit(ACS_PENDING, &pHostapdAdapter->event_flags);
	clear_bit(ACS_IN_PROGRESS, &pHddCtx->g_event_flags);

	/* Mark the indoor channel (passive) to disable */
	if (iniConfig->disable_indoor_channel &&
			pHostapdAdapter->device_mode == QDF_SAP_MODE) {
		hdd_update_indoor_channel(pHddCtx, true);
		if (QDF_IS_STATUS_ERROR(
		    sme_update_channel_list(pHddCtx->hHal))) {
			hdd_update_indoor_channel(pHddCtx, false);
			hdd_err("Can't start BSS: update channel list failed");
			qdf_mem_free(sme_config);
			return -EINVAL;
		}

		/* check if STA is on indoor channel*/
		if (cds_is_force_scc())
			hdd_check_and_disconnect_sta_on_invalid_channel(
								       pHddCtx);
	}

	if (pHostapdAdapter->device_mode == QDF_SAP_MODE &&
	    !iniConfig->disable_channel) {
		/*
		 * Disable the channels received in command
		 * SET_DISABLE_CHANNEL_LIST
		 */
		status = wlan_hdd_disable_channels(pHddCtx);
		if (!QDF_IS_STATUS_SUCCESS(status))
			hdd_err("Disable channel list fail");
		hdd_check_and_disconnect_sta_on_invalid_channel(pHddCtx);
	}

	pConfig = &pHostapdAdapter->sessionCtx.ap.sapConfig;

	pBeacon = pHostapdAdapter->sessionCtx.ap.beacon;

	pMgmt_frame = (struct ieee80211_mgmt *)pBeacon->head;

	pConfig->beacon_int = pMgmt_frame->u.beacon.beacon_int;

	pConfig->auto_channel_select_weight =
			     iniConfig->auto_channel_select_weight;
	pConfig->disableDFSChSwitch = iniConfig->disableDFSChSwitch;
	pConfig->sap_chanswitch_beacon_cnt =
			    iniConfig->sap_chanswitch_beacon_cnt;
	pConfig->sap_chanswitch_mode = iniConfig->sap_chanswitch_mode;

	pConfig->reduced_beacon_interval =
			iniConfig->reduced_beacon_interval;

	pConfig->dfs_beacon_tx_enhanced = iniConfig->dfs_beacon_tx_enhanced;

	/* channel is already set in the set_channel Call back */
	/* pConfig->channel = pCommitConfig->channel; */

	/* Protection parameter to enable or disable */
	pConfig->protEnabled = iniConfig->apProtEnabled;

	pConfig->chan_switch_hostapd_rate_enabled =
			iniConfig->chan_switch_hostapd_rate_enabled;

	if (iniConfig->WlanMccToSccSwitchMode !=
			QDF_MCC_TO_SCC_SWITCH_DISABLE) {
		pConfig->chan_switch_hostapd_rate_enabled = false;
	}

	pConfig->enOverLapCh = iniConfig->gEnableOverLapCh;
	pConfig->dtim_period = pBeacon->dtim_period;
	hdd_debug("acs_mode %d", pConfig->acs_cfg.acs_mode);

	if (pConfig->acs_cfg.acs_mode == true) {
		hdd_debug("acs_channel %d, acs_dfs_mode %d",
			pHddCtx->acs_policy.acs_channel,
			pHddCtx->acs_policy.acs_dfs_mode);

		if (pHddCtx->acs_policy.acs_channel)
			pConfig->channel = pHddCtx->acs_policy.acs_channel;
		mode = pHddCtx->acs_policy.acs_dfs_mode;
		pConfig->acs_dfs_mode = wlan_hdd_get_dfs_mode(mode);
	}

	pConfig->user_config_channel = pConfig->channel;

	hdd_debug("pConfig->channel %d, pConfig->acs_dfs_mode %d",
		pConfig->channel, pConfig->acs_dfs_mode);

	hdd_debug("****pConfig->dtim_period=%d***",
		pConfig->dtim_period);

	if (pHostapdAdapter->device_mode == QDF_SAP_MODE) {
		pIe =
			wlan_hdd_cfg80211_get_ie_ptr(pBeacon->tail,
						     pBeacon->tail_len,
						     WLAN_EID_COUNTRY);
		if (pIe) {
			if (pIe[1] < IEEE80211_COUNTRY_IE_MIN_LEN) {
				hdd_err("Invalid Country IE len: %d", pIe[1]);
				ret = -EINVAL;
				goto error;
			}

			pConfig->ieee80211d = 1;
			qdf_mem_copy(pConfig->countryCode, &pIe[2], 3);
			sme_set_reg_info(hHal, pConfig->countryCode);
			sme_apply_channel_power_info_to_fw(hHal);
		} else {
			pConfig->countryCode[0] = pHddCtx->reg.alpha2[0];
			pConfig->countryCode[1] = pHddCtx->reg.alpha2[1];
			pConfig->ieee80211d = 0;
		}

		ret = wlan_hdd_sap_cfg_dfs_override(pHostapdAdapter);
		if (ret < 0) {
			goto error;
		} else {
			if (ret == 0) {
				if (CDS_IS_DFS_CH(pConfig->channel))
					pHddCtx->dev_dfs_cac_status =
							DFS_CAC_NEVER_DONE;
			}
		}

		/*
		 * If auto channel is configured i.e. channel is 0,
		 * so skip channel validation.
		 */
		if (AUTO_CHANNEL_SELECT != pConfig->channel) {
			if (QDF_STATUS_SUCCESS !=
			    wlan_hdd_validate_operation_channel(pHostapdAdapter,
							pConfig->channel)) {
				hdd_err("Invalid Channel: %d", pConfig->channel);
				ret = -EINVAL;
				goto error;
			}

			/* reject SAP if DFS channel scan is not allowed */
			if (!(pHddCtx->config->enableDFSChnlScan) &&
				(CHANNEL_STATE_DFS == cds_get_channel_state(
					pConfig->channel))) {
				hdd_err("No SAP start on DFS channel");
				ret = -EOPNOTSUPP;
				goto error;
			}
		}

		if (iniConfig->ignoreCAC ||
				((iniConfig->WlanMccToSccSwitchMode !=
				  QDF_MCC_TO_SCC_SWITCH_DISABLE) &&
				 iniConfig->sta_sap_scc_on_dfs_chan))
			ignore_cac = 1;

		wlansap_set_dfs_ignore_cac(hHal, ignore_cac);

		wlansap_set_dfs_restrict_japan_w53(hHal,
			iniConfig->gDisableDfsJapanW53);
		wlansap_set_dfs_preferred_channel_location(hHal,
			iniConfig->gSapPreferredChanLocation);
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
		wlan_sap_set_channel_avoidance(hHal,
					iniConfig->sap_channel_avoidance);
#endif
	} else if (pHostapdAdapter->device_mode == QDF_P2P_GO_MODE) {
		pConfig->countryCode[0] = pHddCtx->reg.alpha2[0];
		pConfig->countryCode[1] = pHddCtx->reg.alpha2[1];
		pConfig->ieee80211d = 0;
	} else {
		pConfig->ieee80211d = 0;
	}

	wlansap_set_tx_leakage_threshold(hHal,
		iniConfig->sap_tx_leakage_threshold);

	capab_info = pMgmt_frame->u.beacon.capab_info;

	pConfig->privacy = (pMgmt_frame->u.beacon.capab_info &
			    WLAN_CAPABILITY_PRIVACY) ? true : false;

	(WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uPrivacy = pConfig->privacy;

	/*Set wps station to configured */
	pIe = wlan_hdd_get_wps_ie_ptr(pBeacon->tail, pBeacon->tail_len);

	if (pIe) {
		/* To acess pIe[15], length needs to be atlest 14 */
		if (pIe[1] < 14) {
			hdd_err("**Wps Ie Length(%hhu) is too small***",
				pIe[1]);
			ret = -EINVAL;
			goto error;
		} else if (memcmp(&pIe[2], WPS_OUI_TYPE, WPS_OUI_TYPE_SIZE) ==
			   0) {
			hdd_debug("** WPS IE(len %d) ***", (pIe[1] + 2));
			/* Check 15 bit of WPS IE as it contain information for
			 * wps state
			 */
			if (SAP_WPS_ENABLED_UNCONFIGURED == pIe[15]) {
				pConfig->wps_state =
					SAP_WPS_ENABLED_UNCONFIGURED;
			} else if (SAP_WPS_ENABLED_CONFIGURED == pIe[15]) {
				pConfig->wps_state = SAP_WPS_ENABLED_CONFIGURED;
			}
		}
	} else {
		hdd_debug("WPS disabled");
		pConfig->wps_state = SAP_WPS_DISABLED;
	}
	/* Forward WPS PBC probe request frame up */
	pConfig->fwdWPSPBCProbeReq = 1;

	pConfig->RSNEncryptType = eCSR_ENCRYPT_TYPE_NONE;
	pConfig->mcRSNEncryptType = eCSR_ENCRYPT_TYPE_NONE;
	(WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->ucEncryptType =
		eCSR_ENCRYPT_TYPE_NONE;

	pConfig->RSNWPAReqIELength = 0;
	memset(&pConfig->RSNWPAReqIE[0], 0, sizeof(pConfig->RSNWPAReqIE));
	pIe = wlan_hdd_cfg80211_get_ie_ptr(pBeacon->tail, pBeacon->tail_len,
					   WLAN_EID_RSN);
	if (pIe && pIe[1]) {
		pConfig->RSNWPAReqIELength = pIe[1] + 2;
		if (pConfig->RSNWPAReqIELength < sizeof(pConfig->RSNWPAReqIE))
			memcpy(&pConfig->RSNWPAReqIE[0], pIe,
			       pConfig->RSNWPAReqIELength);
		else
			hdd_err("RSNWPA IE MAX Length exceeded; length =%d",
			       pConfig->RSNWPAReqIELength);
		/* The actual processing may eventually be more extensive than
		 * this. Right now, just consume any PMKIDs that are  sent in
		 * by the app.
		 */
		status =
			hdd_softap_unpack_ie(cds_get_context
						     (QDF_MODULE_ID_SME),
					     &RSNEncryptType, &mcRSNEncryptType,
					     &RSNAuthType, &MFPCapable,
					     &MFPRequired,
					     pConfig->RSNWPAReqIE[1] + 2,
					     pConfig->RSNWPAReqIE);

		if (QDF_STATUS_SUCCESS == status) {
			/* Now copy over all the security attributes you have
			 * parsed out. Use the cipher type in the RSN IE
			 */
			pConfig->RSNEncryptType = RSNEncryptType;
			pConfig->mcRSNEncryptType = mcRSNEncryptType;
			(WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->
			ucEncryptType = RSNEncryptType;
			hdd_debug("CSR AuthType = %d, EncryptionType = %d mcEncryptionType = %d",
			       RSNAuthType, RSNEncryptType, mcRSNEncryptType);
		}
	}

	pIe = wlan_hdd_get_vendor_oui_ie_ptr(WPA_OUI_TYPE, WPA_OUI_TYPE_SIZE,
					     pBeacon->tail, pBeacon->tail_len);

	if (pIe && pIe[1] && (pIe[0] == DOT11F_EID_WPA)) {
		if (pConfig->RSNWPAReqIE[0]) {
			/*Mixed mode WPA/WPA2 */
			prev_rsn_length = pConfig->RSNWPAReqIELength;
			pConfig->RSNWPAReqIELength += pIe[1] + 2;
			if (pConfig->RSNWPAReqIELength <
			    sizeof(pConfig->RSNWPAReqIE))
				memcpy(&pConfig->RSNWPAReqIE[0] +
				       prev_rsn_length, pIe, pIe[1] + 2);
			else
				hdd_err("RSNWPA IE MAX Length exceeded; length: %d",
				       pConfig->RSNWPAReqIELength);
		} else {
			pConfig->RSNWPAReqIELength = pIe[1] + 2;
			if (pConfig->RSNWPAReqIELength <
			    sizeof(pConfig->RSNWPAReqIE))
				memcpy(&pConfig->RSNWPAReqIE[0], pIe,
				       pConfig->RSNWPAReqIELength);
			else
				hdd_err("RSNWPA IE MAX Length exceeded; length: %d",
				       pConfig->RSNWPAReqIELength);
			status = hdd_softap_unpack_ie
					(cds_get_context(QDF_MODULE_ID_SME),
					 &RSNEncryptType,
					 &mcRSNEncryptType, &RSNAuthType,
					 &MFPCapable, &MFPRequired,
					 pConfig->RSNWPAReqIE[1] + 2,
					 pConfig->RSNWPAReqIE);

			if (QDF_STATUS_SUCCESS == status) {
				/* Now copy over all the security attributes
				 * you have parsed out. Use the cipher type
				 * in the RSN IE
				 */
				pConfig->RSNEncryptType = RSNEncryptType;
				pConfig->mcRSNEncryptType = mcRSNEncryptType;
				(WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->
				ucEncryptType = RSNEncryptType;
				hdd_debug("CSR AuthType = %d, EncryptionType = %d mcEncryptionType = %d",
				       RSNAuthType, RSNEncryptType,
				       mcRSNEncryptType);
			}
		}
	}

	if (pConfig->RSNWPAReqIELength > sizeof(pConfig->RSNWPAReqIE)) {
		hdd_err("**RSNWPAReqIELength is too large***");
		ret = -EINVAL;
		goto error;
	}

	pConfig->SSIDinfo.ssidHidden = false;

	if (ssid != NULL) {
		qdf_mem_copy(pConfig->SSIDinfo.ssid.ssId, ssid, ssid_len);
		pConfig->SSIDinfo.ssid.length = ssid_len;

		switch (hidden_ssid) {
		case NL80211_HIDDEN_SSID_NOT_IN_USE:
			hdd_debug("HIDDEN_SSID_NOT_IN_USE");
			pConfig->SSIDinfo.ssidHidden = eHIDDEN_SSID_NOT_IN_USE;
			break;
		case NL80211_HIDDEN_SSID_ZERO_LEN:
			hdd_debug("HIDDEN_SSID_ZERO_LEN");
			pConfig->SSIDinfo.ssidHidden = eHIDDEN_SSID_ZERO_LEN;
			break;
		case NL80211_HIDDEN_SSID_ZERO_CONTENTS:
			hdd_debug("HIDDEN_SSID_ZERO_CONTENTS");
			pConfig->SSIDinfo.ssidHidden =
				eHIDDEN_SSID_ZERO_CONTENTS;
			break;
		default:
			hdd_err("Wrong hidden_ssid param: %d", hidden_ssid);
			break;
		}
	}

	qdf_mem_copy(pConfig->self_macaddr.bytes,
		     pHostapdAdapter->macAddressCurrent.bytes,
		     QDF_MAC_ADDR_SIZE);

	/* default value */
	pConfig->SapMacaddr_acl = eSAP_ACCEPT_UNLESS_DENIED;
	pConfig->num_accept_mac = 0;
	pConfig->num_deny_mac = 0;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	/*
	 * We don't want P2PGO to follow STA's channel
	 * so lets limit the logic for SAP only.
	 * Later if we decide to make p2pgo follow STA's
	 * channel then remove this check.
	 */
	if ((0 == pHddCtx->config->conc_custom_rule1) ||
		(pHddCtx->config->conc_custom_rule1 &&
		QDF_SAP_MODE == pHostapdAdapter->device_mode))
		pConfig->cc_switch_mode = iniConfig->WlanMccToSccSwitchMode;
#endif

	if (!(ssid && qdf_str_len(PRE_CAC_SSID) == ssid_len &&
	      (0 == qdf_mem_cmp(ssid, PRE_CAC_SSID, ssid_len)))) {
		pIe = wlan_hdd_cfg80211_get_ie_ptr(
				&pMgmt_frame->u.beacon.variable[0],
				pBeacon->head_len, WLAN_EID_SUPP_RATES);

		if (pIe != NULL) {
			pIe++;
			if (pIe[0] > SIR_MAC_RATESET_EID_MAX) {
				hdd_err("Invalid supported rates %d",
					pIe[0]);
				ret = -EINVAL;
				goto error;
			}
			pConfig->supported_rates.numRates = pIe[0];
			pIe++;
			for (i = 0;
			     i < pConfig->supported_rates.numRates; i++) {
				if (pIe[i]) {
				     pConfig->supported_rates.rate[i] = pIe[i];
				     hdd_debug("Configured Supported rate is %2x",
					pConfig->supported_rates.rate[i]);
				}
			}
		}
		pIe = wlan_hdd_cfg80211_get_ie_ptr(pBeacon->tail,
				pBeacon->tail_len,
				WLAN_EID_EXT_SUPP_RATES);
		if (pIe != NULL) {
			pIe++;
			if (pIe[0] > SIR_MAC_RATESET_EID_MAX) {
				hdd_err("Invalid supported rates %d",
					pIe[0]);
				ret = -EINVAL;
				goto error;
			}
			pConfig->extended_rates.numRates = pIe[0];
			pIe++;
			for (i = 0; i < pConfig->extended_rates.numRates; i++) {
				if (pIe[i]) {
				      pConfig->extended_rates.rate[i] = pIe[i];
				      hdd_debug("Configured ext Supported rate is %2x",
					pConfig->extended_rates.rate[i]);
				}
			}
		}
	}

	if (!cds_is_sub_20_mhz_enabled())
		wlan_hdd_set_sap_hwmode(pHostapdAdapter);

	if (IS_24G_CH(pConfig->channel) &&
	    pHddCtx->config->enableVhtFor24GHzBand &&
	    (pConfig->SapHw_mode == eCSR_DOT11_MODE_11n ||
	    pConfig->SapHw_mode == eCSR_DOT11_MODE_11n_ONLY))
		pConfig->SapHw_mode = eCSR_DOT11_MODE_11ac;

	if (((pHostapdAdapter->device_mode == QDF_SAP_MODE) &&
	     (pHddCtx->config->sap_force_11n_for_11ac)) ||
	     ((pHostapdAdapter->device_mode == QDF_P2P_GO_MODE) &&
	     (pHddCtx->config->go_force_11n_for_11ac))) {
		if (pConfig->SapHw_mode == eCSR_DOT11_MODE_11ac ||
		    pConfig->SapHw_mode == eCSR_DOT11_MODE_11ac_ONLY)
			pConfig->SapHw_mode = eCSR_DOT11_MODE_11n;
	}

	sme_get_config_param(pHddCtx->hHal, sme_config);
	/* Override hostapd.conf wmm_enabled only for 11n and 11AC configs (IOT)
	 * As per spec 11N/AC STA are QOS STA and may not connect or throughput
	 * may not be good with non QOS 11N AP
	 * Default: enable QOS for SAP unless WMM IE not present for 11bga
	 */
	sme_config->csrConfig.WMMSupportMode = eCsrRoamWmmAuto;
	pIe = wlan_hdd_get_vendor_oui_ie_ptr(WMM_OUI_TYPE, WMM_OUI_TYPE_SIZE,
					pBeacon->tail, pBeacon->tail_len);
	if (!pIe && (pConfig->SapHw_mode == eCSR_DOT11_MODE_11a ||
		pConfig->SapHw_mode == eCSR_DOT11_MODE_11g ||
		pConfig->SapHw_mode == eCSR_DOT11_MODE_11b))
		sme_config->csrConfig.WMMSupportMode = eCsrRoamWmmNoQos;
	sme_update_config(pHddCtx->hHal, sme_config);

	if (!((pHostapdAdapter->device_mode == QDF_SAP_MODE) &&
	     (pHddCtx->config->sap_force_11n_for_11ac)) ||
	     ((pHostapdAdapter->device_mode == QDF_P2P_GO_MODE) &&
	     (pHddCtx->config->go_force_11n_for_11ac))) {
		pConfig->ch_width_orig =
			hdd_map_nl_chan_width(pConfig->ch_width_orig);
	} else {
		if (pConfig->ch_width_orig >= NL80211_CHAN_WIDTH_40)
			pConfig->ch_width_orig = CH_WIDTH_40MHZ;
		else
			pConfig->ch_width_orig = CH_WIDTH_20MHZ;
	}

	if (wlan_hdd_setup_driver_overrides(pHostapdAdapter)) {
		ret = -EINVAL;
		goto error;
	}

	/* ht_capab is not what the name conveys,
	 * this is used for protection bitmap
	 */
	pConfig->ht_capab = iniConfig->apProtection;

	if (0 != wlan_hdd_cfg80211_update_apies(pHostapdAdapter)) {
		hdd_err("SAP Not able to set AP IEs");
		wlansap_reset_sap_config_add_ie(pConfig, eUPDATE_IE_ALL);
		ret = -EINVAL;
		goto error;
	}
	/* Uapsd Enabled Bit */
	pConfig->UapsdEnable = iniConfig->apUapsdEnabled;
	/* Enable OBSS protection */
	pConfig->obssProtEnabled = iniConfig->apOBSSProtEnabled;

	if (pHostapdAdapter->device_mode == QDF_SAP_MODE)
		pConfig->sap_dot11mc =
		    (WLAN_HDD_GET_CTX(pHostapdAdapter))->config->sap_dot11mc;
	else /* for P2P-Go case */
		pConfig->sap_dot11mc = 1;

	hdd_debug("11MC Support Enabled : %d\n",
		pConfig->sap_dot11mc);

#ifdef WLAN_FEATURE_11W
	pConfig->mfpCapable = MFPCapable;
	pConfig->mfpRequired = MFPRequired;
	hdd_debug("Soft AP MFP capable %d, MFP required %d",
	       pConfig->mfpCapable, pConfig->mfpRequired);
#endif

	hdd_debug("SOftAP macaddress : " MAC_ADDRESS_STR,
	       MAC_ADDR_ARRAY(pHostapdAdapter->macAddressCurrent.bytes));
	hdd_debug("ssid =%s, beaconint=%d, channel=%d",
	       pConfig->SSIDinfo.ssid.ssId, (int)pConfig->beacon_int,
	       (int)pConfig->channel);
	hdd_debug("hw_mode=%x, privacy=%d, authType=%d",
	       pConfig->SapHw_mode, pConfig->privacy, pConfig->authType);
	hdd_debug("RSN/WPALen=%d, Uapsd = %d",
	       (int)pConfig->RSNWPAReqIELength, pConfig->UapsdEnable);
	hdd_debug("ProtEnabled = %d, OBSSProtEnabled = %d",
	       pConfig->protEnabled, pConfig->obssProtEnabled);
	hdd_debug("ChanSwitchHostapdRateEnabled = %d",
			pConfig->chan_switch_hostapd_rate_enabled);

	if (test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags)) {
		wlansap_reset_sap_config_add_ie(pConfig, eUPDATE_IE_ALL);
		/* Bss already started. just return. */
		/* TODO Probably it should update some beacon params. */
		if (sme_config)
			qdf_mem_free(sme_config);
		hdd_debug("Bss Already started...Ignore the request");
		EXIT();
		return 0;
	}

	if (check_for_concurrency) {
		if (!cds_allow_concurrency(
					cds_convert_device_mode_to_qdf_type(
					pHostapdAdapter->device_mode),
					pConfig->channel, HW_MODE_20_MHZ)) {
			hdd_err("This concurrency combination is not allowed");
			ret = -EINVAL;
			goto error;
		}
	}

	if (!cds_set_connection_in_progress(true)) {
		hdd_err("Can't start BSS: set connnection in progress failed");
		ret = -EINVAL;
		goto error;
	}

	pConfig->persona = pHostapdAdapter->device_mode;

	pSapEventCallback = hdd_hostapd_sap_event_cb;

	(WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->dfs_cac_block_tx = true;
	set_bit(SOFTAP_INIT_DONE, &pHostapdAdapter->event_flags);

	qdf_event_reset(&pHostapdState->qdf_event);
	status = wlansap_start_bss(
		WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter),
		pSapEventCallback, pConfig, pHostapdAdapter->dev);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		wlansap_reset_sap_config_add_ie(pConfig, eUPDATE_IE_ALL);
		cds_set_connection_in_progress(false);
		hdd_err("SAP Start Bss fail");
		ret = -EINVAL;
		goto error;
	}

	hdd_debug("Waiting for Scan to complete(auto mode) and BSS to start");

	qdf_status = qdf_wait_for_event_completion(&pHostapdState->qdf_event,
						SME_CMD_TIMEOUT_VALUE);

	wlansap_reset_sap_config_add_ie(pConfig, eUPDATE_IE_ALL);

	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_err("qdf wait for single_event failed!!");
		cds_set_connection_in_progress(false);
		sme_get_command_q_status(hHal);
		wlansap_stop_bss(WLAN_HDD_GET_SAP_CTX_PTR(pHostapdAdapter));
		QDF_ASSERT(0);
		ret = -EINVAL;
		goto error;
	}
	/* Succesfully started Bss update the state bit. */
	set_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags);
	/* Initialize WMM configuation */
	hdd_wmm_init(pHostapdAdapter);
	if (pHostapdState->bssState == BSS_START)
		cds_incr_active_session(pHostapdAdapter->device_mode,
					pHostapdAdapter->sessionId);
#ifdef DHCP_SERVER_OFFLOAD
	if (iniConfig->enableDHCPServerOffload)
		wlan_hdd_set_dhcp_server_offload(pHostapdAdapter);
#endif /* DHCP_SERVER_OFFLOAD */

#ifdef WLAN_FEATURE_P2P_DEBUG
	if (pHostapdAdapter->device_mode == QDF_P2P_GO_MODE) {
		if (global_p2p_connection_status == P2P_GO_NEG_COMPLETED) {
			global_p2p_connection_status = P2P_GO_COMPLETED_STATE;
			hdd_debug("[P2P State] From Go nego completed to Non-autonomous Group started");
		} else if (global_p2p_connection_status == P2P_NOT_ACTIVE) {
			global_p2p_connection_status = P2P_GO_COMPLETED_STATE;
			hdd_debug("[P2P State] From Inactive to Autonomous Group started");
		}
	}
#endif
	/* Check and restart SAP if it is on unsafe channel */
	hdd_unsafe_channel_restart_sap(pHddCtx);

	cds_set_connection_in_progress(false);
	pHostapdState->bCommit = true;
	if (sme_config)
		qdf_mem_free(sme_config);

	/* Enable Roaming after start bss */
	wlan_hdd_enable_roaming(pHostapdAdapter);

	EXIT();

	return 0;

error:
	if (pHostapdAdapter->device_mode == QDF_SAP_MODE &&
	    !iniConfig->disable_channel)
		wlan_hdd_restore_channels(pHddCtx);
	/* Revert the indoor to passive marking if START BSS fails */
	if (iniConfig->disable_indoor_channel &&
			pHostapdAdapter->device_mode == QDF_SAP_MODE) {
		hdd_update_indoor_channel(pHddCtx, false);
		sme_update_channel_list(pHddCtx->hHal);
	}
	if (sme_config)
		qdf_mem_free(sme_config);
	clear_bit(SOFTAP_INIT_DONE, &pHostapdAdapter->event_flags);
	qdf_atomic_set(
		&pHostapdAdapter->sessionCtx.ap.acs_in_progress, 0);
	wlan_hdd_undo_acs(pHostapdAdapter);

enable_roaming:
	/* Enable Roaming after start bss in case of failure */
	wlan_hdd_enable_roaming(pHostapdAdapter);

ret_status:
	if (disable_fw_tdls_state)
		wlan_hdd_check_conc_and_update_tdls_state(pHddCtx, false);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_stop_ap() - stop soft ap
 * @wiphy: Pointer to wiphy structure
 * @dev: Pointer to net_device structure
 *
 * Return: 0 for success non-zero for failure
 */
static int __wlan_hdd_cfg80211_stop_ap(struct wiphy *wiphy,
					struct net_device *dev)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx = wiphy_priv(wiphy);
	hdd_scaninfo_t *pScanInfo = NULL;
	hdd_adapter_t *staAdapter = NULL;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	tSirUpdateIE updateIE;
	beacon_data_t *old;
	int ret;
	unsigned long rc;
	hdd_adapter_list_node_t *pAdapterNode = NULL;
	hdd_adapter_list_node_t *pNext = NULL;
	tsap_Config_t *pConfig;

	hdd_info("enter(%s)", netdev_name(dev));

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	if (pHddCtx->driver_status == DRIVER_MODULES_CLOSED) {
		hdd_err("Driver module is closed; dropping request");
		return -EINVAL;
	}

	if (wlan_hdd_validate_session_id(pAdapter->sessionId)) {
		hdd_err("Invalid session id: %d", pAdapter->sessionId);
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_STOP_AP,
			 pAdapter->sessionId, pAdapter->device_mode));

	if (!(pAdapter->device_mode == QDF_SAP_MODE ||
	      pAdapter->device_mode == QDF_P2P_GO_MODE)) {
		return -EOPNOTSUPP;
	}
	/* Clear SOFTAP_INIT_DONE flag to mark stop_ap deinit. So that we do
	 * not restart SAP after SSR as SAP is already stopped from user space.
	 * This update is moved to start of this function to resolve stop_ap
	 * call during SSR case. Adapter gets cleaned up as part of SSR.
	 */
	clear_bit(SOFTAP_INIT_DONE, &pAdapter->event_flags);
	hdd_debug("Device_mode %s(%d)",
		hdd_device_mode_to_string(pAdapter->device_mode),
		pAdapter->device_mode);

	ret = wlan_hdd_validate_context(pHddCtx);
	if (0 != ret)
		return ret;

	cds_flush_sta_ap_intf_work(pHddCtx);

	/*
	 * If a STA connection is in progress in another adapter, disconnect
	 * the STA and complete the SAP operation. STA will reconnect
	 * after SAP stop is done.
	 */
	staAdapter = hdd_get_sta_connection_in_progress(pHddCtx);
	if (staAdapter) {
		hdd_debug("Disconnecting STA with session id: %d",
			  staAdapter->sessionId);
		wlan_hdd_disconnect(staAdapter, eCSR_DISCONNECT_REASON_DEAUTH);
	}

	if (pAdapter->device_mode == QDF_SAP_MODE) {
		wlan_hdd_del_station(pAdapter);
		hdd_green_ap_stop_bss(pHddCtx);
	}

	status = hdd_get_front_adapter(pHddCtx, &pAdapterNode);
	while (NULL != pAdapterNode && QDF_STATUS_SUCCESS == status) {
		staAdapter = pAdapterNode->pAdapter;

		if (QDF_STA_MODE == staAdapter->device_mode ||
		    (QDF_P2P_CLIENT_MODE == staAdapter->device_mode) ||
		    (QDF_P2P_GO_MODE == staAdapter->device_mode)) {
			pScanInfo = &staAdapter->scan_info;

			if (pScanInfo && pScanInfo->mScanPending) {
				hdd_debug("Aborting pending scan for device mode:%d",
				       staAdapter->device_mode);
				INIT_COMPLETION(pScanInfo->abortscan_event_var);
				hdd_abort_mac_scan(staAdapter->pHddCtx,
						   staAdapter->sessionId,
						   INVALID_SCAN_ID,
						   eCSR_SCAN_ABORT_DEFAULT);
				rc = wait_for_completion_timeout(
					&pScanInfo->abortscan_event_var,
					msecs_to_jiffies(
						WLAN_WAIT_TIME_ABORTSCAN));
				if (!rc) {
					hdd_err("Timeout occurred while waiting for abortscan");
					QDF_ASSERT(pScanInfo->mScanPending);
				}
			}
		}

		status = hdd_get_next_adapter(pHddCtx, pAdapterNode, &pNext);
		pAdapterNode = pNext;
	}
	cds_flush_work(&pAdapter->sap_stop_bss_work);
	/*
	 * When ever stop ap adapter gets called, we need to check
	 * whether any restart AP work is pending. If any restart is pending
	 * then lets finish it and go ahead from there.
	 */
	if (pHddCtx->config->conc_custom_rule1 &&
	    (QDF_SAP_MODE == pAdapter->device_mode)) {
		cds_flush_work(&pHddCtx->sap_start_work);
		hdd_debug("Canceled the pending restart work");
		qdf_spin_lock(&pHddCtx->sap_update_info_lock);
		pHddCtx->is_sap_restart_required = false;
		qdf_spin_unlock(&pHddCtx->sap_update_info_lock);
	}

	pConfig = &pAdapter->sessionCtx.ap.sapConfig;
	pConfig->acs_cfg.acs_mode = false;
	qdf_atomic_set(
		&pAdapter->sessionCtx.ap.acs_in_progress, 0);
	wlan_hdd_undo_acs(pAdapter);
	qdf_mem_zero(&pConfig->acs_cfg, sizeof(struct sap_acs_cfg));

	/* Stop all tx queues */
	hdd_info("Disabling queues");
	wlan_hdd_netif_queue_control(pAdapter,
				     WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER,
				     WLAN_CONTROL_PATH);

	old = pAdapter->sessionCtx.ap.beacon;
	if (!old) {
		hdd_err("Session id: %d beacon data points to NULL",
		       pAdapter->sessionId);
		return -EINVAL;
	}

	hdd_cleanup_actionframe(pHddCtx, pAdapter);
	wlan_hdd_cleanup_remain_on_channel_ctx(pAdapter);

	mutex_lock(&pHddCtx->sap_lock);
	if (test_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags)) {
		hdd_hostapd_state_t *pHostapdState =
			WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);

		/* Set the stop_bss_in_progress flag */
		wlansap_set_stop_bss_inprogress(
			WLAN_HDD_GET_SAP_CTX_PTR(pAdapter), true);

		qdf_event_reset(&pHostapdState->qdf_stop_bss_event);
		status = wlansap_stop_bss(WLAN_HDD_GET_SAP_CTX_PTR(pAdapter));
		if (QDF_IS_STATUS_SUCCESS(status)) {
			qdf_status =
				qdf_wait_for_event_completion(&pHostapdState->
					qdf_stop_bss_event,
					SME_CMD_TIMEOUT_VALUE);

			if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
				hdd_err("qdf wait for single_event failed!!");

				if (hdd_ipa_uc_is_enabled(pHddCtx))
					hdd_ipa_clean_adapter_iface(pAdapter);

				QDF_ASSERT(0);
			}
		}
		clear_bit(SOFTAP_BSS_STARTED, &pAdapter->event_flags);

		/* Clear the stop_bss_in_progress flag */
		wlansap_set_stop_bss_inprogress(
			WLAN_HDD_GET_SAP_CTX_PTR(pAdapter), false);

		/*BSS stopped, clear the active sessions for this device mode*/
		cds_decr_session_set_pcl(pAdapter->device_mode,
						pAdapter->sessionId);
		pAdapter->sessionCtx.ap.beacon = NULL;
		qdf_mem_free(old);
	}
	mutex_unlock(&pHddCtx->sap_lock);

	if (wlan_sap_is_pre_cac_active(pHddCtx->hHal))
		hdd_clean_up_pre_cac_interface(pHddCtx);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("Stopping the BSS");
		return -EINVAL;
	}

	qdf_copy_macaddr(&updateIE.bssid, &pAdapter->macAddressCurrent);
	updateIE.smeSessionId = pAdapter->sessionId;
	updateIE.ieBufferlength = 0;
	updateIE.pAdditionIEBuffer = NULL;
	updateIE.append = true;
	updateIE.notify = true;
	if (sme_update_add_ie(WLAN_HDD_GET_HAL_CTX(pAdapter),
			      &updateIE,
			      eUPDATE_IE_PROBE_BCN) == QDF_STATUS_E_FAILURE) {
		hdd_err("Could not pass on PROBE_RSP_BCN data to PE");
	}

	if (sme_update_add_ie(WLAN_HDD_GET_HAL_CTX(pAdapter),
			      &updateIE,
			      eUPDATE_IE_ASSOC_RESP) == QDF_STATUS_E_FAILURE) {
		hdd_err("Could not pass on ASSOC_RSP data to PE");
	}
	/* Reset WNI_CFG_PROBE_RSP Flags */
	wlan_hdd_reset_prob_rspies(pAdapter);

#ifdef WLAN_FEATURE_P2P_DEBUG
	if ((pAdapter->device_mode == QDF_P2P_GO_MODE) &&
	    (global_p2p_connection_status == P2P_GO_COMPLETED_STATE)) {
		hdd_debug("[P2P State] From GO completed to Inactive state GO got removed");
		global_p2p_connection_status = P2P_NOT_ACTIVE;
	}
#endif
	pAdapter->sessionId = HDD_SESSION_ID_INVALID;
	wlan_hdd_check_conc_and_update_tdls_state(pHddCtx, false);
	EXIT();
	return ret;
}

/**
 * wlan_hdd_get_channel_bw() - get channel bandwidth
 * @width: input channel width in nl80211_chan_width value
 *
 * Return: channel width value defined by driver
 */
static enum hw_mode_bandwidth wlan_hdd_get_channel_bw(
					enum nl80211_chan_width width)
{
	enum hw_mode_bandwidth ch_bw = HW_MODE_20_MHZ;

	switch (width) {
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
		ch_bw = HW_MODE_20_MHZ;
		break;
	case NL80211_CHAN_WIDTH_40:
		ch_bw = HW_MODE_40_MHZ;
		break;
	case NL80211_CHAN_WIDTH_80:
		ch_bw = HW_MODE_80_MHZ;
		break;
	case NL80211_CHAN_WIDTH_80P80:
		ch_bw = HW_MODE_80_PLUS_80_MHZ;
		break;
	case NL80211_CHAN_WIDTH_160:
		ch_bw = HW_MODE_160_MHZ;
		break;
	default:
		hdd_err("Invalid width: %d, using default 20MHz", width);
		break;
	}

	return ch_bw;
}

/**
 * wlan_hdd_cfg80211_stop_ap() - stop sap
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to netdev
 *
 * Return: zero for success non-zero for failure
 */
int wlan_hdd_cfg80211_stop_ap(struct wiphy *wiphy,
				struct net_device *dev)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_stop_ap(wiphy, dev);
	cds_ssr_unprotect(__func__);

	return ret;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)) || \
	defined(CFG80211_BEACON_TX_RATE_CUSTOM_BACKPORT)
/**
 * hdd_get_data_rate_from_rate_mask() - convert mask to rate
 * @wiphy: Pointer to wiphy
 * @band: band
 * @bit_rate_mask: pointer to bit_rake_mask
 *
 * This function takes band and bit_rate_mask as input and
 * derives the beacon_tx_rate based on the supported rates
 * published as part of wiphy register.
 *
 * Return: data rate for success or zero for failure
 */
static uint16_t hdd_get_data_rate_from_rate_mask(struct wiphy *wiphy,
		enum nl80211_band band,
		struct cfg80211_bitrate_mask *bit_rate_mask)
{
	struct ieee80211_supported_band *sband = wiphy->bands[band];
	int sband_n_bitrates;
	struct ieee80211_rate *sband_bitrates;
	int i;

	if (sband) {
		sband_bitrates = sband->bitrates;
		sband_n_bitrates = sband->n_bitrates;
		for (i = 0; i < sband_n_bitrates; i++) {
			if (bit_rate_mask->control[band].legacy ==
			    sband_bitrates[i].hw_value)
				return sband_bitrates[i].bitrate;
		}
	}
	return 0;
}

/**
 * hdd_update_beacon_rate() - Update beacon tx rate
 * @pAdapter: Pointer to hdd_adapter_t
 * @wiphy: Pointer to wiphy
 * @params: Pointet to cfg80211_ap_settings
 *
 * This function updates the beacon tx rate which is provided
 * as part of cfg80211_ap_settions in to the sapConfig
 * structure
 *
 * Return: none
 */
static void hdd_update_beacon_rate(hdd_adapter_t *adapter,
		struct wiphy *wiphy,
		struct cfg80211_ap_settings *params)
{
	struct cfg80211_bitrate_mask *beacon_rate_mask;
	enum  nl80211_band band;

	band = params->chandef.chan->band;
	beacon_rate_mask = &params->beacon_rate;
	if (beacon_rate_mask->control[band].legacy) {
		adapter->sessionCtx.ap.sapConfig.beacon_tx_rate =
			hdd_get_data_rate_from_rate_mask(wiphy, band,
					beacon_rate_mask);
		hdd_debug("beacon mask value %u, rate %hu",
			  params->beacon_rate.control[0].legacy,
			  adapter->sessionCtx.ap.sapConfig.beacon_tx_rate);
	}
}
#else
static void hdd_update_beacon_rate(hdd_adapter_t *adapter,
		struct wiphy *wiphy,
		struct cfg80211_ap_settings *params)
{
}
#endif


/**
 * __wlan_hdd_cfg80211_start_ap() - start soft ap mode
 * @wiphy: Pointer to wiphy structure
 * @dev: Pointer to net_device structure
 * @params: Pointer to AP settings parameters
 *
 * Return: 0 for success non-zero for failure
 */
static int __wlan_hdd_cfg80211_start_ap(struct wiphy *wiphy,
					struct net_device *dev,
					struct cfg80211_ap_settings *params)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx;
	enum hw_mode_bandwidth channel_width;
	int status;
	struct sme_sta_inactivity_timeout  *sta_inactivity_timer;
	uint8_t channel;
	bool sta_sap_scc_on_dfs_chan;
	uint16_t sta_cnt;

	hdd_info("enter(%s)", netdev_name(dev));

	clear_bit(SOFTAP_INIT_DONE, &pAdapter->event_flags);
	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	if (wlan_hdd_validate_session_id(pAdapter->sessionId)) {
		hdd_err("Invalid session id: %d", pAdapter->sessionId);
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_START_AP, pAdapter->sessionId,
			 params->beacon_interval));
	if (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic) {
		hdd_err("HDD adapter magic is invalid");
		return -ENODEV;
	}

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	hdd_debug("pAdapter = %pK, Device mode %s(%d) sub20 %d",
		pAdapter, hdd_device_mode_to_string(pAdapter->device_mode),
		pAdapter->device_mode, cds_is_sub_20_mhz_enabled());

	if (cds_is_hw_mode_change_in_progress()) {
		status = qdf_wait_for_connection_update();
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("qdf wait for event failed!!");
			return -EINVAL;
		}
	}

	channel_width = wlan_hdd_get_channel_bw(params->chandef.width);
	channel = ieee80211_frequency_to_channel(
				params->chandef.chan->center_freq);
	pAdapter->sessionCtx.ap.sapConfig.ch_params.center_freq_seg0 =
				cds_freq_to_chan(params->chandef.center_freq1);
	pAdapter->sessionCtx.ap.sapConfig.ch_params.center_freq_seg1 =
				cds_freq_to_chan(params->chandef.center_freq2);

	sta_sap_scc_on_dfs_chan = cds_is_sta_sap_scc_allowed_on_dfs_channel();
	sta_cnt = cds_mode_specific_connection_count(CDS_STA_MODE, NULL);

	hdd_debug("sta_sap_scc_on_dfs_chan %u, sta_cnt %u",
			sta_sap_scc_on_dfs_chan, sta_cnt);

	/* if sta_sap_scc_on_dfs_chan ini is set, DFS master capability is
	 * assumed disabled in the driver.
	 */
	if (channel && (cds_get_channel_state(channel) == CHANNEL_STATE_DFS) &&
			sta_sap_scc_on_dfs_chan && !sta_cnt) {
		hdd_err("SAP not allowed on DFS channel!!");
		return -EINVAL;
	}
	if (cds_is_etsi13_regdmn_srd_chan(cds_chan_to_freq(channel) &&
	    !(pHddCtx->config->etsi_srd_chan_in_master_mode))) {
		hdd_err("SAP SRD master mode not supported. Cannot start SAP");
		return -EINVAL;
	}

	if (cds_is_sap_mandatory_chan_list_enabled()) {
		if (CDS_IS_CHANNEL_5GHZ(channel)) {
			hdd_debug("channel %hu, sap mandatory chan list enabled",
					channel);
			if (!cds_get_sap_mandatory_chan_list_len())
				cds_init_sap_mandatory_2g_chan();
			cds_add_sap_mandatory_chan(channel);
		} else {
			cds_init_sap_mandatory_2g_chan();
		}
	}

	if (cds_is_sub_20_mhz_enabled()) {
		enum channel_state ch_state;
		enum phy_ch_width sub_20_ch_width = CH_WIDTH_INVALID;
		tsap_Config_t *sap_cfg = &pAdapter->sessionCtx.ap.sapConfig;

		/* Avoid ACS/DFS, and overwrite ch wd to 20 */
		if (channel == 0) {
			hdd_err("Can't start SAP-ACS (channel=0) with sub 20 MHz ch width");
			return -EINVAL;
		}
		if (CHANNEL_STATE_DFS == cds_get_channel_state(channel)) {
			hdd_err("Can't start SAP-DFS (channel=%d)with sub 20 MHz ch wd",
				channel);
			return -EINVAL;
		}
		if (channel_width != HW_MODE_20_MHZ) {
			hdd_err("Hostapd (20+ MHz) conflits with config.ini (sub 20 MHz)");
			return -EINVAL;
		}
		if (cds_is_5_mhz_enabled())
			sub_20_ch_width = CH_WIDTH_5MHZ;
		if (cds_is_10_mhz_enabled())
			sub_20_ch_width = CH_WIDTH_10MHZ;
		if (CDS_IS_CHANNEL_5GHZ(channel))
			ch_state = cds_get_5g_bonded_channel_state(channel,
							  sub_20_ch_width);
		else
			ch_state = cds_get_2g_bonded_channel_state(channel,
							  sub_20_ch_width, 0);
		if (CHANNEL_STATE_DISABLE == ch_state) {
			hdd_err("Given ch width not supported by reg domain");
			return -EINVAL;
		}
		sap_cfg->SapHw_mode = eCSR_DOT11_MODE_abg;
	}

	/* check if concurrency is allowed */
	if (!cds_allow_concurrency(
				cds_convert_device_mode_to_qdf_type(
				pAdapter->device_mode),
				channel,
				channel_width)) {
		hdd_err("Connection failed due to concurrency check failure");
		return -EINVAL;
	}

	status = qdf_reset_connection_update();
	if (!QDF_IS_STATUS_SUCCESS(status))
		hdd_err("ERR: clear event failed");

	/*
	 * For Start Ap, the driver checks whether the SAP comes up in a
	 * different or same band( whether we require DBS or Not).
	 * If we dont require DBS, then the driver does nothing assuming
	 * the state would be already in non DBS mode, and just continues
	 * with vdev up on same MAC, by stoping the opportunistic timer,
	 * which results in a connection of 1x1 if already the state was in
	 * DBS. So first stop timer, and check the current hw mode.
	 * If the SAP comes up in band different from STA, DBS mode is already
	 * set. IF not, then well check for upgrade, and shift the connection
	 * back to single MAC 2x2 (if initial was 2x2).
	 */
	cds_checkn_update_hw_mode_single_mac_mode(channel);

	status = cds_current_connections_update(pAdapter->sessionId,
			channel,
			SIR_UPDATE_REASON_START_AP);
	if (status == QDF_STATUS_E_FAILURE) {
		hdd_err("ERROR: connections update failed!!");
		return -EINVAL;
	}

	if (QDF_STATUS_SUCCESS == status) {
		status = qdf_wait_for_connection_update();
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			hdd_err("ERROR: qdf wait for event failed!!");
			return -EINVAL;
		}
	}

	if (QDF_SAP_MODE == pAdapter->device_mode)
		hdd_green_ap_start_bss(pHddCtx);

	if (pAdapter->device_mode == QDF_P2P_GO_MODE) {
		hdd_adapter_t  *p2p_adapter;

		p2p_adapter = hdd_get_adapter(pHddCtx, QDF_P2P_DEVICE_MODE);
		if (p2p_adapter) {
			hdd_debug("Cancel active p2p device ROC before GO starting");
			wlan_hdd_cancel_existing_remain_on_channel(
				p2p_adapter);
		}
	}

	if ((pAdapter->device_mode == QDF_SAP_MODE)
	    || (pAdapter->device_mode == QDF_P2P_GO_MODE)
	    ) {
		beacon_data_t *old, *new;
		enum nl80211_channel_type channel_type;

		old = pAdapter->sessionCtx.ap.beacon;

		if (old)
			return -EALREADY;

		status =
			wlan_hdd_cfg80211_alloc_new_beacon(pAdapter, &new,
							   &params->beacon,
							   params->dtim_period);

		if (status != 0) {
			hdd_err("Error!!! Allocating the new beacon");
			return -EINVAL;
		}
		pAdapter->sessionCtx.ap.beacon = new;

		if (params->chandef.width < NL80211_CHAN_WIDTH_80)
			channel_type = cfg80211_get_chandef_type(
						&(params->chandef));
		else
			channel_type = NL80211_CHAN_HT40PLUS;


		wlan_hdd_set_channel(wiphy, dev,
				     &params->chandef,
				     channel_type);

		hdd_update_beacon_rate(pAdapter, wiphy, params);

		/* set authentication type */
		switch (params->auth_type) {
		case NL80211_AUTHTYPE_OPEN_SYSTEM:
			pAdapter->sessionCtx.ap.sapConfig.authType =
				eSAP_OPEN_SYSTEM;
			break;
		case NL80211_AUTHTYPE_SHARED_KEY:
			pAdapter->sessionCtx.ap.sapConfig.authType =
				eSAP_SHARED_KEY;
			break;
		default:
			pAdapter->sessionCtx.ap.sapConfig.authType =
				eSAP_AUTO_SWITCH;
		}
		pAdapter->sessionCtx.ap.sapConfig.ch_width_orig =
						params->chandef.width;

		status =
			wlan_hdd_cfg80211_start_bss(pAdapter,
				&params->beacon,
				params->ssid, params->ssid_len,
				params->hidden_ssid, true);

		if (status != 0) {
			hdd_err("Error Start bss Failed");
			goto err_start_bss;
		}

		if (pHddCtx->config->sap_max_inactivity_override) {
			sta_inactivity_timer = qdf_mem_malloc(
					sizeof(*sta_inactivity_timer));
			if (!sta_inactivity_timer) {
				hdd_err("Failed to allocate Memory");
				return QDF_STATUS_E_FAILURE;
			}
			sta_inactivity_timer->session_id = pAdapter->sessionId;
			sta_inactivity_timer->sta_inactivity_timeout =
				params->inactivity_timeout;
			sme_update_sta_inactivity_timeout(WLAN_HDD_GET_HAL_CTX
					(pAdapter), sta_inactivity_timer);
			qdf_mem_free(sta_inactivity_timer);
		}

		if (status == 0) {
			if (0 !=
				wlan_hdd_set_udp_resp_offload(pAdapter, true))
				hdd_debug("set udp resp cmd failed %d",
								status);
		}
	}

	goto success;

err_start_bss:
	if (pAdapter->sessionCtx.ap.beacon)
		qdf_mem_free(pAdapter->sessionCtx.ap.beacon);
	pAdapter->sessionCtx.ap.beacon = NULL;
success:
	EXIT();
	return status;
}

/**
 * wlan_hdd_cfg80211_start_ap() - start sap
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to netdev
 * @params: Pointer to start ap configuration parameters
 *
 * Return: zero for success non-zero for failure
 */
int wlan_hdd_cfg80211_start_ap(struct wiphy *wiphy,
				struct net_device *dev,
				struct cfg80211_ap_settings *params)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_start_ap(wiphy, dev, params);
	cds_ssr_unprotect(__func__);

	return ret;
}

/**
 * __wlan_hdd_cfg80211_change_beacon() - change beacon for sofatap/p2p go
 * @wiphy: Pointer to wiphy structure
 * @dev: Pointer to net_device structure
 * @params: Pointer to change beacon parameters
 *
 * Return: 0 for success non-zero for failure
 */
static int __wlan_hdd_cfg80211_change_beacon(struct wiphy *wiphy,
					struct net_device *dev,
					struct cfg80211_beacon_data *params)
{
	hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *pHddCtx;
	beacon_data_t *old, *new;
	int status;

	ENTER();

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EINVAL;
	}

	if (wlan_hdd_validate_session_id(pAdapter->sessionId)) {
		hdd_err("invalid session id: %d", pAdapter->sessionId);
		return -EINVAL;
	}

	MTRACE(qdf_trace(QDF_MODULE_ID_HDD,
			 TRACE_CODE_HDD_CFG80211_CHANGE_BEACON,
			 pAdapter->sessionId, pAdapter->device_mode));
	hdd_debug("Device_mode %s(%d)",
	       hdd_device_mode_to_string(pAdapter->device_mode),
	       pAdapter->device_mode);

	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	status = wlan_hdd_validate_context(pHddCtx);

	if (0 != status)
		return status;

	if (!(pAdapter->device_mode == QDF_SAP_MODE ||
	      pAdapter->device_mode == QDF_P2P_GO_MODE)) {
		return -EOPNOTSUPP;
	}

	old = pAdapter->sessionCtx.ap.beacon;

	if (!old) {
		hdd_err("session id: %d beacon data points to NULL",
		       pAdapter->sessionId);
		return -EINVAL;
	}

	status = wlan_hdd_cfg80211_alloc_new_beacon(pAdapter, &new, params, 0);

	if (status != QDF_STATUS_SUCCESS) {
		hdd_err("new beacon alloc failed");
		return -EINVAL;
	}

	pAdapter->sessionCtx.ap.beacon = new;
	hdd_debug("update beacon for P2P GO/SAP");
	status = wlan_hdd_cfg80211_start_bss(pAdapter, params, NULL,
					0, 0, false);

	EXIT();
	return status;
}

/**
 * wlan_hdd_cfg80211_change_beacon() - change beacon content in sap mode
 * @wiphy: Pointer to wiphy
 * @dev: Pointer to netdev
 * @params: Pointer to change beacon parameters
 *
 * Return: zero for success non-zero for failure
 */
int wlan_hdd_cfg80211_change_beacon(struct wiphy *wiphy,
				struct net_device *dev,
				struct cfg80211_beacon_data *params)
{
	int ret;

	cds_ssr_protect(__func__);
	ret = __wlan_hdd_cfg80211_change_beacon(wiphy, dev, params);
	cds_ssr_unprotect(__func__);

	return ret;
}

bool hdd_is_peer_associated(hdd_adapter_t *adapter,
			    struct qdf_mac_addr *mac_addr)
{
	uint32_t cnt = 0;
	hdd_station_info_t *sta_info = NULL;

	if (!adapter || !mac_addr) {
		hdd_err("Invalid adapter or mac_addr");
		return false;
	}

	sta_info = adapter->aStaInfo;
	spin_lock_bh(&adapter->staInfo_lock);
	for (cnt = 0; cnt < WLAN_MAX_STA_COUNT; cnt++) {
		if ((sta_info[cnt].isUsed) &&
		    !qdf_mem_cmp(&(sta_info[cnt].macAddrSTA), mac_addr,
		    QDF_MAC_ADDR_SIZE))
			break;
	}
	spin_unlock_bh(&adapter->staInfo_lock);
	if (cnt != WLAN_MAX_STA_COUNT)
		return true;

	return false;
}

/**
 * hdd_sap_indicate_disconnect_for_sta() - Indicate disconnect indication
 * to supplicant, if there any clients connected to SAP interface.
 * @adapter: sap adapter context
 *
 * Return:   nothing
 */
void hdd_sap_indicate_disconnect_for_sta(hdd_adapter_t *adapter)
{
	tSap_Event sap_event;
	int sta_id;
	ptSapContext sap_ctx;

	ENTER();

	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter);
	if (!sap_ctx) {
		hdd_err("invalid sap context");
		return;
	}

	for (sta_id = 0; sta_id < WLAN_MAX_STA_COUNT; sta_id++) {
		if (adapter->aStaInfo[sta_id].isUsed) {
			hdd_debug("sta_id: %d isUsed: %d %pK",
				 sta_id, adapter->aStaInfo[sta_id].isUsed,
				 adapter);

			if (qdf_is_macaddr_broadcast(
				&adapter->aStaInfo[sta_id].macAddrSTA))
				continue;

			sap_event.sapHddEventCode = eSAP_STA_DISASSOC_EVENT;
			qdf_mem_copy(
				&sap_event.sapevt.
				sapStationDisassocCompleteEvent.staMac,
				&adapter->aStaInfo[sta_id].macAddrSTA,
				sizeof(struct qdf_mac_addr));
			sap_event.sapevt.sapStationDisassocCompleteEvent.
			reason =
				eSAP_MAC_INITATED_DISASSOC;
			sap_event.sapevt.sapStationDisassocCompleteEvent.
			statusCode =
				QDF_STATUS_E_RESOURCES;
			hdd_hostapd_sap_event_cb(&sap_event,
					sap_ctx->pUsrContext);
		}
	}

	EXIT();
}

/**
 * hdd_sap_destroy_events() - Destroy sap evets
 * @adapter: sap adapter context
 *
 * Return:   nothing
 */
void hdd_sap_destroy_events(hdd_adapter_t *adapter)
{
	ptSapContext sap_ctx;

	ENTER();
	sap_ctx = WLAN_HDD_GET_SAP_CTX_PTR(adapter);
	if (!sap_ctx) {
		hdd_err("invalid sap context");
		return;
	}

	qdf_event_destroy(&sap_ctx->sap_session_opened_evt);
	if (!QDF_IS_STATUS_SUCCESS(
		qdf_mutex_destroy(&sap_ctx->SapGlobalLock))) {
		QDF_TRACE(QDF_MODULE_ID_SAP, QDF_TRACE_LEVEL_ERROR,
			  "wlansap_stop failed destroy lock");
		return;
	}
	EXIT();
}
