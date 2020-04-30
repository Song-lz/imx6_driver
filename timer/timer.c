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
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

struct timer_dev {
	dev_t devid;				/* 设备号 */
	struct cdev cdev;			/* cdev */
	int major;					/* 主设备号 */
	int minor;					/* 次设备号 */
	struct timer_list timer;	/* 定时器 */
	int timer_period;			/* 定时器回调周期 */
};

struct timer_dev timer_device;		/* timer设备 */

/*
 * @description		: 驱动打开执行函数 
 * @param - inode	: 传递给驱动的inode
 * @param - filep	: 设备文件结构体指针
 * @return			: 0 成功; 其他 失败
 * */
static int timer_open(struct inode *inode, struct file *filep)
{
	filep->private_data = &timer_device;		/* 将timer设备文件地址赋给文件私有指针 */
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
static ssize_t timer_read(struct file *filp, char __user *buff, size_t cnt, loff_t *offt)
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
static ssize_t timer_write(struct file *filp, char __user *buff, size_t cnt, loff_t *offt)
{
	return 0;
}

/*
 * @description		: 驱动关闭执行函数 
 * @param - inode	: 传递给驱动的inode
 * @param - filep	: 设备文件结构体指针
 * @return			: 0 成功; 其他 失败
 * */
static int timer_release(struct inode *inode, struct file *filep)
{
	return 0;
}


/* 操作函数结构体 */
static struct file_operations timer_fops = {
	.owner = THIS_MODULE,
	.open = timer_open,
	.read = timer_read,
	.write = timer_write,
	.release = timer_release,
};

static void timer_calback(unsigned long param)
{
	static int sec_count = 0;
	printk("Timer run %d second!\n", sec_count++);
	mod_timer(&timer_device.timer, jiffies + msecs_to_jiffies(timer_device.timer_period));
}

/*
 * @description		: 驱动加载执行函数 
 * @param			: 无
 * @return			: 无
 * */
static __init int timer_init(void)
{
	/* 注册字符设备驱动 */
	/* 1.创建设备号 */
	alloc_chrdev_region(&timer_device.devid, 0, 1, "timer");
	timer_device.major = MAJOR(timer_device.devid);
	timer_device.minor = MINOR(timer_device.devid);
	
	/* 2.初始化cdev */
	timer_device.cdev.owner = THIS_MODULE;
	cdev_init(&timer_device.cdev, &timer_fops);
	/* 3.添加一个cdev */
	cdev_add(&timer_device.cdev, timer_device.devid, 1);

	/* 4.初始化并启动一个timer */
	timer_device.timer_period = 1 * HZ;
	init_timer(&timer_device.timer);
	timer_device.timer.function = timer_calback;
	timer_device.timer.data = (unsigned long)&timer_device;
	timer_device.timer.expires = jiffies + msecs_to_jiffies(timer_device.timer_period);
	add_timer(&timer_device.timer);
	return 0;
}

/*
 * @description		:驱动卸载执行函数
 * @param			:无
 * @return			:无
 */
static __exit void timer_exit(void)
{
	del_timer(&timer_device.timer);
	cdev_del(&timer_device.cdev);
	unregister_chrdev_region(timer_device.devid, 1);
}

module_init(timer_init);
module_exit(timer_exit);


MODULE_AUTHOR("eurphan<eurphan@163.com>");
MODULE_DESCRIPTION("Timer test driver");
MODULE_LICENSE("GPL");

