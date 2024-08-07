#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/leds.h>

struct flashlight_data {
    int flash_en;
    int flash_now;
    struct kobject *kobj;
    struct platform_device *pdev;
    struct pinctrl *pinctrl;
    struct pinctrl_state *gpio_state_active;
    struct pinctrl_state *gpio_state_suspend;
    struct led_classdev cdev;
};

static struct flashlight_data *flash_data;

static void flashlight_brightness_set(struct led_classdev *cdev, enum led_brightness brightness)
{
    int val = (brightness > 0) ? 1 : 0;
    gpio_set_value(flash_data->flash_now, val);
    pr_info("flashlight_brightness_set: Setting GPIO value to %d\n", val);
}

static int flashlight_probe(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;
    int ret;

    flash_data = devm_kzalloc(&pdev->dev, sizeof(*flash_data), GFP_KERNEL);
    if (!flash_data) {
        pr_err("Failed to allocate memory for flashlight_data\n");
        return -ENOMEM;
    }

    flash_data->pdev = pdev;

    flash_data->flash_en = of_get_named_gpio(np, "qcom,flash-gpios", 0);
    if (!gpio_is_valid(flash_data->flash_en)) {
        pr_err("Invalid GPIO for flash_en: %d\n", flash_data->flash_en);
        return -EINVAL;
    }

    ret = devm_gpio_request_one(&pdev->dev, flash_data->flash_en, GPIOF_OUT_INIT_LOW, "flash_en");
    if (ret) {
        pr_err("Failed to request flash_en GPIO: %d\n", ret);
        return ret;
    }

    flash_data->flash_now = of_get_named_gpio(np, "qcom,flash-gpios", 1);
    if (!gpio_is_valid(flash_data->flash_now)) {
        pr_err("Invalid GPIO for flash_now: %d\n", flash_data->flash_now);
        return -EINVAL;
    }

    ret = devm_gpio_request_one(&pdev->dev, flash_data->flash_now, GPIOF_OUT_INIT_LOW, "flash_now");
    if (ret) {
        pr_err("Failed to request flash_now GPIO: %d\n", ret);
        return ret;
    }

    flash_data->cdev.name = "led:torch";
    flash_data->cdev.brightness = 0;
    flash_data->cdev.max_brightness = 1;
    flash_data->cdev.brightness_set = flashlight_brightness_set;

    ret = led_classdev_register(&pdev->dev, &flash_data->cdev);
    if (ret) {
        pr_err("Failed to register LED class device: %d\n", ret);
        return ret;
    }

    pr_info("Flashlight driver initialized with flash_en GPIO %d and flash_now GPIO %d\n",
            flash_data->flash_en, flash_data->flash_now);
    return 0;
}

static int flashlight_remove(struct platform_device *pdev)
{
    pr_info("Removing flashlight driver\n");
    led_classdev_unregister(&flash_data->cdev);
    return 0;
}

static const struct of_device_id flashlight_of_match[] = {
    { .compatible = "qcom,camera-flash", },
    {},
};
MODULE_DEVICE_TABLE(of, flashlight_of_match);

static struct platform_driver flashlight_driver = {
    .driver = {
        .name = "flashlight",
        .of_match_table = flashlight_of_match,
    },
    .probe = flashlight_probe,
    .remove = flashlight_remove,
};

module_platform_driver(flashlight_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HALt");
MODULE_DESCRIPTION("Flashlight driver with sysfs interface and LED class integration");
