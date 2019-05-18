#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/i2c/tsc2007.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#define TSC2007_MEASURE_TEMP0		(0x0 << 4)
#define TSC2007_MEASURE_AUX		(0x2 << 4)
#define TSC2007_MEASURE_TEMP1		(0x4 << 4)
#define TSC2007_ACTIVATE_XN		(0x8 << 4)
#define TSC2007_ACTIVATE_YN		(0x9 << 4)
#define TSC2007_ACTIVATE_YP_XN		(0xa << 4)
#define TSC2007_SETUP			(0xb << 4)
#define TSC2007_MEASURE_X		(0xc << 4)
#define TSC2007_MEASURE_Y		(0xd << 4)
#define TSC2007_MEASURE_Z1		(0xe << 4)
#define TSC2007_MEASURE_Z2		(0xf << 4)

#define TSC2007_POWER_OFF_IRQ_EN	(0x0 << 2)
#define TSC2007_ADC_ON_IRQ_DIS0		(0x1 << 2)
#define TSC2007_ADC_OFF_IRQ_EN		(0x2 << 2)
#define TSC2007_ADC_ON_IRQ_DIS1		(0x3 << 2)

#define TSC2007_12BIT			(0x0 << 1)
#define TSC2007_8BIT			(0x1 << 1)

#define	MAX_12BIT			((1 << 12) - 1)

#define ADC_ON_12BIT	(TSC2007_12BIT | TSC2007_ADC_ON_IRQ_DIS0)

#define READ_Y		(ADC_ON_12BIT | TSC2007_MEASURE_Y)
#define READ_Z1		(ADC_ON_12BIT | TSC2007_MEASURE_Z1)
#define READ_Z2		(ADC_ON_12BIT | TSC2007_MEASURE_Z2)
#define READ_X		(ADC_ON_12BIT | TSC2007_MEASURE_X)
#define PWRDOWN		(TSC2007_12BIT | TSC2007_POWER_OFF_IRQ_EN)

/* tsc2007原始数据 */
struct ts_event {
	u16	x;
	u16	y;
	u16	z1;
	u16 z2;
};

struct tsc2007 {
	struct input_dev	*input;
	char				phys[32];
	struct i2c_client	*client;
	u16					model;
	u16					x_plate_ohms;
	u16					max_rt;
	unsigned long		poll_period; /* in jiffies */
	int					fuzzx;
	int					fuzzy;
	int					fuzzz;
	unsigned			gpio;
	int					irq;
	wait_queue_head_t	wait;
	bool				stopped;
	int	(*get_pendown_state)(struct device *);
};

/* i2c传输数据 */
static inline int tsc2007_xfer(struct tsc2007 *tsc, u8 cmd)
{
	s32 data;
	u16 val;
	data = i2c_smbus_read_word_data(tsc->client, cmd);
	if (data < 0) {
		return data;
	}
	val = swab16(data) >> 4;
	return val;
}

/* 从tsc2007中读取数据坐标 */
static void tsc2007_read_values(struct tsc2007 *tsc, struct ts_event *tc)
{
	tc->y = tsc2007_xfer(tsc, READ_Y);
	tc->x = tsc2007_xfer(tsc, READ_X);
	tc->z1 = tsc2007_xfer(tsc, READ_Z1);
	tc->z2 = tsc2007_xfer(tsc, READ_Z2);
	tsc2007_xfer(tsc, PWRDOWN);
}

/* 计算压力 */
static u32 tsc2007_calculate_pressure(struct tsc2007 *tsc, struct ts_event *tc)
{
	u32 rt = 0;
	if (tc->x == MAX_12BIT)
		tc->x = 0;

	if (likely(tc->x && tc->z1)) {
		rt = tc->z2 - tc->z1;
		rt *= tc->x;
		rt *= tsc->x_plate_ohms;
		rt /= tc->z1;
		rt = (rt + 2047) >> 12;
	}
	return rt;
}

/* 获取触屏按下状态 */
static bool tsc2007_is_pen_down(struct tsc2007 *ts)
{
	/* 没有此函数，函数指针为空,返回true */
	if(!ts->get_pendown_state)
		return true;
	else
		/* 获取按下状态 */
		return ts->get_pendown_state(&ts->client->dev);
}

/* 软中断 */
static irqreturn_t tsc2007_soft_irq(int irq, void *handler)
{
	struct tsc2007 *ts = handler;
	struct input_dev *input = ts->input;
	struct ts_event tc;
	u32 rt;

	/* 未stop且按下 ，如果一直按下会在这里循环*/
	while (!ts->stopped && tsc2007_is_pen_down(ts)) {
		/* 读取tsc2007原始数据 */
		tsc2007_read_values(ts, &tc);
		/* 计算压力 */
		rt = tsc2007_calculate_pressure(ts, &tc);
		if (!rt && !ts->get_pendown_state)
			break;
		if (rt <= ts->max_rt) {
			input_report_key(input, BTN_TOUCH, 1);
			input_report_abs(input, ABS_X, tc.x);
			input_report_abs(input, ABS_Y, tc.y);
			input_report_abs(input, ABS_PRESSURE, rt);
			input_sync(input);
		}
		/* 等待一段时 */
		wait_event_timeout(ts->wait, ts->stopped, ts->poll_period);
	}
	input_report_key(input, BTN_TOUCH, 0);
	input_report_abs(input, ABS_PRESSURE, 0);
	input_sync(input);

	return IRQ_HANDLED;
}

/* 硬中断 */
static  irqreturn_t tsc2007_hard_irq(int irq, void *handle)
{
	struct tsc2007 *ts = handle;
	/* 按下 */
	if (tsc2007_is_pen_down(ts))
		return IRQ_WAKE_THREAD;

	return IRQ_HANDLED;
}

/* stop函数 */
static void tsc2007_stop(struct tsc2007 *ts)
{
	/* 标记为停止状态 */
	ts->stopped = true;
	mb();
	wake_up(&ts->wait);
	/* 关闭外部中断 */
	disable_irq(ts->irq);
}

/* open函数 */
static int tsc2007_open(struct input_dev *input_dev)
{
	struct tsc2007 *ts = input_get_drvdata(input_dev);
	int err;

	/* 标记为启动状态 */
	ts->stopped = false;
	mb();

	/* 开启外部中断 */
	enable_irq(ts->irq);

	/* 使tsc2007掉电，并开启中断 */
	err = tsc2007_xfer(ts, PWRDOWN);
	if (err < 0) {
		tsc2007_stop(ts);
		return err;
	}
	return 0;
}

/* close函数 */
static void tsc2007_close(struct input_dev *input_dev)
{
	struct tsc2007 *ts = input_get_drvdata(input_dev);

	tsc2007_stop(ts);
}

/* 触摸按下状态获取 */
static int  tsc2007_get_pendown_state_gpio(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct tsc2007 *ts = i2c_get_clientdata(client);

	/* 按下返回1 */
	return !gpio_get_value(ts->gpio);
}

/* 设备树读取 */
static int tsc2007_probe_dt(struct i2c_client *client, struct tsc2007 *ts)
{
	struct device_node *np = client->dev.of_node;
	u32 val32;
	u64 val64;

	if (!np) 
		return -EINVAL;

	if (!of_property_read_u32(np, "ti,max-rt", &val32))
		ts->max_rt = val32;
	else
		ts->max_rt = MAX_12BIT;

	if (!of_property_read_u32(np, "ti,fuzzx", &val32))
		ts->fuzzx = val32;

	if (!of_property_read_u32(np, "ti,fuzzy", &val32))
		ts->fuzzy = val32;

	if (!of_property_read_u32(np, "ti,fuzzz", &val32))
		ts->fuzzz = val32;

	if (!of_property_read_u64(np, "ti,poll-period", &val64))
		ts->poll_period = msecs_to_jiffies(val64);
	else
		ts->poll_period = msecs_to_jiffies(1);

	if (!of_property_read_u32(np, "ti,x-plate-ohms", &val32))
		ts->x_plate_ohms = val32;
	else
		return -EINVAL;

	/* 获取GPIO */
	ts->gpio = of_get_gpio(np, 0);
	if (gpio_is_valid(ts->gpio))
		ts->get_pendown_state = tsc2007_get_pendown_state_gpio;

	return 0;
}

/* probe function */
static int tsc2007_probe(struct i2c_client *client, 
			const struct i2c_device_id *id)
{
	struct tsc2007 *ts;
	struct input_dev *input_dev;
	int err;

	/* 给ts分配内存，devm_kzalloc分配的内存不需要手动释放 */
	ts = devm_kzalloc(&client->dev, sizeof(struct tsc2007), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;
	/* 使用设备树属性配置ts */
	err = tsc2007_probe_dt(client, ts);
	if (err)
		return err;

	/* 为input_dev分配内存 */
	input_dev = devm_input_allocate_device(&client->dev);
	if (!input_dev)
		return -ENOMEM;
	
	i2c_set_clientdata(client, ts);

	ts->client = client;
	ts->irq = client->irq;
	ts->input = input_dev;
	/* 初始化等待队列头 */
	init_waitqueue_head(&ts->wait);
	input_dev->name = "TSC2007 TouchScreen";
	input_dev->phys = ts->phys;
	input_dev->id.bustype = BUS_I2C;
	input_dev->open = tsc2007_open;
	input_dev->close = tsc2007_close;
	input_set_drvdata(input_dev, ts);

	// 支持的事件类型
	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	// 支持的按键值
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	/* 绝对坐标 */
	/* 设置上报数据类型，范围 */
	input_set_abs_params(input_dev, ABS_X, 0, MAX_12BIT, ts->fuzzx, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, MAX_12BIT, ts->fuzzy, 0);
	input_set_abs_params(input_dev, ABS_PRESSURE, 0, MAX_12BIT, ts->fuzzz, 0);

	/* 请求软中断 */
	err = devm_request_threaded_irq(&client->dev, ts->irq, tsc2007_hard_irq,
								tsc2007_soft_irq, IRQF_ONESHOT,
								client->dev.driver->name, ts);
	if (err)
		return err;
	tsc2007_stop(ts);

	/* 注册input设备 */
	err = input_register_device(input_dev);
	if (err)
		return err;
	
	return 0;
}


static int tsc2007_remove(struct i2c_client *client, 
			const struct i2c_device_id *id)
{
	return 0;
}

/* 同样用于i2c device和driver配对 */
static const struct i2c_device_id tsc2007_idtable[] = {
	{ "tsc2007", 0 },
	{ }
};

/* 用于device和driver匹配 */
static const struct of_device_id tsc2007_of_match[] = {
	{ .compatible = "ti,tsc2007" },
	{ /* sentinel */}
};
MODULE_DEVICE_TABLE(of, tsc2007_of_match);

/* i2c driver结构体 */
static struct i2c_driver tsc2007_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "tsc2007",
		.of_match_table = of_match_ptr(tsc2007_of_match),
	},
	.id_table	= tsc2007_idtable,
	.probe		= tsc2007_probe,
	.remove 	= tsc2007_remove,
};

module_i2c_driver(tsc2007_driver);

MODULE_AUTHOR("eurphan<eurphan@163.com>");
MODULE_DESCRIPTION("TSC2007 TouchScreen Driver");
MODULE_LICENSE("GPL");
