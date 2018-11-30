/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include "cam_csiphy_core.h"
#include "cam_csiphy_dev.h"
#include "cam_csiphy_soc.h"

#include <soc/qcom/scm.h>
#include <cam_mem_mgr.h>

#define SCM_SVC_CAMERASS 0x18
#define SECURE_SYSCALL_ID 0x6

static int csiphy_dump;
module_param(csiphy_dump, int, 0644);

static int cam_csiphy_notify_secure_mode(int phy, bool protect)
{
	struct scm_desc desc = {0};

	desc.arginfo = SCM_ARGS(2, SCM_VAL, SCM_VAL);
	desc.args[0] = protect;
	desc.args[1] = phy;

	CAM_DBG(CAM_CSIPHY, "phy : %d, protect : %d", phy, protect);
	if (scm_call2(SCM_SIP_FNID(SCM_SVC_CAMERASS, SECURE_SYSCALL_ID),
		&desc)) {
		CAM_ERR(CAM_CSIPHY, "scm call to hypervisor failed");
		return -EINVAL;
	}

	return 0;
}

void cam_csiphy_query_cap(struct csiphy_device *csiphy_dev,
	struct cam_csiphy_query_cap *csiphy_cap)
{
	struct cam_hw_soc_info *soc_info = &csiphy_dev->soc_info;

	csiphy_cap->slot_info = soc_info->index;
	csiphy_cap->version = csiphy_dev->hw_version;
	csiphy_cap->clk_lane = csiphy_dev->clk_lane;
}

void cam_csiphy_reset(struct csiphy_device *csiphy_dev)
{
	int32_t  i;
	void __iomem *base = NULL;
	uint32_t size =
		csiphy_dev->ctrl_reg->csiphy_reg.csiphy_reset_array_size;
	struct cam_hw_soc_info *soc_info = &csiphy_dev->soc_info;

	base = soc_info->reg_map[0].mem_base;

	for (i = 0; i < size; i++) {
		cam_io_w_mb(
			csiphy_dev->ctrl_reg->csiphy_reset_reg[i].reg_data,
			base +
			csiphy_dev->ctrl_reg->csiphy_reset_reg[i].reg_addr);

		usleep_range(csiphy_dev->ctrl_reg->
			csiphy_reset_reg[i].delay * 1000,
			csiphy_dev->ctrl_reg->csiphy_reset_reg[i].delay * 1000
			+ 10);
	}
}

int32_t cam_cmd_buf_parser(struct csiphy_device *csiphy_dev,
	struct cam_config_dev_cmd *cfg_dev)
{
	int32_t                 rc = 0;
	uint64_t                generic_ptr;
	struct cam_packet       *csl_packet = NULL;
	struct cam_cmd_buf_desc *cmd_desc = NULL;
	uint32_t                *cmd_buf = NULL;
	struct cam_csiphy_info  *cam_cmd_csiphy_info = NULL;
	size_t                  len;

	if (!cfg_dev || !csiphy_dev) {
		CAM_ERR(CAM_CSIPHY, "Invalid Args");
		return -EINVAL;
	}

	rc = cam_mem_get_cpu_buf((int32_t) cfg_dev->packet_handle,
		(uint64_t *)&generic_ptr, &len);
	if (rc < 0) {
		CAM_ERR(CAM_CSIPHY, "Failed to get packet Mem address: %d", rc);
		return rc;
	}

	if (cfg_dev->offset > len) {
		CAM_ERR(CAM_CSIPHY,
			"offset is out of bounds: offset: %lld len: %zu",
			cfg_dev->offset, len);
		return -EINVAL;
	}

	csl_packet = (struct cam_packet *)(generic_ptr + cfg_dev->offset);

	cmd_desc = (struct cam_cmd_buf_desc *)
		((uint32_t *)&csl_packet->payload +
		csl_packet->cmd_buf_offset / 4);

	rc = cam_mem_get_cpu_buf(cmd_desc->mem_handle,
		(uint64_t *)&generic_ptr, &len);
	if (rc < 0) {
		CAM_ERR(CAM_CSIPHY,
			"Failed to get cmd buf Mem address : %d", rc);
		return rc;
	}

	cmd_buf = (uint32_t *)generic_ptr;
	cmd_buf += cmd_desc->offset / 4;
	cam_cmd_csiphy_info = (struct cam_csiphy_info *)cmd_buf;

	csiphy_dev->config_count++;
	csiphy_dev->csiphy_info.lane_cnt += cam_cmd_csiphy_info->lane_cnt;
	csiphy_dev->csiphy_info.lane_mask |= cam_cmd_csiphy_info->lane_mask;
	csiphy_dev->csiphy_info.csiphy_3phase =
		cam_cmd_csiphy_info->csiphy_3phase;
	csiphy_dev->csiphy_info.combo_mode |= cam_cmd_csiphy_info->combo_mode;
	if (cam_cmd_csiphy_info->combo_mode == 1)
		csiphy_dev->csiphy_info.settle_time_combo_sensor =
			cam_cmd_csiphy_info->settle_time;
	else
		csiphy_dev->csiphy_info.settle_time =
			cam_cmd_csiphy_info->settle_time;
	csiphy_dev->csiphy_info.data_rate = cam_cmd_csiphy_info->data_rate;
	csiphy_dev->csiphy_info.secure_mode = cam_cmd_csiphy_info->secure_mode;

	return rc;
}

void cam_csiphy_cphy_irq_config(struct csiphy_device *csiphy_dev)
{
	int32_t i;
	void __iomem *csiphybase =
		csiphy_dev->soc_info.reg_map[0].mem_base;

	for (i = 0; i < csiphy_dev->num_irq_registers; i++)
		cam_io_w_mb(
			csiphy_dev->ctrl_reg->csiphy_irq_reg[i].reg_data,
			csiphybase +
			csiphy_dev->ctrl_reg->csiphy_irq_reg[i].reg_addr);
}

void cam_csiphy_cphy_irq_disable(struct csiphy_device *csiphy_dev)
{
	int32_t i;
	void __iomem *csiphybase =
		csiphy_dev->soc_info.reg_map[0].mem_base;

	for (i = 0; i < csiphy_dev->num_irq_registers; i++)
		cam_io_w_mb(0x0, csiphybase +
			csiphy_dev->ctrl_reg->csiphy_irq_reg[i].reg_addr);
}

irqreturn_t cam_csiphy_irq(int irq_num, void *data)
{
	uint32_t irq;
	uint8_t i;
	struct csiphy_device *csiphy_dev =
		(struct csiphy_device *)data;
	struct cam_hw_soc_info *soc_info = NULL;
	struct csiphy_reg_parms_t *csiphy_reg = NULL;
	void __iomem *base = NULL;

	if (!csiphy_dev) {
		CAM_ERR(CAM_CSIPHY, "Invalid Args");
		return IRQ_NONE;
	}

	soc_info = &csiphy_dev->soc_info;
	base =  csiphy_dev->soc_info.reg_map[0].mem_base;
	csiphy_reg = &csiphy_dev->ctrl_reg->csiphy_reg;

	for (i = 0; i < csiphy_dev->num_irq_registers; i++) {
		irq = cam_io_r(base +
			csiphy_reg->mipi_csiphy_interrupt_status0_addr +
			(0x4 * i));
		cam_io_w_mb(irq, base +
			csiphy_reg->mipi_csiphy_interrupt_clear0_addr +
			(0x4 * i));
		CAM_ERR_RATE_LIMIT(CAM_CSIPHY,
			"CSIPHY%d_IRQ_STATUS_ADDR%d = 0x%x",
			soc_info->index, i, irq);
		cam_io_w_mb(0x0, base +
			csiphy_reg->mipi_csiphy_interrupt_clear0_addr +
			(0x4 * i));
	}
	cam_io_w_mb(0x1, base + csiphy_reg->mipi_csiphy_glbl_irq_cmd_addr);
	cam_io_w_mb(0x0, base + csiphy_reg->mipi_csiphy_glbl_irq_cmd_addr);

	return IRQ_HANDLED;
}

int32_t cam_csiphy_config_dev(struct csiphy_device *csiphy_dev)
{
	int32_t      rc = 0;
	uint32_t     lane_enable = 0, mask = 1, size = 0;
	uint16_t     lane_mask = 0, i = 0, cfg_size = 0, temp = 0;
	uint8_t      lane_cnt, lane_pos = 0;
	uint16_t     settle_cnt = 0;
	void __iomem *csiphybase;
	struct csiphy_reg_t *csiphy_common_reg = NULL;
	struct csiphy_reg_t (*reg_array)[MAX_SETTINGS_PER_LANE];

	lane_cnt = csiphy_dev->csiphy_info.lane_cnt;
	csiphybase = csiphy_dev->soc_info.reg_map[0].mem_base;

	if (!csiphybase) {
		CAM_ERR(CAM_CSIPHY, "csiphybase NULL");
		return -EINVAL;
	}

	if (!csiphy_dev->csiphy_info.csiphy_3phase) {
		if (csiphy_dev->csiphy_info.combo_mode == 1)
			reg_array =
				csiphy_dev->ctrl_reg->csiphy_2ph_combo_mode_reg;
		else
			reg_array =
				csiphy_dev->ctrl_reg->csiphy_2ph_reg;
		csiphy_dev->num_irq_registers = 11;
		cfg_size =
		csiphy_dev->ctrl_reg->csiphy_reg.csiphy_2ph_config_array_size;

		lane_mask = csiphy_dev->csiphy_info.lane_mask & 0x1f;
		for (i = 0; i < MAX_DPHY_DATA_LN; i++) {
			if (mask == 0x2) {
				if (lane_mask & mask)
					lane_enable |= 0x80;
				i--;
			} else if (lane_mask & mask) {
				lane_enable |= 0x1 << (i<<1);
			}
			mask <<= 1;
		}
	} else {
		if (csiphy_dev->csiphy_info.combo_mode == 1)
			reg_array =
				csiphy_dev->ctrl_reg->csiphy_2ph_3ph_mode_reg;
		else
			reg_array =
				csiphy_dev->ctrl_reg->csiphy_3ph_reg;
		csiphy_dev->num_irq_registers = 11;
		cfg_size =
		csiphy_dev->ctrl_reg->csiphy_reg.csiphy_3ph_config_array_size;

		lane_mask = csiphy_dev->csiphy_info.lane_mask & 0x7;
		mask = lane_mask;
		while (mask != 0) {
			temp = (i << 1)+1;
			lane_enable |= ((mask & 0x1) << temp);
			mask >>= 1;
			i++;
		}
	}

	size = csiphy_dev->ctrl_reg->csiphy_reg.csiphy_common_array_size;
	for (i = 0; i < size; i++) {
		csiphy_common_reg = &csiphy_dev->ctrl_reg->csiphy_common_reg[i];
		switch (csiphy_common_reg->csiphy_param_type) {
		case CSIPHY_LANE_ENABLE:
				cam_io_w_mb(lane_enable,
					csiphybase +
					csiphy_common_reg->reg_addr);
				usleep_range(csiphy_common_reg->delay * 1000,
					csiphy_common_reg->delay * 1000 + 10);
			break;
		case CSIPHY_DEFAULT_PARAMS:
				cam_io_w_mb(csiphy_common_reg->reg_data,
					csiphybase +
					csiphy_common_reg->reg_addr);
				usleep_range(csiphy_common_reg->delay * 1000,
					csiphy_common_reg->delay * 1000 + 10);
			break;
		default:
			break;
		}
	}

	while (lane_mask) {
		if (!(lane_mask & 0x1)) {
			lane_pos++;
			lane_mask >>= 1;
			continue;
		}

		settle_cnt = (csiphy_dev->csiphy_info.settle_time / 200000000);
		if (csiphy_dev->csiphy_info.combo_mode == 1 &&
			(lane_pos >= 3))
			settle_cnt =
			(csiphy_dev->csiphy_info.settle_time_combo_sensor /
				200000000);
		for (i = 0; i < cfg_size; i++) {
			switch (reg_array[lane_pos][i].csiphy_param_type) {
			case CSIPHY_LANE_ENABLE:
				cam_io_w_mb(lane_enable,
					csiphybase +
					reg_array[lane_pos][i].reg_addr);
			break;
			case CSIPHY_DEFAULT_PARAMS:
				cam_io_w_mb(reg_array[lane_pos][i].reg_data,
					csiphybase +
					reg_array[lane_pos][i].reg_addr);
			break;
			case CSIPHY_SETTLE_CNT_LOWER_BYTE:
				cam_io_w_mb(settle_cnt & 0xFF,
					csiphybase +
					reg_array[lane_pos][i].reg_addr);
			break;
			case CSIPHY_SETTLE_CNT_HIGHER_BYTE:
				cam_io_w_mb((settle_cnt >> 8) & 0xFF,
					csiphybase +
					reg_array[lane_pos][i].reg_addr);
			break;
			default:
				CAM_DBG(CAM_CSIPHY, "Do Nothing");
			break;
			}
			usleep_range(reg_array[lane_pos][i].delay*1000,
				reg_array[lane_pos][i].delay*1000 + 1000);
		}
		lane_mask >>= 1;
		lane_pos++;
	}

	cam_csiphy_cphy_irq_config(csiphy_dev);

	return rc;
}

void cam_csiphy_shutdown(struct csiphy_device *csiphy_dev)
{
	struct cam_hw_soc_info *soc_info;

	if (csiphy_dev->csiphy_state == CAM_CSIPHY_INIT)
		return;

	if (csiphy_dev->csiphy_state == CAM_CSIPHY_START) {
		soc_info = &csiphy_dev->soc_info;

		if (csiphy_dev->csiphy_info.secure_mode)
			cam_csiphy_notify_secure_mode(
				csiphy_dev->soc_info.index,
				CAM_SECURE_MODE_NON_SECURE);

		csiphy_dev->csiphy_info.secure_mode =
			CAM_SECURE_MODE_NON_SECURE;

		cam_csiphy_reset(csiphy_dev);
		cam_soc_util_disable_platform_resource(soc_info, true, true);

		cam_cpas_stop(csiphy_dev->cpas_handle);
		csiphy_dev->csiphy_state = CAM_CSIPHY_ACQUIRE;
	}

	if (csiphy_dev->csiphy_state == CAM_CSIPHY_ACQUIRE) {
		if (csiphy_dev->bridge_intf.device_hdl[0] != -1)
			cam_destroy_device_hdl(
				csiphy_dev->bridge_intf.device_hdl[0]);
		if (csiphy_dev->bridge_intf.device_hdl[1] != -1)
			cam_destroy_device_hdl(
				csiphy_dev->bridge_intf.device_hdl[1]);
		csiphy_dev->bridge_intf.device_hdl[0] = -1;
		csiphy_dev->bridge_intf.device_hdl[1] = -1;
		csiphy_dev->bridge_intf.link_hdl[0] = -1;
		csiphy_dev->bridge_intf.link_hdl[1] = -1;
		csiphy_dev->bridge_intf.session_hdl[0] = -1;
		csiphy_dev->bridge_intf.session_hdl[1] = -1;
	}

	csiphy_dev->ref_count = 0;
	csiphy_dev->is_acquired_dev_combo_mode = 0;
	csiphy_dev->acquire_count = 0;
	csiphy_dev->start_dev_count = 0;
	csiphy_dev->csiphy_state = CAM_CSIPHY_INIT;
}

static int32_t cam_csiphy_external_cmd(struct csiphy_device *csiphy_dev,
	struct cam_config_dev_cmd *p_submit_cmd)
{
	struct cam_csiphy_info cam_cmd_csiphy_info;
	int32_t rc = 0;

	if (copy_from_user(&cam_cmd_csiphy_info,
		(void __user *)p_submit_cmd->packet_handle,
		sizeof(struct cam_csiphy_info))) {
		CAM_ERR(CAM_CSIPHY, "failed to copy cam_csiphy_info\n");
		rc = -EFAULT;
	} else {
		csiphy_dev->csiphy_info.lane_cnt =
			cam_cmd_csiphy_info.lane_cnt;
		csiphy_dev->csiphy_info.lane_cnt =
			cam_cmd_csiphy_info.lane_cnt;
		csiphy_dev->csiphy_info.lane_mask =
			cam_cmd_csiphy_info.lane_mask;
		csiphy_dev->csiphy_info.csiphy_3phase =
			cam_cmd_csiphy_info.csiphy_3phase;
		csiphy_dev->csiphy_info.combo_mode =
			cam_cmd_csiphy_info.combo_mode;
		csiphy_dev->csiphy_info.settle_time =
			cam_cmd_csiphy_info.settle_time;
		csiphy_dev->csiphy_info.data_rate =
			cam_cmd_csiphy_info.data_rate;
		CAM_DBG(CAM_CSIPHY,
			"%s CONFIG_DEV_EXT settle_time= %lld lane_cnt=%d lane_mask=0x%x",
			__func__,
			csiphy_dev->csiphy_info.settle_time,
			csiphy_dev->csiphy_info.lane_cnt,
			csiphy_dev->csiphy_info.lane_mask);
	}

	return rc;
}

int32_t cam_csiphy_core_cfg(void *phy_dev,
			void *arg)
{
	struct csiphy_device *csiphy_dev =
		(struct csiphy_device *)phy_dev;
	struct intf_params   *bridge_intf = NULL;
	struct cam_control   *cmd = (struct cam_control *)arg;
	int32_t              rc = 0;

	if (!csiphy_dev || !cmd) {
		CAM_ERR(CAM_CSIPHY, "Invalid input args");
		return -EINVAL;
	}

	if (cmd->handle_type != CAM_HANDLE_USER_POINTER) {
		CAM_ERR(CAM_CSIPHY, "Invalid handle type: %d",
			cmd->handle_type);
		return -EINVAL;
	}

	CAM_DBG(CAM_CSIPHY, "Opcode received: %d", cmd->op_code);
	mutex_lock(&csiphy_dev->mutex);
	switch (cmd->op_code) {
	case CAM_ACQUIRE_DEV: {
		struct cam_sensor_acquire_dev csiphy_acq_dev;
		struct cam_csiphy_acquire_dev_info csiphy_acq_params;

		struct cam_create_dev_hdl bridge_params;

		rc = copy_from_user(&csiphy_acq_dev,
			(void __user *)cmd->handle,
			sizeof(csiphy_acq_dev));
		if (rc < 0) {
			CAM_ERR(CAM_CSIPHY, "Failed copying from User");
			goto release_mutex;
		}

		csiphy_acq_params.combo_mode = 0;

		if (copy_from_user(&csiphy_acq_params,
			(void __user *)csiphy_acq_dev.info_handle,
			sizeof(csiphy_acq_params))) {
			CAM_ERR(CAM_CSIPHY,
				"Failed copying from User");
			goto release_mutex;
		}

		if (csiphy_dev->acquire_count == 2) {
			CAM_ERR(CAM_CSIPHY,
					"CSIPHY device do not allow more than 2 acquires");
			rc = -EINVAL;
			goto release_mutex;
		}

		if ((csiphy_acq_params.combo_mode == 1) &&
			(csiphy_dev->is_acquired_dev_combo_mode == 1)) {
			CAM_ERR(CAM_CSIPHY,
				"Multiple Combo Acq are not allowed: cm: %d, acm: %d",
				csiphy_acq_params.combo_mode,
				csiphy_dev->is_acquired_dev_combo_mode);
			rc = -EINVAL;
			goto release_mutex;
		}

		if ((csiphy_acq_params.combo_mode != 1) &&
			(csiphy_dev->is_acquired_dev_combo_mode != 1) &&
			(csiphy_dev->acquire_count == 1)) {
			CAM_ERR(CAM_CSIPHY,
				"Multiple Acquires are not allowed cm: %d acm: %d",
				csiphy_acq_params.combo_mode,
				csiphy_dev->is_acquired_dev_combo_mode);
			rc = -EINVAL;
			goto release_mutex;
		}

		bridge_params.ops = NULL;
		bridge_params.session_hdl = csiphy_acq_dev.session_handle;
		bridge_params.v4l2_sub_dev_flag = 0;
		bridge_params.media_entity_flag = 0;
		bridge_params.priv = csiphy_dev;

		if (csiphy_acq_params.combo_mode >= 2) {
			CAM_ERR(CAM_CSIPHY, "Invalid combo_mode %d",
				csiphy_acq_params.combo_mode);
			rc = -EINVAL;
			goto release_mutex;
		}

		csiphy_acq_dev.device_handle =
			cam_create_device_hdl(&bridge_params);
		bridge_intf = &csiphy_dev->bridge_intf;
		bridge_intf->device_hdl[csiphy_acq_params.combo_mode]
			= csiphy_acq_dev.device_handle;
		bridge_intf->session_hdl[csiphy_acq_params.combo_mode] =
			csiphy_acq_dev.session_handle;

		if (copy_to_user((void __user *)cmd->handle,
				&csiphy_acq_dev,
				sizeof(struct cam_sensor_acquire_dev))) {
			CAM_ERR(CAM_CSIPHY, "Failed copying from User");
			rc = -EINVAL;
			goto release_mutex;
		}
		if (csiphy_acq_params.combo_mode == 1)
			csiphy_dev->is_acquired_dev_combo_mode = 1;

		csiphy_dev->acquire_count++;
		csiphy_dev->csiphy_state = CAM_CSIPHY_ACQUIRE;
	}
		break;
	case CAM_QUERY_CAP: {
		struct cam_csiphy_query_cap csiphy_cap = {0};

		cam_csiphy_query_cap(csiphy_dev, &csiphy_cap);
		if (copy_to_user((void __user *)cmd->handle,
			&csiphy_cap, sizeof(struct cam_csiphy_query_cap))) {
			CAM_ERR(CAM_CSIPHY, "Failed copying from User");
			rc = -EINVAL;
			goto release_mutex;
		}
	}
		break;
	case CAM_STOP_DEV: {
		if ((csiphy_dev->csiphy_state != CAM_CSIPHY_START) ||
			!csiphy_dev->start_dev_count) {
			CAM_ERR(CAM_CSIPHY, "Not in right state to stop : %d",
				csiphy_dev->csiphy_state);
			goto release_mutex;
		}

		if (--csiphy_dev->start_dev_count) {
			CAM_DBG(CAM_CSIPHY, "Stop Dev ref Cnt: %d",
				csiphy_dev->start_dev_count);
			goto release_mutex;
		}

		if (csiphy_dev->csiphy_info.secure_mode)
			cam_csiphy_notify_secure_mode(
				csiphy_dev->soc_info.index,
				CAM_SECURE_MODE_NON_SECURE);

		csiphy_dev->csiphy_info.secure_mode =
			CAM_SECURE_MODE_NON_SECURE;

		rc = cam_csiphy_disable_hw(csiphy_dev);
		if (rc < 0)
			CAM_ERR(CAM_CSIPHY, "Failed in csiphy release");

		rc = cam_cpas_stop(csiphy_dev->cpas_handle);
		if (rc < 0)
			CAM_ERR(CAM_CSIPHY, "de-voting CPAS: %d", rc);

		csiphy_dev->csiphy_state = CAM_CSIPHY_ACQUIRE;
	}
		break;
	case CAM_RELEASE_DEV: {
		struct cam_release_dev_cmd release;

		if (!csiphy_dev->acquire_count) {
			CAM_ERR(CAM_CSIPHY, "No valid devices to release");
			rc = -EINVAL;
			goto release_mutex;
		}

		if (copy_from_user(&release, (void __user *) cmd->handle,
			sizeof(release))) {
			rc = -EFAULT;
			goto release_mutex;
		}

		rc = cam_destroy_device_hdl(release.dev_handle);
		if (rc < 0)
			CAM_ERR(CAM_CSIPHY, "destroying the device hdl");
		if (release.dev_handle ==
			csiphy_dev->bridge_intf.device_hdl[0]) {
			csiphy_dev->bridge_intf.device_hdl[0] = -1;
			csiphy_dev->bridge_intf.link_hdl[0] = -1;
			csiphy_dev->bridge_intf.session_hdl[0] = -1;
		} else {
			csiphy_dev->bridge_intf.device_hdl[1] = -1;
			csiphy_dev->bridge_intf.link_hdl[1] = -1;
			csiphy_dev->bridge_intf.session_hdl[1] = -1;
			csiphy_dev->is_acquired_dev_combo_mode = 0;
		}

		csiphy_dev->config_count--;
		csiphy_dev->acquire_count--;

		if (csiphy_dev->acquire_count == 0)
			csiphy_dev->csiphy_state = CAM_CSIPHY_INIT;

		if (csiphy_dev->config_count == 0) {
			CAM_DBG(CAM_CSIPHY, "reset csiphy_info");
			csiphy_dev->csiphy_info.lane_mask = 0;
			csiphy_dev->csiphy_info.lane_cnt = 0;
			csiphy_dev->csiphy_info.combo_mode = 0;
		}
	}
		break;
	case CAM_CONFIG_DEV: {
		struct cam_config_dev_cmd config;

		if (copy_from_user(&config, (void __user *)cmd->handle,
					sizeof(config))) {
			rc = -EFAULT;
		} else {
			rc = cam_cmd_buf_parser(csiphy_dev, &config);
			if (rc < 0) {
				CAM_ERR(CAM_CSIPHY, "Fail in cmd buf parser");
				goto release_mutex;
			}
		}
		break;
	}
	case CAM_START_DEV: {
		struct cam_ahb_vote ahb_vote;
		struct cam_axi_vote axi_vote;

		if (csiphy_dev->csiphy_state == CAM_CSIPHY_START) {
			csiphy_dev->start_dev_count++;
			goto release_mutex;
		}

		ahb_vote.type = CAM_VOTE_ABSOLUTE;
		ahb_vote.vote.level = CAM_SVS_VOTE;
		axi_vote.compressed_bw = CAM_CPAS_DEFAULT_AXI_BW;
		axi_vote.uncompressed_bw = CAM_CPAS_DEFAULT_AXI_BW;

		rc = cam_cpas_start(csiphy_dev->cpas_handle,
			&ahb_vote, &axi_vote);
		if (rc < 0) {
			CAM_ERR(CAM_CSIPHY, "voting CPAS: %d", rc);
			goto release_mutex;
		}

		if (csiphy_dev->csiphy_info.secure_mode) {
			rc = cam_csiphy_notify_secure_mode(
				csiphy_dev->soc_info.index,
				CAM_SECURE_MODE_SECURE);
			if (rc < 0)
				csiphy_dev->csiphy_info.secure_mode =
					CAM_SECURE_MODE_NON_SECURE;
		}

		rc = cam_csiphy_enable_hw(csiphy_dev);
		if (rc != 0) {
			CAM_ERR(CAM_CSIPHY, "cam_csiphy_enable_hw failed");
			cam_cpas_stop(csiphy_dev->cpas_handle);
			goto release_mutex;
		}
		rc = cam_csiphy_config_dev(csiphy_dev);
		if (csiphy_dump == 1)
			cam_csiphy_mem_dmp(&csiphy_dev->soc_info);

		if (rc < 0) {
			CAM_ERR(CAM_CSIPHY, "cam_csiphy_config_dev failed");
			cam_csiphy_disable_hw(csiphy_dev);
			cam_cpas_stop(csiphy_dev->cpas_handle);
			goto release_mutex;
		}
		csiphy_dev->start_dev_count++;
		csiphy_dev->csiphy_state = CAM_CSIPHY_START;
	}
		break;
	case CAM_CONFIG_DEV_EXTERNAL: {
		struct cam_config_dev_cmd submit_cmd;

		if (copy_from_user(&submit_cmd,
			(void __user *)cmd->handle,
			sizeof(struct cam_config_dev_cmd))) {
			CAM_ERR(CAM_CSIPHY, "failed copy config ext\n");
			rc = -EFAULT;
		} else {
			rc = cam_csiphy_external_cmd(csiphy_dev, &submit_cmd);
		}
		break;
	}
	default:
		CAM_ERR(CAM_CSIPHY, "Invalid Opcode: %d", cmd->op_code);
		rc = -EINVAL;
		goto release_mutex;
	}

release_mutex:
	mutex_unlock(&csiphy_dev->mutex);

	return rc;
}
