#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ide.h>
#include <linux/device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <linux/platform_device.h>
#include <asm/io.h>

struct led_dev {
	dev_t devid;				/* 设备号 */
	struct cdev cdev;			/* cdev */
	struct class *class;		/* 类 */
	struct device *device;		/* 设备 */
	int major;					/* 主设备号 */
	int minor;					/* 次设备号 */
	struct device_node *nd;		/* 设备节点 */
	int led_gpio;				/* led使用的gpio编号 */
};

struct led_dev led;		/* led设备 */

/*
 * @description		: 驱动打开执行函数 
 * @param - inode	: 传递给驱动的inode
 * @param - filep	: 设备文件结构体指针
 * @return			: 0 成功; 其他 失败
 * */
static int led_open(struct inode *inode, struct file *filep)
{
	filep->private_data = &led;		/* 将led设备文件地址赋给文件私有指针 */
	return 0;
}

/*
 * @description		: 从设备读取数据函数`
 * @param - filep	: 设备文件结构体指针
 * @param - buff	: 返回数据给用户空间的数据缓存
 * @param - cnt		: 需要读取的数据个数
 * @param - offt	: 读取位置相对于首地址的偏移
 * @return			: 实际读取的字节数，如果为负数代表读取失败
 * */
static ssize_t led_read(struct file *filp, char __user *buff, size_t cnt, loff_t *offt)
{
	return 0;
}

/*
 * @description		: 向设备写入数据函数`
 * @param - filep	: 设备文件结构体指针
 * @param - buff	: 需要写入设备文件的用户空间数据缓存
 * @param - cnt		: 需要写入的数据个数
 * @param - offt	: 写入位置相对于首地址的偏移
 * @return			: 实际写入的字节数，如果为负数代表写入失败
 * */
static ssize_t led_write(struct file *filp, char __user *buff, size_t cnt, loff_t *offt)
{
	int ret;
	char rev_buff[1];
	struct led_dev* dev = filp->private_data;

	ret = copy_from_user(rev_buff, buff, 1);
	if (ret < 0) {
		dev_dbg(&dev->device, "led control failure!\n");
		return -EFAULT;
	}
	if (rev_buff[0] == '1')
		gpio_set_value(dev->led_gpio, 1);
	else if(rev_buff[0] == '0')
		gpio_set_value(dev->led_gpio, 0);
	else {
		dev_dbg(&dev->device, "led control illegal parameter!\n");
		return -EFAULT;
	}
	return 0;
}

/*
 * @description		: 驱动关闭执行函数 
 * @param - inode	: 传递给驱动的inode
 * @param - filep	: 设备文件结构体指针
 * @return			: 0 成功; 其他 失败
 * */
static int led_release(struct inode *inode, struct file *filep)
{
	return 0;
}

/* 操作函数结构体 */
static struct file_operations led_fops = {
	.owner = THIS_MODULE, 
	.open = led_open,
	.read = led_read,
	.write = led_write,
	.release = led_release,
};

/*
 * @description		: platform设备和驱动匹配执行函数
 * @param - dev		: platform设备
 * @return			: 0 成功; 其他 失败
 */
static int led_probe(struct platform_device* pdev)
{
	int ret;
	
	/* 设置LED使用的GPIO */
	/* 1.获取设备节点 */
	led.nd = of_find_node_by_path("/user_leds/led0");
	if (led.nd == NULL) {
		dev_info(&pdev->dev, "led node not find!\n");
		return -EINVAL;
	}

	/* 2.获取节点中的gpio属性 */
	led.led_gpio = of_get_named_gpio(led.nd, "gpio", 0);
	if (led.led_gpio < 0) {
		dev_info(&pdev->dev, "can't get led gpio!\n");
		return -EINVAL;
	} else {
		dev_dbg(&pdev->dev, "led-gpio num is %d.\n", led.led_gpio);
	}
	/* 3.设置gpio为输出，且输出低电平 */
	ret = gpio_direction_output(led.led_gpio, 0);
	if (ret < 0) {
		dev_info(&pdev->dev, "can't set the gpio!\n");
	}
	/* 注册字符设备驱动 */
	/* 创建设备号 */
	if (led.major) { 
		/* 已经定义了主设备号，则直接注册 */
		led.devid = MKDEV(led.major, 0);
		register_chrdev_region(led.devid, 1, "led");
	} else {
		/* 没有定义主设备号，则自动申请 */
		alloc_chrdev_region(&led.devid, 0, 1, "led");
		led.major = MAJOR(led.devid);
		led.minor = MINOR(led.devid);
	}
	dev_info(&pdev->dev, "led device major is %d, minor is %d!\n", led.major, led.minor);
	/* 2.初始化cdev */
	led.cdev.owner = THIS_MODULE;
	cdev_init(&led.cdev, &led_fops);
	/* 3.添加一个cdev */
	cdev_add(&led.cdev, led.devid, 1);
	/* 4.创建类 */
	led.class = class_create(THIS_MODULE, "led");
	if (IS_ERR(led.class)) {
		return PTR_ERR(led.class);
	}
	/* 5.创建设备 */
	led.device = device_create(led.class, NULL, led.devid, NULL, "led");
	if (IS_ERR(led.device)) {
		return PTR_ERR(led.device);
	}
	return 0;
}

/*
 * @description		: platform设备和驱动解除匹配执行函数
 * @param - dev		: platform设备
 * @return			: 0 成功; 其他 失败
 */
static int led_remove(struct platform_device* pdev)
{
	cdev_del(&led.cdev);
	unregister_chrdev_region(led.devid, 1);
	device_destroy(led.class, led.devid);
	class_destroy(led.class);
	return 0;
}

/* 用于device和driver匹配 */
static const struct of_device_id led_of_match[] = {
	{ .compatible = "user_leds" },
	{ /* sentinel */}
};
MODULE_DEVICE_TABLE(of, led_of_match);

/* platform_driver结构 */
static struct platform_driver led_platform_driver = {
	.driver = {
		.name = "user_leds",
		.of_match_table = led_of_match,
	},
	.probe = led_probe,
	.remove = led_remove,
};

/*
 * @description		: 驱动加载执行函数 
 * @param			: 无
 * @return			: 是否加载成功
 * */
static __init int led_init(void)
{
	platform_driver_register(&led_platform_driver);
	return 0;
}

/*
 * @description		:驱动卸载执行函数
 * @param			:无
 * @return			:无
 */
static __exit void led_exit(void)
{
	platform_driver_unregister(&led_platform_driver);
}

module_init(led_init);
module_exit(led_exit);


MODULE_AUTHOR("eurphan<eurphan@163.com>");
MODULE_DESCRIPTION("LED Driver");
MODULE_LICENSE("GPL");

