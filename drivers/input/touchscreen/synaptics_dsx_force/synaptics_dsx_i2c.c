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
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/input/synaptics_dsx_force.h>
#include "synaptics_dsx_core.h"

#define SYN_I2C_RETRY_TIMES 4

#define I2C_BURST_LIMIT 2048

#define MAX_WRITE_SIZE 4096

static unsigned char *wr_buf;

static struct synaptics_dsx_hw_interface hw_if;

static struct platform_device *synaptics_dsx_i2c_device;

#ifdef CONFIG_OF
static int parse_dt(struct device *dev, struct synaptics_dsx_board_data *bdata)
{
	int retval;
	u32 value;
	const char *name;
	struct synaptics_dsx_config_info *config_info;
	struct synaptics_dsx_factory_param *factory_param;
	struct property *prop;
	struct device_node *temp, *np = dev->of_node;

	bdata->irq_gpio = of_get_named_gpio_flags(np,
			"synaptics,irq-gpio", 0, NULL);
	if (bdata->irq_gpio < 0)
		return -EINVAL;

	retval = of_property_read_u32(np, "synaptics,irq-on-state",
			&value);
	if (retval < 0)
		bdata->irq_on_state = 0;
	else
		bdata->irq_on_state = value;

	retval = of_property_read_u32(np, "synaptics,irq-flags", &value);
	if (retval < 0)
		return retval;
	else
		bdata->irq_flags = value;

	retval = of_property_read_string(np, "synaptics,pwr-reg-name", &name);
	if (retval == -EINVAL)
		bdata->pwr_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->pwr_reg_name = name;

	retval = of_property_read_string(np, "synaptics,bus-reg-name", &name);
	if (retval == -EINVAL)
		bdata->bus_reg_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->bus_reg_name = name;

	bdata->panel_is_incell = of_property_read_bool(np, "synaptics,panel-is-incell");

	if (bdata->panel_is_incell) {
		retval = of_property_read_string(np, "synaptics,lab-reg-name", &name);
		if (retval == -EINVAL)
			bdata->lab_reg_name = NULL;
		else if (retval < 0)
			return retval;
		else
			bdata->lab_reg_name = name;

		retval = of_property_read_string(np, "synaptics,ibb-reg-name", &name);
		if (retval == -EINVAL)
			bdata->ibb_reg_name = NULL;
		else if (retval < 0)
			return retval;
		else
			bdata->ibb_reg_name = name;

		retval = of_property_read_string(np, "synaptics,disp-reg-name", &name);
		if (retval == -EINVAL)
			bdata->disp_reg_name = NULL;
		else if (retval < 0)
			return retval;
		else
			bdata->disp_reg_name = name;

		bdata->disp_power_hold = of_property_read_bool(np, "synaptics,disp-power-hold");
	}

	retval = of_property_read_string(np, "synaptics,power-gpio-name", &name);
	if (retval == -EINVAL)
		bdata->power_gpio_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->power_gpio_name = name;

	retval = of_property_read_string(np, "synaptics,reset-gpio-name", &name);
	if (retval == -EINVAL)
		bdata->reset_gpio_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->reset_gpio_name = name;

	retval = of_property_read_string(np, "synaptics,irq-gpio-name", &name);
	if (retval == -EINVAL)
		bdata->irq_gpio_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->irq_gpio_name = name;

	bdata->cut_off_power = of_property_read_bool(np, "synaptics,cut-off-power");

	bdata->power_gpio = of_get_named_gpio_flags(np,
			"synaptics,power-gpio", 0, NULL);
	if (bdata->power_gpio >= 0) {
		retval = of_property_read_u32(np, "synaptics,power-on-state",
				&value);
		if (retval < 0)
			return retval;
		else
			bdata->power_on_state = value;
	} else
		bdata->power_gpio = -1;

	retval = of_property_read_u32(np, "synaptics,power-delay-ms",
			&value);
	if (retval < 0)
		bdata->power_delay_ms = 0;	/* No power delay by default */
	else
		bdata->power_delay_ms = value;

	bdata->drm_reset = of_get_named_gpio_flags(np,
			"synaptics,drm-dsi-reset", 0, NULL);
	if (bdata->drm_reset >= 0) {
		retval = of_property_read_u32(np, "synaptics,drm-reset-state",
				&value);
		if (retval < 0)
			return retval;
		else
			bdata->drm_reset_state = value;
	} else
		bdata->drm_reset = -1;

	bdata->reset_gpio = of_get_named_gpio_flags(np,
			"synaptics,reset-gpio", 0, NULL);
	if (bdata->reset_gpio >= 0) {
		retval = of_property_read_u32(np, "synaptics,reset-on-state",
				&value);
		if (retval < 0)
			return retval;
		else
			bdata->reset_on_state = value;

		retval = of_property_read_u32(np, "synaptics,reset-active-ms",
				&value);
		if (retval < 0)
			return retval;
		else
			bdata->reset_active_ms = value;
	} else
		bdata->reset_gpio = -1;

	retval = of_property_read_u32(np, "synaptics,reset-delay-ms",
			&value);
	if (retval < 0)
		bdata->reset_delay_ms = 0;	/* No reset delay by default */
	else
		bdata->reset_delay_ms = value;

	retval = of_property_read_u32(np, "synaptics,max-y-for-2d",
			&value);
	if (retval < 0)
		bdata->max_y_for_2d = -1;
	else
		bdata->max_y_for_2d = value;

	bdata->swap_axes = of_property_read_bool(np, "synaptics,swap-axes");

	bdata->x_flip = of_property_read_bool(np, "synaptics,x-flip");

	bdata->y_flip = of_property_read_bool(np, "synaptics,y-flip");

	retval = of_property_read_u32(np, "synaptics,ub-i2c-addr",
			&value);
	if (retval < 0)
		bdata->ub_i2c_addr = -1;
	else
		bdata->ub_i2c_addr = (unsigned short)value;

	retval = of_property_read_string(np, "synaptics,backup-fw", &name);
	if (retval == -EINVAL)
		bdata->backup_fw_name = NULL;
	else if (retval < 0)
		return retval;
	else
		bdata->backup_fw_name = name;

	prop = of_find_property(np, "synaptics,cap-button-codes", NULL);
	if (prop && prop->length) {
		bdata->cap_button_map->map = devm_kzalloc(dev,
				prop->length,
				GFP_KERNEL);
		if (!bdata->cap_button_map->map)
			return -ENOMEM;
		bdata->cap_button_map->nbuttons = prop->length / sizeof(u32);
		retval = of_property_read_u32_array(np,
				"synaptics,cap-button-codes",
				bdata->cap_button_map->map,
				bdata->cap_button_map->nbuttons);
		if (retval < 0) {
			bdata->cap_button_map->nbuttons = 0;
			bdata->cap_button_map->map = NULL;
		}
	} else {
		bdata->cap_button_map->nbuttons = 0;
		bdata->cap_button_map->map = NULL;
	}

	retval = of_property_read_string(np, "synaptics,short-jdi-25", &bdata->short_test25);
	if (retval && (retval != -EINVAL)) {
		dev_err(dev, "Unable to read jdi short type 25 value\n");
		bdata->short_test25 = NULL;
	}

	retval = of_property_read_string(np, "synaptics,short-jdi-26", &bdata->short_test26);
	if (retval && (retval != -EINVAL)) {
		dev_err(dev, "Unable to read jdi short type 26 value\n");
		bdata->short_test26 = NULL;
	}

	prop = of_find_property(np, "synaptics,vir-button-codes", NULL);
	if (prop && prop->length) {
		bdata->vir_button_map->map = devm_kzalloc(dev,
				prop->length,
				GFP_KERNEL);
		if (!bdata->vir_button_map->map)
			return -ENOMEM;
		bdata->vir_button_map->nbuttons = prop->length / sizeof(u32);
		bdata->vir_button_map->nbuttons /= 5;
		retval = of_property_read_u32_array(np,
				"synaptics,vir-button-codes",
				bdata->vir_button_map->map,
				bdata->vir_button_map->nbuttons * 5);
		if (retval < 0) {
			bdata->vir_button_map->nbuttons = 0;
			bdata->vir_button_map->map = NULL;
		}
	} else {
		bdata->vir_button_map->nbuttons = 0;
		bdata->vir_button_map->map = NULL;
	}

	if (of_property_read_bool(np, "synaptics,product-id-as-lockdown"))
		bdata->lockdown_area = LOCKDOWN_AREA_PRODUCT_ID;
	else if (of_property_read_bool(np, "synaptics,guest-serialization-as-lockdown"))
		bdata->lockdown_area = LOCKDOWN_AREA_GUEST_SERIALIZATION;
	else
		bdata->lockdown_area = LOCKDOWN_AREA_UNKNOWN;

	prop = of_find_property(np, "synaptics,tp-id-byte", NULL);
	if (prop && prop->length) {
		bdata->tp_id_bytes = devm_kzalloc(dev,
				prop->length,
				GFP_KERNEL);
		if (!bdata->tp_id_bytes)
			return -ENOMEM;
		bdata->tp_id_num = prop->length / sizeof(u8);
		retval = of_property_read_u8_array(np,
				"synaptics,tp-id-byte",
				bdata->tp_id_bytes,
				bdata->tp_id_num);
		if (retval < 0) {
			bdata->tp_id_num = 0;
			bdata->tp_id_bytes = NULL;
		}
	} else {
		dev_err(dev, "Don't know which byte of lockdown info to distinguish TP\n");
		bdata->tp_id_num = 0;
		bdata->tp_id_bytes = NULL;
	}

	retval = of_property_read_u32(np, "synaptics,config-array-size",
		&bdata->config_array_size);
	if (retval < 0) {
		dev_err(dev, "Cannot get config array size\n");
		return retval;
	}

	bdata->config_array = devm_kzalloc(dev, bdata->config_array_size *
					sizeof(struct synaptics_dsx_config_info), GFP_KERNEL);
	if (!bdata->config_array) {
		dev_err(dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	config_info = bdata->config_array;
	for_each_child_of_node(np, temp) {
		retval = of_property_read_u32(temp, "synaptics,chip-id",
				&value);
		if (retval < 0)
			config_info->chip_id = -1;
		else
			config_info->chip_id = value;

		retval = of_property_read_string(temp, "synaptics,chip-id-name",
			&config_info->chip_id_name);
		if (retval && (retval != -EINVAL)) {
			dev_err(dev, "Unable to read chip id name\n");
			return retval;
		}

		config_info->chip_is_tddi = of_property_read_bool(temp, "synaptics,chip-is-tddi");
		config_info->open_test_b7 = of_property_read_bool(temp, "synaptics,open-test-b7");
		config_info->short_test_extend = of_property_read_bool(temp, "synaptics,short-test-extend");
		config_info->is_factory_param = of_property_read_bool(temp, "synaptics,factory-param");

		if (config_info->is_factory_param) {
			factory_param = devm_kzalloc(dev, bdata->config_array_size *
						sizeof(struct synaptics_dsx_factory_param), GFP_KERNEL);
			if (!factory_param) {
				dev_err(dev, "Unable to allocate memory\n");
				return -ENOMEM;
			}

			config_info->factory_param = factory_param;

			retval = of_property_read_u32(temp,
				"synaptics,tddi-short-limit_b", &value);
			if (!retval) {
				factory_param->tddi_short_limit_b = value;
			} else {
				factory_param->tddi_short_limit_b = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,tddi-noise-limit", &value);
			if (!retval) {
				factory_param->tddi_noise_limit = value;
			} else {
				factory_param->tddi_noise_limit = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,tddi-extend-ee-short-reset-dur", &value);
			if (!retval) {
				factory_param->tddi_extend_ee_short_reset_dur = value;
			} else {
				factory_param->tddi_extend_ee_short_reset_dur = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,tddi-extend-ee-short-int_dur", &value);
			if (!retval) {
				factory_param->tddi_extend_ee_short_int_dur = value;
			} else {
				factory_param->tddi_extend_ee_short_int_dur = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,tddi-extend-ee-short-tx-on-count", &value);
			if (!retval) {
				factory_param->tddi_extend_ee_short_tx_on_count = value;
			} else {
				factory_param->tddi_extend_ee_short_tx_on_count = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,tddi-extend-ee-short-rx-on-count", &value);
			if (!retval) {
				factory_param->tddi_extend_ee_short_rx_on_count = value;
			} else {
				factory_param->tddi_extend_ee_short_rx_on_count = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,tddi-extend-ee-short-test-limit-part1", &value);
			if (!retval) {
				factory_param->tddi_extend_ee_short_test_limit_part1 = value;
			} else {
				factory_param->tddi_extend_ee_short_test_limit_part1 = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,tddi-extend-ee-short-test-limit-part2", &value);
			if (!retval) {
				factory_param->tddi_extend_ee_short_test_limit_part2 = value;
			} else {
				factory_param->tddi_extend_ee_short_test_limit_part2 = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,tddi-open-test-int-dur-one", &value);
			if (!retval) {
				factory_param->tddi_open_test_int_dur_one = value;
			} else {
				factory_param->tddi_open_test_int_dur_one = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,tddi-open-test-int-dur-two", &value);
			if (!retval) {
				factory_param->tddi_open_test_int_dur_two = value;
			} else {
				factory_param->tddi_open_test_int_dur_two = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,tddi-open-test-limit-phase2-lower", &value);
			if (!retval) {
				factory_param->tddi_open_test_limit_phase2_lower = value;
			} else {
				factory_param->tddi_open_test_limit_phase2_lower = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,tddi-b7-open-test-int-dur-one", &value);
			if (!retval) {
				factory_param->tddi_b7_open_test_int_dur_one = value;
			} else {
				factory_param->tddi_b7_open_test_int_dur_one = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,tddi-b7-open-test-int-dur-two", &value);
			if (!retval) {
				factory_param->tddi_b7_open_test_int_dur_two = value;
			} else {
				factory_param->tddi_b7_open_test_int_dur_two = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,tddi-b7-open-test-limit-phase2-lower", &value);
			if (!retval) {
				factory_param->tddi_b7_open_test_limit_phase2_lower = value;
			} else {
				factory_param->tddi_b7_open_test_limit_phase2_lower = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,tddi-b7-open-test-limit-phase2-upper", &value);
			if (!retval) {
				factory_param->tddi_b7_open_test_limit_phase2_upper = value;
			} else {
				factory_param->tddi_b7_open_test_limit_phase2_upper = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,button-count", &value);
			if (!retval) {
				factory_param->button_count = value;
			} else {
				factory_param->button_count = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,abs-0d-open-factor", &value);
			if (!retval) {
				factory_param->abs_0d_open_factor = value;
			} else {
				factory_param->abs_0d_open_factor = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,abs-0d-open-test-limit", &value);
			if (!retval) {
				factory_param->abs_0d_open_test_limit = value;
			} else {
				factory_param->abs_0d_open_test_limit = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,elec-open-test-tx-on-count", &value);
			if (!retval) {
				factory_param->elec_open_test_tx_on_count = value;
			} else {
				factory_param->elec_open_test_tx_on_count = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,elec-open-test-rx-on-count", &value);
			if (!retval) {
				factory_param->elec_open_test_rx_on_count = value;
			} else {
				factory_param->elec_open_test_rx_on_count = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,elec-open-int-dur-one", &value);
			if (!retval) {
				factory_param->elec_open_int_dur_one = value;
			} else {
				factory_param->elec_open_int_dur_one = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,elec-open-int-dur-two", &value);
			if (!retval) {
				factory_param->elec_open_int_dur_two = value;
			} else {
				factory_param->elec_open_int_dur_two = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,elec-open-test-limit-one", &value);
			if (!retval) {
				factory_param->elec_open_test_limit_one = value;
			} else {
				factory_param->elec_open_test_limit_one = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,elec-open-test-limit-two", &value);
			if (!retval) {
				factory_param->elec_open_test_limit_two = value;
			} else {
				factory_param->elec_open_test_limit_two = 0;
			}

		}

		if (bdata->panel_is_incell) {
			retval = of_property_read_u32(temp,
				"synaptics,disp-pre-on-sleep", &value);
			if (!retval) {
				config_info->panel_power_seq.disp_pre_on_sleep = value;
			} else {
				config_info->panel_power_seq.disp_pre_on_sleep = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,disp-post-on-sleep", &value);
			if (!retval) {
				config_info->panel_power_seq.disp_post_on_sleep = value;
			} else {
				config_info->panel_power_seq.disp_post_on_sleep = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,disp-pre-off-sleep", &value);
			if (!retval) {
				config_info->panel_power_seq.disp_pre_off_sleep = value;
			} else {
				config_info->panel_power_seq.disp_pre_off_sleep = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,disp-post-off-sleep", &value);
			if (!retval) {
				config_info->panel_power_seq.disp_post_off_sleep = value;
			} else {
				config_info->panel_power_seq.disp_post_off_sleep = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,lab-pre-on-sleep", &value);
			if (!retval) {
				config_info->panel_power_seq.lab_pre_on_sleep = value;
			} else {
				config_info->panel_power_seq.lab_pre_on_sleep = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,lab-post-on-sleep", &value);
			if (!retval) {
				config_info->panel_power_seq.lab_post_on_sleep = value;
			} else {
				config_info->panel_power_seq.lab_post_on_sleep = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,lab-pre-off-sleep", &value);
			if (!retval) {
				config_info->panel_power_seq.lab_pre_off_sleep = value;
			} else {
				config_info->panel_power_seq.lab_pre_off_sleep = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,lab-post-off-sleep", &value);
			if (!retval) {
				config_info->panel_power_seq.lab_post_off_sleep = value;
			} else {
				config_info->panel_power_seq.lab_post_off_sleep = 0;
			}
			retval = of_property_read_u32(temp,
				"synaptics,ibb-pre-on-sleep", &value);
			if (!retval) {
				config_info->panel_power_seq.ibb_pre_on_sleep = value;
			} else {
				config_info->panel_power_seq.ibb_pre_on_sleep = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,ibb-post-on-sleep", &value);
			if (!retval) {
				config_info->panel_power_seq.ibb_post_on_sleep = value;
			} else {
				config_info->panel_power_seq.ibb_post_on_sleep = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,ibb-pre-off-sleep", &value);
			if (!retval) {
				config_info->panel_power_seq.ibb_pre_off_sleep = value;
			} else {
				config_info->panel_power_seq.ibb_pre_off_sleep = 0;
			}

			retval = of_property_read_u32(temp,
				"synaptics,ibb-post-off-sleep", &value);
			if (!retval) {
				config_info->panel_power_seq.ibb_post_off_sleep = value;
			} else {
				config_info->panel_power_seq.ibb_post_off_sleep = 0;
			}
		}

		prop = of_find_property(temp, "synaptics,tp-id", NULL);
		if (prop && prop->length) {
			if (bdata->tp_id_num != prop->length / sizeof(u8)) {
				dev_err(dev, "Invalid TP id length\n");
				return -EINVAL;
			}
			config_info->tp_ids = devm_kzalloc(dev,
					prop->length,
					GFP_KERNEL);
			if (!config_info->tp_ids)
				return -ENOMEM;
			retval = of_property_read_u8_array(temp,
					"synaptics,tp-id",
					config_info->tp_ids,
					bdata->tp_id_num);
			if (retval < 0) {
				dev_err(dev, "Error reading TP id\n");
				return -EINVAL;
			}
		} else if (bdata->tp_id_num == 0) {
			/* No TP id indicated, skip */
			config_info->tp_ids = NULL;
		} else {
			dev_err(dev, "Cannot find TP id\n");
			return -EINVAL;
		}

		retval = of_property_read_string(temp, "synaptics,fw-name",
			&config_info->fw_name);
		if (retval && (retval != -EINVAL)) {
			dev_err(dev, "Unable to read firmware name\n");
			return retval;
		}
		config_info++;
	};

	return 0;
}
#endif

static int synaptics_rmi4_i2c_alloc_buf(struct synaptics_rmi4_data *rmi4_data,
		unsigned int count)
{
	static unsigned int buf_size;

	if (count > buf_size) {
		if (buf_size)
			kfree(wr_buf);
		wr_buf = kzalloc(count, GFP_KERNEL);
		if (!wr_buf) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to alloc mem for buffer\n",
					__func__);
			buf_size = 0;
			return -ENOMEM;
		}
		buf_size = count;
	}

	return 0;
}

static void synaptics_rmi4_i2c_check_addr(struct synaptics_rmi4_data *rmi4_data,
		struct i2c_client *i2c)
{
	if (hw_if.board_data->ub_i2c_addr == -1)
		return;

	if (hw_if.board_data->i2c_addr == i2c->addr)
		hw_if.board_data->i2c_addr = hw_if.board_data->ub_i2c_addr;
	else
		hw_if.board_data->i2c_addr = i2c->addr;

	return;
}

static int synaptics_rmi4_i2c_set_page(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr)
{
	int retval;
	unsigned char retry;
	unsigned char buf[PAGE_SELECT_LEN];
	unsigned char page;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);
	struct i2c_msg msg[1];

	msg[0].addr = hw_if.board_data->i2c_addr;
	msg[0].flags = 0;
	msg[0].len = PAGE_SELECT_LEN;
	msg[0].buf = buf;

	page = ((addr >> 8) & MASK_8BIT);
	buf[0] = MASK_8BIT;
	buf[1] = page;

	if (page != rmi4_data->current_page) {
		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			if (i2c_transfer(i2c->adapter, msg, 1) == 1) {
				rmi4_data->current_page = page;
				retval = PAGE_SELECT_LEN;
				break;
			}
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: I2C retry %d\n",
					__func__, retry + 1);

			if (retry != SYN_I2C_RETRY_TIMES)
				msleep(5);

			if (retry == SYN_I2C_RETRY_TIMES / 2) {
				synaptics_rmi4_i2c_check_addr(rmi4_data, i2c);
				msg[0].addr = hw_if.board_data->i2c_addr;
			}
		}
	} else {
		retval = PAGE_SELECT_LEN;
	}

	return retval;
}

static int synaptics_rmi4_i2c_read(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval;
	unsigned char retry;
	unsigned char buf;
	unsigned short i2c_addr;
	unsigned short data_offset = 0;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);
	struct i2c_adapter *adap = i2c->adapter;
	struct i2c_msg msg[2];
	unsigned short index = 0;
	unsigned short read_size;
	unsigned short max_read_size;
	unsigned short left_bytes = length;

	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = synaptics_rmi4_i2c_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN) {
		retval = -EIO;
		goto exit;
	}

	msg[0].addr = hw_if.board_data->i2c_addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &buf;

	msg[1].addr = hw_if.board_data->i2c_addr;
	msg[1].flags = I2C_M_RD;

#ifdef I2C_BURST_LIMIT
	max_read_size = I2C_BURST_LIMIT;
#else
	max_read_size = length;
#endif

	do {
		if (left_bytes / max_read_size)
			read_size = max_read_size;
		else
			read_size = left_bytes;

		msg[1].len = read_size;
		msg[1].buf = &data[data_offset + index];

		buf = addr & MASK_8BIT;

		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			if (left_bytes == length) {
				retval = i2c_transfer(adap, &msg[0], 2);
				if (retval == 2)
					break;
			} else {
				retval = i2c_transfer(adap, &msg[1], 1);
				if (retval == 1)
					break;
			}

			dev_err(rmi4_data->pdev->dev.parent,
					"%s: I2C retry %d\n",
					__func__, retry + 1);
			msleep(20);

			if (retry == SYN_I2C_RETRY_TIMES / 2) {
				synaptics_rmi4_i2c_check_addr(rmi4_data, i2c);
				i2c_addr = hw_if.board_data->i2c_addr;
				msg[0].addr = i2c_addr;
				msg[1].addr = i2c_addr;
			}
		}

		if (retry == SYN_I2C_RETRY_TIMES) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: I2C read over retry limit\n",
					__func__);
			retval = -EIO;
			goto exit;
		}

		index += read_size;
		left_bytes -= read_size;
	} while (left_bytes);

	retval = length;

exit:
	mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);

	return retval;
}

static int synaptics_rmi4_i2c_write(struct synaptics_rmi4_data *rmi4_data,
		unsigned short addr, unsigned char *data, unsigned short length)
{
	int retval;
	unsigned char retry;
	struct i2c_client *i2c = to_i2c_client(rmi4_data->pdev->dev.parent);
	struct i2c_msg msg[1];
	unsigned short index = 0;
	unsigned short write_size;
	unsigned short max_write_size;
	unsigned short left_bytes = length;

#ifdef MAX_WRITE_SIZE
	max_write_size = MAX_WRITE_SIZE;
#else
	max_write_size = length;
#endif

	mutex_lock(&rmi4_data->rmi4_io_ctrl_mutex);

	retval = synaptics_rmi4_i2c_alloc_buf(rmi4_data, max_write_size + 1);
	if (retval < 0)
		goto exit;

	retval = synaptics_rmi4_i2c_set_page(rmi4_data, addr);
	if (retval != PAGE_SELECT_LEN) {
		retval = -EIO;
		goto exit;
	}

	do {
		if (left_bytes / max_write_size)
			write_size = max_write_size;
		else
			write_size = left_bytes;

		msg[0].addr = hw_if.board_data->i2c_addr;
		msg[0].flags = 0;
		msg[0].len = write_size + 1;
		msg[0].buf = wr_buf;

		wr_buf[0] = addr & MASK_8BIT;
		retval = secure_memcpy(&wr_buf[1], write_size, &data[index], write_size, write_size);
		if (retval < 0) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: Failed to copy data\n",
					__func__);
			goto exit;
		}

		for (retry = 0; retry < SYN_I2C_RETRY_TIMES; retry++) {
			if (i2c_transfer(i2c->adapter, msg, 1) == 1) {
				retval = length;
				break;
			}
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: I2C retry %d\n",
					__func__, retry + 1);
			msleep(20);

			if (retry == SYN_I2C_RETRY_TIMES / 2) {
				synaptics_rmi4_i2c_check_addr(rmi4_data, i2c);
				msg[0].addr = hw_if.board_data->i2c_addr;
			}
		}

		if (retry == SYN_I2C_RETRY_TIMES) {
			dev_err(rmi4_data->pdev->dev.parent,
					"%s: I2C write over retry limit\n",
					__func__);
			retval = -EIO;
		}

		index += write_size;
		left_bytes -= write_size;
	} while (left_bytes);

exit:
	mutex_unlock(&rmi4_data->rmi4_io_ctrl_mutex);

	return retval;
}

static struct synaptics_dsx_bus_access bus_access = {
	.type = BUS_I2C,
	.read = synaptics_rmi4_i2c_read,
	.write = synaptics_rmi4_i2c_write,
};

static void synaptics_rmi4_i2c_dev_release(struct device *dev)
{
	kfree(synaptics_dsx_i2c_device);

	return;
}

static int synaptics_rmi4_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *dev_id)
{
	int retval;

	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev,
				"%s: SMBus byte data commands not supported by host\n",
				__func__);
		return -EIO;
	}

	synaptics_dsx_i2c_device = kzalloc(
			sizeof(struct platform_device),
			GFP_KERNEL);
	if (!synaptics_dsx_i2c_device) {
		dev_err(&client->dev,
				"%s: Failed to allocate memory for synaptics_dsx_i2c_device\n",
				__func__);
		return -ENOMEM;
	}

#ifdef CONFIG_OF
	if (client->dev.of_node) {
		hw_if.board_data = devm_kzalloc(&client->dev,
				sizeof(struct synaptics_dsx_board_data),
				GFP_KERNEL);
		if (!hw_if.board_data) {
			dev_err(&client->dev,
					"%s: Failed to allocate memory for board data\n",
					__func__);
			return -ENOMEM;
		}
		hw_if.board_data->cap_button_map = devm_kzalloc(&client->dev,
				sizeof(struct synaptics_dsx_button_map),
				GFP_KERNEL);
		if (!hw_if.board_data->cap_button_map) {
			dev_err(&client->dev,
					"%s: Failed to allocate memory for 0D button map\n",
					__func__);
			return -ENOMEM;
		}
		hw_if.board_data->vir_button_map = devm_kzalloc(&client->dev,
				sizeof(struct synaptics_dsx_button_map),
				GFP_KERNEL);
		if (!hw_if.board_data->vir_button_map) {
			dev_err(&client->dev,
					"%s: Failed to allocate memory for virtual button map\n",
					__func__);
			return -ENOMEM;
		}
		parse_dt(&client->dev, hw_if.board_data);
	}
#else
	hw_if.board_data = client->dev.platform_data;
#endif

	hw_if.bus_access = &bus_access;
	hw_if.board_data->i2c_addr = client->addr;

	synaptics_dsx_i2c_device->name = PLATFORM_DRIVER_FORCE;
	synaptics_dsx_i2c_device->id = 0;
	synaptics_dsx_i2c_device->num_resources = 0;
	synaptics_dsx_i2c_device->dev.parent = &client->dev;
	synaptics_dsx_i2c_device->dev.platform_data = &hw_if;
	synaptics_dsx_i2c_device->dev.release = synaptics_rmi4_i2c_dev_release;

	retval = platform_device_register(synaptics_dsx_i2c_device);
	if (retval) {
		dev_err(&client->dev,
				"%s: Failed to register platform device\n",
				__func__);
		return -ENODEV;
	}

	return 0;
}

static int synaptics_rmi4_i2c_remove(struct i2c_client *client)
{
	platform_device_unregister(synaptics_dsx_i2c_device);

	return 0;
}

static const struct i2c_device_id synaptics_rmi4_id_table[] = {
	{I2C_DRIVER_FORCE, 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, synaptics_rmi4_id_table);

#ifdef CONFIG_OF
static struct of_device_id synaptics_rmi4_of_match_table_force[] = {
	{
		.compatible = "synaptics,dsx-i2c-force",
	},
	{},
};
MODULE_DEVICE_TABLE(of, synaptics_rmi4_of_match_table_force);
#else
#define synaptics_rmi4_of_match_table NULL
#endif

static struct i2c_driver synaptics_rmi4_i2c_driver = {
	.driver = {
		.name = "synaptics_dsi_force",
		.owner = THIS_MODULE,
		.of_match_table = synaptics_rmi4_of_match_table_force,
	},
	.probe = synaptics_rmi4_i2c_probe,
	.remove = synaptics_rmi4_i2c_remove,
	.id_table = synaptics_rmi4_id_table,
};

int synaptics_rmi4_bus_init_force(void)
{
	return i2c_add_driver(&synaptics_rmi4_i2c_driver);
}

void synaptics_rmi4_bus_exit_force(void)
{
	kfree(wr_buf);

	i2c_del_driver(&synaptics_rmi4_i2c_driver);

	return;
}

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX I2C Bus Support Module");
MODULE_LICENSE("GPL v2");
