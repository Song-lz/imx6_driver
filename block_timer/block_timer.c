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
#include <linux/wait.h>
#include <linux/sched.h>

struct timer_dev {
	dev_t devid;				/* 设备号 */
	struct cdev cdev;			/* cdev */
	struct class *class;		/* 类 */
	struct device *device;		/* 设备 */
	int major;					/* 主设备号 */
	int minor;					/* 次设备号 */
	struct timer_list timer;
	int timer_period;
	int read_flag;
	wait_queue_head_t r_head;	/* 等待队列头 */
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
	int ret;

#if 0 
	DECLARE_WAITQUEUE(wait, current); /* 定义等待队列 */
	add_wait_queue(&timer_device.r_head, &wait); /* 添加等待队列到等待队列头 */
	while (timer_device.read_flag == 0) {
		if (filp->f_flags & O_NONBLOCK) {
			ret = -EAGAIN;
			goto out1;
		}
		__set_current_state(TASK_INTERRUPTIBLE);
		schedule();
		if (signal_pending(current)) {
			ret = -ERESTARTSYS;
			goto out1;
		}
	}
	if (copy_to_user(buff, &timer_device.read_flag, 1)) {
		ret = -EFAULT;
		goto out1;
	}
	else {
		timer_device.read_flag = 0;
		ret = 1;
	}
#else
	if (timer_device.read_flag)
	{
		if (copy_to_user(buff, &timer_device.read_flag, 1)) {
			ret = -EFAULT;
			goto out2;
		}
		else {
			timer_device.read_flag = 0;
			ret = 1;
		}
	}
	else
	{
		ret = wait_event_interruptible(timer_device.r_head, timer_device.read_flag);
		if (ret) {
			ret = -EFAULT;
			goto out2;
		}
		else {
			if (copy_to_user(buff, &timer_device.read_flag, 1)) {
				ret = -EFAULT;
				goto out2;
			}
			else {
				timer_device.read_flag = 0;
				ret = 1;
			}
		}
	}
#endif
out1:
#if 0
	remove_wait_queue(&timer_device.r_head, &wait);
	set_current_state(TASK_RUNNING);
#endif
out2:
	return ret;
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
//	printk("Timer run %d second!\n", sec_count++);
	mod_timer(&timer_device.timer, jiffies + msecs_to_jiffies(timer_device.timer_period));
	timer_device.read_flag = 1;
	wake_up_interruptible(&timer_device.r_head);
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
	timer_device.timer_period = 1000;
	init_timer(&timer_device.timer);
	timer_device.timer.function = timer_calback;
	timer_device.timer.data = (unsigned long)&timer_device;
	timer_device.timer.expires = jiffies + msecs_to_jiffies(timer_device.timer_period);
	add_timer(&timer_device.timer);

	/* 5.初始化等待队列头 */
	timer_device.read_flag = 0;
	init_waitqueue_head(&timer_device.r_head);
	/* 6.创建类 */
	timer_device.class = class_create(THIS_MODULE, "timer");
	if (IS_ERR(timer_device.class)) {
		return PTR_ERR(timer_device.class);
	}
	/* 7.创建设备 */
	timer_device.device = device_create(timer_device.class, NULL, timer_device.devid, NULL, "timer");
	if (IS_ERR(timer_device.device)) {
		return PTR_ERR(timer_device.device);
	}
	return 0;
}

/*
 * @description		:驱动卸载执行函数
 * @param			:无
 * @return			:无
 */
static __exit void timer_exit(void)
{
	del_timer_sync(&timer_device.timer);
	cdev_del(&timer_device.cdev);
	unregister_chrdev_region(timer_device.devid, 1);
}

module_init(timer_init);
module_exit(timer_exit);


MODULE_AUTHOR("eurphan<eurphan@163.com>");
MODULE_DESCRIPTION("Timer test driver");
MODULE_LICENSE("GPL");

