/*
 * $Id: vhci.h 97 2006-03-31 16:08:40Z taka-hir $
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

#include <linux/platform_device.h>
#include HCD_HEADER

#define VHCI_DEVICE_INFO_SIZE 80

struct vhci_device {
	struct usb_device *udev;

	__u32 busnum; /* remote bus num */
	__u32 devnum; /* remote dev num */
	__u32 infnum;

	enum usb_device_speed speed;

	char info[VHCI_DEVICE_INFO_SIZE];

	__u32 rhport; /* root hub port number */

	struct usbip_device ud;

	/* vhci_priv is linked to one of them. */
	spinlock_t priv_lock;
	struct list_head priv_tx;
	struct list_head priv_rx;

	/* vhci_unlink is linked to one of them */
	struct list_head unlink_tx;
	struct list_head unlink_rx;

	wait_queue_head_t waitq;
};


/* urb->hcpriv, use container_of() */
struct vhci_priv {
	unsigned long seqnum;
	struct list_head list;

	struct vhci_device *vdev;
	struct urb *urb;
};


struct vhci_unlink {
	/* seqnum of this request */
	unsigned long seqnum;

	struct list_head list;

	/* seqnum of the unlink target */
	unsigned long unlink_seqnum;
};

/*
 * The number of ports is less than 16 ?
 * USB_MAXCHILDREN is statically defined to 16 in usb.h.  Its maximum value
 * would be 31 because the event_bits[1] of struct usb_hub is defined as
 * unsigned long in hub.h
 */
#define VHCI_NPORTS 8

/* for usb_bus.hcpriv */
struct vhci_hcd {
	struct usb_hcd hcd;  /* must come first! */
	spinlock_t	lock;

	struct platform_device pdev;

	u32	port_status[VHCI_NPORTS];
	int started;
	struct completion released;
	unsigned	resuming:1;
	unsigned long	re_timeout;

	atomic_t seqnum;

	/*
	 * NOTE:
	 * wIndex shows the port number and begins from 1.
	 * But, the index of this array begins from 0.
	 */
	struct vhci_device vdev[VHCI_NPORTS];

	/* vhci_device which has not been assiged its address yet */
	int pending_port;
};



/* vhci_hcd.c */
void rh_port_connect(int rhport, enum usb_device_speed speed);
void rh_port_disconnect(int rhport);
extern struct vhci_hcd *the_controller;
#define hardware        (&the_controller->pdev.dev)

struct vhci_device *port_to_vdev(__u32 port);

void vhci_rx_loop(struct usbip_task *ut);
void vhci_tx_loop(struct usbip_task *ut);


/* vhci_sysfs.c */
extern struct attribute_group dev_attr_group;
