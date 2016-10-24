/*
 * $Id: stub_rx.c 101 2006-05-16 13:52:41Z taka-hir $
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

void set_config_complete(struct urb *urb, struct pt_regs *regs)
{
	kfree(urb->setup_packet);
	usb_free_urb(urb);
}

static void set_configure(struct urb *_urb)
{
	unsigned char setup[] = {0x00, 0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
	struct urb *urb;
	urb = usb_alloc_urb(0, GFP_KERNEL);
	if (!urb) {
		uerr("malloc urb\n");
		/*usbip_event_add(ud, SDEV_EVENT_ERROR_MALLOC);*/
		return;
	}
	urb->transfer_buffer_length = 0;
	memcpy(urb->setup_packet, &setup, 8);
	urb->context			= (void *) urb;
	urb->dev					= _urb->dev;
	urb->pipe				= _urb->pipe;
	urb->complete			= set_config_complete;
	urb->transfer_flags	= 0;
	urb->start_frame		= 0;
	urb->number_of_packets=0;
	urb->interval			= 0;
	usb_submit_urb(urb, GFP_KERNEL);
}

static void check_clear_halt(struct urb *urb)
{
	struct usb_ctrlrequest *req;
	/*if( usb_pipein(urb->pipe) && usb_pipeendpoint(urb->pipe) )
	{
		int targetpipe = usb_rcvctrlpipe(urb->dev, usb_pipeendpoint(urb->pipe));		
		int ret = usb_clear_halt(urb->dev, targetpipe);
		return 0;
	}*/

	if (usb_pipetype(urb->pipe) != PIPE_CONTROL) {
		/*dbg_stub_rx("not clear halt command\n");
		/*uinfo("NOT PIPE_CONTROL!\n");*/
		return;
	}

	if (!urb->setup_packet) {
		/*dbg_stub_rx("no need for check clear halt\n");*/
		return;
	}

	req = (struct usb_ctrlrequest *)urb->setup_packet;
	__u16 alternate = le16_to_cpu(req->wValue);//req->wValue;
	__u16 interface = le16_to_cpu(req->wIndex);//req->wIndex;
	/* wIndex is 2bytes and it has target endpoint number. */
	int endp = interface & 0x000f; /*(req->wIndex & 0x000f );*/
	int in = interface & 0x0080;   /* include USB_DIR_IN bit */

	if (req->bRequest == USB_REQ_CLEAR_FEATURE &&
	    req->bRequestType == USB_RECIP_ENDPOINT &&
	    le16_to_cpu(req->wValue) == USB_ENDPOINT_HALT) {
		/* CLEAR HALT command */
		/* the next line is wrong.
		 * because we must clear halt of stalled endpoint. */
		//int endp = usb_pipeendpoint(urb->pipe);

		//int target_pipe = usb_rcvctrlpipe(urb->dev, endp);
		int target_pipe ;
		int ret;

		if (in) {
			dbg_stub_rx("in\n");
			target_pipe = usb_rcvctrlpipe(urb->dev, endp);
		} else {
			dbg_stub_rx("out\n");
			target_pipe = usb_sndctrlpipe(urb->dev, endp);
		}

		dbg_stub_rx("CLEAR HALT command, devnum %d endp %d\n", urb->dev->devnum, endp);
		ret = usb_clear_halt(urb->dev, target_pipe);
		/*if (ret == 0)
			uinfo("clear halt ep %d ok\n",endp);
		else
			uinfo("clear halt ep %d failed, ret %d\n", endp, ret);*/
		return;
	/*} else if (req->bRequest == USB_REQ_SET_INTERFACE &&
		   req->bRequestType == USB_RECIP_INTERFACE) {
		/* SET INTERFACE command */

		/*dbg_stub_rx("SET INTERFACE command, interface %u alternate %u\n", interface, alternate);
		/*set_configure(urb);*/
		/*usb_set_interface(urb->dev,   interface,  alternate);
		return 0;
	} else if ( req->bRequest == USB_CLASS_APP_SPEC &&
		   req->bRequestType == 0xa1) {
		urb->status = 0;
		urb->actual_length = 1;
		urb->start_frame = 0;
		urb->error_count = 0;
		urb->pipe &= USB_DIR_IN;
		memset(urb->transfer_buffer, 0, urb->actual_length);
		return 1;*/
	} 
	/*dbg_stub_rx("not a clear halt command\n");*/
	return;
}

static void prepare_unlink_urb(struct stub_device *sdev, struct usbip_header *pdu)
{
	struct list_head *listhead = &sdev->priv_init;
	struct list_head *ptr;
	unsigned long  flags;

	struct stub_priv *priv;


	spin_lock_irqsave(&sdev->priv_lock, flags);

	for (ptr = listhead->next; ptr != listhead; ptr = ptr->next) {
		priv = list_entry(ptr, struct stub_priv, list);
		if (priv->seqnum == pdu->u.cmd_unlink.seqnum) {
			int ret;

			uinfo("unlink urb %p\n", priv->urb);

			priv->unlinking = 1;

			/* change to the seqnum of CMD_UNLINK */
			priv->seqnum = pdu->base.seqnum;

			spin_unlock_irqrestore(&sdev->priv_lock, flags);

			ret = usb_unlink_urb(priv->urb);
			if (ret != -EINPROGRESS)
				uerr("faild to unlink a urb %p, ret %d\n", priv->urb, ret);


			return;
		}
	}

	dbg_stub_rx("seqnum %d is not pending\n", pdu->u.cmd_unlink.seqnum);

	stub_enqueue_ret_unlink(sdev, pdu->base.seqnum, 0);

	spin_unlock_irqrestore(&sdev->priv_lock, flags);


	return;
}

/*
 * stub_recv_unlink() unlinks the URB by a call to usb_unlink_urb().
 * By unlinking the urb asynchronously, stub_rx can continuously
 * process coming urbs.  Even if the urb is unlinked, its completion
 * handler will be called and stub_tx will send a return pdu.
 */
static int stub_recv_unlink(struct stub_device *sdev, struct usbip_header *pdu)
{
	prepare_unlink_urb(sdev, pdu);

	return 0;
}

/* hided by TengFei, 10.10.20  start *
static int valid_request(struct stub_device *sdev, struct usbip_header *pdu)
{
	struct usbip_device *ud = &sdev->ud;

	int bus = interface_to_busnum(sdev->interface);
	int dev = interface_to_devnum(sdev->interface);

	if (pdu->base.busnum == bus && pdu->base.devnum == dev) {
		spin_lock(&ud->lock);
		if (ud->status == SDEV_ST_USED) {
			// A request is valid. 
			spin_unlock(&ud->lock);
			return 1;
		}
		spin_unlock(&ud->lock);
	}

	return 0;
}
 * hided by TengFei, 10.10.20   end  */


/* added by TengFei, 10.10.20  start */
static int valid_request(struct stub_device *sdev, struct usbip_header *pdu)
{
	struct usbip_device *ud = &sdev->ud;
	int bus = interface_to_busnum(sdev->interface);
	int dev = interface_to_devnum(sdev->interface);
	int sdev_devid = (bus << 16) | dev;

	/*udbg("pdu->base.devid == %d\tsdev_devid == %d\n", pdu->base.devid, sdev_devid);/*tf 101222*/
	
	if (pdu->base.devid == sdev_devid) {
		spin_lock(&ud->lock);
		if (ud->status == SDEV_ST_USED) {
			/* A request is valid. */
			spin_unlock(&ud->lock);
			return 1;
		}
		spin_unlock(&ud->lock);
	}

	return 0;
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)
static inline int usb_endpoint_xfer_bulk(
		const struct usb_endpoint_descriptor *epd)
{
	return ((epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
			USB_ENDPOINT_XFER_BULK);
}
static inline int usb_endpoint_xfer_control(
		const struct usb_endpoint_descriptor *epd)
{
	return ((epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
			USB_ENDPOINT_XFER_CONTROL);
}
static inline int usb_endpoint_xfer_int(
		const struct usb_endpoint_descriptor *epd)
{
	return ((epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
			USB_ENDPOINT_XFER_INT);
}
static inline int usb_endpoint_xfer_isoc(
		const struct usb_endpoint_descriptor *epd)
{
	return ((epd->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) ==
			USB_ENDPOINT_XFER_ISOC);
}
#endif

static struct usb_host_endpoint *get_ep_from_epnum(struct usb_device *udev,
		int epnum0)
{
	struct usb_host_config *config;
	int i = 0, j = 0;
	struct usb_host_endpoint *ep = NULL;
	int epnum;
	int found = 0;

	if (epnum0 == 0)
		return &udev->ep0;

	config = udev->actconfig;
	if (!config)
		return NULL;

	for (i = 0; i < config->desc.bNumInterfaces; i++) {
		struct usb_host_interface *setting;

		setting = config->interface[i]->cur_altsetting;

		for (j = 0; j < setting->desc.bNumEndpoints; j++) {
			ep = &setting->endpoint[j];
			epnum = (ep->desc.bEndpointAddress & 0x7f);

			if (epnum == epnum0) {
				//uinfo("found epnum %d\n", epnum0);
				found = 1;
				break;
			}
		}
	}

	if (found)
		return ep;
	else
		return NULL;
}

static int get_pipe(struct stub_device *sdev, int epnum, int dir)
{
	struct usb_device *udev = interface_to_usbdev(sdev->interface);
	struct usb_host_endpoint *ep;
	struct usb_endpoint_descriptor *epd = NULL;

	ep = get_ep_from_epnum(udev, epnum);
	if (!ep) {
		uerr("no such endpoint?, %d", epnum);
		BUG();
	}

	epd = &ep->desc;


#if 0
	/* epnum 0 is always control */
	if (epnum == 0) {
		if (dir == USBIP_DIR_OUT)
			return usb_sndctrlpipe(udev, 0);
		else
			return usb_rcvctrlpipe(udev, 0);
	}
#endif

	if (usb_endpoint_xfer_control(epd)) {
		if (dir == USBIP_DIR_OUT)
			return usb_sndctrlpipe(udev, epnum);
		else
			return usb_rcvctrlpipe(udev, epnum);
	}

	if (usb_endpoint_xfer_bulk(epd)) {
		if (dir == USBIP_DIR_OUT)
			return usb_sndbulkpipe(udev, epnum);
		else
			return usb_rcvbulkpipe(udev, epnum);
	}

	if (usb_endpoint_xfer_int(epd)) {
		if (dir == USBIP_DIR_OUT)
			return usb_sndintpipe(udev, epnum);
		else
			return usb_rcvintpipe(udev, epnum);
	}

	if (usb_endpoint_xfer_isoc(epd)) {
		if (dir == USBIP_DIR_OUT)
			return usb_sndisocpipe(udev, epnum);
		else
			return usb_rcvisocpipe(udev, epnum);
	}

	/* NOT REACHED */
	uerr("get pipe, epnum %d\n", epnum);
	return 0;
}

/* added by TengFei, 10.10.20  end */

/* modified by TengFei, 10.10.20  start */
static void stub_recv_submit(struct stub_device *sdev, struct usbip_header *pdu)
{
	int ret;
	struct stub_priv *priv = NULL;
	struct usbip_device *ud = &sdev->ud;
	int pipe = get_pipe(sdev, pdu->base.ep, pdu->base.direction);

	/*
	 * After a stub_priv is linked to a list_head,
	 * the error handler can free allocated data.
	 */
	{
		unsigned long flags;

		spin_lock_irqsave(&sdev->priv_lock, flags);

		priv = kmem_cache_alloc(StubPrivCache, GFP_ATOMIC);
		if (!priv) {
			uerr("malloc stub_priv\n");
			spin_unlock_irqrestore(&sdev->priv_lock, flags);
			usbip_event_add(ud, SDEV_EVENT_ERROR_MALLOC);
			return;
		}
		memset(priv, 0, sizeof(struct stub_priv));

		priv->seqnum = pdu->base.seqnum;
		priv->sdev = sdev;

		list_add_tail(&priv->list, &sdev->priv_init);
		spin_unlock_irqrestore(&sdev->priv_lock, flags);
	}

	/*
	 * setup a urb
	 */
	if (usb_pipeisoc(pipe))
		priv->urb = usb_alloc_urb(pdu->u.cmd_submit.number_of_packets, GFP_KERNEL);
	else
		priv->urb = usb_alloc_urb(0, GFP_KERNEL);

	if (!priv->urb) {
		uerr("malloc urb\n");
		usbip_event_add(ud, SDEV_EVENT_ERROR_MALLOC);
		return;
	}

	/* set priv->urb->transfer_buffer */
	if (pdu->u.cmd_submit.transfer_buffer_length > 0) {
		priv->urb->transfer_buffer = kzalloc(pdu->u.cmd_submit.transfer_buffer_length, GFP_KERNEL);
		if (!priv->urb->transfer_buffer) {
			uerr("malloc x_buff\n");
			usbip_event_add(ud, SDEV_EVENT_ERROR_MALLOC);
			return;
		}
	}

	/* set priv->urb->setup_packet */
	{
		priv->urb->setup_packet = kzalloc(8, GFP_KERNEL);
		if (!priv->urb->setup_packet) {
			uerr("allocate setup_packet\n");
			usbip_event_add(ud, SDEV_EVENT_ERROR_MALLOC);
			return;
		}
		/*unsigned char setup[8];
		setup[0] = pdu->u.cmd_submit.setup[0];
		setup[1] = pdu->u.cmd_submit.setup[1];
		setup[2] = pdu->u.cmd_submit.setup[3];
		setup[3] = pdu->u.cmd_submit.setup[2];
		setup[4] = pdu->u.cmd_submit.setup[5];
		setup[5] = pdu->u.cmd_submit.setup[4];
		setup[6] = pdu->u.cmd_submit.setup[7];
		setup[7] = pdu->u.cmd_submit.setup[6];
		memcpy(priv->urb->setup_packet, &setup, 8);*/
		memcpy(priv->urb->setup_packet, &pdu->u.cmd_submit.setup, 8);
	}

	priv->urb->context                = (void *) priv;
	priv->urb->dev                    = interface_to_usbdev(sdev->interface);
	priv->urb->pipe                   = pipe;
	priv->urb->complete               = stub_complete;

	usbip_pack_pdu(pdu, priv->urb, USBIP_CMD_SUBMIT, 0);
	priv->urb->transfer_flags &= (~URB_SHORT_NOT_OK); 	/* added by tf, necessarily */

	if (usbip_recv_xbuff(ud, priv->urb) < 0)
		return;

	if (usbip_recv_iso(ud, priv->urb) < 0)
		return;

	/*int retu = 0;*/
	check_clear_halt(priv->urb);
	/*if (retu == 1){
		uinfo("begin stub_complete!\n");
		stub_complete(priv->urb, NULL);
	} else {*/
	ret = usb_submit_urb(priv->urb, GFP_KERNEL);

		if (ret == 0) {
			dbg_stub_rx("submit urb ok, seqnum %u\n", pdu->base.seqnum);
		} else {
			uerr("submit_urb error, %d\n", ret);
		/*
		 * Pessimistic.
		 * This connection will be discared.
		 */
			usbip_event_add(ud, SDEV_EVENT_ERROR_SUBMIT);
		}
	/*}*/
	dbg_stub_rx("Leave\n");
	return;
}
/* modified by TengFei, 10.10.20  start */

/* recv a pdu */
static void stub_rx_pdu(struct usbip_device *ud)
{
	int ret;
	struct usbip_header pdu;
	struct stub_device *sdev = container_of(ud, struct stub_device, ud);


	dbg_stub_rx("Enter\n");

	memset(&pdu, 0, sizeof(pdu));


	/* 1. recieve a pdu header */
	ret = usbip_xmit(0, ud->tcp_socket, (char *) &pdu, sizeof(pdu),0);
	if (ret != sizeof(pdu)) {
		uerr("recv a header, %d\n", ret);
		usbip_event_add(ud, SDEV_EVENT_ERROR_TCP);
		return;
	}

	usbip_header_correct_endian(&pdu, 0);	/* added by TengFei, 10.10.20 */

	if (dbg_flag_stub_rx)
		usbip_dump_header(&pdu);

	if (!valid_request(sdev, &pdu)) {
		uerr("recv invalid request\n");
		/*usbip_event_add(ud, SDEV_EVENT_ERROR_TCP); tf 101222*/
		return;
	}

	switch (pdu.base.command) {
		case USBIP_CMD_UNLINK:
			stub_recv_unlink(sdev, &pdu);
			break;

		case USBIP_CMD_SUBMIT:
			stub_recv_submit(sdev, &pdu);
			break;
			
#if CLIENT > 2	
		// added by zl 2011-1-24
		case USBIP_HEARTBEAT_REPLY:
		    //还可添加代码进一步确认是否为心跳包
		    sdev->heartbeat_pending = 0;
			break;
        // end --- added
#endif	

		default:
			/* NOTREACHED */
			uerr("unknown pdu\n");
			usbip_event_add(ud, SDEV_EVENT_ERROR_TCP);
			return;
	}

}

void stub_rx_loop(struct usbip_task *ut)
{
	struct usbip_device *ud = container_of(ut, struct usbip_device, tcp_rx);

	while (1) {
		if (signal_pending(current)) {
			dbg_stub_rx("signal catched!\n");
			break;
		}

		if (usbip_event_happend(ud))
			break;

		stub_rx_pdu(ud);
	}
}

