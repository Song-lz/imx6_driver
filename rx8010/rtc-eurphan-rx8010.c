/*************************************************************************
#  File Name:   drivers/rtc/rtc-eurphan-rx8010.c
#  Description:
#  Author:      eurphan
#  Mail:        eurphan@163.com 
#  Created Time:2019年11月28日 星期四 22时49分35秒
 ************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/bcd.h>
#include <linux/i2c.h>
#include <linux/list.h>
#include <linux/rtc.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/input.h>

// RX8010 Register definitions
#define RX8010_REG_SEC		0x10
#define RX8010_REG_MIN		0x11
#define RX8010_REG_HOUR		0x12
#define RX8010_REG_WDAY		0x13
#define RX8010_REG_MDAY		0x14
#define RX8010_REG_MONTH	0x15
#define RX8010_REG_YEAR		0x16
// 0x17 is reserved
#define RX8010_REG_ALMIN	0x18
#define RX8010_REG_ALHOUR	0x19
#define RX8010_REG_ALWDAY	0x1A
#define RX8010_REG_TCOUNT0	0x1B
#define RX8010_REG_TCOUNT1	0x1C
#define RX8010_REG_EXT		0x1D
#define RX8010_REG_FLAG		0x1E
#define RX8010_REG_CTRL		0x1F
#define RX8010_REG_USER0	0x20
#define RX8010_REG_USER1	0x21
#define RX8010_REG_USER2	0x22
#define RX8010_REG_USER3	0x23
#define RX8010_REG_USER4	0x24
#define RX8010_REG_USER5	0x25
#define RX8010_REG_USER6	0x26
#define RX8010_REG_USER7	0x27
#define RX8010_REG_USER8	0x28
#define RX8010_REG_USER9	0x29
#define RX8010_REG_USERA	0x2A
#define RX8010_REG_USERB	0x2B
#define RX8010_REG_USERC	0x2C
#define RX8010_REG_USERD	0x2D
#define RX8010_REG_USERE	0x2E
#define RX8010_REG_USERF	0x2F
// 0x30 is reserved
// 0x31 is reserved
#define RX8010_REG_IRQ		0x32

// Extension Register (1Dh) bit positions
#define RX8010_BIT_EXT_TSEL		(7 << 0)
#define RX8010_BIT_EXT_WADA		(1 << 3)
#define RX8010_BIT_EXT_TE		(1 << 4)
#define RX8010_BIT_EXT_USEL		(1 << 5)
#define RX8010_BIT_EXT_FSEL		(3 << 6)

// Flag Register (1Eh) bit positions
#define RX8010_BIT_FLAG_VLF		(1 << 1)
#define RX8010_BIT_FLAG_AF		(1 << 3)
#define RX8010_BIT_FLAG_TF		(1 << 4)
#define RX8010_BIT_FLAG_UF		(1 << 5)

// Control Register (1Fh) bit positions
#define RX8010_BIT_CTRL_TSTP	(1 << 2)
#define RX8010_BIT_CTRL_AIE		(1 << 3)
#define RX8010_BIT_CTRL_TIE		(1 << 4)
#define RX8010_BIT_CTRL_UIE		(1 << 5)
#define RX8010_BIT_CTRL_STOP	(1 << 6)
#define RX8010_BIT_CTRL_TEST	(1 << 7)

// 私有结构体
struct rx8010_data {
	struct i2c_client *client;
	struct rtc_device *rtc;
};

// 读取单个寄存器
static int rx8010_read_reg(struct i2c_client *client, int number, unsigned char *value)
{
	int ret = i2c_smbus_read_byte_data(client, number);

	if (ret < 0) {
		dev_err(&client->dev, "Unable to read register #%d\n", number);
		return ret;
	}
	*value = ret;
	return 0;
}

// 读取多个寄存器
static int rx8010_read_regs(struct i2c_client *client, int number, unsigned char *values, int length)
{
	int ret = i2c_smbus_read_i2c_block_data(client, number, length, values);

	if (ret != length) {
		dev_err(&client->dev, "Unable to read register #%d..#%d\n", number, number + length - 1);
		return ret < 0 ? ret : -EIO;
	}
	return 0;
}

// 写入单个寄存器
static int rx8010_write_reg(struct i2c_client *client, int number, unsigned char value)
{
	int ret = i2c_smbus_write_byte_data(client, number, value);

	if (ret)
		dev_err(&client->dev, "Unable to write register #%d\n", number);

	return ret;
}

// 写入多个寄存器
static int rx8010_write_regs(struct i2c_client *client, int number, unsigned char *values, int length)
{
	int ret = i2c_smbus_write_i2c_block_data(client, number, length, values);

	if (ret)
		dev_err(&client->dev, "Unable to write registers #%d..#%d\n", number, number + length - 1);

	return ret;
}

// 获取当前时间
static int rx8010_get_time(struct device *dev, struct rtc_time *dt)
{
	struct rx8010_data *rx8010 = dev_get_drvdata(dev);
	unsigned char date[7];
	int err;

	// read registers
	err = rx8010_read_regs(rx8010->client, RX8010_REG_SEC, date, 7);
	if (err)
		return err;

	dev_dbg(dev, "%s: read 0x%02x 0x%02x "
		"0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", __func__,
		date[0], date[1], date[2], date[3], date[4], date[5], date[6]);

	//Note: need to subtract 0x10 for index as register offset starts at 0x10
	dt->tm_sec = bcd2bin(date[RX8010_REG_SEC - 0x10] & 0x7f);
	dt->tm_min = bcd2bin(date[RX8010_REG_MIN - 0x10] & 0x7f);
	dt->tm_hour = bcd2bin(date[RX8010_REG_HOUR - 0x10] & 0x3f);	//only 24-hour clock
	dt->tm_mday = bcd2bin(date[RX8010_REG_MDAY - 0x10] & 0x3f);
	dt->tm_mon = bcd2bin(date[RX8010_REG_MONTH - 0x10] & 0x1f) - 1;
	dt->tm_year = bcd2bin(date[RX8010_REG_YEAR - 0x10]);
	dt->tm_wday = bcd2bin(date[RX8010_REG_WDAY - 0x10] & 0x7f);

	if (dt->tm_year < 70)
		dt->tm_year += 100;

	dev_dbg(dev, "%s: date %ds %dm %dh %dmd %dm %dy\n", __func__,
		dt->tm_sec, dt->tm_min, dt->tm_hour,
		dt->tm_mday, dt->tm_mon, dt->tm_year);

	return rtc_valid_tm(dt);
}

// 设置时间
static int rx8010_set_time(struct device *dev, struct rtc_time *dt)
{
	struct rx8010_data *rx8010 = dev_get_drvdata(dev);
	unsigned char date[7];
	unsigned char ctrl = 0;
	int ret;

	// set STOP bit before changing clock/calendar
	rx8010_read_reg(rx8010->client, RX8010_REG_CTRL, &ctrl);
	ctrl |= RX8010_BIT_CTRL_STOP;
	rx8010_write_reg(rx8010->client, RX8010_REG_CTRL, ctrl);

	// conversion bin to bcd
	date[RX8010_REG_SEC - 0x10] = bin2bcd(dt->tm_sec);
	date[RX8010_REG_MIN - 0x10] = bin2bcd(dt->tm_min);
	date[RX8010_REG_HOUR - 0x10] = bin2bcd(dt->tm_hour);

	date[RX8010_REG_MDAY - 0x10] = bin2bcd(dt->tm_mday);
	date[RX8010_REG_MONTH - 0x10] = bin2bcd(dt->tm_mon + 1);
	date[RX8010_REG_YEAR - 0x10] = bin2bcd(dt->tm_year % 100);
	date[RX8010_REG_WDAY - 0x10] = bin2bcd(dt->tm_wday);

	dev_dbg(dev, "%s: write 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
		__func__, date[0], date[1], date[2], date[3], date[4], date[5], date[6]);

	// write to register
	ret = rx8010_write_regs(rx8010->client, RX8010_REG_SEC, date, 7);


	// clear STOP bit before changing clock/calendar
	rx8010_read_reg(rx8010->client, RX8010_REG_CTRL, &ctrl);
	ctrl &= ~RX8010_BIT_CTRL_STOP;
	rx8010_write_reg(rx8010->client, RX8010_REG_CTRL, ctrl);

	return ret;
}

// 初始化设备
static int rx8010_init_client(struct i2c_client *client, int* need_reset)
{
	struct rx8010_data *rx8010 = i2c_get_clientdata(client);
	unsigned char ctrl[3];
	int err;

	//set reserved register 0x17 with specified value of 0xD8
	err = rx8010_write_reg(client, 0x17, 0xD8);
	if (err)
		goto out;

	//set reserved regitster 0x30 with specified value of 0x00
	err = rx8010_write_reg(client, 0x30, 0x00);
	if (err)
		goto out;

	//set reserved register 0x31 with specified value of 0x08
	err = rx8010_write_reg(client, 0x31, 0x08);
	if (err)
		goto out;

	// get current extension, flag, control register values
	err = rx8010_read_regs(rx8010->client, RX8010_REG_EXT, ctrl, 3);
	if (err)
		goto out;

	// set extension register, TE to 0, FSEL1-0 to 0(off) and TSEL2-0 to 1Hz
	ctrl[0] &= ~RX8010_BIT_EXT_TE;
	ctrl[0] &= ~RX8010_BIT_EXT_FSEL;
	ctrl[0] |= 0x02;
	err = rx8010_write_reg(client, RX8010_REG_EXT, ctrl[0]);
	if (err)
		goto out;

	// set tht test bit and reserved bits of control register zero

	// check for VLF flag (set at power-on)
	if ((ctrl[1] & RX8010_BIT_FLAG_VLF)) {
		dev_warn(&client->dev, "Frequency stop was detected, probably due to a supply voltage drop\n");
		*need_reset = 1;
	}

	// clear flags
	err = rx8010_write_reg(client, RX8010_REG_FLAG, 0x00);
	if (err)
		goto out;
	// clear ctrl
	err = rx8010_write_reg(client, RX8010_REG_CTRL, 0x00);
	if (err)
		goto out;

out:
	return err;
}

// RTC操作函数结构体
static struct rtc_class_ops rx8010_rtc_ops = {
	.read_time = rx8010_get_time,
	.set_time = rx8010_set_time,
};

// 设备和驱动匹配函数
static int rx8010_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct rx8010_data *rx8010;
	int err, need_reset = 0;

	// 为rx8010结构体分配内存
	rx8010 = kzalloc(sizeof(rx8010), GFP_KERNEL);
	if (!rx8010) {
		dev_err(&adapter->dev, "failed to alloc mempry\n");
		err = -ENOMEM;
		goto errout;
	}
	rx8010->client = client;
	// 将rx8010结构体赋给client私有数据
	i2c_set_clientdata(client, rx8010);
	// 初始化rx8010
	err = rx8010_init_client(client, &need_reset);
	if (err) {
		dev_err(&client->dev, "rx8100 init error\n");
		goto errout_free;
	}
	// rx8010需要复位
	if (need_reset) {
		struct rtc_time tm;
		dev_info(&client->dev, "bad conditions detected, resetting data\n");
		rtc_time_to_tm(0, &tm);
		rx8010_set_time(&client->dev, &tm);
	}
	// 注册RTC设备
	rx8010->rtc = rtc_device_register(client->name, &client->dev, &rx8010_rtc_ops, THIS_MODULE);
	if (!rx8010->rtc) {
		dev_err(&client->dev, "unable to register the rtc device\n");
		goto errout_free;
	}	

	return 0;

errout_free:
	kfree(rx8010);
errout:
	dev_err(&adapter->dev, "probing for rx8010 failed\n");
	return err;
}

//驱动卸载函数 
static int rx8010_remove(struct i2c_client *client)
{
	struct rx8010_data *rx8010 = i2c_get_clientdata(client);

	rtc_device_unregister(rx8010->rtc);
	kfree(rx8010);
	return 0;
}

static const struct i2c_device_id rx8010_id[] = {
	{ "rx8010", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, rx8010_id);

// 设备树匹配结构
static const struct of_device_id epson_rx8010_of_match[] = {
	
	{ .compatible = "eurphan-rx8010", },
	{ },
};
MODULE_DEVICE_TABLE(of, epson_rx8010_of_match);

// i2c驱动结构
static struct i2c_driver rx8010_driver = {
	.driver = {
		.name = "rtc,rx8010",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(epson_rx8010_of_match),
	},
	.probe = rx8010_probe,
	.remove = rx8010_remove,
	.id_table = rx8010_id,
};
module_i2c_driver(rx8010_driver);

MODULE_LICENSE("GPL");
