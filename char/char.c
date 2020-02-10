#include <linux/module.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/ide.h>
#include <linux/device.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

struct char_class {
	dev_t devid;				/* 设备号 */
	struct cdev cdev;			/* cdev */
	int major;					/* 主设备号 */
	int minor;					/* 次设备号 */
};

struct char_class char_dev;		/* char设备 */

static int char_open(struct inode *inode, struct file *filep)
{
	filep->private_data = &char_dev;		/* 将char设备文件地址赋给文件私有指针 */
	return 0;
}

static ssize_t char_read(struct file *filp, char __user *buff, size_t cnt, loff_t *offt)
{
	return 0;
}

static ssize_t char_write(struct file *filp, char __user *buff, size_t cnt, loff_t *offt)
{
	return 0;
}

static int char_release(struct inode *inode, struct file *filep)
{
	return 0;
}

/* 操作函数结构体 */
static struct file_operations char_fops = {
	.owner = THIS_MODULE,
	.open = char_open,
	.read = char_read,
	.write = char_write,
	.release = char_release,
};

static __init int char_init(void)
{
	/* 创建设备号 */
	if (char_dev.major) { 
		/* 已经定义了主设备号，则直接注册 */
		char_dev.devid = MKDEV(char_dev.major, 0);
		register_chrdev_region(char_dev.devid, 1, "char");
	} else {
		/* 没有定义主设备号，则自动申请 */
		alloc_chrdev_region(&char_dev.devid, 0, 1, "char");
		char_dev.major = MAJOR(char_dev.devid);
		char_dev.minor = MINOR(char_dev.devid);
	}
	printk("char major is %d, minor is %d!\n", char_dev.major, char_dev.minor);
	/* 2.初始化cdev */
	char_dev.cdev.owner = THIS_MODULE;
	cdev_init(&char_dev.cdev, &char_fops);
	/* 3.添加一个cdev */
	cdev_add(&char_dev.cdev, char_dev.devid, 1);
	return 0;
}

static __exit void char_exit(void)
{
	cdev_del(&char_dev.cdev);
	unregister_chrdev_region(char_dev.devid, 1);
}

module_init(char_init);
module_exit(char_exit);

MODULE_AUTHOR("eurphan<eurphan@163.com>");
MODULE_DESCRIPTION("Char Driver");
MODULE_LICENSE("GPL");

