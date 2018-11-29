/*
 * Copyright (c) 2017 The Linux Foundation. All rights reserved.
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

#ifndef _WMA_SPECTRAL_H_
#define _WMA_SPECTRAL_H_

#include "qdf_types.h"
#include "sir_api.h"

#define MAX_SPECTRAL_PAYLOAD 1500
#define MAX_NUM_BINS 520
#define SPECTRAL_SIGNATURE  0xdeadbeef

/* Used for the SWAR to obtain approximate combined rssi
 *  in secondary 80Mhz segment
 */
#define OFFSET_CH_WIDTH_20	65
#define OFFSET_CH_WIDTH_40	62
#define OFFSET_CH_WIDTH_80	56
#define OFFSET_CH_WIDTH_160	50

#define SPECTRAL_PHYERR_SIGNATURE           0xbb
#define TLV_TAG_SEARCH_FFT_REPORT           0xFB

/**
 * struct spectral_perchain_rssi_info - rssi information for different bandwidth
 * @rssi_pri20: rssi for primary 20Mhz channel
 * @rssi_sec20: rssi for secondary 20Mhz channel
 * @rssi_sec40: rssi for secondary 40Mhz channel
 * @rssi_sec80: rssi for secondary 80Mhz channel
 */
struct spectral_perchain_rssi_info {
	int8_t rssi_pri20;
	int8_t rssi_sec20;
	int8_t rssi_sec40;
	int8_t rssi_sec80;
};

/**
 * struct spectral_rfqual_info - RF quality info
 * @rssi_comb: combined rssi info
 * @pc_rssi_info: per chain rssi info array
 * @noise_floor: noise floor information
 */
struct spectral_rfqual_info {
	int8_t rssi_comb;
	struct spectral_perchain_rssi_info pc_rssi_info[4];
	int16_t noise_floor[4];
};

/**
 * struct spectral_search_fft_info - search fft report
 * @relpwr_db: In-band/Out-band power ratio (dB step) for this FFT
 * @num_str_bins_ib: Number of strong in-band bins
 * @base_pwr: dB offset used to convert FFT log2 bin magnitude to dBm
 * @total_gain_info: Total radio gain index at time of FFT (dB step)
 * @fft_chn_idx: Rx chain index used (internal BB chain) for this FFT
 * @avgpwr_db: In-band bins summation used for relpwr_db computation.
 * @peak_mag: Magnitude of peak bin (linear or dB).
 * @peak_inx: Index of peak magnitude bin.
 */
struct spectral_search_fft_info {
	uint32_t relpwr_db;
	uint32_t num_str_bins_ib;
	uint32_t base_pwr;
	uint32_t total_gain_info;
	uint32_t fft_chn_idx;
	uint32_t avgpwr_db;
	uint32_t peak_mag;
	int16_t  peak_inx;
};

/**
 * struct spectral_chan_info - spectral scan channel info
 * @center_freq1: center frequency 1 in MHz
 * @center_freq2: center frequency 2 in MHz, valid only for 11ACVHT 80+80 mode
 * @chan_width: channel width in MHz
 */
struct spectral_chan_info {
	uint16_t center_freq1;
	uint16_t center_freq2;
	uint8_t chan_width;
};

/**
 * struct spectral_phyerr_tlv - TLV header for spectral phyerr event header
 * @length: number of bytes following the TLV header
 * @tag: Tag ID for Search FFT Report
 * @signature: Signature for Baseband PHY generated TLV packets
 */
struct spectral_phyerr_tlv {
	u_int16_t length;
	u_int8_t tag;
	u_int8_t signature;
} __packed;

/**
 * struct spectral_phyerr_hdr - spectral scan summary report
 * @hdr_a: summary report header A
 * @hdr_b: summary report header B
 */
struct spectral_phyerr_hdr {
	uint32_t hdr_a;
	uint32_t hdr_b;
};

/**
 * struct spectral_phyerr_fft - spectral scan fft report
 * @buf: spectral scan fft report data buffer
 */
struct spectral_phyerr_fft {
	uint8_t buf[0];
};

/* Mask for time stamp from descriptor */
#define SPECTRAL_TSMASK              0xFFFFFFFF

/**
 * struct samp_msg_params - spectral scan SAMP message parameters
 * @rssi: spectral scan rssi
 * @rssi_sec80: rssi for secondary 80 segment
 * @lower_rssi: rssi of lower band
 * @upper_rssi: rssi of upper band
 * @chain_ctl_rssi: rssi for control channel, for all antennas
 * @chain_ext_rssi: rssi for extension channel, for all antennas
 * @bwinfo: bandwidth info
 * @datalen: bin size
 * @datalen_sec80: bin size for secondary 80 segment
 * @tstamp: timestamp
 * @last_tstamp: last timestamp
 * @max_mag: max magnitude
 * @max_mag_sec80: max magnitude for secondary 80 segment
 * @max_index: the index of max magnitude
 * @max_index_sec80: index of max magnitude for secondary 80 segment
 * @max_exp: max exp
 * @peak: peak magnitude
 * @pwr_count: the number of FFT bins
 * @pwr_count_sec80: the number of FFT bins in secondary 80 segment
 * @nb_lower: not used
 * @nb_upper: not used
 * @max_lower_index: the index of max mag in lower band
 * @max_upper_index: the index of max mag in upper band
 * @bin_pwr_data: contains FFT magnitudes
 * @bin_pwr_data_sec80: contains FFT magnitudes for the secondary 80 segment
 * @freq: frequency
 * @vhtop_ch_freq_seg1: VHT center frequency segment 1
 * @vhtop_ch_freq_seg2: VHT center frequency segment 2
 * @freq_loading: how busy was the channel
 * @noise_floor: the current noise floor
 * @noise_floor_sec80: current noise floor for secondary 80 segment
 * @interf_list: list of interfernce sources
 */
struct samp_msg_params {
	int8_t      rssi;
	int8_t      rssi_sec80;
	int8_t      lower_rssi;
	int8_t      upper_rssi;
	int8_t      chain_ctl_rssi[ATH_MAX_ANTENNA];
	int8_t      chain_ext_rssi[ATH_MAX_ANTENNA];

	uint16_t    bwinfo;
	uint16_t    datalen;
	uint16_t    datalen_sec80;
	uint32_t    tstamp;
	uint32_t    last_tstamp;
	uint16_t    max_mag;
	uint16_t    max_mag_sec80;
	uint16_t    max_index;
	uint16_t    max_index_sec80;
	uint8_t     max_exp;

	int         peak;
	int         pwr_count;
	int         pwr_count_sec80;

	int8_t      nb_lower;
	int8_t      nb_upper;
	uint16_t    max_lower_index;
	uint16_t    max_upper_index;

	uint8_t    **bin_pwr_data;
	uint8_t    **bin_pwr_data_sec80;
	uint16_t   freq;
	uint16_t   vhtop_ch_freq_seg1;
	uint16_t   vhtop_ch_freq_seg2;
	uint16_t   freq_loading;

	int16_t    noise_floor;
	int16_t    noise_floor_sec80;
	struct INTERF_SRC_RSP interf_list;
};

#endif
