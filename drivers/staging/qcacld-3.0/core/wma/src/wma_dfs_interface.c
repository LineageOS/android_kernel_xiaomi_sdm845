/*
 * Copyright (c) 2013-2017 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/**
 * DOC: wma_dfs_interface.c
 *
 * Source code borrowed from QCA_MAIN DFS module
 */

#include "wma.h"
#include "ath_dfs_structs.h"
#include "wma_dfs_interface.h"
#include "dfs_interface.h"

#ifndef ATH_SUPPORT_DFS
#define ATH_SUPPORT_DFS 1
#endif

/**
 * ol_if_dfs_attach() - dfs attach
 * @ic: ieee80211com ptr
 * @ptr: ath_dfs_caps ptr
 * @radar_info: radar info
 *
 * Return: 0 for success or error code
 */
int ol_if_dfs_attach(struct ieee80211com *ic, void *ptr, void *radar_info)
{
	struct ath_dfs_caps *pCap = (struct ath_dfs_caps *)ptr;

	qdf_print("%s: called; ptr=%pK, radar_info=%pK\n",
		  __func__, ptr, radar_info);

	pCap->ath_chip_is_bb_tlv = 1;
	pCap->ath_dfs_combined_rssi_ok = 0;
	pCap->ath_dfs_ext_chan_ok = 0;
	pCap->ath_dfs_use_enhancement = 0;
	pCap->ath_strong_signal_diversiry = 0;
	pCap->ath_fastdiv_val = 0;

	return 0;
}

/**
 * ol_if_get_tsf64() - Place Holder API
 * @ic: ieee80211com ptr
 *
 * We get the tsf from Firmware.
 *
 * Return: always return success(0)
 */
uint64_t ol_if_get_tsf64(struct ieee80211com *ic)
{
	return 0;
}

/**
 * ol_if_dfs_disable() - Place Holder API
 * @ic: ieee80211com ptr
 *
 * ic_dfs_disable is just a place holder
 * function since firmware takes care of
 * disabling the dfs phyerrors disabling.
 *
 * Return: always return success(0)
 */
int ol_if_dfs_disable(struct ieee80211com *ic)
{
	return 0;
}

/**
 * ieee80211_find_channel() - find ieee80211 channel
 * @ic: ieee80211com ptr
 * @freq: frequency
 * @flags: flags
 *
 * Locate a channel given a frequency+flags. We cache
 * the previous lookup to optimize swithing between
 * two channels--as happens with dynamic turbo.
 * This verifies that found channels have not been
 * excluded because of 11d.
 *
 * Return: returns dfs_ieee80211_channel or NULL for error
 */
struct dfs_ieee80211_channel *ieee80211_find_channel(struct ieee80211com *ic,
						 int freq, uint32_t flags)
{
	struct dfs_ieee80211_channel *c;
	int i;

	flags &= IEEE80211_CHAN_ALLTURBO;
	/* brute force search */
	for (i = 0; i < ic->ic_nchans; i++) {
		c = &ic->ic_channels[i];

		if ((!IEEE80211_IS_CHAN_11D_EXCLUDED(c)) &&
		    (c->ic_freq == freq) &&
		    ((c->ic_flags & IEEE80211_CHAN_ALLTURBO) == flags)) {
			return c;
		}
	}

	return NULL;
}

/**
 * ic_dfs_enable() - enable DFS
 * @ic: ieee80211com ptr
 * @is_fastclk: is fastclock
 *
 * For offload solutions, radar PHY errors will be enabled
 * by the target firmware when DFS is requested for the
 * current channel.
 *
 * Return: Always returns success
 */
int ol_if_dfs_enable(struct ieee80211com *ic, int *is_fastclk, void *pe)
{
	/*
	 * For peregrine, treat fastclk as the "oversampling" mode.
	 * It's on by default.  This may change at some point, so
	 * we should really query the firmware to find out what
	 * the current configuration is.
	 */
	(*is_fastclk) = 1;

	return 0;
}

/**
 * ieee80211_ieee2mhz() - Convert IEEE channel number to MHz frequency.
 * @chan: channel number
 * @flags: flags
 *
 * Return: frequency in MHz
 */
uint32_t ieee80211_ieee2mhz(uint32_t chan, uint32_t flags)
{
	if (flags & IEEE80211_CHAN_2GHZ) {
		/* 2GHz band */
		if (chan == 14)
			return 2484;
		if (chan < 14)
			return 2407 + chan * 5;
		else
			return 2512 + ((chan - 15) * 20);
	} else if (flags & IEEE80211_CHAN_5GHZ) {
		/* 5Ghz band */
		return 5000 + (chan * 5);
	}
	/* either, guess */
	if (chan == 14)
		return 2484;
	if (chan < 14)  /* 0-13 */
		return 2407 + chan * 5;
	if (chan < 27)  /* 15-26 */
		return 2512 + ((chan - 15) * 20);
	return 5000 + (chan * 5);
}

/**
 * ol_if_dfs_get_ext_busy() - Place holder function ic_get_ext_busy
 * @ic: ieee80211com ptr
 *
 * Return: Always return success (0)
 */
int ol_if_dfs_get_ext_busy(struct ieee80211com *ic)
{
	return 0;
}

/**
 * ol_if_dfs_get_mib_cycle_counts_pct() - Place holder function
 * @ic: ieee80211com ptr
 *
 * Return: Always return success (0)
 */
int
ol_if_dfs_get_mib_cycle_counts_pct(struct ieee80211com *ic,
				   uint32_t *rxc_pcnt, uint32_t *rxf_pcnt,
				   uint32_t *txf_pcnt)
{
	return 0;
}

/**
 * ol_if_dfs_usenol() - dfs usenol call
 * @ic: ieee80211com ptr
 *
 * Return: 0 fo success or error code
 */
uint16_t ol_if_dfs_usenol(struct ieee80211com *ic)
{
#if ATH_SUPPORT_DFS
	return dfs_usenol(ic);
#else
	return 0;
#endif /* ATH_SUPPORT_DFS */
	return 0;
}

/**
 * ieee80211_mark_dfs() - indicate radar on current operating freq
 * @ic: ieee80211com ptr
 * @ichan: channel
 *
 * Function to indicate Radar on the current
 * SAP operating channel.This indication will
 * be posted to SAP to select a new channel
 * randomly and issue a vdev restart to
 * operate on the new channel.
 *
 * Return: none
 */
void
ieee80211_mark_dfs(struct ieee80211com *ic, struct dfs_ieee80211_channel *ichan)
{
	int status;

	status = wma_dfs_indicate_radar(ic, ichan);
}

#ifdef FEATURE_SPECTRAL_SCAN
/**
 * wma_ieee80211_secondary20_channel_offset() finds the offset for
 * secondary channel
 * @chan: channel for which secondary offset to find
 *
 * Return: secondary offset
 */
int8_t
wma_ieee80211_secondary20_channel_offset(struct dfs_ieee80211_channel *chan)
{
	int8_t pri_center_ch_diff, sec_level;
	u_int16_t pri_chan_40_center;
	int8_t offset = 0;

	if (!chan || IEEE80211_IS_CHAN_A(chan) ||
	    IEEE80211_IS_CHAN_B(chan) || IEEE80211_IS_CHAN_G(chan) ||
	    IEEE80211_IS_CHAN_PUREG(chan) || IEEE80211_IS_CHAN_ANYG(chan) ||
	    IEEE80211_IS_CHAN_11N_HT20(chan) ||
	    IEEE80211_IS_CHAN_11AC_VHT20(chan)) {
		/* No secondary channel */
		return 0;
	}

	if (IEEE80211_IS_CHAN_11AC_VHT40PLUS(chan) ||
	    IEEE80211_IS_CHAN_11NG_HT40PLUS(chan) ||
	    IEEE80211_IS_CHAN_11NA_HT40PLUS(chan)) {
		return 1;
	}

	if (IEEE80211_IS_CHAN_11AC_VHT40MINUS(chan) ||
	    IEEE80211_IS_CHAN_11NG_HT40MINUS(chan) ||
	    IEEE80211_IS_CHAN_11NA_HT40MINUS(chan)) {
		offset = -1;
		return offset;
	}

	if (IEEE80211_IS_CHAN_11AC_VHT80(chan) ||
	    IEEE80211_IS_CHAN_11AC_VHT80P80(chan)) {
		/* The following logic generates the extension channel offset
		 * from the primary channel(ic_ieee) and 80M channel central
		 * frequency.
		 * The channelization for 80M is as following:
		 * | 20M  20M  20M  20M |
		 * | 36   40   44   48  |
		 * |         80M        |
		 * The central frequency is 42 in the example.
		 * If the primary channel is 36 and 44, the extension channel
		 * is 40PLUS. If the primary channel is 40 and 48 the extension
		 * channel is 40MINUS.
		 */

		if (chan->ic_ieee < chan->ic_vhtop_ch_freq_seg1) {
			if ((chan->ic_vhtop_ch_freq_seg1 - chan->ic_ieee) > 4)
				return 1;

			offset = -1;
			return offset;
		}

		if ((chan->ic_ieee - chan->ic_vhtop_ch_freq_seg1) > 4) {
			offset = -1;
			return offset;
		}
		return 1;
	}

	if (IEEE80211_IS_CHAN_11AC_VHT160(chan)) {
		/* The channelization of 160M is as following:
		 * | 20M 20M 20M 20M 20M 20M 20M 20M |
		 * | 36  40  44  48  52  56  60  64  |
		 * The center frequency is 50 in this example.
		 * If primary channel is 36, 44, 52 or 60, the extension channel
		 * is 40PLUS.
		 * If primary channel is 40, 48, 56 or 64, the extension channel
		 * is 40MINUS.
		 */

		pri_center_ch_diff = chan->ic_ieee -
				chan->ic_vhtop_ch_freq_seg2;

		if (pri_center_ch_diff > 0)
			sec_level = -1;
		else
			sec_level = 1;

		if (sec_level * pri_center_ch_diff < -6)
			pri_chan_40_center = chan->ic_vhtop_ch_freq_seg2 -
					(2 * sec_level*6);
		else
			pri_chan_40_center = chan->ic_vhtop_ch_freq_seg2 -
					(2 * sec_level*2);

		if (pri_chan_40_center > chan->ic_ieee)
			return 1;
		offset = -1;
		return offset;
	}

	return 0;
}
#endif
