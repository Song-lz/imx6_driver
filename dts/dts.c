#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/of.h>

/* 模块加载函数 */
static int __init dts_init(void)
{
	int ret = 0;
	struct device_node* bl_nd;
	struct property *compatible, *status;
	u32* bright_val;
	u32 brightlevel_size;
	u32 i;

	/* 查找节点 */
	bl_nd = of_find_node_by_path("/backlight");
	if (bl_nd == NULL) {
		ret = -EINVAL;
		return 0;
	}

	/* 获取compatible属性 */
	compatible = of_find_property(bl_nd, "compatible", NULL);
	if (compatible == NULL) {
		ret = -EINVAL;
		goto comp_fail;
	} else {
		printk("compatible = %s\n", (char*)compatible->value);
	}
	/* 获取背光等级个数 */
	brightlevel_size = of_property_count_elems_of_size(bl_nd, "brightness-levels", sizeof(u32));
	if (brightlevel_size < 0) {
		goto levels_fail;
	} else {
		/* 分配背光等级存储内存 */
		bright_val = kmalloc(brightlevel_size * sizeof(u32), GFP_KERNEL);
		if (bright_val < 0) {
			goto levels_fail;
		} else {
			/* 获取背光等级 */
			ret = of_property_read_u32_array(bl_nd, "brightness-levels", bright_val, brightlevel_size);
			if (ret < 0) {
				goto levels_val_fail;
			} else {
				printk("the brightness level is:");
				for (i = 0; i < brightlevel_size; i++) {
					printk("%d ", *(bright_val + i));
				}
				printk("\n");
			}
		}
	}

	/* 获取默认背光等级 */
	ret = of_property_read_u32(bl_nd, "default-brightness-level", bright_val);
	if (ret < 0) {
		goto levels_val_fail;
	} else {
		printk("default-brightness-level:%d\n", *bright_val);
	}

	/* 获取status属性 */
	status = of_find_property(bl_nd, "status", NULL);
	if (status == NULL) {
		ret = -EINVAL;
		goto status_fail;
	} else {
		printk("status = %s\n", (char*)status->value);
	}

status_fail:
levels_val_fail:
	kfree(bright_val);
levels_fail:
comp_fail:
	return ret;
}

/* 模块卸载函数 */
static void __exit dts_exit(void)
{

}

module_init(dts_init);
module_exit(dts_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("eurphan");
