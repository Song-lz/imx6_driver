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
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/of_irq.h>
#include <linux/irq.h>

struct button_dev {
	int gpio;
	struct timer_list timer;
	struct device_node *nd;
	int irq;
	irqreturn_t (*handler)(int, void*);
	struct work_struct work;
};

struct button_dev button_device;		/* button设备 */

/*
 * @description		: 驱动打开执行函数 
 * @param - inode	: 传递给驱动的inode
 * @param - filep	: 设备文件结构体指针
 * @return			: 0 成功; 其他 失败
 */
static int button_open(struct inode *inode, struct file *filep)
{
	filep->private_data = &button_device;		/* 将button设备文件地址赋给文件私有指针 */
	return 0;
}

/*
 * @description		: 从设备读取数据函数`
 * @param - filep	: 设备文件结构体指针
 * @param - buff	: 返回数据给用户空间的数据缓存
 * @param - cnt		: 需要读取的数据个数
 * @param - offt	: 读取位置相对于首地址的偏移
 * @return			: 实际读取的字节数，如果为负数代表读取失败
 */
static ssize_t button_read(struct file *filp, char __user *buff, size_t cnt, loff_t *offt)
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
 */
static ssize_t button_write(struct file *filp, char __user *buff, size_t cnt, loff_t *offt)
{
	return 0;
}

/*
 * @description		: 驱动关闭执行函数 
 * @param - inode	: 传递给驱动的inode
 * @param - filep	: 设备文件结构体指针
 * @return			: 0 成功; 其他 失败
 */
static int button_release(struct inode *inode, struct file *filep)
{
	return 0;
}

/*
 * @description		: 定时器回调
 * @param - param	: 参数 
 * @return			: 无
 */
static void timer_callback(unsigned long param)
{
	static int count = 0;
	struct button_dev *dev = (struct button_dev *)param;

	if (gpio_get_value(dev->gpio) == 0) {
		count++;
		printk("button pressed %d times.\n", count);
	}
}

/*
 * @description		: work工作函数
 * @param - work	: work结构 
 * @return			: 无
 */
static void work_func(struct work_struct *work)
{
	struct button_dev *dev = container_of(work, struct button_dev, work);
	dev->timer.data = (volatile unsigned long)dev;
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(20));
}

/*
 * @description		: 按键中断函数 
 * @param - irq		: 中断号
 * @param - dev_id	: 设备结构 
 * @return			: 中断执行结果
 */
static irqreturn_t button_handler(int irq, void *dev_id)
{
	struct button_dev *dev = (struct button_dev *)dev_id;
	schedule_work(&dev->work); /* 调用work */
	return IRQ_RETVAL(IRQ_HANDLED);
}

/* 操作函数结构体 */
static struct file_operations button_fops = {
	.owner = THIS_MODULE,
	.open = button_open,
	.read = button_read,
	.write = button_write,
	.release = button_release,
};

/* misc device结构 */
static struct miscdevice button_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "button",
	.fops = &button_fops,
};

/*
 * @description		: 设备、驱动配对执行函数 
 * @param			: dev:设备
 * @return			: 无
 */
static int button_probe(struct platform_device* pdev)
{
	int ret;

	/* 查找设备树节点 */
	button_device.nd = of_find_node_by_path("/user_button");
	if (button_device.nd == NULL) {
		dev_info(&pdev->dev, "cannot find the device tree patch.\n");
		return -EINVAL;
	}
	/* 获取gpio */
	button_device.gpio = of_get_named_gpio(button_device.nd, "button-gpio", 0);
	if (button_device.gpio < 0) {
		dev_info(&pdev->dev, "cannot get the gpio.\n");
		return -EINVAL;
	}
	/* 请求并设置gpio为输入 */
	gpio_request(button_device.gpio, "user_button");
	gpio_direction_input(button_device.gpio);
#if 1
	button_device.irq = gpio_to_irq(button_device.gpio);
#else
	button_device.irq = irq_of_parse_and_map(button_device.nd, 0);
#endif
	dev_dbg(&pdev->dev, "request gpio%d  and irq%d for the button.", \
			button_device.gpio, button_device.irq);

	/* 申请中断 */
	button_device.handler = button_handler;
	ret = request_irq(button_device.irq, button_device.handler, IRQF_TRIGGER_RISING, \
			"button", &button_device);
	if (ret < 0) {
		dev_info(&pdev->dev, "irq %d request failed.", button_device.irq);
		return -EFAULT;
	}

	/* 初始化定时器 */
	init_timer(&button_device.timer);
	button_device.timer.function = timer_callback;

	/* 初始化work */
	INIT_WORK(&button_device.work, work_func);

	/* 注册misc设备驱动 */
	ret = misc_register(&button_miscdev);
	if (ret < 0) {
		printk("Failed to register timer misc driver!\n");
		return -EFAULT;
	}
	return 0;
}

/*
 * @description		: 设备、驱动移除执行函数 
 * @param			: dev:设备
 * @return			: 无
 */
static int button_remove(struct platform_device* dev)
{
	free_irq(button_device.irq, &button_device);
	misc_deregister(&button_miscdev);
	return 0;
}

/* 设备树匹配结构 */
static const struct of_device_id button_of_match[] = {
	{.compatible = "user_button"},
	{},
};

/* platform driver结构 */
static struct platform_driver button_driver = {
	.driver = {
		.name = "user_button",
		.of_match_table = button_of_match,
	},
	.probe = button_probe,
	.remove = button_remove,
};

/*
 * @description		: 驱动加载执行函数 
 * @param			: 无
 * @return			: 无
 */
static __init int button_init(void)
{
	platform_driver_register(&button_driver);
	return 0;
}

/*
 * @description		:驱动卸载执行函数
 * @param			:无
 * @return			:无
 */
static __exit void button_exit(void)
{
	platform_driver_unregister(&button_driver);
}

module_init(button_init);
module_exit(button_exit);


MODULE_AUTHOR("eurphan<eurphan@163.com>");
MODULE_DESCRIPTION("Button interrupt test driver");
MODULE_LICENSE("GPL");

