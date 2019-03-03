// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018-2019 Sultan Alsawaf <sultan@kerneltoast.com>.
 */

#define pr_fmt(fmt) "devfreq_boost: " fmt

#include <linux/devfreq_boost.h>
#include <linux/msm_drm_notify.h>
#include <linux/input.h>
#include <linux/slab.h>

struct boost_dev {
	struct workqueue_struct *wq;
	struct devfreq *df;
	struct work_struct input_boost;
	struct delayed_work input_unboost;
	struct work_struct max_boost;
	struct delayed_work max_unboost;
	unsigned long abs_min_freq;
	unsigned long boost_freq;
	unsigned long max_boost_expires;
	unsigned long max_boost_jiffies;
	spinlock_t lock;
};

struct df_boost_drv {
	struct boost_dev devices[DEVFREQ_MAX];
	struct notifier_block msm_drm_notif;
	bool screen_awake;
};

static struct df_boost_drv *df_boost_drv_g __read_mostly;

static void __devfreq_boost_kick(struct boost_dev *b)
{
	unsigned long flags;

	spin_lock_irqsave(&b->lock, flags);
	if (!b->df) {
		spin_unlock_irqrestore(&b->lock, flags);
		return;
	}
	spin_unlock_irqrestore(&b->lock, flags);

	queue_work(b->wq, &b->input_boost);
}

void devfreq_boost_kick(enum df_device device)
{
	struct df_boost_drv *d = df_boost_drv_g;

	if (!d)
		return;

	if (!d->screen_awake)
		return;

	__devfreq_boost_kick(d->devices + device);
}

static void __devfreq_boost_kick_max(struct boost_dev *b,
				     unsigned int duration_ms)
{
	unsigned long flags, new_expires;

	spin_lock_irqsave(&b->lock, flags);
	if (!b->df) {
		spin_unlock_irqrestore(&b->lock, flags);
		return;
	}

	new_expires = jiffies + b->max_boost_jiffies;
	if (time_after(b->max_boost_expires, new_expires)) {
		spin_unlock_irqrestore(&b->lock, flags);
		return;
	}
	b->max_boost_expires = new_expires;
	b->max_boost_jiffies = msecs_to_jiffies(duration_ms);
	spin_unlock_irqrestore(&b->lock, flags);

	queue_work(b->wq, &b->max_boost);
}

void devfreq_boost_kick_max(enum df_device device, unsigned int duration_ms)
{
	struct df_boost_drv *d = df_boost_drv_g;

	if (!d)
		return;

	if (!d->screen_awake)
		return;

	__devfreq_boost_kick_max(d->devices + device, duration_ms);
}

void devfreq_register_boost_device(enum df_device device, struct devfreq *df)
{
	struct df_boost_drv *d = df_boost_drv_g;
	struct boost_dev *b;
	unsigned long flags;

	if (!d)
		return;

	df->is_boost_device = true;

	b = d->devices + device;
	spin_lock_irqsave(&b->lock, flags);
	b->df = df;
	spin_unlock_irqrestore(&b->lock, flags);
}

static unsigned long devfreq_abs_min_freq(struct boost_dev *b)
{
	struct devfreq *df = b->df;
	int i;

	/* Reuse the absolute min freq found the first time this was called */
	if (b->abs_min_freq != ULONG_MAX)
		return b->abs_min_freq;

	/* Find the lowest non-zero freq from the freq table */
	for (i = 0; i < df->profile->max_state; i++) {
		unsigned int freq = df->profile->freq_table[i];

		if (!freq)
			continue;

		if (b->abs_min_freq > freq)
			b->abs_min_freq = freq;
	}

	/* Use zero for the absolute min freq if nothing was found */
	if (b->abs_min_freq == ULONG_MAX)
		b->abs_min_freq = 0;

	return b->abs_min_freq;
}

static void devfreq_unboost_all(struct df_boost_drv *d)
{
	int i;

	for (i = 0; i < DEVFREQ_MAX; i++) {
		struct boost_dev *b = d->devices + i;
		struct devfreq *df;
		unsigned long flags;

		spin_lock_irqsave(&b->lock, flags);
		df = b->df;
		spin_unlock_irqrestore(&b->lock, flags);

		if (!df)
			continue;

		cancel_work_sync(&b->max_boost);
		cancel_delayed_work_sync(&b->max_unboost);
		cancel_work_sync(&b->input_boost);
		cancel_delayed_work_sync(&b->input_unboost);

		mutex_lock(&df->lock);
		df->max_boost = false;
		df->min_freq = devfreq_abs_min_freq(b);
		update_devfreq(df);
		mutex_unlock(&df->lock);
	}
}

static void devfreq_input_boost(struct work_struct *work)
{
	struct boost_dev *b = container_of(work, typeof(*b), input_boost);

	if (!cancel_delayed_work_sync(&b->input_unboost)) {
		struct devfreq *df = b->df;
		unsigned long boost_freq, flags;

		spin_lock_irqsave(&b->lock, flags);
		boost_freq = b->boost_freq;
		spin_unlock_irqrestore(&b->lock, flags);

		mutex_lock(&df->lock);
		if (df->max_freq)
			df->min_freq = min(boost_freq, df->max_freq);
		else
			df->min_freq = boost_freq;
		update_devfreq(df);
		mutex_unlock(&df->lock);
	}

	queue_delayed_work(b->wq, &b->input_unboost,
		msecs_to_jiffies(CONFIG_DEVFREQ_INPUT_BOOST_DURATION_MS));
}

static void devfreq_input_unboost(struct work_struct *work)
{
	struct boost_dev *b = container_of(to_delayed_work(work),
					   typeof(*b), input_unboost);
	struct devfreq *df = b->df;

	mutex_lock(&df->lock);
	df->min_freq = devfreq_abs_min_freq(b);
	update_devfreq(df);
	mutex_unlock(&df->lock);
}

static void devfreq_max_boost(struct work_struct *work)
{
	struct boost_dev *b = container_of(work, typeof(*b), max_boost);
	unsigned long boost_jiffies, flags;

	if (!cancel_delayed_work_sync(&b->max_unboost)) {
		struct devfreq *df = b->df;

		mutex_lock(&df->lock);
		df->max_boost = true;
		update_devfreq(df);
		mutex_unlock(&df->lock);
	}

	spin_lock_irqsave(&b->lock, flags);
	boost_jiffies = b->max_boost_jiffies;
	spin_unlock_irqrestore(&b->lock, flags);

	queue_delayed_work(b->wq, &b->max_unboost, boost_jiffies);
}

static void devfreq_max_unboost(struct work_struct *work)
{
	struct boost_dev *b = container_of(to_delayed_work(work),
					   typeof(*b), max_unboost);
	struct devfreq *df = b->df;

	mutex_lock(&df->lock);
	df->max_boost = false;
	update_devfreq(df);
	mutex_unlock(&df->lock);
}

static int msm_drm_notifier_cb(struct notifier_block *nb,
			       unsigned long action, void *data)
{
	struct df_boost_drv *d = container_of(nb, typeof(*d), msm_drm_notif);
	struct msm_drm_notifier *evdata = data;
	int *blank = evdata->data;

	/* Parse framebuffer blank events as soon as they occur */
	if (action != MSM_DRM_EARLY_EVENT_BLANK)
		return NOTIFY_OK;

	/* Boost when the screen turns on and unboost when it turns off */
	d->screen_awake = *blank == MSM_DRM_BLANK_UNBLANK;
	if (d->screen_awake) {
		int i;

		for (i = 0; i < DEVFREQ_MAX; i++)
			__devfreq_boost_kick_max(d->devices + i,
				CONFIG_DEVFREQ_WAKE_BOOST_DURATION_MS);
	} else {
		devfreq_unboost_all(d);
	}

	return NOTIFY_OK;
}

static void devfreq_boost_input_event(struct input_handle *handle,
				      unsigned int type, unsigned int code,
				      int value)
{
	struct df_boost_drv *d = handle->handler->private;
	int i;

	if (!d->screen_awake)
		return;

	for (i = 0; i < DEVFREQ_MAX; i++)
		__devfreq_boost_kick(d->devices + i);
}

static int devfreq_boost_input_connect(struct input_handler *handler,
				       struct input_dev *dev,
				       const struct input_device_id *id)
{
	struct input_handle *handle;
	int ret;

	handle = kzalloc(sizeof(*handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "devfreq_boost_handle";

	ret = input_register_handle(handle);
	if (ret)
		goto free_handle;

	ret = input_open_device(handle);
	if (ret)
		goto unregister_handle;

	return 0;

unregister_handle:
	input_unregister_handle(handle);
free_handle:
	kfree(handle);
	return ret;
}

static void devfreq_boost_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id devfreq_boost_ids[] = {
	/* Multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			BIT_MASK(ABS_MT_POSITION_X) |
			BIT_MASK(ABS_MT_POSITION_Y) }
	},
	/* Touchpad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_KEYBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.keybit = { [BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
		.absbit = { [BIT_WORD(ABS_X)] =
			BIT_MASK(ABS_X) | BIT_MASK(ABS_Y) }
	},
	/* Keypad */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT,
		.evbit = { BIT_MASK(EV_KEY) }
	},
	{ }
};

static struct input_handler devfreq_boost_input_handler = {
	.event		= devfreq_boost_input_event,
	.connect	= devfreq_boost_input_connect,
	.disconnect	= devfreq_boost_input_disconnect,
	.name		= "devfreq_boost_handler",
	.id_table	= devfreq_boost_ids
};

static int __init devfreq_boost_init(void)
{
	struct df_boost_drv *d;
	struct workqueue_struct *wq;
	int i, ret;

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;

	wq = alloc_workqueue("devfreq_boost_wq", WQ_HIGHPRI, 0);
	if (!wq) {
		ret = -ENOMEM;
		goto free_d;
	}

	for (i = 0; i < DEVFREQ_MAX; i++) {
		struct boost_dev *b = d->devices + i;

		b->wq = wq;
		b->abs_min_freq = ULONG_MAX;
		spin_lock_init(&b->lock);
		INIT_WORK(&b->input_boost, devfreq_input_boost);
		INIT_DELAYED_WORK(&b->input_unboost, devfreq_input_unboost);
		INIT_WORK(&b->max_boost, devfreq_max_boost);
		INIT_DELAYED_WORK(&b->max_unboost, devfreq_max_unboost);
	}

	d->devices[DEVFREQ_MSM_CPUBW].boost_freq =
		CONFIG_DEVFREQ_MSM_CPUBW_BOOST_FREQ;

	devfreq_boost_input_handler.private = d;
	ret = input_register_handler(&devfreq_boost_input_handler);
	if (ret) {
		pr_err("Failed to register input handler, err: %d\n", ret);
		goto destroy_wq;
	}

	d->msm_drm_notif.notifier_call = msm_drm_notifier_cb;
	d->msm_drm_notif.priority = INT_MAX;
	ret = msm_drm_register_client(&d->msm_drm_notif);
	if (ret) {
		pr_err("Failed to register msm_drm notifier, err: %d\n", ret);
		goto unregister_handler;
	}

	df_boost_drv_g = d;

	return 0;

unregister_handler:
	input_unregister_handler(&devfreq_boost_input_handler);
destroy_wq:
	destroy_workqueue(wq);
free_d:
	kfree(d);
	return ret;
}
subsys_initcall(devfreq_boost_init);
