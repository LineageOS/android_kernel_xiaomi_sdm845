/*
 * Copyright (c) 2017 The Linux Foundation. All rights reserved.
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
 * DOC: spectral_scan_fmt.h
 *
 * WLAN Host Device Driver Spectral Scan Implementation
 */

#ifndef _SPECTRAL_SCAN_FMT_H_
#define _SPECTRAL_SCAN_FMT_H_

#ifndef __ATTRIB_PACK
#define __ATTRIB_PACK __packed
#endif

#define MAX_INTERF 10 /* 5 categories x (lower + upper) bands */
#define MAX_NUM_BINS 520

/**
 * struct INTERF_RSP_INFO - Interference source info
 * @interf_min_freq: Interference minimum frequency
 * @interf_max_freq: Interference maximum frequency
 * @interf_type: Interference type
 */
struct INTERF_RSP_INFO {
	uint16_t interf_min_freq;
	uint16_t interf_max_freq;
	uint8_t  interf_type;
} __ATTRIB_PACK;

/**
 * struct INTERF_SRC_RSP_INFO - Interference source info
 * @count: number of interference sources
 * @interf: interference info
 */
struct INTERF_SRC_RSP_INFO {
	uint16_t count;
	struct INTERF_RSP_INFO interf[MAX_INTERF];
} __ATTRIB_PACK;

/**
 * struct samp_msg_data_info - FFT sampling data
 * @spectral_tstamp: timestamp
 * @spectral_last_tstamp: last time stamp
 * @ch_width: channel width
 * @spectral_data_len: bin size for the sampling data
 * @spectral_data_len_sec80: bin size for secondary 80Mhz segment
 * @spectral_rssi: RSSI value
 * @spectral_rssi_sec80: RSSI value for secondary 80Mhz segment
 * @spectral_bwinfo: bandwidth info
 * @spectral_max_index: index of max magnitude
 * @spectral_max_index_sec80: index of max magnitude for secondary 80Mhz segment
 * @spectral_max_mag: maximum magnitude
 * @spectral_max_mag_sec80: maximum magnitude for secondary 80Mhz segment
 * @spectral_upper_max_index: index of max magnitude in upper band
 * @spectral_lower_max_index: index of max magnitude in lower band
 * @bin_pwr_count: number of FFT bins
 * @bin_pwr_count_sec80: number of FFT bins in secondary 80MHz segment
 * @noise_floor: current noise floor
 * @noise_floor_sec80: current noise floor in secondary 80MHz segment
 * @spectral_max_exp: maximum exp
 * @spectral_combined_rssi: combined RSSI from all antennas
 * @spectral_upper_rssi: RSSI of upper band
 * @spectral_lower_rssi: RSSI of lower band
 * @spectral_chain_ctl_rssi: RSSI for control channel, for all antennas
 * @spectral_chain_ext_rssi: RSSI for extension channel, for all antennas
 * @spectral_max_scale: scale factor
 * @spectral_nb_upper: not used
 * @spectral_nb_lower: not used
 * @lb_edge_extrabins: number of extra bins on left band edge
 * @rb_edge_extrabins: number of extra bins on right band edge
 * @bin_pwr: FFT magnitudes
 * @bin_pwr_sec80: FFT magnitudes in secondary 80MHz segment
 * @interf_list: list of interference source
 */
struct samp_msg_data_info {
	int32_t     spectral_tstamp;
	int32_t     spectral_last_tstamp;
	uint32_t    ch_width;
	int16_t     spectral_data_len;
	int16_t     spectral_data_len_sec80;
	int16_t     spectral_rssi;
	int16_t     spectral_rssi_sec80;
	int16_t     spectral_bwinfo;
	int16_t     spectral_max_index;
	int16_t     spectral_max_index_sec80;
	int16_t     spectral_max_mag;
	int16_t     spectral_max_mag_sec80;
	int16_t     spectral_upper_max_index;
	int16_t     spectral_lower_max_index;
	uint16_t    bin_pwr_count;
	uint16_t    bin_pwr_count_sec80;
	int16_t     noise_floor;
	int16_t     noise_floor_sec80;
	uint8_t     spectral_max_exp;
	int8_t      spectral_combined_rssi;
	int8_t      spectral_upper_rssi;
	int8_t      spectral_lower_rssi;
	int8_t      spectral_chain_ctl_rssi[MAX_SPECTRAL_CHAINS];
	int8_t      spectral_chain_ext_rssi[MAX_SPECTRAL_CHAINS];
	uint8_t     spectral_max_scale;
	uint8_t     spectral_nb_upper;
	uint8_t     spectral_nb_lower;
	uint8_t     lb_edge_extrabins;
	uint8_t     rb_edge_extrabins;
	uint8_t     bin_pwr[MAX_NUM_BINS];
	uint8_t     bin_pwr_sec80[MAX_NUM_BINS];
	struct      INTERF_SRC_RSP_INFO interf_list;
} __ATTRIB_PACK;

#define SPECTRAL_DCS_INT_NONE    0
#define SPECTRAL_DCS_INT_CW      1
#define SPECTRAL_DCS_INT_WIFI    2

#define SAMP_SIGNATURE 0xdeadbeef
/**
 * struct spectral_samp_msg_info - FFT sampling data
 * @signature: flag indicating it is a samp message (0xdeadbeef)
 * @freq: Operating frequency in MHz
 * @vhtop_ch_freq_seg1: VHT channel frequency segment 1
 * @vhtop_ch_freq_seg2: VHT channel frequency segment 2
 * @freq_loading: how busy was the channel
 * @dcs_enabled: if DCS is enabled
 * @int_type: DCS interface type, defined as above values 0, 1, 2
 * @macaddr: interface mac address
 * @samp_data: sampling data
 */
struct spectral_samp_msg_info {
	uint32_t      signature;
	uint16_t      freq;
	uint16_t      vhtop_ch_freq_seg1;
	uint16_t      vhtop_ch_freq_seg2;
	uint16_t      freq_loading;
	uint8_t       dcs_enabled;
	uint8_t       int_type;
	uint8_t       macaddr[6];
	struct samp_msg_data_info samp_data;
} __ATTRIB_PACK;

#endif /* _SPECTRAL_SCAN_FMT_H_ */
