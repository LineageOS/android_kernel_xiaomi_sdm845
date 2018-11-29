/*
 * Copyright (c) 2013-2018 The Linux Foundation. All rights reserved.
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

#ifndef _WLAN_HDD_DEBUGFS_H
#define _WLAN_HDD_DEBUGFS_H

#ifdef WLAN_DEBUGFS
QDF_STATUS hdd_debugfs_init(hdd_adapter_t *adapter);
void hdd_debugfs_exit(hdd_adapter_t *adapter);

/**
 * hdd_wait_for_debugfs_threads_completion() - Wait for debugfs threads
 * completion before proceeding further to stop modules
 *
 * Return: true if there is no debugfs open
 *         false if there is at least one debugfs open
 */
bool hdd_wait_for_debugfs_threads_completion(void);

/**
 * hdd_return_debugfs_threads_count() - Return active debugfs threads
 *
 * Return: total number of active debugfs threads in driver
 */
int hdd_return_debugfs_threads_count(void);

/**
 * hdd_debugfs_thread_increment() - Increment debugfs thread count
 *
 * This function is used to increment and keep track of debugfs thread count.
 * This is invoked for every file open operation.
 *
 * Return: None
 */
void hdd_debugfs_thread_increment(void);

/**
 * hdd_debugfs_thread_decrement() - Decrement debugfs thread count
 *
 * This function is used to decrement and keep track of debugfs thread count.
 * This is invoked for every file release operation.
 *
 * Return: None
 */
void hdd_debugfs_thread_decrement(void);

#else
static inline QDF_STATUS hdd_debugfs_init(hdd_adapter_t *pAdapter)
{
	return QDF_STATUS_SUCCESS;
}

static inline void hdd_debugfs_exit(hdd_adapter_t *adapter)
{
}

/**
 * hdd_wait_for_debugfs_threads_completion() - Wait for debugfs threads
 * completion before proceeding further to stop modules
 *
 * Return: true if there is no debugfs open
 *         false if there is at least one debugfs open
 */
static inline
bool hdd_wait_for_debugfs_threads_completion(void)
{
	return true;
}

/**
 * hdd_return_debugfs_threads_count() - Return active debugfs threads
 *
 * Return: total number of active debugfs threads in driver
 */
static inline
int hdd_return_debugfs_threads_count(void)
{
	return 0;
}

/**
 * hdd_debugfs_thread_increment() - Increment debugfs thread count
 *
 * This function is used to increment and keep track of debugfs thread count.
 * This is invoked for every file open operation.
 *
 * Return: None
 */
static inline
void hdd_debugfs_thread_increment(void)
{
}

/**
 * hdd_debugfs_thread_decrement() - Decrement debugfs thread count
 *
 * This function is used to decrement and keep track of debugfs thread count.
 * This is invoked for every file release operation.
 *
 * Return: None
 */
static inline
void hdd_debugfs_thread_decrement(void)
{
}

#endif /* #ifdef WLAN_DEBUGFS */
#endif /* #ifndef _WLAN_HDD_DEBUGFS_H */
