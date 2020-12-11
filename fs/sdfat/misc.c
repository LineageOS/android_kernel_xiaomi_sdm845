/*
 *  Copyright (C) 2012-2013 Samsung Electronics Co., Ltd.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 *  linux/fs/fat/misc.c
 *
 *  Written 1992,1993 by Werner Almesberger
 *  22/11/2000 - Fixed fat_date_unix2dos for dates earlier than 01/01/1980
 *		 and date_dos2unix for date==0 by Igor Zhbanov(bsg@uniyar.ac.ru)
 */

/************************************************************************/
/*                                                                      */
/*  PROJECT : exFAT & FAT12/16/32 File System                           */
/*  FILE    : misc.c                                                    */
/*  PURPOSE : Helper function for checksum and handing sdFAT error      */
/*                                                                      */
/*----------------------------------------------------------------------*/
/*  NOTES                                                               */
/*                                                                      */
/*                                                                      */
/************************************************************************/

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/buffer_head.h>
#include <linux/time.h>
#include "sdfat.h"
#include "version.h"

#ifdef CONFIG_SDFAT_SUPPORT_STLOG
#ifdef CONFIG_PROC_FSLOG
#include <linux/fslog.h>
#else
#include <linux/stlog.h>
#endif
#else
#define ST_LOG(fmt, ...)
#endif

/*************************************************************************
 * FUNCTIONS WHICH HAS KERNEL VERSION DEPENDENCY
 *************************************************************************/
#ifdef CONFIG_SDFAT_UEVENT
static struct kobject sdfat_uevent_kobj;

int sdfat_uevent_init(struct kset *sdfat_kset)
{
	int err;
	struct kobj_type *ktype = get_ktype(&sdfat_kset->kobj);

	sdfat_uevent_kobj.kset = sdfat_kset;
	err = kobject_init_and_add(&sdfat_uevent_kobj, ktype, NULL, "uevent");
	if (err)
		pr_err("[SDFAT] Unable to create sdfat uevent kobj\n");

	return err;
}

void sdfat_uevent_uninit(void)
{
	kobject_del(&sdfat_uevent_kobj);
	memset(&sdfat_uevent_kobj, 0, sizeof(struct kobject));
}

void sdfat_uevent_ro_remount(struct super_block *sb)
{
	struct block_device *bdev = sb->s_bdev;
	dev_t bd_dev = bdev ? bdev->bd_dev : 0;

	char major[16], minor[16];
	char *envp[] = { major, minor, NULL };

	/* Do not trigger uevent if a device has been ejected */
	if (fsapi_check_bdi_valid(sb))
		return;

	snprintf(major, sizeof(major), "MAJOR=%d", MAJOR(bd_dev));
	snprintf(minor, sizeof(minor), "MINOR=%d", MINOR(bd_dev));

	kobject_uevent_env(&sdfat_uevent_kobj, KOBJ_CHANGE, envp);

	ST_LOG("[SDFAT](%s[%d:%d]): Uevent triggered\n",
			sb->s_id, MAJOR(bd_dev), MINOR(bd_dev));
}
#endif

/*
 * sdfat_fs_error reports a file system problem that might indicate fa data
 * corruption/inconsistency. Depending on 'errors' mount option the
 * panic() is called, or error message is printed FAT and nothing is done,
 * or filesystem is remounted read-only (default behavior).
 * In case the file system is remounted read-only, it can be made writable
 * again by remounting it.
 */
void __sdfat_fs_error(struct super_block *sb, int report, const char *fmt, ...)
{
	struct sdfat_mount_options *opts = &SDFAT_SB(sb)->options;
	va_list args;
	struct va_format vaf;
	struct block_device *bdev = sb->s_bdev;
	dev_t bd_dev = bdev ? bdev->bd_dev : 0;

	if (report) {
		va_start(args, fmt);
		vaf.fmt = fmt;
		vaf.va = &args;
		pr_err("[SDFAT](%s[%d:%d]):ERR: %pV\n",
			sb->s_id, MAJOR(bd_dev), MINOR(bd_dev), &vaf);
#ifdef CONFIG_SDFAT_SUPPORT_STLOG
		if (opts->errors == SDFAT_ERRORS_RO && !sb_rdonly(sb)) {
			ST_LOG("[SDFAT](%s[%d:%d]):ERR: %pV\n",
				sb->s_id, MAJOR(bd_dev), MINOR(bd_dev), &vaf);
		}
#endif
		va_end(args);
	}

	if (opts->errors == SDFAT_ERRORS_PANIC) {
		panic("[SDFAT](%s[%d:%d]): fs panic from previous error\n",
			sb->s_id, MAJOR(bd_dev), MINOR(bd_dev));
	} else if (opts->errors == SDFAT_ERRORS_RO && !sb_rdonly(sb)) {
		sb->s_flags |= SB_RDONLY;
		sdfat_statistics_set_mnt_ro();
		pr_err("[SDFAT](%s[%d:%d]): Filesystem has been set "
			"read-only\n", sb->s_id, MAJOR(bd_dev), MINOR(bd_dev));
#ifdef CONFIG_SDFAT_SUPPORT_STLOG
		ST_LOG("[SDFAT](%s[%d:%d]): Filesystem has been set read-only\n",
			sb->s_id, MAJOR(bd_dev), MINOR(bd_dev));
#endif
		sdfat_uevent_ro_remount(sb);
	}
}
EXPORT_SYMBOL(__sdfat_fs_error);

/**
 * __sdfat_msg() - print preformated SDFAT specific messages.
 * All logs except what uses sdfat_fs_error() should be written by __sdfat_msg()
 * If 'st' is set, the log is propagated to ST_LOG.
 */
void __sdfat_msg(struct super_block *sb, const char *level, int st, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	struct block_device *bdev = sb->s_bdev;
	dev_t bd_dev = bdev ? bdev->bd_dev : 0;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	/* level means KERN_ pacility level */
	printk("%s[SDFAT](%s[%d:%d]): %pV\n", level,
			sb->s_id, MAJOR(bd_dev), MINOR(bd_dev), &vaf);
#ifdef CONFIG_SDFAT_SUPPORT_STLOG
	if (st) {
		ST_LOG("[SDFAT](%s[%d:%d]): %pV\n",
				sb->s_id, MAJOR(bd_dev), MINOR(bd_dev), &vaf);
	}
#endif
	va_end(args);
}
EXPORT_SYMBOL(__sdfat_msg);

void sdfat_log_version(void)
{
	pr_info("[SDFAT] Filesystem version %s\n", SDFAT_VERSION);
#ifdef CONFIG_SDFAT_SUPPORT_STLOG
	ST_LOG("[SDFAT] Filesystem version %s\n", SDFAT_VERSION);
#endif
}
EXPORT_SYMBOL(sdfat_log_version);

/* <linux/time.h> externs sys_tz
 * extern struct timezone sys_tz;
 */
#define UNIX_SECS_1980    315532800L

#if BITS_PER_LONG == 64
#define UNIX_SECS_2108    4354819200L
#endif

/* days between 1970/01/01 and 1980/01/01 (2 leap days) */
#define DAYS_DELTA_DECADE    (365 * 10 + 2)
/* 120 (2100 - 1980) isn't leap year */
#define NO_LEAP_YEAR_2100    (120)
#define IS_LEAP_YEAR(y)    (!((y) & 0x3) && (y) != NO_LEAP_YEAR_2100)

#define SECS_PER_MIN    (60)
#define SECS_PER_HOUR   (60 * SECS_PER_MIN)
#define SECS_PER_DAY    (24 * SECS_PER_HOUR)

#define MAKE_LEAP_YEAR(leap_year, year)                         \
	do {                                                    \
		/* 2100 isn't leap year */                      \
		if (unlikely(year > NO_LEAP_YEAR_2100))         \
			leap_year = ((year + 3) / 4) - 1;       \
		else                                            \
			leap_year = ((year + 3) / 4);           \
	} while (0)

/* Linear day numbers of the respective 1sts in non-leap years. */
static time_t accum_days_in_year[] = {
	/* Month : N 01  02  03  04  05  06  07  08  09  10  11  12 */
	0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 0, 0, 0,
};

#define TIMEZONE_SEC(x)	((x) * 15 * SECS_PER_MIN)
/* Convert a FAT time/date pair to a UNIX date (seconds since 1 1 70). */
void sdfat_time_fat2unix(struct sdfat_sb_info *sbi, sdfat_timespec_t *ts,
								DATE_TIME_T *tp)
{
	time_t year = tp->Year;
	time_t ld; /* leap day */

	MAKE_LEAP_YEAR(ld, year);

	if (IS_LEAP_YEAR(year) && (tp->Month) > 2)
		ld++;

	ts->tv_sec =  tp->Second  + tp->Minute * SECS_PER_MIN
			+ tp->Hour * SECS_PER_HOUR
			+ (year * 365 + ld + accum_days_in_year[tp->Month]
			+ (tp->Day - 1) + DAYS_DELTA_DECADE) * SECS_PER_DAY;

	ts->tv_nsec = 0;

	/* Treat as local time */
	if (!sbi->options.tz_utc && !tp->Timezone.valid) {
		ts->tv_sec += sys_tz.tz_minuteswest * SECS_PER_MIN;
		return;
	}

	/* Treat as UTC time */
	if (!tp->Timezone.valid)
		return;

	/* Treat as UTC time, but need to adjust timezone to UTC0 */
	if (tp->Timezone.off <= 0x3F)
		ts->tv_sec -= TIMEZONE_SEC(tp->Timezone.off);
	else /* 0x40 <= (tp->Timezone & 0x7F) <=0x7F */
		ts->tv_sec += TIMEZONE_SEC(0x80 - tp->Timezone.off);
}

#define TIMEZONE_CUR_OFFSET()	((sys_tz.tz_minuteswest / (-15)) & 0x7F)
/* Convert linear UNIX date to a FAT time/date pair. */
void sdfat_time_unix2fat(struct sdfat_sb_info *sbi, sdfat_timespec_t *ts,
								DATE_TIME_T *tp)
{
	bool tz_valid = (sbi->fsi.vol_type == EXFAT) ? true : false;
	time_t second = ts->tv_sec;
	time_t day, month, year;
	time_t ld; /* leap day */

	tp->Timezone.value = 0x00;

	/* Treats as local time with proper time */
	if (tz_valid || !sbi->options.tz_utc) {
		second -= sys_tz.tz_minuteswest * SECS_PER_MIN;
		if (tz_valid) {
			tp->Timezone.valid = 1;
			tp->Timezone.off = TIMEZONE_CUR_OFFSET();
		}
	}

	/* Jan 1 GMT 00:00:00 1980. But what about another time zone? */
	if (second < UNIX_SECS_1980) {
		tp->Second  = 0;
		tp->Minute  = 0;
		tp->Hour = 0;
		tp->Day  = 1;
		tp->Month  = 1;
		tp->Year = 0;
		return;
	}
#if (BITS_PER_LONG == 64)
	if (second >= UNIX_SECS_2108) {
		tp->Second  = 59;
		tp->Minute  = 59;
		tp->Hour = 23;
		tp->Day  = 31;
		tp->Month  = 12;
		tp->Year = 127;
		return;
	}
#endif

	day = second / SECS_PER_DAY - DAYS_DELTA_DECADE;
	year = day / 365;

	MAKE_LEAP_YEAR(ld, year);
	if (year * 365 + ld > day)
		year--;

	MAKE_LEAP_YEAR(ld, year);
	day -= year * 365 + ld;

	if (IS_LEAP_YEAR(year) && day == accum_days_in_year[3]) {
		month = 2;
	} else {
		if (IS_LEAP_YEAR(year) && day > accum_days_in_year[3])
			day--;
		for (month = 1; month < 12; month++) {
			if (accum_days_in_year[month + 1] > day)
				break;
		}
	}
	day -= accum_days_in_year[month];

	tp->Second  = second % SECS_PER_MIN;
	tp->Minute  = (second / SECS_PER_MIN) % 60;
	tp->Hour = (second / SECS_PER_HOUR) % 24;
	tp->Day  = day + 1;
	tp->Month  = month;
	tp->Year = year;
}

TIMESTAMP_T *tm_now(struct inode *inode, TIMESTAMP_T *tp)
{
	sdfat_timespec_t ts = current_time(inode);
	DATE_TIME_T dt;

	sdfat_time_unix2fat(SDFAT_SB(inode->i_sb), &ts, &dt);

	tp->year = dt.Year;
	tp->mon = dt.Month;
	tp->day = dt.Day;
	tp->hour = dt.Hour;
	tp->min = dt.Minute;
	tp->sec = dt.Second;
	tp->tz.value = dt.Timezone.value;

	return tp;
}

u8 calc_chksum_1byte(void *data, s32 len, u8 chksum)
{
	s32 i;
	u8 *c = (u8 *) data;

	for (i = 0; i < len; i++, c++)
		chksum = (((chksum & 1) << 7) | ((chksum & 0xFE) >> 1)) + *c;

	return chksum;
}

u16 calc_chksum_2byte(void *data, s32 len, u16 chksum, s32 type)
{
	s32 i;
	u8 *c = (u8 *) data;

	for (i = 0; i < len; i++, c++) {
		if (((i == 2) || (i == 3)) && (type == CS_DIR_ENTRY))
			continue;
		chksum = (((chksum & 1) << 15) | ((chksum & 0xFFFE) >> 1)) + (u16) *c;
	}
	return chksum;
}

#ifdef CONFIG_SDFAT_TRACE_ELAPSED_TIME
struct timeval __t1, __t2;
u32 sdfat_time_current_usec(struct timeval *tv)
{
	do_gettimeofday(tv);
	return (u32)(tv->tv_sec*1000000 + tv->tv_usec);
}
#endif /* CONFIG_SDFAT_TRACE_ELAPSED_TIME */

#ifdef CONFIG_SDFAT_DBG_CAREFUL
/* Check the consistency of i_size_ondisk (FAT32, or flags 0x01 only) */
void sdfat_debug_check_clusters(struct inode *inode)
{
	unsigned int num_clusters;
	volatile uint32_t tmp_fat_chain[50];
	volatile int tmp_i = 0;
	volatile unsigned int num_clusters_org, tmp_i = 0;
	CHAIN_T clu;
	FILE_ID_T *fid = &(SDFAT_I(inode)->fid);
	FS_INFO_T *fsi = &(SDFAT_SB(inode->i_sb)->fsi);

	if (SDFAT_I(inode)->i_size_ondisk == 0)
		num_clusters = 0;
	else
		num_clusters = ((SDFAT_I(inode)->i_size_ondisk-1) >> fsi->cluster_size_bits) + 1;

	clu.dir = fid->start_clu;
	clu.size = num_clusters;
	clu.flags = fid->flags;

	num_clusters_org = num_clusters;

	if (clu.flags == 0x03)
		return;

	while (num_clusters > 0) {
		/* FAT chain logging */
		tmp_fat_chain[tmp_i] = clu.dir;
		tmp_i++;
		if (tmp_i >= 50)
			tmp_i = 0;

		BUG_ON(IS_CLUS_EOF(clu.dir) || IS_CLUS_FREE(clu.dir));

		if (get_next_clus_safe(inode->i_sb, &(clu.dir)))
			EMSG("%s: failed to access to FAT\n");

		num_clusters--;
	}

	BUG_ON(!IS_CLUS_EOF(clu.dir));
}

#endif /* CONFIG_SDFAT_DBG_CAREFUL */

#ifdef CONFIG_SDFAT_DBG_MSG
void __sdfat_dmsg(int level, const char *fmt, ...)
{
#ifdef CONFIG_SDFAT_DBG_SHOW_PID
	struct va_format vaf;
	va_list args;

	/* should check type */
	if (level > SDFAT_MSG_LEVEL)
		return;

	va_start(args, fmt);
	vaf.fmt = fmt;
	vaf.va = &args;
	/* fmt already includes KERN_ pacility level */
	printk("[%u] %pV", current->pid,  &vaf);
	va_end(args);
#else
	va_list args;

	/* should check type */
	if (level > SDFAT_MSG_LEVEL)
		return;

	va_start(args, fmt);
	/* fmt already includes KERN_ pacility level */
	vprintk(fmt, args);
	va_end(args);
#endif
}
#endif

