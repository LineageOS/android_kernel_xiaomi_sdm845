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
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/input/synaptics_dsx_force.h>
#include "synaptics_dsx_core.h"

#ifdef KERNEL_ABOVE_2_6_38
#include <linux/input/mt.h>
#endif

#if defined(CONFIG_SECURE_TOUCH)
#include <linux/i2c.h>
#include <linux/pm_runtime.h>
#include <linux/errno.h>
#endif

#ifdef CONFIG_TOUCH_DEBUG_FS
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#endif
#include <drm/drm_notifier.h>
#include <drm/drm_panel.h>

#define INPUT_PHYS_NAME "synaptics_dsx/touch_input"
#define STYLUS_PHYS_NAME "synaptics_dsx/stylus"

#define PROC_SYMLINK_PATH "touchpanel"

#define VIRTUAL_KEY_MAP_FILE_NAME "virtualkeys." PLATFORM_DRIVER_NAME

#ifdef KERNEL_ABOVE_2_6_38
#define TYPE_B_PROTOCOL
#endif

#define WAKEUP_GESTURE false

#define NO_0D_WHILE_2D
#define REPORT_2D_Z
#define REPORT_2D_W

#define REPORT_2D_PRESSURE


#define F12_DATA_15_WORKAROUND

#define IGNORE_FN_INIT_FAILURE
/*
#define FB_READY_RESET
#define FB_READY_WAIT_MS 100
#define FB_READY_TIMEOUT_S 30
*/
#define RPT_TYPE (1 << 0)
#define RPT_X_LSB (1 << 1)
#define RPT_X_MSB (1 << 2)
#define RPT_Y_LSB (1 << 3)
#define RPT_Y_MSB (1 << 4)
#define RPT_Z (1 << 5)
#define RPT_WX (1 << 6)
#define RPT_WY (1 << 7)
#define RPT_DEFAULT (RPT_TYPE | RPT_X_LSB | RPT_X_MSB | RPT_Y_LSB | RPT_Y_MSB)

#define REBUILD_WORK_DELAY_MS 500 /* ms */

#define EXP_FN_WORK_DELAY_MS 500 /* ms */
#define MAX_F11_TOUCH_WIDTH 15
#define MAX_F12_TOUCH_WIDTH 255
#define MAX_F12_TOUCH_PRESSURE 255

#define CHECK_STATUS_TIMEOUT_MS 100

#define F01_STD_QUERY_LEN 21
#define F01_BUID_ID_OFFSET 18
#define F01_PROD_ID_OFFSET 11

#define STATUS_NO_ERROR 0x00
#define STATUS_RESET_OCCURRED 0x01
#define STATUS_INVALID_CONFIG 0x02
#define STATUS_DEVICE_FAILURE 0x03
#define STATUS_CONFIG_CRC_FAILURE 0x04
#define STATUS_FIRMWARE_CRC_FAILURE 0x05
#define STATUS_CRC_IN_PROGRESS 0x06

#define NORMAL_OPERATION (0 << 0)
#define SENSOR_SLEEP (1 << 0)
#define NO_SLEEP_OFF (0 << 2)
#define NO_SLEEP_ON (1 << 2)
#define CONFIGURED (1 << 7)

#define F11_CONTINUOUS_MODE 0x00
#define F11_WAKEUP_GESTURE_MODE 0x04
#define F12_CONTINUOUS_MODE 0x00
#define F12_WAKEUP_GESTURE_MODE 0x02
#define F12_UDG_DETECT 0x0f
#define F12_HOMEKEY_DETECT 0x0c

#define DOUBLE_TAP	0x01
#define HOMEKEY_WAKEUP	0x80

#define INPUT_EVENT_START			0
#define INPUT_EVENT_SENSITIVE_MODE_OFF		0
#define INPUT_EVENT_SENSITIVE_MODE_ON		1
#define INPUT_EVENT_STYLUS_MODE_OFF		2
#define INPUT_EVENT_STYLUS_MODE_ON		3
#define INPUT_EVENT_WAKUP_MODE_OFF		4
#define INPUT_EVENT_WAKUP_MODE_ON		5
#define INPUT_EVENT_COVER_MODE_OFF		6
#define INPUT_EVENT_COVER_MODE_ON		7
#define INPUT_EVENT_END				7

#define BUTTON_WG_EN				1

static int synaptics_rmi4_check_status(struct synaptics_rmi4_data *rmi4_data,
		bool *was_in_bl_mode);
static int synaptics_rmi4_free_fingers(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_reinit_device(struct synaptics_rmi4_data *rmi4_data);
static int synaptics_rmi4_reset_device(struct synaptics_rmi4_data *rmi4_data,
		bool rebuild);
static void synaptics_rmi4_sleep_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable);
static void synaptics_rmi4_wakeup_gesture(struct synaptics_rmi4_data *rmi4_data,
		bool enable);
static int synaptics_rmi4_irq_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable, bool attn_only);
static void synaptics_rmi4_wakeup_reconfigure(struct synaptics_rmi4_data *rmi4_data,
		bool enable);

#if defined(CONFIG_SECURE_TOUCH)
static ssize_t synaptics_secure_touch_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_secure_touch_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_secure_touch_show(struct device *dev,
	    struct device_attribute *attr, char *buf);
#endif

#ifdef CONFIG_DRM
static int synaptics_rmi4_drm_notifier_cb(struct notifier_block *self,
		unsigned long event, void *data);
static int synaptics_rmi4_drm_notifier_cb_tddi(struct notifier_block *self,
		unsigned long event, void *data);
#endif

#define DISP_REG_VDD (1<<0)
#define DISP_REG_LAB (1<<1)
#define DISP_REG_IBB (1<<2)
#define DISP_REG_ALL (DISP_REG_VDD | DISP_REG_LAB | DISP_REG_IBB)

static void drm_regulator_ctrl(struct synaptics_rmi4_data *rmi4_data, unsigned int flag, bool enable);
static void drm_reset_ctrl(const struct synaptics_dsx_board_data *bdata, bool on);
static void drm_reset_action(const struct synaptics_dsx_board_data *bdata);

#ifdef CONFIG_HAS_EARLYSUSPEND
#ifndef CONFIG_DRM
#define USE_EARLYSUSPEND
#endif
#endif

#ifdef USE_EARLYSUSPEND
static void synaptics_rmi4_early_suspend(struct early_suspend *h);

static void synaptics_rmi4_late_resume(struct early_suspend *h);
#endif
static irqreturn_t synaptics_rmi4_irq(int irq, void *data);
static int synaptics_rmi4_suspend(struct device *dev);

static int synaptics_rmi4_resume(struct device *dev);

static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_f01_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f01_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_f01_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_0dbutton_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_0dbutton_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_wake_gesture_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_wake_gesture_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_irq_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_irq_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t synaptics_rmi4_panel_color_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_panel_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t synaptics_rmi4_virtual_key_map_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

struct synaptics_rmi4_f01_device_status {
	union {
		struct {
			unsigned char status_code:4;
			unsigned char reserved:2;
			unsigned char flash_prog:1;
			unsigned char unconfigured:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f11_query_0_5 {
	union {
		struct {
			/* query 0 */
			unsigned char f11_query0_b0__2:3;
			unsigned char has_query_9:1;
			unsigned char has_query_11:1;
			unsigned char has_query_12:1;
			unsigned char has_query_27:1;
			unsigned char has_query_28:1;

			/* query 1 */
			unsigned char num_of_fingers:3;
			unsigned char has_rel:1;
			unsigned char has_abs:1;
			unsigned char has_gestures:1;
			unsigned char has_sensitibity_adjust:1;
			unsigned char f11_query1_b7:1;

			/* query 2 */
			unsigned char num_of_x_electrodes;

			/* query 3 */
			unsigned char num_of_y_electrodes;

			/* query 4 */
			unsigned char max_electrodes:7;
			unsigned char f11_query4_b7:1;

			/* query 5 */
			unsigned char abs_data_size:2;
			unsigned char has_anchored_finger:1;
			unsigned char has_adj_hyst:1;
			unsigned char has_dribble:1;
			unsigned char has_bending_correction:1;
			unsigned char has_large_object_suppression:1;
			unsigned char has_jitter_filter:1;
		} __packed;
		unsigned char data[6];
	};
};

struct synaptics_rmi4_f11_query_7_8 {
	union {
		struct {
			/* query 7 */
			unsigned char has_single_tap:1;
			unsigned char has_tap_and_hold:1;
			unsigned char has_double_tap:1;
			unsigned char has_early_tap:1;
			unsigned char has_flick:1;
			unsigned char has_press:1;
			unsigned char has_pinch:1;
			unsigned char has_chiral_scroll:1;

			/* query 8 */
			unsigned char has_palm_detect:1;
			unsigned char has_rotate:1;
			unsigned char has_touch_shapes:1;
			unsigned char has_scroll_zones:1;
			unsigned char individual_scroll_zones:1;
			unsigned char has_multi_finger_scroll:1;
			unsigned char has_multi_finger_scroll_edge_motion:1;
			unsigned char has_multi_finger_scroll_inertia:1;
		} __packed;
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f11_query_9 {
	union {
		struct {
			unsigned char has_pen:1;
			unsigned char has_proximity:1;
			unsigned char has_large_object_sensitivity:1;
			unsigned char has_suppress_on_large_object_detect:1;
			unsigned char has_two_pen_thresholds:1;
			unsigned char has_contact_geometry:1;
			unsigned char has_pen_hover_discrimination:1;
			unsigned char has_pen_hover_and_edge_filters:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f11_query_12 {
	union {
		struct {
			unsigned char has_small_object_detection:1;
			unsigned char has_small_object_detection_tuning:1;
			unsigned char has_8bit_w:1;
			unsigned char has_2d_adjustable_mapping:1;
			unsigned char has_general_information_2:1;
			unsigned char has_physical_properties:1;
			unsigned char has_finger_limit:1;
			unsigned char has_linear_cofficient_2:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f11_query_27 {
	union {
		struct {
			unsigned char f11_query27_b0:1;
			unsigned char has_pen_position_correction:1;
			unsigned char has_pen_jitter_filter_coefficient:1;
			unsigned char has_group_decomposition:1;
			unsigned char has_wakeup_gesture:1;
			unsigned char has_small_finger_correction:1;
			unsigned char has_data_37:1;
			unsigned char f11_query27_b7:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f11_ctrl_6_9 {
	union {
		struct {
			unsigned char sensor_max_x_pos_7_0;
			unsigned char sensor_max_x_pos_11_8:4;
			unsigned char f11_ctrl7_b4__7:4;
			unsigned char sensor_max_y_pos_7_0;
			unsigned char sensor_max_y_pos_11_8:4;
			unsigned char f11_ctrl9_b4__7:4;
		} __packed;
		unsigned char data[4];
	};
};

struct synaptics_rmi4_f11_data_1_5 {
	union {
		struct {
			unsigned char x_position_11_4;
			unsigned char y_position_11_4;
			unsigned char x_position_3_0:4;
			unsigned char y_position_3_0:4;
			unsigned char wx:4;
			unsigned char wy:4;
			unsigned char z;
		} __packed;
		unsigned char data[5];
	};
};

struct synaptics_rmi4_f12_query_5 {
	union {
		struct {
			unsigned char size_of_query6;
			struct {
				unsigned char ctrl0_is_present:1;
				unsigned char ctrl1_is_present:1;
				unsigned char ctrl2_is_present:1;
				unsigned char ctrl3_is_present:1;
				unsigned char ctrl4_is_present:1;
				unsigned char ctrl5_is_present:1;
				unsigned char ctrl6_is_present:1;
				unsigned char ctrl7_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl8_is_present:1;
				unsigned char ctrl9_is_present:1;
				unsigned char ctrl10_is_present:1;
				unsigned char ctrl11_is_present:1;
				unsigned char ctrl12_is_present:1;
				unsigned char ctrl13_is_present:1;
				unsigned char ctrl14_is_present:1;
				unsigned char ctrl15_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl16_is_present:1;
				unsigned char ctrl17_is_present:1;
				unsigned char ctrl18_is_present:1;
				unsigned char ctrl19_is_present:1;
				unsigned char ctrl20_is_present:1;
				unsigned char ctrl21_is_present:1;
				unsigned char ctrl22_is_present:1;
				unsigned char ctrl23_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl24_is_present:1;
				unsigned char ctrl25_is_present:1;
				unsigned char ctrl26_is_present:1;
				unsigned char ctrl27_is_present:1;
				unsigned char ctrl28_is_present:1;
				unsigned char ctrl29_is_present:1;
				unsigned char ctrl30_is_present:1;
				unsigned char ctrl31_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl32_is_present:1;
				unsigned char ctrl33_is_present:1;
				unsigned char ctrl34_is_present:1;
				unsigned char ctrl35_is_present:1;
				unsigned char ctrl36_is_present:1;
				unsigned char ctrl37_is_present:1;
				unsigned char ctrl38_is_present:1;
				unsigned char ctrl39_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl40_is_present:1;
				unsigned char ctrl41_is_present:1;
				unsigned char ctrl42_is_present:1;
				unsigned char ctrl43_is_present:1;
				unsigned char ctrl44_is_present:1;
				unsigned char ctrl45_is_present:1;
				unsigned char ctrl46_is_present:1;
				unsigned char ctrl47_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl48_is_present:1;
				unsigned char ctrl49_is_present:1;
				unsigned char ctrl50_is_present:1;
				unsigned char ctrl51_is_present:1;
				unsigned char ctrl52_is_present:1;
				unsigned char ctrl53_is_present:1;
				unsigned char ctrl54_is_present:1;
				unsigned char ctrl55_is_present:1;
			} __packed;
			struct {
				unsigned char ctrl56_is_present:1;
				unsigned char ctrl57_is_present:1;
				unsigned char ctrl58_is_present:1;
				unsigned char ctrl59_is_present:1;
				unsigned char ctrl60_is_present:1;
				unsigned char ctrl61_is_present:1;
				unsigned char ctrl62_is_present:1;
				unsigned char ctrl63_is_present:1;
			} __packed;
		};
		unsigned char data[9];
	};
};

struct synaptics_rmi4_f12_query_8 {
	union {
		struct {
			unsigned char size_of_query9;
			struct {
				unsigned char data0_is_present:1;
				unsigned char data1_is_present:1;
				unsigned char data2_is_present:1;
				unsigned char data3_is_present:1;
				unsigned char data4_is_present:1;
				unsigned char data5_is_present:1;
				unsigned char data6_is_present:1;
				unsigned char data7_is_present:1;
			} __packed;
			struct {
				unsigned char data8_is_present:1;
				unsigned char data9_is_present:1;
				unsigned char data10_is_present:1;
				unsigned char data11_is_present:1;
				unsigned char data12_is_present:1;
				unsigned char data13_is_present:1;
				unsigned char data14_is_present:1;
				unsigned char data15_is_present:1;
			} __packed;
			struct {
				unsigned char data16_is_present:1;
				unsigned char data17_is_present:1;
				unsigned char data18_is_present:1;
				unsigned char data19_is_present:1;
				unsigned char data20_is_present:1;
				unsigned char data21_is_present:1;
				unsigned char data22_is_present:1;
				unsigned char data23_is_present:1;
			} __packed;
			struct {
				unsigned char data24_is_present:1;
				unsigned char data25_is_present:1;
				unsigned char data26_is_present:1;
				unsigned char data27_is_present:1;
				unsigned char data28_is_present:1;
				unsigned char data29_is_present:1;
				unsigned char data30_is_present:1;
				unsigned char data31_is_present:1;
			} __packed;
		};
		unsigned char data[5];
	};
};

struct synaptics_rmi4_f12_ctrl_8 {
	union {
		struct {
			unsigned char max_x_coord_lsb;
			unsigned char max_x_coord_msb;
			unsigned char max_y_coord_lsb;
			unsigned char max_y_coord_msb;
			unsigned char rx_pitch_lsb;
			unsigned char rx_pitch_msb;
			unsigned char tx_pitch_lsb;
			unsigned char tx_pitch_msb;
			unsigned char low_rx_clip;
			unsigned char high_rx_clip;
			unsigned char low_tx_clip;
			unsigned char high_tx_clip;
			unsigned char num_of_rx;
			unsigned char num_of_tx;
		};
		unsigned char data[14];
	};
};

struct synaptics_rmi4_f12_ctrl_23 {
	union {
		struct {
			unsigned char finger_enable:1;
			unsigned char active_stylus_enable:1;
			unsigned char palm_enable:1;
			unsigned char unclassified_object_enable:1;
			unsigned char hovering_finger_enable:1;
			unsigned char gloved_finger_enable:1;
			unsigned char f12_ctr23_00_b6__7:2;
			unsigned char max_reported_objects;
			unsigned char f12_ctr23_02_b0:1;
			unsigned char report_active_stylus_as_finger:1;
			unsigned char report_palm_as_finger:1;
			unsigned char report_unclassified_object_as_finger:1;
			unsigned char report_hovering_finger_as_finger:1;
			unsigned char report_gloved_finger_as_finger:1;
			unsigned char report_narrow_object_swipe_as_finger:1;
			unsigned char report_handedge_as_finger:1;
			unsigned char cover_enable:1;
			unsigned char stylus_enable:1;
			unsigned char eraser_enable:1;
			unsigned char small_object_enable:1;
			unsigned char f12_ctr23_03_b4__7:4;
			unsigned char report_cover_as_finger:1;
			unsigned char report_stylus_as_finger:1;
			unsigned char report_eraser_as_finger:1;
			unsigned char report_small_object_as_finger:1;
			unsigned char f12_ctr23_04_b4__7:4;
		};
		unsigned char data[5];
	};
};

struct synaptics_rmi4_f12_ctrl_31 {
	union {
		struct {
			unsigned char max_x_coord_lsb;
			unsigned char max_x_coord_msb;
			unsigned char max_y_coord_lsb;
			unsigned char max_y_coord_msb;
			unsigned char rx_pitch_lsb;
			unsigned char rx_pitch_msb;
			unsigned char rx_clip_low;
			unsigned char rx_clip_high;
			unsigned char wedge_clip_low;
			unsigned char wedge_clip_high;
			unsigned char num_of_p;
			unsigned char num_of_q;
		};
		unsigned char data[12];
	};
};

struct synaptics_rmi4_f12_ctrl_58 {
	union {
		struct {
			unsigned char reporting_format;
			unsigned char f12_ctr58_00_reserved;
			unsigned char min_force_lsb;
			unsigned char min_force_msb;
			unsigned char max_force_lsb;
			unsigned char max_force_msb;
			unsigned char light_press_threshold_lsb;
			unsigned char light_press_threshold_msb;
			unsigned char light_press_hysteresis_lsb;
			unsigned char light_press_hysteresis_msb;
			unsigned char hard_press_threshold_lsb;
			unsigned char hard_press_threshold_msb;
			unsigned char hard_press_hysteresis_lsb;
			unsigned char hard_press_hysteresis_msb;
		};
		unsigned char data[14];
	};
};

struct synaptics_rmi4_f12_finger_data {
	unsigned char object_type_and_status;
	unsigned char x_lsb;
	unsigned char x_msb;
	unsigned char y_lsb;
	unsigned char y_msb;
#ifdef REPORT_2D_Z
	unsigned char z;
#endif
#ifdef REPORT_2D_W
	unsigned char wx;
	unsigned char wy;
#endif
};

struct synaptics_rmi4_f1a_query {
	union {
		struct {
			unsigned char max_button_count:3;
			unsigned char f1a_query0_b3__4:2;
			unsigned char has_query4:1;
			unsigned char has_query3:1;
			unsigned char has_query2:1;
			unsigned char has_general_control:1;
			unsigned char has_interrupt_enable:1;
			unsigned char has_multibutton_select:1;
			unsigned char has_tx_rx_map:1;
			unsigned char has_perbutton_threshold:1;
			unsigned char has_release_threshold:1;
			unsigned char has_strongestbtn_hysteresis:1;
			unsigned char has_filter_strength:1;
		} __packed;
		unsigned char data[2];
	};
};

struct synaptics_rmi4_f1a_query_4 {
	union {
		struct {
			unsigned char has_ctrl19:1;
			unsigned char f1a_query4_b1__4:4;
			unsigned char has_ctrl24:1;
			unsigned char f1a_query4_b6__7:2;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f1a_control_0 {
	union {
		struct {
			unsigned char multibutton_report:2;
			unsigned char filter_mode:2;
			unsigned char reserved:4;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_f1a_control {
	struct synaptics_rmi4_f1a_control_0 general_control;
	unsigned char button_int_enable;
	unsigned char multi_button;
	unsigned char *txrx_map;
	unsigned char *button_threshold;
	unsigned char button_release_threshold;
	unsigned char strongest_button_hysteresis;
	unsigned char filter_strength;
};

struct synaptics_rmi4_f1a_handle {
	int button_bitmask_size;
	unsigned char max_count;
	unsigned char valid_button_count;
	unsigned char *button_data_buffer;
	unsigned char *button_map;
	struct synaptics_rmi4_f1a_query button_query;
	struct synaptics_rmi4_f1a_control button_control;
};

struct synaptics_rmi4_exp_fhandler {
	struct synaptics_rmi4_exp_fn *exp_fn;
	bool insert;
	bool remove;
	struct list_head link;
};

struct synaptics_rmi4_exp_fn_data {
	bool initialized;
	bool queue_work;
	struct mutex mutex;
	struct list_head list;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
	struct synaptics_rmi4_data *rmi4_data;
};

static struct synaptics_rmi4_exp_fn_data exp_data;

static struct synaptics_dsx_button_map *vir_button_map;

static struct device_attribute attrs[] = {
	__ATTR(reset, S_IWUSR,
			synaptics_rmi4_show_error,
			synaptics_rmi4_f01_reset_store),
	__ATTR(productinfo, S_IRUGO,
			synaptics_rmi4_f01_productinfo_show,
			synaptics_rmi4_store_error),
	__ATTR(buildid, S_IRUGO,
			synaptics_rmi4_f01_buildid_show,
			synaptics_rmi4_store_error),
	__ATTR(flashprog, S_IRUGO,
			synaptics_rmi4_f01_flashprog_show,
			synaptics_rmi4_store_error),
	__ATTR(0dbutton, (S_IRUGO | S_IWUSR),
			synaptics_rmi4_0dbutton_show,
			synaptics_rmi4_0dbutton_store),
	__ATTR(suspend, S_IWUSR,
			synaptics_rmi4_show_error,
			synaptics_rmi4_suspend_store),
	__ATTR(wake_gesture, (S_IRUGO | S_IWUSR),
			synaptics_rmi4_wake_gesture_show,
			synaptics_rmi4_wake_gesture_store),
	__ATTR(irq_enable, (S_IRUGO | S_IWUSR),
			synaptics_rmi4_irq_enable_show,
			synaptics_rmi4_irq_enable_store),
};

#if defined(CONFIG_SECURE_TOUCH)
static DEVICE_ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP), synaptics_secure_touch_enable_show, synaptics_secure_touch_enable_store);
static DEVICE_ATTR(secure_touch, S_IRUGO , synaptics_secure_touch_show, NULL);
#if 0
static int synaptics_secure_touch_clk_prepare_enable(
		struct synaptics_rmi4_data *rmi4_data)
{
	int ret;

	ret = clk_prepare_enable(rmi4_data->iface_clk);
	if (ret) {
		dev_err(rmi4_data->pdev->dev.parent,
			"error on clk_prepare_enable(iface_clk):%d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(rmi4_data->core_clk);
	if (ret) {
		clk_disable_unprepare(rmi4_data->iface_clk);
		dev_err(rmi4_data->pdev->dev.parent,
			"error clk_prepare_enable(core_clk):%d\n", ret);
	}
	return ret;
}

static void synaptics_secure_touch_clk_disable_unprepare(
		struct synaptics_rmi4_data *rmi4_data)
{
	clk_disable_unprepare(rmi4_data->core_clk);
	clk_disable_unprepare(rmi4_data->iface_clk);
}
#endif
static void synaptics_secure_touch_init(struct synaptics_rmi4_data *data)
{
	//int ret = 0;

	data->st_initialized = 0;
	init_completion(&data->st_powerdown);
	init_completion(&data->st_irq_processed);
#if 0
	/* Get clocks */
	data->core_clk = clk_get(data->pdev->dev.parent, "core_clk");
	if (IS_ERR(data->core_clk)) {
		ret = PTR_ERR(data->core_clk);
		dev_err(data->pdev->dev.parent,
			"%s: error on clk_get(core_clk):%d\n", __func__, ret);
		return;
	}

	data->iface_clk = clk_get(data->pdev->dev.parent, "iface_clk");
	if (IS_ERR(data->iface_clk)) {
		ret = PTR_ERR(data->iface_clk);
		dev_err(data->pdev->dev.parent,
			"%s: error on clk_get(iface_clk)\n", __func__);
		goto err_iface_clk;
	}
#endif
	data->st_initialized = 1;
	return;
#if 0
err_iface_clk:
		clk_put(data->core_clk);
		data->core_clk = NULL;
#endif
}
static void synaptics_secure_touch_notify(struct synaptics_rmi4_data *data)
{
        sysfs_notify(&data->pdev->dev.parent->kobj, NULL, "secure_touch");
}
static irqreturn_t synaptics_filter_interrupt(struct synaptics_rmi4_data *data)
{
	if (atomic_read(&data->st_enabled)) {
		if (atomic_cmpxchg(&data->st_pending_irqs, 0, 1) == 0) {
			synaptics_secure_touch_notify(data);
			wait_for_completion_interruptible(
				&data->st_irq_processed);
		}
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}
static void synaptics_secure_touch_stop(
	struct synaptics_rmi4_data *data,
	int blocking)
{
	if (atomic_read(&data->st_enabled)) {
		atomic_set(&data->st_pending_irqs, -1);
		synaptics_secure_touch_notify(data);
		if (blocking)
			wait_for_completion_interruptible(&data->st_powerdown);
	}
}
#else
static void synaptics_secure_touch_init(struct synaptics_rmi4_data *data)
{
}
static irqreturn_t synaptics_filter_interrupt(struct synaptics_rmi4_data *data)
{
	return IRQ_NONE;
}
static void synaptics_secure_touch_stop(
	struct synaptics_rmi4_data *data,
	int blocking)
{
}
#endif

#if defined(CONFIG_SECURE_TOUCH)
static ssize_t synaptics_secure_touch_enable_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&data->st_enabled));
}
/*
 * Accept only "0" and "1" valid values.
 * "0" will reset the st_enabled flag, then wake up the reading process and
 * the interrupt handler.
 * The bus driver is notified via pm_runtime that it is not required to stay
 * awake anymore.
 * It will also make sure the queue of events is emptied in the controller,
 * in case a touch happened in between the secure touch being disabled and
 * the local ISR being ungated.
 * "1" will set the st_enabled flag and clear the st_pending_irqs flag.
 * The bus driver is requested via pm_runtime to stay awake.
 */
static ssize_t synaptics_secure_touch_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct synaptics_rmi4_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = container_of(data->pdev->dev.parent, struct i2c_client, dev);
	struct device *adapter = client->adapter->dev.parent;
	unsigned long value;
	int err = 0;

	if (count > 2)
		return -EINVAL;

	err = kstrtoul(buf, 10, &value);
	if (err != 0)
		return err;

	if (!data->st_initialized)
		return -EIO;

	err = count;

	switch (value) {
	case 0:
		if (atomic_read(&data->st_enabled) == 0)
			break;

		//synaptics_secure_touch_clk_disable_unprepare(data);
		pm_runtime_put_sync(adapter);
		atomic_set(&data->st_enabled, 0);
		synaptics_secure_touch_notify(data);
		complete(&data->st_irq_processed);
		synaptics_rmi4_irq(data->irq, data);
		complete(&data->st_powerdown);

		break;
	case 1:
		if (atomic_read(&data->st_enabled)) {
			err = -EBUSY;
			break;
		}

		synchronize_irq(data->irq);
		if (pm_runtime_get_sync(adapter) < 0) {
			dev_err(data->pdev->dev.parent, "pm_runtime_get_sync failed\n");
			err = -EIO;
			break;
		}
#if 0
		if (synaptics_secure_touch_clk_prepare_enable(data) < 0) {
			pm_runtime_put_sync(adapter);
			err = -EIO;
			break;
		}
#endif
		reinit_completion(&data->st_powerdown);
		reinit_completion(&data->st_irq_processed);
		atomic_set(&data->st_enabled, 1);
		atomic_set(&data->st_pending_irqs,  0);
		break;
	default:
		dev_err(data->pdev->dev.parent,
			"unsupported value: %lu\n", value);
		err = -EINVAL;
		break;
	}
       dev_err(data->pdev->dev.parent, "synaptics_secure_touch_enable_store err=%x\n", err);
	return err;
}

/*
 * This function returns whether there are pending interrupts, or
 * other error conditions that need to be signaled to the userspace library,
 * according tot he following logic:
 * - st_enabled is 0 if secure touch is not enabled, returning -EBADF
 * - st_pending_irqs is -1 to signal that secure touch is in being stopped,
 *   returning -EINVAL
 * - st_pending_irqs is 1 to signal that there is a pending irq, returning
 *   the value "1" to the sysfs read operation
 * - st_pending_irqs is 0 (only remaining case left) if the pending interrupt
 *   has been processed, so the interrupt handler can be allowed to continue.
 */
static ssize_t synaptics_secure_touch_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *data = dev_get_drvdata(dev);
	int val = 0;

	if (atomic_read(&data->st_enabled) == 0)
		return -EBADF;

	if (atomic_cmpxchg(&data->st_pending_irqs, -1, 0) == -1)
		return -EINVAL;

	if (atomic_cmpxchg(&data->st_pending_irqs, 1, 0) == 1)
		val = 1;
	else
		complete(&data->st_irq_processed);

	return scnprintf(buf, PAGE_SIZE, "%u", val);

}
#endif

static DEVICE_ATTR(panel_color, S_IRUSR, synaptics_rmi4_panel_color_show, NULL);
static DEVICE_ATTR(panel_vendor, S_IRUSR, synaptics_rmi4_panel_vendor_show, NULL);

static struct kobj_attribute virtual_key_map_attr = {
	.attr = {
		.name = VIRTUAL_KEY_MAP_FILE_NAME,
		.mode = S_IRUGO,
	},
	.show = synaptics_rmi4_virtual_key_map_show,
};

static ssize_t synaptics_rmi4_f01_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int reset;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &reset) != 1)
		return -EINVAL;

	if (reset != 1)
		return -EINVAL;

	retval = synaptics_rmi4_reset_device(rmi4_data, false);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to issue reset command, error = %d\n",
				__func__, retval);
		return retval;
	}

	return count;
}

static ssize_t synaptics_rmi4_f01_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "0x%02x 0x%02x\n",
			(rmi4_data->rmi4_mod_info.product_info[0]),
			(rmi4_data->rmi4_mod_info.product_info[1]));
}

static ssize_t synaptics_rmi4_f01_buildid_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			rmi4_data->firmware_id);
}

static ssize_t synaptics_rmi4_f01_flashprog_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int retval;
	struct synaptics_rmi4_f01_device_status device_status;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			device_status.data,
			sizeof(device_status.data));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read device status, error = %d\n",
				__func__, retval);
		return retval;
	}

	return snprintf(buf, PAGE_SIZE, "%u\n",
			device_status.flash_prog);
}

static ssize_t synaptics_rmi4_0dbutton_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			rmi4_data->button_0d_enabled);
}

static ssize_t synaptics_rmi4_0dbutton_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	unsigned char ii;
	unsigned char intr_enable;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	input = input > 0 ? 1 : 0;

	if (rmi4_data->button_0d_enabled == input)
		return count;

	if (list_empty(&rmi->support_fn_list))
		return -ENODEV;

	list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
		if (fhandler->fn_number == SYNAPTICS_RMI4_F1A) {
			ii = fhandler->intr_reg_num;

			retval = synaptics_rmi4_reg_read(rmi4_data,
					rmi4_data->f01_ctrl_base_addr + 1 + ii,
					&intr_enable,
					sizeof(intr_enable));
			if (retval < 0)
				return retval;

			if (input == 1)
				intr_enable |= fhandler->intr_mask;
			else
				intr_enable &= ~fhandler->intr_mask;

			retval = synaptics_rmi4_reg_write(rmi4_data,
					rmi4_data->f01_ctrl_base_addr + 1 + ii,
					&intr_enable,
					sizeof(intr_enable));
			if (retval < 0)
				return retval;
		}
	}

	rmi4_data->button_0d_enabled = input;

	return count;
}

static ssize_t synaptics_rmi4_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input == 1)
		synaptics_rmi4_suspend(dev);
	else if (input == 0)
		synaptics_rmi4_resume(dev);
	else
		return -EINVAL;

	return count;
}

static ssize_t synaptics_rmi4_wake_gesture_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			rmi4_data->enable_wakeup_gesture);
}

static ssize_t synaptics_rmi4_wake_gesture_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	const struct synaptics_dsx_board_data *bdata =
		rmi4_data->hw_if->board_data;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (bdata->cut_off_power) {
		dev_err(rmi4_data->pdev->dev.parent,
			"%s: Unable to switch wakeup gesture mode\n", __func__);
		return count;
	}

	input = input > 0 ? 1 : 0;

	if (rmi4_data->f11_wakeup_gesture || rmi4_data->f12_wakeup_gesture)
		rmi4_data->enable_wakeup_gesture = input;

	if (rmi4_data->suspend)
		synaptics_rmi4_wakeup_reconfigure(rmi4_data, (bool)input);

	return count;
}

static ssize_t synaptics_rmi4_irq_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n",
			rmi4_data->irq_enabled);
}

static ssize_t synaptics_rmi4_irq_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;
	if (input)
		enable_irq(rmi4_data->irq);
	else
		disable_irq(rmi4_data->irq);

	return count;
}

static ssize_t synaptics_rmi4_panel_color_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%c\n",
			rmi4_data->lockdown_info[2]);
}

static ssize_t synaptics_rmi4_panel_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%c\n",
			rmi4_data->lockdown_info[0]);
}

static ssize_t synaptics_rmi4_virtual_key_map_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int ii;
	int cnt;
	int count = 0;

	for (ii = 0; ii < vir_button_map->nbuttons; ii++) {
		cnt = snprintf(buf, PAGE_SIZE - count, "0x01:%d:%d:%d:%d:%d\n",
				vir_button_map->map[ii * 5 + 0],
				vir_button_map->map[ii * 5 + 1],
				vir_button_map->map[ii * 5 + 2],
				vir_button_map->map[ii * 5 + 3],
				vir_button_map->map[ii * 5 + 4]);
		buf += cnt;
		count += cnt;
	}

	return count;
}

static int synaptics_rmi4_f11_abs_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0; /* number of touch points */
	unsigned char reg_index;
	unsigned char finger;
	unsigned char fingers_supported;
	unsigned char num_of_finger_status_regs;
	unsigned char finger_shift;
	unsigned char finger_status;
	unsigned char finger_status_reg[3];
	unsigned char detected_gestures;
	unsigned short data_addr;
	unsigned short data_offset;
	int x;
	int y;
	int wx;
	int wy;
	int temp;
	struct synaptics_rmi4_f11_data_1_5 data;
	struct synaptics_rmi4_f11_extra_data *extra_data;

	/*
	 * The number of finger status registers is determined by the
	 * maximum number of fingers supported - 2 bits per finger. So
	 * the number of finger status registers to read is:
	 * register_count = ceil(max_num_of_fingers / 4)
	 */
	fingers_supported = fhandler->num_of_data_points;
	num_of_finger_status_regs = (fingers_supported + 3) / 4;
	data_addr = fhandler->full_addr.data_base;

	extra_data = (struct synaptics_rmi4_f11_extra_data *)fhandler->extra;

	if (rmi4_data->suspend && rmi4_data->wakeup_en) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				data_addr + extra_data->data38_offset,
				&detected_gestures,
				sizeof(detected_gestures));
		if (retval < 0)
			return 0;

		if (detected_gestures) {
			input_report_key(rmi4_data->input_dev, KEY_WAKEUP, 1);
			input_sync(rmi4_data->input_dev);
			input_report_key(rmi4_data->input_dev, KEY_WAKEUP, 0);
			input_sync(rmi4_data->input_dev);
		}

		return 0;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			data_addr,
			finger_status_reg,
			num_of_finger_status_regs);
	if (retval < 0)
		return 0;

	mutex_lock(&(rmi4_data->rmi4_report_mutex));

	for (finger = 0; finger < fingers_supported; finger++) {
		reg_index = finger / 4;
		finger_shift = (finger % 4) * 2;
		finger_status = (finger_status_reg[reg_index] >> finger_shift)
				& MASK_2BIT;

		/*
		 * Each 2-bit finger status field represents the following:
		 * 00 = finger not present
		 * 01 = finger present and data accurate
		 * 10 = finger present but data may be inaccurate
		 * 11 = reserved
		 */
#ifdef TYPE_B_PROTOCOL
		input_mt_slot(rmi4_data->input_dev, finger);
		input_mt_report_slot_state(rmi4_data->input_dev,
				MT_TOOL_FINGER, finger_status);
#endif

		if (finger_status) {
			data_offset = data_addr +
					num_of_finger_status_regs +
					(finger * sizeof(data.data));
			retval = synaptics_rmi4_reg_read(rmi4_data,
					data_offset,
					data.data,
					sizeof(data.data));
			if (retval < 0) {
				touch_count = 0;
				goto exit;
			}

			x = (data.x_position_11_4 << 4) | data.x_position_3_0;
			y = (data.y_position_11_4 << 4) | data.y_position_3_0;
			wx = data.wx;
			wy = data.wy;

			if (rmi4_data->hw_if->board_data->swap_axes) {
				temp = x;
				x = y;
				y = temp;
				temp = wx;
				wx = wy;
				wy = temp;
			}

			if (rmi4_data->hw_if->board_data->x_flip)
				x = rmi4_data->sensor_max_x - x;
			if (rmi4_data->hw_if->board_data->y_flip)
				y = rmi4_data->sensor_max_y - y;

			input_report_key(rmi4_data->input_dev,
					BTN_TOUCH, 1);
			input_report_key(rmi4_data->input_dev,
					BTN_TOOL_FINGER, 1);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_X, x);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_Y, y);
#ifdef REPORT_2D_W
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MAJOR, max(wx, wy));
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_TOUCH_MINOR, min(wx, wy));
#endif
#ifndef TYPE_B_PROTOCOL
			input_mt_sync(rmi4_data->input_dev);
#endif

			dev_dbg(rmi4_data->pdev->dev.parent,
					"%s: Finger %d: "
					"status = 0x%02x, "
					"x = %d, "
					"y = %d, "
					"wx = %d, "
					"wy = %d\n",
					__func__, finger,
					finger_status,
					x, y, wx, wy);

			touch_count++;
		}
	}

	if (touch_count == 0) {
		input_report_key(rmi4_data->input_dev,
				BTN_TOUCH, 0);
		input_report_key(rmi4_data->input_dev,
				BTN_TOOL_FINGER, 0);
#ifndef TYPE_B_PROTOCOL
		input_mt_sync(rmi4_data->input_dev);
#endif
	}

	input_sync(rmi4_data->input_dev);

exit:
	mutex_unlock(&(rmi4_data->rmi4_report_mutex));

	return touch_count;
}

static int synaptics_rmi4_f12_abs_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0; /* number of touch points */
	unsigned char index;
	unsigned char finger;
	unsigned char fingers_to_process;
	unsigned char finger_status;
	unsigned char size_of_2d_data;
	unsigned char gesture_type;
	unsigned short data_addr;
	int x;
	int y;
	int wx;
	int wy;
	int temp;
#if defined(REPORT_2D_PRESSURE) || defined(F51_DISCRETE_FORCE)
	int pressure;
#endif
	int touchs = 0;
#ifdef REPORT_2D_PRESSURE
	unsigned char f_fingers;
	unsigned char f_lsb;
	unsigned char f_msb;
	unsigned char *f_data;
#endif
#ifdef F51_DISCRETE_FORCE
	unsigned char force_level;
#endif
	struct synaptics_rmi4_f12_extra_data *extra_data;
	struct synaptics_rmi4_f12_finger_data *data;
	struct synaptics_rmi4_f12_finger_data *finger_data;
	static unsigned char finger_presence;
	static unsigned char stylus_presence;
#ifdef F12_DATA_15_WORKAROUND
	static unsigned char objects_already_present;
#endif

	if (rmi4_data->input_dev == NULL) {
		dev_err(rmi4_data->pdev->dev.parent, "input_dev is NULL, do not report data\n");

		return 0;
	}

	fingers_to_process = fhandler->num_of_data_points;
	data_addr = fhandler->full_addr.data_base;
	extra_data = (struct synaptics_rmi4_f12_extra_data *)fhandler->extra;
	size_of_2d_data = sizeof(struct synaptics_rmi4_f12_finger_data);

	if (rmi4_data->suspend && rmi4_data->wakeup_en) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				data_addr + extra_data->data4_offset,
				rmi4_data->gesture_detection,
				sizeof(rmi4_data->gesture_detection));
		if (retval < 0)
			return 0;

		gesture_type = rmi4_data->gesture_detection[0];
		if (gesture_type && gesture_type != F12_UDG_DETECT) {
			input_report_key(rmi4_data->input_dev, KEY_WAKEUP, 1);
			input_sync(rmi4_data->input_dev);
			input_report_key(rmi4_data->input_dev, KEY_WAKEUP, 0);
			input_sync(rmi4_data->input_dev);
			dev_err(rmi4_data->pdev->dev.parent, "double click send input event\n");
		}

		return 0;
	}

	/* Determine the total number of fingers to process */
	if (extra_data->data15_size) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				data_addr + extra_data->data15_offset,
				extra_data->data15_data,
				extra_data->data15_size);
		if (retval < 0)
			return 0;

		/* Start checking from the highest bit */
		index = extra_data->data15_size - 1; /* Highest byte */
		finger = (fingers_to_process - 1) % 8; /* Highest bit */
		do {
			if (extra_data->data15_data[index] & (1 << finger))
				break;

			if (finger) {
				finger--;
			} else if (index > 0) {
				index--; /* Move to the next lower byte */
				finger = 7;
			}

			fingers_to_process--;
		} while (fingers_to_process);

		dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Number of fingers to process = %d\n",
			__func__, fingers_to_process);
	}

#ifdef F12_DATA_15_WORKAROUND
	fingers_to_process = max(fingers_to_process, objects_already_present);
#endif

	if (!fingers_to_process) {
		synaptics_rmi4_free_fingers(rmi4_data);
		finger_presence = 0;
		stylus_presence = 0;
		rmi4_data->touchs = 0;
		return 0;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			data_addr + extra_data->data1_offset,
			(unsigned char *)fhandler->data,
			fingers_to_process * size_of_2d_data);
	if (retval < 0)
		return 0;

	data = (struct synaptics_rmi4_f12_finger_data *)fhandler->data;

#ifdef REPORT_2D_PRESSURE
	if (rmi4_data->report_pressure) {
		retval = synaptics_rmi4_reg_read(rmi4_data,
				data_addr + extra_data->data29_offset,
				extra_data->data29_data,
				extra_data->data29_size);
		if (retval < 0)
			return 0;
	}
#endif

	mutex_lock(&(rmi4_data->rmi4_report_mutex));

	for (finger = 0; finger < fingers_to_process; finger++) {
		finger_data = data + finger;
		finger_status = finger_data->object_type_and_status;

#ifdef F12_DATA_15_WORKAROUND
		objects_already_present = finger + 1;
#endif

		x = (finger_data->x_msb << 8) | (finger_data->x_lsb);
		y = (finger_data->y_msb << 8) | (finger_data->y_lsb);
#ifdef REPORT_2D_W
		wx = finger_data->wx;
		wy = finger_data->wy;
#endif

		if (rmi4_data->hw_if->board_data->swap_axes) {
			temp = x;
			x = y;
			y = temp;
			temp = wx;
			wx = wy;
			wy = temp;
		}

		if (rmi4_data->hw_if->board_data->x_flip)
			x = rmi4_data->sensor_max_x - x;
		if (rmi4_data->hw_if->board_data->y_flip)
			y = rmi4_data->sensor_max_y - y;

		switch (finger_status) {
		case F12_FINGER_STATUS:
		case F12_GLOVED_FINGER_STATUS:
			if (stylus_presence) /* Stylus has priority over fingers */
				break;
#ifdef TYPE_B_PROTOCOL
			input_mt_slot(rmi4_data->input_dev, finger);
			input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, 1);
#endif

			input_report_key(rmi4_data->input_dev,
					BTN_TOUCH, 1);
			input_report_key(rmi4_data->input_dev,
					BTN_TOOL_FINGER, 1);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_X, x);
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_POSITION_Y, y);
#ifdef REPORT_2D_W
			if (rmi4_data->wedge_sensor) {
				input_report_abs(rmi4_data->input_dev,
						ABS_MT_TOUCH_MAJOR, wx);
				input_report_abs(rmi4_data->input_dev,
						ABS_MT_TOUCH_MINOR, wx);
			} else {
				input_report_abs(rmi4_data->input_dev,
						ABS_MT_TOUCH_MAJOR,
						max(wx, wy));
				input_report_abs(rmi4_data->input_dev,
						ABS_MT_TOUCH_MINOR,
						min(wx, wy));
			}
#endif
#ifdef REPORT_2D_PRESSURE
			if (rmi4_data->report_pressure) {
				f_fingers = extra_data->data29_size / 2;
				f_data = extra_data->data29_data;
				if (finger + 1 > f_fingers) {
					pressure = 1;
				} else {
					f_lsb = finger * 2;
					f_msb = finger * 2 + 1;
					pressure = (int)f_data[f_lsb] << 0 |
							(int)f_data[f_msb] << 8;
				}
				pressure = pressure > 0 ? pressure : 1;
				if (pressure > rmi4_data->force_max)
					pressure = rmi4_data->force_max;
				input_report_abs(rmi4_data->input_dev,
						ABS_MT_PRESSURE, pressure);
			}
#elif defined(F51_DISCRETE_FORCE)
			if (finger == 0) {
				retval = synaptics_rmi4_reg_read(rmi4_data,
						FORCE_LEVEL_ADDR,
						&force_level,
						sizeof(force_level));
				if (retval < 0)
					return 0;
				pressure = force_level > 0 ? force_level : 1;
			} else {
				pressure = 1;
			}
			input_report_abs(rmi4_data->input_dev,
					ABS_MT_PRESSURE, pressure);
#endif
#ifndef TYPE_B_PROTOCOL
			input_mt_sync(rmi4_data->input_dev);
#endif

			dev_dbg(rmi4_data->pdev->dev.parent,
					"%s: Finger %d: "
					"status = 0x%02x, "
					"x = %d, "
					"y = %d, "
					"wx = %d, "
					"wy = %d\n",
					__func__, finger,
					finger_status,
					x, y, wx, wy);

			finger_presence = 1;
			touch_count++;
			touchs |= BIT(finger);
			rmi4_data->touchs |= BIT(finger);
			break;
		case F12_PALM_STATUS:
			dev_dbg(rmi4_data->pdev->dev.parent,
					"%s: Finger %d: "
					"x = %d, "
					"y = %d, "
					"wx = %d, "
					"wy = %d\n",
					__func__, finger,
					x, y, wx, wy);
			break;
		case F12_STYLUS_STATUS:
		case F12_ERASER_STATUS:
			if (finger_presence) { /* Stylus has priority over fingers */
				mutex_unlock(&(rmi4_data->rmi4_report_mutex));
				synaptics_rmi4_free_fingers(rmi4_data);
				mutex_lock(&(rmi4_data->rmi4_report_mutex));
				finger_presence = 0;
			}
			if (stylus_presence) {/* Allow one stylus at a timee */
				if (finger + 1 != stylus_presence)
					break;
			}
			input_report_key(rmi4_data->stylus_dev,
					BTN_TOUCH, 1);
			if (finger_status == F12_STYLUS_STATUS) {
				input_report_key(rmi4_data->stylus_dev,
						BTN_TOOL_PEN, 1);
			} else {
				input_report_key(rmi4_data->stylus_dev,
						BTN_TOOL_RUBBER, 1);
			}
			input_report_abs(rmi4_data->stylus_dev,
					ABS_X, x);
			input_report_abs(rmi4_data->stylus_dev,
					ABS_Y, y);

			stylus_presence = finger + 1;
			touch_count++;
			break;
		default:
#ifdef TYPE_B_PROTOCOL
			input_mt_slot(rmi4_data->input_dev, finger);
			input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, 0);
#endif
			rmi4_data->touchs &= ~BIT(finger);
			touchs &= ~BIT(finger);
			break;
		}
	}

	for (finger = 0; finger < fhandler->num_of_data_points; finger++) {
		if (BIT(finger) & (rmi4_data->touchs ^ touchs)) {
			input_mt_slot(rmi4_data->input_dev, finger);
			input_mt_report_slot_state(rmi4_data->input_dev,
					MT_TOOL_FINGER, 0);
		}
	}

	if (touch_count == 0) {
		finger_presence = 0;
#ifdef F12_DATA_15_WORKAROUND
		objects_already_present = 0;
#endif
		input_report_key(rmi4_data->input_dev,
				BTN_TOUCH, 0);
		input_report_key(rmi4_data->input_dev,
				BTN_TOOL_FINGER, 0);
#ifndef TYPE_B_PROTOCOL
		input_mt_sync(rmi4_data->input_dev);
#endif

		if (rmi4_data->stylus_enable) {
			stylus_presence = 0;
			input_report_key(rmi4_data->stylus_dev,
					BTN_TOUCH, 0);
			input_report_key(rmi4_data->stylus_dev,
					BTN_TOOL_PEN, 0);
			if (rmi4_data->eraser_enable) {
				input_report_key(rmi4_data->stylus_dev,
						BTN_TOOL_RUBBER, 0);
			}
		}

		rmi4_data->touchs = 0;
	}

	input_sync(rmi4_data->input_dev);

	mutex_unlock(&(rmi4_data->rmi4_report_mutex));

	return touch_count;
}

static void synaptics_rmi4_f1a_report(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	int retval;
	unsigned char touch_count = 0;
	unsigned char button;
	unsigned char index;
	unsigned char shift;
	unsigned char status;
	unsigned char *data;
	unsigned short data_addr = fhandler->full_addr.data_base;
	struct synaptics_rmi4_f1a_handle *f1a = fhandler->data;
	static unsigned char do_once = 1;
	static bool current_status[MAX_NUMBER_OF_BUTTONS];
#ifdef NO_0D_WHILE_2D
	static bool before_2d_status[MAX_NUMBER_OF_BUTTONS];
	static bool while_2d_status[MAX_NUMBER_OF_BUTTONS];
#endif

	if (do_once) {
		memset(current_status, 0, sizeof(current_status));
#ifdef NO_0D_WHILE_2D
		memset(before_2d_status, 0, sizeof(before_2d_status));
		memset(while_2d_status, 0, sizeof(while_2d_status));
#endif
		do_once = 0;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			data_addr,
			f1a->button_data_buffer,
			f1a->button_bitmask_size);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read button data registers\n",
				__func__);
		return;
	}

	data = f1a->button_data_buffer;

	mutex_lock(&(rmi4_data->rmi4_report_mutex));

	for (button = 0; button < f1a->valid_button_count; button++) {
		index = button / 8;
		shift = button % 8;
		status = ((data[index] >> shift) & MASK_1BIT);

		if (current_status[button] == status) {
			if (!rmi4_data->suspend)
				continue;
		} else
			current_status[button] = status;

		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Button %d (code %d) ->%d\n",
				__func__, button,
				f1a->button_map[button],
				status);
#ifdef NO_0D_WHILE_2D
		if (rmi4_data->fingers_on_2d == false) {
			if (status == 1) {
				before_2d_status[button] = 1;
			} else {
				if (while_2d_status[button] == 1) {
					while_2d_status[button] = 0;
					continue;
				} else {
					before_2d_status[button] = 0;
				}
			}

			touch_count++;
			input_report_key(rmi4_data->input_dev,
					f1a->button_map[button],
					status);
		} else {
			if (before_2d_status[button] == 1) {
				before_2d_status[button] = 0;
				touch_count++;
				input_report_key(rmi4_data->input_dev,
						f1a->button_map[button],
						status);
			} else {
				if (status == 1)
					while_2d_status[button] = 1;
				else
					while_2d_status[button] = 0;
			}
		}
#else
		touch_count++;
		input_report_key(rmi4_data->input_dev,
				f1a->button_map[button],
				status);
#endif
	}

	if (touch_count)
		input_sync(rmi4_data->input_dev);

	mutex_unlock(&(rmi4_data->rmi4_report_mutex));

	return;
}

static void synaptics_rmi4_report_touch(struct synaptics_rmi4_data *rmi4_data,
		struct synaptics_rmi4_fn *fhandler)
{
	unsigned char touch_count_2d;

	dev_dbg(rmi4_data->pdev->dev.parent,
			"%s: Function %02x reporting\n",
			__func__, fhandler->fn_number);

	switch (fhandler->fn_number) {
	case SYNAPTICS_RMI4_F11:
		touch_count_2d = synaptics_rmi4_f11_abs_report(rmi4_data,
				fhandler);

		if (touch_count_2d)
			rmi4_data->fingers_on_2d = true;
		else
			rmi4_data->fingers_on_2d = false;
		break;
	case SYNAPTICS_RMI4_F12:
		touch_count_2d = synaptics_rmi4_f12_abs_report(rmi4_data,
				fhandler);

		if (touch_count_2d)
			rmi4_data->fingers_on_2d = true;
		else
			rmi4_data->fingers_on_2d = false;
		break;
	case SYNAPTICS_RMI4_F1A:
		synaptics_rmi4_f1a_report(rmi4_data, fhandler);
		break;
	default:
		break;
	}

	return;
}

static void synaptics_rmi4_sensor_report(struct synaptics_rmi4_data *rmi4_data,
		bool report)
{
	int retval;
	unsigned char data[MAX_INTR_REGISTERS + 1];
	unsigned char *intr = &data[1];
	bool was_in_bl_mode;
	struct synaptics_rmi4_f01_device_status status;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	/*
	 * Get interrupt status information from F01 Data1 register to
	 * determine the source(s) that are flagging the interrupt.
	 */
	retval = synaptics_rmi4_reg_read(rmi4_data,
			rmi4_data->f01_data_base_addr,
			data,
			rmi4_data->num_of_intr_regs + 1);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to read interrupt status\n",
				__func__);
		return;
	}

	status.data[0] = data[0];
	if (status.status_code == STATUS_CRC_IN_PROGRESS) {
		retval = synaptics_rmi4_check_status(rmi4_data,
				&was_in_bl_mode);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to check status\n",
					__func__);
			return;
		}
		retval = synaptics_rmi4_reg_read(rmi4_data,
				rmi4_data->f01_data_base_addr,
				status.data,
				sizeof(status.data));
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to read device status\n",
					__func__);
			return;
		}
	}
	if (status.unconfigured && !status.flash_prog) {
		pr_notice("%s: spontaneous reset detected\n", __func__);
		retval = synaptics_rmi4_reinit_device(rmi4_data);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to reinit device\n",
					__func__);
		}
	}

	if (!report)
		return;

	/*
	 * Traverse the function handler list and service the source(s)
	 * of the interrupt accordingly.
	 */
	if (!list_empty(&rmi->support_fn_list)) {
		list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
			if (fhandler->num_of_data_sources) {
				if (fhandler->intr_mask &
						intr[fhandler->intr_reg_num]) {
					synaptics_rmi4_report_touch(rmi4_data,
							fhandler);
				}
			}
		}
	}

	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link) {
			if (!exp_fhandler->insert &&
					!exp_fhandler->remove &&
					(exp_fhandler->exp_fn->attn != NULL))
				exp_fhandler->exp_fn->attn(rmi4_data, intr[0]);
		}
	}
	mutex_unlock(&exp_data.mutex);

	return;
}

static irqreturn_t synaptics_rmi4_irq(int irq, void *data)
{
	struct synaptics_rmi4_data *rmi4_data = data;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	if (gpio_get_value(bdata->irq_gpio) != bdata->irq_on_state)
		goto exit;

	if (IRQ_HANDLED == synaptics_filter_interrupt(data))
		return IRQ_HANDLED;

	synaptics_rmi4_sensor_report(rmi4_data, true);

exit:
	return IRQ_HANDLED;
}

static int synaptics_rmi4_int_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	int retval = 0;
	unsigned char ii;
	unsigned char zero = 0x00;
	unsigned char *intr_mask;
	unsigned short intr_addr;

	intr_mask = rmi4_data->intr_mask;

	for (ii = 0; ii < rmi4_data->num_of_intr_regs; ii++) {
		if (intr_mask[ii] != 0x00) {
			intr_addr = rmi4_data->f01_ctrl_base_addr + 1 + ii;
			if (enable) {
				retval = synaptics_rmi4_reg_write(rmi4_data,
						intr_addr,
						&(intr_mask[ii]),
						sizeof(intr_mask[ii]));
				if (retval < 0)
					return retval;
			} else {
				retval = synaptics_rmi4_reg_write(rmi4_data,
						intr_addr,
						&zero,
						sizeof(zero));
				if (retval < 0)
					return retval;
			}
		}
	}

	return retval;
}

static int synaptics_rmi4_irq_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable, bool attn_only)
{
	int retval = 0;
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

	if (attn_only) {
		retval = synaptics_rmi4_int_enable(rmi4_data, enable);
		return retval;
	}

	if (enable) {
		if (rmi4_data->irq_enabled)
			return retval;

		retval = synaptics_rmi4_int_enable(rmi4_data, false);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to disable synaptics int\n",
					__func__);
			return retval;
		}

		/* Process and clear interrupts */
		synaptics_rmi4_sensor_report(rmi4_data, false);

		retval = request_threaded_irq(rmi4_data->irq, NULL,
				synaptics_rmi4_irq, bdata->irq_flags,
				PLATFORM_DRIVER_NAME, rmi4_data);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to create irq thread\n",
					__func__);
			return retval;
		}

		retval = synaptics_rmi4_int_enable(rmi4_data, true);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to enable synaptics int\n",
					__func__);
			return retval;
		}

		rmi4_data->irq_enabled = true;
	} else {
		if (rmi4_data->irq_enabled) {
			disable_irq(rmi4_data->irq);
			free_irq(rmi4_data->irq, rmi4_data);
			rmi4_data->irq_enabled = false;
		}
	}

	return retval;
}

static void synaptics_rmi4_set_intr_mask(struct synaptics_rmi4_fn *fhandler,
		struct synaptics_rmi4_fn_desc *fd,
		unsigned int intr_count)
{
	unsigned char ii;
	unsigned char intr_offset;

	fhandler->intr_reg_num = (intr_count + 7) / 8;
	if (fhandler->intr_reg_num != 0)
		fhandler->intr_reg_num -= 1;

	/* Set an enable bit for each data source */
	intr_offset = intr_count % 8;
	fhandler->intr_mask = 0;
	for (ii = intr_offset;
			ii < (fd->intr_src_count + intr_offset);
			ii++)
		fhandler->intr_mask |= 1 << ii;

	return;
}

static int synaptics_rmi4_query_product_id(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned short lockdown_addr;

	/* Product ID addr starts from F01_RMI_QUERY11 */
	lockdown_addr = rmi4_data->f01_query_base_addr + F01_PROD_ID_OFFSET;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			lockdown_addr,
			rmi4_data->lockdown_info,
			LOCKDOWN_INFO_SIZE);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
			"Failed reading reg %d\n",
			lockdown_addr);
		return retval;
	}

	dev_info(rmi4_data->pdev->dev.parent,
			"Lockdown info: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
			rmi4_data->lockdown_info[0], rmi4_data->lockdown_info[1],
			rmi4_data->lockdown_info[2], rmi4_data->lockdown_info[3],
			rmi4_data->lockdown_info[4], rmi4_data->lockdown_info[5],
			rmi4_data->lockdown_info[6], rmi4_data->lockdown_info[7]);

	return 0;
}

static int synaptics_rmi4_query_chip_id(struct synaptics_rmi4_data *rmi4_data)
{
	int retval, i;
	unsigned short chipdata_addr;
	unsigned char query_data[8] = {0};

	chipdata_addr = rmi4_data->f01_query_base_addr + F01_PROD_ID_OFFSET;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			chipdata_addr,
			query_data,
			8);
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
			"Failed reading reg %d\n",
			chipdata_addr);
		return retval;
	}

	dev_info(rmi4_data->pdev->dev.parent,
			"chip info: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
			query_data[0], query_data[1],
			query_data[2], query_data[3],
			query_data[4], query_data[5],
			query_data[6], query_data[7]);

	for (i = 0; i < rmi4_data->hw_if->board_data->config_array_size; i++) {
		if (!memcmp(rmi4_data->hw_if->board_data->config_array[i].chip_id_name,
				query_data,
				strlen(rmi4_data->hw_if->board_data->config_array[i].chip_id_name))) {
			rmi4_data->chip_id = rmi4_data->hw_if->board_data->config_array[i].chip_id;
			rmi4_data->chip_is_tddi = rmi4_data->hw_if->board_data->config_array[i].chip_is_tddi;
			rmi4_data->open_test_b7 = rmi4_data->hw_if->board_data->config_array[i].open_test_b7;
			rmi4_data->short_test_extend = rmi4_data->hw_if->board_data->config_array[i].short_test_extend;
			rmi4_data->factory_param = rmi4_data->hw_if->board_data->config_array[i].factory_param;
			rmi4_data->panel_power_seq = rmi4_data->hw_if->board_data->config_array[i].panel_power_seq;
			if (rmi4_data->chip_is_tddi)
				rmi4_data->hw_if->board_data->reset_gpio = -1;
			break;
		}
	}

	if (i >= rmi4_data->hw_if->board_data->config_array_size) {
		dev_err(rmi4_data->pdev->dev.parent, "failed to match the chip id\n");
		rmi4_data->chip_id = -1; /* Set chip_id -1 to ensure it won't do firmware upgrading */
		return -EINVAL;
	}

	synaptics_secure_touch_init(rmi4_data);
	synaptics_secure_touch_stop(rmi4_data, 1);

	return retval;

err_sysfs_panel_vendor:
#if defined(CONFIG_SECURE_TOUCH)
	sysfs_remove_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_secure_touch_enable.attr);
err_sysfs_secure_enable:
	sysfs_remove_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_secure_touch.attr);
err_sysfs_secure:
	sysfs_remove_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_panel_color.attr);
#endif
err_sysfs:
	for (attr_count--; attr_count >= 0; attr_count--) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

err_virtual_buttons:
	if (rmi4_data->board_prop_dir) {
		sysfs_remove_file(rmi4_data->board_prop_dir,
				&virtual_key_map_attr.attr);
		kobject_put(rmi4_data->board_prop_dir);
	}

	synaptics_rmi4_irq_enable(rmi4_data, false, false);

err_enable_irq:
#ifdef CONFIG_DRM
	drm_unregister_client(&rmi4_data->drm_notifier);
#endif

#ifdef USE_EARLYSUSPEND
	unregister_early_suspend(&rmi4_data->early_suspend);
#endif

	synaptics_rmi4_empty_fn_list(rmi4_data);
	input_unregister_device(rmi4_data->input_dev);
	rmi4_data->input_dev = NULL;
	if (rmi4_data->stylus_enable) {
		input_unregister_device(rmi4_data->stylus_dev);
		rmi4_data->stylus_dev = NULL;
	}

err_set_input_dev:
	synaptics_rmi4_gpio_setup(bdata->irq_gpio, false, 0, 0, NULL);

	if (bdata->reset_gpio >= 0)
		synaptics_rmi4_gpio_setup(bdata->reset_gpio, false, 0, 0, NULL);

	if (bdata->power_gpio >= 0)
		synaptics_rmi4_gpio_setup(bdata->power_gpio, false, 0, 0, NULL);
err_ui_hw_init:
err_pinctrl_init:
	if (rmi4_data->ts_pinctrl) {
		devm_pinctrl_put(rmi4_data->ts_pinctrl);
		rmi4_data->ts_pinctrl = NULL;
	}

err_set_gpio:
	synaptics_rmi4_enable_reg(rmi4_data, false);

err_enable_reg:
	synaptics_rmi4_get_reg(rmi4_data, false);

err_get_reg:
	kfree(rmi4_data);

	return retval;
}

static int synaptics_rmi4_remove(struct platform_device *pdev)
{
	unsigned char attr_count;
	struct synaptics_rmi4_data *rmi4_data = platform_get_drvdata(pdev);
	const struct synaptics_dsx_board_data *bdata =
			rmi4_data->hw_if->board_data;

#ifdef FB_READY_RESET
	cancel_work_sync(&rmi4_data->reset_work);
	flush_workqueue(rmi4_data->reset_workqueue);
	destroy_workqueue(rmi4_data->reset_workqueue);
#endif

	cancel_delayed_work_sync(&exp_data.work);
	flush_workqueue(exp_data.workqueue);
	destroy_workqueue(exp_data.workqueue);

	cancel_delayed_work_sync(&rmi4_data->rb_work);
	flush_workqueue(rmi4_data->rb_workqueue);
	destroy_workqueue(rmi4_data->rb_workqueue);

#ifdef CONFIG_TOUCH_DEBUG_FS
	debugfs_remove_recursive(rmi4_data->debugfs);
#endif

	sysfs_remove_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_panel_vendor.attr);
#if defined(CONFIG_SECURE_TOUCH)
	sysfs_remove_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_secure_touch_enable.attr);
	sysfs_remove_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_secure_touch.attr);
#endif
	sysfs_remove_file(&rmi4_data->pdev->dev.parent->kobj, &dev_attr_panel_color.attr);

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		sysfs_remove_file(&rmi4_data->input_dev->dev.kobj,
				&attrs[attr_count].attr);
	}

	if (rmi4_data->board_prop_dir) {
		sysfs_remove_file(rmi4_data->board_prop_dir,
				&virtual_key_map_attr.attr);
		kobject_put(rmi4_data->board_prop_dir);
	}

	synaptics_rmi4_irq_enable(rmi4_data, false, false);

#ifdef CONFIG_DRM
	drm_unregister_client(&rmi4_data->drm_notifier);
#endif

#ifdef USE_EARLYSUSPEND
	unregister_early_suspend(&rmi4_data->early_suspend);
#endif

	synaptics_rmi4_empty_fn_list(rmi4_data);
	input_unregister_device(rmi4_data->input_dev);
	rmi4_data->input_dev = NULL;
	if (rmi4_data->stylus_enable) {
		input_unregister_device(rmi4_data->stylus_dev);
		rmi4_data->stylus_dev = NULL;
	}

	synaptics_rmi4_gpio_setup(bdata->irq_gpio, false, 0, 0, NULL);

	if (bdata->reset_gpio >= 0)
		synaptics_rmi4_gpio_setup(bdata->reset_gpio, false, 0, 0, NULL);

	if (bdata->power_gpio >= 0)
		synaptics_rmi4_gpio_setup(bdata->power_gpio, false, 0, 0, NULL);

	synaptics_rmi4_enable_reg(rmi4_data, false);
	synaptics_rmi4_get_reg(rmi4_data, false);

	kfree(rmi4_data);

	return 0;
}

static void synaptics_rmi4_f11_wg(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	int retval;
	unsigned char reporting_control;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
		if (fhandler->fn_number == SYNAPTICS_RMI4_F11)
			break;
	}

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.ctrl_base,
			&reporting_control,
			sizeof(reporting_control));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to change reporting mode\n",
				__func__);
		return;
	}

	reporting_control = (reporting_control & ~MASK_3BIT);
	if (enable)
		reporting_control |= F11_WAKEUP_GESTURE_MODE;
	else
		reporting_control |= F11_CONTINUOUS_MODE;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			fhandler->full_addr.ctrl_base,
			&reporting_control,
			sizeof(reporting_control));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to change reporting mode\n",
				__func__);
		return;
	}

	return;
}

static void drm_regulator_ctrl(struct synaptics_rmi4_data *rmi4_data, unsigned int flag, bool enable)
{
	int retval = 0;
	static unsigned int status = 0;

	if (rmi4_data == NULL)
		return;
	if (!rmi4_data->hw_if->board_data->panel_is_incell)
		return;

	if (enable) {
		if (rmi4_data->disp_reg && (flag & DISP_REG_VDD) && !(status & DISP_REG_VDD)) {
			retval = regulator_enable(rmi4_data->disp_reg);
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to enable disp regulator\n",
						__func__);
				return;
			}

			status |= DISP_REG_VDD;
		}

		if (rmi4_data->lab_reg && (flag & DISP_REG_LAB) && !(status & DISP_REG_LAB)) {
			retval = regulator_enable(rmi4_data->lab_reg);
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to enable lab regulator\n",
						__func__);
				return;
			}

			status |= DISP_REG_LAB;
		}

		if (rmi4_data->ibb_reg && (flag & DISP_REG_IBB) && !(status & DISP_REG_IBB)) {
			retval = regulator_enable(rmi4_data->ibb_reg);
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to enable ibb regulator\n",
						__func__);
				return;
			}

			status |= DISP_REG_IBB;
		}
	} else {
		if (rmi4_data->ibb_reg && (flag & DISP_REG_IBB) && (status & DISP_REG_IBB)) {
			retval = regulator_disable(rmi4_data->ibb_reg);
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to disable ibb regulator\n",
						__func__);
				return;
			}

			status &= ~DISP_REG_IBB;
		}

		if (rmi4_data->lab_reg && (flag & DISP_REG_LAB) && (status & DISP_REG_LAB)) {
			retval = regulator_disable(rmi4_data->lab_reg);
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to disable lab regulator\n",
						__func__);
				return;
			}

			status &= ~DISP_REG_LAB;
		}

		if (rmi4_data->disp_reg && (flag & DISP_REG_VDD) && (status & DISP_REG_VDD)) {
			retval = regulator_disable(rmi4_data->disp_reg);
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to disable disp regulator\n",
						__func__);
				return;
			}

			status &= ~DISP_REG_VDD;
		}
	}
}

static void drm_reset_ctrl(const struct synaptics_dsx_board_data *bdata, bool on)
{
	if (!bdata->panel_is_incell)
		return;

	if (bdata->drm_reset > 0) {
		if (on)
			gpio_set_value(bdata->drm_reset, bdata->drm_reset_state);
		else
			gpio_set_value(bdata->drm_reset, !bdata->drm_reset_state);
	}
}

#if 0
static void mdss_panel_poweron(struct synaptics_rmi4_data *rmi4_data, bool enable)
{
	if (!rmi4_data->hw_if->board_data->panel_is_incell)
		return;

	if (enable) {
#if 0
		/*disp regulator always on, so do not control this regulator*/
		if (rmi4_data->panel_power_seq.disp_pre_on_sleep)
			msleep(rmi4_data->panel_power_seq.disp_pre_on_sleep);
		mdss_regulator_ctrl(rmi4_data, DISP_REG_VDD, true);
		if (rmi4_data->panel_power_seq.disp_post_on_sleep)
			msleep(rmi4_data->panel_power_seq.disp_post_on_sleep);
#endif
		if (rmi4_data->panel_power_seq.lab_pre_on_sleep)
			msleep(rmi4_data->panel_power_seq.lab_pre_on_sleep);
		mdss_regulator_ctrl(rmi4_data, DISP_REG_LAB, true);
		if (rmi4_data->panel_power_seq.lab_post_on_sleep)
			msleep(rmi4_data->panel_power_seq.lab_post_on_sleep);

		if (rmi4_data->panel_power_seq.ibb_pre_on_sleep)
			msleep(rmi4_data->panel_power_seq.ibb_pre_on_sleep);
		mdss_regulator_ctrl(rmi4_data, DISP_REG_IBB, true);
		if (rmi4_data->panel_power_seq.ibb_post_on_sleep)
			msleep(rmi4_data->panel_power_seq.ibb_post_on_sleep);
	} else {
		if (rmi4_data->panel_power_seq.ibb_pre_off_sleep)
			msleep(rmi4_data->panel_power_seq.ibb_pre_off_sleep);
		mdss_regulator_ctrl(rmi4_data, DISP_REG_IBB, false);
		if (rmi4_data->panel_power_seq.ibb_post_off_sleep)
			msleep(rmi4_data->panel_power_seq.ibb_post_off_sleep);

		if (rmi4_data->panel_power_seq.lab_pre_off_sleep)
			msleep(rmi4_data->panel_power_seq.lab_pre_off_sleep);
		mdss_regulator_ctrl(rmi4_data, DISP_REG_LAB, false);
		if (rmi4_data->panel_power_seq.lab_post_off_sleep)
			msleep(rmi4_data->panel_power_seq.lab_post_off_sleep);
#if 0
		/*disp regulator always on, so do not control this regulator*/
		if (rmi4_data->panel_power_seq.disp_pre_off_sleep)
			msleep(rmi4_data->panel_power_seq.disp_pre_off_sleep);
		mdss_regulator_ctrl(rmi4_data, DISP_REG_VDD, false);
		if (rmi4_data->panel_power_seq.disp_post_off_sleep)
			msleep(rmi4_data->panel_power_seq.disp_post_off_sleep);
#endif
	}
	pr_debug("power %s seq:\n", enable ? "on" : "off");
#if 0
	/*disp regulator always on, so do not control this regulator*/
	pr_debug("IOVDD: preonsleep=%d,postonsleep=%d,preoffsleep=%d,postoffsleep=%d\n",
			rmi4_data->panel_power_seq.disp_pre_on_sleep,
			rmi4_data->panel_power_seq.disp_post_on_sleep,
			rmi4_data->panel_power_seq.disp_pre_off_sleep,
			rmi4_data->panel_power_seq.disp_post_off_sleep);
#endif
	pr_debug("LAB: preonsleep=%d,postonsleep=%d,preoffsleep=%d,postoffsleep=%d\n",
			rmi4_data->panel_power_seq.lab_pre_on_sleep,
			rmi4_data->panel_power_seq.lab_post_on_sleep,
			rmi4_data->panel_power_seq.lab_pre_off_sleep,
			rmi4_data->panel_power_seq.lab_post_off_sleep);
	pr_debug("IBB: preonsleep=%d,postonsleep=%d,preoffsleep=%d,postoffsleep=%d\n",
			rmi4_data->panel_power_seq.ibb_pre_on_sleep,
			rmi4_data->panel_power_seq.ibb_post_on_sleep,
			rmi4_data->panel_power_seq.ibb_pre_off_sleep,
			rmi4_data->panel_power_seq.ibb_post_off_sleep);
}
#endif
static void drm_reset_action(const struct synaptics_dsx_board_data *bdata)
{
	if (bdata->drm_reset > 0) {
		gpio_set_value(bdata->drm_reset, !bdata->drm_reset_state);
		msleep(10);
		gpio_set_value(bdata->drm_reset, bdata->drm_reset_state);
		msleep(10);
	}
}

static void synaptics_rmi4_f12_wg(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	int retval;
	unsigned char offset;
	unsigned char reporting_control[3];
	struct synaptics_rmi4_f12_extra_data *extra_data;
	struct synaptics_rmi4_fn *fhandler;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	list_for_each_entry(fhandler, &rmi->support_fn_list, link) {
		if (fhandler->fn_number == SYNAPTICS_RMI4_F12)
			break;
	}

	extra_data = (struct synaptics_rmi4_f12_extra_data *)fhandler->extra;
	offset = extra_data->ctrl20_offset;

	retval = synaptics_rmi4_reg_read(rmi4_data,
			fhandler->full_addr.ctrl_base + offset,
			reporting_control,
			sizeof(reporting_control));
	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to change reporting mode\n",
				__func__);
		return;
	}

	if (enable)
		reporting_control[2] = F12_WAKEUP_GESTURE_MODE;
	else
		reporting_control[2] = F12_CONTINUOUS_MODE;

	retval = synaptics_rmi4_reg_write(rmi4_data,
			fhandler->full_addr.ctrl_base + offset,
			reporting_control,
			sizeof(reporting_control));

	if (retval < 0) {
		dev_err(rmi4_data->pdev->dev.parent,
				"%s: Failed to change reporting mode\n",
				__func__);
		return;
	}

	return;
}

static void synaptics_rmi4_wakeup_gesture(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	if (rmi4_data->f11_wakeup_gesture)
		synaptics_rmi4_f11_wg(rmi4_data, enable);
	else if (rmi4_data->f12_wakeup_gesture)
		synaptics_rmi4_f12_wg(rmi4_data, enable);

	return;
}

#ifdef CONFIG_DRM
static int synaptics_rmi4_drm_notifier_cb(struct notifier_block *self,
		unsigned long event, void *data)
{
	int *transition;
	struct drm_notify_data *evdata = data;
	struct synaptics_rmi4_data *rmi4_data =
			container_of(self, struct synaptics_rmi4_data,
			drm_notifier);

	const struct synaptics_dsx_board_data *bdata = NULL;

	if (rmi4_data->hw_if->board_data)
		bdata = rmi4_data->hw_if->board_data;
	else
		return 0;

	/* Receive notifications from primary panel only */
	if (evdata && evdata->data && rmi4_data && evdata->is_primary) {
		if (event == DRM_EVENT_BLANK) {
			transition = evdata->data;
			if (*transition == DRM_BLANK_POWERDOWN) {
				synaptics_rmi4_suspend(&rmi4_data->pdev->dev);
				rmi4_data->fb_ready = false;
			} else if (*transition == DRM_BLANK_UNBLANK) {
				synaptics_rmi4_resume(&rmi4_data->pdev->dev);
				rmi4_data->fb_ready = true;
				if (rmi4_data->wakeup_en) {
					drm_panel_reset_skip_enable(false);
					//drm_regulator_ctrl(rmi4_data, DISP_REG_ALL, false);
					drm_dsi_ulps_enable(false);
					rmi4_data->wakeup_en = false;
				}

				rmi4_data->disable_data_dump = false;
			}
		} else if (event == DRM_EARLY_EVENT_BLANK) {
			transition = evdata->data;
			if (*transition == DRM_BLANK_POWERDOWN) {
				rmi4_data->disable_data_dump = true;
				if (rmi4_data->dump_flags) {
					reinit_completion(&rmi4_data->dump_completion);
					wait_for_completion_timeout(&rmi4_data->dump_completion, 4 * HZ);
				}

				if (rmi4_data->enable_wakeup_gesture) {
					rmi4_data->wakeup_en = true;
					//drm_regulator_ctrl(rmi4_data, DISP_REG_ALL, true);
					drm_panel_reset_skip_enable(true);
					drm_dsi_ulps_enable(true);
				}
			} else if (*transition == DRM_BLANK_UNBLANK) {
				if (bdata->reset_gpio >= 0 && rmi4_data->suspend) {
					gpio_set_value(bdata->reset_gpio, bdata->reset_on_state);
					msleep(bdata->reset_active_ms);
					gpio_set_value(bdata->reset_gpio, !bdata->reset_on_state);
				}
				if (rmi4_data->wakeup_en) {
					drm_reset_action(bdata);
				}
			}
		}
	}
	return 0;
}

static int synaptics_rmi4_drm_notifier_cb_tddi(struct notifier_block *self,
		unsigned long event, void *data)
{
#if 0
	int *transition;
	struct fb_event *evdata = data;
	struct synaptics_rmi4_data *rmi4_data =
			container_of(self, struct synaptics_rmi4_data,
			fb_notifier);

	if (mdss_prim_panel_is_dead())
		return 0;

	/* Receive notifications from primary panel only */
	if (evdata && evdata->data && rmi4_data && mdss_panel_is_prim(evdata->info)) {
		if (event == FB_EVENT_BLANK) {
			transition = evdata->data;
			if ((*transition == FB_BLANK_UNBLANK) || (*transition == FB_BLANK_NORMAL)) {
				if (rmi4_data->wakeup_en) {
					mdss_panel_reset_skip_enable(false);
					mdss_regulator_ctrl(rmi4_data, DISP_REG_ALL, false);
					rmi4_data->wakeup_en = false;
				} else {
					synaptics_rmi4_resume(&rmi4_data->pdev->dev);
					rmi4_data->fb_ready = true;
				}

				rmi4_data->disable_data_dump = false;
			} else if ((*transition == FB_BLANK_POWERDOWN) || (*transition == FB_BLANK_NORMAL)) {
				if (rmi4_data->wakeup_en) {
					synaptics_rmi4_suspend(&rmi4_data->pdev->dev);
					rmi4_data->fb_ready = true;
				}
			}
		} else if (event == FB_EARLY_EVENT_BLANK) {
			transition = evdata->data;
			if (*transition == FB_BLANK_UNBLANK) {
				if (rmi4_data->wakeup_en) {
					synaptics_rmi4_resume(&rmi4_data->pdev->dev);
					rmi4_data->fb_ready = true;
					msleep(30);
				}
			} else if ((*transition == FB_BLANK_POWERDOWN) || (*transition == FB_BLANK_NORMAL)) {
				rmi4_data->disable_data_dump = true;
				if (rmi4_data->dump_flags) {
					reinit_completion(&rmi4_data->dump_completion);
					wait_for_completion_timeout(&rmi4_data->dump_completion, 4 * HZ);
				}

				if (rmi4_data->enable_wakeup_gesture) {
					rmi4_data->wakeup_en = true;
					mdss_panel_reset_skip_enable(true);
					mdss_regulator_ctrl(rmi4_data, DISP_REG_ALL, true);
				}

				if (!rmi4_data->wakeup_en) {
					synaptics_rmi4_suspend(&rmi4_data->pdev->dev);
					rmi4_data->fb_ready = false;
				}
			}
		}
	}
#endif
	return 0;
}

#endif

#ifdef USE_EARLYSUSPEND
static void synaptics_rmi4_early_suspend(struct early_suspend *h)
{
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data =
			container_of(h, struct synaptics_rmi4_data,
			early_suspend);

	if (rmi4_data->stay_awake)
		return;

	if (rmi4_data->enable_wakeup_gesture) {
		synaptics_rmi4_wakeup_gesture(rmi4_data, true);
		enable_irq_wake(rmi4_data->irq);
		goto exit;
	}

	synaptics_rmi4_irq_enable(rmi4_data, false, false);
	synaptics_rmi4_sleep_enable(rmi4_data, true);
	synaptics_rmi4_free_fingers(rmi4_data);

exit:
	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->early_suspend != NULL)
				exp_fhandler->exp_fn->early_suspend(rmi4_data);
	}
	mutex_unlock(&exp_data.mutex);

	rmi4_data->suspend = true;

	return;
}

static void synaptics_rmi4_late_resume(struct early_suspend *h)
{
#ifdef FB_READY_RESET
	int retval;
#endif
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data =
			container_of(h, struct synaptics_rmi4_data,
			early_suspend);

	if (rmi4_data->stay_awake)
		return;

	if (rmi4_data->enable_wakeup_gesture) {
		synaptics_rmi4_wakeup_gesture(rmi4_data, false);
		disable_irq_wake(rmi4_data->irq);
		goto exit;
	}

	rmi4_data->current_page = MASK_8BIT;

	if (rmi4_data->suspend) {
		synaptics_rmi4_sleep_enable(rmi4_data, false);
		synaptics_rmi4_irq_enable(rmi4_data, true, false);
	}

exit:
#ifdef FB_READY_RESET
	if (rmi4_data->suspend) {
		retval = synaptics_rmi4_reset_device(rmi4_data, false);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to issue reset command\n",
					__func__);
		}
	}
#endif
	mutex_lock(&exp_data.mutex);
	if (!list_empty(&exp_data.list)) {
		list_for_each_entry(exp_fhandler, &exp_data.list, link)
			if (exp_fhandler->exp_fn->late_resume != NULL)
				exp_fhandler->exp_fn->late_resume(rmi4_data);
	}
	mutex_unlock(&exp_data.mutex);

	rmi4_data->suspend = false;

	return;
}
#endif

static int synaptics_rmi4_suspend(struct device *dev)
{
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	const struct synaptics_dsx_board_data *bdata =
		rmi4_data->hw_if->board_data;

	if (rmi4_data->stay_awake || rmi4_data->suspend)
		return 0;

	if (bdata->cut_off_power || (rmi4_data->chip_is_tddi && !rmi4_data->wakeup_en)) {
		if (rmi4_data->fw_updating)
			return 0;

		synaptics_rmi4_irq_enable(rmi4_data, false, false);

		if (bdata->power_gpio >= 0)
			gpio_set_value(bdata->power_gpio,
					!bdata->power_on_state);

		if (bdata->reset_gpio >= 0) {
			gpio_set_value(bdata->reset_gpio,
					bdata->reset_on_state);
			mdelay(bdata->reset_active_ms);
		}

		synaptics_rmi4_enable_reg(rmi4_data, false);
	} else {
		synaptics_secure_touch_stop(rmi4_data, 1);

		synaptics_rmi4_irq_enable(rmi4_data, false, false);

		if (!rmi4_data->wakeup_en)
			synaptics_rmi4_sleep_enable(rmi4_data, true);
		else {
			if (rmi4_data->chip_is_tddi)
				msleep(120);
			synaptics_rmi4_wakeup_gesture(rmi4_data, true);
			synaptics_rmi4_irq_enable(rmi4_data, true, false);
			goto exit;
		}

		mutex_lock(&exp_data.mutex);
		if (!list_empty(&exp_data.list)) {
			list_for_each_entry(exp_fhandler, &exp_data.list, link)
				if (exp_fhandler->exp_fn->suspend != NULL)
					exp_fhandler->exp_fn->suspend(rmi4_data);
		}
		mutex_unlock(&exp_data.mutex);
	}

exit:
	synaptics_rmi4_free_fingers(rmi4_data);

	rmi4_data->suspend = true;

	return 0;
}

static int synaptics_rmi4_resume(struct device *dev)
{
#ifdef FB_READY_RESET
	int retval;
#endif
	struct synaptics_rmi4_exp_fhandler *exp_fhandler;
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	const struct synaptics_dsx_board_data *bdata =
		rmi4_data->hw_if->board_data;

#ifdef CONFIG_DRM
	static int skip = 0;

	if (skip == 0) {
		skip = 1;
		return 0;
	}
#endif

	if (rmi4_data->stay_awake || !rmi4_data->suspend)
		return 0;

	if (bdata->cut_off_power || (rmi4_data->chip_is_tddi && !rmi4_data->wakeup_en)) {
		synaptics_rmi4_enable_reg(rmi4_data, true);

		if (bdata->power_gpio >= 0) {
			gpio_set_value(bdata->power_gpio,
					bdata->power_on_state);
			mdelay(bdata->power_delay_ms);
		}

		if (bdata->reset_gpio >= 0) {
			gpio_set_value(bdata->reset_gpio,
					!bdata->reset_on_state);
			mdelay(bdata->reset_delay_ms);
		}

		synaptics_rmi4_irq_enable(rmi4_data, true, false);
	} else {
		synaptics_secure_touch_stop(rmi4_data, 0);

		if (rmi4_data->wakeup_en) {
			synaptics_rmi4_wakeup_gesture(rmi4_data, false);
		} else {
			rmi4_data->current_page = MASK_8BIT;

			synaptics_rmi4_sleep_enable(rmi4_data, false);
			synaptics_rmi4_irq_enable(rmi4_data, true, false);

#ifdef FB_READY_RESET
			retval = synaptics_rmi4_reset_device(rmi4_data, false);
			if (retval < 0) {
				dev_err(rmi4_data->pdev->dev.parent,
						"%s: Failed to issue reset command\n",
						__func__);
			}
#endif
			mutex_lock(&exp_data.mutex);
			if (!list_empty(&exp_data.list)) {
				list_for_each_entry(exp_fhandler, &exp_data.list, link)
					if (exp_fhandler->exp_fn->resume != NULL)
						exp_fhandler->exp_fn->resume(rmi4_data);
			}
			mutex_unlock(&exp_data.mutex);
		}
	}

	rmi4_data->suspend = false;

	if (rmi4_data->enable_cover_mode)
		cover_mode_set(rmi4_data, rmi4_data->enable_cover_mode);

	return 0;
}

#ifdef CONFIG_PM
static int synaptics_rmi4_pm_suspend(struct device *dev)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	const struct synaptics_dsx_board_data *bdata =
		rmi4_data->hw_if->board_data;

	if (device_may_wakeup(dev) &&
			rmi4_data->wakeup_en &&
			!bdata->cut_off_power) {
		dev_info(rmi4_data->pdev->dev.parent,
			"Enable touch irq wake\n");
		disable_irq(rmi4_data->irq);
		enable_irq_wake(rmi4_data->irq);
	}

	return 0;

}

static int synaptics_rmi4_pm_resume(struct device *dev)
{
	struct synaptics_rmi4_data *rmi4_data = dev_get_drvdata(dev);
	const struct synaptics_dsx_board_data *bdata =
		rmi4_data->hw_if->board_data;

	if (device_may_wakeup(dev) &&
			rmi4_data->wakeup_en &&
			!bdata->cut_off_power) {
		dev_info(rmi4_data->pdev->dev.parent,
			"Disable touch irq wake\n");
		disable_irq_wake(rmi4_data->irq);
		enable_irq(rmi4_data->irq);
	}

	return 0;
}

static const struct dev_pm_ops synaptics_rmi4_dev_pm_ops = {
	.suspend = synaptics_rmi4_pm_suspend,
	.resume  = synaptics_rmi4_pm_resume,
};
#endif

static struct platform_driver synaptics_rmi4_driver = {
	.driver = {
		.name = PLATFORM_DRIVER_FORCE,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &synaptics_rmi4_dev_pm_ops,
#endif
	},
	.probe = synaptics_rmi4_probe,
	.remove = synaptics_rmi4_remove,
};

static int __init synaptics_rmi4_init(void)
{
	int retval;

	retval = synaptics_rmi4_bus_init_force();
	if (retval)
		return retval;

	return platform_driver_register(&synaptics_rmi4_driver);
}

static void __exit synaptics_rmi4_exit(void)
{
	platform_driver_unregister(&synaptics_rmi4_driver);

	synaptics_rmi4_bus_exit_force();

	return;
}

module_init(synaptics_rmi4_init);
module_exit(synaptics_rmi4_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX Touch Driver");
MODULE_LICENSE("GPL v2");
