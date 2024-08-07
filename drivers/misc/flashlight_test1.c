#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kobject.h>
#include <linux/of_gpio.h>

struct flashlight_data {
    int flash_en;
    int flash_now;
    struct kobject *kobj;
    struct platform_device *pdev;
};

static struct flashlight_data *flash_data;

static ssize_t flash_on_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int val = gpio_get_value(flash_data->flash_en);
    pr_info("flash_on_show: GPIO value = %d\n", val);
    return sprintf(buf, "%d\n", val);
}

static ssize_t flash_on_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int ret;
    unsigned long val;

    ret = kstrtoul(buf, 10, &val);
    if (ret) {
        pr_err("flash_on_store: kstrtoul error %d\n", ret);
        return ret;
    }

    pr_info("flash_on_store: Setting GPIO value to %ld\n", val);
    gpio_set_value(flash_data->flash_en, val); // No need to assign to ret

    return count;
}


static struct kobj_attribute flash_on_attribute = __ATTR(flash_on, 0664, flash_on_show, flash_on_store);

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

    flash_data->kobj = kobject_create_and_add("flashlight", kernel_kobj);
    if (!flash_data->kobj) {
        pr_err("Failed to create kobject\n");
        return -ENOMEM;
    }

    ret = sysfs_create_file(flash_data->kobj, &flash_on_attribute.attr);
    if (ret) {
        pr_err("Failed to create sysfs file: %d\n", ret);
        kobject_put(flash_data->kobj);
        return ret;
    }

    pr_info("Flashlight driver initialized with flash_en GPIO %d and flash_now GPIO %d\n",
            flash_data->flash_en, flash_data->flash_now);
    return 0;
}

static int flashlight_remove(struct platform_device *pdev)
{
    pr_info("Removing flashlight driver\n");
    kobject_put(flash_data->kobj);
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
MODULE_DESCRIPTION("Flashlight driver with sysfs interface and enhanced debugging");
