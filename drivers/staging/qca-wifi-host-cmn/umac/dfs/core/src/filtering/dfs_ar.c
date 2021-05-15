/*
 * Copyright (c) 2013, 2016-2017 The Linux Foundation. All rights reserved.
 * Copyright (c) 2002-2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * DOC: This file contains now obsolete code which used to implement AR
 * (Adaptive Radio) feature for older chipsets.
 */

#include "../dfs.h"

#define UPDATE_TOP_THREE_PEAKS(_histo, _peakPtrList, _currWidth) \
	do {	\
		if ((_histo)[(_peakPtrList)[0]] < (_histo)[(_currWidth)]) { \
			(_peakPtrList)[2] = \
			(_currWidth != (_peakPtrList)[1]) ? \
			(_peakPtrList)[1] : (_peakPtrList)[2];  \
			(_peakPtrList)[1] = (_peakPtrList)[0]; \
			(_peakPtrList)[0] = (_currWidth); \
		} else if ((_currWidth != (_peakPtrList)[0]) \
				&& ((_histo)[(_peakPtrList)[1]] <	\
					(_histo)[(_currWidth)])) { \
			(_peakPtrList)[2] = (_peakPtrList)[1]; \
			(_peakPtrList)[1] = (_currWidth);      \
		} else if ((_currWidth != (_peakPtrList)[1])   \
				&& (_currWidth != (_peakPtrList)[0])  \
				&& ((_histo)[(_peakPtrList)[2]] < \
					(_histo)[(_currWidth)])) { \
			(_peakPtrList)[2] = (_currWidth);  \
		} \
	} while (0)

void dfs_process_ar_event(struct wlan_dfs *dfs,
		struct dfs_channel *chan)
{
	struct dfs_ar_state *ar;
	struct dfs_event *re = NULL;
	uint32_t sumpeak = 0, numpeaks = 0;
	uint32_t rssi = 0, width = 0;
	uint32_t origregionsum = 0, i = 0;
	uint16_t thistimestamp;
	int empty;

	if (!dfs) {
		dfs_err(dfs, WLAN_DEBUG_DFS_ALWAYS,  "dfs is NULL");
		return;
	}

	ar = (struct dfs_ar_state *) &(dfs->dfs_ar_state);
	WLAN_ARQ_LOCK(dfs);
	empty = STAILQ_EMPTY(&(dfs->dfs_arq));
	WLAN_ARQ_UNLOCK(dfs);
	while (!empty) {
		WLAN_ARQ_LOCK(dfs);
		re = STAILQ_FIRST(&(dfs->dfs_arq));
		if (re != NULL)
			STAILQ_REMOVE_HEAD(&(dfs->dfs_arq), re_list);
		WLAN_ARQ_UNLOCK(dfs);
		if (!re)
			return;

		thistimestamp = re->re_ts;
		rssi = re->re_rssi;
		width = re->re_dur;

		/* Return the dfs event to the free event list. */
		qdf_mem_zero(re, sizeof(struct dfs_event));
		WLAN_DFSEVENTQ_LOCK(dfs);
		STAILQ_INSERT_TAIL(&(dfs->dfs_eventq), re, re_list);
		WLAN_DFSEVENTQ_UNLOCK(dfs);

		/*
		 * Determine if current radar is an extension of previous
		 * radar.
		 */
		if (ar->ar_prevwidth == 255) {
			/*
			 * Tag on previous width for consideraion of low data
			 * rate ACKs.
			 */
			ar->ar_prevwidth += width;
			width = (width == 255) ? 255 : ar->ar_prevwidth;
		} else if ((width == 255) &&
				(ar->ar_prevwidth == 510 ||
				 ar->ar_prevwidth == 765 ||
				 ar->ar_prevwidth == 1020)) {
			/*
			 * Aggregate up to 5 consecuate max radar widths to
			 * consider 11Mbps long preamble 1500-byte pkts.
			 */
			ar->ar_prevwidth += width;
		} else if (ar->ar_prevwidth == 1275 && width != 255) {
			/* Found 5th consecute maxed out radar, reset history */
			width += ar->ar_prevwidth;
			ar->ar_prevwidth = 0;
		} else if (ar->ar_prevwidth > 255) {
			/*
			 * Ignore if there are less than 5 consecutive maxed
			 * out radars.
			 */
			ar->ar_prevwidth = width;
			width = 255;
		} else {
			ar->ar_prevwidth = width;
		}

		/*
		 * For ignoring noises with radar duration in ranges
		 * of 3-30: AP4x. Region 7 - 5.5Mbps (long pre)
		 * ACK = 270 = 216 us.
		 */
		if ((width >= 257 && width <= 278) ||
				/*
				 * Region 8 - 2Mbps (long pre)
				 * ACKC = 320 = 256us.
				 */
				(width >= 295 && width <= 325) ||
				(width >= 1280 && width <= 1300)) {
			uint16_t wraparoundadj = 0;
			uint16_t base = (width >= 1280) ? 1275 : 255;

			if (thistimestamp < ar->ar_prevtimestamp)
				wraparoundadj = 32768;

			if ((thistimestamp + wraparoundadj -
				    ar->ar_prevtimestamp) != (width - base))
				width = 1;
		}
		if (width <= 10) {
			WLAN_ARQ_LOCK(dfs);
			empty = STAILQ_EMPTY(&(dfs->dfs_arq));
			WLAN_ARQ_UNLOCK(dfs);
			continue;
		}

		/*
		 * Overloading the width=2 in: Store a count of
		 * radars w/max duration and high RSSI (not noise)
		 */
		if ((width == 255) && (rssi > DFS_AR_RSSI_THRESH_STRONG_PKTS))
			width = 2;

		/*
		 * Overloading the width=3 bin:
		 * Double and store a count of rdars of durtaion that matches
		 * 11Mbps (long preamble) TCP ACKs or 1500-byte data packets.
		 */
		if ((width >= 1280 && width <= 1300) ||
				(width >= 318 && width <= 325)) {
			width = 3;
			ar->ar_phyerrcount[3] += 2;
			ar->ar_acksum += 2;
		}

		/* Build histogram of radar duration. */
		if (width > 0 && width <= 510)
			ar->ar_phyerrcount[width]++;
		else {
			/* Invalid radar width, throw it away. */
			WLAN_ARQ_LOCK(dfs);
			empty = STAILQ_EMPTY(&(dfs->dfs_arq));
			WLAN_ARQ_UNLOCK(dfs);
			continue;
		}

		/* Received radar of interest (i.e., signature match),
		 * proceed to check if there is enough neighboring
		 * traffic to drop out of Turbo.
		 */
		/* Region 0: 24Mbps ACK = 35 = 28us */
		if ((width >= REG0_MIN_WIDTH && width <= REG0_MAX_WIDTH) ||
			/* Region 1: 12Mbps ACK = 40 = 32us */
			(width >= REG1_MIN_WIDTH && width <= REG1_MAX_WIDTH) ||
			/* Region 2: 6Mbps ACK = 55 = 44us */
			(width >= REG2_MIN_WIDTH && width <= REG2_MAX_WIDTH) ||
			/* Region 3: 11Mbps ACK = 135 = 108us */
			(width >= REG3_MIN_WIDTH && width <= REG3_MAX_WIDTH) ||
			/* Region 4: 5.5Mbps ACK = 150 = 120us */
			(width >= REG4_MIN_WIDTH && width <= REG4_MAX_WIDTH) ||
			/* Region 5: 2Mbps ACK = 200 = 160us */
			(width >= REG5_MIN_WIDTH && width <= REG5_MAX_WIDTH) ||
			/* Region 6: 1Mbps ACK = 400 = 320us */
			(width >= REG6_MIN_WIDTH && width <= REG6_MAX_WIDTH) ||
			/* Region 7: 5.5Mbps (Long Pre) ACK = 270 = 216us. */
			(width >= REG7_MIN_WIDTH && width <= REG7_MAX_WIDTH) ||
			/* Region 8: 2Mbps (Long Pre) ACK = 320 = 256us. */
			(width >= REG8_MIN_WIDTH && width <= REG8_MAX_WIDTH) ||
			/*
			 * Ignoring Region 9 due to overlap with 255 which is
			 * same as board noise.
			 */
			/* Region 9: 11Mbps (Long Pre) ACK = 255 = 204us. */
			(width == 3)) {
			ar->ar_acksum++;
			/*
			 * Double the count for strong radars that match
			 * one of the ACK signatures.
			 */
			if (rssi > DFS_AR_RSSI_DOUBLE_THRESHOLD) {
				ar->ar_phyerrcount[width]++;
				ar->ar_acksum++;
			}
			UPDATE_TOP_THREE_PEAKS(ar->ar_phyerrcount,
				ar->ar_peaklist, width);
			/* Sum the counts of these peaks. */
			numpeaks = DFS_AR_MAX_NUM_PEAKS;
			origregionsum = ar->ar_acksum;
			for (i = 0; i < DFS_AR_MAX_NUM_PEAKS; i++) {
				if (ar->ar_peaklist[i] > 0) {
					if ((i == 0) &&
						(ar->ar_peaklist[i] == 3) &&
						(ar->ar_phyerrcount[3] <
						 ar->ar_phyerrcount[2]) &&
						(ar->ar_phyerrcount[3] > 6)) {
						/*
						 * If the top peak is one that
						 * matches the 11Mbps long
						 * preamble TCP Ack/1500-byte
						 * data, include the count for
						 * radars that hav emax duration
						 * and high rssi (width = 2) to
						 * boost the sum for the PAR
						 * test that follows.
						 */
						sumpeak +=
						    (ar->ar_phyerrcount[2] +
						     ar->ar_phyerrcount[3]);
						ar->ar_acksum +=
						    (ar->ar_phyerrcount[2] +
						     ar->ar_phyerrcount[3]);
					} else {
						sumpeak += ar->ar_phyerrcount[
						    ar->ar_peaklist[i]];
					}
				} else
					numpeaks--;
			}
			/*
			 * If sum of patterns matches exceeds packet threshold,
			 * perform comparison between peak-to-avg ratio against
			 * parThreshold.
			 */
			if ((ar->ar_acksum > ar->ar_packetthreshold) &&
				((sumpeak * DFS_AR_REGION_WIDTH) >
				 (ar->ar_parthreshold * numpeaks *
				  ar->ar_acksum))) {
				/*
				 * Neighboring traffic detected, get out of
				 * Turbo.
				 */
				chan->dfs_ch_flagext |= CHANNEL_INTERFERENCE;
				qdf_mem_zero(ar->ar_peaklist,
						sizeof(ar->ar_peaklist));
				ar->ar_acksum = 0;
				qdf_mem_zero(ar->ar_phyerrcount,
					sizeof(ar->ar_phyerrcount));
			} else {
				/*
				 * Reset sum of matches to discount the count
				 * of strong radars with max duration.
				 */
				ar->ar_acksum = origregionsum;
			}
		}
		ar->ar_prevtimestamp = thistimestamp;
		WLAN_ARQ_LOCK(dfs);
		empty = STAILQ_EMPTY(&(dfs->dfs_arq));
		WLAN_ARQ_UNLOCK(dfs);
	}
}

void dfs_reset_ar(struct wlan_dfs *dfs)
{
	if (!dfs) {
		dfs_err(dfs, WLAN_DEBUG_DFS_ALWAYS,  "dfs is NULL");
		return;
	}

	qdf_mem_zero(&dfs->dfs_ar_state, sizeof(dfs->dfs_ar_state));
	dfs->dfs_ar_state.ar_packetthreshold = DFS_AR_PKT_COUNT_THRESH;
	dfs->dfs_ar_state.ar_parthreshold = DFS_AR_ACK_DETECT_PAR_THRESH;
}

void dfs_reset_arq(struct wlan_dfs *dfs)
{
	struct dfs_event *event;

	if (!dfs) {
		dfs_err(dfs, WLAN_DEBUG_DFS_ALWAYS,  "dfs is NULL");
		return;
	}

	WLAN_ARQ_LOCK(dfs);
	WLAN_DFSEVENTQ_LOCK(dfs);
	while (!STAILQ_EMPTY(&(dfs->dfs_arq))) {
		event = STAILQ_FIRST(&(dfs->dfs_arq));
		STAILQ_REMOVE_HEAD(&(dfs->dfs_arq), re_list);
		qdf_mem_zero(event, sizeof(struct dfs_event));
		STAILQ_INSERT_TAIL(&(dfs->dfs_eventq), event, re_list);
	}
	WLAN_DFSEVENTQ_UNLOCK(dfs);
	WLAN_ARQ_UNLOCK(dfs);
}
