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
 * DOC : wlan_hdd_spectralscan.h
 *
 * WLAN Host Device Driver spectral scan implementation
 *
 */

#if !defined(WLAN_HDD_SPECTRALSCAN_H)
#define WLAN_HDD_SPECTRALSCAN_H

#ifdef FEATURE_SPECTRAL_SCAN
/* Static default values for spectral state and configuration. */
#define SPECTRAL_SCAN_ACTIVE_DEFAULT           (0)
#define SPECTRAL_SCAN_ENABLE_DEFAULT           (0)
#define SPECTRAL_SCAN_COUNT_DEFAULT            (0)
#define SPECTRAL_SCAN_PERIOD_DEFAULT           (35)
#define SPECTRAL_SCAN_PRIORITY_DEFAULT         (1)
#define SPECTRAL_SCAN_FFT_SIZE_DEFAULT         (7)
#define SPECTRAL_SCAN_GC_ENA_DEFAULT           (1)
#define SPECTRAL_SCAN_RESTART_ENA_DEFAULT      (0)
#define SPECTRAL_SCAN_NOISE_FLOOR_REF_DEFAULT  (-96)
#define SPECTRAL_SCAN_INIT_DELAY_DEFAULT       (80)
#define SPECTRAL_SCAN_NB_TONE_THR_DEFAULT      (12)
#define SPECTRAL_SCAN_STR_BIN_THR_DEFAULT      (8)
#define SPECTRAL_SCAN_WB_RPT_MODE_DEFAULT      (0)
#define SPECTRAL_SCAN_RSSI_RPT_MODE_DEFAULT    (0)
#define SPECTRAL_SCAN_RSSI_THR_DEFAULT         (0xf0)
#define SPECTRAL_SCAN_PWR_FORMAT_DEFAULT       (0)
#define SPECTRAL_SCAN_RPT_MODE_DEFAULT         (2)
#define SPECTRAL_SCAN_BIN_SCALE_DEFAULT        (1)
#define SPECTRAL_SCAN_DBM_ADJ_DEFAULT          (1)
#define SPECTRAL_SCAN_CHN_MASK_DEFAULT         (1)

#define MAX_SPECTRAL_PAYLOAD 1500

enum spectral_scan_msg_type {
	SPECTRAL_SCAN_REGISTER_REQ,
	SPECTRAL_SCAN_REGISTER_RSP,
};

struct spectral_scan_msg {
	uint32_t msg_type;
	uint32_t pid;
};

#define FEATURE_SPECTRAL_SCAN_VENDOR_COMMANDS \
{ \
	.info.vendor_id = QCA_NL80211_VENDOR_ID, \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_START, \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV | \
			WIPHY_VENDOR_CMD_NEED_NETDEV, \
	.doit = wlan_hdd_cfg80211_spectral_scan_start \
}, \
{ \
	.info.vendor_id = QCA_NL80211_VENDOR_ID, \
	.info.subcmd = QCA_NL80211_VENDOR_SUBCMD_SPECTRAL_SCAN_STOP, \
	.flags = WIPHY_VENDOR_CMD_NEED_WDEV | \
		WIPHY_VENDOR_CMD_NEED_NETDEV, \
	.doit = wlan_hdd_cfg80211_spectral_scan_stop \
},

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
						int data_len);

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
						int data_len);

/**
 * spectral_scan_activate_service() - Activate spectral scan  message handler
 *
 * This function registers a handler to receive netlink message from
 * the spectral scan application process.
 *
 * Return - 0 for success, non zero for failure
 */
int spectral_scan_activate_service(void);

/**
 * spectral_scan_deactivate_service() - Deactivate spectral scan message handler
 *
 * This function deregisters a handler to receive netlink message from
 * the spectral scan application process.
 *
 * Return - 0 for success, non zero for failure
 */
int spectral_scan_deactivate_service(void);

/**
 * hdd_init_spectral_scan() - Initialize spectral scan config parameters
 *
 * This function initialize spectral scan configuration parameters
 * with default values.
 * @hdd_ctx: HDD context pointer
 *
 * Return - None
 */
void hdd_init_spectral_scan(hdd_context_t *hdd_ctx);

/**
 * hdd_register_spectral_scan_cb() - register callback function for sending
 *	spectral scan results to user space.
 *
 * @hdd_ctx: HDD context pointer
 * @cb: callback function
 *
 * Return - None
 */
void hdd_register_spectral_scan_cb(hdd_context_t *hdd_ctx,
				void (*cb)(void *, struct spectral_samp_msg *));

/**
 * spectral_scan_callback() - callback function for sending
 *      spectral scan results to user space.
 *
 * @hdd_ctx: HDD context pointer
 * @samp_msg: SAMP message that contains spectral scan result
 *
 * Return - None
 */
void spectral_scan_callback(void *context, struct spectral_samp_msg *samp_msg);
#else
static inline int spectral_scan_activate_service(void)
{
	return 0;
}

static inline int spectral_scan_deactivate_service(void)
{
	return 0;
}

static inline void hdd_init_spectral_scan(hdd_context_t *hdd_ctx)
{
}

static inline void hdd_register_spectral_scan_cb(hdd_context_t *hdd_ctx,
				void (*cb)(void *, struct spectral_samp_msg *))
{
}

static inline void spectral_scan_callback(void *context,
				struct spectral_samp_msg *samp_msg)
{
}

#define FEATURE_SPECTRAL_SCAN_VENDOR_COMMANDS
#endif
#endif

