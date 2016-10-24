/*
 * $Id: vhci_sysfs.c 97 2006-03-31 16:08:40Z taka-hir $
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

#include <linux/in.h>
#include <linux/in6.h>
#include "usbip_common.h"
#include "vhci.h"


struct vhci_device *port_to_vdev(__u32 port)
{
	return &the_controller->vdev[port];
}


static int vhci_proc_vcdown(__u32 rhport)
{
	struct vhci_device *vdev;

	dbg_vhci_sysfs("enter\n");
	
	spin_lock(&the_controller->lock);
	vdev = port_to_vdev(rhport);
	spin_lock(&vdev->priv_lock);

	if (vdev->ud.status == VDEV_ST_NULL) {
		uerr("not connected %d\n", vdev->ud.status);
		spin_unlock(&vdev->priv_lock);
		spin_unlock(&the_controller->lock);
		return -EINVAL;
	}

	spin_unlock(&vdev->priv_lock);
	spin_unlock(&the_controller->lock);

	usbip_event_add(&vdev->ud, VDEV_EVENT_DOWN);

	return 0;
}

static int vhci_proc_status(char *out)
{
	char *s = out;
	int i = 0;

	/*
	 * prt sta bus dev ipaddr                                  port   bus_id(remote/local)
	 * 000 004 000 000 xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:xxxx xxxxxx xxxxx... xxxxx...
	 * 001 004 000 000 xxx.xxx.xxx.xxx                         xxxxxx xxxxx... xxxxx...   
	 */

	out += sprintf(out, "prt sta spd bus dev ipaddr                                  port   bus_id(r/l)\n");

	if (!the_controller) {
		uerr("the_controller is NULL\n");
		return 0;
		}


	for (i=0; i < VHCI_NPORTS; i++) {
		struct vhci_device *vdev = port_to_vdev(i);

		spin_lock(&vdev->ud.lock);

		out += sprintf(out, "%03u %03u ", i, vdev->ud.status);

		if (vdev->ud.status == VDEV_ST_USED) {
			out += sprintf(out, "%03u %03u %03u ", vdev->speed, vdev->busnum, vdev->devnum);

			if (vdev->ud.tcp_ss.ss_family == AF_INET) {
				out += sprintf(out, "%03u.%03u.%03u.%03u                         ",
						NIPQUAD(ss_v4_addr(vdev->ud.tcp_ss)));
				out += sprintf(out, "%06d ", ntohs(ss_v4_port(vdev->ud.tcp_ss)));

			} else if (vdev->ud.tcp_ss.ss_family == AF_INET6) {
				out += sprintf(out, "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x ",
						NIP6(ss_v6_addr(vdev->ud.tcp_ss)));
				out += sprintf(out, "%06d ", ntohs(ss_v6_port(vdev->ud.tcp_ss)));

			} else {
				uerr("unknown ss_family %d\n", vdev->ud.tcp_ss.ss_family);
			}

			out += sprintf(out, "%s ", vdev->info);
			out += sprintf(out, "%s", vdev->udev->dev.bus_id);

		} else {
			out += sprintf(out, "000 000 000 0000:0000:0000:0000:0000:0000:0000:0000 000000 xxx xxx");
		}


		out += sprintf(out, "\n");

		spin_unlock(&vdev->ud.lock);
	}


	return out - s ;
}






static int valid_args(__u32 rhport, __u32 busnum, __u32 devnum, enum usb_device_speed speed)
{

	/* check rhport */
	if ((rhport < 0) || (rhport >= VHCI_NPORTS)) {
		uerr("invalid port %u\n", rhport);
		return -EINVAL;
	}

	/* check busnum & devnum */
	if ((busnum<=0) || (busnum>=128) || (devnum<=0) || (devnum>=128)) {
		uerr("invalid busnum or portnum\n");
		return -EINVAL;
	}

	/* check speed */
	switch(speed) {
		case USB_SPEED_LOW:
		case USB_SPEED_FULL:
		case USB_SPEED_HIGH:
			break;

		default:
			uerr("invalid speed\n");
			return -EINVAL;
	}

	return 0;
}



/* -------------------------------------------- */

static ssize_t show_status(struct device *dev, struct device_attribute *attr, char *buf)
{
	return vhci_proc_status(buf);
}
static DEVICE_ATTR(status, S_IRUGO, show_status, NULL);

static ssize_t store_detach(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int err;
	__u32 rhport = 0;

	sscanf(buf, "%u", &rhport);

	/* check rhport */
	if ((rhport < 0) || (rhport >= VHCI_NPORTS)) {
		uerr("invalid port %u\n", rhport);
		return -EINVAL;
	}

	err = vhci_proc_vcdown(rhport);
	if (err < 0) {
		return -EINVAL;
	}

	dbg_vhci_sysfs("Leave\n");
	return count;
}
static DEVICE_ATTR(detach, S_IWUSR, NULL, store_detach);

static ssize_t store_attach(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct socket *socket;
	char info[VHCI_DEVICE_INFO_SIZE];
	__u32 rhport=0, sockfd=0, busnum=0, devnum=0, speed=0;
	struct vhci_device *vdev;

	memset(info, 0, VHCI_DEVICE_INFO_SIZE);
	sscanf(buf, "%u %u %u %u %u %s", &rhport, &sockfd, &busnum, &devnum, &speed, info);

	dbg_vhci_sysfs("rhport(%u) sockfd(%u) busnum(%u) devnum(%u) speed(%u)\n",
			rhport, sockfd, busnum, devnum, speed);

	if (valid_args(rhport, busnum, devnum, speed) < 0) {
		return -EINVAL;
	}

	/* check sockfd */
	socket = sockfd_to_socket(sockfd);
	if (!socket) {
		return  -EINVAL;
	}
#if 0
	setnodelay(socket);
#endif

	spin_lock(&the_controller->lock);
	vdev = port_to_vdev(rhport);

	/* begin a lock */
	spin_lock(&vdev->ud.lock);

	if (vdev->ud.status != VDEV_ST_NULL) {
		spin_unlock(&vdev->ud.lock);
		spin_unlock(&the_controller->lock);
		uerr("port %d already used\n", rhport);
		return -EINVAL;
	}

	uinfo("rhport(%u) sockfd(%u) busnum(%u) devnum(%u) speed(%u)\n",
			rhport, sockfd, busnum, devnum, speed);
	uinfo("   info: %s\n", info);

	vdev->busnum        = busnum;
	vdev->devnum        = devnum;
	vdev->speed         = speed;
	vdev->ud.tcp_socket = socket;
	set_sockaddr(socket, &vdev->ud.tcp_ss);
	vdev->ud.status     = VDEV_ST_NOTASSIGNED;
	memcpy(vdev->info, info, VHCI_DEVICE_INFO_SIZE);

	spin_unlock(&vdev->ud.lock);
	spin_unlock(&the_controller->lock);
	/* end the lock */

	if (vdev->ud.tcp_ss.ss_family == AF_INET)
		uinfo("connected to %u.%u.%u.%u(%d)\n",
				NIPQUAD(ss_v4_addr(vdev->ud.tcp_ss)), ss_v4_port(vdev->ud.tcp_ss));
	else if (vdev->ud.tcp_ss.ss_family == AF_INET6)
		uinfo("connected to %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x(%d)",
				NIP6(ss_v6_addr(vdev->ud.tcp_ss)), ss_v6_port(vdev->ud.tcp_ss));
	else
		uerr("unknown ss_family %d\n", vdev->ud.tcp_ss.ss_family);



	usbip_start_threads(&vdev->ud);
	rh_port_connect(rhport, speed);

	return count;
}
static DEVICE_ATTR(attach, S_IWUSR, NULL, store_attach);

static struct attribute *dev_attrs[] = {
	&dev_attr_status.attr,
	&dev_attr_detach.attr,
	&dev_attr_attach.attr,
	&dev_attr_usbip_debug.attr,
	NULL,
};

struct attribute_group dev_attr_group = {
	.attrs = dev_attrs,
};

