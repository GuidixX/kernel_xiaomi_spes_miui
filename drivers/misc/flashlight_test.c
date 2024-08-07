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
};

static struct flashlight_data *flash_data;

static ssize_t flash_on_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int val = gpio_get_value(flash_data->flash_en);
    return sprintf(buf, "%d\n", val);
}

static ssize_t flash_on_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int ret;
    unsigned long val;

    ret = kstrtoul(buf, 10, &val);
    if (ret)
        return ret;

    gpio_set_value(flash_data->flash_en, val);
    return count;
}

static struct kobj_attribute flash_on_attribute = __ATTR(flash_on, 0664, flash_on_show, flash_on_store);

static int flashlight_probe(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;
    int ret;

    flash_data = devm_kzalloc(&pdev->dev, sizeof(*flash_data), GFP_KERNEL);
    if (!flash_data)
        return -ENOMEM;

    flash_data->flash_en = of_get_named_gpio(np, "qcom,flash-gpios", 0);
    if (!gpio_is_valid(flash_data->flash_en)) {
        dev_err(&pdev->dev, "Invalid GPIO for flash_en\n");
        return -EINVAL;
    }

    ret = devm_gpio_request_one(&pdev->dev, flash_data->flash_en, GPIOF_OUT_INIT_LOW, "flash_en");
    if (ret) {
        dev_err(&pdev->dev, "Failed to request flash_en GPIO\n");
        return ret;
    }

    flash_data->flash_now = of_get_named_gpio(np, "qcom,flash-gpios", 1);
    if (!gpio_is_valid(flash_data->flash_now)) {
        dev_err(&pdev->dev, "Invalid GPIO for flash_now\n");
        return -EINVAL;
    }

    ret = devm_gpio_request_one(&pdev->dev, flash_data->flash_now, GPIOF_OUT_INIT_LOW, "flash_now");
    if (ret) {
        dev_err(&pdev->dev, "Failed to request flash_now GPIO\n");
        return ret;
    }

    flash_data->kobj = kobject_create_and_add("flashlight", kernel_kobj);
    if (!flash_data->kobj)
        return -ENOMEM;

    ret = sysfs_create_file(flash_data->kobj, &flash_on_attribute.attr);
    if (ret) {
        kobject_put(flash_data->kobj);
        return ret;
    }

    dev_info(&pdev->dev, "Flashlight driver initialized\n");
    return 0;
}

static int flashlight_remove(struct platform_device *pdev)
{
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
MODULE_DESCRIPTION("Flashlight driver with sysfs interface");
