/*
 * Copyright (C) 2011 XiaoMi, Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/input/ft5x46_ts.h>
#include <linux/hwinfo.h>
#include <linux/input/touch_common_info.h>
#include "ft8716_pramboot.h"

/* #define FT5X46_DEBUG_PERMISSION */
#define FT5X46_APK_DEBUG_CHANNEL

/* register address */
#define FT5X0X_REG_DEVIDE_MODE	0x00
#define FT5X0X_REG_ROW_ADDR		0x01
#define FT5X0X_REG_TD_STATUS		0x02
#define FT5X0X_REG_START_SCAN		0x02
#define FT5X0X_REG_TOUCH_START	0x03
#define FT5X0X_REG_VOLTAGE		0x05
#define FT5X0X_REG_CHIP_ID		0xA3
#define FT5X0X_ID_G_PMODE			0xA5
#define FT5X0X_REG_FW_VER			0xA6
#define FT5X0X_ID_G_FT5201ID		0xA8
#define FT5X0X_NOISE_FILTER		0x8B
#define FT5X0X_REG_POINT_RATE		0x88
#define FT5X0X_REG_THGROUP		0x80
#define FT5X0X_REG_RESET			0xFC

#define FT5X0X_DEVICE_MODE_NORMAL	0x00
#define FT5X0X_DEVICE_MODE_TEST	0x40
#define FT5X0X_DEVICE_START_CALIB	0x04
#define FT5X0X_DEVICE_SAVE_RESULT	0x05

#define FT5X0X_POWER_ACTIVE             0x00
#define FT5X0X_POWER_MONITOR            0x01
#define FT5X0X_POWER_HIBERNATE          0x03

#define FT5X0X_REG_CONFIG_INFO		0x92

/* ft5x0x register list */
#define FT5X0X_TOUCH_LENGTH		6

#define FT5X0X_TOUCH_XH			0x00 /* offset from each touch */
#define FT5X0X_TOUCH_XL			0x01
#define FT5X0X_TOUCH_YH			0x02
#define FT5X0X_TOUCH_YL			0x03
#define FT5X0X_TOUCH_PRESSURE		0x04
#define FT5X0X_TOUCH_SIZE		0x05

/* ft5x46 finger register list */
#define FT5X46_MAX_ID			0x0F
#define FT5X46_TOUCH_LENGTH		6
#define FT5X46_XH_POS			3
#define FT5X46_XL_POS			4
#define FT5X46_YH_POS			5
#define FT5X46_YL_POS			6
#define FT5X46_XY_POS			7
#define FT5X46_EVENT_POS		3
#define FT5X46_ID_POS			5
#define FT5X46_MISC_POS			8

/* ft5x46 default pressure */
#define FT5X46_DEF_PRESSURE		8

/* ft5x0x bit field definition */
#define FT5X0X_MODE_NORMAL		0x00
#define FT5X0X_MODE_SYSINFO		0x10
#define FT5X0X_MODE_TEST		0x40
#define FT5X0X_MODE_MASK		0x70

#define FT5X0X_EVENT_DOWN		0x00
#define FT5X0X_EVENT_UP			0x40
#define FT5X0X_EVENT_CONTACT		0x80
#define FT5X0X_EVENT_MASK		0xc0

/* ft8716 firmware upgrade definition */
#define FT8716_FIRMWARE_VERION		0x10e

/* ft5x0x firmware upgrade definition */
#define FT5X0X_FIRMWARE_TAIL		(-8) /* base on the end of firmware */
#define FT5X0X_FIRMWARE_VERION		(-2)
#define FT5X46_RD_FLASH_PACKET_SIZE	4
#define FT5X0X_PACKET_HEADER		6
#define FT5X0X_PACKET_LENGTH		128

/* ft5x0x absolute value */
#define FT5X0X_MAX_SIZE			0xff
#define FT5X0X_MAX_PRESSURE		0xff
#define FT5X46_PRESSURE			8

#define FT5X46_POINT_READ_BUF		(3 + FT5X46_TOUCH_LENGTH * FT5X0X_MAX_FINGER)

#define NOISE_FILTER_DELAY	HZ

#define FT_VTG_MIN_UV		2600000
#define FT_VTG_MAX_UV		3300000
#define FT_I2C_VTG_MIN_UV	1800000
#define FT_I2C_VTG_MAX_UV	1800000

/*upgrade config of FT5316*/
#define FT5316_UPGRADE_AA_DELAY 		50
#define FT5316_UPGRADE_55_DELAY 		40
#define FT5316_UPGRADE_ID_1		0x79
#define FT5316_UPGRADE_ID_2		0x07
#define FT5316_UPGRADE_READID_DELAY 		1

/*upgrade config of FT5X36*/
#define FT5X36_UPGRADE_AA_DELAY 		30
#define FT5X36_UPGRADE_55_DELAY 		30
#define FT5X36_UPGRADE_ID_1		0x79
#define FT5X36_UPGRADE_ID_2		0x11
#define FT5X36_UPGRADE_READID_DELAY 		10

/*upgrade config of FT5X46*/
#define FT5X46_UPGRADE_AA_DELAY 		2
#define FT5X46_UPGRADE_55_DELAY 		2
#define FT5X46_UPGRADE_ID_1		0x54
#define FT5X46_UPGRADE_ID_2		0x2C
#define FT5X46_UPGRADE_READID_DELAY 		20

/*upgrade config of FT8716*/
#define FT8716_UPGRADE_AA_DELAY 		2
#define FT8716_UPGRADE_55_DELAY 		2
#define FT8716_UPGRADE_ID_1		0x87
#define FT8716_UPGRADE_ID_2		0xA6
#define FT8716_UPGRADE_READID_DELAY 		20

/*upgrade config of FTE716*/
#define FTE716_UPGRADE_AA_DELAY 		2
#define FTE716_UPGRADE_55_DELAY 		2
#define FTE716_UPGRADE_ID_1		0xE7
#define FTE716_UPGRADE_ID_2		0xA6
#define FTE716_UPGRADE_READID_DELAY 		20

#define FT5316_CHIP_ID		0x0A
#define FT5X36_CHIP_ID		0x14
#define FT5X46_CHIP_ID		0x54
#define FT8716_CHIP_ID		0x87
#define FTE716_CHIP_ID		0xE7

#define FT5316_PROJ_ID		"FTS0000x000"
#define FT5X36_PROJ_ID		"FTS0000P000"

#define FT5X0X_UPGRADE_LOOP	3

#define	BL_VERSION_LZ4		0
#define	BL_VERSION_Z7		1
#define	BL_VERSION_GZF	2


#ifdef FT5X46_APK_DEBUG_CHANNEL
#define FT5X46_PROC_NAME		"ftxxxx-debug"

#define FT5X46_PROC_UPGRADE		0
#define FT5X46_PROC_READ_REGISTER	1
#define FT5X46_PROC_WRITE_REGISTER	2
#define FT5X46_PROC_AUTOCLB		4
#define FT5X46_PROC_UPGRADE_INFO	5
#define FT5X46_PROC_WRITE_DATA		6
#define FT5X46_PROC_READ_DATA		7

#define FT5X46_GESTURE_POINTS_HEADER	8
#define FT5X46_GESTURE_UP		0x22
#define FT5X46_GESTURE_DBLCLICK		0x24

#define FT5X46_INPUT_EVENT_START			0
#define FT5X46_INPUT_EVENT_SENSITIVE_MODE_OFF		0
#define FT5X46_INPUT_EVENT_SENSITIVE_MODE_ON		1
#define FT5X46_INPUT_EVENT_STYLUS_MODE_OFF		2
#define FT5X46_INPUT_EVENT_STYLUS_MODE_ON		3
#define FT5X46_INPUT_EVENT_WAKUP_MODE_OFF		4
#define FT5X46_INPUT_EVENT_WAKUP_MODE_ON		5
#define FT5X46_INPUT_EVENT_END				5

#define LEN_FLASH_ECC_MAX		0xFFFE
#define FT5X46_ESD_CHECK_PERIOD	5000

static bool lcd_need_reset;
static unsigned char proc_operate_mode = FT5X46_PROC_UPGRADE;
static struct proc_dir_entry *ft5x46_proc_entry;
#endif
struct ft5x46_packet {
	u8  magic1;
	u8  magic2;
	u16 offset;
	u16 length;
	u8  payload[FT5X0X_PACKET_LENGTH];
};

struct ft5x46_rd_flash_packet {
	u8 magic;
	u8 addr_h;
	u8 addr_m;
	u8 addr_l;
};

struct ft5x46_finger {
	int x, y;
	int size;
	int pressure;
	bool detect;
};
struct ft5x46_mode_switch {
	struct ft5x46_data *data;
	u8 mode;
	struct work_struct switch_mode_work;
};
static struct ft5x46_keypad_data *vir_keypad;
struct ft5x46_data *ft_data;

static int ft5x46_recv_byte(struct ft5x46_data *ft5x46, u8 len, ...)
{
	int error = 0;
	va_list varg;
	u8 i, buf[len];

	error = ft5x46->bops->recv(ft5x46->dev, buf, len);
	if (error)
		return error;

	va_start(varg, len);
	for (i = 0; i < len; i++)
		*va_arg(varg, u8 *) = buf[i];
	va_end(varg);

	return 0;
}

static int ft5x46_send_block(struct ft5x46_data *ft5x46,
				const void *buf, int len)
{
	return ft5x46->bops->send(ft5x46->dev, buf, len);
}

static int ft5x46_recv_block(struct ft5x46_data *ft5x46,
				void *buf, int len)
{
	return ft5x46->bops->recv(ft5x46->dev, buf, len);
}

static int ft5x46_send_byte(struct ft5x46_data *ft5x46, u8 len, ...)
{
	va_list varg;
	u8 i, buf[len];

	va_start(varg, len);
	for (i = 0; i < len; i++)
		buf[i] = va_arg(varg, int); /* u8 promote to int */
	va_end(varg);

	return ft5x46_send_block(ft5x46, buf, len);
}

static int ft5x46_read_block(struct ft5x46_data *ft5x46,
				u8 addr, void *buf, u8 len)
{
	return ft5x46->bops->read(ft5x46->dev, addr, buf, len);
}

static int ft5x46_read_byte(struct ft5x46_data *ft5x46, u8 addr, u8 *data)
{
	return ft5x46_read_block(ft5x46, addr, data, sizeof(*data));
}

static int ft5x46_write_byte(struct ft5x46_data *ft5x46, u8 addr, u8 data)
{
	return ft5x46->bops->write(ft5x46->dev, addr, &data, sizeof(data));
}

static void ft5x46_charger_state_changed(struct ft5x46_data *ft5x46)
{
	u8 is_usb_exist;

	if (!ft5x46->hw_is_ready)
		return;
	is_usb_exist = power_supply_is_system_supplied();
	if ((is_usb_exist != ft5x46->is_usb_plug_in) || (ft5x46->force_update_noise_filter)) {
		ft5x46->is_usb_plug_in = is_usb_exist;
		dev_info(ft5x46->dev, "Power state changed, set noise filter to 0x%x\n", is_usb_exist);
		mutex_lock(&ft5x46->mutex);
		ft5x46_write_byte(ft5x46, FT5X0X_NOISE_FILTER, is_usb_exist);
		mutex_unlock(&ft5x46->mutex);
	}
}

static void ft5x46_noise_filter_delayed_work(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ft5x46_data *ft5x46 = container_of(delayed_work, struct ft5x46_data, noise_filter_delayed_work);

	dev_info(ft5x46->dev, "ft5x46_noise_filter_delayed_work called\n");
	ft5x46_charger_state_changed(ft5x46);
}

#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
static int ft5x46_proximity_enable(struct ft5x46_data *ft5x46, bool enable)
{
	int error = ft5x46_write_byte(ft5x46, 0xB0, enable);
	if (error < 0) {
		dev_err(ft5x46->dev, "ERROR: Write proximity enable byte failed!\n");
		return -EINVAL;
	}
	return 0;
}

static void ft5x46_prox_enable_delayed_work(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ft5x46_data *ft5x46 = container_of(delayed_work, struct ft5x46_data, prox_enable_delayed_work);

	if (!ft5x46->proximity_enable_in_suspend)
		return;
	mutex_lock(&ft5x46->mutex);
	ft5x46->proximity_enable = true;
	ft5x46->proximity_enable_in_suspend = false;
	ft5x46_proximity_enable(ft5x46, ft5x46->proximity_enable);
	mutex_unlock(&ft5x46->mutex);
	dev_info(ft5x46->dev, "ft5x46_prox_enable_delayed_work called\n");
}
#endif

static int ft5x46_power_supply_event(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct ft5x46_data *ft5x46 = container_of(nb, struct ft5x46_data, power_supply_notifier);

	if (ft5x46->dbgdump)
		dev_info(ft5x46->dev, "Power_supply_event\n");
	if (!ft5x46->in_suspend) {
		ft5x46->force_update_noise_filter = false;
		schedule_delayed_work(&ft5x46->noise_filter_delayed_work,
				NOISE_FILTER_DELAY);
	} else if (ft5x46->dbgdump)
		dev_info(ft5x46->dev, "Don't response to power supply event in suspend mode!\n");

	return 0;
}

static void ft5x46_disable_irq(struct ft5x46_data *ft5x46)
{
	if (likely(ft5x46->irq_enabled)) {
		disable_irq(ft5x46->irq);
		ft5x46->irq_enabled = false;
	}
}

static void ft5x46_enable_irq(struct ft5x46_data *ft5x46)
{
	if (likely(!ft5x46->irq_enabled)) {
		enable_irq(ft5x46->irq);
		ft5x46->irq_enabled = true;
	}
}

#ifdef CONFIG_TOUCHSCREEN_FT5X46_CALIBRATE
static int ft5x46_auto_calib(struct ft5x46_data *ft5x46)
{
	int error = 0;
	u8 val1;
	int i;
	msleep(200);
	error = ft5x46_write_byte(ft5x46, /* enter factory mode */
				FT5X0X_REG_DEVIDE_MODE, FT5X0X_DEVICE_MODE_TEST);
	if (error)
		return error;
	msleep(100);

	error = ft5x46_write_byte(ft5x46, /* start calibration */
				FT5X0X_REG_START_SCAN, FT5X0X_DEVICE_START_CALIB);
	if (error)
		return error;
	msleep(300);

	for (i = 0; i < 100; i++) {
		error = ft5x46_read_byte(ft5x46, FT5X0X_REG_DEVIDE_MODE, &val1);
		if (error)
			return error;
		if (((val1 & 0x70) >> 4) == 0) /* return to normal mode? */
			break;
		msleep(200); /* not yet, wait and try again later */
	}
	dev_info(ft5x46->dev, "[FTS] calibration OK.\n");

	msleep(300); /* enter factory mode again */
	error = ft5x46_write_byte(ft5x46, FT5X0X_REG_DEVIDE_MODE, FT5X0X_DEVICE_MODE_TEST);
	if (error)
		return error;
	msleep(100);

	error = ft5x46_write_byte(ft5x46, /* save calibration result */
				FT5X0X_REG_START_SCAN, FT5X0X_DEVICE_SAVE_RESULT);
	if (error)
		return error;
	msleep(300);

	error = ft5x46_write_byte(ft5x46, /* return to normal mode */
				FT5X0X_REG_DEVIDE_MODE, FT5X0X_DEVICE_MODE_NORMAL);
	if (error)
		return error;
	msleep(300);
	dev_info(ft5x46->dev, "[FTS] Save calib result OK.\n");

	return 0;
}
#endif

static u8 reset_delay[] = {
	30, 33, 36, 39, 42, 45, 27, 24, 11, 18, 15
};

static int ft5x46_hid_to_std_i2c(struct ft5x46_data *ft5x46)
{
	int error = 0;
	u8 val1, val2, val3;

	error = ft5x46_send_byte(ft5x46, 3, 0xEB, 0xAA, 0x09);
	if (error) {
		dev_err(ft5x46->dev, "Failed to send EB AA 09\n");
		return error;
	}

	msleep(10);
	error = ft5x46_recv_byte(ft5x46, 3, &val1, &val2, &val3);
	if (error) {
		dev_err(ft5x46->dev, "Failed to receive device id\n");
		return error;
	}

	dev_dbg(ft5x46->dev, "Changed to STDI2C Value: REG1 = 0x%02x, REG2 = 0x%02x, REG3 = 0x%02x\n",
		val1, val2, val3);

	if (val1 == 0xEB && val2 == 0xAA && val3 == 0x08) {
		dev_info(ft5x46->dev, "HidI2c To StdI2c succeed\n");
		return 0;
	} else {
		dev_err(ft5x46->dev, "HidI2c To StdI2c failed\n");
		return -ENODEV;
	}
}

static u8 ft5x46_get_factory_id(struct ft5x46_data *ft5x46)
{
	int error = 0;
	int i, j;
	u8 val1, val2;
	struct ft5x46_rd_flash_packet packet;
	u8 vid;

	for (i = 0, error = -1; i < sizeof(reset_delay) && error; i++) {
		/* step 1a: reset device */
		error = ft5x46_write_byte(ft5x46, FT5X0X_REG_RESET, 0xaa);
		if (error)
			continue;
		msleep(50);

		error = ft5x46_write_byte(ft5x46, FT5X0X_REG_RESET, 0x55);
		if (error)
			continue;
		msleep(200);
		dev_info(ft5x46->dev, "[FTS] step1: Reset device.\n");

		/* step1b: switch to std i2c */
		error = ft5x46_hid_to_std_i2c(ft5x46);
		if (error) {
			dev_err(ft5x46->dev, "HidI2c change to StdI2c fail!\n");
		}

		/* step 2: enter upgrade mode */
		for (j = 0; j < 10; j++) {
			error = ft5x46_send_byte(ft5x46, 2, 0x55, 0xaa);
			msleep(5);
			if (!error)
				break;
		}
		if (error)
			continue;

		dev_info(ft5x46->dev, "[FTS] step2: Enter upgrade mode.\n");

		/* step 4: check device id */
		error = ft5x46_send_byte(ft5x46, 4, 0x90, 0x00, 0x00, 0x00);
		if (error) {
			dev_err(ft5x46->dev, "Failed to send 90 00 00 00\n");
			continue;
		}

		error = ft5x46_recv_byte(ft5x46, 2, &val1, &val2);
		if (error) {
			dev_err(ft5x46->dev, "Failed to receive device id\n");
			continue;
		}

		dev_info(ft5x46->dev, "read id = 0x%x, 0x%x\n", val1, val2);

		dev_info(ft5x46->dev, "[FTS] step3: Check device id.\n");

		if (val1 == FT5316_UPGRADE_ID_1 && val2 == FT5316_UPGRADE_ID_2) {
			ft5x46->chip_id = FT5316_CHIP_ID;
			break;
		} else if (val1 == FT5X36_UPGRADE_ID_1 && val2 == FT5X36_UPGRADE_ID_2) {
			ft5x46->chip_id = FT5X36_CHIP_ID;
			break;
		} else if (val1 == FT5X46_UPGRADE_ID_1 && val2 == FT5X46_UPGRADE_ID_2) {
			ft5x46->chip_id = FT5X46_CHIP_ID;
			break;
		} else if (val1 == FT8716_UPGRADE_ID_1 && val2 == FT8716_UPGRADE_ID_2) {
			ft5x46->chip_id = FT8716_CHIP_ID;
			break;
		} else if (val1 == FTE716_UPGRADE_ID_1 && val2 == FTE716_UPGRADE_ID_2) {
			ft5x46->chip_id = FTE716_CHIP_ID;
			break;
		}
	}

	if (error)
		return error;

	packet.magic = 0x03;
	packet.addr_h = 0x00;
	packet.addr_m = 0xD7;
	packet.addr_l = 0x84;

	error = ft5x46_send_block(ft5x46, &packet, FT5X46_RD_FLASH_PACKET_SIZE);
	if (error)
		return error;
	msleep(5);

	error = ft5x46_recv_byte(ft5x46, 1, &vid);
	if (error)
		return error;

	error = ft5x46_send_byte(ft5x46, 1, 0x07);
	if (error)
		return error;
	msleep(200);

	return vid;
}

static void ft5x46_wait_for_ready(struct ft5x46_data *ft5x46,
		u8 val1, u8 val2, int try_times, int delay_ms)
{
	int i;
	int error;
	u8 recv1, recv2;
	for (i = 0; i < try_times; i++) {
		error = ft5x46_send_byte(ft5x46, 1, 0x6A);
		if (error) {
			dev_err(ft5x46->dev, "write failed\n");
			return;
		}

		error = ft5x46_recv_byte(ft5x46, 2, &recv1, &recv2);
		if (!error && recv1 == val1 && recv2 == val2) {
			dev_dbg(ft5x46->dev, "Ready: 0x%02x 0x%02x\n", val1, val2);
			break;
		}
		msleep(delay_ms);
	}

	if (i == try_times) {
		dev_err(ft5x46->dev, "Wait timeout and recv error 0x%02x 0x%02x(0x%02x 0x%02x)\n",
			recv1, recv2, val1, val2);
	}
}

static int ft5x46_load_firmware(struct ft5x46_data *ft5x46,
		struct ft5x46_firmware_data *firmware, bool *upgraded)
{
	struct ft5x46_packet packet;
	int i, j, length, error = 0;
	u8 val1, val2, vid,  ecc = 0, id;
#ifdef CONFIG_TOUCHSCREEN_FT5X46_CALIBRATE
	const int max_calib_time = 3;
	bool calib_ok = false;
#endif
	bool is_5336_fwsize_30 = false;
	const struct firmware *fw;
	int packet_num;
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;
	struct ft5x46_upgrade_info *ui = &pdata->ui;

	/* step 0a: check and init argument */
	if (upgraded)
		*upgraded = false;

	if (firmware == NULL)
		return 0;

	error = ft5x46_hid_to_std_i2c(ft5x46);
	if (error) {
		dev_err(ft5x46->dev, "HidI2c change to StdI2c fail!\n");
	}

	/* step 0b: find the right firmware for touch screen */
	error = ft5x46_read_byte(ft5x46, FT5X0X_ID_G_FT5201ID, &vid);
	if (error)
		return error;

	dev_info(ft5x46->dev, "firmware vendor is %02x\n", vid);

	id = vid;
	if (vid == FT5X0X_ID_G_FT5201ID ||
		vid == 0 || ft5x46->chip_id == 0) {
		vid = ft5x46_get_factory_id(ft5x46);
		dev_err(ft5x46->dev, "firmware corruption, read real factory id = 0x%x!\n", vid);
	}

	for (i = 0; i < pdata->cfg_size; i++, firmware++) {
		if (vid == firmware->vendor) {
			if (ft5x46->dev->of_node)
				if (ft5x46->chip_id == firmware->chip) {
					dev_info(ft5x46->dev, "chip id = 0x%x, found it!\n",
						ft5x46->chip_id);
					ft5x46->current_index = i;
					break;
				}
				else
					continue;
			else
				break;
		}
	}

	if (!ft5x46->dev->of_node && firmware->size == 0) {
		dev_err(ft5x46->dev, "unknown touch screen vendor, failed!\n");
		return -ENOENT;
	} else if (ft5x46->dev->of_node && (i == pdata->cfg_size)) {
		dev_err(ft5x46->dev, "Failed to find matched config!\n");
		return -ENOENT;
	}

	if (firmware->size == 0 && ft5x46->dev->of_node) {
		dev_info(ft5x46->dev, "firmware name = %s\n", firmware->fwname);
		error = request_firmware(&fw, firmware->fwname, ft5x46->dev);
		if (!error) {
			firmware->data = kmalloc((int)fw->size, GFP_KERNEL);
			if (!firmware->data) {
				dev_err(ft5x46->dev, "Failed to allocate firmware!\n");
				return -ENOMEM;
			} else {
				memcpy(firmware->data, fw->data, (int)fw->size);
				firmware->size = (int)fw->size;
			}
			release_firmware(fw);
		} else {
			dev_err(ft5x46->dev, "Cannot find firmware %s\n", firmware->fwname);
			return -ENODEV;
		}
	}

	if (firmware->data[firmware->size - 12] == 30)
		is_5336_fwsize_30 = true;
	else
		is_5336_fwsize_30 = false;

	/* step 1: check firmware id is different */
	error = ft5x46_read_byte(ft5x46, FT5X0X_REG_FW_VER, &id);
	if (error)
		return error;
	dev_info(ft5x46->dev, "firmware version is %02x\n", id);

	if (id == firmware->data[firmware->size + FT5X0X_FIRMWARE_VERION])
		return 0;
	dev_info(ft5x46->dev, "upgrade firmware to %02x\n",
		firmware->data[firmware->size + FT5X0X_FIRMWARE_VERION]);
	dev_info(ft5x46->dev, "[FTS] step1: check fw id\n");

	for (i = 0, error = -1; i < sizeof(reset_delay) && error; i++) {
		/* step 2: reset device */
		error = ft5x46_write_byte(ft5x46, FT5X0X_REG_RESET, 0xaa);
		if (error)
			continue;
		msleep(ui->delay_aa);

		error = ft5x46_write_byte(ft5x46, FT5X0X_REG_RESET, 0x55);
		if (error)
			continue;
		msleep(reset_delay[i]);
		dev_info(ft5x46->dev, "[FTS] step2: Reset device.\n");

		error = ft5x46_hid_to_std_i2c(ft5x46);
		if (error) {
			dev_err(ft5x46->dev, "HidI2c change to StdI2c fail!\n");
		}
		msleep(10);

		for (j = 0; j < 10; j++) {
			error = ft5x46_send_byte(ft5x46, 2, 0x55, 0xaa);
			msleep(5);
			if (!error)
				break;
		}
		if (error)
			continue;

		dev_info(ft5x46->dev, "[FTS] step3: Enter upgrade mode.\n");

		/* step 4: check device id */
		error = ft5x46_send_byte(ft5x46, 4, 0x90, 0x00, 0x00, 0x00);
		if (error) {
			dev_err(ft5x46->dev, "Failed to send 90 00 00 00\n");
			continue;
		}

		error = ft5x46_recv_byte(ft5x46, 2, &val1, &val2);
		if (error) {
			dev_err(ft5x46->dev, "Failed to receive device id\n");
			continue;
		}

		dev_info(ft5x46->dev, "read id = 0x%x, 0x%x, id = 0x%x, 0x%x\n", val1, val2,
			ui->upgrade_id_1, ui->upgrade_id_2);
		if (val1 != ui->upgrade_id_1 || val2 != ui->upgrade_id_2)
			error = -ENODEV;

		dev_info(ft5x46->dev, "[FTS] step4: Check device id.\n");
	}

	if (error) /* check the final result */
		return error;

	error = ft5x46_send_byte(ft5x46, 1, 0xcd);
	if (error)
		return error;
	error = ft5x46_recv_byte(ft5x46, 1, &val1);
	if (error)
		return error;
	dev_info(ft5x46->dev, "[FTS] bootloader version is 0x%x\n", val1);

	/* step 5: erase device */
	error = ft5x46_send_byte(ft5x46, 1, 0x61);
	if (error)
		return error;
	msleep(1500);
	if (0) {
		if (is_5336_fwsize_30) {
			error = ft5x46_send_byte(ft5x46, 1, 0x63);
			if (error)
				return error;
			msleep(50);
		}
	}
	ft5x46_wait_for_ready(ft5x46, 0xF0, 0xAA, 15, 50);

	/* Write firmware file length to bootloader */
	error = ft5x46_send_byte(ft5x46, 4, 0xB0,
		(u8)((firmware->size >> 16) & 0xFF),
		(u8)((firmware->size >> 8) & 0xFF),
		(u8)(firmware->size & 0xFF));

	dev_info(ft5x46->dev, "[FTS] step5: Erase device.\n");

	/* step 6: flash firmware to device */

	/* step 6a: send data in 128 bytes chunk each time */
	packet.magic1 = 0xbf;
	packet.magic2 = 0x00;
	packet_num = firmware->size / FT5X0X_PACKET_LENGTH;

	for (i = 0; i < packet_num ; i++) {
		length = FT5X0X_PACKET_LENGTH;
		packet.offset = cpu_to_be16(i * FT5X0X_PACKET_LENGTH);
		packet.length = cpu_to_be16(length);

		for (j = 0; j < length; j++) {
			packet.payload[j] = firmware->data[i * FT5X0X_PACKET_LENGTH + j];
			ecc ^= firmware->data[i * FT5X0X_PACKET_LENGTH + j];
		}

		error = ft5x46_send_block(ft5x46, &packet,
					FT5X0X_PACKET_HEADER + length);
		if (error)
			return error;

		ft5x46_wait_for_ready(ft5x46, (u8)((0x1000 + i) >> 8), (u8)((0x1000 + i) & 0xFF), 30, 1);
	}
#if 0
	for (i = 0; i < firmware->size; i += length) {
		length = min(FT5X0X_PACKET_LENGTH,
				firmware->size - i);

		packet.offset = cpu_to_be16(i);
		packet.length = cpu_to_be16(length);

		for (j = 0; j < length; j++) {
			packet.payload[j] = firmware->data[i + j];
			ecc ^= firmware->data[i + j];
		}

		error = ft5x46_send_block(ft5x46, &packet,
					FT5X0X_PACKET_HEADER + length);
		if (error)
			return error;

		ft5x46_wait_for_ready(ft5x46, (u8)((0x1000 + packet_num) >> 8), (u8)((0x1000 + packet_num) & 0xFF), 30, 1);
		packet_num ++;
	}
#endif
	dev_info(ft5x46->dev, "[FTS] step6a: Send data in 128 bytes chunk each time.\n");

	/* step 6b: send the last bytes */
	if (firmware->size % FT5X0X_PACKET_LENGTH > 0) {
		length = firmware->size % FT5X0X_PACKET_LENGTH;

		packet.offset = cpu_to_be16(packet_num * FT5X0X_PACKET_LENGTH);
		packet.length = cpu_to_be16(length);

		for (j = 0; j < length; j++) {
			packet.payload[j] = firmware->data[packet_num * FT5X0X_PACKET_LENGTH + j];
			ecc ^= firmware->data[packet_num * FT5X0X_PACKET_LENGTH + j];
		}

		error = ft5x46_send_block(ft5x46, &packet,
					FT5X0X_PACKET_HEADER + length);
		if (error)
			return error;
		ft5x46_wait_for_ready(ft5x46, (u8)((0x1000 + i) >> 8), (u8)((0x1000 + i) & 0xFF), 30, 1);
	}
	dev_info(ft5x46->dev, "[FTS] step6b: Send the last bytes.\n");

	msleep(100);

	/* step 7: verify checksum */
	error = ft5x46_send_byte(ft5x46, 1, 0x64);
	if (error)
		return error;
	msleep(300);

	error = ft5x46_send_byte(ft5x46, 6, 0x65, 0, 0, 0,
			(u8)((firmware->size >> 8) & 0xFF),
			(u8)(firmware->size & 0xFF));
	if (error)
		return error;
	msleep(300);

	ft5x46_wait_for_ready(ft5x46, 0xF0, 0x55, 10, 10);

	error = ft5x46_send_byte(ft5x46, 1, 0x66);
	if (error)
		return error;

	error = ft5x46_recv_byte(ft5x46, 1, &val1);
	if (error)
		return error;

	if (val1 != ecc) {
		dev_err(ft5x46->dev, "ECC error! CHIP=%02x, FW=%02x",
			val1, ecc);
		return -ERANGE;
	}
	dev_info(ft5x46->dev, "[FTS] step7: Verify checksum.\n");

	/* step 8: reset to new firmware */
	error = ft5x46_send_byte(ft5x46, 1, 0x07);
	if (error)
		return error;
	msleep(300);
	dev_info(ft5x46->dev, "[FTS] step8: Reset to new firmware.\n");

	error = ft5x46_hid_to_std_i2c(ft5x46);
	if (error) {
		dev_err(ft5x46->dev, "[FTS] Change to StdI2C failed\n");
	}

#ifdef CONFIG_TOUCHSCREEN_FT5X46_CALIBRATE
	/* step 9: calibrate the reference value */
	for (i = 0; i < max_calib_time; i++) {
		error = ft5x46_auto_calib(ft5x46);
		if (!error) {
			calib_ok = true;
			dev_info(ft5x46->dev, "[FTS] step9: Calibrate the ref value successfully.\n");
			break;
		}
	}
	if (!calib_ok) {
		dev_info(ft5x46->dev, "[FTS] step9: Calibrate the ref value failed.\n");
		return error;
	}
#endif

	if (upgraded)
		*upgraded = true;

	return 0;
}

static int ft8716_load_pramboot(struct ft5x46_data *ft5x46, u8 *pramboot, u32 pramboot_size)
{
	struct ft5x46_packet packet;
	int i, j, length, error = 0;
	u8 val1, val2, ecc = 0;
	int packet_num;
	u8 *pcheckbuffer = NULL;

	pcheckbuffer = kmalloc(pramboot_size + 1, GFP_ATOMIC);
	if (!pcheckbuffer) {
		dev_err(ft5x46->dev, "Memory alloc failed\n");
		return -ENOMEM;
	}

	for (i = 0, error = -1; i < 30 && error; i++) {
		/* step 2: reset device */
		error = ft5x46_write_byte(ft5x46, FT5X0X_REG_RESET, 0xaa);
		if (error)
			continue;
		msleep(FT8716_UPGRADE_AA_DELAY);

		error = ft5x46_write_byte(ft5x46, FT5X0X_REG_RESET, 0x55);
		if (error)
			continue;
		msleep(200);
		dev_info(ft5x46->dev, "[FTS] step1: Reset device.\n");

		for (j = 0; j < 10; j++) {
			error = ft5x46_send_byte(ft5x46, 1, 0x55);
			msleep(5);
			if (!error)
				break;
		}

		if (error)
			continue;

		dev_info(ft5x46->dev, "[FTS] step2: Enter upgrade mode.\n");

		/* step 4: check device id */
		error = ft5x46_send_byte(ft5x46, 4, 0x90, 0x00, 0x00, 0x00);
		if (error) {
			dev_err(ft5x46->dev, "Failed to send 90 00 00 00\n");
			continue;
		}

		error = ft5x46_recv_byte(ft5x46, 2, &val1, &val2);
		if (error) {
			dev_err(ft5x46->dev, "Failed to receive device id\n");
			continue;
		}

		dev_info(ft5x46->dev, "read id = 0x%x, 0x%x\n", val1, val2);
		if ((val1 != FT8716_CHIP_ID && val1 != FTE716_CHIP_ID) || val2 != 0x16)
			error = -ENODEV;
	}

	if (error) /* check the final result */
		goto out;

	dev_info(ft5x46->dev, "[FTS] step3: load pram boot.\n");

	/* step 3: send data in 128 bytes chunk each time */
	packet.magic1 = 0xae;
	packet.magic2 = 0x00;
	packet_num = pramboot_size / FT5X0X_PACKET_LENGTH;

	for (i = 0; i < packet_num ; i++) {
		length = FT5X0X_PACKET_LENGTH;
		packet.offset = cpu_to_be16(i * FT5X0X_PACKET_LENGTH);
		packet.length = cpu_to_be16(length);

		for (j = 0; j < length; j++) {
			packet.payload[j] = pramboot[i * FT5X0X_PACKET_LENGTH + j];
			ecc ^= pramboot[i * FT5X0X_PACKET_LENGTH + j];
		}

		error = ft5x46_send_block(ft5x46, &packet,
					FT5X0X_PACKET_HEADER + length);
		if (error)
			goto out;
	}

	/* step 4: send the last bytes */
	if (pramboot_size % FT5X0X_PACKET_LENGTH > 0) {
		length = pramboot_size % FT5X0X_PACKET_LENGTH;

		packet.offset = cpu_to_be16(packet_num * FT5X0X_PACKET_LENGTH);
		packet.length = cpu_to_be16(length);

		for (j = 0; j < length; j++) {
			packet.payload[j] = pramboot[packet_num * FT5X0X_PACKET_LENGTH + j];
			ecc ^= pramboot[packet_num * FT5X0X_PACKET_LENGTH + j];
		}

		error = ft5x46_send_block(ft5x46, &packet,
					FT5X0X_PACKET_HEADER + length);
		if (error)
			goto out;
	}
	dev_info(ft5x46->dev, "[FTS] step4: Send the last bytes.\n");

	msleep(100);

	/* step 5: verify checksum */
	packet.magic1 = 0x85;
	packet.magic2 = 0x00;
	packet_num = pramboot_size / FT5X0X_PACKET_LENGTH;
	for (i = 0; i < packet_num ; i++) {
		length = FT5X0X_PACKET_LENGTH;
		packet.offset = cpu_to_be16(i * FT5X0X_PACKET_LENGTH);
		packet.length = cpu_to_be16(length);

		error = ft5x46_send_block(ft5x46, &packet, 4);
		if (error)
			goto out;
		error = ft5x46_recv_block(ft5x46, pcheckbuffer + i * FT5X0X_PACKET_LENGTH, length);
		if (error)
			goto out;
	}

	if (pramboot_size % FT5X0X_PACKET_LENGTH > 0) {
		length = pramboot_size % FT5X0X_PACKET_LENGTH;

		packet.offset = cpu_to_be16(packet_num * FT5X0X_PACKET_LENGTH);
		packet.length = cpu_to_be16(length);

		error = ft5x46_send_block(ft5x46, &packet, 4);
		if (error)
			goto out;
		error = ft5x46_recv_block(ft5x46, pcheckbuffer + packet_num * FT5X0X_PACKET_LENGTH, length);
		if (error)
			goto out;
	}

	for (i = 0; i < pramboot_size; i++) {
		if (pcheckbuffer[i] != pramboot[i]) {
			dev_err(ft5x46->dev, "[FTS] step5: verify checksum error\n");
			error = ENOENT;
			goto out;
		}
	}
	dev_info(ft5x46->dev, "[FTS] step5: verify checksum\n");
	error = ft5x46_send_byte(ft5x46, 1, 0x08);
	msleep(20);
	dev_info(ft5x46->dev, "[FTS] step6: start app\n");
out:
	kfree(pcheckbuffer);
	return error;
}

static u8 ft8716_get_factory_id(struct ft5x46_data *ft5x46)
{
	int j, error = 0;
	struct ft5x46_rd_flash_packet packet;
	u8 val1, val2, flash_vendor_id;
	u8 vid;

	error = ft8716_load_pramboot(ft5x46, ft8716_pramboot, sizeof(ft8716_pramboot));
	if (error)
		return error;

	for (j = 0; j < 10; j++) {
		error = ft5x46_send_byte(ft5x46, 2, 0x55, 0xaa);
		msleep(5);
		if (!error)
			break;
	}
	if (error) {
		dev_err(ft5x46->dev, "Failed to reset device\n");
		goto fw_reset;
	}

	error = ft5x46_send_byte(ft5x46, 4, 0x90, 0x00, 0x00, 0x00);
	if (error) {
		dev_err(ft5x46->dev, "Failed to send 90 00 00 00\n");
		goto fw_reset;
	}
	error = ft5x46_recv_byte(ft5x46, 2, &val1, &val2);
	if (error) {
		dev_err(ft5x46->dev, "Failed to receive device id\n");
		goto fw_reset;
	}
	dev_info(ft5x46->dev, "read id = 0x%x, 0x%x\n", val1, val2);

	if (val1 == FT5316_UPGRADE_ID_1 && val2 == FT5316_UPGRADE_ID_2) {
		ft5x46->chip_id = FT5316_CHIP_ID;
	} else if (val1 == FT5X36_UPGRADE_ID_1 && val2 == FT5X36_UPGRADE_ID_2) {
		ft5x46->chip_id = FT5X36_CHIP_ID;
	} else if (val1 == FT5X46_UPGRADE_ID_1 && val2 == FT5X46_UPGRADE_ID_2) {
		ft5x46->chip_id = FT5X46_CHIP_ID;
	} else if (val1 == FT8716_UPGRADE_ID_1 && val2 == FT8716_UPGRADE_ID_2) {
		ft5x46->chip_id = FT8716_CHIP_ID;
	} else if (val1 == FTE716_UPGRADE_ID_1 && val2 == FTE716_UPGRADE_ID_2) {
		ft5x46->chip_id = FTE716_CHIP_ID;
	}

	/* Read flash vendor id */
	error = ft5x46_send_byte(ft5x46, 1, 0x05);
	if (error) {
		dev_err(ft5x46->dev, "Failed to send read flash command\n");
		goto fw_reset;
	}

	error = ft5x46_recv_byte(ft5x46, 1, &flash_vendor_id);
	if (error) {
		dev_err(ft5x46->dev, "Failed to read flash vendor id\n");
		goto fw_reset;
	}
	dev_info(ft5x46->dev, "flash vendor id is 0x%x\n", flash_vendor_id);

	/* set flash sys clock 24Mhz */
	error = ft5x46_send_byte(ft5x46, 3, 0x05, 0x00, 0x00);
	if (error) {
		dev_err(ft5x46->dev, "Failed to set sys clk\n");
		goto fw_reset;
	}

	packet.magic = 0x03;
	packet.addr_h = 0x00;
	packet.addr_m = 0x00;
	packet.addr_l = 0x04;

	error = ft5x46_send_block(ft5x46, &packet, FT5X46_RD_FLASH_PACKET_SIZE);
	if (error) {
		dev_err(ft5x46->dev, "Failed to send flash reading command\n");
		goto fw_reset;
	}

	msleep(5);

	error = ft5x46_recv_byte(ft5x46, 1, &vid);
	if (error) {
		dev_err(ft5x46->dev, "Failed to read flash\n");
		goto fw_reset;
	}

fw_reset:
	error = ft5x46_send_byte(ft5x46, 1, 0x07);
	if (error)
		return error;
	msleep(200);

	return vid;
}

static int ft8716_load_lockdown_info(struct ft5x46_data *ft5x46)
{
	int error = 0;
	int i, j;
	u8 val1, val2, flash_vendor_id;

	struct ft5x46_rd_flash_packet packet;

	error = ft8716_load_pramboot(ft5x46, ft8716_pramboot, sizeof(ft8716_pramboot));
	if (error)
		return error;

	for (j = 0; j < 10; j++) {
		error = ft5x46_send_byte(ft5x46, 2, 0x55, 0xaa);
		msleep(5);
		if (!error)
			break;
	}
	if (error) {
		dev_err(ft5x46->dev, "Failed to reset device\n");
		goto out;
	}

	error = ft5x46_send_byte(ft5x46, 4, 0x90, 0x00, 0x00, 0x00);
	if (error) {
		dev_err(ft5x46->dev, "Failed to send 90 00 00 00\n");
		goto out;
	}
	error = ft5x46_recv_byte(ft5x46, 2, &val1, &val2);
	if (error) {
		dev_err(ft5x46->dev, "Failed to receive device id\n");
		goto out;
	}
	dev_info(ft5x46->dev, "read id = 0x%x, 0x%x\n", val1, val2);

	if (val1 == FT5316_UPGRADE_ID_1 && val2 == FT5316_UPGRADE_ID_2) {
		ft5x46->chip_id = FT5316_CHIP_ID;
	} else if (val1 == FT5X36_UPGRADE_ID_1 && val2 == FT5X36_UPGRADE_ID_2) {
		ft5x46->chip_id = FT5X36_CHIP_ID;
	} else if (val1 == FT5X46_UPGRADE_ID_1 && val2 == FT5X46_UPGRADE_ID_2) {
		ft5x46->chip_id = FT5X46_CHIP_ID;
	} else if (val1 == FT8716_UPGRADE_ID_1 && val2 == FT8716_UPGRADE_ID_2) {
		ft5x46->chip_id = FT8716_CHIP_ID;
	} else if (val1 == FTE716_UPGRADE_ID_1 && val2 == FTE716_UPGRADE_ID_2) {
		ft5x46->chip_id = FTE716_CHIP_ID;
	}

	/* Read flash vendor id */
	error = ft5x46_send_byte(ft5x46, 1, 0x05);
	if (error) {
		dev_err(ft5x46->dev, "Failed to send read flash command\n");
		goto out;
	}

	error = ft5x46_recv_byte(ft5x46, 1, &flash_vendor_id);
	if (error) {
		dev_err(ft5x46->dev, "Failed to read flash vendor id\n");
		goto out;
	}
	dev_info(ft5x46->dev, "flash vendor id is 0x%x\n", flash_vendor_id);

	/* set flash sys clock 24Mhz */
	error = ft5x46_send_byte(ft5x46, 3, 0x05, 0x00, 0x00);
	if (error) {
		dev_err(ft5x46->dev, "Failed to set sys clk\n");
		goto out;
	}

	packet.magic = 0x03;
	packet.addr_h = 0x00;
	packet.addr_m = 0x00;
	packet.addr_l = 0x20;

	error = ft5x46_send_block(ft5x46, &packet, FT5X46_RD_FLASH_PACKET_SIZE);
	msleep(5);
	if (error) {
		dev_err(ft5x46->dev, "Failed to send flash reading command\n");
		goto out;
	}
	error = ft5x46_recv_byte(ft5x46, 8,
		&ft5x46->lockdown_info[0], &ft5x46->lockdown_info[1],
		&ft5x46->lockdown_info[2], &ft5x46->lockdown_info[3],
		&ft5x46->lockdown_info[4], &ft5x46->lockdown_info[5],
		&ft5x46->lockdown_info[6], &ft5x46->lockdown_info[7]);
	if (error) {
		dev_err(ft5x46->dev, "Failed to read flash\n");
		goto out;
	}

	for (i = 0; i < 8; i++)
		dev_info(ft5x46->dev, "Lockdown[%d] val = 0x%x\n", i, ft5x46->lockdown_info[i]);
out:
	return error;
}

static int ft8716_reset_firmware(struct ft5x46_data *ft5x46)
{
	int error = 0;

	error = ft5x46_send_byte(ft5x46, 1, 0x07);
	if (error)
		return error;
	msleep(200);
	return error;
}

static int ft8716_get_lockdown_info(struct ft5x46_data *ft5x46)
{
	ft8716_load_lockdown_info(ft5x46);
	return ft8716_reset_firmware(ft5x46);
}

static int ft8716_load_firmware(struct ft5x46_data *ft5x46,
		struct ft5x46_firmware_data *firmware, bool *upgraded)
{
	int i, j, error = 0;
	u8 val1, val2, vid,  ecc = 0, id, reg_val_id = 0;
	u32 length, check_start = 0x1000;
	u32 check_off = 0x20;
	u32 start_addr = 0x00;
	u8 packet_buf[16 + FT5X0X_PACKET_LENGTH];
	u8 *tp_maker = NULL;

	const struct firmware *fw;
	int packet_num;
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;
	struct ft5x46_upgrade_info *ui = &pdata->ui;

	/* step 0a: check and init argument */
	if (upgraded)
		*upgraded = false;

	if (firmware == NULL)
		return 0;

	/* step 0b: find the right firmware for touch screen */
	error = ft5x46_read_byte(ft5x46, FT5X0X_ID_G_FT5201ID, &vid);
	if (error)
		return error;

	dev_info(ft5x46->dev, "firmware vendor is %02x\n", vid);

	id = vid;
	if (vid == FT5X0X_ID_G_FT5201ID ||
		vid == 0 || ft5x46->chip_id == 0) {
		vid = ft8716_get_factory_id(ft5x46);
		dev_err(ft5x46->dev, "firmware corruption, read real factory id = 0x%x!\n", vid);
	}

	error = ft5x46_read_byte(ft5x46, FT5X0X_REG_FW_VER, &id);
	if (error)
		return error;
	dev_info(ft5x46->dev, "firmware version is %02x\n", id);

	if (!ft5x46->lockdown_info_acquired) {
		error = ft8716_load_lockdown_info(ft5x46);
		if (error) {
			ft8716_reset_firmware(ft5x46);
			return error;
		}
		update_hardware_info(TYPE_TP_MAKER, ft5x46->lockdown_info[0] - 0x30);
		tp_maker = kzalloc(20, GFP_KERNEL);
		if (tp_maker == NULL)
			dev_err(ft5x46->dev, "fail to alloc vendor name memory\n");
		else {
			kfree(tp_maker);
			tp_maker = NULL;
		}
		ft5x46->lockdown_info_acquired = true;
		wake_up(&ft5x46->lockdown_info_acquired_wq);
	}

	for (i = 0; i < pdata->cfg_size; i++, firmware++) {
		if (ft5x46->lockdown_info[0] == firmware->tp_vendor &&
			ft5x46->lockdown_info[1] == firmware->lcd_vendor) {
			if (ft5x46->dev->of_node) {
				dev_info(ft5x46->dev, "chip id = 0x%x, found it!\n",
					ft5x46->chip_id);
				ft5x46->current_index = i;
				break;
			} else
				break;
		}
	}

	if (!ft5x46->dev->of_node && firmware->size == 0) {
		dev_err(ft5x46->dev, "unknown touch screen vendor, failed!\n");
		ft8716_reset_firmware(ft5x46);
		return -ENOENT;
	} else if (ft5x46->dev->of_node && (i == pdata->cfg_size)) {
		dev_err(ft5x46->dev, "Failed to find matched config!\n");
		ft8716_reset_firmware(ft5x46);
		return -ENOENT;
	}

	if (firmware->size == 0 && ft5x46->dev->of_node) {
		dev_info(ft5x46->dev, "firmware name = %s\n", firmware->fwname);
		error = request_firmware(&fw, firmware->fwname, ft5x46->dev);
		if (!error) {
			firmware->data = kmalloc((int)fw->size, GFP_KERNEL);
			if (!firmware->data) {
				dev_err(ft5x46->dev, "Failed to allocate firmware!\n");
				ft8716_reset_firmware(ft5x46);
				return -ENOMEM;
			} else {
				memcpy(firmware->data, fw->data, (int)fw->size);
				firmware->size = (int)fw->size;
			}
			release_firmware(fw);
		} else {
			dev_err(ft5x46->dev, "Cannot find firmware %s\n", firmware->fwname);
			ft8716_reset_firmware(ft5x46);
			return -ENODEV;
		}
	}

	/* step 1: check firmware id is different */

	if (id == firmware->data[FT8716_FIRMWARE_VERION]) {
		ft8716_reset_firmware(ft5x46);
		return 0;
	}

	if (ft5x46->hw_is_ready) { /* Reload pramboot */
		error = ft8716_load_pramboot(ft5x46, ft8716_pramboot, sizeof(ft8716_pramboot));
		if (error)
			return error;
	}

	error = ft5x46_send_byte(ft5x46, 1, 0x05);
	if (error)
		return error;

	error = ft5x46_recv_byte(ft5x46, 1, &reg_val_id);
	if (error)
		return error;
	if (reg_val_id == 0x80 && firmware->size > (1024 * 64)) {
		error = -ERANGE;
		return error;
	} else if (reg_val_id == 0x81 && firmware->size > (1024 * 60)) {
		error = -ERANGE;
		return error;
	}

	dev_info(ft5x46->dev, "upgrade firmware to %02x\n",
		firmware->data[FT8716_FIRMWARE_VERION]);
	dev_info(ft5x46->dev, "[FTS] step1: check fw id\n");

	for (i = 0, error = -1; i < sizeof(reset_delay) && error; i++) {
		/* step 2: reset device */
		msleep(10);

		for (j = 0; j < 10; j++) {
			error = ft5x46_send_byte(ft5x46, 2, 0x55, 0xaa);
			msleep(5);
			if (!error)
				break;
		}
		if (error)
			continue;

		dev_info(ft5x46->dev, "[FTS] step2: Enter upgrade mode.\n");

		/* step 4: check device id */
		error = ft5x46_send_byte(ft5x46, 4, 0x90, 0x00, 0x00, 0x00);
		if (error) {
			dev_err(ft5x46->dev, "Failed to send 90 00 00 00\n");
			continue;
		}

		error = ft5x46_recv_byte(ft5x46, 2, &val1, &val2);
		if (error) {
			dev_err(ft5x46->dev, "Failed to receive device id\n");
			continue;
		}

		dev_info(ft5x46->dev, "read id = 0x%x, 0x%x, id = 0x%x, 0x%x\n", val1, val2,
			ui->upgrade_id_1, ui->upgrade_id_2);
		if ((val1 != FT8716_UPGRADE_ID_1 || val2 != FT8716_UPGRADE_ID_2) &&
			(val1 != FTE716_UPGRADE_ID_1 || val2 != FTE716_UPGRADE_ID_2))
			error = -ENODEV;

		dev_info(ft5x46->dev, "[FTS] step3: Check device id.\n");
	}

	if (error) /* check the final result */
		return error;

	/* step 5: erase app and panel parameter area */
	error = ft5x46_send_byte(ft5x46, 3, 0x05, reg_val_id, 0x00);
	if (error)
		return error;

	dev_info(ft5x46->dev, "[FTS] step4: erase app and panel parameter area\n");

	/* step 6: set paramboot download mode */
	error = ft5x46_send_byte(ft5x46, 2, 0x09, 0x0B);
	if (error)
		return error;

	msleep(50);
	error = ft5x46_send_byte(ft5x46, 1, 0x61);
	if (error)
		return error;
	msleep(1500);

	ft5x46_wait_for_ready(ft5x46, 0xF0, 0xAA, 15, 50);

	dev_info(ft5x46->dev, "[FTS] step5: flash firmware to device.\n");
	/* step 6a: send data in 128 bytes chunk each time */
	packet_buf[0] = 0xbf;
	packet_num = firmware->size / FT5X0X_PACKET_LENGTH;

	for (i = 0; i < packet_num; i++) {
		length = check_start + i * FT5X0X_PACKET_LENGTH;
		packet_buf[1] = length >> 16;
		packet_buf[2] = length >> 8;
		packet_buf[3] = length;

		start_addr  = length;
		length = FT5X0X_PACKET_LENGTH;

		packet_buf[4] = length >> 8;
		packet_buf[5] = length;

		check_off = start_addr/length;

		for (j = 0; j < FT5X0X_PACKET_LENGTH; j++) {
			packet_buf[6 + j] = firmware->data[i * FT5X0X_PACKET_LENGTH + j];
			ecc ^= firmware->data[i * FT5X0X_PACKET_LENGTH + j];
		}

		error = ft5x46_send_block(ft5x46, packet_buf, 6 + length);
		if (error)
			return error;

		ft5x46_wait_for_ready(ft5x46, (u8)((check_off + check_start) >> 8),
			(u8)((check_off + check_start) & 0xFF), 30, 1);
	}

	dev_info(ft5x46->dev, "[FTS] step6a: Send data in 128 bytes chunk each time.\n");

	/* step 6b: send the last bytes */
	if (firmware->size % FT5X0X_PACKET_LENGTH > 0) {
		length = check_start + packet_num * FT5X0X_PACKET_LENGTH;
		packet_buf[1] = length >> 16;
		packet_buf[2] = length >> 8;
		packet_buf[3] = length;

		start_addr  = length;
		length = firmware->size % FT5X0X_PACKET_LENGTH;

		packet_buf[4] = length >> 8;
		packet_buf[5] = length;

		check_off = start_addr/length;

		for (j = 0; j < length; j++) {
			packet_buf[6 + j] = firmware->data[packet_num * FT5X0X_PACKET_LENGTH + j];
			ecc ^= firmware->data[packet_num * FT5X0X_PACKET_LENGTH + j];
		}

		error = ft5x46_send_block(ft5x46, packet_buf, 6 + length);
		if (error)
			return error;

		ft5x46_wait_for_ready(ft5x46, (u8)((check_off + check_start) >> 8),
			(u8)((check_off + check_start) & 0xFF), 30, 1);
	}
	dev_info(ft5x46->dev, "[FTS] step6b: Send the last bytes.\n");

	msleep(100);

	/* step 7: verify checksum */
	error = ft5x46_send_byte(ft5x46, 1, 0x64);
	if (error)
		return error;
	msleep(300);

	if (firmware->size > LEN_FLASH_ECC_MAX)
		length = LEN_FLASH_ECC_MAX;
	else
		length = firmware->size;

	error = ft5x46_send_byte(ft5x46, 6, 0x65,
			(u8)(check_start >> 16),
			(u8)(check_start >> 8),
			(u8)check_start,
			(u8)((length >> 8) & 0xFF),
			(u8)(length & 0xFF));
	if (error)
		return error;
	msleep(firmware->size/256);

	ft5x46_wait_for_ready(ft5x46, 0xF0, 0x55, 10, 10);

	if (firmware->size > LEN_FLASH_ECC_MAX) {
		length = LEN_FLASH_ECC_MAX;
		error = ft5x46_send_byte(ft5x46, 6, 0x65,
			(u8)(length >> 16),
			(u8)(length >> 8),
			(u8)length,
			(u8)(((firmware->size - length) >> 8) & 0xFF),
			(u8)((firmware->size - length) & 0xFF));
		if (error)
			return error;
		msleep(firmware->size/256);
		ft5x46_wait_for_ready(ft5x46, 0xF0, 0x55, 10, 10);
	}

	error = ft5x46_send_byte(ft5x46, 1, 0x66);
	if (error)
		return error;

	error = ft5x46_recv_byte(ft5x46, 1, &val1);
	if (error)
		return error;

	if (val1 != ecc) {
		dev_err(ft5x46->dev, "ECC error! CHIP=%02x, FW=%02x",
			val1, ecc);
		return -ERANGE;
	}
	dev_info(ft5x46->dev, "[FTS] step7: Verify checksum.\n");

	/* step 8: reset to new firmware */
	error = ft5x46_send_byte(ft5x46, 1, 0x07);
	if (error)
		return error;
	msleep(300);
	dev_info(ft5x46->dev, "[FTS] step8: Reset to new firmware.\n");

	if (upgraded)
		*upgraded = true;

	return 0;
}


static int ft5x46_read_touchdata(struct ft5x46_data *ft5x46)
{
	struct ft5x46_ts_event *event = &ft5x46->event;
	u8 buf[FT5X46_POINT_READ_BUF] = {0};
	int i, ret;
	u8 point_id;
	u16 xh, xl, yh, yl;
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	bool proximity_near = 0;
#endif

	ret = ft5x46_read_block(ft5x46, 0,
				buf, FT5X46_POINT_READ_BUF);
	if (ret < 0) {
		dev_err(ft5x46->dev, "read touchdata failed\n");
		return ret;
	}

#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	if (ft5x46->proximity_enable) {
		if (buf[1] == 0xC0) {
			proximity_near = 1;
		} else if (buf[1] == 0xE0) {
			proximity_near = 0;
		} else
			pr_err("touch data err 0x%x\n", buf[1]);

		if (proximity_near != ft5x46->proximity_near) {
			ft5x46->proximity_near = proximity_near;
			input_report_abs(ft5x46->proximity, ABS_DISTANCE, proximity_near);
			input_sync(ft5x46->proximity);
		}
	}
#endif

	memset(event, 0, sizeof(struct ft5x46_ts_event));

	for (i = 0; i < FT5X0X_MAX_FINGER; i++) {
		point_id = (buf[FT5X46_TOUCH_LENGTH * i + FT5X46_ID_POS]) >> 4;
		if (point_id >= FT5X46_MAX_ID)
			break;
		else
			event->touch_point++;

		xh = (u16)(buf[FT5X46_TOUCH_LENGTH * i + FT5X46_XH_POS] & 0x0F);
		xl = (u16)(buf[FT5X46_TOUCH_LENGTH * i + FT5X46_XL_POS] & 0xFF);

		yh = (u16)(buf[FT5X46_TOUCH_LENGTH * i + FT5X46_YH_POS] & 0x0F);
		yl = (u16)(buf[FT5X46_TOUCH_LENGTH * i + FT5X46_YL_POS] & 0xFF);

		event->x[i] = (xh << 8) | xl;
		event->y[i] = (yh << 8) | yl;
		event->touch_event[i] = buf[FT5X46_TOUCH_LENGTH * i + FT5X46_EVENT_POS] >> 6;
		event->finger_id[i] = buf[FT5X46_TOUCH_LENGTH * i + FT5X46_ID_POS] >> 4;
		event->touch_misc[i] = buf[FT5X46_TOUCH_LENGTH * i + FT5X46_MISC_POS] & 0xF;

		pr_debug("[FINGER %d]: id = %d, event = %d, x = %d, y = %d\n",
			i, event->finger_id[i], event->touch_event[i],
			event->x[i], event->y[i]);
	}

	return 0;
}

static void ft5x46_clear_touch_event(struct ft5x46_data *ft5x46)
{
	struct ft5x46_ts_event *event = &ft5x46->event;
	int i;

	for (i = 0; i < FT5X0X_MAX_FINGER; i++) {
		input_mt_slot(ft5x46->input, i);
		input_mt_report_slot_state(ft5x46->input, MT_TOOL_FINGER, false);
	}

	memset(event, 0, sizeof(struct ft5x46_ts_event));
	ft5x46->touchs = 0;

	input_report_key(ft5x46->input, BTN_TOUCH, 0);
	input_sync(ft5x46->input);
}

static void ft5x46_report_value(struct ft5x46_data *ft5x46)
{
	struct ft5x46_ts_event *event = &ft5x46->event;
	int i;
	int up_point = 0;
	int touchs = 0;

	for (i = 0; i < event->touch_point; i++) {
		input_mt_slot(ft5x46->input, event->finger_id[i]);

		if (event->touch_event[i] == 0 || event->touch_event[i] == 2) {
			/* Touch down */
			input_mt_report_slot_state(ft5x46->input, MT_TOOL_FINGER, true);
			input_report_abs(ft5x46->input, ABS_MT_TOUCH_MAJOR, event->touch_misc[i]);
			input_report_abs(ft5x46->input, ABS_MT_TOUCH_MINOR, event->touch_misc[i]);
			input_report_abs(ft5x46->input, ABS_MT_POSITION_X, event->x[i]);
			input_report_abs(ft5x46->input, ABS_MT_POSITION_Y, event->y[i]);

			touchs |= BIT(event->finger_id[i]);
			ft5x46->touchs |= BIT(event->finger_id[i]);
		} else {
			/* Release finger */
			up_point++;
			input_mt_report_slot_state(ft5x46->input, MT_TOOL_FINGER, false);

			ft5x46->touchs &= ~BIT(event->finger_id[i]);
		}
	}


	for (i = 0; i < FT5X0X_MAX_FINGER; i++) {
		if (BIT(i) & (ft5x46->touchs ^ touchs)) {
			ft5x46->touchs &= ~BIT(i);
			input_mt_slot(ft5x46->input, i);
			input_mt_report_slot_state(ft5x46->input, MT_TOOL_FINGER, false);
		}
	}

	if (event->touch_point == up_point) {
		ft5x46->touchs = 0;
		input_report_key(ft5x46->input, BTN_TOUCH, 0);
	} else
		input_report_key(ft5x46->input, BTN_TOUCH, (event->touch_point > 0));

	input_sync(ft5x46->input);
}

static int ft5x46_read_gesture(struct ft5x46_data *ft5x46)
{
	unsigned char buf[FT5X46_GESTURE_POINTS_HEADER] = { 0 };
	int error = 0;
	char ch[64] = {0x0,};

	error = ft5x46_read_block(ft5x46, 0xD3, buf,
			FT5X46_GESTURE_POINTS_HEADER);
	if (error) {
		dev_err(ft5x46->dev, "Error reading GESTURE_COOR\n");
		return error;
	}

	dev_info(ft5x46->dev, "Gesture ID = %d, Point num = %d\n", buf[0],
			buf[1]);

	if (buf[0] == FT5X46_GESTURE_UP || buf[0] == FT5X46_GESTURE_DBLCLICK) {
		dev_info(ft5x46->dev, "input report key wakeup\n");
		input_event(ft5x46->input, EV_KEY, KEY_WAKEUP, 1);
		input_sync(ft5x46->input);
		input_event(ft5x46->input, EV_KEY, KEY_WAKEUP, 0);
		input_sync(ft5x46->input);
		ft5x46->dbclick_count++;
		snprintf(ch, sizeof(ch), "%d", ft5x46->dbclick_count);
	}

	return 0;
}

static int ft5x46_wakeup_reconfigure(struct ft5x46_data *ft5x46, bool enable)
{
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;
	int error = 0;

	mutex_lock(&ft5x46->mutex);

	/* wait for device reset to normal mode first */
	gpio_set_value(pdata->reset_gpio, 0);
	mdelay(5);
	gpio_set_value(pdata->reset_gpio, 1);
	mdelay(200);

	if (enable) {
		error = ft5x46_write_byte(ft5x46, 0xD0, 0x01);	/* Enable wakeup gesture */
		error |= ft5x46_write_byte(ft5x46, 0xD1, 0x14);	/* Only enable swipe up and double click */
	} else {
		error = ft5x46_write_byte(ft5x46,
				FT5X0X_ID_G_PMODE, FT5X0X_POWER_HIBERNATE);
	}

	mutex_unlock(&ft5x46->mutex);

	if (enable)
		ft5x46_enable_irq(ft5x46);
	else
		ft5x46_disable_irq(ft5x46);

	return error;
}

static irqreturn_t ft5x46_interrupt(int irq, void *dev_id)
{
	struct ft5x46_data *ft5x46 = dev_id;
	int error = 0;
	u8 val = 0;

	mutex_lock(&ft5x46->mutex);

	if (ft5x46->wakeup_mode && ft5x46->in_suspend) {
		error = ft5x46_read_byte(ft5x46, 0xD0, &val);
		if (error)
			dev_err(ft5x46->dev, "Error reading register 0xD0\n");
		else
			if (val == 1) {
				error = ft5x46_read_gesture(ft5x46);
				if (error)
					dev_err(ft5x46->dev, "Failed to read wakeup gesture\n");
			} else
				dev_err(ft5x46->dev, "Chip is in suspend, but wakeup gesture is not enabled.\n");

			goto out;
	}

	error = ft5x46_read_touchdata(ft5x46);
	if (!error) {
		ft5x46_report_value(ft5x46);
	}

out:
	mutex_unlock(&ft5x46->mutex);
	return IRQ_HANDLED;
}

int ft5x46_suspend(struct ft5x46_data *ft5x46)
{
	int error = 0;

#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	if (ft5x46->proximity_enable) {
		pr_info("Skip suspend\n");
		return 0;
	}
#endif

	if (!ft5x46->wakeup_mode)
		ft5x46_disable_irq(ft5x46);

	mutex_lock(&ft5x46->mutex);

	if (ft5x46->in_suspend)
		goto out;

	ft5x46_clear_touch_event(ft5x46);
	mutex_unlock(&ft5x46->mutex);

	cancel_delayed_work_sync(&ft5x46->noise_filter_delayed_work);
	cancel_delayed_work_sync(&ft5x46->lcd_esdcheck_work);
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	cancel_delayed_work_sync(&ft5x46->prox_enable_delayed_work);
#endif

	mutex_lock(&ft5x46->mutex);
	if (ft5x46->wakeup_mode) {
		dev_info(ft5x46->dev, "enter wakeup gesture mode\n");
		error = ft5x46_write_byte(ft5x46, 0xD0, 0x01);	/* Enable wakeup gesture */
		error |= ft5x46_write_byte(ft5x46, 0xD1, 0x14);	/* Only enable swipe up and double click */
	} else {
		error = ft5x46_write_byte(ft5x46,
				FT5X0X_ID_G_PMODE, FT5X0X_POWER_HIBERNATE);
	}
	ft5x46->in_suspend = true;

out:
	mutex_unlock(&ft5x46->mutex);
	return error;
}
EXPORT_SYMBOL_GPL(ft5x46_suspend);

int ft5x46_resume(struct ft5x46_data *ft5x46)
{
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;

#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	if (ft5x46->proximity_enable) {
		mutex_lock(&ft5x46->mutex);
		ft5x46_proximity_enable(ft5x46, ft5x46->proximity_enable);
		mutex_unlock(&ft5x46->mutex);
		pr_info("Skip resume\n");
		return 0;
	}
#endif
	/* If we are in wakeup mode, should disable IRQ firstly to avoid false interrupt */
	if (ft5x46->wakeup_mode)
		ft5x46_disable_irq(ft5x46);

	mutex_lock(&ft5x46->mutex);

	if (!ft5x46->in_suspend)
		goto out;

	if (!pdata->skip_reset_in_resume) {
		/* reset device */
		gpio_set_value(pdata->reset_gpio, 0);
		mdelay(5);
		gpio_set_value(pdata->reset_gpio, 1);
		mdelay(5);
	} else {
		pr_debug("skip reset in resume\n");
	}

	mutex_unlock(&ft5x46->mutex);

	ft5x46->force_update_noise_filter = true;
	schedule_delayed_work(&ft5x46->noise_filter_delayed_work,
				NOISE_FILTER_DELAY);
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	if (ft5x46->proximity_enable_in_suspend)
		schedule_delayed_work(&ft5x46->prox_enable_delayed_work, HZ);
#endif

	mutex_lock(&ft5x46->mutex);
	ft5x46->in_suspend = false;

	schedule_delayed_work(&ft5x46->lcd_esdcheck_work,
				msecs_to_jiffies(FT5X46_ESD_CHECK_PERIOD));

out:
	mutex_unlock(&ft5x46->mutex);
	ft5x46_enable_irq(ft5x46);
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	wake_up(&ft5x46->resume_wq);
#endif

	return 0;
}
EXPORT_SYMBOL_GPL(ft5x46_resume);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ft5x46_early_suspend(struct early_suspend *h)
{
	struct ft5x46_data *ft5x46 = container_of(h,
					struct ft5x46_data, early_suspend);
	ft5x46_suspend(ft5x46);
}

static void ft5x46_early_resume(struct early_suspend *h)
{
	struct ft5x46_data *ft5x46 = container_of(h,
					struct ft5x46_data, early_suspend);
	ft5x46_resume(ft5x46);
}
#else
static int ft5x46_input_disable(struct input_dev *in_dev)
{
	struct ft5x46_data *ft5x46 = input_get_drvdata(in_dev);

	dev_info(ft5x46->dev, "ft5x46 disable!\n");
	ft5x46_suspend(ft5x46);

	return 0;
}

static int ft5x46_input_enable(struct input_dev *in_dev)
{
	struct ft5x46_data *ft5x46 = input_get_drvdata(in_dev);

	dev_dbg(ft5x46->dev, "ft5x46 enable!\n");
	ft5x46_resume(ft5x46);

	return 0;
}
#endif

static ssize_t ft5x46_vkeys_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int i;
	int cnt;
	int count = 0;

	for (i = 0; i < vir_keypad->length; i++) {
		cnt = snprintf(buf, PAGE_SIZE - count, "0x01:%d:%d:%d:%d:%d\n",
				vir_keypad->keymap[i * 5 + 0],
				vir_keypad->keymap[i * 5 + 1],
				vir_keypad->keymap[i * 5 + 2],
				vir_keypad->keymap[i * 5 + 3],
				vir_keypad->keymap[i * 5 + 4]);
		buf += cnt;
		count += cnt;
	}

	return count;

}

static int ft5x46_get_lockdown_info(struct ft5x46_data *ft5x46)
{
	int error = 0;
	int i, j;
	u8 val1, val2;
	struct ft5x46_rd_flash_packet packet;

	for (i = 0, error = -1; i < sizeof(reset_delay) && error; i++) {
		/* step 1a: reset device */
		error = ft5x46_write_byte(ft5x46, FT5X0X_REG_RESET, 0xaa);
		if (error)
			continue;
		msleep(50);

		error = ft5x46_write_byte(ft5x46, FT5X0X_REG_RESET, 0x55);
		if (error)
			continue;
		msleep(200);
		dev_info(ft5x46->dev, "[FTS] step1: Reset device.\n");

		/* step1b: switch to std i2c */
		error = ft5x46_hid_to_std_i2c(ft5x46);
		if (error) {
			dev_err(ft5x46->dev, "HidI2c change to StdI2c fail!\n");
		}

		/* step 2: enter upgrade mode */
		for (j = 0; j < 10; j++) {
			error = ft5x46_send_byte(ft5x46, 2, 0x55, 0xaa);
			msleep(5);
			if (!error)
				break;
		}
		if (error)
			continue;

		dev_info(ft5x46->dev, "[FTS] step2: Enter upgrade mode.\n");

		/* step 4: check device id */
		error = ft5x46_send_byte(ft5x46, 4, 0x90, 0x00, 0x00, 0x00);
		if (error) {
			dev_err(ft5x46->dev, "Failed to send 90 00 00 00\n");
			continue;
		}

		error = ft5x46_recv_byte(ft5x46, 2, &val1, &val2);
		if (error) {
			dev_err(ft5x46->dev, "Failed to receive device id\n");
			continue;
		}

		dev_info(ft5x46->dev, "read id = 0x%x, 0x%x\n", val1, val2);

		dev_info(ft5x46->dev, "[FTS] step3: Check device id.\n");

		if (val1 == FT5316_UPGRADE_ID_1 && val2 == FT5316_UPGRADE_ID_2) {
			ft5x46->chip_id = FT5316_CHIP_ID;
			break;
		} else if (val1 == FT5X36_UPGRADE_ID_1 && val2 == FT5X36_UPGRADE_ID_2) {
			ft5x46->chip_id = FT5X36_CHIP_ID;
			break;
		} else if (val1 == FT5X46_UPGRADE_ID_1 && val2 == FT5X46_UPGRADE_ID_2) {
			ft5x46->chip_id = FT5X46_CHIP_ID;
			break;
		} else if (val1 == FT8716_UPGRADE_ID_1 && val2 == FT8716_UPGRADE_ID_2) {
			ft5x46->chip_id = FT8716_CHIP_ID;
			break;
		} else if (val1 == FTE716_UPGRADE_ID_1 && val2 == FTE716_UPGRADE_ID_2) {
			ft5x46->chip_id = FTE716_CHIP_ID;
			break;
		}
	}

	if (error)
		return error;

	packet.magic = 0x03;
	packet.addr_h = 0x00;
	packet.addr_m = 0xD7;
	packet.addr_l = 0xA0;

	error = ft5x46_send_block(ft5x46, &packet, FT5X46_RD_FLASH_PACKET_SIZE);
	msleep(5);
	error = ft5x46_recv_byte(ft5x46, 8,
		&ft5x46->lockdown_info[0], &ft5x46->lockdown_info[1],
		&ft5x46->lockdown_info[2], &ft5x46->lockdown_info[3],
		&ft5x46->lockdown_info[4], &ft5x46->lockdown_info[5],
		&ft5x46->lockdown_info[6], &ft5x46->lockdown_info[7]);

	for (i = 0; i < 8; i++)
		dev_info(ft5x46->dev, "Lockdown[%d] val = 0x%x\n", i, ft5x46->lockdown_info[i]);

	error = ft5x46_send_byte(ft5x46, 1, 0x07);
	if (error)
		return error;
	msleep(200);

	return error;
}

static ssize_t ft5x46_object_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	static struct {
		u8 addr;
		const char *fmt;
	} reg_list[] = {
		/* threshold setting */
		{0x80, "THGROUP          %3d\n"  },
		{0x81, "THPEAK           %3d\n"  },
		{0x82, "THCAL            %3d\n"  },
		{0x83, "THWATER          %3d\n"  },
		{0x84, "THTEMP           %3d\n"  },
		{0x85, "THDIFF           %3d\n"  },
		{0xae, "THBAREA          %3d\n"  },
		/* mode setting */
		{0x86, "CTRL              %02x\n"},
		{0xa0, "AUTOCLB           %02x\n"},
		{0xa4, "MODE              %02x\n"},
		{0xa5, "PMODE             %02x\n"},
		{0xa7, "STATE             %02x\n"},
		{0xa9, "ERR               %02x\n"},
		/* timer setting */
		{0x87, "TIME2MONITOR     %3d\n"  },
		{0x88, "PERIODACTIVE     %3d\n"  },
		{0x89, "PERIODMONITOR    %3d\n"  },
		/* version info */
		{0xa1, "LIBVERH           %02x\n"},
		{0xa2, "LIBVERL           %02x\n"},
		{0xa3, "CIPHER            %02x\n"},
		{0xa6, "FIRMID            %02x\n"},
		{0xa8, "FT5201ID          %02x\n"},
		{/* end of the list */},
	};

	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);
	int i, error, count = 0;
	u8 val;

	mutex_lock(&ft5x46->mutex);
	for (i = 0; reg_list[i].addr != 0; i++) {
		error = ft5x46_read_byte(ft5x46, reg_list[i].addr, &val);
		if (error)
			break;

		count += snprintf(buf+count, PAGE_SIZE-count,
				reg_list[i].fmt, val);
	}
	mutex_unlock(&ft5x46->mutex);

	return error ? : count;
}

static ssize_t ft5x46_object_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);
	u8 addr, val;
	int error = 0;

	mutex_lock(&ft5x46->mutex);
	if (sscanf(buf, "%hhx=%hhx", &addr, &val) == 2)
		error = ft5x46_write_byte(ft5x46, addr, val);
	else
		error = -EINVAL;
	mutex_unlock(&ft5x46->mutex);

	return error ? : count;
}

static ssize_t ft5x46_dbgdump_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);
	int count;

	mutex_lock(&ft5x46->mutex);
	count = sprintf(buf, "%d\n", ft5x46->dbgdump);
	mutex_unlock(&ft5x46->mutex);

	return count;
}

static ssize_t ft5x46_dbgdump_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);
	unsigned long dbgdump;
	int error = 0;

	mutex_lock(&ft5x46->mutex);
	error = kstrtoul(buf, 0, &dbgdump);
	if (!error)
		ft5x46->dbgdump = dbgdump;
	mutex_unlock(&ft5x46->mutex);

	return error ? : count;
}

static int ft5x46_updatefw_with_filename(struct ft5x46_data *ft5x46,
		const char *filename, bool *upgraded)
{
	struct ft5x46_firmware_data firmware;
	const struct firmware *fw;
	int error;
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;

	error = request_firmware(&fw, filename, ft5x46->dev);
	if (!error) {
		firmware.data = kmalloc((int)fw->size, GFP_KERNEL);
		memcpy(firmware.data, fw->data, (int)fw->size);
		firmware.size = fw->size;

		mutex_lock(&ft5x46->mutex);
		if (pdata->ui.upgrade_id_1 == FT8716_UPGRADE_ID_1
			|| pdata->ui.upgrade_id_1 == FTE716_UPGRADE_ID_1)
			error = ft8716_load_firmware(ft5x46, &firmware, upgraded);
		else
			error = ft5x46_load_firmware(ft5x46, &firmware, upgraded);
		mutex_unlock(&ft5x46->mutex);

		kfree(firmware.data);
		release_firmware(fw);
	}

	return error;
}

static ssize_t ft5x46_updatefw_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);
	int error = 0;
	bool upgraded;

	error = ft5x46_updatefw_with_filename(ft5x46,
			"ft5x06.bin", &upgraded);

	return error ? : count;
}

static ssize_t ft5x46_tpfwver_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);
	ssize_t num_read_chars = 0;
	u8 fwver = 0;
	int error = 0;
	mutex_lock(&ft5x46->mutex);
	error = ft5x46_read_byte(ft5x46, FT5X0X_REG_FW_VER, &fwver);
	if (error)
		num_read_chars = snprintf(buf, PAGE_SIZE, "Get firmware version failed!\n");
	else
		num_read_chars = snprintf(buf, PAGE_SIZE, "%02X\n", fwver);
	mutex_unlock(&ft5x46->mutex);
	return num_read_chars;
}

static int ft5x46_enter_factory(struct ft5x46_data *ft5x46_ts)
{
	u8 reg_val;
	int error;

	cancel_delayed_work_sync(&ft5x46_ts->lcd_esdcheck_work);

	error = ft5x46_write_byte(ft5x46_ts, FT5X0X_REG_DEVIDE_MODE,
							FT5X0X_DEVICE_MODE_TEST);
	if (error)
		return -EPERM;
	msleep(100);
	error = ft5x46_read_byte(ft5x46_ts, FT5X0X_REG_DEVIDE_MODE, &reg_val);
	if (error)
		return -EPERM;
	if ((reg_val & 0x70) != FT5X0X_DEVICE_MODE_TEST) {
		dev_info(ft5x46_ts->dev, "ERROR: The Touch Panel was not put in Factory Mode.");
		return -EPERM;
	}

	return 0;
}

static int ft5x46_enter_work(struct ft5x46_data *ft5x46_ts)
{
	u8 reg_val = 0;
	int error = 0;
	error = ft5x46_write_byte(ft5x46_ts, FT5X0X_REG_DEVIDE_MODE,
							FT5X0X_DEVICE_MODE_NORMAL);
	if (error)
		return -EPERM;
	msleep(100);
	error = ft5x46_read_byte(ft5x46_ts, FT5X0X_REG_DEVIDE_MODE, &reg_val);
	if (error)
		return -EPERM;
	if ((reg_val & 0x70) != FT5X0X_DEVICE_MODE_NORMAL) {
		dev_info(ft5x46_ts->dev, "ERROR: The Touch Panel was not put in Normal Mode.\n");
		return -EPERM;
	}

	schedule_delayed_work(&ft5x46_ts->lcd_esdcheck_work,
				msecs_to_jiffies(FT5X46_ESD_CHECK_PERIOD));

	return 0;
}

#define FT5X0X_MAX_RX_NUM   	22
static int ft5x46_get_rawData(struct ft5x46_data *ft5x46_ts,
					 u16 *rawdata)
{
	int ret_val = 0;
	int error = 0;
	u8 val;
	int row_num = 0;
	u8 read_buffer[FT5X0X_MAX_RX_NUM * 2];
	int read_len;
	int i;
	struct ft5x46_ts_platform_data *pdata = ft5x46_ts->dev->platform_data;
	int index = ft5x46_ts->current_index;
	int tx_num = pdata->testdata[index].tx_num;
	int rx_num = pdata->testdata[index].rx_num;

	error = ft5x46_read_byte(ft5x46_ts, FT5X0X_REG_DEVIDE_MODE, &val);
	if (error < 0) {
		dev_err(ft5x46_ts->dev, "ERROR: Read mode failed!\n");
		ret_val = -1;
		goto error_return;
	}
	val |= 0x80;
	error = ft5x46_write_byte(ft5x46_ts, FT5X0X_REG_DEVIDE_MODE, val);
	if (error < 0) {
		dev_err(ft5x46_ts->dev, "ERROR: Write mode failed!\n");
		ret_val = -1;
		goto error_return;
	}
	msleep(20);
	error = ft5x46_read_byte(ft5x46_ts, FT5X0X_REG_DEVIDE_MODE, &val);
	if (error < 0) {
		dev_err(ft5x46_ts->dev, "ERROR: Read mode failed!\n");
		ret_val = -1;
		goto error_return;
	}
	if (0x00 != (val & 0x80)) {
		dev_err(ft5x46_ts->dev, "ERROR: Read mode failed!\n");
		ret_val = -1;
		goto error_return;
	}
	dev_info(ft5x46_ts->dev, "Read rawdata......\n");
	for (row_num = 0; row_num < tx_num; row_num++) {
		memset(read_buffer, 0x00, rx_num * 2);
		error = ft5x46_write_byte(ft5x46_ts, FT5X0X_REG_ROW_ADDR, row_num);
		if (error < 0) {
			dev_err(ft5x46_ts->dev, "ERROR: Write row addr failed!\n");
			ret_val = -1;
			goto error_return;
		}
		msleep(1);
		read_len = rx_num * 2;
		error = ft5x46_write_byte(ft5x46_ts, 0x10, read_len);
		if (error < 0) {
			dev_err(ft5x46_ts->dev, "ERROR: Write len failed!\n");
			ret_val = -1;
			goto error_return;
		}
		error = ft5x46_read_block(ft5x46_ts, 0x10,
							read_buffer, rx_num * 2);
		if (error < 0) {
			dev_err(ft5x46_ts->dev,
				"ERROR: Coule not read row %u data!\n", row_num);
			ret_val = -1;
			goto error_return;
		}
		for (i = 0; i < rx_num; i++) {
			rawdata[row_num * rx_num + i] = read_buffer[i<<1];
			rawdata[row_num * rx_num + i] = rawdata[row_num * rx_num + i] << 8;
			rawdata[row_num * rx_num + i] |= read_buffer[(i<<1)+1];
		}
	}
error_return:
	return ret_val;
}

static ssize_t ft5x46_rawdata_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;
	int index = ft5x46->current_index;
	u16 *rawdata;
	int error = 0;
	int i = 0, j = 0;
	int num_read_chars = 0;
	int tx_num = pdata->testdata[index].tx_num;
	int rx_num = pdata->testdata[index].rx_num;

	rawdata = (u16*)kmalloc(sizeof(u16) * tx_num * rx_num, GFP_KERNEL);
	if (rawdata == NULL)
		return -ENOMEM;

	mutex_lock(&ft5x46->mutex);

	disable_irq_nosync(ft5x46->irq);
	error = ft5x46_enter_factory(ft5x46);
	if (error < 0) {
		dev_err(ft5x46->dev, "ERROR: Could not enter factory mode!\n");
		goto end;
	}

	error = ft5x46_get_rawData(ft5x46, rawdata);
	if (error < 0)
		sprintf(buf, "%s", "Could not get rawdata\n");
	else {
		for (i = 0; i < tx_num; i++) {
			for (j = 0; j < rx_num; j++) {
				num_read_chars += sprintf(&buf[num_read_chars],
								"%u ", rawdata[i * rx_num + j]);
			}
			buf[num_read_chars-1] = '\n';
		}
	}

	error = ft5x46_enter_work(ft5x46);
	if (error < 0)
		dev_err(ft5x46->dev, "ERROR: Could not enter work mode!\n");

end:
	enable_irq(ft5x46->irq);
	mutex_unlock(&ft5x46->mutex);
	kfree(rawdata);
	return num_read_chars;
}

unsigned int ft5x46_do_selftest(struct ft5x46_data *ft5x46)
{
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;
	int index = ft5x46->current_index;
	u16 *testdata;
	int i, j;
	int error = 0;
	int tx_num = pdata->testdata[index].tx_num;
	int rx_num = pdata->testdata[index].rx_num;
	int final_tx_num = tx_num;
	int final_rx_num = rx_num;

	testdata = (u16*)kmalloc(sizeof(u16) * tx_num * rx_num, GFP_KERNEL);
	if (testdata == NULL)
		return -ENOMEM;

	/* 1. test raw data */
	error = ft5x46_get_rawData(ft5x46, testdata);
	if (error)
		return 0;

	if (tx_num > rx_num)
		final_tx_num -= 1;
	else
		final_rx_num -= 1;

	for (i = 0; i < final_tx_num; i++) {
		for (j = 0; j < final_rx_num; j++) {
			if (testdata[i * rx_num + j] < pdata->raw_min ||
				testdata[i * rx_num +j] > pdata->raw_max)
				return 0;
		}
	}

	return 1;
}

static ssize_t ft5x46_selftest_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);
	int error = 0;
	unsigned long val;

	error = kstrtoul(buf, 0, &val);
	if (error)
		return error;
	if (val != 1)
		return -EINVAL;

	mutex_lock(&ft5x46->mutex);

	disable_irq_nosync(ft5x46->irq);
	error = ft5x46_enter_factory(ft5x46);
	if (error < 0) {
		dev_err(ft5x46->dev, "ERROR: Could not enter factory mode!\n");
		goto end;
	}

	ft5x46->test_result = ft5x46_do_selftest(ft5x46);

	error = ft5x46_enter_work(ft5x46);
	if (error < 0)
		dev_err(ft5x46->dev, "ERROR: Could not enter work mode!\n");

end:
	enable_irq(ft5x46->irq);
	mutex_unlock(&ft5x46->mutex);
	return count;
}

static ssize_t ft5x46_selftest_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);

	return sprintf(&buf[0], "%u\n", ft5x46->test_result);
}

static ssize_t ft5x46_lockdown_info_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);
	int error = 0;
	unsigned long val;
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;

	error = kstrtoul(buf, 0, &val);
	if (error)
		return error;
	if (val != 1)
		return -EINVAL;

	if (pdata->ui.upgrade_id_1 == FT8716_UPGRADE_ID_1
		|| pdata->ui.upgrade_id_1 == FTE716_UPGRADE_ID_1) {
		/* Force to get lockdown info */
		error = ft8716_get_lockdown_info(ft5x46);
		if (!error)
			ft5x46->lockdown_info_acquired = true;
	} else
		error = ft5x46_get_lockdown_info(ft5x46);
	if (error)
		return -EINVAL;
	else
		return count;
}

static ssize_t ft5x46_lockdown_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);
	int error = 0;
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;

	if (pdata->ui.upgrade_id_1 == FT8716_UPGRADE_ID_1
		|| pdata->ui.upgrade_id_1 == FTE716_UPGRADE_ID_1) {
		if (wait_event_timeout(ft5x46->lockdown_info_acquired_wq,
				ft5x46->lockdown_info_acquired, 5 * HZ) == 0) {
			error = ft8716_get_lockdown_info(ft5x46);
			if (!error)
				ft5x46->lockdown_info_acquired = true;
		} else
			pr_info("panel lockdown info has been acquired\n");
	} else
		error = ft5x46_get_lockdown_info(ft5x46);
	if (error)
		return sprintf(buf, "Failed to get lockdown info\n");
	else
		return sprintf(buf, "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
			(int)ft5x46->lockdown_info[0], (int)ft5x46->lockdown_info[1],
			(int)ft5x46->lockdown_info[2], (int)ft5x46->lockdown_info[3],
			(int)ft5x46->lockdown_info[4], (int)ft5x46->lockdown_info[5],
			(int)ft5x46->lockdown_info[6], (int)ft5x46->lockdown_info[7]);
}

static ssize_t ft5x46_config_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);

	return sprintf(buf, "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
		(int)ft5x46->config_info[0], (int)ft5x46->config_info[1],
		(int)ft5x46->config_info[2], (int)ft5x46->config_info[3],
		(int)ft5x46->config_info[4], (int)ft5x46->config_info[5],
		(int)ft5x46->config_info[6], (int)ft5x46->config_info[7]);
}

static ssize_t ft5x46_wakeup_mode_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int error = 0;
	unsigned long val;
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);

	error = kstrtoul(buf, 0, &val);

	if (!error)
		ft5x46->wakeup_mode = !!val;

	if (ft5x46->in_suspend)
		ft5x46_wakeup_reconfigure(ft5x46, (bool)val);

	return error ? : count;
}

static ssize_t ft5x46_wakeup_mode_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", (int)ft5x46->wakeup_mode);
}

static ssize_t ft5x46_panel_color_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);
	int error = 0, count = 0;
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;

	if (ft5x46->in_suspend)
		ft5x46_resume(ft5x46);

	if (pdata->ui.upgrade_id_1 == FT8716_UPGRADE_ID_1
		|| pdata->ui.upgrade_id_1 == FTE716_UPGRADE_ID_1) {
		if (wait_event_timeout(ft5x46->lockdown_info_acquired_wq,
				ft5x46->lockdown_info_acquired, 5 * HZ) == 0) {
			error = ft8716_get_lockdown_info(ft5x46);
			if (!error)
				ft5x46->lockdown_info_acquired = true;
		} else
			pr_info("panel lockdown info has been acquired\n");
	} else
		error = ft5x46_get_lockdown_info(ft5x46);
	if (error || ft5x46->lockdown_info[2] == 0)
		count = snprintf(buf, PAGE_SIZE, "0\n");
	else
		count = snprintf(buf, PAGE_SIZE, "%c\n", ft5x46->lockdown_info[2]);

	return count;
}

#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
static ssize_t ft5x46_prox_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int error = 0;
	unsigned long val;
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);

	error = kstrtoul(buf, 0, &val);

	if (error) {
		pr_err("Invalid data\n");
		return -EINVAL;
	}

	ft5x46->proximity_enable_in_suspend = false;
	cancel_delayed_work_sync(&ft5x46->prox_enable_delayed_work);
	if (ft5x46->in_suspend) {
		if (wait_event_timeout(ft5x46->resume_wq, !(ft5x46->in_suspend), HZ/2) == 0) {
			pr_info("Proximity: device is in suspend\n");
			ft5x46->proximity_enable_in_suspend = !!val;
			return error ? : count;
		} else /* Wait for TP calibrating finished */
			msleep(300);
	}

	mutex_lock(&ft5x46->mutex);
	ft5x46->proximity_enable = !!val;
	error = ft5x46_proximity_enable(ft5x46, ft5x46->proximity_enable);
	if (!ft5x46->proximity_enable)
		ft5x46->proximity_near = 0;
	input_report_abs(ft5x46->proximity, ABS_DISTANCE, ft5x46->proximity_near);
	input_sync(ft5x46->proximity);
	mutex_unlock(&ft5x46->mutex);
	pr_info("ft5x46 proximity %s\n", ft5x46->proximity_enable?"enabled":"disabled");

	return error ? : count;
}

static ssize_t ft5x46_prox_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", (int)ft5x46->proximity_enable);
}

static ssize_t ft5x46_prox_rawdata_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", (int)ft5x46->proximity_near);
}
#endif

static ssize_t ft5x46_irq_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int error;
	unsigned long val;
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);

	error = kstrtoul(buf, 0, &val);

	if (error) {
		pr_err("Invalid data\n");
		return -EINVAL;
	}
	if (val)
		enable_irq(ft5x46->irq);
	else
		disable_irq(ft5x46->irq);

	return error ? : count;
}

static ssize_t ft5x46_irq_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", (int)ft5x46->irq_enabled);
}

static ssize_t ft5x46_panel_vendor_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);
	int count = 0;

	count = snprintf(buf, PAGE_SIZE, "%c\n", ft5x46->lockdown_info[0]);
	return count;
}

static ssize_t ft5x46_lcd_esd_test(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int error;
	unsigned long val;

	error = kstrtoul(buf, 0, &val);

	if (error) {
		pr_err("Invalid data\n");
		return -EINVAL;
	}
	if (val)
		lcd_need_reset = true;
	else
		lcd_need_reset = false;

	return error ? : count;
}

/* sysfs */
#ifdef FT5X46_DEBUG_PERMISSION
static DEVICE_ATTR(tpfwver, 0666, ft5x46_tpfwver_show, NULL);
static DEVICE_ATTR(object, 0666, ft5x46_object_show, ft5x46_object_store);
static DEVICE_ATTR(dbgdump, 0666, ft5x46_dbgdump_show, ft5x46_dbgdump_store);
static DEVICE_ATTR(updatefw, 0222, NULL, ft5x46_updatefw_store);
static DEVICE_ATTR(rawdatashow, 0666, ft5x46_rawdata_show, NULL);
static DEVICE_ATTR(selftest, 0666, ft5x46_selftest_show, ft5x46_selftest_store);
static DEVICE_ATTR(lockdown_info, 0666, ft5x46_lockdown_info_show, ft5x46_lockdown_info_store);
static DEVICE_ATTR(config_info, 0666, ft5x46_config_info_show, NULL);
static DEVICE_ATTR(wakeup_mode, 0666, ft5x46_wakeup_mode_show, ft5x46_wakeup_mode_store);
static DEVICE_ATTR(panel_color, 0666, ft5x46_panel_color_show, NULL);
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
static DEVICE_ATTR(prox_enable, 0666, ft5x46_prox_enable_show, ft5x46_prox_enable_store);
static DEVICE_ATTR(prox_rawdata, 0666, ft5x46_prox_rawdata_show, NULL);
#endif
static DEVICE_ATTR(irq_enable, 0644, ft5x46_irq_enable_show, ft5x46_irq_enable_store);
static DEVICE_ATTR(panel_vendor, 0666, ft5x46_panel_vendor_show, NULL);
#else
static DEVICE_ATTR(tpfwver, 0644, ft5x46_tpfwver_show, NULL);
static DEVICE_ATTR(object, 0644, ft5x46_object_show, ft5x46_object_store);
static DEVICE_ATTR(dbgdump, 0644, ft5x46_dbgdump_show, ft5x46_dbgdump_store);
static DEVICE_ATTR(updatefw, 0200, NULL, ft5x46_updatefw_store);
static DEVICE_ATTR(rawdatashow, 0644, ft5x46_rawdata_show, NULL);
static DEVICE_ATTR(selftest, 0644, ft5x46_selftest_show, ft5x46_selftest_store);
static DEVICE_ATTR(lockdown_info, 0644, ft5x46_lockdown_info_show, ft5x46_lockdown_info_store);
static DEVICE_ATTR(config_info, 0644, ft5x46_config_info_show, NULL);
static DEVICE_ATTR(wakeup_mode, 0644, ft5x46_wakeup_mode_show, ft5x46_wakeup_mode_store);
static DEVICE_ATTR(panel_color, 0444, ft5x46_panel_color_show, NULL);
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
static DEVICE_ATTR(prox_enable, 0644, ft5x46_prox_enable_show, ft5x46_prox_enable_store);
static DEVICE_ATTR(prox_rawdata, 0444, ft5x46_prox_rawdata_show, NULL);
#endif
static DEVICE_ATTR(irq_enable, 0644, ft5x46_irq_enable_show, ft5x46_irq_enable_store);
static DEVICE_ATTR(panel_vendor, 0644, ft5x46_panel_vendor_show, NULL);
#endif
static DEVICE_ATTR(esd_test, 0644, NULL, ft5x46_lcd_esd_test);

static struct attribute *ft5x46_attrs[] = {
	&dev_attr_tpfwver.attr,
	&dev_attr_object.attr,
	&dev_attr_dbgdump.attr,
	&dev_attr_updatefw.attr,
	&dev_attr_rawdatashow.attr,
	&dev_attr_selftest.attr,
	&dev_attr_lockdown_info.attr,
	&dev_attr_config_info.attr,
	&dev_attr_wakeup_mode.attr,
	&dev_attr_panel_color.attr,
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	&dev_attr_prox_enable.attr,
	&dev_attr_prox_rawdata.attr,
#endif
	&dev_attr_irq_enable.attr,
	&dev_attr_panel_vendor.attr,
	&dev_attr_esd_test.attr,
	NULL
};

static const struct attribute_group ft5x46_attr_group = {
	.attrs = ft5x46_attrs
};

static int ft5x46_panel_power(struct ft5x46_data *data, bool on)
{
	static bool status;
	int rc = 0;

	if (on == status) {
		pr_info("Regulator is already %s\n", on?"on":"off");
		return 0;
	}

	if (on) {
		rc = regulator_enable(data->lab);
		if (rc) {
			dev_err(data->dev,
				"Regulator lab enable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_enable(data->ibb);
		if (rc) {
			dev_err(data->dev,
				"Regulator ibb enable failed rc=%d\n", rc);
			return rc;
		}
	} else {
		rc = regulator_disable(data->ibb);
		if (rc) {
			dev_err(data->dev,
				"Regulator ibb disable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_disable(data->lab);
		if (rc) {
			dev_err(data->dev,
				"Regulator lab disable failed rc=%d\n", rc);
			return rc;
		}
	}
	status = on;

	return rc;
}

static int ft5x46_panel_vddio_power(struct ft5x46_data *data, bool on)
{
	static bool status;
	int rc = 0;

	if (on == status) {
		pr_info("Regulator is already %s\n", on?"on":"off");
		return 0;
	}

	if (on) {
		rc = regulator_enable(data->vdd);
		if (rc) {
			dev_err(data->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_enable(data->vddio);
		if (rc) {
			dev_err(data->dev,
				"Regulator vddio enable failed rc=%d\n", rc);
			return rc;
		}
	} else {
		rc = regulator_disable(data->vddio);
		if (rc) {
			dev_err(data->dev,
				"Regulator vddio disable failed rc=%d\n", rc);
			return rc;
		}

		rc = regulator_disable(data->vdd);
		if (rc) {
			dev_err(data->dev,
				"Regulator vdd disable failed rc=%d\n", rc);
			return rc;
		}
	}
	status = on;

	return rc;
}


static int ft5x46_power_on(struct ft5x46_data *data, bool on)
{
	static bool status;
	int rc = 0;

	if (on == status) {
		pr_info("Regulator is already %s\n", on?"on":"off");
		return 0;
	}

	if (on) {
		rc = ft5x46_panel_vddio_power(data, on);
		if (rc)
			return rc;

		rc = ft5x46_panel_power(data, on);
		if (rc)
			return rc;

	} else {
		rc = ft5x46_panel_power(data, on);
		if (rc)
			return rc;

		rc = ft5x46_panel_vddio_power(data, on);
		if (rc)
			return rc;
	}
	status = on;

	return rc;
}

static int ft5x46_power_init(struct ft5x46_data *data, bool on)
{
	int rc = 0;

	if (!on)
		goto pwr_deinit;

	data->vdd = regulator_get(data->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		rc = PTR_ERR(data->vdd);
		dev_err(data->dev,
			"Regulator get failed vdd rc=%d\n", rc);
		goto exit;
	}

	data->vddio = regulator_get(data->dev, "vddio");
	if (IS_ERR(data->vddio)) {
		rc = PTR_ERR(data->vddio);
		dev_err(data->dev,
			"Regulator get failed vddio rc=%d\n", rc);
		goto err_get_vddio;
	}

	data->lab = regulator_get(data->dev, "lab");
	if (IS_ERR(data->lab)) {
		rc = PTR_ERR(data->lab);
		dev_err(data->dev,
			"Regulator get failed lab rc=%d\n", rc);
		goto err_get_lab;
	}

	data->ibb = regulator_get(data->dev, "ibb");
	if (IS_ERR(data->ibb)) {
		rc = PTR_ERR(data->ibb);
		dev_err(data->dev,
			"Regulator get failed ibb rc=%d\n", rc);
		goto err_get_ibb;
	}

	return 0;

pwr_deinit:
	regulator_put(data->ibb);
err_get_ibb:
	regulator_put(data->lab);
err_get_lab:
	regulator_put(data->vddio);
err_get_vddio:
	regulator_put(data->vdd);
exit:
	return rc;
}

static void ft5x46_dt_dump(struct device *dev,
			struct ft5x46_ts_platform_data *pdata)
{
	int j = 0;

	dev_dbg(dev, "i2c-pull-up = %d\n", (int)pdata->i2c_pull_up);
	dev_dbg(dev, "reset gpio = %d\n", (int)pdata->reset_gpio);
	dev_dbg(dev, "irq gpio = %d\n", (int)pdata->irq_gpio);
	dev_dbg(dev, "x_max = %d\n", (int)pdata->x_max);
	dev_dbg(dev, "y_max = %d\n", (int)pdata->y_max);
	dev_dbg(dev, "z_max = %d\n", (int)pdata->z_max);
	dev_dbg(dev, "w_max = %d\n", (int)pdata->w_max);
	dev_dbg(dev, "landing_jiffies = %d\n", (int)pdata->landing_jiffies);
	dev_dbg(dev, "landing_threshold = %d\n", (int)pdata->landing_threshold);
	dev_dbg(dev, "staying_threshold = %d\n", (int)pdata->staying_threshold);
	dev_dbg(dev, "raw min = %d\n", (int)pdata->raw_min);
	dev_dbg(dev, "raw max = %d\n", (int)pdata->raw_max);
	dev_dbg(dev, "fw delay 55 ms = %d\n", (int)pdata->ui.delay_55);
	dev_dbg(dev, "fw delay aa ms = %d\n", (int)pdata->ui.delay_aa);
	dev_dbg(dev, "fw upgrade id 1 = %d\n", (int)pdata->ui.upgrade_id_1);
	dev_dbg(dev, "fw upgrade id 2 = %d\n", (int)pdata->ui.upgrade_id_2);
	dev_dbg(dev, "fw delay readid ms = %d\n", (int)pdata->ui.delay_readid);

	for (j = 0; j < pdata->cfg_size; j++) {
		dev_dbg(dev, "firmare %d chip = 0x%x, vendor = 0x%x name = %s\n",
				j, pdata->firmware[j].chip,
				pdata->firmware[j].vendor,
				pdata->firmware[j].fwname);
	}
}

static int ft5x46_initialize_pinctrl(struct ft5x46_data *ft5x46)
{
	int ret = 0;

	/* Get pinctrl if target uses pinctrl */
	ft5x46->ts_pinctrl = devm_pinctrl_get(ft5x46->dev);
	if (IS_ERR_OR_NULL(ft5x46->ts_pinctrl)) {
		pr_err("Target does not use pinctrl\n");
		ret = PTR_ERR(ft5x46->ts_pinctrl);
		ft5x46->ts_pinctrl = NULL;
		return ret;
	}

	ft5x46->gpio_state_active
		= pinctrl_lookup_state(ft5x46->ts_pinctrl, "pmx_ts_active");
	if (IS_ERR_OR_NULL(ft5x46->gpio_state_active)) {
		pr_err("Can not get ts default pinstate\n");
		ret = PTR_ERR(ft5x46->gpio_state_active);
		ft5x46->ts_pinctrl = NULL;
		return ret;
	}

	ft5x46->gpio_state_suspend
		= pinctrl_lookup_state(ft5x46->ts_pinctrl, "pmx_ts_suspend");
	if (IS_ERR_OR_NULL(ft5x46->gpio_state_suspend)) {
		pr_err("Can not get ts sleep pinstate\n");
		ret = PTR_ERR(ft5x46->gpio_state_suspend);
		ft5x46->ts_pinctrl = NULL;
		return ret;
	}

	return 0;
}

static int ft5x46_pinctrl_select(struct ft5x46_data *ft5x46, bool on)
{
	int ret = 0;
	struct pinctrl_state *pins_state;

	pins_state = on ? ft5x46->gpio_state_active : ft5x46->gpio_state_suspend;
	if (!IS_ERR_OR_NULL(pins_state)) {
		ret = pinctrl_select_state(ft5x46->ts_pinctrl, pins_state);
		if (ret) {
			dev_err(ft5x46->dev, "Cannot set %s pins\n",
				on ? "pmx_ts_active" : "pmx_ts_suspend");
			return ret;
		}
	} else {
		dev_err(ft5x46->dev, "not a valid '%s' pinstate\n",
			on ? "pmx_ts_active" : "pmx_ts_suspend");
	}

	return 0;
}

#if defined(CONFIG_DRM)
static int fb_notifier_cb(struct notifier_block *self,
	unsigned long event, void *data)
{
	int *blank;
	int rc = 0;
	struct drm_notify_data *evdata = data;
	struct ft5x46_data *ft5x46 =
		container_of(self, struct ft5x46_data, drm_notifier);

	/* Receive notifications from primary panel only */
	if (evdata && evdata->data && ft5x46 && evdata->is_primary) {
		if (event == DRM_EVENT_BLANK) {
			blank = evdata->data;
			if (*blank == DRM_BLANK_UNBLANK) {
				dev_dbg(ft5x46->dev, "##### UNBLANK SCREEN #####\n");
				ft5x46_input_enable(ft5x46->input);
				if (!ft5x46->wakeup_mode)
					rc = ft5x46_panel_power(ft5x46, true);
				drm_dsi_ulps_enable(false);
			} else if (*blank == DRM_BLANK_POWERDOWN) {
				dev_dbg(ft5x46->dev, "##### BLANK SCREEN #####\n");
				ft5x46_input_disable(ft5x46->input);
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
				if (!(ft5x46->wakeup_mode || ft5x46->proximity_enable))
#else
				if (!ft5x46->wakeup_mode)
#endif
					rc = ft5x46_panel_power(ft5x46, false);
			}
		} else if (event == DRM_EARLY_EVENT_BLANK) {
			blank = evdata->data;
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
			if (*blank == DRM_BLANK_POWERDOWN &&
				(ft5x46->wakeup_mode || ft5x46->proximity_enable)) {
#else
			if (*blank == DRM_BLANK_POWERDOWN && ft5x46->wakeup_mode) {
#endif
				pr_debug("Enable suspend ulps\n");
				drm_dsi_ulps_enable(true);
			}
		}
	}

	return rc;
}

static int ft5x46_configure_sleep(struct ft5x46_data *ft5x46, bool enable)
{
	int ret = 0;

	ft5x46->drm_notifier.notifier_call = fb_notifier_cb;
	if (enable) {
		ret = drm_register_client(&ft5x46->drm_notifier);
		if (ret) {
			dev_err(ft5x46->dev,
				"Unable to register fb_notifier, err: %d\n", ret);
		}
	} else {
		ret = drm_unregister_client(&ft5x46->drm_notifier);
		if (ret) {
			dev_err(ft5x46->dev,
				"Unable to unregister fb_notifier, err: %d\n", ret);
		}
	}
	return ret;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static int ft5x46_configure_sleep(struct ft5x46_data *ft5x46, bool enable)
{
	if (enable) {
		ft5x46->early_suspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN+1;
		ft5x46->early_suspend.suspend = ft5x46_early_suspend;
		ft5x46->early_suspend.resume  = ft5x46_early_resume;
		register_early_suspend(&ft5x46->early_suspend);
	} else {
		unregister_early_suspend(&ft5x46->early_suspend);
	}

	return 0;
}
#else
static int ft5x46_configure_sleep(struct ft5x46_data *ft5x46, bool enable)
{
	if (enable) {
		ft5x46->input->enable = ft5x46_input_enable;
		ft5x46->input->disable = ft5x46_input_disable;
		ft5x46->input->enabled = true;
	} else {
		ft5x46->input->enable = NULL;
		ft5x46->input->disable = NULL;
		ft5x46->input->enabled = false;
	}
	return 0;
}
#endif

#ifdef CONFIG_OF
static int ft5x46_parse_dt(struct device *dev,
			struct ft5x46_ts_platform_data *pdata)
{
	int rc = 0, j = 0;
	struct device_node *np = dev->of_node;
	u32 temp_val, num_fw;
	struct device_node *sub_np;
	struct property *prop;

	pdata->i2c_pull_up = of_property_read_bool(np,
						"ft5x46_i2c,i2c-pull-up");
	/* Won't do reset in resume. Reset action would be done in DDIC internal.
	* Otherwise, there would be 2 reset action when panel is lighted on.
	* In this case, it will take a long time enter active state for touch.
	* Moreover, no response issue would be met in the early period of screen on.
	* Usually, this option would be enabled for TDDI.
	*/
	pdata->skip_reset_in_resume = of_property_read_bool(np,
						"ft5x46_i2c,skip-reset-in-resume");

	prop = of_find_property(np, "ft5x46_i2c,vir-keypad-codes", NULL);
	if (prop && prop->length) {
		vir_keypad->keymap = kzalloc(prop->length, GFP_KERNEL);
		if (!vir_keypad->keymap)
			return -ENOMEM;
		vir_keypad->length = prop->length / sizeof(u32);
		vir_keypad->length /= 5;
		rc = of_property_read_u32_array(np,
				"ft5x46_i2c,vir-keypad-codes",
				vir_keypad->keymap,
				vir_keypad->length * 5);
		if (rc < 0) {
			vir_keypad->length = 0;
			kfree(vir_keypad->keymap);
			vir_keypad->keymap = NULL;
		}
	} else {
		vir_keypad->length = 0;
		vir_keypad->keymap = NULL;
	}

	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "ft5x46_i2c,reset-gpio",
				0, &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0)
		return pdata->reset_gpio;

	pdata->irq_gpio = of_get_named_gpio_flags(np, "ft5x46_i2c,irq-gpio",
				0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0)
		return pdata->irq_gpio;

	rc = of_property_read_u32(np, "ft5x46_i2c,x-max", &pdata->x_max);
	if (rc) {
		dev_err(dev, "can't read x-max\n");
		return rc;
	}
	rc = of_property_read_u32(np, "ft5x46_i2c,y-max", &pdata->y_max);
	if (rc) {
		dev_err(dev, "can't read y-max\n");
		return rc;
	}
	rc = of_property_read_u32(np, "ft5x46_i2c,z-max", &pdata->z_max);
	if (rc) {
		dev_err(dev, "can't read z-max\n");
		return rc;
	}
	rc = of_property_read_u32(np, "ft5x46_i2c,w-max", &pdata->w_max);
	if (rc) {
		dev_err(dev, "can't read w-max\n");
		return rc;
	}

	rc = of_property_read_u32(np, "ft5x46_i2c,landing-jiffies", &temp_val);
	if (rc) {
		dev_err(dev, "can't read landing-jiffies\n");
		return rc;
	} else
		pdata->landing_jiffies = (unsigned long)temp_val;
	rc = of_property_read_u32(np, "ft5x46_i2c,landing-threshold", &pdata->landing_threshold);
	if (rc) {
		dev_err(dev, "can't read landing-threshold\n");
		return rc;
	}
	rc = of_property_read_u32(np, "ft5x46_i2c,staying-threshold", &pdata->staying_threshold);
	if (rc) {
		dev_err(dev, "can't read staying-threshold\n");
		return rc;
	}

	rc = of_property_read_u32(np, "ft5x46_i2c,raw-min", &temp_val);
	if (rc) {
		dev_err(dev, "can't read raw-min\n");
		return rc;
	} else
		pdata->raw_min = (u16)temp_val;
	rc = of_property_read_u32(np, "ft5x46_i2c,raw-max", &temp_val);
	if (rc) {
		dev_err(dev, "can't read raw-max\n");
		return rc;
	} else
		pdata->raw_max = (u16)temp_val;
	rc = of_property_read_u32(np, "ft5x46_i2c,open-min", &temp_val);
	if (rc)
		dev_err(dev, "can't read open-min\n");
	else
		pdata->open_min = (u16)temp_val;
	rc = of_property_read_u32(np, "ft5x46_i2c,open-max", &temp_val);
	if (rc)
		dev_err(dev, "can't read open-max\n");
	else
		pdata->open_max = (u16)temp_val;
	rc = of_property_read_u32(np, "ft5x46_i2c,short-min", &temp_val);
	if (rc)
		dev_err(dev, "can't read short-min\n");
	else
		pdata->short_min = (u16)temp_val;
	rc = of_property_read_u32(np, "ft5x46_i2c,short-max", &temp_val);
	if (rc)
		dev_err(dev, "can't read short-max\n");
	else
		pdata->short_max = (u32)temp_val;
	rc = of_property_read_u32(np, "ft5x46_i2c,imin-cc", &temp_val);
	if (rc)
		dev_err(dev, "can't read imin-cc\n");
	else
		pdata->imin_cc = (u32)temp_val;
	rc = of_property_read_u32(np, "ft5x46_i2c,imin-cg", &temp_val);
	if (rc)
		dev_err(dev, "can't read short-max\n");
	else
		pdata->imin_cg = (u32)temp_val;
	/* Get upgrade info */
	rc = of_property_read_u32(np, "ft5x46_i2c,fw-delay-55-ms", &temp_val);
	if (rc) {
		dev_err(dev, "can't read delay_55\n");
		return rc;
	} else
		pdata->ui.delay_55 = (u16)temp_val;

	rc = of_property_read_u32(np, "ft5x46_i2c,fw-delay-aa-ms", &temp_val);
	if (rc) {
		dev_err(dev, "can't read delay_aa\n");
		return rc;
	} else
		pdata->ui.delay_aa = (u16)temp_val;

	rc = of_property_read_u32(np, "ft5x46_i2c,fw-upgrade-id1", &temp_val);
	if (rc) {
		dev_err(dev, "can't read fw-upgrade-id1\n");
		return rc;
	} else
		pdata->ui.upgrade_id_1 = (u8)temp_val;

	rc = of_property_read_u32(np, "ft5x46_i2c,fw-upgrade-id2", &temp_val);
	if (rc) {
		dev_err(dev, "can't read fw-upgrade-id2\n");
		return rc;
	} else
		pdata->ui.upgrade_id_2 = (u8)temp_val;

	rc = of_property_read_u32(np, "ft5x46_i2c,fw-delay-readid-ms", &temp_val);
	if (rc) {
		dev_err(dev, "can't read fw-delay-readid-ms\n");
		return rc;
	} else
		pdata->ui.delay_readid = (u16)temp_val;

	rc = of_property_read_u32(np, "ft5x46_i2c,firmware-array-size", &num_fw);
	if (rc) {
		dev_err(dev, "can't get firmware-array-size\n");
		return rc;
	}

	pdata->firmware = kzalloc(sizeof(struct ft5x46_firmware_data) * (num_fw + 1),
				GFP_KERNEL);
	if (pdata->firmware == NULL)
		return -ENOMEM;
	pdata->testdata = kzalloc(sizeof(struct ft5x46_test_data) * num_fw, GFP_KERNEL);
	if (pdata->testdata == NULL)
		return -ENOMEM;

	pdata->cfg_size = num_fw;
	j = 0;
	for_each_child_of_node(np, sub_np) {
		rc = of_property_read_u32(sub_np, "ft5x46_i2c,chip", &temp_val);
		if (rc) {
			dev_err(dev, "can't get chip id\n");
			return rc;
		} else
			pdata->firmware[j].chip = (u8)temp_val;

		rc = of_property_read_u32(sub_np, "ft5x46_i2c,vendor", &temp_val);
		if (rc) {
			dev_err(dev, "can't get vendor id\n");
			return rc;
		} else
			pdata->firmware[j].vendor = (u8)temp_val;

		rc = of_property_read_u32(sub_np, "ft5x46_i2c,tp-vendor", &temp_val);
		if (rc) {
			dev_err(dev, "can't get tp_vendor id\n");
			return rc;
		} else
			pdata->firmware[j].tp_vendor = (u8)temp_val;

		rc = of_property_read_u32(sub_np, "ft5x46_i2c,lcd-vendor", &temp_val);
		if (rc) {
			dev_err(dev, "can't get lcd_vendor id\n");
			return rc;
		} else
			pdata->firmware[j].lcd_vendor = (u8)temp_val;

		rc = of_property_read_string(sub_np, "ft5x46_i2c,fw-name",
						&pdata->firmware[j].fwname);
		if (rc && (rc != -EINVAL)) {
			dev_err(dev, "can't read fw-name\n");
			return rc;
		}
		pdata->firmware[j].size = 0;

		rc = of_property_read_u32(sub_np, "ft5x46_i2c,tx-num", &pdata->testdata[j].tx_num);
		if (rc) {
			dev_err(dev, "can't read tx-num\n");
			return rc;
		}
		rc = of_property_read_u32(sub_np, "ft5x46_i2c,rx-num", &pdata->testdata[j].rx_num);
		if (rc) {
			dev_err(dev, "can't read rx-num\n");
			return rc;
		}

		j ++;
	}

	ft5x46_dt_dump(dev, pdata);

	return 0;
}
#else
static int ft5x46_parse_dt(struct device *dev,
			struct ft5x46_ts_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static int ft5x46_read_config_info(struct ft5x46_data *ft5x46)
{
	int error;

	error = ft5x46_read_block(ft5x46, FT5X0X_REG_CONFIG_INFO,
			ft5x46->config_info, FT5X46_CONFIG_INFO_SIZE);
	if (error) {
		dev_err(ft5x46->dev, "Read config info register %d failed\n",
			FT5X0X_REG_CONFIG_INFO);
		return error;
	}

	dev_info(ft5x46->dev, "Config info: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X",
		ft5x46->config_info[0], ft5x46->config_info[1],
		ft5x46->config_info[2], ft5x46->config_info[3],
		ft5x46->config_info[4], ft5x46->config_info[5],
		ft5x46->config_info[6], ft5x46->config_info[7]);

	return 0;
}

static void ft5x46_switch_mode_work(struct work_struct *work)
{
	struct ft5x46_mode_switch *ms = container_of(work, struct ft5x46_mode_switch, switch_mode_work);
	struct ft5x46_data *ft5x46 = ms->data;
	u8 value = ms->mode;
	char ch[16] = {0x0,};

	if (value == FT5X46_INPUT_EVENT_WAKUP_MODE_ON || value == FT5X46_INPUT_EVENT_WAKUP_MODE_OFF) {
		if (ft5x46) {
			ft5x46->wakeup_mode = value - FT5X46_INPUT_EVENT_WAKUP_MODE_OFF;

			if (ft5x46->in_suspend)
				ft5x46_wakeup_reconfigure(ft5x46,
					(bool)(value - FT5X46_INPUT_EVENT_WAKUP_MODE_OFF));
			snprintf(ch, sizeof(ch), "%s", ft5x46->wakeup_mode ? "enabled" : "disabled");
		}
	}

	if (ms != NULL) {
		kfree(ms);
		ms = NULL;
	}
}

static int ft5x46_input_event(struct input_dev *dev,
		unsigned int type, unsigned int code, int value)
{
	struct ft5x46_data *ft5x46 = input_get_drvdata(dev);
	struct ft5x46_mode_switch *ms;

	if (type == EV_SYN && code == SYN_CONFIG) {
		dev_info(ft5x46->dev,
				"event write value = %d\n", value);

		if (value >= FT5X46_INPUT_EVENT_START && value <= FT5X46_INPUT_EVENT_END) {
			ms = (struct ft5x46_mode_switch*)kmalloc(sizeof(struct ft5x46_mode_switch), GFP_ATOMIC);
			if (ms != NULL) {
				ms->data = ft5x46;
				ms->mode = (u8)value;
				INIT_WORK(&ms->switch_mode_work, ft5x46_switch_mode_work);
				schedule_work(&ms->switch_mode_work);
			} else {
				dev_err(ft5x46->dev,
					"Failed in allocating memory for ft5x46_mode_switch!\n");
				return -ENOMEM;
			}
		}
	}

	return 0;
}

#ifdef FT5X46_APK_DEBUG_CHANNEL
static ssize_t ft5x46_apk_debug_read(struct file *file, char __user *buffer,
				size_t buflen, loff_t *fpos)
{
	struct ft5x46_data *ft5x46 = ft_data;
	int ret = 0;
	unsigned char *buf = NULL;
	int num_read_chars = 0;
	u8 val;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);

	ft5x46_disable_irq(ft5x46);
	mutex_lock(&ft5x46->mutex);
	switch (proc_operate_mode) {
		case FT5X46_PROC_UPGRADE:
			ret = ft5x46_read_byte(ft5x46, FT5X0X_REG_FW_VER, &val);
			if (ret < 0)
				num_read_chars = snprintf(buf, PAGE_SIZE,
					"%s", "Get FW version failed.\n");
			else
				num_read_chars = snprintf(buf, PAGE_SIZE,
					"current fw version:0x%02x\n", val);
			break;

		case FT5X46_PROC_READ_REGISTER:
			ret = ft5x46_recv_byte(ft5x46, 1, &val);
			if (ret < 0) {
				dev_err(ft5x46->dev, "%s: read register failed\n", __func__);
			} else {
				buf[0] = val;
				num_read_chars = 1;
			}
			break;

		case FT5X46_PROC_READ_DATA:
			ret = ft5x46_recv_block(ft5x46, buf, buflen);
			if (ret < 0) {
				dev_err(ft5x46->dev, "%s: read data failed\n", __func__);
			} else {
				num_read_chars = buflen;
			}
			break;

		case FT5X46_PROC_WRITE_REGISTER:
		case FT5X46_PROC_WRITE_DATA:
		default:
			break;
	}
	mutex_unlock(&ft5x46->mutex);
	ft5x46_enable_irq(ft5x46);

	if (ret == 0) {
		ret = copy_to_user(buffer, buf, min((int)buflen, num_read_chars));
	}

	kfree(buf);
	return ret;
}

#define FT5X46_MAX_FIRMWARE_LENGTH 128

static ssize_t ft5x46_apk_debug_write(struct file *file, const char __user *buffer,
				size_t buflen, loff_t *fpos)
{
	struct ft5x46_data *ft5x46 = ft_data;
	unsigned char writebuf[FT5X0X_PACKET_LENGTH];
	char upgrade_file_name[FT5X46_MAX_FIRMWARE_LENGTH];
	int ret = 0;

	if (copy_from_user(&writebuf, buffer,
		((buflen < FT5X0X_PACKET_LENGTH) ? buflen : FT5X0X_PACKET_LENGTH))) {
		dev_err(ft5x46->dev, "%s: copy from user failed\n", __func__);
		return -EFAULT;
	}

	proc_operate_mode = writebuf[0];
	switch (proc_operate_mode) {
		case FT5X46_PROC_UPGRADE:
			memset(upgrade_file_name, 0, FT5X46_MAX_FIRMWARE_LENGTH);
			snprintf(upgrade_file_name, FT5X46_MAX_FIRMWARE_LENGTH,
				"%s", writebuf + 1);
			upgrade_file_name[buflen - 1] = '\0';
			dev_info(ft5x46->dev, "%s: upgrade file name: %s\n",
				__func__, upgrade_file_name);
			ret = ft5x46_updatefw_with_filename(ft5x46, upgrade_file_name, NULL);
			if (ret < 0) {
				dev_err(ft5x46->dev, "%s: upgrade failed\n",
						__func__);
				return ret;
			}
			break;

		case FT5X46_PROC_READ_REGISTER:
			dev_info(ft5x46->dev, "%s: read register from %d\n",
				__func__, writebuf[1]);
			ret = ft5x46_send_byte(ft5x46, 1, writebuf[1]);
			if (ret < 0) {
				dev_err(ft5x46->dev, "%s: read register failed\n", __func__);
				return ret;
			}
			break;

		case FT5X46_PROC_WRITE_REGISTER:
			dev_info(ft5x46->dev, "%s: write to register %d: %d\n",
				__func__, writebuf[1], writebuf[2]);
			ret = ft5x46_write_byte(ft5x46, writebuf[1], writebuf[2]);
			if (ret < 0) {
				dev_err(ft5x46->dev, "%s: write register failed\n", __func__);
				return ret;
			}
			break;

		case FT5X46_PROC_AUTOCLB:
#ifdef CONFIG_TOUCHSCREEN_FT5X46_CALIBRATE
			dev_info(ft5x46->dev, "%s: Auto calibration\n", __func__);
			ft5x46_auto_calib(ft5x46);
#endif
			break;

		case FT5X46_PROC_READ_DATA:
		case FT5X46_PROC_WRITE_DATA:
			dev_info(ft5x46->dev, "%s: Read/Write data\n", __func__);
			ret = ft5x46_send_block(ft5x46, writebuf + 1, buflen - 1);
			if (ret < 0) {
				dev_err(ft5x46->dev, "%s: read/write data failed\n", __func__);
				return ret;
			}
			break;

		default:
			break;
	}

	return buflen;
}

static const struct file_operations ft5x46_proc_operations = {
	.read		= ft5x46_apk_debug_read,
	.write		= ft5x46_apk_debug_write,
};

static int ft5x46_create_apk_debug_channel(struct ft5x46_data *ft5x46)
{
	ft5x46_proc_entry = proc_create(FT5X46_PROC_NAME, 0777, NULL, &ft5x46_proc_operations);
	if (!ft5x46_proc_entry) {
		dev_err(ft5x46->dev, "Unable to create proc entry\n");
		return -ENOMEM;
	} else {
		dev_info(ft5x46->dev, "Create proc entry successfully\n");
		/* ft5x46_proc_entry->data = ft5x46; */
	}

	return 0;
}

static void ft5x46_release_apk_debug_channel(struct ft5x46_data *ft5x46)
{
	if (ft5x46_proc_entry)
		remove_proc_entry(FT5X46_PROC_NAME, NULL);
}
#endif

#ifdef CONFIG_PM
int ft5x46_pm_suspend(struct device *dev)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);

	if (ft5x46->wakeup_mode) {
		dev_info(dev, "touch enable irq wake\n");
		disable_irq(ft5x46->irq);
		enable_irq_wake(ft5x46->irq);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ft5x46_pm_suspend);

int ft5x46_pm_resume(struct device *dev)
{
	struct ft5x46_data *ft5x46 = dev_get_drvdata(dev);

	if (ft5x46->wakeup_mode) {
		dev_info(dev, "touch disable irq wake\n");
		disable_irq_wake(ft5x46->irq);
		enable_irq(ft5x46->irq);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(ft5x46_pm_resume);
#endif

void ft5x46_hw_init_work(struct work_struct *work)
{
	struct ft5x46_data *ft5x46 =
		container_of(work, struct ft5x46_data, work);
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;
	struct device *dev = ft5x46->dev;
	int error = 0;

	/* Reset */
	msleep(10);
	gpio_set_value_cansleep(pdata->reset_gpio, 1);
	msleep(300);

	/* For ft5x46, must swith to stdI2C mode before enter into firmware
	 * loading state. But this operation is not needed for ft8716/fte716.
	 */
	if (pdata->ui.upgrade_id_1 != FT8716_UPGRADE_ID_1
		&& pdata->ui.upgrade_id_1 != FTE716_UPGRADE_ID_1) {
		error = ft5x46_hid_to_std_i2c(ft5x46);
		if (error) {
			dev_err(dev, "Failed to switch to StdI2C\n");
			goto failed;
		}
	}

	error = ft5x46_read_byte(ft5x46, FT5X0X_REG_CHIP_ID, &ft5x46->chip_id);
	if (error) {
		dev_err(dev, "failed to read chip id\n");
		goto failed;
	} else {
		dev_info(dev, "ft5x46 chip id = %02x\n", ft5x46->chip_id);
	}

	/* ft8716 or ft5x46 */
	if (pdata->ui.upgrade_id_1 == FT8716_UPGRADE_ID_1
		|| pdata->ui.upgrade_id_1 == FTE716_UPGRADE_ID_1)
		error = ft8716_load_firmware(ft5x46, pdata->firmware, NULL);
	else
		error = ft5x46_load_firmware(ft5x46, pdata->firmware, NULL);
	if (error) {
		dev_err(dev, "fail to load firmware\n");
		goto failed;
	}

	if (0) {
	error = ft5x46_read_config_info(ft5x46);
	if (error) {
		dev_err(dev, "Failed to read config info\n");
		goto failed;
	}
	}

	ft5x46_enable_irq(ft5x46);
	ft5x46->hw_is_ready = true;

	queue_delayed_work(ft5x46->lcd_esdcheck_workqueue,
						&ft5x46->lcd_esdcheck_work,
						msecs_to_jiffies(FT5X46_ESD_CHECK_PERIOD));
	return;

failed:
	pr_err("ft5x46 hw init failed\n");
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	wake_up(&ft5x46->resume_wq);
	cancel_delayed_work_sync(&ft5x46->prox_enable_delayed_work);
#endif
	cancel_delayed_work_sync(&ft5x46->noise_filter_delayed_work);
	power_supply_unreg_notifier(&ft5x46->power_supply_notifier);
	if (ft5x46->vkeys_dir)
		sysfs_remove_file(ft5x46->vkeys_dir, &ft5x46->vkeys_attr.attr);
	kobject_put(ft5x46->vkeys_dir);
	ft5x46_configure_sleep(ft5x46, false);
#ifdef FT5X46_APK_DEBUG_CHANNEL
	ft5x46_release_apk_debug_channel(ft5x46);
#endif
	sysfs_remove_group(&ft5x46->dev->kobj, &ft5x46_attr_group);
	free_irq(ft5x46->irq, ft5x46);
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	kfree(ft5x46->proximity->phys);
#endif
	kfree(ft5x46->input->phys);
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	input_unregister_device(ft5x46->proximity);
#endif
	input_unregister_device(ft5x46->input);
	if (pdata->firmware) {
		if (pdata->firmware->data)
			kfree(pdata->firmware->data);
		kfree(pdata->firmware);
	}
	if (pdata->testdata)
		kfree(pdata->testdata);
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
	if (pdata->power_on)
		pdata->power_on(false);
	else
		ft5x46_power_on(ft5x46, false);
	if (pdata->power_init)
		pdata->power_init(false);
	else
		ft5x46_power_init(ft5x46, false);

	return;
}

static void tpdbg_suspend(struct ft5x46_data *ft5x46, bool enable)
{
	if (enable)
		ft5x46_suspend(ft5x46);
	else
		ft5x46_resume(ft5x46);
}

static int tpdbg_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t tpdbg_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{

	const char *str = "cmd support as below:\n \
				\necho \"irq-disable\" or \"irq-enable\" to ctrl irq\n \
				\necho \"tp-suspend-en\" or \"tp-suspend-off\" to ctrl panel in or off suspend status\n \
				\necho \"tp-sd-en\" or \"tp-sd-off\" to ctrl panel in or off sleep status\n";

	loff_t pos = *ppos;
	int len = strlen(str);

	if (pos < 0)
		return -EINVAL;
	if (pos >= len)
		return 0;

	if (copy_to_user(buf, str, len))
		return -EFAULT;

	*ppos = pos + len;

	return len;
}

static ssize_t tpdbg_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	struct ft5x46_data *ft5x46 = file->private_data;
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;
	char *cmd = kzalloc(size + 1, GFP_KERNEL);
	int ret = size;

	if (!cmd)
		return -ENOMEM;

	if (copy_from_user(cmd, buf, size)) {
		ret = -EFAULT;
		goto out;
	}

	cmd[size] = '\0';

	if (!strncmp(cmd, "irq-disable", 11))
		disable_irq(ft5x46->irq);
	else if (!strncmp(cmd, "irq-enable", 10))
		enable_irq(ft5x46->irq);
	else if (!strncmp(cmd, "tp-suspend-en", 13))
		tpdbg_suspend(ft5x46, true);
	else if (!strncmp(cmd, "tp-suspend-off", 14))
		tpdbg_suspend(ft5x46, false);
	else if (!strncmp(cmd, "tp-sd-en", 8))
		ft5x46_write_byte(ft5x46, FT5X0X_ID_G_PMODE, FT5X0X_POWER_HIBERNATE);
	else if (!strncmp(cmd, "tp-sd-off", 9)) {
		gpio_set_value(pdata->reset_gpio, 0);
		mdelay(5);
		gpio_set_value(pdata->reset_gpio, 1);
		mdelay(5);
	}
out:
	kfree(cmd);

	return ret;
}

static int tpdbg_release (struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}

static const struct file_operations tpdbg_operations = {
	.owner = THIS_MODULE,
	.open = tpdbg_open,
	.read = tpdbg_read,
	.write = tpdbg_write,
	.release = tpdbg_release,
};

static ssize_t tp_ic_vendor_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{

	const char *str = "FocalTech\n";

	loff_t pos = *ppos;
	int len = strlen(str);

	if (pos < 0)
		return -EINVAL;
	if (pos >= len)
		return 0;

	if (copy_to_user(buf, str, len))
		return -EFAULT;

	*ppos = pos + len;

	return len;
}

static const struct file_operations tp_ic_vendor_operations = {
	.owner = THIS_MODULE,
	.read = tp_ic_vendor_read,
};

#define TP_INFO_MAX_LENGTH 50
static ssize_t ft5x46_fw_version_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	int cnt = 0, ret = 0;
	char tmp[TP_INFO_MAX_LENGTH];
	u8 id = 0;
	struct ft5x46_data *ft5x46 = ft_data;

	if (*pos != 0)
		return 0;
	ret = ft5x46_read_byte(ft5x46, FT5X0X_REG_FW_VER, &id);
	if (ret)
		return 0;

	cnt = snprintf(tmp, TP_INFO_MAX_LENGTH, "%x\n", id);
	ret = copy_to_user(buf, tmp, cnt);
	*pos += cnt;
	if (ret != 0)
		return 0;
	else
		return cnt;
}

static const struct file_operations ft5x46_fw_version_ops = {
	.read		= ft5x46_fw_version_read,
};

static ssize_t ft5x46_lockdown_info_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	int cnt = 0, ret = 0;
	char tmp[TP_INFO_MAX_LENGTH];
	struct ft5x46_data *ft5x46 = ft_data;

	if (*pos != 0)
		return 0;
	cnt = snprintf(tmp, TP_INFO_MAX_LENGTH, "0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
			 ft5x46->lockdown_info[0], ft5x46->lockdown_info[1], ft5x46->lockdown_info[2], ft5x46->lockdown_info[3],
			 ft5x46->lockdown_info[4], ft5x46->lockdown_info[5], ft5x46->lockdown_info[6], ft5x46->lockdown_info[7]);
	ret = copy_to_user(buf, tmp, cnt);
	*pos += cnt;
	if (ret != 0)
		return 0;
	else
		return cnt;
}

static const struct file_operations ft5x46_lockdown_info_ops = {
	.read		= ft5x46_lockdown_info_read,
};

int idc_esdcheck_lcderror(struct ft5x46_data *ft5x46)
{
	u8 val;
	int ret;

	ret = ft5x46_read_byte(ft5x46, 0xED, &val);
	if (ret) {
		dev_err(ft5x46->dev, "[ESD] Failed to read 0xaa\n");
		return -EIO;
	}

	if (val == 0xAA) {
		dev_err(ft5x46->dev, "[ESD] LCD esd, Need Execute LCD reset\n");
		lcd_need_reset = true;
	}
	dev_dbg(ft5x46->dev, "%s ESD:0x%x\n", __func__, val);

	if (lcd_need_reset == true) {
		report_esd_panel_dead();
		lcd_need_reset = false;
	}

	return 0;
}

static void esdcheck_func(struct work_struct *work)
{

	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ft5x46_data *ft5x46 = container_of(delayed_work, struct ft5x46_data, lcd_esdcheck_work);
	if (!ft5x46)
		return;

	if (!ft5x46->in_suspend) {
		idc_esdcheck_lcderror(ft5x46);
		queue_delayed_work(ft5x46->lcd_esdcheck_workqueue,
							&ft5x46->lcd_esdcheck_work,
							msecs_to_jiffies(FT5X46_ESD_CHECK_PERIOD));
	}
}

struct ft5x46_data *ft5x46_probe(struct device *dev,
				const struct ft5x46_bus_ops *bops)
{
	int error = 0;
	struct ft5x46_data *ft5x46;
	struct ft5x46_ts_platform_data *pdata;

	/* check input argument */
	if (dev->of_node) {
		pdata = devm_kzalloc(dev,
				sizeof(struct ft5x46_ts_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(dev, "Failed to allocate memory!\n");
			return ERR_PTR(-ENOMEM);
		}

		vir_keypad = kzalloc(sizeof(struct ft5x46_keypad_data), GFP_KERNEL);
		if (vir_keypad == NULL)
			goto err;

		error = ft5x46_parse_dt(dev, pdata);
		if (error)
			goto err;
	} else
		pdata = dev->platform_data;

	if (pdata == NULL) {
		dev_err(dev, "platform data doesn't exist\n");
		error = -EINVAL;
		goto err;
	}

	/* alloc and init data object */
	ft5x46 = kzalloc(sizeof(struct ft5x46_data), GFP_KERNEL);
	if (ft5x46 == NULL) {
		dev_err(dev, "fail to allocate data object\n");
		error = -ENOMEM;
		goto err;
	}
	ft_data = ft5x46;
	ft5x46->dev  = dev;
	ft5x46->irq  = gpio_to_irq(pdata->irq_gpio);
	ft5x46->bops = bops;
	ft5x46->lockdown_info_acquired = false;
	if (dev->of_node)
		ft5x46->dev->platform_data = pdata;

	if (gpio_is_valid(pdata->irq_gpio)) {
		error = gpio_request(pdata->irq_gpio, "ft5x46_irq_gpio");
		if (error < 0) {
			dev_err(dev, "irq gpio request failed");
			goto err_free_data;
		}
		error = gpio_direction_input(pdata->irq_gpio);
		if (error < 0) {
			dev_err(dev, "set_direction for irq gpio failed\n");
			goto free_irq_gpio;
		}
	}

	if (gpio_is_valid(pdata->reset_gpio)) {
		error = gpio_request(pdata->reset_gpio, "ft5x46_reset_gpio");
		if (error < 0) {
			dev_err(dev, "irq gpio request failed");
			goto free_irq_gpio;
		}
		error = gpio_direction_output(pdata->reset_gpio, 0);
		if (error < 0) {
			dev_err(dev, "set_direction for irq gpio failed\n");
			goto free_reset_gpio;
		}
	}

	/* init platform stuff */
	if (pdata->power_init) {
		error = pdata->power_init(true);
		if (error) {
			dev_err(dev, "fail to power_init platform (pdata)\n");
			goto free_reset_gpio;
		}
	} else {
		error = ft5x46_power_init(ft5x46, true);
		if (error) {
			dev_err(dev, "fail to power_init platform\n");
			goto free_reset_gpio;
		}
	}

	if (pdata->power_on) {
		error = pdata->power_on(true);
		if (error) {
			dev_err(dev, "fail to power on (pdata)!\n");
			goto err_power_init;
		}
	} else {
		error = ft5x46_power_on(ft5x46, true);
		if (error) {
			dev_err(dev, "fail to power on\n");
			goto err_power_init;
		}
	}

	error = ft5x46_initialize_pinctrl(ft5x46);
	if (error || !ft5x46->ts_pinctrl) {
		dev_err(dev, "Initialize pinctrl failed\n");
		goto err_power;
	} else {
		error = ft5x46_pinctrl_select(ft5x46, true);
		if (error < 0) {
			dev_err(dev, "pinctrl select failed\n");
			goto err_power;
		}
	}

	mutex_init(&ft5x46->mutex);

	/* alloc and init input device */
	ft5x46->input = input_allocate_device();
	if (ft5x46->input == NULL) {
		dev_err(dev, "fail to allocate input device\n");
		error = -ENOMEM;
		goto err_pinctrl_init;
	}

	input_set_drvdata(ft5x46->input, ft5x46);
	ft5x46->input->name       = "ft5x46";
	ft5x46->input->id.bustype = bops->bustype;
	ft5x46->input->id.vendor  = 0x4654; /* FocalTech */
	ft5x46->input->id.product = 0x5000; /* ft5x0x    */
	ft5x46->input->id.version = 0x0100; /* 1.0       */
	ft5x46->input->dev.parent = dev;
	ft5x46->input->event      = ft5x46_input_event;

	/* init touch parameter */
	input_mt_init_slots(ft5x46->input, FT5X0X_MAX_FINGER, 0);
	set_bit(ABS_MT_TOUCH_MAJOR, ft5x46->input->absbit);
	set_bit(ABS_MT_TOUCH_MINOR, ft5x46->input->absbit);
	set_bit(ABS_MT_POSITION_X, ft5x46->input->absbit);
	set_bit(ABS_MT_POSITION_Y, ft5x46->input->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, ft5x46->input->absbit);
	set_bit(INPUT_PROP_DIRECT, ft5x46->input->propbit);

	input_set_abs_params(ft5x46->input,
			     ABS_MT_POSITION_X, 0, pdata->x_max, 0, 0);
	input_set_abs_params(ft5x46->input,
			     ABS_MT_POSITION_Y, 0, pdata->y_max, 0, 0);
	input_set_abs_params(ft5x46->input,
			     ABS_MT_TOUCH_MAJOR, 0, pdata->z_max, 0, 0);
	input_set_abs_params(ft5x46->input,
			     ABS_MT_WIDTH_MAJOR, 0, pdata->w_max, 0, 0);
	input_set_abs_params(ft5x46->input,
			     ABS_MT_TRACKING_ID, 0, 10, 0, 0);
	input_set_capability(ft5x46->input, EV_KEY, KEY_WAKEUP);

	set_bit(EV_KEY, ft5x46->input->evbit);
	set_bit(EV_ABS, ft5x46->input->evbit);
	set_bit(BTN_TOUCH, ft5x46->input->keybit);

#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	/* alloc and init proximity device */
	ft5x46->proximity = input_allocate_device();
	if (ft5x46->proximity == NULL) {
		dev_err(dev, "fail to allocate proximity device\n");
		error = -ENOMEM;
		goto err_free_input;
	}

	input_set_drvdata(ft5x46->proximity, ft5x46);
	ft5x46->proximity->name       = "proximity";
	ft5x46->proximity->id.bustype = bops->bustype;
	ft5x46->proximity->id.vendor  = 0x4654; /* FocalTech */
	ft5x46->proximity->id.product = 0x5000; /* ft5x0x    */
	ft5x46->proximity->id.version = 0x0100; /* 1.0       */
	ft5x46->proximity->dev.parent = dev;
	ft5x46->proximity->event      = NULL;

	/* init touch parameter */
	set_bit(ABS_DISTANCE, ft5x46->proximity->absbit);
	set_bit(INPUT_PROP_DIRECT, ft5x46->proximity->propbit);
	set_bit(EV_ABS, ft5x46->proximity->evbit);

	input_set_abs_params(ft5x46->proximity, ABS_DISTANCE, 0, 1, 0, 0);
#endif

	INIT_WORK(&ft5x46->work, ft5x46_hw_init_work);

	/* register input device */
	error = input_register_device(ft5x46->input);
	if (error) {
		dev_err(dev, "fail to register input device\n");
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
		goto err_free_proximity;
#else
		goto err_free_input;
#endif
	}

#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	/* register input device */
	error = input_register_device(ft5x46->proximity);
	if (error) {
		dev_err(dev, "fail to register proximity device\n");
		goto err_unregister_input;
	}
#endif

	ft5x46->input->phys =
		kobject_get_path(&ft5x46->input->dev.kobj, GFP_KERNEL);
	if (ft5x46->input->phys == NULL) {
		dev_err(dev, "fail to get input device path\n");
		error = -ENOMEM;
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
		goto err_unregister_proximity;
#else
		goto err_unregister_input;
#endif
	}

#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	ft5x46->proximity->phys =
		kobject_get_path(&ft5x46->proximity->dev.kobj, GFP_KERNEL);
	if (ft5x46->proximity->phys == NULL) {
		dev_err(dev, "fail to get proximity device path\n");
		error = -ENOMEM;
		goto err_free_phys;
	}
#endif

	/* start interrupt process */
	error = request_threaded_irq(ft5x46->irq, NULL, ft5x46_interrupt,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "ft5x46", ft5x46);
	if (error) {
		dev_err(dev, "fail to request interrupt\n");
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
		goto err_free_proximity_phys;
#else
		goto err_free_phys;
#endif
	}

	ft5x46->irq_enabled = true;
	ft5x46_disable_irq(ft5x46);

	/* export sysfs entries */
	error = sysfs_create_group(&dev->kobj, &ft5x46_attr_group);
	if (error) {
		dev_err(dev, "fail to export sysfs entires\n");
		goto err_free_irq;
	}

	error = ft5x46_configure_sleep(ft5x46, true);
	if (error) {
		dev_err(dev, "Failed to configure sleep\n");
		goto err_sysfs_create_group;
	}

	if (vir_keypad->length > 0) {
		ft5x46->vkeys_dir = kobject_create_and_add("board_properties",
				NULL);
		if (!ft5x46->vkeys_dir) {
			error = -ENOMEM;
			dev_err(dev, "Failed to create board_properties entry\n");
			goto err_configure_sleep;
		}

		sysfs_attr_init(&ft5x46->vkeys_attr.attr);
		ft5x46->vkeys_attr.attr.name = "virtualkeys.ft5x46";
		ft5x46->vkeys_attr.attr.mode = S_IRUGO;
		ft5x46->vkeys_attr.show = ft5x46_vkeys_show;

		error = sysfs_create_file(ft5x46->vkeys_dir,
				&ft5x46->vkeys_attr.attr);
		if (error) {
			dev_err(dev, "Failed to create virtual keys entry\n");
			goto err_put_vkeys_dir;
		}
	}

#ifdef FT5X46_APK_DEBUG_CHANNEL
	error = ft5x46_create_apk_debug_channel(ft5x46);
	if (error) {
		dev_err(dev, "Failed to create APK debug channel\n");
		goto err_sysfs_create_virtualkeys;
	}
#endif

	ft5x46->power_supply_notifier.notifier_call = ft5x46_power_supply_event;
	power_supply_reg_notifier(&ft5x46->power_supply_notifier);

	INIT_DELAYED_WORK(&ft5x46->noise_filter_delayed_work,
				ft5x46_noise_filter_delayed_work);

	INIT_DELAYED_WORK(&ft5x46->lcd_esdcheck_work, esdcheck_func);
	ft5x46->lcd_esdcheck_workqueue = create_workqueue("touch_lcd_esdcheck_wq");
	if (!ft5x46->lcd_esdcheck_workqueue) {
		dev_err(dev, "Failed to create lcd esd workqueue\n");
		goto err_sysfs_create_virtualkeys;
	}
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	INIT_DELAYED_WORK(&ft5x46->prox_enable_delayed_work,
				ft5x46_prox_enable_delayed_work);
	init_waitqueue_head(&ft5x46->resume_wq);
#endif

	ft5x46->debugfs = debugfs_create_dir("tp_debug", NULL);
	if (ft5x46->debugfs) {
		debugfs_create_file("switch_state", 0660, ft5x46->debugfs, ft5x46, &tpdbg_operations);
	}
	ft5x46->tpfs = debugfs_create_dir("touch", NULL);
	if (ft5x46->tpfs)
		debugfs_create_file("tp_ic_vendor", 0400, ft5x46->tpfs, ft5x46, &tp_ic_vendor_operations);

	ft5x46->tp_fw_version_proc = proc_create("tp_fw_version", 0, NULL, &ft5x46_fw_version_ops);
	ft5x46->tp_lockdown_info_proc = proc_create("tp_lockdown_info", 0, NULL, &ft5x46_lockdown_info_ops);

	ft5x46->hw_is_ready = false;
	init_waitqueue_head(&ft5x46->lockdown_info_acquired_wq);
	schedule_work(&ft5x46->work);
	ft5x46->dbclick_count = 0;
	return ft5x46;

#ifdef FT5X46_APK_DEBUG_CHANNEL
err_sysfs_create_virtualkeys:
	if (ft5x46->vkeys_dir)
		sysfs_remove_file(ft5x46->vkeys_dir, &ft5x46->vkeys_attr.attr);
#endif
err_put_vkeys_dir:
	kobject_put(ft5x46->vkeys_dir);
err_configure_sleep:
	ft5x46_configure_sleep(ft5x46, false);
err_sysfs_create_group:
	sysfs_remove_group(&dev->kobj, &ft5x46_attr_group);
err_free_irq:
	free_irq(ft5x46->irq, ft5x46);
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
err_free_proximity_phys:
	kfree(ft5x46->proximity->phys);
#endif
err_free_phys:
	kfree(ft5x46->input->phys);
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
err_unregister_proximity:
	input_unregister_device(ft5x46->proximity);
	ft5x46->proximity = NULL;
#endif
err_unregister_input:
	input_unregister_device(ft5x46->input);
	ft5x46->input = NULL;
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
err_free_proximity:
	input_free_device(ft5x46->proximity);
#endif
err_free_input:
	input_free_device(ft5x46->input);
err_pinctrl_init:
	if (ft5x46->ts_pinctrl) {
		devm_pinctrl_put(ft5x46->ts_pinctrl);
		ft5x46->ts_pinctrl = NULL;
	}
err_power:
	if (pdata->power_on)
		pdata->power_on(false);
	else
		ft5x46_power_on(ft5x46, false);
err_power_init:
	if (pdata->power_init)
		pdata->power_init(false);
	else
		ft5x46_power_init(ft5x46, false);
free_reset_gpio:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
free_irq_gpio:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
err_free_data:
	kfree(ft5x46);
err:
	if (vir_keypad) {
		kfree(vir_keypad->keymap);
		kfree(vir_keypad);
	}
	return ERR_PTR(error);
}
EXPORT_SYMBOL_GPL(ft5x46_probe);

void ft5x46_remove(struct ft5x46_data *ft5x46)
{
	struct ft5x46_ts_platform_data *pdata = ft5x46->dev->platform_data;

	cancel_work_sync(&ft5x46->work);
	if (!ft5x46->hw_is_ready) {
		kfree(ft5x46);
		if (vir_keypad) {
			kfree(vir_keypad->keymap);
			kfree(vir_keypad);
		}
		return;
	}
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	wake_up(&ft5x46->resume_wq);
	cancel_delayed_work_sync(&ft5x46->prox_enable_delayed_work);
#endif
	cancel_delayed_work_sync(&ft5x46->noise_filter_delayed_work);
	cancel_delayed_work_sync(&ft5x46->lcd_esdcheck_work);
	power_supply_unreg_notifier(&ft5x46->power_supply_notifier);
	if (ft5x46->vkeys_dir)
		sysfs_remove_file(ft5x46->vkeys_dir, &ft5x46->vkeys_attr.attr);
	kobject_put(ft5x46->vkeys_dir);
	ft5x46_configure_sleep(ft5x46, false);
#ifdef FT5X46_APK_DEBUG_CHANNEL
	ft5x46_release_apk_debug_channel(ft5x46);
#endif
	sysfs_remove_group(&ft5x46->dev->kobj, &ft5x46_attr_group);
	free_irq(ft5x46->irq, ft5x46);
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	kfree(ft5x46->proximity->phys);
#endif
	kfree(ft5x46->input->phys);
#ifdef CONFIG_TOUCHSCREEN_FT5X46P_PROXIMITY
	input_unregister_device(ft5x46->proximity);
#endif
	input_unregister_device(ft5x46->input);
	if (pdata->firmware) {
		if (pdata->firmware->data)
			kfree(pdata->firmware->data);
		kfree(pdata->firmware);
	}
	if (pdata->testdata)
		kfree(pdata->testdata);
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
	if (pdata->power_on)
		pdata->power_on(false);
	else
		ft5x46_power_on(ft5x46, false);
	if (pdata->power_init)
		pdata->power_init(false);
	else
		ft5x46_power_init(ft5x46, false);
	kfree(ft5x46);

	if (vir_keypad) {
		kfree(vir_keypad->keymap);
		kfree(vir_keypad);
	}
}
EXPORT_SYMBOL_GPL(ft5x46_remove);

MODULE_AUTHOR("Zhang Bo <zhangbo_a@xiaomi.com>");
MODULE_DESCRIPTION("ft5x0x touchscreen input driver");
MODULE_LICENSE("GPL");
