/*
 * Copyright (c) 2016-2017 The Linux Foundation. All rights reserved.
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

#if !defined __HDD_REGULATORY_H
#define __HDD_REGULATORY_H

/**
 * DOC: wlan_hdd_regulatory.h
 *
 * HDD Regulatory prototype implementation
 */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0)) || defined(WITH_BACKPORTS)
#define IEEE80211_CHAN_PASSIVE_SCAN IEEE80211_CHAN_NO_IR
#define IEEE80211_CHAN_NO_IBSS IEEE80211_CHAN_NO_IR
#endif

void hdd_reset_global_reg_params(void);
int hdd_regulatory_init(hdd_context_t *hdd_ctx, struct wiphy *wiphy);
void hdd_program_country_code(hdd_context_t *hdd_ctx);

/**
 * hdd_update_indoor_channel() - enable/disable indoor channel
 * @hdd_ctx: hdd context
 * @disable: whether to enable / disable indoor channel
 *
 * enable/disable indoor channel in wiphy/cds
 *
 * Return: void
 */
void hdd_update_indoor_channel(hdd_context_t *hdd_ctx,
					bool disable);
/**
 * hdd_modify_indoor_channel_state_flags() - modify wiphy flags and cds state
 * @wiphy_chan: wiphy channel number
 * @cds_chan: cds channel structure
 * @chan_enum: channel enum maintain in reg db
 * @chan_num: channel index
 * @disable: Disable/enable the flags
 *
 * Modify wiphy flags and cds state if channel is indoor.
 *
 * Return: void
 */
void hdd_modify_indoor_channel_state_flags(
	hdd_context_t *hdd_ctx,
	struct ieee80211_channel *wiphy_chan,
	struct regulatory_channel *cds_chan,
	enum channel_enum chan_enum, int chan_num, bool disable);

/**
 * hdd_apply_cached_country_info() - apply cached ctry info
 * @hdd_ctx: hdd context
 *
 * Return: Error code
 */
int hdd_apply_cached_country_info(hdd_context_t *hdd_ctx);

#endif
