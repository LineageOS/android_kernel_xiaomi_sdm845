// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Adithya R <gh0strider.2k18.reborn@gmail.com>.
 */

#ifndef _BATTERY_SAVER_H_
#define _BATTERY_SAVER_H_

#include <linux/types.h>

#ifdef CONFIG_BATTERY_SAVER
bool is_battery_saver_on(void);
void enable_battery_saver(bool status);
#else
static inline bool is_battery_saver_on()
{
	return false;
}
static inline void update_battery_saver(bool status)
{
}
#endif

#endif /* _BATTERY_SAVER_H_ */
