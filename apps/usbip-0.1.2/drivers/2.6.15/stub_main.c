/*
 * $Id: stub_main.c 100 2006-03-31 18:39:31Z taka-hir $
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

/* Version Information */
#define DRIVER_VERSION "$Id: stub_main.c 100 2006-03-31 18:39:31Z taka-hir $"
#define DRIVER_AUTHOR "Takahiro Hirofuchi <taka-hir@is.naist.jp>"
#define DRIVER_DESC "Stub Driver for USB/IP"


/* interface for user to attach special devices, added by tf, 110602 */
static __u16 vendor  = 0x0;
static __u16 product = 0x0;

module_param(vendor, ushort, 0);
MODULE_PARM_DESC(vendor, "User specified USB idVendor");

module_param(product, ushort, 0);
MODULE_PARM_DESC(product, "User specified USB idProduct");

/* stub_priv is allocated from StubPrivCache */
kmem_cache_t *StubPrivCache = NULL;

extern struct usb_device_id stub_table [];

static int __init usb_stub_init(void)
{
	int ret;

	StubPrivCache = kmem_cache_create("stub_priv", sizeof(struct stub_priv),
			0, SLAB_HWCACHE_ALIGN, NULL, NULL);
	if (!StubPrivCache) {
		uerr("create stub_priv_cache\n");
		return -ENOMEM;
	}

	/* added by tf, 110602, add special decive to stub table. */
	stub_table[1].idVendor = vendor;
	stub_table[1].idProduct = product;
	stub_table[1].match_flags = USB_DEVICE_ID_MATCH_VENDOR | USB_DEVICE_ID_MATCH_PRODUCT;
	stub_table[1].driver_info = 1;

	ret = usb_register(&stub_driver);
	if (ret) {
		uerr("usb_register failed %d\n", ret);
		return ret;
	}


	info(DRIVER_DESC "" DRIVER_VERSION);
	return ret;
}


static struct stub_priv *stub_priv_pop_from_listhead(struct list_head *listhead)
{
	struct stub_priv *priv, *tmp;

	list_for_each_entry_safe(priv, tmp, listhead, list) {
		list_del(&priv->list);
		return priv;
	}

	return NULL;
}

static struct stub_priv *stub_priv_pop(struct stub_device *sdev)
{
	unsigned long flags;
	struct stub_priv *priv;

	spin_lock_irqsave(&sdev->priv_lock, flags);

	priv = stub_priv_pop_from_listhead(&sdev->priv_init);
	if (priv) {
		spin_unlock_irqrestore(&sdev->priv_lock, flags);
		return priv;
	}

	priv = stub_priv_pop_from_listhead(&sdev->priv_tx);
	if (priv) {
		spin_unlock_irqrestore(&sdev->priv_lock, flags);
		return priv;
	}

	priv = stub_priv_pop_from_listhead(&sdev->priv_free);
	if (priv) {
		spin_unlock_irqrestore(&sdev->priv_lock, flags);
		return priv;
	}

	spin_unlock_irqrestore(&sdev->priv_lock, flags);
	return NULL;
}

void stub_device_cleanup_urbs(struct stub_device *sdev)
{
	struct stub_priv *priv;

	udbg("free sdev %p\n", sdev);
	while ((priv = stub_priv_pop(sdev))) {
		struct urb *urb = priv->urb;
		udbg("   free urb %p\n", urb);

		usb_kill_urb(urb);
		kmem_cache_free(StubPrivCache, priv);
		if (urb->transfer_buffer != NULL)
			kfree(urb->transfer_buffer);
		if (urb->setup_packet != NULL)
			kfree(urb->setup_packet);
		usb_free_urb(urb);

	}
}

static void __exit usb_stub_exit(void)
{
	int ret;

	udbg("enter\n");


	/* deregister() calls stub_disconnect() for all devices. Device
	 * specific data is cleared in stub_disconnect(). */
	usb_deregister(&stub_driver);


	ret = kmem_cache_destroy(StubPrivCache);
	if (ret != 0) {
		uerr("memory leak of stub_priv, %d\n", ret);
	}


	udbg("bye\n");
}




module_init (usb_stub_init);
module_exit (usb_stub_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
