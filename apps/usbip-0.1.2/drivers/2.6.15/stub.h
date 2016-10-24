/*
 * $Id: stub.h 97 2006-03-31 16:08:40Z taka-hir $
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

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/slab.h>

#define CLIENT 3

#if CLIENT > 2
#define HEARTBEAT_FAILED_TIMES  4
#define HEARTBEAT_INTERVAL      30  //30s一个心跳包
#endif

struct stub_device {
	struct usb_interface *interface;
	struct list_head list;

	struct usbip_device ud;

	/*
	 * stub_priv preserves private data of each urb.
	 * It is allocated as StubPrivCache and assigned to urb->context.
	 *
	 * stub_priv is always linked to any one of 3 lists;
	 * 	priv_init: linked to this until the comletion of a urb.
	 * 	priv_tx  : linked to this after the completion of a urb.
	 * 	priv_free: linked to this after the sending of the result.
	 *
	 * Any of these list operations should be locked by priv_lock.
	 */
	spinlock_t priv_lock;
	struct list_head priv_init;
	struct list_head priv_tx;
	struct list_head priv_free;

	struct list_head unlink_tx;
	struct list_head unlink_free;


	wait_queue_head_t tx_waitq;
	
#if CLIENT > 2	
	// added by zl 2011-1-24
	struct timer_list timer; 

    int	heartbeat_pending;
    int heartbeat_timer_running;
    int heartbeat_seqNum;
    // end --- added
#endif
};

struct stub_priv {
	unsigned long seqnum;
	struct list_head list;
	struct stub_device *sdev;
	struct urb *urb;

	int unlinking;
};

struct stub_unlink {
	unsigned long seqnum;
	struct list_head list;
	__u32 status;
};

extern kmem_cache_t *StubPrivCache;

void stub_device_cleanup_urbs(struct stub_device *sdev);

/*
 * prototype declarations
 */


/* stub_tx.c */
void stub_complete(struct urb*, struct pt_regs *);
void stub_tx_loop(struct usbip_task *);

#if CLIENT > 2
void heartbeat_timeout(unsigned long data); // added by zl 2011-1-24
#endif

/* stub_dev.c */
extern struct usb_driver stub_driver;

/* stub_rx.c */
void stub_rx_loop(struct usbip_task *);

void stub_enqueue_ret_unlink(struct stub_device *, __u32, __u32);
