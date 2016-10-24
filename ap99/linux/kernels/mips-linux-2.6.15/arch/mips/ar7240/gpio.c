#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/signal.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/resource.h>
#include <linux/proc_fs.h>
#include <asm/types.h>
#include <asm/irq.h>


#include <linux/fs.h>
#include <linux/major.h>
#include <linux/mtd/mtd.h>
#include <asm-mips/ioctl.h>
#include <asm-mips/uaccess.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <asm/system.h>

#include "ar7240.h"


/*
 * IOCTL Command Codes
 */

#define AR7240_GPIO_IOCTL_BASE			0x01
#define AR7240_GPIO_IOCTL_CMD1      	AR7240_GPIO_IOCTL_BASE
#define AR7240_GPIO_IOCTL_CMD2      	AR7240_GPIO_IOCTL_BASE + 0x01
#define AR7240_GPIO_IOCTL_CMD3      	AR7240_GPIO_IOCTL_BASE + 0x02
#define AR7240_GPIO_IOCTL_MAX			AR7240_GPIO_IOCTL_CMD3

#define AR7240_GPIO_MAGIC 				0xB2
#define	AR7240_GPIO_BTN_READ			_IOR(AR7240_GPIO_MAGIC, AR7240_GPIO_IOCTL_CMD1, int)
#define	AR7240_GPIO_LED_READ			_IOR(AR7240_GPIO_MAGIC, AR7240_GPIO_IOCTL_CMD2, int)
#define	AR7240_GPIO_LED_WRITE			_IOW(AR7240_GPIO_MAGIC, AR7240_GPIO_IOCTL_CMD3, int)

#define gpio_major      				238
#define gpio_minor      				0

/*
 * GPIO assignment
 */
#if 0
/* 090108, modified by lsz */
#define RST_DFT_GPIO		11	/* reset default */
#define SYS_LED_GPIO		1	/* system led 	*/
#define JUMPSTART_GPIO		12	/* wireless jumpstart */
#define RST_HOLD_TIME		5	/* How long the user must press the button before Router rst */
#endif

#define RST_DFT_GPIO		CONFIG_GPIO_RESET_FAC_BIT	    /* reset default */
#define SYS_LED_GPIO		CONFIG_GPIO_READY_STATUS_BIT	/* system LED 	*/
#define SYS_LED_OFF         (!CONFIG_GPIO_READY_STATUS_ON)  /* system LED's value when on */
#define JUMPSTART_GPIO		CONFIG_GPIO_JUMPSTART_SW_BIT	/* wireless jumpstart */
#define RST_HOLD_TIME		CONFIG_GPIO_FAC_RST_HOLD_TIME	/* How long the user must press the button before Router rst */

#ifdef CONFIG_GPIO_USB_LED_BIT
#define AP_USB_LED_GPIO     CONFIG_GPIO_USB_LED_BIT         /* USB LED */
#define USB_LED_OFF         (!CONFIG_GPIO_USB_LED_ON)       /* USB LED's value when off */
#define USB_LED_ON          CONFIG_GPIO_USB_LED_ON          /* USB LED's value when on */
#else
#undef AP_USB_LED_GPIO
#endif

#define TRICOLOR_LED_GREEN_PIN  CONFIG_GPIO_JUMPSTART_LED_BIT   /* jump start LED */
//#define TRICOLOR_LED_YELLOW_PIN 5         /* GPIO 5 */

#define OFF     (!CONFIG_GPIO_JUMPSTART_LED_ON)             /* jump start LED's value when off */
#define ON      CONFIG_GPIO_JUMPSTART_LED_ON               /* jump start LED'S value when on */


int counter = 0;
int jiff_when_press = 0;
int bBlockWps = 1;
struct timer_list rst_timer;

int g_usbLedBlinkCountDown = 1;

/* control params for reset button reuse, by zjg, 13Apr10 */
static int l_bMultiUseResetButton		=	0;
static int l_bWaitForQss				= 	1;


EXPORT_SYMBOL(g_usbLedBlinkCountDown);

/*
 * GPIO interrupt stuff
 */
typedef enum {
    INT_TYPE_EDGE,
    INT_TYPE_LEVEL,
}ar7240_gpio_int_type_t;

typedef enum {
    INT_POL_ACTIVE_LOW,
    INT_POL_ACTIVE_HIGH,
}ar7240_gpio_int_pol_t;


/* 
** Simple Config stuff
*/

#if !defined(IRQ_NONE)
#define IRQ_NONE
#define IRQ_HANDLED
#endif /* !defined(IRQ_NONE) */



typedef irqreturn_t(*sc_callback_t)(int, void *, struct pt_regs *);

static sc_callback_t registered_cb = NULL;
static void *cb_arg;
static int ignore_pushbutton = 1;
static struct proc_dir_entry *simple_config_entry = NULL;
static struct proc_dir_entry *simulate_push_button_entry = NULL;
static struct proc_dir_entry *tricolor_led_entry = NULL;

/* ZJin 100317: for 3g usb led blink feature, use procfs simple config. */
static struct proc_dir_entry *usb_led_blink_entry = NULL;

/* added by zjg, 12Apr10 */
static struct proc_dir_entry *multi_use_reset_button_entry = NULL;

/*added by  ZQQ<10.06.02 for usb power*/
#define SYS_USB_POWER_GPIO 	6
#define USB_POWER_ON		1
#define USB_POWER_OFF		(!USB_POWER_ON)
static struct proc_dir_entry *usb_power_entry = NULL;
/*end added*/

void ar7240_gpio_config_int(int gpio, 
                       ar7240_gpio_int_type_t type,
                       ar7240_gpio_int_pol_t polarity)
{
    u32 val;

    /*
     * allow edge sensitive/rising edge too
     */
    if (type == INT_TYPE_LEVEL) {
        /* level sensitive */
        ar7240_reg_rmw_set(AR7240_GPIO_INT_TYPE, (1 << gpio));
    }
    else {
       /* edge triggered */
       val = ar7240_reg_rd(AR7240_GPIO_INT_TYPE);
       val &= ~(1 << gpio);
       ar7240_reg_wr(AR7240_GPIO_INT_TYPE, val);
    }

    if (polarity == INT_POL_ACTIVE_HIGH) {
        ar7240_reg_rmw_set (AR7240_GPIO_INT_POLARITY, (1 << gpio));
    }
    else {
       val = ar7240_reg_rd(AR7240_GPIO_INT_POLARITY);
       val &= ~(1 << gpio);
       ar7240_reg_wr(AR7240_GPIO_INT_POLARITY, val);
    }

    ar7240_reg_rmw_set(AR7240_GPIO_INT_ENABLE, (1 << gpio));
}

void
ar7240_gpio_config_output(int gpio)
{
    ar7240_reg_rmw_set(AR7240_GPIO_OE, (1 << gpio));
}

void
ar7240_gpio_config_input(int gpio)
{
    ar7240_reg_rmw_clear(AR7240_GPIO_OE, (1 << gpio));
}

void
ar7240_gpio_out_val(int gpio, int val)
{
    if (val & 0x1) {
        ar7240_reg_rmw_set(AR7240_GPIO_OUT, (1 << gpio));
    }
    else {
        ar7240_reg_rmw_clear(AR7240_GPIO_OUT, (1 << gpio));
    }
}

int
ar7240_gpio_in_val(int gpio)
{
    return((1 << gpio) & (ar7240_reg_rd(AR7240_GPIO_IN)));
}

static void
ar7240_gpio_intr_enable(unsigned int irq)
{
    ar7240_reg_rmw_set(AR7240_GPIO_INT_MASK, 
                      (1 << (irq - AR7240_GPIO_IRQ_BASE)));
}

static void
ar7240_gpio_intr_disable(unsigned int irq)
{
    ar7240_reg_rmw_clear(AR7240_GPIO_INT_MASK, 
                        (1 << (irq - AR7240_GPIO_IRQ_BASE)));
}

static unsigned int
ar7240_gpio_intr_startup(unsigned int irq)
{
	ar7240_gpio_intr_enable(irq);
	return 0;
}

static void
ar7240_gpio_intr_shutdown(unsigned int irq)
{
	ar7240_gpio_intr_disable(irq);
}

static void
ar7240_gpio_intr_ack(unsigned int irq)
{
	ar7240_gpio_intr_disable(irq);
}

static void
ar7240_gpio_intr_end(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED | IRQ_INPROGRESS)))
		ar7240_gpio_intr_enable(irq);
}

static void
ar7240_gpio_intr_set_affinity(unsigned int irq, cpumask_t mask)
{
	/* 
     * Only 1 CPU; ignore affinity request
     */
}

struct hw_interrupt_type ar7240_gpio_intr_controller = {
	"AR7240 GPIO",
	ar7240_gpio_intr_startup,
	ar7240_gpio_intr_shutdown,
	ar7240_gpio_intr_enable,
	ar7240_gpio_intr_disable,
	ar7240_gpio_intr_ack,
	ar7240_gpio_intr_end,
	ar7240_gpio_intr_set_affinity,
};

void
ar7240_gpio_irq_init(int irq_base)
{
	int i;

	for (i = irq_base; i < irq_base + AR7240_GPIO_IRQ_COUNT; i++) {
		irq_desc[i].status  = IRQ_DISABLED;
		irq_desc[i].action  = NULL;
		irq_desc[i].depth   = 1;
		irq_desc[i].handler = &ar7240_gpio_intr_controller;
	}
}


/* 
 *  USB GPIO control
 */

void ap_usb_led_on(void)
{
#ifdef AP_USB_LED_GPIO	
    ar7240_gpio_out_val(AP_USB_LED_GPIO, USB_LED_ON);
#endif
}

EXPORT_SYMBOL(ap_usb_led_on);

void ap_usb_led_off(void)
{
#ifdef AP_USB_LED_GPIO
    ar7240_gpio_out_val(AP_USB_LED_GPIO, USB_LED_OFF);
#endif
}

EXPORT_SYMBOL(ap_usb_led_off);

void register_simple_config_callback (void *callback, void *arg)
{
    registered_cb = (sc_callback_t) callback;
    cb_arg = arg;
}
EXPORT_SYMBOL(register_simple_config_callback);

void unregister_simple_config_callback (void)
{
    registered_cb = NULL;
    cb_arg = NULL;
}
EXPORT_SYMBOL(unregister_simple_config_callback);

/*
 * Irq for front panel SW jumpstart switch
 * Connected to XSCALE through GPIO4
 */
irqreturn_t jumpstart_irq(int cpl, void *dev_id, struct pt_regs *regs)
{
        if (ignore_pushbutton) {
        ar7240_gpio_config_int (JUMPSTART_GPIO,INT_TYPE_LEVEL,
                                INT_POL_ACTIVE_HIGH);
            ignore_pushbutton = 0;
            return IRQ_HANDLED;
        }

        ar7240_gpio_config_int (JUMPSTART_GPIO, INT_TYPE_LEVEL, INT_POL_ACTIVE_LOW);
        ignore_pushbutton = 1;

    printk ("Jumpstart button pressed.\n");

    if (registered_cb && !bBlockWps) {
        return registered_cb (cpl, cb_arg, regs);
    }
    return IRQ_HANDLED;
}

static int ignore_rstbutton = 1;
	
/* irq handler for reset button */
irqreturn_t rst_irq(int cpl, void *dev_id, struct pt_regs *regs)
{
    if (ignore_rstbutton) 
	{
        ar7240_gpio_config_int(RST_DFT_GPIO, INT_TYPE_LEVEL, INT_POL_ACTIVE_HIGH);
        ignore_rstbutton = 0;

		mod_timer(&rst_timer, jiffies + RST_HOLD_TIME * HZ);

        return IRQ_HANDLED;
            }

    ar7240_gpio_config_int (RST_DFT_GPIO, INT_TYPE_LEVEL, INT_POL_ACTIVE_LOW);
    ignore_rstbutton = 1;

	printk("Reset button pressed.\n");

	/* mark reset status, by zjg, 12Apr10 */
    if (registered_cb && !bBlockWps && l_bMultiUseResetButton && l_bWaitForQss) 
	{
        return registered_cb (cpl, cb_arg, regs);
    }

            return IRQ_HANDLED;
}

void check_rst(unsigned long nothing)
{
	if (!ignore_rstbutton)
	{
		printk ("restoring factory default...\n");
    	counter ++;

		/* to mark reset status, forbid QSS, added by zjg, 12Apr10 */
		l_bWaitForQss	= 0;
    }
}

static int multi_use_reset_button_read (char *page, char **start, off_t off,
                               int count, int *eof, void *data)
{
    return sprintf (page, "%d\n", l_bMultiUseResetButton);
}

static int multi_use_reset_button_write (struct file *file, const char *buf,
                                        unsigned long count, void *data)
{
    u_int32_t val;

	if (sscanf(buf, "%d", &val) != 1)
        return -EINVAL;

	/* only admit "0" or "1" */
	if ((val < 0) || (val > 1))
		return -EINVAL;	

	l_bMultiUseResetButton = val;
	
	return count;

}

static int push_button_read (char *page, char **start, off_t off,
                               int count, int *eof, void *data)
{
    return 0;
}

static int push_button_write (struct file *file, const char *buf,
                                        unsigned long count, void *data)
{
    if (registered_cb) {
        registered_cb (0, cb_arg, 0);
    }
    return count;
}

static int usb_led_blink_read (char *page, char **start, off_t off,
                               int count, int *eof, void *data)
{
    return sprintf (page, "%d\n", g_usbLedBlinkCountDown);
}

static int usb_led_blink_write (struct file *file, const char *buf,
                                        unsigned long count, void *data)
{
    u_int32_t val;

	if (sscanf(buf, "%d", &val) != 1)
        return -EINVAL;

	if ((val < 0) || (val > 1))
		return -EINVAL;

	g_usbLedBlinkCountDown = val;

	return count;
}

/*added by ZQQ,10.06.02*/
static int usb_power_read (char *page, char **start, off_t off,
                               int count, int *eof, void *data)
{
    return 0;
}

static int usb_power_write (struct file *file, const char *buf,
                                        unsigned long count, void *data)
{
    u_int32_t val = 0;

	if (sscanf(buf, "%d", &val) != 1)
        return -EINVAL;

	if ((val < 0) || (val > 1))
		return -EINVAL;

	printk("%s %d: write gpio:value = %d\r\n",__FUNCTION__,__LINE__,val);
	if (USB_POWER_ON == val)
	{
		ar7240_gpio_out_val(SYS_USB_POWER_GPIO, USB_POWER_ON);
	}
	else
	{
		ar7240_gpio_out_val(SYS_USB_POWER_GPIO, USB_POWER_OFF);
	}
	
	return count;
}
/*end added */

typedef enum {
        LED_STATE_OFF   =       0,
        LED_STATE_GREEN =       1,
        LED_STATE_YELLOW =      2,
        LED_STATE_ORANGE =      3,
        LED_STATE_MAX =         4
} led_state_e;

static led_state_e gpio_tricolorled = LED_STATE_OFF;

static int gpio_tricolor_led_read (char *page, char **start, off_t off,
               int count, int *eof, void *data)
{
    return sprintf (page, "%d\n", gpio_tricolorled);
}

static int gpio_tricolor_led_write (struct file *file, const char *buf,
                                        unsigned long count, void *data)
{
    u_int32_t val, green_led_onoff = 0, yellow_led_onoff = 0;

    if (sscanf(buf, "%d", &val) != 1)
        return -EINVAL;

    if (val >= LED_STATE_MAX)
        return -EINVAL;

    if (val == gpio_tricolorled)
    return count;

    switch (val) {
        case LED_STATE_OFF :
                green_led_onoff = OFF;   /* both LEDs OFF */
                yellow_led_onoff = OFF;
                break;

        case LED_STATE_GREEN:
                green_led_onoff = ON;    /* green ON, Yellow OFF */
                yellow_led_onoff = OFF;
                break;

        case LED_STATE_YELLOW:
                green_led_onoff = OFF;   /* green OFF, Yellow ON */
                yellow_led_onoff = ON;
                break;

        case LED_STATE_ORANGE:
                green_led_onoff = ON;    /* both LEDs ON */
                yellow_led_onoff = ON;
                break;
}

    ar7240_gpio_out_val (TRICOLOR_LED_GREEN_PIN, green_led_onoff);
    //ar7240_gpio_out_val (TRICOLOR_LED_YELLOW_PIN, yellow_led_onoff);
    gpio_tricolorled = val;

    return count;
}


static int create_simple_config_led_proc_entry (void)
{
    if (simple_config_entry != NULL) {
        printk ("Already have a proc entry for /proc/simple_config!\n");
        return -ENOENT;
    }

    simple_config_entry = proc_mkdir("simple_config", NULL);
    if (!simple_config_entry)
        return -ENOENT;

    simulate_push_button_entry = create_proc_entry ("push_button", 0644,
                                                      simple_config_entry);
    if (!simulate_push_button_entry)
        return -ENOENT;

    simulate_push_button_entry->write_proc = push_button_write;
    simulate_push_button_entry->read_proc = push_button_read;

	/* added by zjg, 12Apr10 */
	multi_use_reset_button_entry = create_proc_entry ("multi_use_reset_button", 0644,
                                                      simple_config_entry);
    if (!multi_use_reset_button_entry)
        return -ENOENT;

    multi_use_reset_button_entry->write_proc	= multi_use_reset_button_write;
    multi_use_reset_button_entry->read_proc 	= multi_use_reset_button_read;
	/* end added */

    tricolor_led_entry = create_proc_entry ("tricolor_led", 0644,
                                            simple_config_entry);
    if (!tricolor_led_entry)
        return -ENOENT;

    tricolor_led_entry->write_proc = gpio_tricolor_led_write;
    tricolor_led_entry->read_proc = gpio_tricolor_led_read;

	/* for usb led blink */
	usb_led_blink_entry = create_proc_entry ("usb_blink", 0666,
                                                      simple_config_entry);
	if (!usb_led_blink_entry)
		return -ENOENT;
	
    usb_led_blink_entry->write_proc = usb_led_blink_write;
    usb_led_blink_entry->read_proc = usb_led_blink_read;
	
	/*added by ZQQ, 10.06.02 for usb power*/
	usb_power_entry = create_proc_entry("usb_power", 0666, simple_config_entry);
	if(!usb_power_entry)
		return -ENOENT;

	usb_power_entry->write_proc = usb_power_write;
	usb_power_entry->read_proc = usb_power_read;	
	/*end added*/

    /* configure gpio as outputs */
    ar7240_gpio_config_output (TRICOLOR_LED_GREEN_PIN); 
    //ar7240_gpio_config_output (TRICOLOR_LED_YELLOW_PIN); 

    /* switch off the led */
	/* TRICOLOR_LED_GREEN_PIN is poll up, so ON is OFF modified by tiger 09/07/15 */
    ar7240_gpio_out_val(TRICOLOR_LED_GREEN_PIN, OFF);
    //ar7240_gpio_out_val(TRICOLOR_LED_YELLOW_PIN, OFF);

    return 0;
}



/******* begin ioctl stuff **********/
#ifdef CONFIG_GPIO_DEBUG
void print_gpio_regs(char* prefix)
{
	printk("\n-------------------------%s---------------------------\n", prefix);
	printk("AR7240_GPIO_OE:%#X\n", ar7240_reg_rd(AR7240_GPIO_OE));
	printk("AR7240_GPIO_IN:%#X\n", ar7240_reg_rd(AR7240_GPIO_IN));
	printk("AR7240_GPIO_OUT:%#X\n", ar7240_reg_rd(AR7240_GPIO_OUT));
	printk("AR7240_GPIO_SET:%#X\n", ar7240_reg_rd(AR7240_GPIO_SET));
	printk("AR7240_GPIO_CLEAR:%#X\n", ar7240_reg_rd(AR7240_GPIO_CLEAR));
	printk("AR7240_GPIO_INT_ENABLE:%#X\n", ar7240_reg_rd(AR7240_GPIO_INT_ENABLE));
	printk("AR7240_GPIO_INT_TYPE:%#X\n", ar7240_reg_rd(AR7240_GPIO_INT_TYPE));
	printk("AR7240_GPIO_INT_POLARITY:%#X\n", ar7240_reg_rd(AR7240_GPIO_INT_POLARITY));
	printk("AR7240_GPIO_INT_PENDING:%#X\n", ar7240_reg_rd(AR7240_GPIO_INT_PENDING));
	printk("AR7240_GPIO_INT_MASK:%#X\n", ar7240_reg_rd(AR7240_GPIO_INT_MASK));
	printk("\n-------------------------------------------------------\n");
	}
#endif

/* ioctl for reset default detection and system led switch*/
int ar7240_gpio_ioctl(struct inode *inode, struct file *file,  unsigned int cmd, unsigned long arg)
{
	int i;
	int* argp = (int *)arg;

	if (_IOC_TYPE(cmd) != AR7240_GPIO_MAGIC ||
		_IOC_NR(cmd) < AR7240_GPIO_IOCTL_BASE ||
		_IOC_NR(cmd) > AR7240_GPIO_IOCTL_MAX)
	{
		printk("type:%d nr:%d\n", _IOC_TYPE(cmd), _IOC_NR(cmd));
		printk("ar7240_gpio_ioctl:unknown command\n");
		return -1;
}

	switch (cmd)
{
		case AR7240_GPIO_BTN_READ:
			*argp = counter;
			counter = 0;
			break;
			
		case AR7240_GPIO_LED_READ:
			printk("\n\n");
			for (i = 0; i < AR7240_GPIO_COUNT; i ++)
			{
				printk("pin%d: %d\n", i, ar7240_gpio_in_val(i));
	}
			printk("\n");

			#ifdef CONFIG_GPIO_DEBUG
			print_gpio_regs("");
			#endif
			
			break;
			
		case AR7240_GPIO_LED_WRITE:
			if (unlikely(bBlockWps))
				bBlockWps = 0;
			ar7240_gpio_out_val(SYS_LED_GPIO, *argp);
			break;
			
		default:
			printk("command not supported\n");
			return -1;
	}


	return 0;
}


int ar7240_gpio_open (struct inode *inode, struct file *filp)
{
	int minor = iminor(inode);
	int devnum = minor; //>> 1;
	struct mtd_info *mtd;

	if ((filp->f_mode & 2) && (minor & 1))
{
		printk("You can't open the RO devices RW!\n");
		return -EACCES;
}

	mtd = get_mtd_device(NULL, devnum);   
	if (!mtd)
{
		printk("Can not open mtd!\n");
		return -ENODEV;	
	}
	filp->private_data = mtd;
	return 0;

}

/* struct for cdev */
struct file_operations gpio_device_op =
{
	.owner = THIS_MODULE,
	.ioctl = ar7240_gpio_ioctl,
	.open = ar7240_gpio_open,
};

/* struct for ioctl */
static struct cdev gpio_device_cdev =
{
	.owner  = THIS_MODULE,
	.ops	= &gpio_device_op,
};
/******* end  ioctl stuff **********/



int __init ar7240_simple_config_init(void)
{
    int req;

	init_timer(&rst_timer);
	rst_timer.function = check_rst;

	/* This is NECESSARY, lsz 090109 */
	ar7240_gpio_config_input(JUMPSTART_GPIO);

    /* configure JUMPSTART_GPIO as level triggered interrupt */
    ar7240_gpio_config_int (JUMPSTART_GPIO, INT_TYPE_LEVEL, INT_POL_ACTIVE_LOW);

    req = request_irq (AR7240_GPIO_IRQn(JUMPSTART_GPIO), jumpstart_irq, 0,
                       "SW_JUMPSTART", NULL);
    if (req != 0)
	{
        printk (KERN_ERR "unable to request IRQ for SWJUMPSTART GPIO (error %d)\n", req);
    }

    create_simple_config_led_proc_entry ();

	/* restore factory default and system led */
	dev_t dev;
    int rt;
    int ar7240_gpio_major = gpio_major;
    int ar7240_gpio_minor = gpio_minor;

	ar7240_gpio_config_input(RST_DFT_GPIO);

	/* configure GPIO RST_DFT_GPIO as level triggered interrupt */
    ar7240_gpio_config_int (RST_DFT_GPIO, INT_TYPE_LEVEL, INT_POL_ACTIVE_LOW);

    rt = request_irq (AR7240_GPIO_IRQn(RST_DFT_GPIO), rst_irq, 0,
                       "RESTORE_FACTORY_DEFAULT", NULL);
    if (rt != 0)
	{
        printk (KERN_ERR "unable to request IRQ for RESTORE_FACTORY_DEFAULT GPIO (error %d)\n", rt);
    }

	/* ZJin: write 1 to GPIO6, to give power for USB interface */
	ar7240_gpio_config_output(SYS_USB_POWER_GPIO);
	ar7240_gpio_out_val(SYS_USB_POWER_GPIO, USB_POWER_ON);

	/* configure SYS_LED_GPIO as output led */
	ar7240_gpio_config_output(SYS_LED_GPIO);

#ifdef AP_USB_LED_GPIO
	ar7240_gpio_config_output(AP_USB_LED_GPIO) ;
#endif

    if (ar7240_gpio_major)
	{
        dev = MKDEV(ar7240_gpio_major, ar7240_gpio_minor);
        rt = register_chrdev_region(dev, 1, "ar7240_gpio_chrdev");
    }
	else
	{
        rt = alloc_chrdev_region(&dev, ar7240_gpio_minor, 1, "ar7240_gpio_chrdev");
        ar7240_gpio_major = MAJOR(dev);
    }

    if (rt < 0)
	{
        printk(KERN_WARNING "ar7240_gpio_chrdev : can`t get major %d\n", ar7240_gpio_major);
        return rt;
    }

    cdev_init (&gpio_device_cdev, &gpio_device_op);
    rt = cdev_add(&gpio_device_cdev, dev, 1);
	
    if (rt < 0) 
		printk(KERN_NOTICE "Error %d adding ar7240_gpio_chrdev ", rt);

    return 0;
}

subsys_initcall(ar7240_simple_config_init);

