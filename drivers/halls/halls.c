#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/pm.h>
#include <linux/input.h>

#define HALLS_DEVICE_NAME "halls"
#define HALLS_GPIO_FIRST 49
#define HALLS_GPIO_SECOND 113

enum {
	KEYCODE_SLIDER_UP = 594,
	KEYCODE_SLIDER_DOWN,
};

enum {
	STATE_SLIDER_UP = 0,
	STATE_SLIDER_DOWN,
	STATE_SLIDER_SLIDING,
};

struct halls_data {
	struct delayed_work notify_work;
	struct wakeup_source *wakelock;
	struct input_dev *input;

	int first_gpio;
	int second_gpio;
};

extern int elliptic_set_hall_state(int state);
static void halls_send_input(struct halls_data *hdata, int state) {
	int keycode;

	if (state == STATE_SLIDER_UP)
		keycode = KEYCODE_SLIDER_UP;
	else if (state == STATE_SLIDER_DOWN)
		keycode = KEYCODE_SLIDER_DOWN;
	else
		return;

	input_report_key(hdata->input, keycode, 1);
	input_sync(hdata->input);
	input_report_key(hdata->input, keycode, 0);
	input_sync(hdata->input);
}

static void halls_notify_work_func(struct work_struct *work) {
	struct halls_data *hdata = container_of(to_delayed_work(work),
			struct halls_data, notify_work);
	int first_value, second_value;
	int state;

	first_value = gpio_get_value_cansleep(hdata->first_gpio);
	second_value = gpio_get_value_cansleep(hdata->second_gpio);

	if (first_value == 1 && second_value == 0)
		state = STATE_SLIDER_UP;
	else if (first_value == 0 && second_value == 1)
		state = STATE_SLIDER_DOWN;
	else
		state = STATE_SLIDER_SLIDING;

	elliptic_set_hall_state(state);
	halls_send_input(hdata, state);
}

static irqreturn_t halls_irq_handler(int irqno, void *dev_id) {
	struct halls_data *hdata = dev_id;
	__pm_wakeup_event(hdata->wakelock, 20);
	schedule_delayed_work(&hdata->notify_work, 0);
	return 0;
}

static int halls_configure_irq(struct halls_data *hdata, int gpio, const char *label) {
	int ret, irq;

	ret = gpio_request(gpio, label);
	if (ret) {
		pr_err("%s: failed to request GPIO %d\n", __func__, gpio);
		return ret;
	}

	ret = gpio_direction_input(gpio);
	if (ret) {
		pr_err("%s: failed to set GPIO %d direction to input\n", __func__, gpio);
		return ret;
	}

	irq = gpio_to_irq(gpio);
	ret = request_irq(irq, halls_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING
			| IRQF_ONESHOT, label, hdata);
	if (ret) {
		pr_err("%s: failed to configure GPIO %d IRQ\n", __func__, gpio);
		return ret;
	}

	irq_set_irq_wake(irq, 1);

	return 0;
}

static int __init halls_init(void) {
	struct halls_data *hdata;
	int ret;

	hdata = kzalloc(sizeof(*hdata), GFP_KERNEL);
	if (!hdata) {
		pr_err("%s: failed to allocate memory for driver data\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	hdata->input = input_allocate_device();
	if (!hdata->input) {
		pr_err("%s: failed to allocate memory for input device\n", __func__);
		ret = -ENOMEM;
		goto free_halls;
	}

	hdata->input->name = HALLS_DEVICE_NAME;
	input_set_drvdata(hdata->input, hdata);

	set_bit(EV_KEY, hdata->input->evbit);
	set_bit(KEYCODE_SLIDER_UP, hdata->input->keybit);
	set_bit(KEYCODE_SLIDER_DOWN, hdata->input->keybit);

	ret = input_register_device(hdata->input);
	if (ret) {
		pr_err("%s: failed to register input device\n", __func__);
		goto free_input;
	}

	hdata->first_gpio = HALLS_GPIO_FIRST;
	hdata->second_gpio = HALLS_GPIO_SECOND;

	ret = halls_configure_irq(hdata, hdata->first_gpio, "halls-first");
	if (ret) {
		goto unregister_input;
	}

	ret = halls_configure_irq(hdata, hdata->second_gpio, "halls-second");
	if (ret) {
		goto unregister_input;
	}

	INIT_DELAYED_WORK(&hdata->notify_work, halls_notify_work_func);

	hdata->wakelock = wakeup_source_register("halls-ws");
	if (!hdata->wakelock) {
		pr_err("%s: failed to register wakeup source\n", __func__);
		ret = -EINVAL;
		goto unregister_input;
	}

	return 0;

unregister_input:
	input_unregister_device(hdata->input);
free_input:
	input_free_device(hdata->input);
free_halls:
	kfree(hdata);
exit:
	return ret;
}

device_initcall(halls_init);

MODULE_LICENSE("GPL v2");
