/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012-2015 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/ctype.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/input/synaptics_dsx_force.h>
#include "synaptics_dsx_core.h"
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>

#define SYSFS_FOLDER_NAME "f54"

/* #define USE_RT8 */

#define GET_REPORT_TIMEOUT_S 3
#define CALIBRATION_TIMEOUT_S 10
#define COMMAND_TIMEOUT_100MS 30

#define NO_SLEEP_OFF (0 << 2)
#define NO_SLEEP_ON (1 << 2)

#define STATUS_IDLE 0
#define STATUS_BUSY 1
#define STATUS_ERROR 2

#define REPORT_INDEX_OFFSET 1
#define REPORT_DATA_OFFSET 3

#define SENSOR_RX_MAPPING_OFFSET 1
#define SENSOR_TX_MAPPING_OFFSET 2

#define COMMAND_GET_REPORT 1
#define COMMAND_FORCE_CAL 2
#define COMMAND_FORCE_UPDATE 4
#define COMMAND_ENTER_INCELL_TEST_MODE 16

#define CONTROL_NO_AUTO_CAL 1

#define CONTROL_0_SIZE 1
#define CONTROL_1_SIZE 1
#define CONTROL_2_SIZE 2
#define CONTROL_3_SIZE 1
#define CONTROL_4_6_SIZE 3
#define CONTROL_7_SIZE 1
#define CONTROL_8_9_SIZE 3
#define CONTROL_10_SIZE 1
#define CONTROL_11_SIZE 2
#define CONTROL_12_13_SIZE 2
#define CONTROL_14_SIZE 1
#define CONTROL_15_SIZE 1
#define CONTROL_16_SIZE 1
#define CONTROL_17_SIZE 1
#define CONTROL_18_SIZE 1
#define CONTROL_19_SIZE 1
#define CONTROL_20_SIZE 1
#define CONTROL_21_SIZE 2
#define CONTROL_22_26_SIZE 7
#define CONTROL_27_SIZE 1
#define CONTROL_28_SIZE 2
#define CONTROL_29_SIZE 1
#define CONTROL_30_SIZE 1
#define CONTROL_31_SIZE 1
#define CONTROL_32_35_SIZE 8
#define CONTROL_36_SIZE 1
#define CONTROL_37_SIZE 1
#define CONTROL_38_SIZE 1
#define CONTROL_39_SIZE 1
#define CONTROL_40_SIZE 1
#define CONTROL_41_SIZE 1
#define CONTROL_42_SIZE 2
#define CONTROL_43_54_SIZE 13
#define CONTROL_55_56_SIZE 2
#define CONTROL_57_SIZE 1
#define CONTROL_58_SIZE 1
#define CONTROL_59_SIZE 2
#define CONTROL_60_62_SIZE 3
#define CONTROL_63_SIZE 1
#define CONTROL_64_67_SIZE 4
#define CONTROL_68_73_SIZE 8
#define CONTROL_74_SIZE 2
#define CONTROL_75_SIZE 1
#define CONTROL_76_SIZE 1
#define CONTROL_77_78_SIZE 2
#define CONTROL_79_83_SIZE 5
#define CONTROL_84_85_SIZE 2
#define CONTROL_86_SIZE 1
#define CONTROL_87_SIZE 1
#define CONTROL_88_SIZE 1
#define CONTROL_89_SIZE 1
#define CONTROL_90_SIZE 1
#define CONTROL_91_SIZE 1
#define CONTROL_92_SIZE 1
#define CONTROL_93_SIZE 1
#define CONTROL_94_SIZE 1
#define CONTROL_95_SIZE 1
#define CONTROL_96_SIZE 1
#define CONTROL_97_SIZE 1
#define CONTROL_98_SIZE 1
#define CONTROL_99_SIZE 1
#define CONTROL_100_SIZE 1
#define CONTROL_101_SIZE 1
#define CONTROL_102_SIZE 1
#define CONTROL_103_SIZE 1
#define CONTROL_104_SIZE 1
#define CONTROL_105_SIZE 1
#define CONTROL_106_SIZE 1
#define CONTROL_107_SIZE 1
#define CONTROL_108_SIZE 1
#define CONTROL_109_SIZE 1
#define CONTROL_110_SIZE 1
#define CONTROL_111_SIZE 1
#define CONTROL_112_SIZE 1
#define CONTROL_113_SIZE 1
#define CONTROL_114_SIZE 1
#define CONTROL_115_SIZE 1
#define CONTROL_116_SIZE 1
#define CONTROL_117_SIZE 1
#define CONTROL_118_SIZE 1
#define CONTROL_119_SIZE 1
#define CONTROL_120_SIZE 1
#define CONTROL_121_SIZE 1
#define CONTROL_122_SIZE 1
#define CONTROL_123_SIZE 1
#define CONTROL_124_SIZE 1
#define CONTROL_125_SIZE 1
#define CONTROL_126_SIZE 1
#define CONTROL_127_SIZE 1
#define CONTROL_128_SIZE 1
#define CONTROL_129_SIZE 1
#define CONTROL_130_SIZE 1
#define CONTROL_131_SIZE 1
#define CONTROL_132_SIZE 1
#define CONTROL_133_SIZE 1
#define CONTROL_134_SIZE 1
#define CONTROL_135_SIZE 1
#define CONTROL_136_SIZE 1
#define CONTROL_137_SIZE 1
#define CONTROL_138_SIZE 1
#define CONTROL_139_SIZE 1
#define CONTROL_140_SIZE 1
#define CONTROL_141_SIZE 1
#define CONTROL_142_SIZE 1
#define CONTROL_143_SIZE 1
#define CONTROL_144_SIZE 1
#define CONTROL_145_SIZE 1
#define CONTROL_146_SIZE 1
#define CONTROL_147_SIZE 1
#define CONTROL_148_SIZE 1
#define CONTROL_149_SIZE 1
#define CONTROL_150_SIZE 1
#define CONTROL_151_SIZE 1
#define CONTROL_152_SIZE 1
#define CONTROL_153_SIZE 1
#define CONTROL_154_SIZE 1
#define CONTROL_155_SIZE 1
#define CONTROL_156_SIZE 1
#define CONTROL_157_158_SIZE 2
#define CONTROL_163_SIZE 1
#define CONTROL_164_SIZE 1
#define CONTROL_165_SIZE 1
#define CONTROL_166_SIZE 1
#define CONTROL_167_SIZE 1
#define CONTROL_168_SIZE 1
#define CONTROL_169_SIZE 1
#define CONTROL_171_SIZE 1
#define CONTROL_172_SIZE 1
#define CONTROL_173_SIZE 1
#define CONTROL_174_SIZE 1
#define CONTROL_175_SIZE 1
#define CONTROL_176_SIZE 1
#define CONTROL_177_178_SIZE 2
#define CONTROL_179_SIZE 1
#define CONTROL_182_SIZE 1
#define CONTROL_183_SIZE 1
#define CONTROL_185_SIZE 1
#define CONTROL_186_SIZE 1
#define CONTROL_187_SIZE 1
#define CONTROL_188_SIZE 1
#define CONTROL_189_SIZE 1
#define CONTROL_190_SIZE 1
#define CONTROL_191_SIZE 1
#define CONTROL_192_SIZE 1
#define CONTROL_193_SIZE 1
#define CONTROL_194_SIZE 1
#define CONTROL_195_SIZE 1
#define CONTROL_196_SIZE 1
#define CONTROL_197_SIZE 1
#define CONTROL_198_SIZE 1
#define CONTROL_199_SIZE 1
#define CONTROL_200_SIZE 1
#define CONTROL_201_SIZE 1
#define CONTROL_202_SIZE 1
#define CONTROL_203_SIZE 1
#define CONTROL_204_SIZE 1
#define CONTROL_205_SIZE 1
#define CONTROL_206_SIZE 1
#define CONTROL_207_SIZE 1
#define CONTROL_208_SIZE 1
#define CONTROL_209_SIZE 1
#define CONTROL_210_SIZE 1
#define CONTROL_211_SIZE 1
#define CONTROL_212_SIZE 1
#define CONTROL_213_SIZE 1
#define CONTROL_214_SIZE 1
#define CONTROL_215_SIZE 1
#define CONTROL_216_SIZE 1
#define CONTROL_217_SIZE 1
#define CONTROL_218_SIZE 1
#define CONTROL_219_SIZE 1
#define CONTROL_220_SIZE 1
#define CONTROL_221_SIZE 1
#define CONTROL_222_SIZE 1
#define CONTROL_223_SIZE 1
#define CONTROL_224_SIZE 1
#define CONTROL_225_SIZE 1

#define HIGH_RESISTANCE_DATA_SIZE 6
#define FULL_RAW_CAP_MIN_MAX_DATA_SIZE 4
#define TRX_OPEN_SHORT_DATA_SIZE 7


#define F54_POLLING_GET_REPORT
static char *factory_string[] = {
	"tddi_short_limit_b",
	"tddi_noise_limit",
	"tddi_extend_ee_short_reset_dur",
	"tddi_extend_ee_short_int_dur",
	"tddi_extend_ee_short_tx_on_count",
	"tddi_extend_ee_short_rx_on_count",
	"tddi_extend_ee_short_test_limit_part1",
	"tddi_extend_ee_short_test_limit_part2",
	"tddi_open_test_int_dur_one",
	"tddi_open_test_int_dur_two",
	"tddi_open_test_limit_phase2_lower",
	"tddi_b7_open_test_int_dur_one",
	"tddi_b7_open_test_int_dur_two",
	"tddi_b7_open_test_limit_phase2_lower",
	"tddi_b7_open_test_limit_phase2_upper",
	"button_count",
	"abs_0d_open_factor",
	"abs_0d_open_test_limit",
	"elec_open_test_tx_on_count",
	"elec_open_test_rx_on_count",
	"elec_open_int_dur_one",
	"elec_open_int_dur_two",
	"elec_open_test_limit_one",
	"elec_open_test_limit_two",
	NULL,
	};

#if 0
/* test limit config */
#define TDDI_SHORT_LIMIT_B			150
#define TDDI_NOISE_LIMIT			28

/* #define ENABLE_EXTEND_EE_SHORT */
#define TDDI_EXTEND_EE_SHORT_RESET_DUR  60
#define TDDI_EXTEND_EE_SHORT_INT_DUR  150
#define TDDI_EXTEND_EE_SHORT_TX_ON_COUNT  146
#define TDDI_EXTEND_EE_SHORT_RX_ON_COUNT  146
#define TDDI_EXTEND_EE_SHORT_TEST_LIMIT_PART1  100
#define TDDI_EXTEND_EE_SHORT_TEST_LIMIT_PART2  96

#define TDDI_OPEN_TEST_INT_DUR_ONE		145
#define TDDI_OPEN_TEST_INT_DUR_TWO		15
#define TDDI_OPEN_TEST_LIMIT_PHASE2_LOWER	50

/* #define ENABLE_AMP_OPEN_B7 */
#define TDDI_B7_OPEN_TEST_INT_DUR_ONE		23
#define TDDI_B7_OPEN_TEST_INT_DUR_TWO		27
#define TDDI_B7_OPEN_TEST_LIMIT_PHASE2_LOWER	0
#define TDDI_B7_OPEN_TEST_LIMIT_PHASE2_UPPER   115

#define BUTTON_COUNT 3
#define ABS_0D_OPEN_FACTOR 8
#define ABS_0D_OPEN_TEST_LIMIT 30

#define ELEC_OPEN_TEST_TX_ON_COUNT 2
#define ELEC_OPEN_TEST_RX_ON_COUNT 2
#define ELEC_OPEN_INT_DUR_ONE 4
#define ELEC_OPEN_INT_DUR_TWO 15
#define ELEC_OPEN_TEST_LIMIT_ONE 500
#define ELEC_OPEN_TEST_LIMIT_TWO 80
/* test limit config - */
#endif

#define TEST_INVALID	0
#define TEST_FAILED	1
#define TEST_OK		2

#define TEST_TYPE_20_333X	"20"
#define TEST_TYPE_25_333X	"25"
#define TEST_TYPE_26_333X	"26"
#define TEST_TYPE_02		"2"
#define TEST_TYPE_03		"3"

#define CHIP_ID_OFFSET	11

#define concat(a, b) a##b

#define attrify(propname) (&dev_attr_##propname.attr)

#define show_prototype(propname)\
static ssize_t concat(test_sysfs, _##propname##_show)(\
		struct device *dev,\
		struct device_attribute *attr,\
		char *buf);\
\
static struct device_attribute dev_attr_##propname =\
		__ATTR(propname, S_IRUGO,\
		concat(test_sysfs, _##propname##_show),\
		synaptics_rmi4_store_error);

#define store_prototype(propname)\
static ssize_t concat(test_sysfs, _##propname##_store)(\
		struct device *dev,\
		struct device_attribute *attr,\
		const char *buf, size_t count);\
\
static struct device_attribute dev_attr_##propname =\
		__ATTR(propname, (S_IWUSR | S_IWGRP),\
		synaptics_rmi4_show_error,\
		concat(test_sysfs, _##propname##_store));

#define show_store_prototype(propname)\
static ssize_t concat(test_sysfs, _##propname##_show)(\
		struct device *dev,\
		struct device_attribute *attr,\
		char *buf);\
\
static ssize_t concat(test_sysfs, _##propname##_store)(\
		struct device *dev,\
		struct device_attribute *attr,\
		const char *buf, size_t count);\
\
static struct device_attribute dev_attr_##propname =\
		__ATTR(propname, (S_IRUGO | S_IWUSR | S_IWGRP),\
		concat(test_sysfs, _##propname##_show),\
		concat(test_sysfs, _##propname##_store));

#define disable_cbc(ctrl_num)\
do {\
	retval = synaptics_rmi4_reg_read(rmi4_data,\
			f54->control.ctrl_num->address,\
			f54->control.ctrl_num->data,\
			sizeof(f54->control.ctrl_num->data));\
	if (retval < 0) {\
		dev_err(rmi4_data->pdev->dev.parent,\
				"%s: Failed to disable CBC (" #ctrl_num ")\n",\
				__func__);\
		return retval;\
	} \
	f54->control.ctrl_num->cbc_tx_carrier_selection = 0;\
	retval = synaptics_rmi4_reg_write(rmi4_data,\
			f54->control.ctrl_num->address,\
			f54->control.ctrl_num->data,\
			sizeof(f54->control.ctrl_num->data));\
	if (retval < 0) {\
		dev_err(rmi4_data->pdev->dev.parent,\
				"%s: Failed to disable CBC (" #ctrl_num ")\n",\
				__func__);\
		return retval;\
	} \
} while (0)

enum f54_report_types {
	F54_8BIT_IMAGE = 1,
	F54_16BIT_IMAGE = 2,
	F54_RAW_16BIT_IMAGE = 3,
	F54_HIGH_RESISTANCE = 4,
	F54_TX_TO_TX_SHORTS = 5,
	F54_RX_TO_RX_SHORTS_1 = 7,
	F54_TRUE_BASELINE = 9,
	F54_FULL_RAW_CAP_MIN_MAX = 13,
	F54_RX_OPENS_1 = 14,
	F54_TX_OPENS = 15,
	F54_TX_TO_GND_SHORTS = 16,
	F54_RX_TO_RX_SHORTS_2 = 17,
	F54_RX_OPENS_2 = 18,
	F54_FULL_RAW_CAP = 19,
#ifdef USE_RT8
	F54_FULL_RAW_CAP_NO_RX_COUPLING = 8,
#else
	F54_FULL_RAW_CAP_NO_RX_COUPLING = 20,
#endif
	F54_SENSOR_SPEED = 22,
	F54_ADC_RANGE = 23,
	F54_TRX_OPENS = 24,
	F54_TRX_TO_GND_SHORTS = 25,
	F54_TRX_SHORTS = 26,
	F54_ABS_RAW_CAP = 38,
	F54_ABS_DELTA_CAP = 40,
	F54_ABS_HYBRID_DELTA_CAP = 59,
	F54_ABS_HYBRID_RAW_CAP = 63,
	F54_AMP_FULL_RAW_CAP = 78,
	F54_AMP_RAW_ADC = 83,
	/* td43xx start */
	F54_TD43XX_FULL_RAW = 92,
	F54_TD43XX_NOISE = 94,
	F54_TD43XX_EE_SHORT = 95,
	/* td43xx end */
	F54_ABS_DOZE_NO_CBC_INCELL = 121,
	INVALID_REPORT_TYPE = -1,
};

enum f54_afe_cal {
	F54_AFE_CAL,
	F54_AFE_IS_CAL,
};

struct f54_query {
	union {
		struct {
			/* query 0 */
			unsigned char num_of_rx_electrodes;

			/* query 1 */
			unsigned char num_of_tx_electrodes;

			/* query 2 */
			unsigned char f54_query2_b0__1:2;
			unsigned char has_baseline:1;
			unsigned char has_image8:1;
			unsigned char f54_query2_b4__5:2;
			unsigned char has_image16:1;
			unsigned char f54_query2_b7:1;

			/* queries 3.0 and 3.1 */
			unsigned short clock_rate;

			/* query 4 */
			unsigned char touch_controller_family;

			/* query 5 */
			unsigned char has_pixel_touch_threshold_adjustment:1;
			unsigned char f54_query5_b1__7:7;

			/* query 6 */
			unsigned char has_sensor_assignment:1;
			unsigned char has_interference_metric:1;
			unsigned char has_sense_frequency_control:1;
			unsigned char has_firmware_noise_mitigation:1;
			unsigned char has_ctrl11:1;
			unsigned char has_two_byte_report_rate:1;
			unsigned char has_one_byte_report_rate:1;
			unsigned char has_relaxation_control:1;

			/* query 7 */
			unsigned char curve_compensation_mode:2;
			unsigned char f54_query7_b2__7:6;

			/* query 8 */
			unsigned char f54_query8_b0:1;
			unsigned char has_iir_filter:1;
			unsigned char has_cmn_removal:1;
			unsigned char has_cmn_maximum:1;
			unsigned char has_touch_hysteresis:1;
			unsigned char has_edge_compensation:1;
			unsigned char has_per_frequency_noise_control:1;
			unsigned char has_enhanced_stretch:1;

			/* query 9 */
			unsigned char has_force_fast_relaxation:1;
			unsigned char has_multi_metric_state_machine:1;
			unsigned char has_signal_clarity:1;
			unsigned char has_variance_metric:1;
			unsigned char has_0d_relaxation_control:1;
			unsigned char has_0d_acquisition_control:1;
			unsigned char has_status:1;
			unsigned char has_slew_metric:1;

			/* query 10 */
			unsigned char has_h_blank:1;
			unsigned char has_v_blank:1;
			unsigned char has_long_h_blank:1;
			unsigned char has_startup_fast_relaxation:1;
			unsigned char has_esd_control:1;
			unsigned char has_noise_mitigation2:1;
			unsigned char has_noise_state:1;
			unsigned char has_energy_ratio_relaxation:1;

			/* query 11 */
			unsigned char has_excessive_noise_reporting:1;
			unsigned char has_slew_option:1;
			unsigned char has_two_overhead_bursts:1;
			unsigned char has_query13:1;
			unsigned char has_one_overhead_burst:1;
			unsigned char f54_query11_b5:1;
			unsigned char has_ctrl88:1;
			unsigned char has_query15:1;

			/* query 12 */
			unsigned char number_of_sensing_frequencies:4;
			unsigned char f54_query12_b4__7:4;
		} __packed;
		unsigned char data[14];
	};
};

struct f54_query_13 {
	union {
		struct {
			unsigned char has_ctrl86:1;
			unsigned char has_ctrl87:1;
			unsigned char has_ctrl87_sub0:1;
			unsigned char has_ctrl87_sub1:1;
			unsigned char has_ctrl87_sub2:1;
			unsigned char has_cidim:1;
			unsigned char has_noise_mitigation_enhancement:1;
			unsigned char has_rail_im:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_15 {
	union {
		struct {
			unsigned char has_ctrl90:1;
			unsigned char has_transmit_strength:1;
			unsigned char has_ctrl87_sub3:1;
			unsigned char has_query16:1;
			unsigned char has_query20:1;
			unsigned char has_query21:1;
			unsigned char has_query22:1;
			unsigned char has_query25:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_16 {
	union {
		struct {
			unsigned char has_query17:1;
			unsigned char has_data17:1;
			unsigned char has_ctrl92:1;
			unsigned char has_ctrl93:1;
			unsigned char has_ctrl94_query18:1;
			unsigned char has_ctrl95_query19:1;
			unsigned char has_ctrl99:1;
			unsigned char has_ctrl100:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_21 {
	union {
		struct {
			unsigned char has_abs_rx:1;
			unsigned char has_abs_tx:1;
			unsigned char has_ctrl91:1;
			unsigned char has_ctrl96:1;
			unsigned char has_ctrl97:1;
			unsigned char has_ctrl98:1;
			unsigned char has_data19:1;
			unsigned char has_query24_data18:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_22 {
	union {
		struct {
			unsigned char has_packed_image:1;
			unsigned char has_ctrl101:1;
			unsigned char has_dynamic_sense_display_ratio:1;
			unsigned char has_query23:1;
			unsigned char has_ctrl103_query26:1;
			unsigned char has_ctrl104:1;
			unsigned char has_ctrl105:1;
			unsigned char has_query28:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_23 {
	union {
		struct {
			unsigned char has_ctrl102:1;
			unsigned char has_ctrl102_sub1:1;
			unsigned char has_ctrl102_sub2:1;
			unsigned char has_ctrl102_sub4:1;
			unsigned char has_ctrl102_sub5:1;
			unsigned char has_ctrl102_sub9:1;
			unsigned char has_ctrl102_sub10:1;
			unsigned char has_ctrl102_sub11:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_25 {
	union {
		struct {
			unsigned char has_ctrl106:1;
			unsigned char has_ctrl102_sub12:1;
			unsigned char has_ctrl107:1;
			unsigned char has_ctrl108:1;
			unsigned char has_ctrl109:1;
			unsigned char has_data20:1;
			unsigned char f54_query25_b6:1;
			unsigned char has_query27:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_27 {
	union {
		struct {
			unsigned char has_ctrl110:1;
			unsigned char has_data21:1;
			unsigned char has_ctrl111:1;
			unsigned char has_ctrl112:1;
			unsigned char has_ctrl113:1;
			unsigned char has_data22:1;
			unsigned char has_ctrl114:1;
			unsigned char has_query29:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_29 {
	union {
		struct {
			unsigned char has_ctrl115:1;
			unsigned char has_ground_ring_options:1;
			unsigned char has_lost_bursts_tuning:1;
			unsigned char has_aux_exvcom2_select:1;
			unsigned char has_ctrl116:1;
			unsigned char has_data23:1;
			unsigned char has_ctrl117:1;
			unsigned char has_query30:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_30 {
	union {
		struct {
			unsigned char has_ctrl118:1;
			unsigned char has_ctrl119:1;
			unsigned char has_ctrl120:1;
			unsigned char has_ctrl121:1;
			unsigned char has_ctrl122_query31:1;
			unsigned char has_ctrl123:1;
			unsigned char has_ctrl124:1;
			unsigned char has_query32:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_32 {
	union {
		struct {
			unsigned char has_ctrl125:1;
			unsigned char has_ctrl126:1;
			unsigned char has_ctrl127:1;
			unsigned char has_abs_charge_pump_disable:1;
			unsigned char has_query33:1;
			unsigned char has_data24:1;
			unsigned char has_query34:1;
			unsigned char has_query35:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_33 {
	union {
		struct {
			unsigned char has_ctrl128:1;
			unsigned char has_ctrl129:1;
			unsigned char has_ctrl130:1;
			unsigned char has_ctrl131:1;
			unsigned char has_ctrl132:1;
			unsigned char has_ctrl133:1;
			unsigned char has_ctrl134:1;
			unsigned char has_query36:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_35 {
	union {
		struct {
			unsigned char has_data25:1;
			unsigned char has_ctrl135:1;
			unsigned char has_ctrl136:1;
			unsigned char has_ctrl137:1;
			unsigned char has_ctrl138:1;
			unsigned char has_ctrl139:1;
			unsigned char has_data26:1;
			unsigned char has_ctrl140:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_36 {
	union {
		struct {
			unsigned char has_ctrl141:1;
			unsigned char has_ctrl142:1;
			unsigned char has_query37:1;
			unsigned char has_ctrl143:1;
			unsigned char has_ctrl144:1;
			unsigned char has_ctrl145:1;
			unsigned char has_ctrl146:1;
			unsigned char has_query38:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_38 {
	union {
		struct {
			unsigned char has_ctrl147:1;
			unsigned char has_ctrl148:1;
			unsigned char has_ctrl149:1;
			unsigned char has_ctrl150:1;
			unsigned char has_ctrl151:1;
			unsigned char has_ctrl152:1;
			unsigned char has_ctrl153:1;
			unsigned char has_query39:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_39 {
	union {
		struct {
			unsigned char has_ctrl154:1;
			unsigned char has_ctrl155:1;
			unsigned char has_ctrl156:1;
			unsigned char has_ctrl160:1;
			unsigned char has_ctrl157_ctrl158:1;
			unsigned char f54_query39_b5__6:2;
			unsigned char has_query40:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_40 {
	union {
		struct {
			unsigned char has_ctrl169:1;
			unsigned char has_ctrl163_query41:1;
			unsigned char has_ctrl164:1;
			unsigned char has_ctrl165_query42:1;
			unsigned char has_ctrl166:1;
			unsigned char has_ctrl167:1;
			unsigned char has_ctrl168:1;
			unsigned char has_query43:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_43 {
	union {
		struct {
			unsigned char f54_query43_b0__1:2;
			unsigned char has_ctrl171:1;
			unsigned char has_ctrl172_query44_query45:1;
			unsigned char has_ctrl173:1;
			unsigned char has_ctrl174:1;
			unsigned char has_ctrl175:1;
			unsigned char has_query46:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_46 {
	union {
		struct {
			unsigned char has_ctrl176:1;
			unsigned char has_ctrl177_ctrl178:1;
			unsigned char has_ctrl179:1;
			unsigned char f54_query46_b3:1;
			unsigned char has_data27:1;
			unsigned char has_data28:1;
			unsigned char f54_query46_b6:1;
			unsigned char has_query47:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_47 {
	union {
		struct {
			unsigned char f54_query47_b0:1;
			unsigned char has_ctrl182:1;
			unsigned char has_ctrl183:1;
			unsigned char f54_query47_b3:1;
			unsigned char has_ctrl185:1;
			unsigned char has_ctrl186:1;
			unsigned char has_ctrl187:1;
			unsigned char has_query49:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_49 {
	union {
		struct {
			unsigned char f54_query49_b0__1:2;
			unsigned char has_ctrl188:1;
			unsigned char has_data31:1;
			unsigned char has_data32:1;
			unsigned char has_ctrl189:1;
			unsigned char has_ctrl190:1;
			unsigned char has_query50:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_50 {
	union {
		struct {
			unsigned char has_ctrl191:1;
			unsigned char has_ctrl192:1;
			unsigned char f54_query50_b2__4:3;
			unsigned char has_ctrl194:1;
			unsigned char has_ctrl195:1;
			unsigned char has_query51:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_51 {
	union {
		struct {
			unsigned char f54_query51_b0:1;
			unsigned char has_ctrl196:1;
			unsigned char f54_query51_b2:1;
			unsigned char f54_query51_b3:1;
			unsigned char f54_query51_b4:1;
			unsigned char has_query53_query54_ctrl198:1;
			unsigned char has_ctrl199:1;
			unsigned char has_query55:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_55 {
	union {
		struct {
			unsigned char has_query56:1;
			unsigned char has_data33_data34:1;
			unsigned char has_alternate_report_rate:1;
			unsigned char has_ctrl200:1;
			unsigned char has_ctrl201_ctrl202:1;
			unsigned char has_ctrl203:1;
			unsigned char has_ctrl204:1;
			unsigned char has_query57:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_57 {
	union {
		struct {
			unsigned char has_ctrl205:1;
			unsigned char has_ctrl206:1;
			unsigned char has_usb_bulk_read:1;
			unsigned char has_ctrl207:1;
			unsigned char has_ctrl208:1;
			unsigned char has_ctrl209:1;
			unsigned char has_ctrl210:1;
			unsigned char has_query58:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_58 {
	union {
		struct {
			unsigned char has_query59:1;
			unsigned char has_query60:1;
			unsigned char has_ctrl211:1;
			unsigned char has_ctrl212:1;
			unsigned char has_hybrid_abs_tx_axis_filtering:1;
			unsigned char f54_query58_b5:1;
			unsigned char has_ctrl213:1;
			unsigned char has_query61:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_61 {
	union {
		struct {
			unsigned char has_ctrl214:1;
			unsigned char has_ctrl215_query62_query63:1;
			unsigned char f54_query61_b2__4:3;
			unsigned char has_ctrl218:1;
			unsigned char has_hybrid_abs_buttons:1;
			unsigned char has_query64:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_62 {
	union {
		struct {
			unsigned char size_of_ctrl215_sub1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_63 {
	union {
		struct {
			unsigned char size_of_ctrl215_sub2;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_64 {
	union {
		struct {
			unsigned char f54_query64_b0:1;
			unsigned char has_ctrl220:1;
			unsigned char f54_query64_b2:1;
			unsigned char has_ctrl222:1;
			unsigned char has_ctrl219_sub1:1;
			unsigned char has_ctrl103_sub3:1;
			unsigned char has_ctrl224_ctrl226_ctrl227:1;
			unsigned char has_query65:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_65 {
	union {
		struct {
			unsigned char has_ctrl225:1;
			unsigned char has_query66_ctrl231:1;
			unsigned char has_ctrl232:1;
			unsigned char has_query67:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_67 {
	union {
		struct {
			unsigned char has_abs_doze_spatial_filter_enable:1;
			unsigned char has_abs_doze_average_filter_enable:1;
			unsigned char has_single_display_pulse:1;
			unsigned char f54_query67_b3__6:4;
			unsigned char has_query68:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_68 {
	union {
		struct {
			unsigned char f54_query68_b0__4:5;
			unsigned char has_freq_filter_bw_ext:1;
			unsigned char f54_query68_b6:1;
			unsigned char has_query69:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_query_69 {
	union {
		struct {
			unsigned char has_ctrl240_sub0:1;
			unsigned char has_ctrl240_sub1_sub2:1;
			unsigned char has_ctrl240_sub3:1;
			unsigned char has_ctrl240_sub4:1;
			unsigned char burst_mode_report_type_enabled:1;
			unsigned char f54_query69_b5__7:3;
		} __packed;
		unsigned char data[1];
	};
};

struct f54_data_31 {
	union {
		struct {
			unsigned char is_calibration_crc:1;
			unsigned char calibration_crc:1;
			unsigned char short_test_row_number:5;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_7 {
	union {
		struct {
			unsigned char cbc_cap:3;
			unsigned char cbc_polarity:1;
			unsigned char cbc_tx_carrier_selection:1;
			unsigned char f54_ctrl7_b5__7:3;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_41 {
	union {
		struct {
			unsigned char no_signal_clarity:1;
			unsigned char f54_ctrl41_b1__7:7;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_57 {
	union {
		struct {
			unsigned char cbc_cap:3;
			unsigned char cbc_polarity:1;
			unsigned char cbc_tx_carrier_selection:1;
			unsigned char f54_ctrl57_b5__7:3;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_86 {
	union {
		struct {
			unsigned char enable_high_noise_state:1;
			unsigned char dynamic_sense_display_ratio:2;
			unsigned char f54_ctrl86_b3__7:5;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_88 {
	union {
		struct {
			unsigned char tx_low_reference_polarity:1;
			unsigned char tx_high_reference_polarity:1;
			unsigned char abs_low_reference_polarity:1;
			unsigned char abs_polarity:1;
			unsigned char cbc_polarity:1;
			unsigned char cbc_tx_carrier_selection:1;
			unsigned char charge_pump_enable:1;
			unsigned char cbc_abs_auto_servo:1;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

/* td43xx start */
struct f54_control_91 {
	union {
		struct {
			unsigned char reflo_transcap_capacitance;
			unsigned char refhi_transcap_capacitance;
			unsigned char receiver_feedback_capacitance;
			unsigned char reference_receiver_feedback_capacitance;
			unsigned char gain_ctrl;
		} __packed;
		struct {
			unsigned char data[5];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_96 {
	union {
		struct {
			unsigned char cbc_transcap[64];
		} __packed;
		struct {
			unsigned char data[64];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_99 {
	union {
		struct {
			unsigned char integration_duration_lsb;
			unsigned char integration_duration_msb;
			unsigned char reset_duration;
		} __packed;
		struct {
			unsigned char data[3];
			unsigned short address;
		} __packed;
	};
};
/* td43xx end */

struct f54_control_110 {
	union {
		struct {
			unsigned char active_stylus_rx_feedback_cap;
			unsigned char active_stylus_rx_feedback_cap_reference;
			unsigned char active_stylus_low_reference;
			unsigned char active_stylus_high_reference;
			unsigned char active_stylus_gain_control;
			unsigned char active_stylus_gain_control_reference;
			unsigned char active_stylus_timing_mode;
			unsigned char active_stylus_discovery_bursts;
			unsigned char active_stylus_detection_bursts;
			unsigned char active_stylus_discovery_noise_multiplier;
			unsigned char active_stylus_detection_envelope_min;
			unsigned char active_stylus_detection_envelope_max;
			unsigned char active_stylus_lose_count;
		} __packed;
		struct {
			unsigned char data[13];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_149 {
	union {
		struct {
			unsigned char trans_cbc_global_cap_enable:1;
			unsigned char f54_ctrl149_b1__7:7;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_182 {
	union {
		struct {
			unsigned char cbc_timing_ctrl_tx_lsb;
			unsigned char cbc_timing_ctrl_tx_msb;
			unsigned char cbc_timing_ctrl_rx_lsb;
			unsigned char cbc_timing_ctrl_rx_msb;
		} __packed;
		struct {
			unsigned char data[4];
			unsigned short address;
		} __packed;
	};
};
struct f54_control_188 {
	union {
		struct {
			unsigned char start_calibration:1;
			unsigned char start_is_calibration:1;
			unsigned char frequency:2;
			unsigned char start_production_test:1;
			unsigned char short_test_calibration:1;
			unsigned char f54_ctrl188_b7:1;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_223 {
	union {
		struct {
			unsigned char voltages_for_0d:8;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};

struct f54_control_225 {
	union {
		struct {
			unsigned char force_doze_mode:1;
			unsigned char f54_ctrl225_b1__7:7;
		} __packed;
		struct {
			unsigned char data[1];
			unsigned short address;
		} __packed;
	};
};


struct f54_control {
	struct f54_control_7 *reg_7;
	struct f54_control_41 *reg_41;
	struct f54_control_57 *reg_57;
	struct f54_control_86 *reg_86;
	struct f54_control_88 *reg_88;
	struct f54_control_91 *reg_91;
	struct f54_control_96 *reg_96;
	struct f54_control_99 *reg_99;
	struct f54_control_110 *reg_110;
	struct f54_control_149 *reg_149;
	struct f54_control_182 *reg_182;
	struct f54_control_188 *reg_188;
	struct f54_control_223 *reg_223;
	struct f54_control_225 *reg_225;
};

struct synaptics_rmi4_f54_handle {
	bool no_auto_cal;
	bool skip_preparation;
	char *data;
	unsigned char status;
	unsigned char intr_mask;
	unsigned char intr_reg_num;
	unsigned char tx_assigned;
	unsigned char rx_assigned;
	/* td43xx start */
	unsigned char swap_sensor_side;
	unsigned char left_mux_size;
	unsigned char right_mux_size;
	/* td43xx end */
	unsigned char *report_data;
	unsigned short query_base_addr;
	unsigned short control_base_addr;
	unsigned short data_base_addr;
	unsigned short command_base_addr;
	unsigned short fifoindex;
	unsigned int report_size;
	unsigned int data_buffer_size;
	unsigned int data_pos;
	int result_type;
	enum f54_report_types report_type;
	struct f54_query query;
	struct f54_query_13 query_13;
	struct f54_query_15 query_15;
	struct f54_query_16 query_16;
	struct f54_query_21 query_21;
	struct f54_query_22 query_22;
	struct f54_query_23 query_23;
	struct f54_query_25 query_25;
	struct f54_query_27 query_27;
	struct f54_query_29 query_29;
	struct f54_query_30 query_30;
	struct f54_query_32 query_32;
	struct f54_query_33 query_33;
	struct f54_query_35 query_35;
	struct f54_query_36 query_36;
	struct f54_query_38 query_38;
	struct f54_query_39 query_39;
	struct f54_query_40 query_40;
	struct f54_query_43 query_43;
	struct f54_query_46 query_46;
	struct f54_query_47 query_47;
	struct f54_query_49 query_49;
	struct f54_query_50 query_50;
	struct f54_query_51 query_51;
	struct f54_query_55 query_55;
	struct f54_query_57 query_57;
	struct f54_query_58 query_58;
	struct f54_query_61 query_61;
	struct f54_query_62 query_62;
	struct f54_query_63 query_63;
	struct f54_query_64 query_64;
	struct f54_query_65 query_65;
	struct f54_query_67 query_67;
	struct f54_query_68 query_68;
	struct f54_query_69 query_69;
	struct f54_data_31 data_31;
	struct f54_control control;
	struct mutex status_mutex;
	struct kobject *sysfs_dir;
	struct hrtimer watchdog;
	struct work_struct timeout_work;
	struct work_struct test_report_work;
	struct work_struct resume_touch_work;
	struct workqueue_struct *test_report_workqueue;
	struct synaptics_rmi4_data *rmi4_data;
};

struct f55_query {
	union {
		struct {
			/* query 0 */
			unsigned char num_of_rx_electrodes;

			/* query 1 */
			unsigned char num_of_tx_electrodes;

			/* query 2 */
			unsigned char has_sensor_assignment:1;
			unsigned char has_edge_compensation:1;
			unsigned char curve_compensation_mode:2;
			unsigned char has_ctrl6:1;
			unsigned char has_alternate_transmitter_assignment:1;
			unsigned char has_single_layer_multi_touch:1;
			unsigned char has_query5:1;
		} __packed;
		unsigned char data[3];
	};
};

struct f55_query_3 {
	union {
		struct {
			unsigned char has_ctrl8:1;
			unsigned char has_ctrl9:1;
			unsigned char has_oncell_pattern_support:1;
			unsigned char has_data0:1;
			unsigned char has_single_wide_pattern_support:1;
			unsigned char has_mirrored_tx_pattern_support:1;
			unsigned char has_discrete_pattern_support:1;
			unsigned char has_query9:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f55_query_5 {
	union {
		struct {
			unsigned char has_corner_compensation:1;
			unsigned char has_ctrl12:1;
			unsigned char has_trx_configuration:1;
			unsigned char has_ctrl13:1;
			unsigned char f55_query5_b4:1;
			unsigned char has_ctrl14:1;
			unsigned char has_basis_function:1;
			unsigned char has_query17:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f55_query_17 {
	union {
		struct {
			unsigned char f55_query17_b0:1;
			unsigned char has_ctrl16:1;
			unsigned char has_ctrl18_ctrl19:1;
			unsigned char has_ctrl17:1;
			unsigned char has_ctrl20:1;
			unsigned char has_ctrl21:1;
			unsigned char has_ctrl22:1;
			unsigned char has_query18:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f55_query_18 {
	union {
		struct {
			unsigned char has_ctrl23:1;
			unsigned char has_ctrl24:1;
			unsigned char has_query19:1;
			unsigned char has_ctrl25:1;
			unsigned char has_ctrl26:1;
			unsigned char has_ctrl27_query20:1;
			unsigned char has_ctrl28_query21:1;
			unsigned char has_query22:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f55_query_22 {
	union {
		struct {
			unsigned char has_ctrl29:1;
			unsigned char has_query23:1;
			unsigned char has_guard_disable:1;
			unsigned char has_ctrl30:1;
			unsigned char has_ctrl31:1;
			unsigned char has_ctrl32:1;
			unsigned char has_query24_through_query27:1;
			unsigned char has_query28:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f55_query_23 {
	union {
		struct {
			unsigned char amp_sensor_enabled:1;
			unsigned char image_transposed:1;
			unsigned char first_column_at_left_side:1;
			unsigned char size_of_column2mux:5;
		} __packed;
		unsigned char data[1];
	};
};

struct f55_query_28 {
	union {
		struct {
			unsigned char f55_query28_b0__4:5;
			unsigned char has_ctrl37:1;
			unsigned char has_query29:1;
			unsigned char has_query30:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f55_query_30 {
	union {
		struct {
			unsigned char has_ctrl38:1;
			unsigned char has_query31_query32:1;
			unsigned char has_ctrl39:1;
			unsigned char has_ctrl40:1;
			unsigned char has_ctrl41:1;
			unsigned char has_ctrl42:1;
			unsigned char has_ctrl43_ctrl44:1;
			unsigned char has_query33:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f55_query_33 {
	union {
		struct {
			unsigned char has_extended_amp_pad:1;
			unsigned char has_extended_amp_btn:1;
			unsigned char has_ctrl45_ctrl46:1;
			unsigned char f55_query33_b3:1;
			unsigned char has_ctrl47_sub0_sub1:1;
			unsigned char f55_query33_b5__7:3;
		} __packed;
		unsigned char data[1];
	};
};

struct f55_control_43 {
	union {
		struct {
			unsigned char swap_sensor_side:1;
			unsigned char f55_ctrl43_b1__7:7;
			unsigned char afe_l_mux_size:4;
			unsigned char afe_r_mux_size:4;
		} __packed;
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f55_handle {
	bool amp_sensor;
	bool extended_amp;
	bool extended_amp_btn;
	bool has_force;
	unsigned char size_of_column2mux;
	unsigned char afe_mux_offset;
	unsigned char force_tx_offset;
	unsigned char force_rx_offset;
	unsigned char *tx_assignment;
	unsigned char *rx_assignment;
	unsigned char *force_tx_assignment;
	unsigned char *force_rx_assignment;
	unsigned short query_base_addr;
	unsigned short control_base_addr;
	unsigned short data_base_addr;
	unsigned short command_base_addr;
	struct f55_query query;
	struct f55_query_3 query_3;
	struct f55_query_5 query_5;
	struct f55_query_17 query_17;
	struct f55_query_18 query_18;
	struct f55_query_22 query_22;
	struct f55_query_23 query_23;
	struct f55_query_28 query_28;
	struct f55_query_30 query_30;
	struct f55_query_33 query_33;
};

show_prototype(num_of_mapped_tx)
show_prototype(num_of_mapped_rx)
show_prototype(tx_mapping)
show_prototype(rx_mapping)
show_prototype(force_tx_mapping)
show_prototype(force_rx_mapping)
show_prototype(report_size)
show_prototype(status)
store_prototype(do_preparation)
store_prototype(force_cal)
store_prototype(get_report)
store_prototype(resume_touch)
store_prototype(do_afe_calibration)
show_store_prototype(report_type)
show_store_prototype(fifoindex)
show_store_prototype(no_auto_cal)
show_store_prototype(read_report)
/* td43xx start */
show_store_prototype(td43xx_full_raw)
show_store_prototype(td43xx_noise)
show_store_prototype(tddi_extend_ee_short)
show_store_prototype(td43xx_ee_short)
show_store_prototype(td4722_b7_amp_open)
show_store_prototype(td43xx_amp_open)
show_store_prototype(tddi_amp_electrode_open)
show_store_prototype(abs_0d_open_w_autoservo)
/* td43xx end */

static struct attribute *attrs[] = {
	attrify(num_of_mapped_tx),
	attrify(num_of_mapped_rx),
	attrify(tx_mapping),
	attrify(rx_mapping),
	attrify(force_tx_mapping),
	attrify(force_rx_mapping),
	attrify(report_size),
	attrify(status),
	attrify(do_preparation),
	attrify(force_cal),
	attrify(get_report),
	attrify(resume_touch),
	attrify(do_afe_calibration),
	attrify(report_type),
	attrify(fifoindex),
	attrify(no_auto_cal),
	attrify(read_report),
	/* td43xx start */
	attrify(td43xx_full_raw),
	attrify(td43xx_noise),
	attrify(tddi_extend_ee_short),
	attrify(td43xx_ee_short),
	attrify(td4722_b7_amp_open),
	attrify(td43xx_amp_open),
	attrify(tddi_amp_electrode_open),
	attrify(abs_0d_open_w_autoservo),
	/* td43xx end */
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};
/*
static ssize_t test_sysfs_data_read(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static struct bin_attribute test_report_data = {
	.attr = {
		.name = "report_data",
		.mode = S_IRUGO,
	},
	.size = 0,
	.read = test_sysfs_data_read,
};
*/
static struct synaptics_rmi4_f54_handle *f54;
static struct synaptics_rmi4_f55_handle *f55;

/* td43xx start */
static unsigned char *td43xx_full_raw_data;
static signed short *td43xx_noise_delta;
static unsigned char *td43xx_ee_short_data;
static unsigned char *td43xx_amp_open_data;
/* td43xx end */
static int *g_abs_0d_open_data_output;
static char g_flag_read_report_fail;

DECLARE_COMPLETION(test_remove_complete_force);

static bool test_report_type_valid(enum f54_report_types report_type)
{
	switch (report_type) {
	case F54_8BIT_IMAGE:
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_HIGH_RESISTANCE:
	case F54_TX_TO_TX_SHORTS:
	case F54_RX_TO_RX_SHORTS_1:
	case F54_TRUE_BASELINE:
	case F54_FULL_RAW_CAP_MIN_MAX:
	case F54_RX_OPENS_1:
	case F54_TX_OPENS:
	case F54_TX_TO_GND_SHORTS:
	case F54_RX_TO_RX_SHORTS_2:
	case F54_RX_OPENS_2:
	case F54_FULL_RAW_CAP:
	case F54_FULL_RAW_CAP_NO_RX_COUPLING:
	case F54_SENSOR_SPEED:
	case F54_ADC_RANGE:
	case F54_TRX_OPENS:
	case F54_TRX_TO_GND_SHORTS:
	case F54_TRX_SHORTS:
	case F54_ABS_RAW_CAP:
	case F54_ABS_DELTA_CAP:
	case F54_ABS_HYBRID_DELTA_CAP:
	case F54_ABS_HYBRID_RAW_CAP:
	case F54_AMP_FULL_RAW_CAP:
	case F54_AMP_RAW_ADC:
	/* td43xx start */
	case F54_TD43XX_FULL_RAW:
	case F54_TD43XX_NOISE:
	case F54_TD43XX_EE_SHORT:
	/* td43xx end */
		return true;
		break;
	case F54_ABS_DOZE_NO_CBC_INCELL:
		if (f54->query_65.has_ctrl225) {
			return true;
		}
		else {
			return false;
		}
		break;
	default:
		f54->report_type = INVALID_REPORT_TYPE;
		f54->report_size = 0;
		return false;
	}
}

static void test_set_report_size(void)
{
	int retval;
	unsigned char tx = f54->tx_assigned;
	unsigned char rx = f54->rx_assigned;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	switch (f54->report_type) {
	case F54_8BIT_IMAGE:
		f54->report_size = tx * rx;
		break;
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_TRUE_BASELINE:
	case F54_FULL_RAW_CAP:
	case F54_FULL_RAW_CAP_NO_RX_COUPLING:
	case F54_SENSOR_SPEED:
	case F54_AMP_FULL_RAW_CAP:
	case F54_AMP_RAW_ADC:
	/* td43xx start */
	case F54_TD43XX_FULL_RAW:
	case F54_TD43XX_NOISE:
	/* td43xx end */
		f54->report_size = 2 * tx * rx;
		break;
	/* td43xx start */
	case F54_TD43XX_EE_SHORT:
		f54->report_size = 2 * 2 * tx * rx;
		break;
	/* td43xx end */
	case F54_HIGH_RESISTANCE:
		f54->report_size = HIGH_RESISTANCE_DATA_SIZE;
		break;
	case F54_TX_TO_TX_SHORTS:
	case F54_TX_OPENS:
	case F54_TX_TO_GND_SHORTS:
		f54->report_size = (tx + 7) / 8;
		break;
	case F54_RX_TO_RX_SHORTS_1:
	case F54_RX_OPENS_1:
		if (rx < tx)
			f54->report_size = 2 * rx * rx;
		else
			f54->report_size = 2 * tx * rx;
		break;
	case F54_FULL_RAW_CAP_MIN_MAX:
		f54->report_size = FULL_RAW_CAP_MIN_MAX_DATA_SIZE;
		break;
	case F54_RX_TO_RX_SHORTS_2:
	case F54_RX_OPENS_2:
		if (rx <= tx)
			f54->report_size = 0;
		else
			f54->report_size = 2 * rx * (rx - tx);
		break;
	case F54_ADC_RANGE:
		if (f54->query.has_signal_clarity) {
			retval = synaptics_rmi4_reg_read(rmi4_data,
					f54->control.reg_41->address,
					f54->control.reg_41->data,
					sizeof(f54->control.reg_41->data));
			if (retval < 0) {
				dev_dbg(rmi4_data->pdev->dev.parent,
						"%s: Failed to read control reg_41\n",
						__func__);
				f54->report_size = 0;
				break;
			}
			if (!f54->control.reg_41->no_signal_clarity) {
				if (tx % 4)
					tx += 4 - (tx % 4);
			}
		}
		f54->report_size = 2 * tx * rx;
		break;
	case F54_TRX_OPENS:
	case F54_TRX_TO_GND_SHORTS:
	case F54_TRX_SHORTS:
		f54->report_size = TRX_OPEN_SHORT_DATA_SIZE;
		break;
	case F54_ABS_RAW_CAP:
	case F54_ABS_DELTA_CAP:
	case F54_ABS_HYBRID_DELTA_CAP:
	case F54_ABS_HYBRID_RAW_CAP:
		f54->report_size = 4 * (tx + rx);
		break;
	case F54_ABS_DOZE_NO_CBC_INCELL:
		f54->report_size = 4 * (tx + rx);
		break;
	default:
		f54->report_size = 0;
	}

	return;
}

static int test_set_interrupt(bool set)
{
	int retval;
	unsigned char ii;
	unsigned char zero = 0x00;
	unsigned char temp;
	unsigned char *intr_mask;
	unsigned short f01_ctrl_reg;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	intr_mask = rmi4_data->intr_mask;
	f01_ctrl_reg = rmi4_data->f01_ctrl_base_addr + 1 + f54->intr_reg_num;

	if (!set) {
		retval = synaptics_rmi4_reg_write(rmi4_data,
				f01_ctrl_reg,
				&zero,
				sizeof(zero));
		if (retval < 0)
			return retval;
	}

	for (ii = 0; ii < rmi4_data->num_of_intr_regs; ii++) {
		if (intr_mask[ii] != 0x00) {
			f01_ctrl_reg = rmi4_data->f01_ctrl_base_addr + 1 + ii;
			if (set) {
				retval = synaptics_rmi4_reg_write(rmi4_data,
						f01_ctrl_reg,
						&zero,
						sizeof(zero));
				if (retval < 0)
					return retval;
			} else {
				retval = synaptics_rmi4_reg_write(rmi4_data,
						f01_ctrl_reg,
						&(intr_mask[ii]),
						sizeof(intr_mask[ii]));
				if (retval < 0)
					return retval;

				retval = synaptics_rmi4_reg_read(rmi4_data,
						f01_ctrl_reg,
						&temp,
						sizeof(temp));
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: Failed to read interrupt enable register\n",
							__func__);
					return retval;
				}

				pr_err("%s: intr_mask[%d] = 0x%02x\n", __func__, ii, intr_mask[ii]);
				pr_err("%s: temp = 0x%02x\n", __func__, temp);

				retval = synaptics_rmi4_reg_write(rmi4_data,
						f01_ctrl_reg,
						&(intr_mask[ii]),
						sizeof(intr_mask[ii]));
				if (retval < 0)
					return retval;
			}
		}
	}

	f01_ctrl_reg = rmi4_data->f01_ctrl_base_addr + 1 + f54->intr_reg_num;

	if (set) {
		retval = synaptics_rmi4_reg_write(rmi4_data,
				f01_ctrl_reg,
				&f54->intr_mask,
				1);
		if (retval < 0)
			return retval;
	}

	return 0;
}

static int test_wait_for_command_completion(void)
{
	int retval;
	unsigned char value;
	unsigned char timeout_count;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	timeout_count = 0;
	do {
		if (rmi4_data->suspend)
			return -EINVAL;

		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->command_base_addr,
				&value,
				sizeof(value));
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to read command register\n",
					__func__);
			return retval;
		}

		if (value == 0x00)
			break;

		msleep(100);
		timeout_count++;
	} while (timeout_count < COMMAND_TIMEOUT_100MS);

	if (timeout_count == COMMAND_TIMEOUT_100MS) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Timed out waiting for command completion\n",
				__func__);
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: command = 0x%02x\n", __func__, value);
		return -ETIMEDOUT;
	}

	return 0;
}

static int test_do_command(unsigned char command)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			f54->command_base_addr,
			&command,
			sizeof(command));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write command\n",
				__func__);
		return retval;
	}

	retval = test_wait_for_command_completion();
	if (retval < 0)
		return retval;

	return 0;
}

static int test_do_preparation(void)
{
	int retval;
	unsigned char value;
	unsigned char zero = 0x00;
#ifndef USE_RT8
	unsigned char device_ctrl;
#endif
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
#ifndef USE_RT8
	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set no sleep\n",
				__func__);
		return retval;
	}

	device_ctrl |= NO_SLEEP_ON;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set no sleep\n",
				__func__);
		return retval;
	}

#endif
	if (f54->skip_preparation)
		return 0;

	switch (f54->report_type) {
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_SENSOR_SPEED:
	case F54_ADC_RANGE:
	case F54_ABS_RAW_CAP:
	case F54_ABS_DELTA_CAP:
	case F54_ABS_HYBRID_DELTA_CAP:
	case F54_ABS_HYBRID_RAW_CAP:
#ifdef USE_RT8
	case F54_FULL_RAW_CAP_NO_RX_COUPLING:
#endif
	case F54_TD43XX_FULL_RAW:
	case F54_TD43XX_NOISE:
	case F54_TD43XX_EE_SHORT:
		break;
	case F54_AMP_RAW_ADC:
		if (f54->query_49.has_ctrl188) {
			retval = synaptics_rmi4_reg_read(rmi4_data,
					f54->control.reg_188->address,
					f54->control.reg_188->data,
					sizeof(f54->control.reg_188->data));
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to set start production test\n",
						__func__);
				return retval;
			}
			f54->control.reg_188->start_production_test = 1;
			retval = synaptics_rmi4_reg_write(rmi4_data,
					f54->control.reg_188->address,
					f54->control.reg_188->data,
					sizeof(f54->control.reg_188->data));
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to set start production test\n",
						__func__);
				return retval;
			}
		}
		break;
	case F54_ABS_DOZE_NO_CBC_INCELL:
		if (f54->query_65.has_ctrl225) {
			/* enable force doze mode */
			retval = synaptics_rmi4_reg_read(rmi4_data,
					f54->control.reg_225->address,
					&value,
					sizeof(value));
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: fail to read f54 ctrl225 reg\n",
						__func__);
				return retval;
			}

			if (!(value && 0x01)) {
				value = (value | 0x01);

				retval = synaptics_rmi4_reg_write(rmi4_data,
						f54->control.reg_225->address,
						&value,
						sizeof(value));
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: fail to set f54 ctrl225 reg\n",
							__func__);
					return retval;
				}
				retval = test_do_command(COMMAND_FORCE_UPDATE);
				if (retval < 0) {
					dev_err(rmi4_data->pdev->dev.parent,
							"%s: fail to do force update\n",
							__func__);
					return retval;
				}
			}


			retval = synaptics_rmi4_reg_read(rmi4_data,
					rmi4_data->f01_ctrl_base_addr,
					&device_ctrl,
					sizeof(device_ctrl));
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: fail to get f01 ctrl base reg\n",
						__func__);
				return retval;
			}

			device_ctrl &= 0xF8;  /* clear the bit of no-sleep and set sleep mode = '00'*/

			retval = synaptics_rmi4_reg_write(rmi4_data,
					rmi4_data->f01_ctrl_base_addr,
					&device_ctrl,
					sizeof(device_ctrl));
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: fail to set f01 ctrl base reg\n",
						__func__);
				return retval;
			}

		}
		break;
	default:
		if (f54->query.touch_controller_family == 1)
			disable_cbc(reg_7);
		else if (f54->query.has_ctrl88)
			disable_cbc(reg_88);

		if (f54->query.has_0d_acquisition_control)
			disable_cbc(reg_57);

		if ((f54->query.has_query15) &&
				(f54->query_15.has_query25) &&
				(f54->query_25.has_query27) &&
				(f54->query_27.has_query29) &&
				(f54->query_29.has_query30) &&
				(f54->query_30.has_query32) &&
				(f54->query_32.has_query33) &&
				(f54->query_33.has_query36) &&
				(f54->query_36.has_query38) &&
				(f54->query_38.has_ctrl149)) {
			retval = synaptics_rmi4_reg_write(rmi4_data,
					f54->control.reg_149->address,
					&zero,
					sizeof(f54->control.reg_149->data));
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to disable global CBC\n",
						__func__);
				return retval;
			}
		}

		if (f54->query.has_signal_clarity) {
			retval = synaptics_rmi4_reg_read(rmi4_data,
					f54->control.reg_41->address,
					&value,
					sizeof(f54->control.reg_41->data));
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to disable signal clarity\n",
						__func__);
				return retval;
			}
			value |= 0x01;
			retval = synaptics_rmi4_reg_write(rmi4_data,
					f54->control.reg_41->address,
					&value,
					sizeof(f54->control.reg_41->data));
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to disable signal clarity\n",
						__func__);
				return retval;
			}
		}

		retval = test_do_command(COMMAND_FORCE_UPDATE);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to do force update\n",
					__func__);
			return retval;
		}

		retval = test_do_command(COMMAND_FORCE_CAL);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to do force cal\n",
					__func__);
			return retval;
		}
	}

	return 0;
}

static int test_do_afe_calibration(enum f54_afe_cal mode)
{
	int retval;
	unsigned char timeout = CALIBRATION_TIMEOUT_S;
	unsigned char timeout_count = 0;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			f54->control.reg_188->address,
			f54->control.reg_188->data,
			sizeof(f54->control.reg_188->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to start calibration\n",
				__func__);
		return retval;
	}

	if (mode == F54_AFE_CAL)
		f54->control.reg_188->start_calibration = 1;
	else if (mode == F54_AFE_IS_CAL)
		f54->control.reg_188->start_is_calibration = 1;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			f54->control.reg_188->address,
			f54->control.reg_188->data,
			sizeof(f54->control.reg_188->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to start calibration\n",
				__func__);
		return retval;
	}

	do {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->control.reg_188->address,
				f54->control.reg_188->data,
				sizeof(f54->control.reg_188->data));
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to complete calibration\n",
					__func__);
			return retval;
		}

		if (mode == F54_AFE_CAL) {
			if (!f54->control.reg_188->start_calibration)
				break;
		} else if (mode == F54_AFE_IS_CAL) {
			if (!f54->control.reg_188->start_is_calibration)
				break;
		}

		if (timeout_count == timeout) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Timed out waiting for calibration completion\n",
					__func__);
			return -EBUSY;
		}

		timeout_count++;
		msleep(1000);
	} while (true);

	/* check CRC */
	retval = synaptics_rmi4_reg_read(rmi4_data,
			f54->data_31.address,
			f54->data_31.data,
			sizeof(f54->data_31.data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read calibration CRC\n",
				__func__);
		return retval;
	}

	if (mode == F54_AFE_CAL) {
		if (f54->data_31.calibration_crc == 0)
			return 0;
	} else if (mode == F54_AFE_IS_CAL) {
		if (f54->data_31.is_calibration_crc == 0)
			return 0;
	}

	dev_err(rmi4_data->pdev->dev.parent,
			"%s: Failed to read calibration CRC\n",
			__func__);

	return -EINVAL;
}

static int test_check_for_idle_status(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	switch (f54->status) {
	case STATUS_IDLE:
		retval = 0;
		break;
	case STATUS_BUSY:
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Status busy\n",
				__func__);
		retval = -EINVAL;
		break;
	case STATUS_ERROR:
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Status error\n",
				__func__);
		retval = -EINVAL;
		break;
	default:
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Invalid status (%d)\n",
				__func__, f54->status);
		retval = -EINVAL;
	}

	return retval;
}

static void test_timeout_work(struct work_struct *work)
{
	int retval;
	unsigned char command;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	mutex_lock(&f54->status_mutex);

	if (f54->status == STATUS_BUSY) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->command_base_addr,
				&command,
				sizeof(command));
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to read command register\n",
					__func__);
		} else if (command & COMMAND_GET_REPORT) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Report type not supported by FW\n",
					__func__);
		} else {
			queue_work(f54->test_report_workqueue,
					&f54->test_report_work);
			goto exit;
		}
		f54->status = STATUS_ERROR;
		f54->report_size = 0;
	}

exit:
	mutex_unlock(&f54->status_mutex);

	return;
}

static enum hrtimer_restart test_get_report_timeout(struct hrtimer *timer)
{
	schedule_work(&(f54->timeout_work));

	return HRTIMER_NORESTART;
}

static int test_do_resume_touch(void)
{
	int retval = 0;
#ifndef USE_RT8
	unsigned char device_ctrl;
#endif
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
#ifndef USE_RT8
	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to restore no sleep setting\n",
				__func__);
		return retval;
	}

	device_ctrl = device_ctrl & ~NO_SLEEP_ON;
	device_ctrl |= rmi4_data->no_sleep_setting;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			&device_ctrl,
			sizeof(device_ctrl));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to restore no sleep setting\n",
				__func__);
		return retval;
	}
#endif
	test_set_interrupt(false);

	if (f54->skip_preparation)
		return 0;

	switch (f54->report_type) {
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_SENSOR_SPEED:
	case F54_ADC_RANGE:
	case F54_ABS_RAW_CAP:
	case F54_ABS_DELTA_CAP:
	case F54_ABS_HYBRID_DELTA_CAP:
	case F54_ABS_HYBRID_RAW_CAP:
#ifdef USE_RT8
	case F54_FULL_RAW_CAP_NO_RX_COUPLING:
#endif
	case F54_TD43XX_FULL_RAW:
		break;
	case F54_ABS_DOZE_NO_CBC_INCELL:
		rmi4_data->reset_device(rmi4_data, false);
		break;
	case F54_AMP_RAW_ADC:
		if (f54->query_49.has_ctrl188) {
			retval = synaptics_rmi4_reg_read(rmi4_data,
					f54->control.reg_188->address,
					f54->control.reg_188->data,
					sizeof(f54->control.reg_188->data));
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to set start production test\n",
						__func__);
				return retval;
			}
			f54->control.reg_188->start_production_test = 0;
			retval = synaptics_rmi4_reg_write(rmi4_data,
					f54->control.reg_188->address,
					f54->control.reg_188->data,
					sizeof(f54->control.reg_188->data));
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to set start production test\n",
						__func__);
				return retval;
			}
		}
		break;
	default:
		pr_err("%s: resetting device\n", __func__);
		rmi4_data->reset_device(rmi4_data, false);
		pr_err("%s: done resetting device\n", __func__);
	}

	return retval;
}

static void test_resume_touch_work(struct work_struct *work)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = test_do_resume_touch();
	if (retval < 0)
		dev_err(rmi4_data->pdev->dev.parent,
			"%s: Resume touch failed\n", __func__);

}

static ssize_t test_sysfs_num_of_mapped_tx_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", f54->tx_assigned);
}

static ssize_t test_sysfs_num_of_mapped_rx_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", f54->rx_assigned);
}

static ssize_t test_sysfs_tx_mapping_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cnt;
	int count = 0;
	unsigned char ii;
	unsigned char tx_num;
	unsigned char tx_electrodes;

	if (!f55)
		return -EINVAL;

	tx_electrodes = f55->query.num_of_tx_electrodes;

	for (ii = 0; ii < tx_electrodes; ii++) {
		tx_num = f55->tx_assignment[ii];
		if (tx_num == 0xff)
			cnt = snprintf(buf, PAGE_SIZE - count, "xx ");
		else
			cnt = snprintf(buf, PAGE_SIZE - count, "%02u ", tx_num);
		buf += cnt;
		count += cnt;
	}

	snprintf(buf, PAGE_SIZE - count, "\n");
	count++;

	return count;
}

static ssize_t test_sysfs_rx_mapping_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cnt;
	int count = 0;
	unsigned char ii;
	unsigned char rx_num;
	unsigned char rx_electrodes;

	if (!f55)
		return -EINVAL;

	rx_electrodes = f55->query.num_of_rx_electrodes;

	for (ii = 0; ii < rx_electrodes; ii++) {
		rx_num = f55->rx_assignment[ii];
		if (rx_num == 0xff)
			cnt = snprintf(buf, PAGE_SIZE - count, "xx ");
		else
			cnt = snprintf(buf, PAGE_SIZE - count, "%02u ", rx_num);
		buf += cnt;
		count += cnt;
	}

	snprintf(buf, PAGE_SIZE - count, "\n");
	count++;

	return count;
}

static ssize_t test_sysfs_force_tx_mapping_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cnt;
	int count = 0;
	unsigned char ii;
	unsigned char tx_num;
	unsigned char tx_electrodes;

	if (!f55 || !f55->has_force)
		return -EINVAL;

	tx_electrodes = f55->query.num_of_tx_electrodes;

	for (ii = 0; ii < tx_electrodes; ii++) {
		tx_num = f55->force_tx_assignment[ii];
		if (tx_num == 0xff)
			cnt = snprintf(buf, PAGE_SIZE - count, "xx ");
		else
			cnt = snprintf(buf, PAGE_SIZE - count, "%02u ", tx_num);
		buf += cnt;
		count += cnt;
	}

	snprintf(buf, PAGE_SIZE - count, "\n");
	count++;

	return count;
}

static ssize_t test_sysfs_force_rx_mapping_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int cnt;
	int count = 0;
	unsigned char ii;
	unsigned char rx_num;
	unsigned char rx_electrodes;

	if (!f55 || !f55->has_force)
		return -EINVAL;

	rx_electrodes = f55->query.num_of_rx_electrodes;

	for (ii = 0; ii < rx_electrodes; ii++) {
		rx_num = f55->force_rx_assignment[ii];
		if (rx_num == 0xff)
			cnt = snprintf(buf, PAGE_SIZE - count, "xx ");
		else
			cnt = snprintf(buf, PAGE_SIZE - count, "%02u ", rx_num);
		buf += cnt;
		count += cnt;
	}

	snprintf(buf, PAGE_SIZE - count, "\n");
	count++;

	return count;
}

static ssize_t test_sysfs_report_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", f54->report_size);
}

static ssize_t test_sysfs_status_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;

	mutex_lock(&f54->status_mutex);

	retval = snprintf(buf, PAGE_SIZE, "%u\n", f54->status);

	mutex_unlock(&f54->status_mutex);

	return retval;
}

static ssize_t test_sysfs_do_preparation_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting != 1)
		return -EINVAL;

	mutex_lock(&f54->status_mutex);

	retval = test_check_for_idle_status();
	if (retval < 0)
		goto exit;

	retval = test_do_preparation();
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do preparation\n",
				__func__);
		goto exit;
	}

	retval = count;

exit:
	mutex_unlock(&f54->status_mutex);

	return retval;
}

static ssize_t test_sysfs_force_cal_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting != 1)
		return -EINVAL;

	mutex_lock(&f54->status_mutex);

	retval = test_check_for_idle_status();
	if (retval < 0)
		goto exit;

	retval = test_do_command(COMMAND_FORCE_CAL);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do force cal\n",
				__func__);
		goto exit;
	}

	retval = count;

exit:
	mutex_unlock(&f54->status_mutex);

	return retval;
}

/* tddi f54 test reporting + */
#ifdef F54_POLLING_GET_REPORT
static ssize_t test_sysfs_get_report_polling(void)
{
	int retval = 0;
	unsigned char report_index[2];
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = test_wait_for_command_completion();
	if (retval < 0) {
		retval = -EIO;
		f54->status = STATUS_ERROR;
		return retval;
	}

	test_set_report_size();
	if (f54->report_size == 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Report data size = 0\n", __func__);
		retval = -EIO;
		f54->status = STATUS_ERROR;
		return retval;
	}

	if (f54->data_buffer_size < f54->report_size) {
		if (f54->data_buffer_size)
			kfree(f54->report_data);
		f54->report_data = kzalloc(f54->report_size, GFP_KERNEL);
		if (!f54->report_data) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to alloc mem for data buffer\n", __func__);
			f54->data_buffer_size = 0;
			retval = -EIO;
			f54->status = STATUS_ERROR;
			return retval;
		}
		f54->data_buffer_size = f54->report_size;
	}

	report_index[0] = 0;
	report_index[1] = 0;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			f54->data_base_addr + REPORT_INDEX_OFFSET,
			report_index,
			sizeof(report_index));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write report data index\n", __func__);
		retval = -EIO;
		f54->status = STATUS_ERROR;
		return retval;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			f54->data_base_addr + REPORT_DATA_OFFSET,
			f54->report_data,
			f54->report_size);

	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report data\n",
				__func__);
		retval = -EIO;
		f54->status = STATUS_ERROR;
		return retval;
	}

	f54->status = STATUS_IDLE;
	return retval;
}
#endif
/* tddi f54 test reporting - */


static ssize_t test_sysfs_get_report_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned char command;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting != 1)
		return -EINVAL;

	mutex_lock(&f54->status_mutex);

	retval = test_check_for_idle_status();
	if (retval < 0)
		goto exit;

	if (!test_report_type_valid(f54->report_type)) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Invalid report type\n",
				__func__);
		retval = -EINVAL;
		goto exit;
	}

	test_set_interrupt(true);

	command = (unsigned char)COMMAND_GET_REPORT;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			f54->command_base_addr,
			&command,
			sizeof(command));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write get report command\n",
				__func__);
		goto exit;
	}

/* tddi f54 test reporting + */
#ifdef F54_POLLING_GET_REPORT

	retval = test_sysfs_get_report_polling();
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to get report image\n",
				__func__);
		goto exit;
	}

#else
	f54->status = STATUS_BUSY;
	f54->report_size = 0;
	f54->data_pos = 0;
/*
	hrtimer_start(&f54->watchdog,
			ktime_set(GET_REPORT_TIMEOUT_S, 0),
			HRTIMER_MODE_REL);
*/
	retval = count;

#endif

exit:
	mutex_unlock(&f54->status_mutex);

	return retval;
}

static ssize_t test_sysfs_resume_touch_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting != 1)
		return -EINVAL;

	retval = test_do_resume_touch();
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Resume touch failed\n", __func__);
		return -EINVAL;
	}

	return count;
}

static ssize_t test_sysfs_do_afe_calibration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (!f54->query_49.has_ctrl188) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: F54_ANALOG_Ctrl188 not found\n",
				__func__);
		return -EINVAL;
	}

	if (setting == 0 || setting == 1)
		retval = test_do_afe_calibration((enum f54_afe_cal)setting);
	else
		return -EINVAL;

	if (retval)
		return retval;
	else
		return count;
}

static ssize_t test_sysfs_report_type_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", f54->report_type);
}

static ssize_t test_sysfs_report_type_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned char data;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
#ifdef USE_RT8
	static bool do_once;
#endif

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	mutex_lock(&f54->status_mutex);

	retval = test_check_for_idle_status();
	if (retval < 0)
		goto exit;

#ifdef USE_RT8
	if (setting == 20)
		setting = 8;
#endif

	if (!test_report_type_valid((enum f54_report_types)setting)) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Report type not supported by driver\n",
				__func__);
		retval = -EINVAL;
		goto exit;
	}

#ifdef USE_RT8
	if ((f54->report_type == F54_FULL_RAW_CAP_NO_RX_COUPLING) && do_once) {
		rmi4_data->reset_device(rmi4_data, false);
		msleep(1000);
		retval = test_do_command(COMMAND_ENTER_INCELL_TEST_MODE);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to enter incell test mode\n",
					__func__);
			goto exit;
		}
		msleep(100);
		do_once = false;
	}
#endif

	f54->report_type = (enum f54_report_types)setting;
	data = (unsigned char)setting;
	retval = synaptics_rmi4_reg_write(rmi4_data,
			f54->data_base_addr,
			&data,
			sizeof(data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write report type\n",
				__func__);
		goto exit;
	}

	retval = count;

exit:
	mutex_unlock(&f54->status_mutex);

	return retval;
}

static ssize_t test_sysfs_fifoindex_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	unsigned char data[2];
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			f54->data_base_addr + REPORT_INDEX_OFFSET,
			data,
			sizeof(data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report index\n",
				__func__);
		return retval;
	}

	batohs(&f54->fifoindex, data);

	return snprintf(buf, PAGE_SIZE, "%u\n", f54->fifoindex);
}

static ssize_t test_sysfs_fifoindex_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned char data[2];
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	f54->fifoindex = setting;

	hstoba(data, (unsigned short)setting);

	retval = synaptics_rmi4_reg_write(rmi4_data,
			f54->data_base_addr + REPORT_INDEX_OFFSET,
			data,
			sizeof(data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write report index\n",
				__func__);
		return retval;
	}

	return count;
}

static ssize_t test_sysfs_no_auto_cal_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", f54->no_auto_cal);
}

static ssize_t test_sysfs_no_auto_cal_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned char data;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting > 1)
		return -EINVAL;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			f54->control_base_addr,
			&data,
			sizeof(data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read no auto cal setting\n",
				__func__);
		return retval;
	}

	if (setting)
		data |= CONTROL_NO_AUTO_CAL;
	else
		data &= ~CONTROL_NO_AUTO_CAL;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			f54->control_base_addr,
			&data,
			sizeof(data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write no auto cal setting\n",
				__func__);
		return retval;
	}

	f54->no_auto_cal = (setting == 1);

	return count;
}

static ssize_t test_sysfs_read_report_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int ii;
	unsigned int jj;
	int cnt;
	int count = 0;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	char *report_data_8;
	short *report_data_16;
	int *report_data_32;
	unsigned short *report_data_u16;
	unsigned int *report_data_u32;

	if (f54->report_data == NULL)
		return -EINVAL;

	pr_err("%s: f54->report_type = %d\n", __func__, f54->report_type);

	switch (f54->report_type) {
	case F54_8BIT_IMAGE:
		report_data_8 = (char *)f54->report_data;
		for (ii = 0; ii < f54->report_size; ii++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "%03d: %d\n",
					ii, *report_data_8);
			report_data_8++;
			buf += cnt;
			count += cnt;
		}
		break;
	case F54_AMP_RAW_ADC:
		report_data_u16 = (unsigned short *)f54->report_data;
		cnt = snprintf(buf, PAGE_SIZE - count, "tx = %d\nrx = %d\n",
				tx_num, rx_num);
		buf += cnt;
		count += cnt;

		for (ii = 0; ii < tx_num; ii++) {
			for (jj = 0; jj < (rx_num - 1); jj++) {
				cnt = snprintf(buf, PAGE_SIZE - count, "%-4d ",
						*report_data_u16);
				report_data_u16++;
				buf += cnt;
				count += cnt;
			}
			cnt = snprintf(buf, PAGE_SIZE - count, "%-4d\n",
					*report_data_u16);
			report_data_u16++;
			buf += cnt;
			count += cnt;
		}
		break;
	case F54_16BIT_IMAGE:
	case F54_RAW_16BIT_IMAGE:
	case F54_TRUE_BASELINE:
	case F54_FULL_RAW_CAP:
	case F54_FULL_RAW_CAP_NO_RX_COUPLING:
	case F54_SENSOR_SPEED:
	case F54_AMP_FULL_RAW_CAP:
	/* td43xx start */
	case F54_TD43XX_NOISE:
	/* td43xx end */
		report_data_16 = (short *)f54->report_data;
		cnt = snprintf(buf, PAGE_SIZE - count, "tx = %d\nrx = %d\n",
				tx_num, rx_num);
		buf += cnt;
		count += cnt;

		for (ii = 0; ii < tx_num; ii++) {
			for (jj = 0; jj < (rx_num - 1); jj++) {
				cnt = snprintf(buf, PAGE_SIZE - count, "%-4d ",
						*report_data_16);
				report_data_16++;
				buf += cnt;
				count += cnt;
			}
			cnt = snprintf(buf, PAGE_SIZE - count, "%-4d\n",
					*report_data_16);
			report_data_16++;
			buf += cnt;
			count += cnt;
		}
		break;
	/* tddi f54 test reporting + */
	case F54_TD43XX_FULL_RAW:
		report_data_u16 = (unsigned short *)f54->report_data;
		cnt = snprintf(buf, PAGE_SIZE - count, "tx = %d\nrx = %d\n\n",
				tx_num, rx_num);
		buf += cnt;
		count += cnt;

		for (ii = 0; ii < tx_num; ii++) {
			for (jj = 0; jj < (rx_num - 1); jj++) {
				cnt = snprintf(buf, PAGE_SIZE - count, "%-5d ",
						*report_data_u16);
				report_data_u16++;
				buf += cnt;
				count += cnt;
			}
			cnt = snprintf(buf, PAGE_SIZE - count, "%-5d\n",
					*report_data_u16);
			report_data_u16++;
			buf += cnt;
			count += cnt;
		}
		break;
	case F54_TD43XX_EE_SHORT:
		report_data_u16 = (unsigned short *)f54->report_data;
		cnt = snprintf(buf, PAGE_SIZE - count, "tx = %d\nrx = %d\n\n",
				tx_num, rx_num);
		buf += cnt;
		count += cnt;

		for (ii = 0; ii < tx_num; ii++) {
			for (jj = 0; jj < (rx_num - 1); jj++) {
				cnt = snprintf(buf, PAGE_SIZE - count, "%-4d ",
						*report_data_u16);
				report_data_u16++;
				buf += cnt;
				count += cnt;
			}
			cnt = snprintf(buf, PAGE_SIZE - count, "%-4d\n",
					*report_data_u16);
			report_data_u16++;
			buf += cnt;
			count += cnt;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "\n");
		buf += cnt;
		count += cnt;
		for (ii = 0; ii < tx_num; ii++) {
			for (jj = 0; jj < (rx_num - 1); jj++) {
				cnt = snprintf(buf, PAGE_SIZE - count, "%-4d ",
						*report_data_u16);
				report_data_u16++;
				buf += cnt;
				count += cnt;
			}
			cnt = snprintf(buf, PAGE_SIZE - count, "%-4d\n",
					*report_data_u16);
			report_data_u16++;
			buf += cnt;
			count += cnt;
		}
		break;
	/* tddi f54 test reporting - */
	case F54_HIGH_RESISTANCE:
	case F54_FULL_RAW_CAP_MIN_MAX:
		report_data_16 = (short *)f54->report_data;
		for (ii = 0; ii < f54->report_size; ii += 2) {
			cnt = snprintf(buf, PAGE_SIZE - count, "%03d: %d\n",
					ii / 2, *report_data_16);
			report_data_16++;
			buf += cnt;
			count += cnt;
		}
		break;
	case F54_ABS_RAW_CAP:
		report_data_u32 = (unsigned int *)f54->report_data;
		cnt = snprintf(buf, PAGE_SIZE - count, "rx ");
		buf += cnt;
		count += cnt;
		for (ii = 0; ii < rx_num; ii++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "     %2d", ii);
			buf += cnt;
			count += cnt;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "\n");
		buf += cnt;
		count += cnt;

		cnt = snprintf(buf, PAGE_SIZE - count, "   ");
		buf += cnt;
		count += cnt;
		for (ii = 0; ii < rx_num; ii++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "  %5u",
					*report_data_u32);
			report_data_u32++;
			buf += cnt;
			count += cnt;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "\n");
		buf += cnt;
		count += cnt;

		cnt = snprintf(buf, PAGE_SIZE - count, "tx ");
		buf += cnt;
		count += cnt;
		for (ii = 0; ii < tx_num; ii++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "     %2d", ii);
			buf += cnt;
			count += cnt;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "\n");
		buf += cnt;
		count += cnt;

		cnt = snprintf(buf, PAGE_SIZE - count, "   ");
		buf += cnt;
		count += cnt;
		for (ii = 0; ii < tx_num; ii++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "  %5u",
					*report_data_u32);
			report_data_u32++;
			buf += cnt;
			count += cnt;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "\n");
		buf += cnt;
		count += cnt;
		break;
	case F54_ABS_DELTA_CAP:
	case F54_ABS_HYBRID_DELTA_CAP:
	case F54_ABS_HYBRID_RAW_CAP:
		report_data_32 = (int *)f54->report_data;
		cnt = snprintf(buf, PAGE_SIZE - count, "rx ");
		buf += cnt;
		count += cnt;
		for (ii = 0; ii < rx_num; ii++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "     %2d", ii);
			buf += cnt;
			count += cnt;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "\n");
		buf += cnt;
		count += cnt;

		cnt = snprintf(buf, PAGE_SIZE - count, "   ");
		buf += cnt;
		count += cnt;
		for (ii = 0; ii < rx_num; ii++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "  %5d",
					*report_data_32);
			report_data_32++;
			buf += cnt;
			count += cnt;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "\n");
		buf += cnt;
		count += cnt;

		cnt = snprintf(buf, PAGE_SIZE - count, "tx ");
		buf += cnt;
		count += cnt;
		for (ii = 0; ii < tx_num; ii++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "     %2d", ii);
			buf += cnt;
			count += cnt;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "\n");
		buf += cnt;
		count += cnt;

		cnt = snprintf(buf, PAGE_SIZE - count, "   ");
		buf += cnt;
		count += cnt;
		for (ii = 0; ii < tx_num; ii++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "  %5d",
					*report_data_32);
			report_data_32++;
			buf += cnt;
			count += cnt;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "\n");
		buf += cnt;
		count += cnt;
		break;
	case F54_ABS_DOZE_NO_CBC_INCELL:
		cnt = snprintf(buf, PAGE_SIZE - count, "tx = %d, rx = %d\n\n",
				tx_num, rx_num);
		buf += cnt;
		count += cnt;

		report_data_u32 = (unsigned int *)f54->report_data;
		cnt = snprintf(buf, PAGE_SIZE - count, "rx ");
		buf += cnt;
		count += cnt;
		for (ii = 0; ii < rx_num; ii++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "     %2d", ii);
			buf += cnt;
			count += cnt;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "\n");
		buf += cnt;
		count += cnt;

		cnt = snprintf(buf, PAGE_SIZE - count, "   ");
		buf += cnt;
		count += cnt;
		for (ii = 0; ii < rx_num; ii++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "  %5u",
					*report_data_u32);
			report_data_u32++;
			buf += cnt;
			count += cnt;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "\n");
		buf += cnt;
		count += cnt;

		cnt = snprintf(buf, PAGE_SIZE - count, "tx ");
		buf += cnt;
		count += cnt;
		for (ii = 0; ii < tx_num; ii++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "     %2d", ii);
			buf += cnt;
			count += cnt;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "\n");
		buf += cnt;
		count += cnt;

		cnt = snprintf(buf, PAGE_SIZE - count, "   ");
		buf += cnt;
		count += cnt;
		for (ii = 0; ii < tx_num; ii++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "  %5u",
					*report_data_u32);
			report_data_u32++;
			buf += cnt;
			count += cnt;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "\n");
		buf += cnt;
		count += cnt;
		break;
	default:
		for (ii = 0; ii < f54->report_size; ii++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "%03d: 0x%02x\n",
					ii, f54->report_data[ii]);
			buf += cnt;
			count += cnt;
		}
	}

	snprintf(buf, PAGE_SIZE - count, "\n");
	count++;

	return count;
}

static ssize_t test_sysfs_read_report_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned char timeout = GET_REPORT_TIMEOUT_S * 10;
	unsigned char timeout_count;
	const char cmd[] = {'1', 0};
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	if (rmi4_data->suspend) {
		return -EINVAL;
	}

	pr_err("%s\n", __func__);

	pr_err("%s: f54->tx_assigned = %d\n", __func__, f54->tx_assigned);
	pr_err("%s: f54->rx_assigned = %d\n", __func__, f54->rx_assigned);

	retval = test_sysfs_report_type_store(dev, attr, buf, count);
	if (retval < 0)
		goto exit;

	pr_err("%s: done test_sysfs_report_type_store\n", __func__);

	retval = test_sysfs_do_preparation_store(dev, attr, cmd, 1);
	if (retval < 0)
		goto exit;

	pr_err("%s: done test_sysfs_do_preparation_store\n", __func__);

	retval = test_sysfs_get_report_store(dev, attr, cmd, 1);
	if (retval < 0)
		goto exit;

	pr_err("%s: done test_sysfs_get_report_store\n", __func__);

	retval = test_wait_for_command_completion();
	if (retval < 0)
		goto exit;

	pr_err("%s: done test_wait_for_command_completion\n", __func__);

	if (rmi4_data->chip_is_tddi)
		queue_work(f54->test_report_workqueue, &f54->test_report_work);

	timeout_count = 0;
	do {
		if (f54->status != STATUS_BUSY)
			break;
		msleep(100);
		timeout_count++;
	} while (timeout_count < timeout);

	if ((f54->status != STATUS_IDLE) || (f54->report_size == 0)) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report\n",
				__func__);
		retval = -EINVAL;
		goto exit;
	}

	pr_err("%s: got report\n", __func__);

	if (f54->rmi4_data->dump_flags) {
		retval = test_sysfs_resume_touch_store(dev, attr, cmd, 1);
		if (retval < 0)
			goto exit;
	} else
		rmi4_data->reset_device(rmi4_data, false);

	pr_err("%s: exiting\n", __func__);

	return count;

exit:
	rmi4_data->reset_device(rmi4_data, false);
	return retval;
}

/* td43xx start */
static ssize_t test_sysfs_read_report_td43xx(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count,
		bool do_preparation, bool do_reset)
{
	int retval;
	unsigned char timeout = GET_REPORT_TIMEOUT_S * 10;
	unsigned char timeout_count;
	const char cmd[] = {'1', 0};
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = test_sysfs_report_type_store(dev, attr, buf, count);
	if (retval < 0)
		goto exit;

	if (do_preparation) {
		retval = test_sysfs_do_preparation_store(dev, attr, cmd, 1);
		if (retval < 0)
			goto exit;
	}
	retval = test_sysfs_get_report_store(dev, attr, cmd, 1);
	if (retval < 0)
		goto exit;

	retval = test_wait_for_command_completion();
	if (retval < 0)
		goto exit;

	queue_work(f54->test_report_workqueue, &f54->test_report_work);

	timeout_count = 0;
	do {
		if (f54->status != STATUS_BUSY)
			break;
		msleep(100);
		timeout_count++;
	} while (timeout_count < timeout);

	if ((f54->status != STATUS_IDLE) || (f54->report_size == 0)) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report\n",
				__func__);
		retval = -EINVAL;
		goto exit;
	}


	return count;

exit:
	if (do_reset)
		rmi4_data->reset_device(rmi4_data, false);

	return retval;
}

static short FindMedian(short *pdata, int num)
{
	int i, j;
	short temp;
	short *value;
	short median;

	value = (short *)kzalloc(num * sizeof(short), GFP_KERNEL);

	for (i = 0; i < num; i++)
		*(value + i) = *(pdata + i);

	for (i = 1; i <= num - 1; i++) {
		for (j = 1; j <= num - i; j++) {
			if (*(value + j - 1) <= *(value + j)) {
			   temp = *(value + j - 1);
			   *(value + j - 1) = *(value + j);
			   *(value + j) = temp;
			} else
				continue;
		}
	}

	if (num % 2 == 0)
		median = (*(value + (num / 2 - 1)) + *(value + (num / 2)))/2;
	else
		median = *(value + (num / 2));

	if (value)
		kfree(value);

	return median;
}

static int tddi_ratio_calculation(signed short *p_image)
{
	int retval = 0;	int i, j;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	unsigned char left_size = f54->left_mux_size;
	unsigned char right_size = f54->right_mux_size;
	signed short *p_data_16;
	signed short *p_left_median = NULL;
	signed short *p_right_median = NULL;
	signed short *p_left_column_buf = NULL;
	signed short *p_right_column_buf = NULL;
	signed int temp;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	if (!p_image) {
		dev_err(rmi4_data->pdev->dev.parent, "%s: Fail. p_image is null\n", __func__);
		retval = -EINVAL;
		goto exit;
	}


	p_right_median = (signed short *) kzalloc(rx_num * sizeof(short), GFP_KERNEL);
	if (!p_right_median) {
		dev_err(rmi4_data->pdev->dev.parent, "%s: Failed to alloc mem for p_right_median\n", __func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_left_median = (signed short *) kzalloc(rx_num * sizeof(short), GFP_KERNEL);
	if (!p_left_median) {
		dev_err(rmi4_data->pdev->dev.parent, "%s: Failed to alloc mem for p_left_median\n", __func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_right_column_buf = (signed short *) kzalloc(right_size * rx_num * sizeof(short), GFP_KERNEL);
	if (!p_right_column_buf) {
		dev_err(rmi4_data->pdev->dev.parent, "%s: Failed to alloc mem for p_right_column_buf\n", __func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_left_column_buf = (signed short *) kzalloc(left_size * rx_num * sizeof(short), GFP_KERNEL);
	if (!p_left_column_buf) {
		dev_err(rmi4_data->pdev->dev.parent, "%s: Failed to alloc mem for p_left_column_buf\n", __func__);
		retval = -ENOMEM;
		goto exit;
	}


	if (f54->swap_sensor_side) {

		p_data_16 = p_image;
		for (i = 0; i < rx_num; i++) {
			for (j = 0; j < left_size; j++) {
				p_left_column_buf[i * left_size + j] = p_data_16[j * rx_num + i];
			}
		}

		p_data_16 = p_image + left_size * rx_num;
		for (i = 0; i < rx_num; i++) {
			for (j = 0; j < right_size; j++) {
				p_right_column_buf[i * right_size + j] = p_data_16[j * rx_num + i];
			}
		}
	}
	else {

		p_data_16 = p_image;
		for (i = 0; i < rx_num; i++) {
			for (j = 0; j < right_size; j++) {
				p_right_column_buf[i * right_size + j] = p_data_16[j * rx_num + i];
			}
		}

		p_data_16 = p_image + right_size * rx_num;
		for (i = 0; i < rx_num; i++) {
			for (j = 0; j < left_size; j++) {
				p_left_column_buf[i * left_size + j] = p_data_16[j * rx_num + i];
			}
		}
	}


	for (i = 0; i < rx_num; i++) {
		p_left_median[i] = FindMedian(p_left_column_buf + i * left_size, left_size);
		p_right_median[i] = FindMedian(p_right_column_buf + i * right_size, right_size);
	}



	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {

			if (f54->swap_sensor_side) {

				if (i < left_size) {
					temp = (signed int) p_image[i * rx_num + j];
					temp = temp * 100 / p_left_median[j];
				}
				else {
					temp = (signed int) p_image[i * rx_num + j];
					temp = temp * 100 / p_right_median[j];
				}
			}
			else {

				if (i < right_size) {
					temp = (signed int) p_image[i * rx_num + j];
					temp = temp * 100 / p_right_median[j];
				}
				else {
					temp = (signed int) p_image[i * rx_num + j];
					temp = temp * 100 / p_left_median[j];
				}
			}


			p_image[i * rx_num + j] = temp;
		}
	}

exit:
	kfree(p_right_median);
	kfree(p_left_median);
	kfree(p_right_column_buf);
	kfree(p_left_column_buf);
	return retval;
}
static ssize_t test_sysfs_tddi_extend_ee_short_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	int i, j, offset;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	signed short *tddi_rt95_part_one = NULL;
	signed short *tddi_rt95_part_two = NULL;
	unsigned int buffer_size = tx_num * rx_num * 2;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	struct synaptics_dsx_factory_param *factory_param = rmi4_data->factory_param;

	struct f54_control control = f54->control;

	struct f54_control_99  original_f54_ctrl99;
	struct f54_control_182 original_f54_ctrl182;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting != 1 || !factory_param)
		return -EINVAL;

	/* allocate the g_tddi_ee_short_data_output */
	if (td43xx_ee_short_data)
		kfree(td43xx_ee_short_data);

	td43xx_ee_short_data = kzalloc(tx_num * rx_num, GFP_KERNEL);
	if (!td43xx_ee_short_data) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for td43xx_ee_short_data\n",
				__func__);
		return -ENOMEM;
	}


	tddi_rt95_part_one = kzalloc(buffer_size, GFP_KERNEL);
	if (!tddi_rt95_part_one) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for tddi_rt95_part_one\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	tddi_rt95_part_two = kzalloc(buffer_size, GFP_KERNEL);
	if (!tddi_rt95_part_two) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for td43xx_rt95_part_two\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	g_flag_read_report_fail = 0;

	/* step 1 */
	/* keep the original integration and reset duration */
	retval = synaptics_rmi4_reg_read(rmi4_data,
			control.reg_99->address,
			original_f54_ctrl99.data,
			sizeof(original_f54_ctrl99.data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read original data from f54_ctrl99\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	/* keep the original timing control */
	retval = synaptics_rmi4_reg_read(rmi4_data,
			control.reg_182->address,
			original_f54_ctrl182.data,
			sizeof(original_f54_ctrl182.data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read original data from f54_ctrl182\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	/* step 2 */
	/* Modify the Reset duration, Integration duration, and CBC Timing Control as well */
	control.reg_99->integration_duration_lsb = factory_param->tddi_extend_ee_short_int_dur;
	control.reg_99->integration_duration_msb = (factory_param->tddi_extend_ee_short_int_dur >> 8) & 0xff;
	control.reg_99->reset_duration = factory_param->tddi_extend_ee_short_reset_dur;
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_99->address,
			control.reg_99->data,
			sizeof(control.reg_99->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set f54_ctrl99 in step 2\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	control.reg_182->cbc_timing_ctrl_tx_lsb = factory_param->tddi_extend_ee_short_tx_on_count;
	control.reg_182->cbc_timing_ctrl_tx_msb = (factory_param->tddi_extend_ee_short_tx_on_count >> 8) & 0xff;
	control.reg_182->cbc_timing_ctrl_rx_lsb = factory_param->tddi_extend_ee_short_rx_on_count;
	control.reg_182->cbc_timing_ctrl_rx_msb = (factory_param->tddi_extend_ee_short_rx_on_count >> 8) & 0xff;
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_182->address,
			control.reg_182->data,
			sizeof(control.reg_182->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set f54_reg_182 in step 2\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	/* step 3 */
	/* Force update */
	retval = test_do_command(COMMAND_FORCE_UPDATE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do force update in step 4\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	/* step 4 */
	/* get report image 95 */
	retval = test_sysfs_read_report_td43xx(dev, attr, "95", count,
				false, false);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report 95. exit\n", __func__);
		retval = -EIO;
		g_flag_read_report_fail = 1;
		goto exit;
	}


	/* step 5 */
	/* use the upper half as part 1 image */
	/* the data should be lower than TEST_LIMIT_PART1 ( fail, if > TEST_LIMIT_PART1 ) */
	for (i = 0, offset = 0; i < tx_num * rx_num; i++) {
		tddi_rt95_part_one[i] = (signed short)(f54->report_data[offset]) |
								((signed short)(f54->report_data[offset + 1]) << 8);
		offset += 2;
	}


	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			if ((i == tx_num-1) && (f55->extended_amp_btn)) {
				break;
			}

			if (tddi_rt95_part_one[i*rx_num + j] > factory_param->tddi_extend_ee_short_test_limit_part1) {
				dev_err(f54->rmi4_data->pdev->dev.parent,
						"%s: fail at (tx%-2d, rx%-2d) = %-4d in part 1 image (limit = %d)\n",
						__func__, i, j, tddi_rt95_part_one[i*rx_num + j], factory_param->tddi_extend_ee_short_test_limit_part1);

				tddi_rt95_part_one[i*rx_num + j] = 1;
			}
			else {
				tddi_rt95_part_one[i*rx_num + j] = 0;
			}
		}
	}

	/* step 6 */
	/* use the lower half as part 2 image */
	/* and perform the calculation */
	/* the calculated data should be over than TEST_LIMIT_PART2 ( fail, if < TEST_LIMIT_PART2 ) */
	for (i = 0, offset = buffer_size; i < tx_num * rx_num; i++) {
		tddi_rt95_part_two[i] = (signed short)(f54->report_data[offset]) |
								((signed short)(f54->report_data[offset + 1]) << 8);
		offset += 2;
	}


	tddi_ratio_calculation(tddi_rt95_part_two);

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			if ((i == tx_num-1) && (f55->extended_amp_btn)) {
				break;
			}

			if (tddi_rt95_part_two[i*rx_num + j] < factory_param->tddi_extend_ee_short_test_limit_part2) {
				dev_err(f54->rmi4_data->pdev->dev.parent,
						"%s: fail at (tx%-2d, rx%-2d) = %-4d in part 2 image (limit = %d)\n",
						__func__, i, j, tddi_rt95_part_two[i*rx_num + j], factory_param->tddi_extend_ee_short_test_limit_part2);

				tddi_rt95_part_two[i*rx_num + j] = 1;
			}
			else {
				tddi_rt95_part_two[i*rx_num + j] = 0;
			}
		}
	}

	/* step 7 */
	/* filling out the g_tddi_ee_short_data_output */
	/* 1: fail / 0 : pass */
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			if ((i == tx_num-1) && (f55->extended_amp_btn)) {
				break;
			}

			td43xx_ee_short_data[i * rx_num + j] =
				(unsigned char)(tddi_rt95_part_one[i * rx_num + j]) || tddi_rt95_part_two[i * rx_num + j];
		}
	}


	/* step 8 */
	/* restore the original setting */
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_99->address,
			original_f54_ctrl99.data,
			sizeof(original_f54_ctrl99.data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to restore f54_ctrl99 data\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_182->address,
			original_f54_ctrl182.data,
			sizeof(original_f54_ctrl182.data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to restore f54_ctrl182 data\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	retval = test_do_command(COMMAND_FORCE_UPDATE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do force update in step 8\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	retval = count;

exit:
	kfree(tddi_rt95_part_one);
	kfree(tddi_rt95_part_two);

	pr_err("%s: resetting device\n", __func__);
	rmi4_data->reset_device(rmi4_data, false);
	pr_err("%s: done resetting device\n", __func__);

	return retval;
}

static ssize_t test_sysfs_tddi_extend_ee_short_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, j;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	int fail_count = 0;

	if (!td43xx_ee_short_data)
		return -EINVAL;



	if (1 == g_flag_read_report_fail) {
		kfree(td43xx_ee_short_data);
		td43xx_ee_short_data = NULL;
		return snprintf(buf, PAGE_SIZE, "\nERROR: fail to read report image\n");
	}

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			if (td43xx_ee_short_data[i * rx_num + j] != 0) {

				fail_count += 1;
			}
		}
	}

	kfree(td43xx_ee_short_data);
	td43xx_ee_short_data = NULL;

	return snprintf(buf, PAGE_SIZE, "%s\n", (fail_count == 0) ? "PASS" : "FAIL");
}

static ssize_t test_sysfs_td43xx_ee_short_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	int i, j, offset;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	signed short *td43xx_rt95_part_one = NULL;
	signed short *td43xx_rt95_part_two = NULL;
	unsigned int td43xx_rt95_report_size = tx_num * rx_num * 2;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	struct synaptics_dsx_factory_param *factory_param = rmi4_data->factory_param;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting != 1 || !factory_param)
		return -EINVAL;

	if (td43xx_ee_short_data)
		kfree(td43xx_ee_short_data);
	td43xx_ee_short_data = kzalloc(tx_num * rx_num, GFP_KERNEL);
	if (!td43xx_ee_short_data) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for td43xx_ee_short_data\n",
				__func__);
		return -ENOMEM;
	}

	g_flag_read_report_fail = 0;

	td43xx_rt95_part_one = kzalloc(td43xx_rt95_report_size, GFP_KERNEL);
	if (!td43xx_rt95_part_one) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for td43xx_rt95_part_one\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	td43xx_rt95_part_two = kzalloc(td43xx_rt95_report_size, GFP_KERNEL);
	if (!td43xx_rt95_part_two) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for td43xx_rt95_part_two\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	retval = test_sysfs_read_report_td43xx(dev, attr, "95", count,
				false, false);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report 95. exit\n",
				__func__);
		retval = -EIO;
		g_flag_read_report_fail = 1;
		goto exit;
	}
	for (i = 0, offset = 0; i < tx_num * rx_num; i++) {
		td43xx_rt95_part_one[i] = (signed short)(f54->report_data[offset]) |
								((signed short)(f54->report_data[offset + 1]) << 8);
		offset += 2;
	}

	for (i = 0; i < tx_num * rx_num; i++) {
		*(td43xx_rt95_part_one + i) =
			(*(td43xx_rt95_part_one + i) > factory_param->tddi_short_limit_b) ? 1 : 0;
	}

	for (i = 0, offset = td43xx_rt95_report_size; i < tx_num * rx_num; i++) {
		td43xx_rt95_part_two[i] = (signed short)(f54->report_data[offset]) |
								((signed short)(f54->report_data[offset + 1]) << 8);
		offset += 2;
	}

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
				td43xx_ee_short_data[i * rx_num + j] = td43xx_rt95_part_one[i * rx_num + j];
		}
	}

	retval = count;

exit:
	kfree(td43xx_rt95_part_one);
	kfree(td43xx_rt95_part_two);

	pr_err("%s: resetting device\n", __func__);
	rmi4_data->reset_device(rmi4_data, false);
	pr_err("%s: done resetting device\n", __func__);

	return retval;
}

static ssize_t test_sysfs_td43xx_ee_short_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, j;
	int tx_num = f54->tx_assigned - 1;
	int rx_num = f54->rx_assigned;
	bool result = 1;

	if (!td43xx_ee_short_data)
		return -EINVAL;


	if (1 == g_flag_read_report_fail) {
		kfree(td43xx_ee_short_data);
		td43xx_ee_short_data = NULL;
		return snprintf(buf, PAGE_SIZE, "ERROR: Fail to read RT95 image\n");
	}

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			if (td43xx_ee_short_data[i * rx_num + j] != 0) {
				result &= 0;
				dev_err(f54->rmi4_data->pdev->dev.parent, "0x%02x\n", td43xx_ee_short_data[i * rx_num + j]);
			}
		}
	}

	kfree(td43xx_ee_short_data);
	td43xx_ee_short_data = NULL;

	return snprintf(buf, PAGE_SIZE, "%s\n", (result == 1) ? "PASS" : "FAIL");
}

static ssize_t test_sysfs_td43xx_noise_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	int i, j, offset;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	int repeat;
	int times = 5;
	signed short report_data_16;
	signed short *td43xx_noise_max = NULL;
	signed short *td43xx_noise_min = NULL;
	unsigned char *td43xx_noise_data = NULL;
	unsigned int noise_report_size = tx_num * rx_num * 2;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval || (setting != 1))
		return -EINVAL;

	if (td43xx_noise_data)
		kfree(td43xx_noise_data);
	td43xx_noise_data = kzalloc(noise_report_size, GFP_KERNEL);
	if (!td43xx_noise_data) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for td43xx_noise_data\n",
				__func__);
		return -ENOMEM;
	}

	if (td43xx_noise_max)
		kfree(td43xx_noise_data);
	td43xx_noise_max = (unsigned short *)kzalloc(noise_report_size, GFP_KERNEL);
	if (!td43xx_noise_max) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for td43xx_noise_max\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	if (td43xx_noise_min)
		kfree(td43xx_noise_min);
	td43xx_noise_min = (unsigned short *) kzalloc(noise_report_size, GFP_KERNEL);
	if (!td43xx_noise_min) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for td43xx_noise_min\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	if (td43xx_noise_delta)
		kfree(td43xx_noise_delta);
	td43xx_noise_delta = (signed short *)kzalloc(noise_report_size, GFP_KERNEL);
	if (!td43xx_noise_delta) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for td43xx_noise_delta\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	for (repeat = 0; repeat < times; repeat++) {
		retval = test_sysfs_read_report_td43xx(dev, attr, "94", count,
					false, false);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to read report 94 at %d round. exit\n",
					__func__, repeat);
			retval = -EIO;
			goto exit;
		}

		memset(td43xx_noise_data, 0x00, noise_report_size);

		secure_memcpy(td43xx_noise_data, noise_report_size,
			f54->report_data, f54->report_size, f54->report_size);

		for (i = 0, offset = 0; i < tx_num; i++) {
			for (j = 0; j < rx_num; j++) {
				report_data_16 =
					(signed short)td43xx_noise_data[offset] +
					((signed short)td43xx_noise_data[offset+1] << 8);
				offset += 2;
				td43xx_noise_max[i*rx_num + j] =
					max_t(signed short,
						td43xx_noise_max[i*rx_num + j],
						report_data_16);
				td43xx_noise_min[i*rx_num + j] =
					min_t(signed short,
						td43xx_noise_min[i*rx_num + j],
						report_data_16);
			}
		}
	}

	for (i = 0; i < tx_num; i++)
		for (j = 0; j < rx_num; j++)
			td43xx_noise_delta[i*rx_num + j] =
			td43xx_noise_max[i*rx_num + j] -
			td43xx_noise_min[i*rx_num + j];
exit:
	kfree(td43xx_noise_max);
	kfree(td43xx_noise_min);
	kfree(td43xx_noise_data);

	pr_err("%s: resetting device\n", __func__);
	rmi4_data->reset_device(rmi4_data, false);
	pr_err("%s: done resetting device\n", __func__);

	return retval;
}

static ssize_t test_sysfs_td43xx_noise_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, j;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	bool result = 1;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	struct synaptics_dsx_factory_param *factory_param = rmi4_data->factory_param;
	unsigned int noise_upper_limit;

	if (!factory_param)
		return -EINVAL;

	noise_upper_limit = factory_param->tddi_noise_limit;

	if (!td43xx_noise_delta)
		return -EINVAL;

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			if (td43xx_noise_delta[i * rx_num + j] > noise_upper_limit) {
				result &= 0;
			}
		}
	}

	kfree(td43xx_noise_delta);
	td43xx_noise_delta = NULL;
	return snprintf(buf, PAGE_SIZE, "%s\n", (result == 1) ? "PASS" : "FAIL");
}

static ssize_t test_sysfs_td43xx_full_raw_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	unsigned int full_raw_report_size = tx_num * rx_num * 2;
	unsigned long setting;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting != 1)
		return -EINVAL;

	if (td43xx_full_raw_data)
		kfree(td43xx_full_raw_data);

	td43xx_full_raw_data = kzalloc(full_raw_report_size, GFP_KERNEL);
	if (!td43xx_full_raw_data) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for td43xx_full_raw_data\n",
				__func__);
		return -ENOMEM;
	}

	retval = test_sysfs_read_report_td43xx(dev, attr, "92", count,
				false, false);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report 92. exit\n",
				__func__);
		return -EIO;
	}

	secure_memcpy(td43xx_full_raw_data, full_raw_report_size,
		f54->report_data, f54->report_size, f54->report_size);

	retval = count;

	return retval;
}

static ssize_t test_sysfs_td43xx_full_raw_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned int ii;
	unsigned int jj;
	int cnt;
	int count = 0;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	unsigned short *report_data_16;

	if (!td43xx_full_raw_data)
		return -EINVAL;

	report_data_16 = (short *)td43xx_full_raw_data;
	cnt = snprintf(buf, PAGE_SIZE - count, "tx = %d\nrx = %d\n",
			tx_num, rx_num);
	buf += cnt;
	count += cnt;

	for (ii = 0; ii < tx_num; ii++) {
		for (jj = 0; jj < (rx_num - 1); jj++) {
			cnt = snprintf(buf, PAGE_SIZE - count, "%-4d ",
					*report_data_16);
			report_data_16++;
			buf += cnt;
			count += cnt;
		}
		cnt = snprintf(buf, PAGE_SIZE - count, "%-4d\n",
				*report_data_16);
		report_data_16++;
		buf += cnt;
		count += cnt;
	}


	kfree(td43xx_full_raw_data);
	td43xx_full_raw_data = NULL;

	snprintf(buf, PAGE_SIZE - count, "\n");
	count++;

	return count;
}


static int tddi_amp_open_data_testing_b7(signed short *p_image,
									unsigned char *p_result,
									bool is_b7)
{
	int retval = 0;
	int i, j;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	unsigned char left_size = f54->left_mux_size;
	unsigned char right_size = f54->right_mux_size;
	signed short *p_data_16;
	signed short *p_left_median = NULL;
	signed short *p_right_median = NULL;
	signed short *p_left_column_buf = NULL;
	signed short *p_right_column_buf = NULL;
	signed int temp;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	struct synaptics_dsx_factory_param *factory_param = rmi4_data->factory_param;

	if (!factory_param)
		return -EINVAL;

	if (!p_image) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Fail. p_image is null\n",
				__func__);
		retval = -EINVAL;
		goto exit;
	}

	if (!p_result) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Fail. p_result is null\n",
				__func__);
		retval = -EINVAL;
		goto exit;
	}

	if (f55->extended_amp_btn) {
		tx_num -= 1;
	}


	p_right_median = (signed short *) kzalloc(rx_num * sizeof(short), GFP_KERNEL);
	if (!p_right_median) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for right_median\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_left_median = (signed short *) kzalloc(rx_num * sizeof(short), GFP_KERNEL);
	if (!p_left_median) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for left_median\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_right_column_buf = (signed short *) kzalloc(right_size * rx_num * sizeof(short), GFP_KERNEL);
	if (!p_right_column_buf) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for left_column_buf\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_left_column_buf = (signed short *) kzalloc(left_size * rx_num * sizeof(short), GFP_KERNEL);
	if (!p_left_column_buf) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for left_column_buf\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	if (f54->swap_sensor_side) {

		p_data_16 = p_image;
		for (i = 0; i < rx_num; i++) {
			for (j = 0; j < left_size; j++) {
				p_left_column_buf[i * left_size + j] = p_data_16[j * rx_num + i];
			}
		}

		p_data_16 = p_image + left_size * rx_num;
		for (i = 0; i < rx_num; i++) {
			for (j = 0; j < right_size; j++) {
				p_right_column_buf[i * right_size + j] = p_data_16[j * rx_num + i];
			}
		}
	}
	else {

		p_data_16 = p_image;
		for (i = 0; i < rx_num; i++) {
			for (j = 0; j < right_size; j++) {
				p_right_column_buf[i * right_size + j] = p_data_16[j * rx_num + i];
			}
		}

		p_data_16 = p_image + right_size * rx_num;
		for (i = 0; i < rx_num; i++) {
			for (j = 0; j < left_size; j++) {
				p_left_column_buf[i * left_size + j] = p_data_16[j * rx_num + i];
			}
		}
	}


	for (i = 0; i < rx_num; i++) {
		p_left_median[i] = FindMedian(p_left_column_buf + i * left_size, left_size);
		p_right_median[i] = FindMedian(p_right_column_buf + i * right_size, right_size);
	}



	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {

			if (f54->swap_sensor_side) {

				if (i < left_size) {
					temp = (signed int) p_image[i * rx_num + j];
					temp = temp * 100 / p_left_median[j];
				} else {
					temp = (signed int) p_image[i * rx_num + j];
					temp = temp * 100 / p_right_median[j];
				}
			}
			else {

				if (i < right_size) {
					temp = (signed int) p_image[i * rx_num + j];
					temp = temp * 100 / p_right_median[j];
				} else {
					temp = (signed int) p_image[i * rx_num + j];
					temp = temp * 100 / p_left_median[j];
				}
			}

			p_result[i * rx_num + j] =  0;



			if (is_b7) {
				if ((temp < factory_param->tddi_b7_open_test_limit_phase2_lower) ||
					(temp > factory_param->tddi_b7_open_test_limit_phase2_upper)) {
						p_result[i * rx_num + j] =  1;

						pr_info("%s : phase 2 failed at (tx%-2d, rx%-2d), data = %d\n",
								__func__, i, j, temp);
				}
			}
			else {
				 if (temp < factory_param->tddi_b7_open_test_limit_phase2_lower) {
					p_result[i * rx_num + j] =  1;

					pr_info("%s : phase 2 failed at (tx%-2d, rx%-2d), data = %d\n",
							__func__, i, j, temp);
				 }
			}
		}
	}

exit:
	kfree(p_right_median);
	kfree(p_left_median);
	kfree(p_right_column_buf);
	kfree(p_left_column_buf);

	return retval;
}

static ssize_t test_sysfs_td4722_b7_amp_open_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	int i, j, k;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	struct synaptics_dsx_factory_param *factory_param = rmi4_data->factory_param;

	unsigned long setting;
	unsigned char original_data_f54_ctrl99[3] = {0x00, 0x00, 0x00};
	struct f54_control control = f54->control;
	unsigned char *p_report_data_8 = NULL;
	signed short  *p_rt92_image_1 = NULL;
	signed short  *p_rt92_image_2 = NULL;
	unsigned char *p_result_1 = NULL;
	unsigned char *p_result_2 = NULL;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting != 1 || !factory_param)
		return -EINVAL;

	if (f55->extended_amp_btn) {
		tx_num -= 1;
	}

	if (td43xx_amp_open_data)
		kfree(td43xx_amp_open_data);
	td43xx_amp_open_data = kzalloc(tx_num * rx_num, GFP_KERNEL);
	if (!td43xx_amp_open_data) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for td43xx_amp_open_data\n",
				__func__);
		return -ENOMEM;
	}


	p_report_data_8 = kzalloc(tx_num * rx_num * 2, GFP_KERNEL);
	if (!p_report_data_8) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_report_data_8\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_rt92_image_1 = kzalloc(tx_num * rx_num * sizeof(signed short), GFP_KERNEL);
	if (!p_rt92_image_1) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_rt92_image_1\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_rt92_image_2 = kzalloc(tx_num * rx_num * sizeof(signed short), GFP_KERNEL);
	if (!p_rt92_image_2) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_rt92_image_2\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_result_1 = kzalloc(tx_num * rx_num, GFP_KERNEL);
	if (!p_result_1) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_result_1\n",
				__func__);
		return -ENOMEM;
	}

	p_result_2 = kzalloc(tx_num * rx_num, GFP_KERNEL);
	if (!p_result_2) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_result_2\n",
				__func__);
		return -ENOMEM;
	}



	if (f54->query.touch_controller_family != 2) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: not support touch controller family = 0 or 1 \n",
				__func__);
		retval = -EINVAL;
		goto exit;
	}


	retval = synaptics_rmi4_reg_read(rmi4_data,
			control.reg_99->address,
			original_data_f54_ctrl99,
			sizeof(original_data_f54_ctrl99));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read integration duration\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	g_flag_read_report_fail = 0;

	/* step 1 */
	/* set the in_iter_duration_1 setting */
	/* and read the first rt92 image */
	control.reg_99->integration_duration_lsb = factory_param->tddi_b7_open_test_int_dur_one;
	control.reg_99->integration_duration_msb = (factory_param->tddi_b7_open_test_int_dur_one >> 8) & 0xff;
	control.reg_99->reset_duration = original_data_f54_ctrl99[2];
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_99->address,
			control.reg_99->data,
			sizeof(control.reg_99->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write the integration duration to f54_ctrl_99 in step 1\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	retval = test_do_command(COMMAND_FORCE_UPDATE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do force update in step 1\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	retval = test_sysfs_read_report_td43xx(dev, attr, "92", count,
				false, false);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report 92 in step 1. exit\n",
				__func__);
	  g_flag_read_report_fail = 1;
		retval = -EIO;
		goto exit;
	}

	secure_memcpy(p_report_data_8, tx_num * rx_num * 2,
		f54->report_data, f54->report_size, f54->report_size);


	k = 0;
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			p_rt92_image_1[i * rx_num + j] =
				(signed short)(p_report_data_8[k] & 0xff) | (signed short)(p_report_data_8[k + 1] << 8);

			k += 2;
		}
	}


	tddi_amp_open_data_testing_b7(p_rt92_image_1,
								p_result_1,
								true);

	memset(p_report_data_8, 0x00, tx_num * rx_num * 2);

	/* step 2 */
	/* set the in_iter_duration_2 setting */
	/* and read the second rt92 image */
	control.reg_99->integration_duration_lsb = factory_param->tddi_b7_open_test_int_dur_two;
	control.reg_99->integration_duration_msb = (factory_param->tddi_b7_open_test_int_dur_two >> 8) & 0xff;
	control.reg_99->reset_duration = original_data_f54_ctrl99[2];
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_99->address,
			control.reg_99->data,
			sizeof(control.reg_99->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write the integration duration to f54_ctrl_99 in step 2\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	retval = test_do_command(COMMAND_FORCE_UPDATE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do force update in step 2\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	retval = test_sysfs_read_report_td43xx(dev, attr, "92", count,
				false, false);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report 92 in step 2. exit\n",
				__func__);
		g_flag_read_report_fail = 1;
		retval = -EIO;
		goto exit;
	}

	secure_memcpy(p_report_data_8, tx_num * rx_num * 2,
		f54->report_data, f54->report_size, f54->report_size);


	k = 0;
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			p_rt92_image_2[i * rx_num + j] =
				(signed short)(p_report_data_8[k] & 0xff) | (signed short)(p_report_data_8[k + 1] << 8);

			k += 2;
		}
	}


	tddi_amp_open_data_testing_b7(p_rt92_image_2,
								p_result_2,
								true);

	/* restore the original settings */
	control.reg_99->integration_duration_lsb = original_data_f54_ctrl99[0];
	control.reg_99->integration_duration_msb = original_data_f54_ctrl99[1];
	control.reg_99->reset_duration = original_data_f54_ctrl99[2];
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_99->address,
			control.reg_99->data,
			sizeof(control.reg_99->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write the integration duration to f54_ctrl_99 in restore phase\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	retval = test_do_command(COMMAND_FORCE_UPDATE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do force update in restore phase\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	/* step 3 */
	/* check the result */
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {

			if ((1 == p_result_1[i * rx_num + j]) || (1 == p_result_2[i * rx_num + j]))
				td43xx_amp_open_data[i * rx_num + j] = 1;
			else
				td43xx_amp_open_data[i * rx_num + j] = 0;
		}
	}

	retval = count;


exit:

	kfree(p_rt92_image_1);
	kfree(p_rt92_image_2);
	kfree(p_report_data_8);
	kfree(p_result_1);
	kfree(p_result_2);

	pr_err("%s: resetting device\n", __func__);
	rmi4_data->reset_device(rmi4_data, false);
	pr_err("%s: done resetting device\n", __func__);

	return count;
}

static ssize_t test_sysfs_td4722_b7_amp_open_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i = 0, j = 0;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	int result = 0;

	if (f55->extended_amp_btn) {
		tx_num -= 1;
	}

	if (!td43xx_amp_open_data)
		return -EINVAL;


	if (1 == g_flag_read_report_fail) {
		kfree(td43xx_amp_open_data);
		td43xx_amp_open_data = NULL;
		return snprintf(buf, PAGE_SIZE, "ERROR: Fail to read RT92 image\n");
	}
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			if (td43xx_amp_open_data[i * rx_num + j] != 0) {

				result = 0;
				break;
			}
		}
	}

	if ((i == tx_num) && (j == rx_num))
		result = 1;

	kfree(td43xx_amp_open_data);
	td43xx_amp_open_data = NULL;

	return snprintf(buf, PAGE_SIZE, "%s\n", (result == 1) ? "PASS" : "FAIL");
}
static int tddi_amp_open_data_testing(signed short *p_image)
{
	int retval = 0;
	int i, j;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	unsigned char left_size = f54->left_mux_size;
	unsigned char right_size = f54->right_mux_size;
	signed short *p_data_16;
	signed short *p_left_median = NULL;
	signed short *p_right_median = NULL;
	signed short *p_left_column_buf = NULL;
	signed short *p_right_column_buf = NULL;
	signed int temp;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	struct synaptics_dsx_factory_param *factory_param = rmi4_data->factory_param;

	if (!factory_param)
		return -EINVAL;

	if (!p_image) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Fail. p_image is null\n",
				__func__);
		retval = -EINVAL;
		goto exit;
	}

	p_right_median = (signed short *) kzalloc(rx_num * sizeof(short), GFP_KERNEL);
	if (!p_right_median) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for right_median\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_left_median = (signed short *) kzalloc(rx_num * sizeof(short), GFP_KERNEL);
	if (!p_left_median) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for left_median\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_right_column_buf = (signed short *) kzalloc(right_size * rx_num * sizeof(short), GFP_KERNEL);
	if (!p_right_column_buf) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for left_column_buf\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_left_column_buf = (signed short *) kzalloc(left_size * rx_num * sizeof(short), GFP_KERNEL);
	if (!p_left_column_buf) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for left_column_buf\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}


	if (f54->swap_sensor_side) {
		p_data_16 = p_image;
		for (i = 0; i < rx_num; i++) {
			for (j = 0; j < left_size; j++) {
				p_left_column_buf[i * left_size + j] = p_data_16[j * rx_num + i];
			}
		}

		p_data_16 = p_image + left_size * rx_num;
		for (i = 0; i < rx_num; i++) {
			for (j = 0; j < right_size; j++) {
				p_right_column_buf[i * right_size + j] = p_data_16[j * rx_num + i];
			}
		}

		for (i = 0; i < rx_num; i++) {
			p_left_median[i] = FindMedian(p_left_column_buf + i * left_size, left_size);
			p_right_median[i] = FindMedian(p_right_column_buf + i * right_size, right_size);
		}

		for (i = 0; i < tx_num; i++) {
			for (j = 0; j < rx_num; j++) {
				if (i < left_size) {
					temp = (signed int) p_image[i * rx_num + j];
					temp = temp * 100 / p_left_median[j];
				} else {
					temp = (signed int) p_image[i * rx_num + j];
					temp = temp * 100 / p_right_median[j];
				}

				if ((temp > 0) && temp < factory_param->tddi_open_test_limit_phase2_lower) {
					td43xx_amp_open_data[i * rx_num + j] = 1;

					pr_info("%s : phase 2 failed at (tx%-2d, rx%-2d), data = %d\n",
							__func__, i, j, temp);
				} else
					td43xx_amp_open_data[i * rx_num + j] = 0;
			}
		}
	} else {
		p_data_16 = p_image;
		for (i = 0; i < rx_num; i++) {
			for (j = 0; j < right_size; j++) {
				p_right_column_buf[i * right_size + j] = p_data_16[j * rx_num + i];
			}
		}

		p_data_16 = p_image + right_size * rx_num;
		for (i = 0; i < rx_num; i++) {
			for (j = 0; j < left_size; j++) {
				p_left_column_buf[i * left_size + j] = p_data_16[j * rx_num + i];
			}
		}

		for (i = 0; i < rx_num; i++) {
			p_right_median[i] = FindMedian(p_right_column_buf + i * right_size, right_size);
			p_left_median[i] = FindMedian(p_left_column_buf + i * left_size, left_size);
		}

		for (i = 0; i < tx_num; i++) {
			for (j = 0; j < rx_num; j++) {
				if (i < right_size) {
					temp = (signed int) p_image[i * rx_num + j];
					temp = temp * 100 / p_right_median[j];
				} else {
					temp = (signed int) p_image[i * rx_num + j];
					temp = temp * 100 / p_left_median[j];
				}

				if ((temp > 0) && (temp < factory_param->tddi_open_test_limit_phase2_lower)) {
					td43xx_amp_open_data[i * rx_num + j] = 1;

					pr_info("%s : phase 2 failed at (tx%-2d, rx%-2d), data = %d\n",
						__func__, i, j, temp);
				} else
					td43xx_amp_open_data[i * rx_num + j] = 0;
			}
		}
	}

exit:
	kfree(p_right_median);
	kfree(p_left_median);
	kfree(p_right_column_buf);
	kfree(p_left_column_buf);

	return retval;
}

static ssize_t test_sysfs_td43xx_amp_open_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	int i, j, k;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	struct synaptics_dsx_factory_param *factory_param = rmi4_data->factory_param;

	unsigned long setting;
	unsigned char original_data_f54_ctrl99[3] = {0x00, 0x00, 0x00};
	struct f54_control control = f54->control;
	unsigned char *p_report_data_8 = NULL;
	signed short  *p_rt92_delta_image = NULL;
	signed short  *p_rt92_image_1 = NULL;
	signed short  *p_rt92_image_2 = NULL;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting != 1 || !factory_param)
		return -EINVAL;

	if (f55->extended_amp_btn) {
		tx_num -= 1;
	}

	if (td43xx_amp_open_data)
		kfree(td43xx_amp_open_data);
	td43xx_amp_open_data = kzalloc(tx_num * rx_num, GFP_KERNEL);
	if (!td43xx_amp_open_data) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for td43xx_amp_open_data\n",
				__func__);
		return -ENOMEM;
	}

	p_report_data_8 = kzalloc(tx_num * rx_num * 2, GFP_KERNEL);
	if (!p_report_data_8) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_report_data_8\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}
	memset(p_report_data_8, 0x00, tx_num * rx_num * 2);

	p_rt92_delta_image = kzalloc(tx_num * rx_num * sizeof(signed short), GFP_KERNEL);
	if (!p_rt92_delta_image) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_rt92_delta_image\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}
	memset(p_rt92_delta_image, 0x00, tx_num * rx_num * sizeof(signed short));

	p_rt92_image_1 = kzalloc(tx_num * rx_num * sizeof(signed short), GFP_KERNEL);
	if (!p_rt92_image_1) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_rt92_image_1\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}
	memset(p_rt92_image_1, 0x00, tx_num * rx_num * sizeof(signed short));

	p_rt92_image_2 = kzalloc(tx_num * rx_num * sizeof(signed short), GFP_KERNEL);
	if (!p_rt92_image_2) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_rt92_image_2\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}
	memset(p_rt92_image_2, 0x00, tx_num * rx_num * sizeof(signed short));

	if (f54->query.touch_controller_family != 2) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: not support touch controller family = 0 or 1 \n",
				__func__);
		retval = -EINVAL;
		goto exit;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			control.reg_99->address,
			original_data_f54_ctrl99,
			sizeof(original_data_f54_ctrl99));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read integration duration\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	control.reg_99->integration_duration_lsb = factory_param->tddi_open_test_int_dur_one;
	control.reg_99->integration_duration_msb = (factory_param->tddi_open_test_int_dur_one >> 8) & 0xff;
	control.reg_99->reset_duration = original_data_f54_ctrl99[2];
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_99->address,
			control.reg_99->data,
			sizeof(control.reg_99->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write the integration duration to f54_ctrl_99 in step 1\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	retval = test_do_command(COMMAND_FORCE_UPDATE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do force update in step 1\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	retval = test_sysfs_read_report_td43xx(dev, attr, "92", count,
				false, false);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report 92 in step 1. exit\n",
				__func__);
		g_flag_read_report_fail = 1;
		retval = -EIO;
		goto exit;
	}

	secure_memcpy(p_report_data_8, tx_num * rx_num * 2,
		f54->report_data, f54->report_size, f54->report_size);

	k = 0;
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			p_rt92_image_1[i * rx_num + j] =
				(signed short)(p_report_data_8[k] & 0xff) | (signed short)(p_report_data_8[k + 1] << 8);
			k += 2;
		}
	}

	memset(p_report_data_8, 0x00, tx_num * rx_num * 2);

	control.reg_99->integration_duration_lsb = factory_param->tddi_open_test_int_dur_two;
	control.reg_99->integration_duration_msb = (factory_param->tddi_open_test_int_dur_two >> 8) & 0xff;
	control.reg_99->reset_duration = original_data_f54_ctrl99[2];
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_99->address,
			control.reg_99->data,
			sizeof(control.reg_99->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write the integration duration to f54_ctrl_99 in step 2\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	retval = test_do_command(COMMAND_FORCE_UPDATE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do force update in step 2\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	retval = test_sysfs_read_report_td43xx(dev, attr, "92", count,
				false, false);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report 92 in step 2. exit\n",
				__func__);
		g_flag_read_report_fail = 1;
		retval = -EIO;
		goto exit;
	}

	secure_memcpy(p_report_data_8, tx_num * rx_num * 2,
		f54->report_data, f54->report_size, f54->report_size);

	k = 0;
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			p_rt92_image_2[i * rx_num + j] =
				(signed short)(p_report_data_8[k] & 0xff) | (signed short)(p_report_data_8[k + 1] << 8);

			k += 2;
		}
	}

	control.reg_99->integration_duration_lsb = original_data_f54_ctrl99[0];
	control.reg_99->integration_duration_msb = original_data_f54_ctrl99[1];
	control.reg_99->reset_duration = original_data_f54_ctrl99[2];
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_99->address,
			control.reg_99->data,
			sizeof(control.reg_99->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write the integration duration to f54_ctrl_99 in restore phase\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	retval = test_do_command(COMMAND_FORCE_UPDATE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do force update in restore pahse\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	for (i = 0; i < tx_num * rx_num; i++) {
		p_rt92_delta_image[i] = p_rt92_image_1[i] - p_rt92_image_2[i];
	}

	tddi_amp_open_data_testing(p_rt92_delta_image);

	retval = count;

exit:
	kfree(p_rt92_image_1);
	kfree(p_rt92_image_2);
	kfree(p_rt92_delta_image);
	kfree(p_report_data_8);

	pr_err("%s: resetting device\n", __func__);
	rmi4_data->reset_device(rmi4_data, false);
	pr_err("%s: done resetting device\n", __func__);

	return count;
}

static ssize_t test_sysfs_td43xx_amp_open_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, j;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	bool result = 1;

	if (!td43xx_amp_open_data)
		return -EINVAL;


	if (1 == g_flag_read_report_fail) {
		kfree(td43xx_amp_open_data);
		td43xx_amp_open_data = NULL;
		return snprintf(buf, PAGE_SIZE, "ERROR: Fail to read RT92 image\n");
	}
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			if (td43xx_amp_open_data[i * rx_num + j] != 0) {

				result &= 0;
			}
		}
	}

	kfree(td43xx_amp_open_data);
	td43xx_amp_open_data = NULL;

	return snprintf(buf, PAGE_SIZE, "%s\n", (result == 1) ? "PASS" : "FAIL");
}


static ssize_t test_sysfs_tddi_amp_electrode_open_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	int i, j, k;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	struct synaptics_dsx_factory_param *factory_param = rmi4_data->factory_param;

	struct f54_control control = f54->control;
	unsigned long setting;

	struct f54_control_91  original_f54_ctrl91;
	struct f54_control_99  original_f54_ctrl99;
	struct f54_control_182 original_f54_ctrl182;

	unsigned char *p_report_data_8 = NULL;
	signed short  *p_rt92_image_1 = NULL;
	signed short  *p_rt92_image_2 = NULL;
	signed short  *p_rt92_delta_image = NULL;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting != 1 || !factory_param)
		return -EINVAL;


	if (td43xx_amp_open_data)
		kfree(td43xx_amp_open_data);
	td43xx_amp_open_data = kzalloc(tx_num * rx_num, GFP_KERNEL);
	if (!td43xx_amp_open_data) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for td43xx_amp_open_data\n",
				__func__);
		return -ENOMEM;
	}


	p_report_data_8 = kzalloc(tx_num * rx_num * 2, GFP_KERNEL);
	if (!p_report_data_8) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_report_data_8\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_rt92_delta_image = kzalloc(tx_num * rx_num * sizeof(signed short), GFP_KERNEL);
	if (!p_rt92_delta_image) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_rt92_delta_image\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_rt92_image_1 = kzalloc(tx_num * rx_num * sizeof(signed short), GFP_KERNEL);
	if (!p_rt92_image_1) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_rt92_image_1\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_rt92_image_2 = kzalloc(tx_num * rx_num * sizeof(signed short), GFP_KERNEL);
	if (!p_rt92_image_2) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_rt92_image_2\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}


	/* keep the original reference high/low capacitance */
	retval = synaptics_rmi4_reg_read(rmi4_data,
			control.reg_91->address,
			original_f54_ctrl91.data,
			sizeof(original_f54_ctrl91));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read original data from f54_ctrl91\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	/* keep the original integration and reset duration */
	retval = synaptics_rmi4_reg_read(rmi4_data,
			control.reg_99->address,
			original_f54_ctrl99.data,
			sizeof(original_f54_ctrl99));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read original data from f54_ctrl99\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	/* keep the original timing control */
	retval = synaptics_rmi4_reg_read(rmi4_data,
			control.reg_182->address,
			original_f54_ctrl182.data,
			sizeof(original_f54_ctrl182));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read original data from f54_ctrl182\n",
				__func__);
		retval = -EIO;
		goto exit;
	}


	/* step 1 */
	/* Wide refcap hi/ lo and feedback, Write 0x0F to F54_ANALOG_CTRL91 */
	control.reg_91->reflo_transcap_capacitance = 0x0f;
	control.reg_91->refhi_transcap_capacitance = 0x0f;
	control.reg_91->receiver_feedback_capacitance = 0x0f;
	control.reg_91->reference_receiver_feedback_capacitance = original_f54_ctrl91.reference_receiver_feedback_capacitance;
	control.reg_91->gain_ctrl = original_f54_ctrl91.gain_ctrl;
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_91->address,
			control.reg_91->data,
			sizeof(control.reg_91->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set f54_ctrl91 in step 1\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	/* step 2 */
	/* Increase RST_DUR to 1.53us, Write 0x5c to F54_ANALOG_CTRL99 */
	control.reg_99->integration_duration_lsb = original_f54_ctrl99.integration_duration_lsb;
	control.reg_99->integration_duration_msb = original_f54_ctrl99.integration_duration_msb;
	control.reg_99->reset_duration = 0x5c;
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_99->address,
			control.reg_99->data,
			sizeof(control.reg_99->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set f54_ctrl99 in step 2\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	/* step 3 */
	/* Write 0x02 to F54_ANALOG_CTRL182 (00)/00 and (00)/02 */
	control.reg_182->cbc_timing_ctrl_tx_lsb = factory_param->elec_open_test_tx_on_count;
	control.reg_182->cbc_timing_ctrl_tx_msb = (factory_param->elec_open_test_tx_on_count >> 8) & 0xff;
	control.reg_182->cbc_timing_ctrl_rx_lsb = factory_param->elec_open_test_rx_on_count;
	control.reg_182->cbc_timing_ctrl_rx_msb = (factory_param->elec_open_test_rx_on_count >> 8) & 0xff;
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_182->address,
			control.reg_182->data,
			sizeof(control.reg_182->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set f54_reg_182 in step 3\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	/* step 4 */
	/* Change the INT_DUR as ELEC_OPEN_INT_DUR_ONE */
	retval = synaptics_rmi4_reg_read(rmi4_data,
			control.reg_99->address,
			control.reg_99->data,
			sizeof(control.reg_99->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read data from f54_ctrl99 in step 4\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	control.reg_99->integration_duration_lsb = factory_param->elec_open_int_dur_one;
	control.reg_99->integration_duration_msb = (factory_param->elec_open_int_dur_one >> 8) & 0xff;
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_99->address,
			control.reg_99->data,
			sizeof(control.reg_99->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to seet ELEC_OPEN_INT_DUR_ONE(%d) in step 4\n",
				__func__, factory_param->elec_open_int_dur_one);
		retval = -EIO;
		goto exit;
	}

	retval = test_do_command(COMMAND_FORCE_UPDATE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do force update in step 4\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	/* step 5 */
	/* Capture raw capacitance (rt92) image 1 */
	/* Run Report Type 92 */
	retval = test_sysfs_read_report_td43xx(dev, attr, "92", count,
				false, false);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report 92 in step 5. exit\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	secure_memcpy(p_report_data_8, tx_num * rx_num * 2,
		f54->report_data, f54->report_size, f54->report_size);

	k = 0;
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			p_rt92_image_1[i * rx_num + j] =
				(signed short)(p_report_data_8[k] & 0xff) | (signed short)(p_report_data_8[k + 1] << 8);

			k += 2;
		}
	}
	memset(p_report_data_8, 0x00, tx_num * rx_num * 2);

	/* step 6 */
	/* Change the INT_DUR into ELEC_OPEN_INT_DUR_TWO */
	retval = synaptics_rmi4_reg_read(rmi4_data,
			control.reg_99->address,
			control.reg_99->data,
			sizeof(control.reg_99->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read data from f54_ctrl99 in step 6\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	control.reg_99->integration_duration_lsb = factory_param->elec_open_int_dur_two;
	control.reg_99->integration_duration_msb = (factory_param->elec_open_int_dur_two >> 8) & 0xff;
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_99->address,
			control.reg_99->data,
			sizeof(control.reg_99->data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to seet ELEC_OPEN_INT_DUR_TWO(%d) in step 6\n",
				__func__, factory_param->elec_open_int_dur_two);
		retval = -EIO;
		goto exit;
	}

	retval = test_do_command(COMMAND_FORCE_UPDATE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do force update in step 6\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	/* step 7 */
	/* Capture raw capacitance (rt92) image 2 */
	/* Run Report Type 92 */
	retval = test_sysfs_read_report_td43xx(dev, attr, "92", count,
				false, false);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report 92 in step 7. exit\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	secure_memcpy(p_report_data_8, tx_num * rx_num * 2,
		f54->report_data, f54->report_size, f54->report_size);

	k = 0;
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			p_rt92_image_2[i * rx_num + j] =
				(signed short)(p_report_data_8[k] & 0xff) | (signed short)(p_report_data_8[k + 1] << 8);

			k += 2;
		}
	}

	/* step 8 */
	/* generate the delta image, which is equeal to image2 - image1 */
	/* unit is femtofarad (fF) */
	for (i = 0; i < tx_num * rx_num; i++) {
		p_rt92_delta_image[i] = p_rt92_image_2[i] - p_rt92_image_1[i];
	}

	/* step 9 */
	/* restore the original configuration */
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_91->address,
			original_f54_ctrl91.data,
			sizeof(original_f54_ctrl91));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to restore f54_ctrl91 data\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_99->address,
			original_f54_ctrl99.data,
			sizeof(original_f54_ctrl99));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to restore f54_ctrl99 data\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_182->address,
			original_f54_ctrl182.data,
			sizeof(original_f54_ctrl182));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to restore f54_ctrl182 data\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	retval = test_do_command(COMMAND_FORCE_UPDATE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do force update in step 6\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	memset(p_rt92_image_1, 0x00, tx_num * rx_num * 2);

	/* step 10 */
	/* phase 1, data verification */
	/* the delta value should be lower than the test limit */
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			if ((i == tx_num-1) && (f55->extended_amp_btn)) {
				break;
			}
			if (p_rt92_delta_image[i * rx_num + j] < factory_param->elec_open_test_limit_one) {

				dev_err(f54->rmi4_data->pdev->dev.parent,
						"%s: fail at (tx%-2d, rx%-2d) = %-4d at phase 1 (limit = %d)\n",
 						__func__, i, j, p_rt92_delta_image[i*rx_num + j], factory_param->elec_open_test_limit_one);

				p_rt92_image_1[i*rx_num + j] = TEST_FAILED;
			}
			else {
				p_rt92_image_1[i*rx_num + j] = TEST_OK;
			}
		}
	}

	memset(p_rt92_image_2, 0x00, tx_num * rx_num * 2);

	/* step 11 */
	/* phase 2, data calculation and verification */
	/* the calculated ratio should be lower than the test limit */


	tddi_ratio_calculation(p_rt92_delta_image);
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			if ((i == tx_num-1) && (f55->extended_amp_btn)) {
				break;
			}
			if (p_rt92_delta_image[i * rx_num + j] < factory_param->elec_open_test_limit_two) {

				dev_err(f54->rmi4_data->pdev->dev.parent,
						"%s: fail at (tx%-2d, rx%-2d) = %-4d at phase 2 (limit = %d)\n",
						__func__, i, j, p_rt92_delta_image[i*rx_num + j], factory_param->elec_open_test_limit_two);

				p_rt92_image_2[i*rx_num + j] = TEST_FAILED;
			}
			else {
				p_rt92_image_2[i*rx_num + j] = TEST_OK;
			}
		}
	}

	/* step 12 */
	/* filling out the td43xx_amp_open_data */
	/* 1: fail / 0 : pass */
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			if ((i == tx_num-1) && (f55->extended_amp_btn)) {
				break;
			}
			td43xx_amp_open_data[i * rx_num + j] =
				(unsigned char)(p_rt92_image_1[i * rx_num + j]) & p_rt92_image_2[i * rx_num + j];
		}
	}

exit:

	kfree(p_report_data_8);
	kfree(p_rt92_image_1);
	kfree(p_rt92_image_2);
	kfree(p_rt92_delta_image);

	pr_err("%s: resetting device\n", __func__);
	rmi4_data->reset_device(rmi4_data, false);
	pr_err("%s: done resetting device\n", __func__);

	return count;
}
static ssize_t test_sysfs_tddi_amp_electrode_open_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i, j;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	int fail_count = 0;

	if (!td43xx_amp_open_data)
		return -EINVAL;

	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			if ((i == tx_num-1) && (f55->extended_amp_btn)) {
				break;
			}
			if (td43xx_amp_open_data[i * rx_num + j] != TEST_OK) {
				fail_count += 1;
			}
		}
	}

	kfree(td43xx_amp_open_data);
	td43xx_amp_open_data = NULL;

	return snprintf(buf, PAGE_SIZE, "%s\n", (fail_count == 0) ? "PASS" : "FAIL");
}


static ssize_t test_sysfs_abs_0d_open_w_autoservo_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval = 0;
	int i, j, k;
	int tx_num = f54->tx_assigned;
	int rx_num = f54->rx_assigned;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	struct synaptics_dsx_factory_param *factory_param = rmi4_data->factory_param;
	struct f54_control control = f54->control;
	unsigned long setting;

	unsigned char original_data_f54_ctr223 = 0x00;
	unsigned char data = 0x00;

	signed char *p_report_data_8 = NULL;
	signed short  *p_rt92_image_1 = NULL;
	signed short  *p_rt92_image_2 = NULL;

	unsigned char button_count = rmi4_data->valid_button_count;

	if (!factory_param)
		return -EINVAL;

	button_count = factory_param->button_count;

	retval = sstrtoul(buf, 10, &setting);
	if (retval)
		return retval;

	if (setting != 1)
		return -EINVAL;

	if (button_count == 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to get rmi button (count = %d)\n",
				__func__, rmi4_data->valid_button_count);
		retval = -EINVAL;
		goto exit;
	}

	/* allocate the g_tddi_amp_open_data_output */
	if (g_abs_0d_open_data_output)
		kfree(g_abs_0d_open_data_output);
	g_abs_0d_open_data_output = kzalloc(button_count * sizeof(int), GFP_KERNEL);
	if (!g_abs_0d_open_data_output) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for g_abs_0d_open_data_output\n",
				__func__);
		return -ENOMEM;
	}

	g_flag_read_report_fail = 0;

	/* allocate the buffer */
	p_report_data_8 = kzalloc(tx_num * rx_num * 2, GFP_KERNEL);
	if (!p_report_data_8) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_report_data_8\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_rt92_image_1 = kzalloc(tx_num * rx_num * sizeof(signed short), GFP_KERNEL);
	if (!p_rt92_image_1) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_rt92_image_1\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	p_rt92_image_2 = kzalloc(tx_num * rx_num * sizeof(signed short), GFP_KERNEL);
	if (!p_rt92_image_2) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for p_rt92_image_2\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	/* step 1 */
	/* get first rt92 image */
	retval = test_sysfs_read_report_td43xx(dev, attr, "92", count,
				false, false);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report 92 in step 1\n",
				__func__);
		g_flag_read_report_fail = 1;  /* special code to indicate the failure of report reading */
		retval = -EIO;
		goto exit;
	}

	secure_memcpy(p_report_data_8, tx_num * rx_num * 2,
		f54->report_data, f54->report_size, f54->report_size);

	/* normalize the rt92 image with 16-bit */
	k = 0;
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			p_rt92_image_1[i * rx_num + j] =
				(signed short)(p_report_data_8[k] & 0xff) | (signed short)(p_report_data_8[k + 1] << 8);

			k += 2;
		}
	}

	memset(p_report_data_8, 0x00, tx_num * rx_num * 2);

	/* step 2 */
	/* read original 0d voltages */
	retval = synaptics_rmi4_reg_read(rmi4_data,
			control.reg_223->address,
			&original_data_f54_ctr223,
			sizeof(original_data_f54_ctr223));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read original 0d_voltaes in step 2\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	/* step 3 */
	/* modify 0d voltages and force update */
	data = original_data_f54_ctr223 - factory_param->abs_0d_open_factor;
	if (data < 0)
		data = 0;
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_223->address,
			&data,
			sizeof(data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to modify 0d voltages to f54_ctrl_223 in step 3\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	retval = test_do_command(COMMAND_FORCE_UPDATE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do force update in step 3\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	msleep(100);

	/* step 4 */
	/* get second rt92 image */
	retval = test_sysfs_read_report_td43xx(dev, attr, "92", count,
				false, false);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report 92 in step 4\n",
				__func__);
		g_flag_read_report_fail = 1;  /* special code to indicate the failure of report reading */
		retval = -EIO;
		goto exit;
	}

	secure_memcpy(p_report_data_8, tx_num * rx_num * 2,
		f54->report_data, f54->report_size, f54->report_size);

	/* normalize the rt92 image with 16-bit */
	k = 0;
	for (i = 0; i < tx_num; i++) {
		for (j = 0; j < rx_num; j++) {
			p_rt92_image_2[i * rx_num + j] =
				(signed short)(p_report_data_8[k] & 0xff) | (signed short)(p_report_data_8[k + 1] << 8);

			k += 2;
		}
	}

	/* step 5 */
	/* restore original 0d voltages and force update */
	retval = synaptics_rmi4_reg_write(rmi4_data,
			control.reg_223->address,
			&original_data_f54_ctr223,
			sizeof(original_data_f54_ctr223));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to restore 0d voltages to f54_ctrl_223 in step 5\n",
				__func__);
		retval = -EIO;
		goto exit;
	}
	retval = test_do_command(COMMAND_FORCE_UPDATE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to do force update in step 5\n",
				__func__);
		retval = -EIO;
		goto exit;
	}

	/* step 6 */
	/* calculation */
	for (i = 0; i < button_count; i++) {
		g_abs_0d_open_data_output[i] = abs(p_rt92_image_1[(tx_num-1)*rx_num + i] - p_rt92_image_2[(tx_num-1)*rx_num + i]);
		g_abs_0d_open_data_output[i] = (g_abs_0d_open_data_output[i] * 100)/p_rt92_image_1[(tx_num-1)*rx_num + i];
		printk(KERN_ERR "ref value is %d value1 is %d	value2 is %d\n", g_abs_0d_open_data_output[i], p_rt92_image_1[(tx_num-1)*rx_num + i], p_rt92_image_2[(tx_num-1)*rx_num + i]);
		if (g_abs_0d_open_data_output[i] < factory_param->abs_0d_open_test_limit)
			g_abs_0d_open_data_output[i] =  1; /* fail */
		else
			g_abs_0d_open_data_output[i] =  0; /* pass */
	}

exit:
	/* release resource */
	kfree(p_rt92_image_1);
	kfree(p_rt92_image_2);
	kfree(p_report_data_8);

	pr_err("%s: resetting device\n", __func__);
	rmi4_data->reset_device(rmi4_data, false);
	pr_err("%s: done resetting device\n", __func__);

	return count;
}


static ssize_t test_sysfs_abs_0d_open_w_autoservo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	bool result = 0;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	struct synaptics_dsx_factory_param *factory_param = rmi4_data->factory_param;
	unsigned char button_count = rmi4_data->valid_button_count;
	int i;

	if (!factory_param)
		return -EINVAL;

	if (!g_abs_0d_open_data_output)
		return -EINVAL;

	/* check the special code if the failure of report image reading */
	if (1 == g_flag_read_report_fail) {
		kfree(g_abs_0d_open_data_output);
		g_abs_0d_open_data_output = NULL;
		return snprintf(buf, PAGE_SIZE, "ERROR: Fail to read RT92 image\n");
	}

	button_count = factory_param->button_count;

	for (i = 0; i < button_count; i++) {
		if (g_abs_0d_open_data_output[i] != 0) {

			result = 0; /* 0: fail */
			break;
			/*dev_err(f54->rmi4_data->pdev->dev.parent,
				"%s: Failed at 0D_button_%d\n", __func__, i); */
		}
	}

	if (i == button_count)
		result = 1;

	kfree(g_abs_0d_open_data_output);
	g_abs_0d_open_data_output = NULL;
	return snprintf(buf, PAGE_SIZE, "%s\n", (result == 1) ? "PASS" : "FAIL");
}

static void test_report_work(struct work_struct *work)
{
	int retval;
	unsigned char report_index[2];
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	mutex_lock(&f54->status_mutex);

	if (f54->status != STATUS_BUSY) {
		retval = f54->status;
		goto exit;
	}

	retval = test_wait_for_command_completion();
	if (retval < 0) {
		retval = STATUS_ERROR;
		goto exit;
	}

	test_set_report_size();
	if (f54->report_size == 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Report data size = 0\n",
				__func__);
		retval = STATUS_ERROR;
		goto exit;
	}

	if (f54->data_buffer_size < f54->report_size) {
		if (f54->data_buffer_size)
			kfree(f54->report_data);
		f54->report_data = kzalloc(f54->report_size, GFP_KERNEL);
		if (!f54->report_data) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to alloc mem for data buffer\n",
					__func__);
			f54->data_buffer_size = 0;
			retval = STATUS_ERROR;
			goto exit;
		}
		f54->data_buffer_size = f54->report_size;
	}

	report_index[0] = 0;
	report_index[1] = 0;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			f54->data_base_addr + REPORT_INDEX_OFFSET,
			report_index,
			sizeof(report_index));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to write report data index\n",
				__func__);
		retval = STATUS_ERROR;
		goto exit;
	}

	msleep(2000);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			f54->data_base_addr + REPORT_DATA_OFFSET,
			f54->report_data,
			f54->report_size);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read report data\n",
				__func__);
		retval = STATUS_ERROR;
		goto exit;
	}

	pr_err("%s: f54->report_type = %d\n", __func__, f54->report_type);
	pr_err("%s: f54->report_size = %d\n", __func__, f54->report_size);

	retval = STATUS_IDLE;

exit:
	mutex_unlock(&f54->status_mutex);

	if (retval == STATUS_ERROR)
		f54->report_size = 0;

	f54->status = retval;

	return;
}

static void test_remove_sysfs(void)
{
	sysfs_remove_group(f54->sysfs_dir, &attr_group);
	kobject_put(f54->sysfs_dir);

	return;
}

static int test_set_sysfs(void)
{
	int retval;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	f54->sysfs_dir = kobject_create_and_add(SYSFS_FOLDER_NAME,
			&rmi4_data->input_dev->dev.kobj);
	if (!f54->sysfs_dir) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to create sysfs directory\n",
				__func__);
		goto exit_directory;
	}

	retval = sysfs_create_group(f54->sysfs_dir, &attr_group);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to create sysfs attributes\n",
				__func__);
		goto exit_attributes;
	}

	return 0;

exit_attributes:
	sysfs_remove_group(f54->sysfs_dir, &attr_group);
	kobject_put(f54->sysfs_dir);
exit_directory:
	return -ENODEV;
}

static void test_free_control_mem(void)
{
	struct f54_control control = f54->control;

	kfree(control.reg_7);
	kfree(control.reg_41);
	kfree(control.reg_57);
	kfree(control.reg_86);
	kfree(control.reg_88);
	kfree(control.reg_91);
	kfree(control.reg_96);
	kfree(control.reg_99);
	kfree(control.reg_110);
	kfree(control.reg_149);
	kfree(control.reg_182);
	kfree(control.reg_188);
	kfree(control.reg_223);
	kfree(control.reg_225);

	return;
}

static void test_set_data(void)
{
	unsigned short reg_addr;

	reg_addr = f54->data_base_addr + REPORT_DATA_OFFSET + 1;

	/* data 4 */
	if (f54->query.has_sense_frequency_control)
		reg_addr++;

	/* data 5 reserved */

	/* data 6 */
	if (f54->query.has_interference_metric)
		reg_addr += 2;

	/* data 7 */
	if (f54->query.has_one_byte_report_rate |
			f54->query.has_two_byte_report_rate)
		reg_addr++;
	if (f54->query.has_two_byte_report_rate)
		reg_addr++;

	/* data 8 */
	if (f54->query.has_variance_metric)
		reg_addr += 2;

	/* data 9 */
	if (f54->query.has_multi_metric_state_machine)
		reg_addr += 2;

	/* data 10 */
	if (f54->query.has_multi_metric_state_machine |
			f54->query.has_noise_state)
		reg_addr++;

	/* data 11 */
	if (f54->query.has_status)
		reg_addr++;

	/* data 12 */
	if (f54->query.has_slew_metric)
		reg_addr += 2;

	/* data 13 */
	if (f54->query.has_multi_metric_state_machine)
		reg_addr += 2;

	/* data 14 */
	if (f54->query_13.has_cidim)
		reg_addr++;

	/* data 15 */
	if (f54->query_13.has_rail_im)
		reg_addr++;

	/* data 16 */
	if (f54->query_13.has_noise_mitigation_enhancement)
		reg_addr++;

	/* data 17 */
	if (f54->query_16.has_data17)
		reg_addr++;

	/* data 18 */
	if (f54->query_21.has_query24_data18)
		reg_addr++;

	/* data 19 */
	if (f54->query_21.has_data19)
		reg_addr++;

	/* data_20 */
	if (f54->query_25.has_ctrl109)
		reg_addr++;

	/* data 21 */
	if (f54->query_27.has_data21)
		reg_addr++;

	/* data 22 */
	if (f54->query_27.has_data22)
		reg_addr++;

	/* data 23 */
	if (f54->query_29.has_data23)
		reg_addr++;

	/* data 24 */
	if (f54->query_32.has_data24)
		reg_addr++;

	/* data 25 */
	if (f54->query_35.has_data25)
		reg_addr++;

	/* data 26 */
	if (f54->query_35.has_data26)
		reg_addr++;

	/* data 27 */
	if (f54->query_46.has_data27)
		reg_addr++;

	/* data 28 */
	if (f54->query_46.has_data28)
		reg_addr++;

	/* data 29 30 reserved */

	/* data 31 */
	if (f54->query_49.has_data31) {
		f54->data_31.address = reg_addr;
		reg_addr++;
	}

	return;
}

static int test_set_controls(void)
{
	int retval;
	unsigned char length;
	unsigned char num_of_sensing_freqs;
	unsigned short reg_addr = f54->control_base_addr;
	struct f54_control *control = &f54->control;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	num_of_sensing_freqs = f54->query.number_of_sensing_frequencies;

	/* control 0 */
	reg_addr += CONTROL_0_SIZE;

	/* control 1 */
	if ((f54->query.touch_controller_family == 0) ||
			(f54->query.touch_controller_family == 1))
		reg_addr += CONTROL_1_SIZE;

	/* control 2 */
	reg_addr += CONTROL_2_SIZE;

	/* control 3 */
	if (f54->query.has_pixel_touch_threshold_adjustment)
		reg_addr += CONTROL_3_SIZE;

	/* controls 4 5 6 */
	if ((f54->query.touch_controller_family == 0) ||
			(f54->query.touch_controller_family == 1))
		reg_addr += CONTROL_4_6_SIZE;

	/* control 7 */
	if (f54->query.touch_controller_family == 1) {
		control->reg_7 = kzalloc(sizeof(*(control->reg_7)),
				GFP_KERNEL);
		if (!control->reg_7)
			goto exit_no_mem;
		control->reg_7->address = reg_addr;
		reg_addr += CONTROL_7_SIZE;
	}

	/* controls 8 9 */
	if ((f54->query.touch_controller_family == 0) ||
			(f54->query.touch_controller_family == 1))
		reg_addr += CONTROL_8_9_SIZE;

	/* control 10 */
	if (f54->query.has_interference_metric)
		reg_addr += CONTROL_10_SIZE;

	/* control 11 */
	if (f54->query.has_ctrl11)
		reg_addr += CONTROL_11_SIZE;

	/* controls 12 13 */
	if (f54->query.has_relaxation_control)
		reg_addr += CONTROL_12_13_SIZE;

	/* controls 14 15 16 */
	if (f54->query.has_sensor_assignment) {
		reg_addr += CONTROL_14_SIZE;
		reg_addr += CONTROL_15_SIZE * f54->query.num_of_rx_electrodes;
		reg_addr += CONTROL_16_SIZE * f54->query.num_of_tx_electrodes;
	}

	/* controls 17 18 19 */
	if (f54->query.has_sense_frequency_control) {
		reg_addr += CONTROL_17_SIZE * num_of_sensing_freqs;
		reg_addr += CONTROL_18_SIZE * num_of_sensing_freqs;
		reg_addr += CONTROL_19_SIZE * num_of_sensing_freqs;
	}

	/* control 20 */
	reg_addr += CONTROL_20_SIZE;

	/* control 21 */
	if (f54->query.has_sense_frequency_control)
		reg_addr += CONTROL_21_SIZE;

	/* controls 22 23 24 25 26 */
	if (f54->query.has_firmware_noise_mitigation)
		reg_addr += CONTROL_22_26_SIZE;

	/* control 27 */
	if (f54->query.has_iir_filter)
		reg_addr += CONTROL_27_SIZE;

	/* control 28 */
	if (f54->query.has_firmware_noise_mitigation)
		reg_addr += CONTROL_28_SIZE;

	/* control 29 */
	if (f54->query.has_cmn_removal)
		reg_addr += CONTROL_29_SIZE;

	/* control 30 */
	if (f54->query.has_cmn_maximum)
		reg_addr += CONTROL_30_SIZE;

	/* control 31 */
	if (f54->query.has_touch_hysteresis)
		reg_addr += CONTROL_31_SIZE;

	/* controls 32 33 34 35 */
	if (f54->query.has_edge_compensation)
		reg_addr += CONTROL_32_35_SIZE;

	/* control 36 */
	if ((f54->query.curve_compensation_mode == 1) ||
			(f54->query.curve_compensation_mode == 2)) {
		if (f54->query.curve_compensation_mode == 1) {
			length = max(f54->query.num_of_rx_electrodes,
					f54->query.num_of_tx_electrodes);
		} else if (f54->query.curve_compensation_mode == 2) {
			length = f54->query.num_of_rx_electrodes;
		}
		reg_addr += CONTROL_36_SIZE * length;
	}

	/* control 37 */
	if (f54->query.curve_compensation_mode == 2)
		reg_addr += CONTROL_37_SIZE * f54->query.num_of_tx_electrodes;

	/* controls 38 39 40 */
	if (f54->query.has_per_frequency_noise_control) {
		reg_addr += CONTROL_38_SIZE * num_of_sensing_freqs;
		reg_addr += CONTROL_39_SIZE * num_of_sensing_freqs;
		reg_addr += CONTROL_40_SIZE * num_of_sensing_freqs;
	}

	/* control 41 */
	if (f54->query.has_signal_clarity) {
		control->reg_41 = kzalloc(sizeof(*(control->reg_41)),
				GFP_KERNEL);
		if (!control->reg_41)
			goto exit_no_mem;
		control->reg_41->address = reg_addr;
		reg_addr += CONTROL_41_SIZE;
	}

	/* control 42 */
	if (f54->query.has_variance_metric)
		reg_addr += CONTROL_42_SIZE;

	/* controls 43 44 45 46 47 48 49 50 51 52 53 54 */
	if (f54->query.has_multi_metric_state_machine)
		reg_addr += CONTROL_43_54_SIZE;

	/* controls 55 56 */
	if (f54->query.has_0d_relaxation_control)
		reg_addr += CONTROL_55_56_SIZE;

	/* control 57 */
	if (f54->query.has_0d_acquisition_control) {
		control->reg_57 = kzalloc(sizeof(*(control->reg_57)),
				GFP_KERNEL);
		if (!control->reg_57)
			goto exit_no_mem;
		control->reg_57->address = reg_addr;
		reg_addr += CONTROL_57_SIZE;
	}

	/* control 58 */
	if (f54->query.has_0d_acquisition_control)
		reg_addr += CONTROL_58_SIZE;

	/* control 59 */
	if (f54->query.has_h_blank)
		reg_addr += CONTROL_59_SIZE;

	/* controls 60 61 62 */
	if ((f54->query.has_h_blank) ||
			(f54->query.has_v_blank) ||
			(f54->query.has_long_h_blank))
		reg_addr += CONTROL_60_62_SIZE;

	/* control 63 */
	if ((f54->query.has_h_blank) ||
			(f54->query.has_v_blank) ||
			(f54->query.has_long_h_blank) ||
			(f54->query.has_slew_metric) ||
			(f54->query.has_slew_option) ||
			(f54->query.has_noise_mitigation2))
		reg_addr += CONTROL_63_SIZE;

	/* controls 64 65 66 67 */
	if (f54->query.has_h_blank)
		reg_addr += CONTROL_64_67_SIZE * 7;
	else if ((f54->query.has_v_blank) ||
			(f54->query.has_long_h_blank))
		reg_addr += CONTROL_64_67_SIZE;

	/* controls 68 69 70 71 72 73 */
	if ((f54->query.has_h_blank) ||
			(f54->query.has_v_blank) ||
			(f54->query.has_long_h_blank))
		reg_addr += CONTROL_68_73_SIZE;

	/* control 74 */
	if (f54->query.has_slew_metric)
		reg_addr += CONTROL_74_SIZE;

	/* control 75 */
	if (f54->query.has_enhanced_stretch)
		reg_addr += CONTROL_75_SIZE * num_of_sensing_freqs;

	/* control 76 */
	if (f54->query.has_startup_fast_relaxation)
		reg_addr += CONTROL_76_SIZE;

	/* controls 77 78 */
	if (f54->query.has_esd_control)
		reg_addr += CONTROL_77_78_SIZE;

	/* controls 79 80 81 82 83 */
	if (f54->query.has_noise_mitigation2)
		reg_addr += CONTROL_79_83_SIZE;

	/* controls 84 85 */
	if (f54->query.has_energy_ratio_relaxation)
		reg_addr += CONTROL_84_85_SIZE;

	/* control 86 */
	if (f54->query_13.has_ctrl86) {
		control->reg_86 = kzalloc(sizeof(*(control->reg_86)),
				GFP_KERNEL);
		if (!control->reg_86)
			goto exit_no_mem;
		control->reg_86->address = reg_addr;
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->control.reg_86->address,
				f54->control.reg_86->data,
				sizeof(f54->control.reg_86->data));
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to read sense display ratio\n",
					__func__);
			return retval;
		}
		reg_addr += CONTROL_86_SIZE;
	}

	/* control 87 */
	if (f54->query_13.has_ctrl87)
		reg_addr += CONTROL_87_SIZE;

	/* control 88 */
	if (f54->query.has_ctrl88) {
		control->reg_88 = kzalloc(sizeof(*(control->reg_88)),
				GFP_KERNEL);
		if (!control->reg_88)
			goto exit_no_mem;
		control->reg_88->address = reg_addr;
		reg_addr += CONTROL_88_SIZE;
	}

	/* control 89 */
	if (f54->query_13.has_cidim ||
			f54->query_13.has_noise_mitigation_enhancement ||
			f54->query_13.has_rail_im)
		reg_addr += CONTROL_89_SIZE;

	/* control 90 */
	if (f54->query_15.has_ctrl90)
		reg_addr += CONTROL_90_SIZE;

	/* control 91 */
	if (f54->query_21.has_ctrl91) {
		/* td43xx start */
		control->reg_91 = kzalloc(sizeof(*(control->reg_91)),
				GFP_KERNEL);
		if (!control->reg_91)
			goto exit_no_mem;
		control->reg_91->address = reg_addr;
		/* td43xx end */
		reg_addr += CONTROL_91_SIZE;
	}

	/* control 92 */
	if (f54->query_16.has_ctrl92)
		reg_addr += CONTROL_92_SIZE;

	/* control 93 */
	if (f54->query_16.has_ctrl93)
		reg_addr += CONTROL_93_SIZE;

	/* control 94 */
	if (f54->query_16.has_ctrl94_query18)
		reg_addr += CONTROL_94_SIZE;

	/* control 95 */
	if (f54->query_16.has_ctrl95_query19)
		reg_addr += CONTROL_95_SIZE;

	/* control 96 */
	if (f54->query_21.has_ctrl96) {
		/* td43xx start */
		control->reg_96 = kzalloc(sizeof(*(control->reg_96)),
				GFP_KERNEL);
		if (!control->reg_96)
			goto exit_no_mem;
		control->reg_96->address = reg_addr;
		/* td43xx end */
		reg_addr += CONTROL_96_SIZE;
	}

	/* control 97 */
	if (f54->query_21.has_ctrl97)
		reg_addr += CONTROL_97_SIZE;

	/* control 98 */
	if (f54->query_21.has_ctrl98)
		reg_addr += CONTROL_98_SIZE;

	/* control 99 */
	if (f54->query.touch_controller_family == 2) {
		/* td43xx start */
		control->reg_99 = kzalloc(sizeof(*(control->reg_99)),
				GFP_KERNEL);
		if (!control->reg_99)
			goto exit_no_mem;
		control->reg_99->address = reg_addr;
		/* td43xx end */
		reg_addr += CONTROL_99_SIZE;
	}

	/* control 100 */
	if (f54->query_16.has_ctrl100)
		reg_addr += CONTROL_100_SIZE;

	/* control 101 */
	if (f54->query_22.has_ctrl101)
		reg_addr += CONTROL_101_SIZE;


	/* control 102 */
	if (f54->query_23.has_ctrl102)
		reg_addr += CONTROL_102_SIZE;

	/* control 103 */
	if (f54->query_22.has_ctrl103_query26) {
		f54->skip_preparation = true;
		reg_addr += CONTROL_103_SIZE;
	}

	/* control 104 */
	if (f54->query_22.has_ctrl104)
		reg_addr += CONTROL_104_SIZE;

	/* control 105 */
	if (f54->query_22.has_ctrl105)
		reg_addr += CONTROL_105_SIZE;

	/* control 106 */
	if (f54->query_25.has_ctrl106)
		reg_addr += CONTROL_106_SIZE;

	/* control 107 */
	if (f54->query_25.has_ctrl107)
		reg_addr += CONTROL_107_SIZE;

	/* control 108 */
	if (f54->query_25.has_ctrl108)
		reg_addr += CONTROL_108_SIZE;

	/* control 109 */
	if (f54->query_25.has_ctrl109)
		reg_addr += CONTROL_109_SIZE;

	/* control 110 */
	if (f54->query_27.has_ctrl110) {
		control->reg_110 = kzalloc(sizeof(*(control->reg_110)),
				GFP_KERNEL);
		if (!control->reg_110)
			goto exit_no_mem;
		control->reg_110->address = reg_addr;
		reg_addr += CONTROL_110_SIZE;
	}

	/* control 111 */
	if (f54->query_27.has_ctrl111)
		reg_addr += CONTROL_111_SIZE;

	/* control 112 */
	if (f54->query_27.has_ctrl112)
		reg_addr += CONTROL_112_SIZE;

	/* control 113 */
	if (f54->query_27.has_ctrl113)
		reg_addr += CONTROL_113_SIZE;

	/* control 114 */
	if (f54->query_27.has_ctrl114)
		reg_addr += CONTROL_114_SIZE;

	/* control 115 */
	if (f54->query_29.has_ctrl115)
		reg_addr += CONTROL_115_SIZE;

	/* control 116 */
	if (f54->query_29.has_ctrl116)
		reg_addr += CONTROL_116_SIZE;

	/* control 117 */
	if (f54->query_29.has_ctrl117)
		reg_addr += CONTROL_117_SIZE;

	/* control 118 */
	if (f54->query_30.has_ctrl118)
		reg_addr += CONTROL_118_SIZE;

	/* control 119 */
	if (f54->query_30.has_ctrl119)
		reg_addr += CONTROL_119_SIZE;

	/* control 120 */
	if (f54->query_30.has_ctrl120)
		reg_addr += CONTROL_120_SIZE;

	/* control 121 */
	if (f54->query_30.has_ctrl121)
		reg_addr += CONTROL_121_SIZE;

	/* control 122 */
	if (f54->query_30.has_ctrl122_query31)
		reg_addr += CONTROL_122_SIZE;

	/* control 123 */
	if (f54->query_30.has_ctrl123)
		reg_addr += CONTROL_123_SIZE;

	/* control 124 */
	if (f54->query_30.has_ctrl124)
		reg_addr += CONTROL_124_SIZE;

	/* control 125 */
	if (f54->query_32.has_ctrl125)
		reg_addr += CONTROL_125_SIZE;

	/* control 126 */
	if (f54->query_32.has_ctrl126)
		reg_addr += CONTROL_126_SIZE;

	/* control 127 */
	if (f54->query_32.has_ctrl127)
		reg_addr += CONTROL_127_SIZE;

	/* control 128 */
	if (f54->query_33.has_ctrl128)
		reg_addr += CONTROL_128_SIZE;

	/* control 129 */
	if (f54->query_33.has_ctrl129)
		reg_addr += CONTROL_129_SIZE;

	/* control 130 */
	if (f54->query_33.has_ctrl130)
		reg_addr += CONTROL_130_SIZE;

	/* control 131 */
	if (f54->query_33.has_ctrl131)
		reg_addr += CONTROL_131_SIZE;

	/* control 132 */
	if (f54->query_33.has_ctrl132)
		reg_addr += CONTROL_132_SIZE;

	/* control 133 */
	if (f54->query_33.has_ctrl133)
		reg_addr += CONTROL_133_SIZE;

	/* control 134 */
	if (f54->query_33.has_ctrl134)
		reg_addr += CONTROL_134_SIZE;

	/* control 135 */
	if (f54->query_35.has_ctrl135)
		reg_addr += CONTROL_135_SIZE;

	/* control 136 */
	if (f54->query_35.has_ctrl136)
		reg_addr += CONTROL_136_SIZE;

	/* control 137 */
	if (f54->query_35.has_ctrl137)
		reg_addr += CONTROL_137_SIZE;

	/* control 138 */
	if (f54->query_35.has_ctrl138)
		reg_addr += CONTROL_138_SIZE;

	/* control 139 */
	if (f54->query_35.has_ctrl139)
		reg_addr += CONTROL_139_SIZE;

	/* control 140 */
	if (f54->query_35.has_ctrl140)
		reg_addr += CONTROL_140_SIZE;

	/* control 141 */
	if (f54->query_36.has_ctrl141)
		reg_addr += CONTROL_141_SIZE;

	/* control 142 */
	if (f54->query_36.has_ctrl142)
		reg_addr += CONTROL_142_SIZE;

	/* control 143 */
	if (f54->query_36.has_ctrl143)
		reg_addr += CONTROL_143_SIZE;

	/* control 144 */
	if (f54->query_36.has_ctrl144)
		reg_addr += CONTROL_144_SIZE;

	/* control 145 */
	if (f54->query_36.has_ctrl145)
		reg_addr += CONTROL_145_SIZE;

	/* control 146 */
	if (f54->query_36.has_ctrl146)
		reg_addr += CONTROL_146_SIZE;

	/* control 147 */
	if (f54->query_38.has_ctrl147)
		reg_addr += CONTROL_147_SIZE;

	/* control 148 */
	if (f54->query_38.has_ctrl148)
		reg_addr += CONTROL_148_SIZE;

	/* control 149 */
	if (f54->query_38.has_ctrl149) {
		control->reg_149 = kzalloc(sizeof(*(control->reg_149)),
				GFP_KERNEL);
		if (!control->reg_149)
			goto exit_no_mem;
		control->reg_149->address = reg_addr;
		reg_addr += CONTROL_149_SIZE;
	}

	/* control 150 */
	if (f54->query_38.has_ctrl150)
		reg_addr += CONTROL_150_SIZE;

	/* control 151 */
	if (f54->query_38.has_ctrl151)
		reg_addr += CONTROL_151_SIZE;

	/* control 152 */
	if (f54->query_38.has_ctrl152)
		reg_addr += CONTROL_152_SIZE;

	/* control 153 */
	if (f54->query_38.has_ctrl153)
		reg_addr += CONTROL_153_SIZE;

	/* control 154 */
	if (f54->query_39.has_ctrl154)
		reg_addr += CONTROL_154_SIZE;

	/* control 155 */
	if (f54->query_39.has_ctrl155)
		reg_addr += CONTROL_155_SIZE;

	/* control 156 */
	if (f54->query_39.has_ctrl156)
		reg_addr += CONTROL_156_SIZE;

	/* controls 157 158 */
	if (f54->query_39.has_ctrl157_ctrl158)
		reg_addr += CONTROL_157_158_SIZE;

	/* controls 159 to 162 reserved */

	/* control 163 */
	if (f54->query_40.has_ctrl163_query41)
		reg_addr += CONTROL_163_SIZE;

	/* control 164 */
	if (f54->query_40.has_ctrl164)
		reg_addr += CONTROL_164_SIZE;

	/* control 165 */
	if (f54->query_40.has_ctrl165_query42)
		reg_addr += CONTROL_165_SIZE;

	/* control 166 */
	if (f54->query_40.has_ctrl166)
		reg_addr += CONTROL_166_SIZE;

	/* control 167 */
	if (f54->query_40.has_ctrl167)
		reg_addr += CONTROL_167_SIZE;

	/* control 168 */
	if (f54->query_40.has_ctrl168)
		reg_addr += CONTROL_168_SIZE;

	/* control 169 */
	if (f54->query_40.has_ctrl169)
		reg_addr += CONTROL_169_SIZE;

	/* control 170 reserved */

	/* control 171 */
	if (f54->query_43.has_ctrl171)
		reg_addr += CONTROL_171_SIZE;

	/* control 172 */
	if (f54->query_43.has_ctrl172_query44_query45)
		reg_addr += CONTROL_172_SIZE;

	/* control 173 */
	if (f54->query_43.has_ctrl173)
		reg_addr += CONTROL_173_SIZE;

	/* control 174 */
	if (f54->query_43.has_ctrl174)
		reg_addr += CONTROL_174_SIZE;

	/* control 175 */
	if (f54->query_43.has_ctrl175)
		reg_addr += CONTROL_175_SIZE;

	/* control 176 */
	if (f54->query_46.has_ctrl176)
		reg_addr += CONTROL_176_SIZE;

	/* controls 177 178 */
	if (f54->query_46.has_ctrl177_ctrl178)
		reg_addr += CONTROL_177_178_SIZE;

	/* control 179 */
	if (f54->query_46.has_ctrl179)
		reg_addr += CONTROL_179_SIZE;

	/* controls 180 to 181 reserved */

	/* control 182 */
	if (f54->query_47.has_ctrl182){
		control->reg_182 = kzalloc(sizeof(*(control->reg_182)),
				GFP_KERNEL);
		if (!control->reg_182)
			goto exit_no_mem;
		control->reg_182->address = reg_addr;
		reg_addr += CONTROL_182_SIZE;
	}

	/* control 183 */
	if (f54->query_47.has_ctrl183)
		reg_addr += CONTROL_183_SIZE;

	/* control 184 reserved */

	/* control 185 */
	if (f54->query_47.has_ctrl185)
		reg_addr += CONTROL_185_SIZE;

	/* control 186 */
	if (f54->query_47.has_ctrl186)
		reg_addr += CONTROL_186_SIZE;

	/* control 187 */
	if (f54->query_47.has_ctrl187)
		reg_addr += CONTROL_187_SIZE;

	/* control 188 */
	if (f54->query_49.has_ctrl188) {
		control->reg_188 = kzalloc(sizeof(*(control->reg_188)),
				GFP_KERNEL);
		if (!control->reg_188)
			goto exit_no_mem;
		control->reg_188->address = reg_addr;
		reg_addr += CONTROL_188_SIZE;
	}

	/* control 189 */
	if (f54->query_49.has_ctrl189)
		reg_addr += CONTROL_189_SIZE;

	/* control 190 */
	if (f54->query_49.has_ctrl190)
		reg_addr += CONTROL_190_SIZE;

	/* control 191 */
	if (f54->query_50.has_ctrl191)
		reg_addr += CONTROL_191_SIZE;

	/* control 192 */
	if (f54->query_50.has_ctrl192)
		reg_addr += CONTROL_192_SIZE;

	/* control 193 reserved */

	/* control 194 */
	if (f54->query_50.has_ctrl194)
		reg_addr += CONTROL_194_SIZE;

	/* control 195 */
	if (f54->query_50.has_ctrl195)
		reg_addr += CONTROL_195_SIZE;

	/* control 196 */
	if (f54->query_51.has_ctrl196)
		reg_addr += CONTROL_196_SIZE;

	/* controls 197 to 205 reserved */

	/* control 206 */
	if (f54->query_57.has_ctrl206)
		reg_addr += CONTROL_206_SIZE;

	/* control 207 */
	if (f54->query_57.has_ctrl207)
		reg_addr += CONTROL_207_SIZE;

	/* control 208 reserved */

	/* control 209 */
	if (f54->query_57.has_ctrl209)
		reg_addr += CONTROL_209_SIZE;

	/* control 210 */
	if (f54->query_57.has_ctrl210)
		reg_addr += CONTROL_210_SIZE;

	/* controls 211 to 213 reserved */

	/* control 214 */
	if (f54->query_61.has_ctrl214)
		reg_addr += CONTROL_214_SIZE;

	/* control 215 */
	if (f54->query_61.has_ctrl215_query62_query63)
		reg_addr += CONTROL_215_SIZE;

	/* control 217 reserved */

	/* control 218 */
	if (f54->query_61.has_ctrl218)
		reg_addr += CONTROL_218_SIZE;

	/* control 219 - 221 reserved */

	/* control 222 */
	if (f54->query_64.has_ctrl222)
		reg_addr += CONTROL_222_SIZE;

	/* control 223 */
	if (f54->query_64.has_ctrl103_sub3) {
		control->reg_223 = kzalloc(sizeof(*(control->reg_223)),
				GFP_KERNEL);
		if (!control->reg_223)
			goto exit_no_mem;
		control->reg_223->address = reg_addr;
		reg_addr += CONTROL_223_SIZE;
	}

	/* control 224 reserved */

	/* control 225 */
	if (f54->query_65.has_ctrl225) {
		control->reg_225 = kzalloc(sizeof(*(control->reg_225)),
				GFP_KERNEL);
		if (!control->reg_225)
			goto exit_no_mem;
		control->reg_225->address = reg_addr;
		reg_addr += CONTROL_225_SIZE;
	}

	return 0;

exit_no_mem:
	dev_err(rmi4_data->pdev->dev.parent,
			"%s: Failed to alloc mem for control registers\n",
			__func__);
	return -ENOMEM;
}

static int test_set_queries(void)
{
	int retval;
	unsigned char offset;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			f54->query_base_addr,
			f54->query.data,
			sizeof(f54->query.data));
	if (retval < 0)
		return retval;

	offset = sizeof(f54->query.data);

	/* query 12 */
	if (f54->query.has_sense_frequency_control == 0)
		offset -= 1;

	/* query 13 */
	if (f54->query.has_query13) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_13.data,
				sizeof(f54->query_13.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 14 */
	if (f54->query_13.has_ctrl87)
		offset += 1;

	/* query 15 */
	if (f54->query.has_query15) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_15.data,
				sizeof(f54->query_15.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 16 */
	if (f54->query_15.has_query16) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_16.data,
				sizeof(f54->query_16.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 17 */
	if (f54->query_16.has_query17)
		offset += 1;

	/* query 18 */
	if (f54->query_16.has_ctrl94_query18)
		offset += 1;

	/* query 19 */
	if (f54->query_16.has_ctrl95_query19)
		offset += 1;

	/* query 20 */
	if (f54->query_15.has_query20)
		offset += 1;

	/* query 21 */
	if (f54->query_15.has_query21) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_21.data,
				sizeof(f54->query_21.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 22 */
	if (f54->query_15.has_query22) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_22.data,
				sizeof(f54->query_22.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 23 */
	if (f54->query_22.has_query23) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_23.data,
				sizeof(f54->query_23.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 24 */
	if (f54->query_21.has_query24_data18)
		offset += 1;

	/* query 25 */
	if (f54->query_15.has_query25) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_25.data,
				sizeof(f54->query_25.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 26 */
	if (f54->query_22.has_ctrl103_query26)
		offset += 1;

	/* query 27 */
	if (f54->query_25.has_query27) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_27.data,
				sizeof(f54->query_27.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 28 */
	if (f54->query_22.has_query28)
		offset += 1;

	/* query 29 */
	if (f54->query_27.has_query29) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_29.data,
				sizeof(f54->query_29.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 30 */
	if (f54->query_29.has_query30) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_30.data,
				sizeof(f54->query_30.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 31 */
	if (f54->query_30.has_ctrl122_query31)
		offset += 1;

	/* query 32 */
	if (f54->query_30.has_query32) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_32.data,
				sizeof(f54->query_32.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 33 */
	if (f54->query_32.has_query33) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_33.data,
				sizeof(f54->query_33.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 34 */
	if (f54->query_32.has_query34)
		offset += 1;

	/* query 35 */
	if (f54->query_32.has_query35) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_35.data,
				sizeof(f54->query_35.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 36 */
	if (f54->query_33.has_query36) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_36.data,
				sizeof(f54->query_36.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 37 */
	if (f54->query_36.has_query37)
		offset += 1;

	/* query 38 */
	if (f54->query_36.has_query38) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_38.data,
				sizeof(f54->query_38.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 39 */
	if (f54->query_38.has_query39) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_39.data,
				sizeof(f54->query_39.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 40 */
	if (f54->query_39.has_query40) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_40.data,
				sizeof(f54->query_40.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 41 */
	if (f54->query_40.has_ctrl163_query41)
		offset += 1;

	/* query 42 */
	if (f54->query_40.has_ctrl165_query42)
		offset += 1;

	/* query 43 */
	if (f54->query_40.has_query43) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_43.data,
				sizeof(f54->query_43.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* queries 44 45 */
	if (f54->query_43.has_ctrl172_query44_query45)
		offset += 2;

	/* query 46 */
	if (f54->query_43.has_query46) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_46.data,
				sizeof(f54->query_46.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 47 */
	if (f54->query_46.has_query47) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_47.data,
				sizeof(f54->query_47.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 48 reserved */

	/* query 49 */
	if (f54->query_47.has_query49) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_49.data,
				sizeof(f54->query_49.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 50 */
	if (f54->query_49.has_query50) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_50.data,
				sizeof(f54->query_50.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 51 */
	if (f54->query_50.has_query51) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_51.data,
				sizeof(f54->query_51.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 52 reserved */

	/* queries 53 54 */
	if (f54->query_51.has_query53_query54_ctrl198)
		offset += 2;

	/* query 55 */
	if (f54->query_51.has_query55) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_55.data,
				sizeof(f54->query_55.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* queries 56 */
	if (f54->query_55.has_query56)
		offset += 1;

	/* query 57 */
	if (f54->query_55.has_query57) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_57.data,
				sizeof(f54->query_57.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 58 */
	if (f54->query_57.has_query58) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_58.data,
				sizeof(f54->query_58.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* queries 59 */
	if (f54->query_58.has_query59)
		offset += 1;

	/* queries 60 */
	if (f54->query_58.has_query60)
		offset += 1;

	/* queries 61 */
	if (f54->query_58.has_query61) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_61.data,
				sizeof(f54->query_61.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* queries 62 63 */
	if (f54->query_61.has_ctrl215_query62_query63)
		offset += 2;

	/* queries 64 */
	if (f54->query_61.has_query64) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_64.data,
				sizeof(f54->query_64.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* queries 65 */
	if (f54->query_64.has_query65) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_65.data,
				sizeof(f54->query_65.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* queries 66 */
	if (f54->query_65.has_query66_ctrl231)
		offset += 1;

	/* queries 67 */
	if (f54->query_65.has_query67) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_67.data,
				sizeof(f54->query_67.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* queries 68 */
	if (f54->query_67.has_query68) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_68.data,
				sizeof(f54->query_68.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* queries 69 */
	if (f54->query_68.has_query69) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f54->query_base_addr + offset,
				f54->query_69.data,
				sizeof(f54->query_69.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	return 0;
}

static void test_f54_set_regs(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count,
		unsigned char page)
{
	unsigned char ii;
	unsigned char intr_offset;

	f54->query_base_addr = fd->query_base_addr | (page << 8);
	f54->control_base_addr = fd->ctrl_base_addr | (page << 8);
	f54->data_base_addr = fd->data_base_addr | (page << 8);
	f54->command_base_addr = fd->cmd_base_addr | (page << 8);

	f54->intr_reg_num = (intr_count + 7) / 8;
	if (f54->intr_reg_num != 0)
		f54->intr_reg_num -= 1;

	f54->intr_mask = 0;
	intr_offset = intr_count % 8;
	for (ii = intr_offset;
			ii < (fd->intr_src_count + intr_offset);
			ii++) {
		f54->intr_mask |= 1 << ii;
	}

	return;
}

static int test_f55_set_controls(void)
{
	unsigned char offset = 0;

	/* controls 0 1 2 */
	if (f55->query.has_sensor_assignment)
		offset += 3;

	/* control 3 */
	if (f55->query.has_edge_compensation)
		offset++;

	/* control 4 */
	if (f55->query.curve_compensation_mode == 0x1 ||
			f55->query.curve_compensation_mode == 0x2)
		offset++;

	/* control 5 */
	if (f55->query.curve_compensation_mode == 0x2)
		offset++;

	/* control 6 */
	if (f55->query.has_ctrl6)
		offset++;

	/* control 7 */
	if (f55->query.has_alternate_transmitter_assignment)
		offset++;

	/* control 8 */
	if (f55->query_3.has_ctrl8)
		offset++;

	/* control 9 */
	if (f55->query_3.has_ctrl9)
		offset++;

	/* control 10 */
	if (f55->query_5.has_corner_compensation)
		offset++;

	/* control 11 */
	if (f55->query.curve_compensation_mode == 0x3)
		offset++;

	/* control 12 */
	if (f55->query_5.has_ctrl12)
		offset++;

	/* control 13 */
	if (f55->query_5.has_ctrl13)
		offset++;

	/* control 14 */
	if (f55->query_5.has_ctrl14)
		offset++;

	/* control 15 */
	if (f55->query_5.has_basis_function)
		offset++;

	/* control 16 */
	if (f55->query_17.has_ctrl16)
		offset++;

	/* control 17 */
	if (f55->query_17.has_ctrl17)
		offset++;

	/* controls 18 19 */
	if (f55->query_17.has_ctrl18_ctrl19)
		offset += 2;

	/* control 20 */
	if (f55->query_17.has_ctrl20)
		offset++;

	/* control 21 */
	if (f55->query_17.has_ctrl21)
		offset++;

	/* control 22 */
	if (f55->query_17.has_ctrl22)
		offset++;

	/* control 23 */
	if (f55->query_18.has_ctrl23)
		offset++;

	/* control 24 */
	if (f55->query_18.has_ctrl24)
		offset++;

	/* control 25 */
	if (f55->query_18.has_ctrl25)
		offset++;

	/* control 26 */
	if (f55->query_18.has_ctrl26)
		offset++;

	/* control 27 */
	if (f55->query_18.has_ctrl27_query20)
		offset++;

	/* control 28 */
	if (f55->query_18.has_ctrl28_query21)
		offset++;

	/* control 29 */
	if (f55->query_22.has_ctrl29)
		offset++;

	/* control 30 */
	if (f55->query_22.has_ctrl30)
		offset++;

	/* control 31 */
	if (f55->query_22.has_ctrl31)
		offset++;

	/* control 32 */
	if (f55->query_22.has_ctrl32)
		offset++;

	/* controls 33 34 35 36 reserved */

	/* control 37 */
	if (f55->query_28.has_ctrl37)
		offset++;

	/* control 38 */
	if (f55->query_30.has_ctrl38)
		offset++;

	/* control 39 */
	if (f55->query_30.has_ctrl39)
		offset++;

	/* control 40 */
	if (f55->query_30.has_ctrl40)
		offset++;

	/* control 41 */
	if (f55->query_30.has_ctrl41)
		offset++;

	/* control 42 */
	if (f55->query_30.has_ctrl42)
		offset++;

	/* controls 43 44 */
	if (f55->query_30.has_ctrl43_ctrl44) {
		f55->afe_mux_offset = offset;
		offset += 2;
	}

	/* controls 45 46 */
	if (f55->query_33.has_ctrl45_ctrl46) {
		f55->has_force = true;
		f55->force_tx_offset = offset;
		f55->force_rx_offset = offset + 1;
		offset += 2;
	}

	return 0;
}

static int test_f55_set_queries(void)
{
	int retval;
	unsigned char offset;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			f55->query_base_addr,
			f55->query.data,
			sizeof(f55->query.data));
	if (retval < 0)
		return retval;

	offset = sizeof(f55->query.data);

	/* query 3 */
	if (f55->query.has_single_layer_multi_touch) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f55->query_base_addr + offset,
				f55->query_3.data,
				sizeof(f55->query_3.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 4 */
	if (f55->query_3.has_ctrl9)
		offset += 1;

	/* query 5 */
	if (f55->query.has_query5) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f55->query_base_addr + offset,
				f55->query_5.data,
				sizeof(f55->query_5.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* queries 6 7 */
	if (f55->query.curve_compensation_mode == 0x3)
		offset += 2;

	/* query 8 */
	if (f55->query_3.has_ctrl8)
		offset += 1;

	/* query 9 */
	if (f55->query_3.has_query9)
		offset += 1;

	/* queries 10 11 12 13 14 15 16 */
	if (f55->query_5.has_basis_function)
		offset += 7;

	/* query 17 */
	if (f55->query_5.has_query17) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f55->query_base_addr + offset,
				f55->query_17.data,
				sizeof(f55->query_17.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 18 */
	if (f55->query_17.has_query18) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f55->query_base_addr + offset,
				f55->query_18.data,
				sizeof(f55->query_18.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 19 */
	if (f55->query_18.has_query19)
		offset += 1;

	/* query 20 */
	if (f55->query_18.has_ctrl27_query20)
		offset += 1;

	/* query 21 */
	if (f55->query_18.has_ctrl28_query21)
		offset += 1;

	/* query 22 */
	if (f55->query_18.has_query22) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f55->query_base_addr + offset,
				f55->query_22.data,
				sizeof(f55->query_22.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 23 */
	if (f55->query_22.has_query23) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f55->query_base_addr + offset,
				f55->query_23.data,
				sizeof(f55->query_23.data));
		if (retval < 0)
			return retval;
		offset += 1;

		f55->amp_sensor = f55->query_23.amp_sensor_enabled;
		f55->size_of_column2mux = f55->query_23.size_of_column2mux;
	}

	/* queries 24 25 26 27 reserved */

	/* query 28 */
	if (f55->query_22.has_query28) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f55->query_base_addr + offset,
				f55->query_28.data,
				sizeof(f55->query_28.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* query 29 */
	if (f55->query_28.has_query29)
		offset += 1;

	/* query 30 */
	if (f55->query_28.has_query30) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f55->query_base_addr + offset,
				f55->query_30.data,
				sizeof(f55->query_30.data));
		if (retval < 0)
			return retval;
		offset += 1;
	}

	/* queries 31 32 */
	if (f55->query_30.has_query31_query32)
		offset += 2;

	/* query 33 */
	if (f55->query_30.has_query33) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f55->query_base_addr + offset,
				f55->query_33.data,
				sizeof(f55->query_33.data));
		if (retval < 0)
			return retval;
		offset += 1;

		f55->extended_amp = f55->query_33.has_extended_amp_pad;
		f55->extended_amp_btn = f55->query_33.has_extended_amp_btn;
	}

	return 0;
}

static void test_f55_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char ii;
	unsigned char rx_electrodes;
	unsigned char tx_electrodes;
	struct f55_control_43 ctrl_43;

	retval = test_f55_set_queries();
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read f55 query registers\n",
				__func__);
		return;
	}

	if (!f55->query.has_sensor_assignment)
		return;

	retval = test_f55_set_controls();
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set up f55 control registers\n",
				__func__);
		return;
	}

	tx_electrodes = f55->query.num_of_tx_electrodes;
	rx_electrodes = f55->query.num_of_rx_electrodes;

	f55->tx_assignment = kzalloc(tx_electrodes, GFP_KERNEL);
	f55->rx_assignment = kzalloc(rx_electrodes, GFP_KERNEL);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			f55->control_base_addr + SENSOR_TX_MAPPING_OFFSET,
			f55->tx_assignment,
			tx_electrodes);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read f55 tx assignment\n",
				__func__);
		return;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			f55->control_base_addr + SENSOR_RX_MAPPING_OFFSET,
			f55->rx_assignment,
			rx_electrodes);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read f55 rx assignment\n",
				__func__);
		return;
	}

	f54->tx_assigned = 0;
	for (ii = 0; ii < tx_electrodes; ii++) {
		if (f55->tx_assignment[ii] != 0xff)
			f54->tx_assigned++;
	}

	f54->rx_assigned = 0;
	for (ii = 0; ii < rx_electrodes; ii++) {
		if (f55->rx_assignment[ii] != 0xff)
			f54->rx_assigned++;
	}

	if (f55->amp_sensor) {
		f54->tx_assigned = f55->size_of_column2mux;
		f54->rx_assigned /= 2;
	}

	if (f55->extended_amp) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				f55->control_base_addr + f55->afe_mux_offset,
				ctrl_43.data,
				sizeof(ctrl_43.data));
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to read f55 AFE mux sizes\n",
					__func__);
			return;
		}

		f54->tx_assigned = ctrl_43.afe_l_mux_size +
				ctrl_43.afe_r_mux_size;

		f54->swap_sensor_side = ctrl_43.swap_sensor_side;
		f54->left_mux_size = ctrl_43.afe_l_mux_size;
		f54->right_mux_size = ctrl_43.afe_r_mux_size;

		if (f55->query_33.has_extended_amp_btn)
			f54->tx_assigned++;
	}

	/* force mapping */
	if (f55->has_force) {
		f55->force_tx_assignment = kzalloc(tx_electrodes, GFP_KERNEL);
		f55->force_rx_assignment = kzalloc(rx_electrodes, GFP_KERNEL);

		retval = synaptics_rmi4_reg_read(rmi4_data,
				f55->control_base_addr + f55->force_tx_offset,
				f55->force_tx_assignment,
				tx_electrodes);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to read f55 force tx assignment\n",
					__func__);
			return;
		}

		retval = synaptics_rmi4_reg_read(rmi4_data,
				f55->control_base_addr + f55->force_rx_offset,
				f55->force_rx_assignment,
				rx_electrodes);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to read f55 force rx assignment\n",
					__func__);
			return;
		}

		for (ii = 0; ii < tx_electrodes; ii++) {
			if (f55->force_tx_assignment[ii] != 0xff)
				f54->tx_assigned++;
		}

		for (ii = 0; ii < rx_electrodes; ii++) {
			if (f55->force_rx_assignment[ii] != 0xff)
				f54->rx_assigned++;
		}
	}

	return;
}

static void test_f55_set_regs(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned char page)
{
	f55 = kzalloc(sizeof(*f55), GFP_KERNEL);
	if (!f55) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for f55\n",
				__func__);
		return;
	}

	f55->query_base_addr = fd->query_base_addr | (page << 8);
	f55->control_base_addr = fd->ctrl_base_addr | (page << 8);
	f55->data_base_addr = fd->data_base_addr | (page << 8);
	f55->command_base_addr = fd->cmd_base_addr | (page << 8);

	return;
}

static int test_scan_pdt(void)
{
	int retval;
	unsigned char intr_count = 0;
	unsigned char page;
	unsigned short addr;
	bool f54found = false;
	bool f55found = false;
	struct synaptics_rmi4_fn_desc rmi_fd;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;

	for (page = 0; page < PAGES_TO_SERVICE; page++) {
		for (addr = PDT_START; addr > PDT_END; addr -= PDT_ENTRY_SIZE) {
			addr |= (page << 8);

			retval = synaptics_rmi4_reg_read(rmi4_data,
					addr,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
			if (retval < 0)
				return retval;

			addr &= ~(MASK_8BIT << 8);

			if (!rmi_fd.fn_number)
				break;

			switch (rmi_fd.fn_number) {
			case SYNAPTICS_RMI4_F54:
				test_f54_set_regs(rmi4_data,
						&rmi_fd, intr_count, page);
				f54found = true;
				break;
			case SYNAPTICS_RMI4_F55:
				test_f55_set_regs(rmi4_data,
						&rmi_fd, page);
				f55found = true;
				break;
			default:
				break;
			}

			if (f54found && f55found)
				goto pdt_done;

			intr_count += rmi_fd.intr_src_count;
		}
	}

	if (!f54found) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to find F54\n",
				__func__);
		return -EINVAL;
	}

pdt_done:
	return 0;
}

static void synaptics_rmi4_test_attn(struct synaptics_rmi4_data *rmi4_data,
		unsigned char intr_mask)
{
	if (!f54)
		return;

	if (f54->intr_mask & intr_mask) {
		pr_err("%s\n", __func__);
		queue_work(f54->test_report_workqueue, &f54->test_report_work);
	}

	return;
}

static ssize_t short_test_333x(void)
{
	int retval;
	const struct synaptics_dsx_board_data *bdata =
		f54->rmi4_data->hw_if->board_data;

	if (!bdata || !bdata->short_test25 || !bdata->short_test26) {
		retval = -EINVAL;
		goto out;
	}

	retval = test_sysfs_read_report_store(NULL, NULL, TEST_TYPE_25_333X, strlen(TEST_TYPE_25_333X));
	if (retval < 0)
		goto out;

	retval = test_sysfs_read_report_show(NULL, NULL, f54->data);
	if (retval < 0)
		goto out;

	if (strncmp(bdata->short_test25, f54->data, strlen(bdata->short_test25))) {
		f54->result_type = TEST_FAILED;
		goto out;
	}

	msleep(1000);

	retval = test_sysfs_read_report_store(NULL, NULL, TEST_TYPE_26_333X, strlen(TEST_TYPE_26_333X));
	if (retval < 0)
		goto out;

	retval = test_sysfs_read_report_show(NULL, NULL, f54->data);
	if (retval < 0)
		goto out;

	if (strncmp(bdata->short_test26, f54->data, strlen(bdata->short_test26))) {
		f54->result_type = TEST_FAILED;
		goto out;
	}

	f54->result_type = TEST_OK;

out:
	if (retval < 0)
		f54->result_type = TEST_INVALID;

	return retval;
}

static ssize_t short_test_4x22(void)
{
	int retval;
	char buf[4];

	retval = test_sysfs_td43xx_ee_short_store(NULL, NULL, "1", 1);
	if (retval < 0)
		goto out;

	msleep(100);

	retval = test_sysfs_td43xx_ee_short_show(NULL, NULL, buf);
	if (retval < 0)
		goto out;

	if (!strncmp(buf, "PASS", 4))
		f54->result_type = TEST_OK;
	else
		f54->result_type = TEST_FAILED;

out:
	if (retval < 0)
		f54->result_type = TEST_INVALID;

	return retval;
}

static ssize_t short_test_tddi_extend(void)
{
	int retval;
	char buf[4];

	retval = test_sysfs_tddi_extend_ee_short_store(NULL, NULL, "1", 1);
	if (retval < 0)
		goto out;

	msleep(100);

	retval = test_sysfs_tddi_extend_ee_short_show(NULL, NULL, buf);
	if (retval < 0)
		goto out;

	if (!strncmp(buf, "PASS", 4))
		f54->result_type = TEST_OK;
	else
		f54->result_type = TEST_FAILED;

out:
	if (retval < 0)
		f54->result_type = TEST_INVALID;

	return retval;
}


static ssize_t open_test_333x(void)
{
	int retval;
	int val, tx, rx, num = 0;
	char *pos;

	retval = test_sysfs_read_report_store(NULL, NULL, TEST_TYPE_03, strlen(TEST_TYPE_03));
	if (retval < 0)
		goto out;

	retval = test_sysfs_read_report_show(NULL, NULL, f54->data);
	if (retval < 0)
		goto out;

	pos = f54->data;
	pos += 5;
	tx = simple_strtoul(pos, NULL, 10);
	pos = strnchr(pos, 16, '\n') + 1;
	pos += 5;
	rx = simple_strtoul(pos, NULL, 10);
	pos = strnchr(pos, 16, '\n') + 1;

	while (num < (tx * rx)) {
		if ((*pos == ' ') || (*pos == '\n'))
			pos++;
		else {
			val = simple_strtol(pos, NULL, 10);
			if (((val > 0) && (val < 100)) || (val < 0)) {
				f54->result_type = TEST_FAILED;
				goto out;
			}

			while ((*pos != ' ') && (*pos != '\n'))
				pos++;
			num++;
		}
	}

	f54->result_type = TEST_OK;

out:
	if (retval < 0)
		f54->result_type = TEST_INVALID;

	return retval;
}

static ssize_t open_test_4x22(void)
{
	int retval;
	char buf[4];

	retval = test_sysfs_td43xx_amp_open_store(NULL, NULL, "1", 1);
	if (retval < 0)
		goto out;

	msleep(100);

	retval = test_sysfs_td43xx_amp_open_show(NULL, NULL, buf);
	if (retval < 0)
		goto out;

	if (!strncmp(buf, "PASS", 4))
		f54->result_type = TEST_OK;
	else
		f54->result_type = TEST_FAILED;

out:
	if (retval < 0)
		f54->result_type = TEST_INVALID;

	return retval;
}

static ssize_t open_test_b7(void)
{
	int retval;
	char buf[4];

	retval = test_sysfs_td4722_b7_amp_open_store(NULL, NULL, "1", 1);
	if (retval < 0)
		goto out;

	msleep(100);

	retval = test_sysfs_td4722_b7_amp_open_show(NULL, NULL, buf);
	if (retval < 0)
		goto out;

	if (!strncmp(buf, "PASS", 4))
		f54->result_type = TEST_OK;
	else
		f54->result_type = TEST_FAILED;

out:
	if (retval < 0)
		f54->result_type = TEST_INVALID;

	return retval;
}

static ssize_t i2c_test(void)
{
	int retval;
	unsigned short addr;
	unsigned char query_data[8] = {0};

	addr = f54->rmi4_data->f01_query_base_addr + CHIP_ID_OFFSET;

	retval = synaptics_rmi4_reg_read(f54->rmi4_data,
			addr,
			query_data,
			8);
	if (retval < 0) {
		f54->result_type = TEST_FAILED;
		retval = 0;
		goto out;
	}

	f54->result_type = TEST_OK;

out:
	if (retval < 0)
		f54->result_type = TEST_INVALID;

	return retval;
}

static int syna_selftest_open(struct inode *inode, struct file *file)
{
	f54->data = vmalloc(PAGE_SIZE);

	return 0;
}

static ssize_t syna_selftest_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	int retval = 0;
	char tmp[5];

	if (*pos != 0)
		return 0;

	snprintf(tmp, sizeof(f54->result_type), "%d\n", f54->result_type);
	if (copy_to_user(buf, tmp, strlen(tmp))) {
		return -EFAULT;
	}
	retval = strlen(tmp);

	*pos += retval;

	return retval;
}

static ssize_t syna_selftest_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	int retval = 0;
	char tmp[6];

	if (!f54->data || count > sizeof(tmp)) {
		retval = -EINVAL;
		f54->result_type = TEST_INVALID;
		goto out;
	}

	if (copy_from_user(tmp, buf, count)) {
		retval = -EFAULT;
		goto out;
	}

	if (!strncmp(tmp, "short", 5)) {
		if (!f54->rmi4_data->chip_is_tddi)
			retval = short_test_333x();
		else {
			if (!f54->rmi4_data->short_test_extend)
				retval = short_test_4x22();
			else
				retval = short_test_tddi_extend();
		}
	} else if (!strncmp(tmp, "open", 4)) {
		if (!f54->rmi4_data->chip_is_tddi)
			retval = open_test_333x();
		else {
			if (!f54->rmi4_data->open_test_b7)
				retval = open_test_4x22();
			else
				retval = open_test_b7();
		}
	} else if (!strncmp(tmp, "i2c", 3))
			retval = i2c_test();

out:
	if (retval >= 0)
		retval = count;

	return retval;
}

static int syna_selftest_release(struct inode *inode, struct file *file)
{
	vfree(f54->data);
	f54->data = NULL;

	return 0;
}

static const struct file_operations syna_selftest_ops = {
	.open		= syna_selftest_open,
	.read		= syna_selftest_read,
	.write		= syna_selftest_write,
	.release	= syna_selftest_release,
};

static ssize_t syna_factory_param_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	char *buffer = NULL, *pbuf;
	char **p = NULL;
	int i, cnt;
	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	struct synaptics_dsx_factory_param *factory_param = rmi4_data->factory_param;

	if (*pos != 0 || !factory_param)
		return 0;

	buffer = vmalloc(PAGE_SIZE);
	if (!buffer)
		return -ENOMEM;

	pbuf = buffer;
	i = 0;
	cnt = 0;
	for (p = factory_string; *p != NULL; p++) {
		cnt = sprintf(pbuf, "%s:%d\n", *p, factory_param->data[i++]);
		pbuf += cnt;
	}

	pbuf[0] = '\0';
	if (copy_to_user(buf, buffer, strlen(buffer) + 1)) {
		return -EFAULT;
	}

	cnt = strlen(buffer);
	*pos += cnt;

	vfree(buffer);
	return cnt;
}

static ssize_t syna_factory_param_write(struct file *file, const char __user *buf, size_t count, loff_t *pos)
{
	int retval = 0;
	char **p = NULL;
	char tmp[100];
	char str[100];
	int i, value = -1;

	struct synaptics_rmi4_data *rmi4_data = f54->rmi4_data;
	struct synaptics_dsx_factory_param *factory_param = rmi4_data->factory_param;

	if (!factory_param)
		return -EINVAL;

	if (copy_from_user(tmp, buf, count)) {
		retval = -EFAULT;
		goto out;
	}

	sscanf(tmp, "%s %d", str, &value);

	i = 0;
	for (p = factory_string; *p != NULL; p++) {
		if (!strncmp(str, *p, strlen(*p))) {
			factory_param->data[i] = value;
			break;
		}
		i++;
	}

out:
	if (retval >= 0)
		retval = count;

	return count;
}

static const struct file_operations syna_factory_ops = {
	.read		= syna_factory_param_read,
	.write		= syna_factory_param_write,
};

static int syna_datadump_open(struct inode *inode, struct file *file)
{
	f54->rmi4_data->dump_flags = true;

	return 0;
}

static ssize_t syna_datadump_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	int retval;
	int cnt = 0;
	char *data = NULL;

	if (*pos != 0)
		return 0;

	if (f54->rmi4_data->disable_data_dump)
		return -EINVAL;

	data = vmalloc(PAGE_SIZE * 4);
	if (!data)
		return -ENOMEM;

	retval = test_sysfs_read_report_store(NULL, NULL, TEST_TYPE_02, strlen(TEST_TYPE_02));
	if (retval < 0) {
		snprintf(data, PAGE_SIZE, "data test error: start\n");
		goto out;
	}

	retval = test_sysfs_read_report_show(NULL, NULL, data);
	if (retval < 0) {
		snprintf(data, PAGE_SIZE, "data test error: read\n");
		goto out;
	}

	cnt += retval;

	retval = test_sysfs_read_report_store(NULL, NULL, TEST_TYPE_03, strlen(TEST_TYPE_03));
	if (retval < 0) {
		snprintf(data, PAGE_SIZE, "data test error: start\n");
		goto out;
	}

	retval = test_sysfs_read_report_show(NULL, NULL, data + cnt);
	if (retval < 0) {
		snprintf(data, PAGE_SIZE, "data test error: read\n");
		goto out;
	}

	if (copy_to_user(buf, data, strlen(data))) {
		vfree(data);
		complete(&f54->rmi4_data->dump_completion);
		return -EFAULT;
	}

	retval = strlen(data);

out:
	vfree(data);
	complete(&f54->rmi4_data->dump_completion);

	*pos += retval;

	return retval;
}

static int syna_datadump_release(struct inode *inode, struct file *file)
{
	f54->rmi4_data->dump_flags = false;

	return 0;
}

static const struct file_operations syna_datadump_ops = {
	.open		= syna_datadump_open,
	.read		= syna_datadump_read,
	.release	= syna_datadump_release,
};

static int synaptics_rmi4_test_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	static bool proc_already;

	if (f54) {
		dev_dbg(rmi4_data->pdev->dev.parent,
				"%s: Handle already exists\n",
				__func__);
		return 0;
	}

	f54 = kzalloc(sizeof(*f54), GFP_KERNEL);
	if (!f54) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to alloc mem for f54\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	f54->result_type = TEST_INVALID;

	f54->rmi4_data = rmi4_data;

	f55 = NULL;

	retval = test_scan_pdt();
	if (retval < 0)
		goto exit_free_mem;

	retval = test_set_queries();
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read f54 query registers\n",
				__func__);
		goto exit_free_mem;
	}

	f54->tx_assigned = f54->query.num_of_tx_electrodes;
	f54->rx_assigned = f54->query.num_of_rx_electrodes;

	retval = test_set_controls();
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set up f54 control registers\n",
				__func__);
		goto exit_free_control;
	}

	test_set_data();

	if (f55)
		test_f55_init(rmi4_data);

	if (rmi4_data->external_afe_buttons)
		f54->tx_assigned++;

	retval = test_set_sysfs();
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to create sysfs entries\n",
				__func__);
		goto exit_sysfs;
	}

	f54->test_report_workqueue =
			create_singlethread_workqueue("test_report_workqueue");
	INIT_WORK(&f54->test_report_work, test_report_work);

	hrtimer_init(&f54->watchdog, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	f54->watchdog.function = test_get_report_timeout;
	INIT_WORK(&f54->timeout_work, test_timeout_work);

	INIT_WORK(&f54->resume_touch_work, test_resume_touch_work);

	mutex_init(&f54->status_mutex);
	f54->status = STATUS_IDLE;

	if (!proc_already) {
		proc_already = true;
		proc_create("tp_selftest", 0644, NULL, &syna_selftest_ops);
		proc_create("tp_data_dump", 0444, NULL, &syna_datadump_ops);
		proc_create("tp_factory_param", 0644, NULL, &syna_factory_ops);
	}

	return 0;

exit_sysfs:
	if (f55) {
		kfree(f55->tx_assignment);
		kfree(f55->rx_assignment);
		kfree(f55->force_tx_assignment);
		kfree(f55->force_rx_assignment);
	}

exit_free_control:
	test_free_control_mem();

exit_free_mem:
	kfree(f55);
	f55 = NULL;
	kfree(f54);
	f54 = NULL;

exit:
	return retval;
}

static void synaptics_rmi4_test_remove(struct synaptics_rmi4_data *rmi4_data)
{
	if (!f54)
		goto exit;

	hrtimer_cancel(&f54->watchdog);

	cancel_work_sync(&f54->test_report_work);
	flush_workqueue(f54->test_report_workqueue);
	destroy_workqueue(f54->test_report_workqueue);

	cancel_work_sync(&f54->resume_touch_work);

	test_remove_sysfs();

	if (f55) {
		kfree(f55->tx_assignment);
		kfree(f55->rx_assignment);
		kfree(f55->force_tx_assignment);
		kfree(f55->force_rx_assignment);
	}

	test_free_control_mem();

	if (f54->data_buffer_size)
		kfree(f54->report_data);

	kfree(f55);
	f55 = NULL;

	kfree(f54);
	f54 = NULL;

exit:
	complete(&test_remove_complete_force);

	return;
}

static void synaptics_rmi4_test_reset(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	if (!f54) {
		synaptics_rmi4_test_init(rmi4_data);
		return;
	}

	if (f55) {
		kfree(f55->tx_assignment);
		kfree(f55->rx_assignment);
		kfree(f55->force_tx_assignment);
		kfree(f55->force_rx_assignment);
	}

	test_free_control_mem();

	kfree(f55);
	f55 = NULL;

	retval = test_scan_pdt();
	if (retval < 0)
		goto exit_free_mem;

	retval = test_set_queries();
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read f54 query registers\n",
				__func__);
		goto exit_free_mem;
	}

	f54->tx_assigned = f54->query.num_of_tx_electrodes;
	f54->rx_assigned = f54->query.num_of_rx_electrodes;

	retval = test_set_controls();
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to set up f54 control registers\n",
				__func__);
		goto exit_free_control;
	}

	test_set_data();

	if (f55)
		test_f55_init(rmi4_data);

	if (rmi4_data->external_afe_buttons)
		f54->tx_assigned++;

	f54->status = STATUS_IDLE;

	return;

exit_free_control:
	test_free_control_mem();

exit_free_mem:
	hrtimer_cancel(&f54->watchdog);

	cancel_work_sync(&f54->test_report_work);
	flush_workqueue(f54->test_report_workqueue);
	destroy_workqueue(f54->test_report_workqueue);

	test_remove_sysfs();

	if (f54->data_buffer_size)
		kfree(f54->report_data);

	kfree(f55);
	f55 = NULL;

	kfree(f54);
	f54 = NULL;

	return;
}

static struct synaptics_rmi4_exp_fn test_module = {
	.fn_type = RMI_TEST_REPORTING,
	.init = synaptics_rmi4_test_init,
	.remove = synaptics_rmi4_test_remove,
	.reset = synaptics_rmi4_test_reset,
	.reinit = NULL,
	.early_suspend = NULL,
	.suspend = NULL,
	.resume = NULL,
	.late_resume = NULL,
	.attn = synaptics_rmi4_test_attn,
};

static int __init rmi4_test_module_init(void)
{
	synaptics_rmi4_new_function_force(&test_module, true);

	return 0;
}

static void __exit rmi4_test_module_exit(void)
{
	synaptics_rmi4_new_function_force(&test_module, false);

	wait_for_completion(&test_remove_complete_force);

	return;
}

module_init(rmi4_test_module_init);
module_exit(rmi4_test_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX Test Reporting Module");
MODULE_LICENSE("GPL v2");
