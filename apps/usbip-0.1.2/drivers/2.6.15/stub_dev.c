/*
 * $Id: stub_dev.c 101 2006-05-16 13:52:41Z taka-hir $
 *
 * Copyright (C) 2003-2006 Takahiro Hirofuchi <taka-hir@is.naist.jp>
 *
 *
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 */

#include <linux/kernel.h>
#include "usbip_common.h"
#include "stub.h"


static int stub_probe(struct usb_interface *interface,  const struct usb_device_id *id);
static void stub_disconnect(struct usb_interface *interface);


/* Now all devices except USB Hub are claimed. */
struct usb_device_id stub_table [] = {
#if 0
	{ USB_DEVICE(0x05ac, 0x0301) },   /* Mac 1 button mouse */
	{ USB_DEVICE(0x0430, 0x0009) },   /* Plat Home Keyboard */
	{ USB_DEVICE(0x059b, 0x0001) },   /* Iomega USB Zip 100 */
	{ USB_DEVICE(0x04b3, 0x4427) },   /* IBM USB CD-ROM */
	{ USB_DEVICE(0x05a9, 0xa511) },   /* LifeView USB cam */
	{ USB_DEVICE(0x55aa, 0x0201) },   /* Imation card reader */
	{ USB_DEVICE(0x046d, 0x0870) },   /* Qcam Express(QV-30) */
	{ USB_DEVICE(0x04bb, 0x0101) },   /* IO-DATA HD 120GB */
	{ USB_DEVICE(0x04bb, 0x0904) },   /* IO-DATA USB-ET/TX */
	{ USB_DEVICE(0x04bb, 0x0201) },   /* IO-DATA USB-ET/TX */
	{ USB_DEVICE(0x08bb, 0x2702) },   /* ONKYO USB Speaker */
	{ USB_DEVICE(0x046d, 0x08b2) },   /* Logicool Qcam 4000 Pro */
#endif
#if 0
	{ .driver_info = 1 },
#endif

#define USB_INTERFACE_INFO_CLASS(cl)	\
	.match_flags = USB_DEVICE_ID_MATCH_INT_CLASS, .bInterfaceClass = (cl)
#if 0
	{ USB_INTERFACE_INFO_CLASS(USB_CLASS_MASS_STORAGE) },
#endif
	{ USB_INTERFACE_INFO_CLASS(USB_CLASS_PRINTER) },
	{ }, 
	{ }			/* Terminating entry */
};

#include <linux/module.h>
MODULE_DEVICE_TABLE (usb, stub_table);

/* usb specific object needed to register this driver with the usb subsystem */
struct usb_driver stub_driver = {
	.name  = "usbip",
	.probe = stub_probe,
	.disconnect = stub_disconnect,
	/* If id_table is null and any other driver claimed,
	 * probe() is always called . */
	.id_table = stub_table,
};



/* ------------------------------------------------------------ */
/* ------------------------------------------------------------ */
/* ------------------------------------------------------------ */


static ssize_t show_status(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct stub_device *sdev = dev_get_drvdata(dev);
	int status;

	if (!sdev) {
		uerr("sdev is null\n");
		return -ENODEV;
	}

	spin_lock(&sdev->ud.lock);
	status = sdev->ud.status;
	spin_unlock(&sdev->ud.lock);

	return snprintf(buf, PAGE_SIZE, "%d\n", status);
}
static DEVICE_ATTR(usbip_status, S_IRUGO, show_status, NULL);

static ssize_t store_sockfd(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct stub_device *sdev = dev_get_drvdata(dev);
	int sockfd = 0;
	struct socket *socket;

	if (!sdev) {
		uerr("sdev is null\n");
		return -ENODEV;
	}

	sscanf(buf, "%u", &sockfd);

	if (sockfd != 0) {
		uinfo("stub up\n");

		spin_lock(&sdev->ud.lock);

		if (sdev->ud.status != SDEV_ST_AVAILABLE) {
			uerr("not ready\n");
			spin_unlock(&sdev->ud.lock);
			return -EINVAL;
		}

		socket = sockfd_to_socket(sockfd);
		if (!socket) {
			spin_unlock(&sdev->ud.lock);
			return -EINVAL;
		}

		/*	hide it by TengFei 10.10.20 *
		setnodelay(socket);
		setkeepalive(socket);
		*/
		setreuse(socket);

		sdev->ud.tcp_socket = socket;

		spin_unlock(&sdev->ud.lock);

		usbip_start_threads(&sdev->ud);

		spin_lock(&sdev->ud.lock);
		sdev->ud.status = SDEV_ST_USED;
		spin_unlock(&sdev->ud.lock);

#if CLIENT > 2
		// added by zl 2011-1-24
		uinfo("store_sockfd:  add timer~~~~~~~~~~~~~~\n");
        sdev->timer.expires = jiffies + HZ * 110; // 50s
        sdev->timer.function = heartbeat_timeout;
        sdev->timer.data = (unsigned long)sdev;
        sdev->heartbeat_timer_running = 1;
		add_timer(&sdev->timer);
		uinfo("store_sockfd:  add timer  over ..................\n");
		// end --- added
#endif
	} else {
		uinfo("stub down\n");

#if CLIENT > 2
		 // added by zl 2011-1-24
        if (sdev->heartbeat_timer_running != 0)
        {
		sdev->heartbeat_timer_running = 0;
		del_timer(&sdev->timer); 
	}
		// end --- added
#endif
		
		spin_lock(&sdev->ud.lock);
		if (sdev->ud.status != SDEV_ST_USED) {
			spin_unlock(&sdev->ud.lock);
			return -EINVAL;
		}
		spin_unlock(&sdev->ud.lock);

		usbip_event_add(&sdev->ud, SDEV_EVENT_DOWN);
	}

	return count;
}

static ssize_t show_sockfd(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct stub_device *sdev = dev_get_drvdata(dev);

	if (!sdev) {
		uerr("sdev is null\n");
		return -ENODEV;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}
static DEVICE_ATTR(usbip_sockfd, S_IWUGO | S_IRUGO, show_sockfd, store_sockfd);

static void stub_add_files(struct device *dev)
{
	device_create_file(dev, &dev_attr_usbip_status);
	device_create_file(dev, &dev_attr_usbip_sockfd);
	device_create_file(dev, &dev_attr_usbip_debug);
}

static void stub_remove_files(struct device *dev)
{
	device_remove_file(dev, &dev_attr_usbip_status);
	device_remove_file(dev, &dev_attr_usbip_sockfd);
	device_remove_file(dev, &dev_attr_usbip_debug);
}



/* ------------------------------------------------------------ */
/* ------------------------------------------------------------ */
/* ------------------------------------------------------------ */

static void stub_shutdown_connection(struct usbip_device *ud)
{
	struct stub_device *sdev = container_of(ud, struct stub_device, ud);

	/* 1. stop threads */
	usbip_stop_threads(ud);

#if CLIENT > 2
	// added by zl 2011-2-15
    	if (sdev->heartbeat_timer_running != 0)
    	{
       	 sdev->heartbeat_timer_running = 0;
        	del_timer(&sdev->timer);      
    	}
   	 // end --- added
#endif

	/* 2. close the socket */
	/*
	 * tcp_socket is freed after threads are killed.
	 * So usbip_xmit do not touch NULL socket.
	 */
	if (ud->tcp_socket != NULL) {
		sock_release(ud->tcp_socket);
		ud->tcp_socket = NULL;
	}

	/* 3. free used data */
	stub_device_cleanup_urbs(sdev);

	/* 4. free stub_unlink */
	{
		unsigned long flags;
		struct stub_unlink *unlink, *tmp;

		spin_lock_irqsave(&sdev->priv_lock, flags);

		list_for_each_entry_safe(unlink, tmp, &sdev->unlink_tx, list) {
			list_del(&unlink->list);
			kfree(unlink);
		}

		list_for_each_entry_safe(unlink, tmp, &sdev->unlink_free, list) {
			list_del(&unlink->list);
			kfree(unlink);
		}

		spin_unlock_irqrestore(&sdev->priv_lock, flags);
	}
}


static void stub_device_reset(struct usbip_device *ud)
{
	struct stub_device *sdev = container_of(ud, struct stub_device, ud);
	struct usb_device *udev = interface_to_usbdev(sdev->interface);
	int ret;

#if CLIENT > 2
	printk("stub_device_reset Enter    heartbeat_timer_running=%d\n", sdev->heartbeat_timer_running);
	// added by zl 2011-2-15
    if (sdev->heartbeat_timer_running != 0)
    {
        sdev->heartbeat_timer_running = 0;
        del_timer(&sdev->timer);      
    }
    // end --- added
   #endif

	ret = usb_lock_device_for_reset(udev, sdev->interface);
	if (ret < 0) {
		uerr("lock for reset\n");

		spin_lock(&ud->lock);
		ud->status = SDEV_ST_ERROR;
		spin_unlock(&ud->lock);

		return;
	}

	/* try to reset the device */
	ret = usb_reset_device(udev);

	usb_unlock_device(udev);

	spin_lock(&ud->lock);
	if (ret) {
		uerr("device reset\n");
		ud->status = SDEV_ST_ERROR;

	} else {
		uinfo("device reset\n");
		ud->status = SDEV_ST_AVAILABLE;

	}
	spin_unlock(&ud->lock);

	return;
}

static void stub_device_unusable(struct usbip_device *ud)
{
	spin_lock(&ud->lock);
	ud->status = SDEV_ST_ERROR;
	spin_unlock(&ud->lock);
}


/* ------------------------------------------------------------ */
/* ------------------------------------------------------------ */
/* ------------------------------------------------------------ */

/**
 * stub_device_alloc - allocate a new stub_device struct
 * @interface: usb_interface of a new device
 *
 * Allocates and initializes a new stub_devce struct.
 */
static struct stub_device * stub_device_alloc(struct usb_interface *interface)
{
	struct stub_device *sdev;

	/* yes, it's a new device */
	sdev = (struct stub_device *) kzalloc(sizeof(struct stub_device), GFP_KERNEL);
	if (!sdev) {
		uerr("no memory for stub_device\n");
		return NULL;
	}

	sdev->interface = interface;

	usbip_task_init(&sdev->ud.tcp_rx, "stub_rx", stub_rx_loop);
	usbip_task_init(&sdev->ud.tcp_tx, "stub_tx", stub_tx_loop);

	sdev->ud.side = USBIP_STUB;
	sdev->ud.status = SDEV_ST_AVAILABLE;
	sdev->ud.lock = SPIN_LOCK_UNLOCKED;
	sdev->ud.tcp_socket = NULL;

#if CLIENT > 2	
	// added by zl 2011-1-24
    sdev->heartbeat_seqNum = 1;
    sdev->heartbeat_pending = 0;
    sdev->heartbeat_timer_running = 0;
    init_timer(&sdev->timer);
    // end --- added
#endif

	INIT_LIST_HEAD(&sdev->priv_init);
	INIT_LIST_HEAD(&sdev->priv_tx);
	INIT_LIST_HEAD(&sdev->priv_free);
	INIT_LIST_HEAD(&sdev->unlink_free);
	INIT_LIST_HEAD(&sdev->unlink_tx);
	sdev->priv_lock = SPIN_LOCK_UNLOCKED;

	init_waitqueue_head(&sdev->tx_waitq);

	sdev->ud.eh_ops.shutdown = stub_shutdown_connection;
	sdev->ud.eh_ops.reset    = stub_device_reset;
	sdev->ud.eh_ops.unusable = stub_device_unusable;

	usbip_start_eh(&sdev->ud);

	udbg("register new interface\n");
	return sdev;
}

static int stub_device_free(struct stub_device *sdev)
{
	if ( !sdev )
		return -EINVAL;

	kfree(sdev);
	udbg("kfree udev ok\n");

	return 0;
}

static int stub_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
	struct usb_device *udev = interface_to_usbdev(interface);
	struct stub_device *sdev = NULL;

	udbg("Enter\n");

	/* We do not claim HUB device */
	if (udev->descriptor.bDeviceClass ==  USB_CLASS_HUB) {
		udbg("HUB device, we do no claim\n");
		return -ENOMEM;
	}

#include <linux/string.h>
	if (strcmp(udev->bus->bus_name, "VHCI") == 0) {
		udbg("dev's bus is VHCI, so do not go anymore!\n");
		return -ENOMEM;
	}


	if ((sdev = stub_device_alloc(interface)) != NULL) {
		struct usb_device *udev = interface_to_usbdev(interface);

		uinfo("USB/IP Stub: new inteface register, bus %u dev %u ifn %u\n",
				udev->bus->busnum, udev->devnum, interface->cur_altsetting->desc.bInterfaceNumber);
	} else {
		uerr("error \n"); return -ENOMEM;
	}


	/* init MUTEX LOCKED here? */


	{
		int i;
		for(i=0; i< interface->num_altsetting; i++) {
			uinfo("alt %u ", interface->altsetting[i].desc.bAlternateSetting);
			printk("NrEp %u ", interface->altsetting[i].desc.bNumEndpoints);
			printk("Cls %x ", interface->altsetting[i].desc.bInterfaceClass);
			printk("SCls %x ", interface->altsetting[i].desc.bInterfaceSubClass);
			printk("Pro %x ", interface->altsetting[i].desc.bInterfaceProtocol);
			printk("\n");
		}
	}
	
	// added by zl 2011-2-9
#if 0
    {
        printk("stub_probe: Mfr=%d, Product=%d, SerialNumber=%d\n",
			udev->descriptor.iManufacturer,
			udev->descriptor.iProduct,
			udev->descriptor.iSerialNumber);
			
        if(udev->product)
        {
            printk("stub_probe:  product----%s \n", udev->product);
        }

        if(udev->manufacturer)
        {
            printk("stub_probe:  manufacturer----%s  \n", udev->manufacturer);
        }

        if(udev->serial)
        {
            printk("stub_probe:  serial----%s   \n", udev->serial);
        }
    }
#endif
    // end ---- added

	/* set private data to usb_interface */
	usb_set_intfdata(interface, sdev);

	stub_add_files(&interface->dev);

	return 0;
}




/* called in usb_disconnect() or usb_deregister()
 * but only if actconfig(active configuration) exists */
static void stub_disconnect(struct usb_interface *interface)
{
	struct stub_device *sdev = usb_get_intfdata(interface);

	udbg("Enter\n");

	/* get stub_device */
	if (!sdev)
		BUG();

	usb_set_intfdata(interface, NULL);

#if CLIENT > 2
	// added by zl 2011-1-24
    if (sdev->heartbeat_timer_running != 0)
    {
        sdev->heartbeat_timer_running = 0;
        del_timer(&sdev->timer);      
    }
    // end --- added
#endif

	/*
	 * NOTE:
	 * rx/tx threads are invoked for each usb_device.
	 */
	stub_remove_files(&interface->dev);

	/* 1. shutdown the current connection */
	usbip_event_add(&sdev->ud, SDEV_EVENT_REMOVED);

	/* 2. wait for the stop of the event handler */
	usbip_stop_eh(&sdev->ud);

	/* 3. free sdev */
	stub_device_free(sdev);


	udbg("bye\n");
}
