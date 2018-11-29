/*
 * Copyright (c) 2016-2018 The Linux Foundation. All rights reserved.
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
 * DOC: cds_config.h
 *
 * Defines the configuration Information for various modules. Default values
 * are read from the INI file and saved into cds_config_info which are passed
 * to various modules for the initialization.
 */

#if !defined(__CDS_CONFIG_H)
#define __CDS_CONFIG_H

#include "cdp_txrx_cmn.h"
#include "cdp_txrx_cfg.h"

/**
 * enum cfg_sub_20_channel_width: ini values for su 20 mhz channel width
 * @WLAN_SUB_20_CH_WIDTH_5: Use 5 mhz channel width
 * @WLAN_SUB_20_CH_WIDTH_10: Use 10 mhz channel width
 */
enum cfg_sub_20_channel_width {
	WLAN_SUB_20_CH_WIDTH_NONE = 0,
	WLAN_SUB_20_CH_WIDTH_5 = 1,
	WLAN_SUB_20_CH_WIDTH_10 = 2,
};

/**
 * enum active_apf_mode - the modes active APF can operate in
 * @ACTIVE_APF_DISABLED: APF is disabled in active mode
 * @ACTIVE_APF_ENABLED: APF is enabled for all packets
 * @ACTIVE_APF_ADAPTIVE: APF is enabled for packets up to some threshold
 * @ACTIVE_APF_MODE_COUNT: The number of active APF modes
 */
enum active_apf_mode {
	ACTIVE_APF_DISABLED = 0,
	ACTIVE_APF_ENABLED,
	ACTIVE_APF_ADAPTIVE,
	ACTIVE_APF_MODE_COUNT
};

/**
 * enum cds_hang_reason - host hang/ssr reason
 * @CDS_REASON_UNSPECIFIED: Unspecified reason
 * @CDS_RX_HASH_NO_ENTRY_FOUND: No Map for the MAC entry for the received frame
 * @CDS_PEER_DELETION_TIMEDOUT: peer deletion timeout happened
 * @CDS_PEER_UNMAP_TIMEDOUT: peer unmap timeout
 * @CDS_SCAN_REQ_EXPIRED: Scan request timed out
 * @CDS_SCAN_ATTEMPT_FAILURES: Consecutive Scan attempt failures
 * @CDS_GET_MSG_BUFF_FAILURE: Unable to get the message buffer
 * @CDS_ACTIVE_LIST_TIMEOUT: Current command processing is timedout
 * @CDS_SUSPEND_TIMEOUT: Timeout for an ACK from FW for suspend request
 * @CDS_RESUME_TIMEOUT: Timeout for an ACK from FW for resume request
 */
enum cds_hang_reason {
	CDS_REASON_UNSPECIFIED = 0,
	CDS_RX_HASH_NO_ENTRY_FOUND = 1,
	CDS_PEER_DELETION_TIMEDOUT = 2,
	CDS_PEER_UNMAP_TIMEDOUT = 3,
	CDS_SCAN_REQ_EXPIRED = 4,
	CDS_SCAN_ATTEMPT_FAILURES = 5,
	CDS_GET_MSG_BUFF_FAILURE = 6,
	CDS_ACTIVE_LIST_TIMEOUT = 7,
	CDS_SUSPEND_TIMEOUT = 8,
	CDS_RESUME_TIMEOUT = 9,
};

/**
 * enum cds_auto_pwr_detect_failure_mode_t - auto detect failure modes
 * @CDS_FW_TO_CRASH_ON_PWR_FAILURE: Don't register wow wakeup event and FW
 * crashes on power failure
 * @CDS_FW_TO_SEND_WOW_IND_ON_PWR_FAILURE: Register wow wakeup event and FW
 * sends failure event to host on power failure
 * @CDS_FW_TO_REJUVENATE_ON_PWR_FAILURE: Don't register wow wakeup event and
 * FW silently rejuvenate on power failure
 * @CDS_AUTO_PWR_FAILURE_DETECT_DISABLE: Don't register wow wakeup event and the
 * auto power failure detect feature is disabled in FW.
 */
enum cds_auto_pwr_detect_failure_mode_t {
	CDS_FW_TO_CRASH_ON_PWR_FAILURE,
	CDS_FW_TO_SEND_WOW_IND_ON_PWR_FAILURE,
	CDS_FW_TO_REJUVENATE_ON_PWR_FAILURE,
	CDS_AUTO_PWR_FAILURE_DETECT_DISABLE
};

/**
 * struct cds_config_info - Place Holder for cds configuration
 * @max_station: Max station supported
 * @max_bssid: Max Bssid Supported
 * @powersave_offload_enabled: Indicate if powersave offload is enabled
 * @sta_maxlimod_dtim: station max listen interval
 * @sta_mod_dtim: station mode DTIM
 * @sta_dynamic_dtim: station dynamic DTIM
 * @driver_type: Enumeration of Driver Type whether FTM or Mission mode
 * @max_wow_filters: Max wow filters to be configured to fw
 * @wow_enable: Indicate whether wow is enabled or not
 * @ol_ini_info: Status of offload enabled from ini 1st bit for arm,2nd for NS
 * currently rest of bits are not used
 * @ssdp: Indicate ssdp is enabled or not
 * @enable_mc_list : To Check if Multicast list filtering is enabled in FW
 * @dfs_phyerr_filter_offload: DFS Phyerror Filtering offload status from ini
 * Indicates whether support is enabled or not
 * @ap_disable_intrabss_fwd: pass intra-bss-fwd info to txrx module
 * @ap_maxoffload_peers: max offload peer
 * @ap_maxoffload_reorderbuffs: max offload reorder buffs
 * @ra_ratelimit_interval: RA rate limit value
 * @is_ra_ratelimit_enabled: Indicate RA rate limit enabled or not
 * @reorder_offload: is RX re-ordering offloaded to the fw
 * @dfs_pri_multiplier: dfs radar pri multiplier
 * @uc_offload_enabled: IPA Micro controller data path offload enable flag
 * @uc_txbuf_count: IPA Micro controller data path offload TX buffer count
 * @uc_txbuf_size: IPA Micro controller data path offload TX buffer size
 * @uc_rxind_ringcount: IPA Micro controller data path offload RX indication
 * ring count
 * @uc_tx_partition_base: IPA Micro controller datapath offload TX partition
 * base
 * @enable_rxthread: Rx processing in thread from TXRX
 * @ip_tcp_udp_checksum_offload: checksum offload enabled or not
 * @ce_classify_enabled: CE based classification enabled
 * @max_scan: Maximum number of parallel scans
 * @tx_flow_stop_queue_th: Threshold to stop queue in percentage
 * @tx_flow_start_queue_offset: Start queue offset in percentage
 * @is_lpass_enabled: Indicate whether LPASS is enabled or not
 * @is_nan_enabled: Indicate whether NAN is enabled or not
 * @bool apf_packet_filter_enable; Indicate apf filter enabled or not
 * @tx_chain_mask_cck: Tx chain mask enabled or not
 * @self_gen_frm_pwr: Self gen from power
 * @sub_20_channel_width: Sub 20 MHz ch width, ini intersected with fw cap
 * @flow_steering_enabled: Receive flow steering.
 * @is_fw_timeout: Indicate whether crash host when fw timesout or not
 * @force_target_assert_enabled: Indicate whether target assert enabled or not
 * @active_uc_apf_mode: Setting that determines how APF is applied in active
 * mode for uc packets
 * @active_mc_bc_apf_mode: Setting that determines how APF is applied in
 * active mode for MC/BC packets
 * @rps_enabled: RPS enabled in SAP mode
 * @delay_before_vdev_stop: wait time for tx complete before vdev stop
 * @ito_repeat_count: Indicates ito repeated count
 * @bandcapability: Configured band by user
 * @etsi_srd_chan_in_master_mode: Use of ETSI SRD chan in SAP/P2P-GO ACS/PCL
 * @dot11p_mode: dot11p user configuration
 * Structure for holding cds ini parameters.
 */

struct cds_config_info {
	uint16_t max_station;
	uint16_t max_bssid;
	uint8_t powersave_offload_enabled;
	uint8_t sta_maxlimod_dtim;
	uint8_t sta_mod_dtim;
	uint8_t sta_dynamic_dtim;
	enum qdf_driver_type driver_type;
	uint8_t max_wow_filters;
	uint8_t wow_enable;
	uint8_t ol_ini_info;
	bool ssdp;
	bool enable_mc_list;
	uint8_t dfs_phyerr_filter_offload;
	uint8_t ap_disable_intrabss_fwd;
	uint8_t ap_maxoffload_peers;
	uint8_t ap_maxoffload_reorderbuffs;
#ifdef FEATURE_WLAN_RA_FILTERING
	uint16_t ra_ratelimit_interval;
	bool is_ra_ratelimit_enabled;
#endif
	uint8_t reorder_offload;
	int32_t dfs_pri_multiplier;
	uint8_t uc_offload_enabled;
	uint32_t uc_txbuf_count;
	uint32_t uc_txbuf_size;
	uint32_t uc_rxind_ringcount;
	uint32_t uc_tx_partition_base;
	bool enable_rxthread;
	bool ip_tcp_udp_checksum_offload;
	bool ce_classify_enabled;
	uint8_t max_scan;
#ifdef QCA_LL_TX_FLOW_CONTROL_V2
	uint32_t tx_flow_stop_queue_th;
	uint32_t tx_flow_start_queue_offset;
#endif
#ifdef WLAN_FEATURE_LPSS
	bool is_lpass_enabled;
#endif
#ifdef WLAN_FEATURE_NAN
	bool is_nan_enabled;
#endif
	bool apf_packet_filter_enable;
	bool tx_chain_mask_cck;
	uint16_t self_gen_frm_pwr;
	enum cfg_sub_20_channel_width sub_20_channel_width;
	bool flow_steering_enabled;
	uint8_t max_msdus_per_rxinorderind;
	bool self_recovery_enabled;
	bool fw_timeout_crash;

	struct ol_tx_sched_wrr_ac_specs_t ac_specs[TX_WMM_AC_NUM];

	bool force_target_assert_enabled;
	enum active_apf_mode active_uc_apf_mode;
	enum active_apf_mode active_mc_bc_apf_mode;
	bool rps_enabled;
	uint8_t delay_before_vdev_stop;
	enum cds_auto_pwr_detect_failure_mode_t auto_power_save_fail_mode;
	uint8_t ito_repeat_count;
	uint8_t bandcapability;
	bool etsi_srd_chan_in_master_mode;
	uint8_t dot11p_mode;
};

#ifdef WLAN_FEATURE_FILS_SK
#define MAX_PMK_LEN 48
#define MAX_PMKID_LEN 16
#define FILS_MAX_KEYNAME_NAI_LENGTH 253
#define FILS_MAX_REALM_LEN 255
#define FILS_MAX_RRK_LENGTH 64
#define FILS_MAX_RIK_LENGTH FILS_MAX_RRK_LENGTH

struct cds_fils_connection_info {
	bool is_fils_connection;
	uint8_t keyname_nai[FILS_MAX_KEYNAME_NAI_LENGTH];
	uint32_t key_nai_length;
	uint16_t sequence_number;
	uint8_t r_rk[FILS_MAX_RRK_LENGTH];
	uint32_t r_rk_length;
	uint8_t realm[FILS_MAX_REALM_LEN];
	uint32_t realm_len;
	uint8_t akm_type;
	uint8_t auth_type;
	uint8_t pmk[MAX_PMK_LEN];
	uint8_t pmk_len;
	uint8_t pmkid[16];
};
#endif
#endif /* !defined( __CDS_CONFIG_H ) */
