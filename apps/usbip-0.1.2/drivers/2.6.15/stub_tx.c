/*
 * $Id: stub_tx.c 100 2006-03-31 18:39:31Z taka-hir $
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

#include "usbip_common.h"
#include "stub.h"

int clear_halt_flag = 0;
unsigned int clear_halt_pipe;
struct usb_device *clear_halt_dev;

static void stub_free_priv_and_urb(struct stub_priv *priv)
{
	struct urb *urb = priv->urb;

	if (urb->setup_packet)
		kfree(urb->setup_packet);

	if (urb->transfer_buffer)
		kfree(urb->transfer_buffer);

	list_del(&priv->list);
	kmem_cache_free(StubPrivCache, priv);

	usb_free_urb(urb);
}

/* be in spin_lock_irqsave(&sdev->priv_lock, flags) */
void stub_enqueue_ret_unlink(struct stub_device *sdev, __u32 seqnum, __u32 status)
{
	struct stub_unlink *unlink;

	unlink = kzalloc(sizeof(struct stub_unlink), GFP_ATOMIC);
	if (!unlink) {
		uerr("alloc stub_unlink\n");
		usbip_event_add(&sdev->ud, VDEV_EVENT_ERROR_MALLOC);
		return;
	}

	unlink->seqnum = seqnum;
	unlink->status = status;

	list_add_tail(&unlink->list, &sdev->unlink_tx);
}

/**
 * stub_complete - completion handler of a usbip urb
 * @urb: pointer to the urb completed
 * @regs:
 *
 * When a urb has completed, the USB core driver calls this function in the
 * interrupt context. To return the result of a urb, the completed urb is
 * linked to the pending list of returning.
 *
 */
void stub_complete(struct urb *urb, struct pt_regs *regs)
{
	struct stub_priv *priv = (struct stub_priv *) urb->context;
	struct stub_device *sdev = priv->sdev;
	unsigned long flags;

	dbg_stub_tx("complete! status %d\n", urb->status);


	switch (urb->status) {
		case 0:
			/* OK */
			break;
		case -ENOENT:
			uinfo("stopped by a call to usb_kill_urb()\n");
			uinfo("because of cleaning up a virtual connection\n");
			return;
		case -ECONNRESET:
			uinfo("unlinked by a call to usb_unlink_urb()\n");
			break;
		case -EPIPE:
			uinfo("endpoint is stalled\n");
			/*clear_halt_flag = 1;
			clear_halt_pipe = urb->pipe;
			clear_halt_dev = urb->dev;
			unsigned char _setup[9] = {0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
			_setup[5] = epnum;
			if (usb_pipein(urb->pipe)) 
				_setup[5] = 0x80 | epnum;
			urb->transfer_flags = 0;
			urb->transfer_buffer_length = 0;
			usb_submit_urb(urb, GFP_KERNEL);*/
			break;
		default:
			uerr("NOT YET: completion with non-zero status %d\n", urb->status);
	}

	/* link a urb to the queue of tx. */
	spin_lock_irqsave(&sdev->priv_lock, flags);

	if (priv->unlinking) {
		stub_enqueue_ret_unlink(sdev, priv->seqnum, urb->status);
		stub_free_priv_and_urb(priv);
	} else {
		list_move_tail(&priv->list, &sdev->priv_tx);
	}

	spin_unlock_irqrestore(&sdev->priv_lock, flags);

	/* wake up tx_thread */
	wake_up(&sdev->tx_waitq);
}





static void setup_ret_submit_pdu(struct usbip_header *rpdu, struct urb *urb)
{
	struct stub_priv *priv = (struct stub_priv *) urb->context;

	rpdu->base.command = USBIP_RET_SUBMIT;
	/* rpdu->base.pipe = urb->pipe; */
	rpdu->base.devid = 0;
	rpdu->base.ep = 0;
	rpdu->base.direction = 0;
	rpdu->base.seqnum = priv->seqnum;

	usbip_pack_pdu(rpdu, urb, USBIP_RET_SUBMIT, 1);
}

static struct stub_priv *dequeue_from_priv_tx(struct stub_device *sdev)
{
	unsigned long flags;
	struct stub_priv *priv, *tmp;

	spin_lock_irqsave(&sdev->priv_lock, flags);

	list_for_each_entry_safe(priv, tmp, &sdev->priv_tx, list) {
		list_move_tail(&priv->list, &sdev->priv_free);
		spin_unlock_irqrestore(&sdev->priv_lock, flags);
		return priv;
	}

	spin_unlock_irqrestore(&sdev->priv_lock, flags);

	return NULL;
}

static int stub_send_ret_submit(struct stub_device *sdev)
{
	unsigned long flags;
	struct stub_priv *priv, *tmp;

	struct msghdr msg;
	struct kvec iov[3];
	size_t txsize;

	size_t total_size = 0;

	while ((priv = dequeue_from_priv_tx(sdev)) != NULL) {
		int ret;
		struct urb *urb = priv->urb;
		struct usbip_header pdu_header;
		void *iso_buffer = NULL;

		txsize = 0;
		memset(&pdu_header, 0, sizeof(pdu_header));
		memset(&msg, 0, sizeof(msg));
		memset(&iov, 0, sizeof(iov));

		/*dbg_stub_tx("setup txdata urb %p\n", urb);*/


		/* 1. setup usbip_header */
		setup_ret_submit_pdu(&pdu_header, urb);
		usbip_header_correct_endian(&pdu_header, 1);

		/*usbip_dump_header(&pdu_header);/* tf 101222*/

		iov[0].iov_base = &pdu_header;
		iov[0].iov_len  = sizeof(pdu_header);
		txsize += sizeof(pdu_header);

		/* 2. setup transfer buffer */
		if (usb_pipein(urb->pipe) && urb->actual_length > 0) {
			iov[1].iov_base = urb->transfer_buffer;
			iov[1].iov_len  = urb->actual_length;
			txsize += urb->actual_length;
		}

		/* 3. setup iso_packet_descriptor */
		if (usb_pipetype(urb->pipe) == PIPE_ISOCHRONOUS) {
			ssize_t len = 0;

			iso_buffer = usbip_alloc_iso_desc_pdu(urb, &len);
			if (!iso_buffer) {
				usbip_event_add(&sdev->ud, SDEV_EVENT_ERROR_MALLOC);
				return -1;
			}
			
			iov[2].iov_base = iso_buffer;/*&urb->iso_frame_desc[0];*/
			iov[2].iov_len  = len;/*urb->number_of_packets * sizeof(struct usb_iso_packet_descriptor);*/
			txsize += len;/*urb->number_of_packets * sizeof(struct usb_iso_packet_descriptor);*/
		}

		ret = kernel_sendmsg(sdev->ud.tcp_socket, &msg, iov, 3, txsize);
		if (ret != txsize) {
			uerr("sendmsg failed!, retval %d for %d\n", ret, txsize);
			if (iso_buffer)
				kfree(iso_buffer);
			usbip_event_add(&sdev->ud, SDEV_EVENT_ERROR_TCP);
			return -1;
		}

		if (iso_buffer)
			kfree(iso_buffer);
		/*dbg_stub_tx("send txdata\n");*/

		total_size += txsize;

		/* added by tf, clear halt endpoint */
		if (urb->status == -EPIPE)
		{
			int ok = usb_clear_halt(urb->dev, urb->pipe);
			if (ok == 0)
				uinfo("tx: clr halt ep %d\n", usb_pipeendpoint(urb->pipe));
			else 
				uinfo("tx: clr ep %d failed\n", usb_pipeendpoint(urb->pipe));
		}
	}


	spin_lock_irqsave(&sdev->priv_lock, flags);

	list_for_each_entry_safe(priv, tmp, &sdev->priv_free, list) {
		stub_free_priv_and_urb(priv);
	}

	spin_unlock_irqrestore(&sdev->priv_lock, flags);

	return total_size;
}



static struct stub_unlink *dequeue_from_unlink_tx(struct stub_device *sdev)
{
	unsigned long flags;
	struct stub_unlink *unlink, *tmp;

	spin_lock_irqsave(&sdev->priv_lock, flags);

	list_for_each_entry_safe(unlink, tmp, &sdev->unlink_tx, list) {
		list_move_tail(&unlink->list, &sdev->unlink_free);
		spin_unlock_irqrestore(&sdev->priv_lock, flags);
		return unlink;
	}

	spin_unlock_irqrestore(&sdev->priv_lock, flags);

	return NULL;
}

static int stub_send_ret_unlink(struct stub_device *sdev)
{
	unsigned long flags;
	struct stub_unlink *unlink, *tmp;

	struct msghdr msg;
	struct kvec iov[1];
	size_t txsize;

	size_t total_size = 0;

	while ((unlink = dequeue_from_unlink_tx(sdev)) != NULL) {
		int ret;
		struct usbip_header pdu_header;

		txsize = 0;
		memset(&pdu_header, 0, sizeof(pdu_header));
		memset(&msg, 0, sizeof(msg));
		memset(&iov, 0, sizeof(iov));

		dbg_stub_tx("setup ret unlink %lu\n", unlink->seqnum);

		/* 1. setup usbip_header */
		pdu_header.base.command = USBIP_RET_UNLINK;
		pdu_header.base.seqnum  = unlink->seqnum;
		pdu_header.base.devid = 0;
		pdu_header.base.ep = 0;
		pdu_header.base.direction = 0;
		pdu_header.u.ret_unlink.status = unlink->status;
		usbip_header_correct_endian(&pdu_header, 1);

		iov[0].iov_base = &pdu_header;
		iov[0].iov_len  = sizeof(pdu_header);
		txsize += sizeof(pdu_header);

		ret = kernel_sendmsg(sdev->ud.tcp_socket, &msg, iov, 3, txsize);
		if (ret != txsize) {
			uerr("sendmsg failed!, retval %d for %d\n", ret, txsize);
			usbip_event_add(&sdev->ud, SDEV_EVENT_ERROR_TCP);
			return -1;
		}


		dbg_stub_tx("send txdata\n");

		total_size += txsize;
	}


	spin_lock_irqsave(&sdev->priv_lock, flags);

	list_for_each_entry_safe(unlink, tmp, &sdev->unlink_free, list) {
		list_del(&unlink->list);
		kfree(unlink);
	}

	spin_unlock_irqrestore(&sdev->priv_lock, flags);

	return total_size;
}

#if CLIENT > 2
// added by zl 2011-1-24
void heartbeat_timeout(unsigned long data)
{
    struct stub_device * sdev = (struct stub_device *)data;
    char tmp[]= "server: How are you?";


    uinfo("heartbeat_timeout Enter..............\n");

    if (NULL == sdev || NULL == sdev->ud.tcp_socket)
    {
        return ;
    }  
    
    if (sdev->heartbeat_timer_running != 0)
    {
        
        if (sdev->heartbeat_pending >= HEARTBEAT_FAILED_TIMES)
        {// 网络连接有问题 
            sdev->heartbeat_timer_running = 0;
            sdev->heartbeat_pending = 0;
            usbip_event_add(&sdev->ud, SDEV_EVENT_ERROR_TCP);
        }
        else
        {
	        struct msghdr msg;
	        struct kvec iov[1];
		    struct usbip_header pdu_header;
		    size_t txsize = sizeof(pdu_header);

		    memset(&pdu_header, 0, sizeof(pdu_header));
		    memset(&msg, 0, sizeof(msg));
		    memset(&iov, 0, sizeof(iov));

		    dbg_stub_tx("send HEARTBEAT-----\n");
		    
            if(sdev->heartbeat_seqNum == 0xffff)
            {
                sdev->heartbeat_seqNum = 1;
            }
		    /* setup usbip_header */
		    pdu_header.base.command = USBIP_HEARTBEAT_ECHO;
		    pdu_header.base.seqnum = sdev->heartbeat_seqNum++;
		    memcpy(&pdu_header.u.cmd_submit, tmp, sizeof(tmp));
		    usbip_header_correct_endian(&pdu_header, 1);

    		iov[0].iov_base = &pdu_header;
    		iov[0].iov_len  = sizeof(pdu_header);

    		printk("send HEARTBEAT  :%d-----pending:%d\n", 
    		        sdev->heartbeat_seqNum - 1, sdev->heartbeat_pending + 1);

            kernel_sendmsg(sdev->ud.tcp_socket, &msg, iov, 1, txsize);
            /*
    		ret = kernel_sendmsg(sdev->ud.tcp_socket, &msg, iov, 3, txsize);
    		if (ret != txsize) {
    			uerr("sendmsg failed!, retval %d for %d\n", ret, txsize);
    			usbip_event_add(&sdev->ud, SDEV_EVENT_ERROR_TCP);
    			return -1;
    		}
    		*/
    		
    		sdev->heartbeat_pending++;
            mod_timer(&sdev->timer, jiffies + HZ * HEARTBEAT_INTERVAL);
            sdev->heartbeat_timer_running = 1;   
        }
    }
}
// end --- added
#endif

void stub_tx_loop(struct usbip_task *ut)
{
	struct usbip_device *ud = container_of(ut, struct usbip_device, tcp_tx);
	struct stub_device *sdev = container_of(ud, struct stub_device, ud);

	while (1) {
		if (signal_pending(current)) {
			dbg_stub_tx("signal catched\n");
			break;
		}

		if (usbip_event_happend(ud))
			break;

		/*
		 * send_ret_submit comes earlier than send_ret_unlink.  stub_rx
		 * looks at only priv_init queue. If the completion of a URB is
		 * earlier than the recieve of CMD_UNLINK, priv is moved to
		 * priv_tx queue and stub_rx does not find the target priv. In
		 * this case, vhci_rx recieves the result of the submit request
		 * and then recieves the result of the unlink request. The
		 * result of the submit is given back to the usbcore as the
		 * completion of the unlink request. The request of the
		 * unlink is ignored. This is ok because a driver who calls
		 * usb_unlink_urb() understands the unlink was too late by
		 * getting the status of the given-backed URB which has the
		 * status of usb_submit_urb().
		 */
		
		if (stub_send_ret_submit(sdev) < 0)
			break;

		if (stub_send_ret_unlink(sdev) < 0)
			break;

		wait_event_interruptible(sdev->tx_waitq,
				(!list_empty(&sdev->priv_tx) || !list_empty(&sdev->unlink_tx)) );
	}
}
